#include "Controller.h"
#include "Operations.h"
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
#include <QUrl>
namespace {
QJsonObject fail(const QString &text) {return {{"status","error"},{"message",text}};}
QJsonObject process(const QString &exe,const QStringList &args,bool smart=false,std::atomic_bool *cancel=nullptr) {
 QProcess p;p.setProcessChannelMode(QProcess::SeparateChannels);p.start(exe,args);
 if(!p.waitForStarted(5000)) return fail("Engine unavailable: "+exe+". Install the engine separately; no bundled engines yet.");
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
 result.insert("status",smart && (p.exitCode()&3)?"error":"completed");result.insert("engineExitCode",p.exitCode());
 result.insert("operation",smart?"smart_read":"disk_discovery");
 return result;
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
   for(auto item:list) {const auto d=item.toObject();m_disks.append(QVariantMap{{"device",d.value("DeviceID").toString()},{"model",d.value("Model").toString()},{"serial",d.value("SerialNumber").toString()},{"bytes",d.value("Size").toVariant()},{"transport",d.value("InterfaceType").toString()}});}
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
  const QString command="@{ devices = @(Get-CimInstance Win32_DiskDrive | Select-Object DeviceID,Model,SerialNumber,Size,InterfaceType) } | ConvertTo-Json -Depth 4 -Compress";
  return process("powershell.exe",{"-NoProfile","-NonInteractive","-Command",command},false,&m_cancel);
#else
  return process("lsblk",{"--json","--bytes","--nodeps","--output","PATH,MODEL,SERIAL,SIZE,TRAN,TYPE"},false,&m_cancel);
#endif
 });
}
void Controller::inspectHealth(const QString &device) {
 if(m_busy)return;
 bool known=false;for(const auto &d:m_disks) if(d.toMap().value("device").toString()==device)known=true;
 if(!known) {setResult(fail("Select an enumerated disk. Arbitrary device paths are rejected."));return;}
 auto engine=QStandardPaths::findExecutable("smartctl");
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
void Controller::cancel(){m_cancel=true;}
QString Controller::localPath(const QString &url) const {QUrl u(url);return u.isLocalFile()?u.toLocalFile():url;}
void Controller::setResult(const QJsonObject &input) {
 auto result=input;result.insert("appVersion","0.1.0");result.insert("recordedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
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
