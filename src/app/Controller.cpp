#include "Controller.h"
#include "Operations.h"
#include "StoragePolicy.h"
#include "MediaRead.h"
#include "Recovery.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QUuid>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif
#include <QtConcurrent>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QSqlQuery>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
namespace {
QJsonObject fail(const QString &text) {return {{"status","error"},{"message",text}};}
QJsonObject process(const QString &exe,const QStringList &args,bool smart=false,std::atomic_bool *cancel=nullptr) {
 QProcess p;p.setProcessChannelMode(QProcess::SeparateChannels);p.start(exe,args);
 if(!p.waitForStarted(5000)) return fail("Engine unavailable: "+exe+". Required engine could not be started.");
 QElapsedTimer timeout;timeout.start();
 while(!p.waitForFinished(100) && p.state()!=QProcess::NotRunning) {
  if(cancel && cancel->load()){p.kill();p.waitForFinished(2000);return {{"status","cancelled"},{"message","Read engine stopped."}};}
  if(timeout.elapsed()>15000){p.kill();p.waitForFinished(2000);return fail("Engine timed out; process stopped.");}
 }
 auto bytes=p.readAllStandardOutput();
 auto doc=QJsonDocument::fromJson(bytes);
 if(p.exitStatus()!=QProcess::NormalExit || doc.isNull()) return fail(QString::fromUtf8(p.readAllStandardError()).left(2000)+" Engine returned no valid JSON.");
 if(!smart && p.exitCode()!=0) return fail("Device enumeration failed.");
 // smartctl exit status is a bitmask: failing health is valuable data, not discarded.
 QJsonObject result=doc.isObject()?doc.object():QJsonObject{{"devices",doc.array()}};
 result.insert("status",smart && (p.exitCode()&7)?"error":"completed");result.insert("engineExitCode",p.exitCode());
 result.insert("operation",smart?"smart_read":"disk_discovery");
 if(smart){result.insert("summary",dc::summarizeSmart(result));QStringList messages;for(auto m:result.value("smartctl").toObject().value("messages").toArray())messages.append(m.toObject().value("string").toString());if(!messages.isEmpty())result.insert("message",messages.join("; "));}
 return result;
}
QJsonObject storageProcess(const QJsonObject &request,std::atomic_bool *cancel=nullptr) {
#ifdef Q_OS_WIN
 QTemporaryDir dir;
 if(!dir.isValid())return fail("Cannot stage storage helper.");
 for(const auto &name:QStringList{"storage.ps1","policy.ps1","hybrid.ps1"}) {
  QFile src(":/storage/"+name);
  if(!src.copy(dir.filePath(name)))return fail("Storage helper unavailable.");
 }
 const QString encoded=QString::fromLatin1(QJsonDocument(request).toJson(QJsonDocument::Compact).toBase64());
 QProcess p;p.start("powershell.exe",{"-NoProfile","-NonInteractive","-ExecutionPolicy","Bypass","-File",dir.filePath("storage.ps1"),"-RequestBase64",encoded});
 if(!p.waitForStarted(5000))return fail("Cannot start Windows storage service.");
 const bool inventory=request.value("action")=="inventory";
 QElapsedTimer timer;timer.start();
 while(!p.waitForFinished(100) && p.state()!=QProcess::NotRunning) {
  if(inventory && ((cancel && cancel->load()) || timer.elapsed()>20000)){p.kill();p.waitForFinished();return QJsonObject{{"status","cancelled"},{"operation","disk_discovery"},{"message","Device discovery stopped."}};}
  // Never terminate a filesystem or partition mutation halfway through.
 }
 QJsonParseError error;auto doc=QJsonDocument::fromJson(p.readAllStandardOutput().trimmed(),&error);
 if(p.exitStatus()!=QProcess::NormalExit || error.error!=QJsonParseError::NoError || !doc.isObject())return fail("Storage helper failed: "+QString::fromUtf8(p.readAllStandardError()).left(2000));
 return doc.object();
#else
 Q_UNUSED(request);Q_UNUSED(cancel);return fail("Storage management is implemented for Windows in this release.");
#endif
}
}
Controller::Controller(QObject *parent):QObject(parent) {
 auto data=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);QDir().mkpath(data);
 m_db=QSqlDatabase::addDatabase("QSQLITE","dc-history");m_db.setDatabaseName(QDir(data).filePath("operations.sqlite"));
 if(m_db.open()) {
  QSqlQuery q(m_db);
  if(!q.exec("CREATE TABLE IF NOT EXISTS operations(id INTEGER PRIMARY KEY, created TEXT, operation TEXT, report TEXT)"))
   m_storageNotice="History database initialization failed; reports remain exportable.";
 } else m_storageNotice="History unavailable; reports remain exportable.";
 loadHistory();
 connect(&m_watcher,&QFutureWatcher<QJsonObject>::finished,this,[this]{
  auto result=m_watcher.result();
  if(result.value("operation")=="disk_discovery" && result.value("status")=="completed") {
   m_disks.clear();
#ifdef Q_OS_WIN
   const auto list=result.value("devices").toArray();
   for(auto item:list) {const auto d=item.toObject();m_disks.append(d.toVariantMap());}
#else
   for(auto item:result.value("blockdevices").toArray()) {const auto d=item.toObject();if(d.value("type")!="disk")continue;m_disks.append(QVariantMap{{"device",d.value("path").toString()},{"model",d.value("model").toString()},{"serial",d.value("serial").toString()},{"bytes",d.value("size").toVariant()},{"transport",d.value("tran").toString()}});}
#endif
   emit disksChanged();
  }
  m_busy=false;
  // An error or cancelled operation must never show a completed progress bar.
  if(result.value("status")=="completed" || result.value("status")=="mismatch")m_progress=1;
  setResult(result);emit stateChanged();
 });
 refresh();
}
Controller::~Controller(){m_cancel=true;m_watcher.waitForFinished();m_db.close();m_db=QSqlDatabase();QSqlDatabase::removeDatabase("dc-history");}
void Controller::start(const QString &name,Work work) {
 if(m_busy)return;
 m_mutating=name.startsWith("storage_");
 m_operation=name;m_cancel=false;m_busy=true;m_progress=0;
 m_report=QString::fromUtf8(QJsonDocument(QJsonObject{{"status","running"},{"operation",name}}).toJson(QJsonDocument::Indented));
 m_result = QVariantMap{{"status","running"},{"operation",name}};
 emit reportChanged();emit stateChanged();m_watcher.setFuture(QtConcurrent::run(std::move(work)));
}
void Controller::refresh() {
 if(m_busy)return;
 m_volumes.clear();m_disks.clear();emit disksChanged();
 for(const auto &v:QStorageInfo::mountedVolumes()) if(v.isValid() && v.isReady() && v.bytesTotal()>0)
  m_volumes.append(QVariantMap{{"root",v.rootPath()},{"name",v.displayName()},{"device",QString::fromUtf8(v.device())},{"bytes",double(v.bytesTotal())},{"free",double(v.bytesAvailable())},{"readOnly",v.isReadOnly()},{"filesystem",QString::fromUtf8(v.fileSystemType())}});
 emit volumesChanged();
 start("disk_discovery",[this]{
#ifdef Q_OS_WIN
  return storageProcess(QJsonObject{{"action","inventory"}},&m_cancel);
#else
  return process("lsblk",{"--json","--bytes","--nodeps","--output","PATH,MODEL,SERIAL,SIZE,TRAN,TYPE"},false,&m_cancel);
#endif
 });
}
void Controller::inspectHealth(const QString &device) {
 if(m_busy)return;
 bool known=false;for(const auto &d:m_disks) if(d.toMap().value("device").toString()==device)known=true;
 if(!known) {setResult(fail("Select an enumerated disk. Arbitrary device paths are rejected."));return;}
 auto engine=QDir(QCoreApplication::applicationDirPath()).filePath("engines/smartctl.exe");
 if(!QFile::exists(engine))engine=QStandardPaths::findExecutable("smartctl");
 if(engine.isEmpty()){setResult(fail("smartctl is not installed or not on PATH. SMART unavailable; disk health is unknown."));return;}
 start("smart_read",[this,engine,device]{return process(engine,{"--json","--all",device},true,&m_cancel);});
}
void Controller::scanImage(const QString &path) {
 if(m_busy)return;
 start("image_read",[this,path]{return dc::scanImage(path,{&m_cancel,[this](qint64 n,qint64 total){QMetaObject::invokeMethod(this,[this,n,total]{m_progress=total?double(n)/double(total):0;emit stateChanged();},Qt::QueuedConnection);}});});
}
void Controller::copyImage(const QString &source,const QString &dest) {
 if(m_busy)return;
 start("image_copy",[this,source,dest]{return dc::copyImage(source,dest,{&m_cancel,[this](qint64 n,qint64 total){QMetaObject::invokeMethod(this,[this,n,total]{m_progress=total?double(n)/double(total):0;emit stateChanged();},Qt::QueuedConnection);}});});
}
void Controller::testCapacity(const QString &dir,int mib,bool ack) {
 if(m_busy)return;
 start("directory_capacity_test",[this,dir,mib,ack]{return dc::testDirectory(dir,mib,ack,{&m_cancel,[this](qint64 n,qint64 total){QMetaObject::invokeMethod(this,[this,n,total]{m_progress=total?double(n)/double(total):0;emit stateChanged();},Qt::QueuedConnection);}});});
}
void Controller::cancel(){if(interruptible())m_cancel=true;}
QString Controller::fileUrl(const QString &path) const {return QUrl::fromLocalFile(path).toString();}
QString Controller::localPath(const QString &url) const {QUrl u(url);return u.isLocalFile()?u.toLocalFile():url;}
void Controller::setResult(const QJsonObject &input) {
 auto result=input;result.insert("appVersion","0.3.0");result.insert("recordedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
 m_report=QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented));
 if(m_db.isOpen()) {QSqlQuery q(m_db);q.prepare("INSERT INTO operations(created,operation,report) VALUES(?,?,?)");q.addBindValue(result.value("recordedAt").toString());q.addBindValue(result.value("operation").toString(m_operation));q.addBindValue(m_report);if(!q.exec()){result.insert("historySaved",false);m_report=QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented));}}
 m_result=result.toVariantMap();
 emit reportChanged();loadHistory();
}
void Controller::loadHistory(){m_history.clear();if(m_db.isOpen()){QSqlQuery q("SELECT created,operation,report FROM operations ORDER BY id DESC LIMIT 50",m_db);while(q.next())m_history.append(QVariantMap{{"created",q.value(0)},{"operation",q.value(1)},{"report",q.value(2)}});}emit historyChanged();}
bool Controller::exportReport(const QString &dest) {
 if(m_report.isEmpty() || m_busy)return false;
 QFile f(dest);if(!f.open(QIODevice::WriteOnly|QIODevice::NewOnly))return false;
 auto bytes=m_report.toUtf8();bool ok=f.write(bytes)==bytes.size() && f.flush();f.close();if(!ok)f.remove();return ok;
}

