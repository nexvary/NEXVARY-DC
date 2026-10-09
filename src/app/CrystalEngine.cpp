#include "CrystalEngine.h"
#include "CrystalReport.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace dc {
namespace {
void stage(const QString &package,const QString &target) {
 auto linked=[](const QString &path){const QFileInfo info(path);return info.isSymLink()||info.isJunction();};
 if(linked(package)||linked(target))throw std::runtime_error("Engine directory is a link");
 QFile manifest(":/engines/cdi-files.json");if(!manifest.open(QIODevice::ReadOnly))throw std::runtime_error("Bundled engine manifest missing");
 const auto files=QJsonDocument::fromJson(manifest.readAll()).object();if(files.size()!=642)throw std::runtime_error("Invalid engine manifest");
 QDirIterator existing(target,QDir::Files,QDirIterator::Subdirectories);
 while(existing.hasNext()){auto path=existing.next();auto relative=QDir(target).relativeFilePath(path);const auto lower=relative.toLower();if((lower.endsWith(".dll")||lower.endsWith(".exe"))&&!files.contains(relative))throw std::runtime_error("Unexpected executable in engine state directory");if(QFileInfo(path).isSymLink())throw std::runtime_error("Engine state contains a link");}
 for(auto it=files.begin();it!=files.end();++it){
  const auto name=it.key();if(name.contains("..")||name.startsWith('/')||name.contains(':'))throw std::runtime_error("Invalid engine member path");
  const auto source=QDir(package).filePath(name),output=QDir(target).filePath(name);
  const auto root=QDir(target).absolutePath();auto ancestor=QFileInfo(output).absolutePath();
  for(int depth=0;ancestor.compare(root,Qt::CaseInsensitive)!=0;++depth){
   if(depth>=64||linked(ancestor))throw std::runtime_error("Engine parent directory is a link or exceeds nesting limit");
   const auto parent=QFileInfo(ancestor).absolutePath();if(parent==ancestor)throw std::runtime_error("Engine member escapes state directory");ancestor=parent;
  }
  QFile input(source);if(QFileInfo(source).isSymLink()||!input.open(QIODevice::ReadOnly))throw std::runtime_error("Incomplete bundled engine");
  QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&input)||hash.result().toHex()!=it.value().toString().toLatin1())throw std::runtime_error("Bundled engine checksum mismatch");input.close();
  bool valid=false;QFile old(output);if(old.open(QIODevice::ReadOnly)){QCryptographicHash h(QCryptographicHash::Sha256);valid=h.addData(&old)&&h.result().toHex()==it.value().toString().toLatin1();old.close();}
  if(!valid){QDir().mkpath(QFileInfo(output).absolutePath());if(QFile::exists(output)&&!QFile::remove(output))throw std::runtime_error("Cannot replace engine file");if(!QFile::copy(source,output))throw std::runtime_error("Cannot stage engine file");}
 }
}
QString decode(const QByteArray &bytes) {
 if(bytes.size()>2&&((uchar(bytes[0])==0xff&&uchar(bytes[1])==0xfe)||bytes.contains('\0'))){const int offset=uchar(bytes[0])==0xff?2:0;return QString::fromUtf16(reinterpret_cast<const char16_t*>(bytes.constData()+offset),(bytes.size()-offset)/2);}
 return QString::fromUtf8(bytes);
}
QJsonObject error(const QString &message){return {{"status","error"},{"operation","crystal_read"},{"message",message}};}
#ifdef Q_OS_WIN
void ini(const QString &directory,const wchar_t *key,const wchar_t *value) {
 const auto path=QDir::toNativeSeparators(QDir(directory).filePath("DiskInfo.ini"));
 if(!QFile::exists(path)){QFile file(path);if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)||file.write(QByteArray::fromHex("fffe"))!=2)throw std::runtime_error("Cannot initialize engine settings");}
 if(!WritePrivateProfileStringW(L"Setting",key,value,reinterpret_cast<LPCWSTR>(path.utf16())))throw std::runtime_error("Cannot save engine settings");
}
#endif
}
QJsonObject readCrystalEngine(const QString &package,std::atomic_bool *cancel) {
#ifdef Q_OS_WIN
 try {
  QTemporaryDir temp;if(!temp.isValid())return error("Cannot stage SMART engine.");stage(package,temp.path());
  ini(temp.path(),L"Language",L"English");ini(temp.path(),L"AutoAamApm",L"0");ini(temp.path(),L"Resident",L"0");ini(temp.path(),L"Startup",L"0");ini(temp.path(),L"AutoRefresh",L"0");
  QProcess process;process.setWorkingDirectory(temp.path());process.start(temp.filePath("DiskInfo64.exe"),{"/CopyExit"});if(!process.waitForStarted(5000))return error("CrystalDiskInfo requires administrator permission or its process could not start.");
  QElapsedTimer timer;timer.start();while(!process.waitForFinished(100)&&process.state()!=QProcess::NotRunning){if((cancel&&cancel->load())||timer.elapsed()>120000){process.kill();process.waitForFinished();return error("SMART engine cancelled or timed out; health remains unknown.");}}
  QFile report(temp.filePath("DiskInfo.txt"));if(process.exitStatus()!=QProcess::NormalExit||!report.open(QIODevice::ReadOnly)||report.size()>16*1024*1024)return error("Engine produced no report. Close an existing advanced-panel instance and retry.");
  return parseCrystalReport(decode(report.readAll()));
 }catch(const std::exception &e){return error(QString::fromUtf8(e.what()));}
#else
 Q_UNUSED(package);Q_UNUSED(cancel);return error("CrystalDiskInfo is a Windows engine; native SMART monitoring uses smartctl on Linux.");
#endif
}
QJsonObject launchCrystalEngine(const QString &package,const QString &stateDirectory,bool arabic) {
#ifdef Q_OS_WIN
 try {
  QDir().mkpath(stateDirectory);stage(package,stateDirectory);
  ini(stateDirectory,L"Language",arabic?L"Arabic":L"English");
  // Never apply saved AAM/APM commands just by opening a health panel. Users
  // may explicitly apply supported settings in the upstream control dialog.
  ini(stateDirectory,L"AutoAamApm",L"0");
  qint64 pid=0;QProcess process;process.setProgram(QDir(stateDirectory).filePath("DiskInfo64.exe"));process.setWorkingDirectory(stateDirectory);if(!process.startDetached(&pid))return error("Cannot open advanced panel; administrator permission is required.");
  return {{"status","completed"},{"operation","crystal_panel"},{"pid",double(pid)},{"message","Opened the unmodified licensed advanced panel. AAM/APM, graphs, resident alarms, mail, controller settings and health thresholds use the upstream interface. Saved automatic AAM/APM application was disabled for this launch."}};
 }catch(const std::exception &e){return error(QString::fromUtf8(e.what()));}
#else
 Q_UNUSED(package);Q_UNUSED(stateDirectory);Q_UNUSED(arabic);return error("Advanced CrystalDiskInfo panel requires Windows.");
#endif
}
}