bool Controller::windows() const {
#ifdef Q_OS_WIN
 return true;
#else
 return false;
#endif
}
bool Controller::administrator() const {
#ifdef Q_OS_WIN
 SID_IDENTIFIER_AUTHORITY auth=SECURITY_NT_AUTHORITY;PSID group=nullptr;BOOL admin=FALSE;
 if(AllocateAndInitializeSid(&auth,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&group)){CheckTokenMembership(nullptr,group,&admin);FreeSid(group);}return admin;
#else
 return false;
#endif
}
bool Controller::relaunchAdministrator() {
#ifdef Q_OS_WIN
 if(m_busy)return false;
 auto exe=QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
 auto result=ShellExecuteW(nullptr,L"runas",reinterpret_cast<LPCWSTR>(exe.utf16()),nullptr,nullptr,SW_SHOWNORMAL);
 if(reinterpret_cast<INT_PTR>(result)>32){QCoreApplication::quit();return true;}
#endif
 return false;
}
QVariantMap Controller::prepareStorage(const QString &device,const QString &action,int part,const QString &fs,const QString &style,int mib,const QString &source) {
 m_pendingToken.clear();m_pending={};
 if(m_busy)return QVariantMap{{"error","Wait for the current operation."}};
 if(!windows())return QVariantMap{{"error","Storage management in this release requires Windows."}};
 if(!administrator())return QVariantMap{{"error","Administrator permission required. Relaunch and select the disk again."}};
 QJsonObject d;for(const auto &v:m_disks)if(v.toMap().value("device").toString()==device)d=QJsonObject::fromVariantMap(v.toMap());
 auto r=d;r.insert("action",action);r.insert("partition",part);r.insert("filesystem",fs);r.insert("style",style);r.insert("sizeMiB",mib);r.insert("source",source.isEmpty()?QString{}:QFileInfo(source).absoluteFilePath());r.insert("appDirectory",QCoreApplication::applicationDirPath());
 for(auto p:d.value("partitions").toArray())if(p.toObject().value("partition").toInt()==part){r.insert("offset",p.toObject().value("offset"));r.insert("partitionBytes",p.toObject().value("partitionBytes"));}
 const auto error=dc::validateStorageRequest(d,r);if(!error.isEmpty())return QVariantMap{{"error",error}};
 m_pending=r;m_pendingToken=QUuid::createUuid().toString(QUuid::WithoutBraces);m_pendingExpires=QDateTime::currentSecsSinceEpoch()+180;
 return QVariantMap{{"token",m_pendingToken},{"device",device},{"model",d.value("model").toString()},{"bytes",d.value("bytes").toVariant()},{"action",action},{"partition",part},{"challenge",device},{"allData",action=="layout" || action=="windows_usb" || action=="windows_bios_usb" || action=="linux_usb"}};
}
void Controller::executeStorage(const QString &token,const QString &confirmation,bool acknowledged) {
 if(m_busy)return;
 const auto expected=m_pendingToken;m_pendingToken.clear();
 if(expected.isEmpty() || token!=expected || QDateTime::currentSecsSinceEpoch()>m_pendingExpires || !acknowledged || confirmation!=m_pending.value("device").toString()) {setResult(fail("Confirmation missing, expired or invalid. No changes made."));return;}
 auto request=m_pending;m_pending={};
 start("storage_"+request.value("action").toString(),[request]{return storageProcess(request);});
}

void Controller::scanSurface(const QString &device,bool acknowledged) {
 if(m_busy)return;
 qint64 bytes=0;for(const auto &d:m_disks)if(d.toMap().value("device").toString()==device)bytes=d.toMap().value("bytes").toLongLong();
 if(!acknowledged || !bytes){setResult(fail("Select an enumerated disk and acknowledge that a full read can stress failing media."));return;}
 start("surface_read",[this,device,bytes]{return dc::scanMedia(device,bytes,{&m_cancel,[this](qint64 n,qint64 total){QMetaObject::invokeMethod(this,[this,n,total]{m_progress=total?double(n)/double(total):0;emit stateChanged();},Qt::QueuedConnection);}});});
}
void Controller::rescueDisk(const QString &device,const QString &destination,bool acknowledged) {
 if(m_busy)return;
 qint64 bytes=0;for(const auto &d:m_disks)if(d.toMap().value("device").toString()==device)bytes=d.toMap().value("bytes").toLongLong();
 if(!acknowledged || !bytes){setResult(fail("Select an enumerated disk and acknowledge rescue limitations."));return;}
 start("media_rescue",[this,device,bytes,destination]{return dc::rescueMedia(device,bytes,destination,{&m_cancel,[this](qint64 n,qint64 total){QMetaObject::invokeMethod(this,[this,n,total]{m_progress=total?double(n)/double(total):0;emit stateChanged();},Qt::QueuedConnection);}});});
}

void Controller::recoverImage(const QString &source,const QString &directory,bool fat32) {
 if(m_busy)return;
 start(fat32?"fat32_recovery":"file_recovery",[this,source,directory,fat32]{return (fat32?dc::recoverFat32:dc::recoverImage)(source,directory,{&m_cancel,[this](qint64 n,qint64 total){QMetaObject::invokeMethod(this,[this,n,total]{m_progress=total?double(n)/double(total):0;emit stateChanged();},Qt::QueuedConnection);}});});
}
