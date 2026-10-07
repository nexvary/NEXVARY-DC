#include "Operations.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QStorageInfo>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QtEndian>
#ifdef Q_OS_WIN
#include <io.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
namespace dc {
namespace {
constexpr qsizetype Chunk = 1024 * 1024;
QJsonObject error(const QString &message) { return {{"status","error"},{"message",message}}; }
QJsonObject cancelled() { return {{"status","cancelled"},{"message","Operation cancelled; no complete result."}}; }
bool syncFile(QFile &f) {
 if (!f.flush()) return false;
#ifdef Q_OS_WIN
 return _commit(int(f.handle())) == 0;
#else
 return fsync(int(f.handle())) == 0;
#endif
}
bool regularHandle(QFile &f) {
#ifdef Q_OS_WIN
 return !f.fileName().startsWith("\\\\") && QFileInfo(f.fileName()).isFile();
#else
 struct stat s{};
 return fstat(int(f.handle()), &s) == 0 && S_ISREG(s.st_mode);
#endif
}
}
bool isRegularSource(const QString &path) {
 const auto p = QDir::fromNativeSeparators(path);
 if(p.startsWith("/dev/") || p.startsWith("\\\\") || p.startsWith("//") || p.startsWith("/proc/") || p.startsWith("/sys/")) return false;
 QFileInfo info(path);
 return info.exists() && info.isFile() && !info.isSymLink();
}
QJsonObject scanImage(const QString &path, const Context &ctx) {
 if(!isRegularSource(path)) return error("Select a regular image file; raw devices and symlinks are not supported.");
 QFile f(path);
 if(!f.open(QIODevice::ReadOnly) || !regularHandle(f)) return error("Cannot open regular source file for reading.");
 const qint64 total=f.size();
 if(total<=0) return error("Source file is empty.");
 QCryptographicHash hash(QCryptographicHash::Sha256);
 QElapsedTimer timer; timer.start();
 qint64 done=0;
 while(done<total) {
  if(ctx.cancelled()) return cancelled();
  QByteArray bytes=f.read(qMin<qint64>(Chunk,total-done));
  if(bytes.isEmpty()) return {{"status","error"},{"message","Read failed or source changed; scan incomplete."},{"readBytes",double(done)},{"errorOffset",double(done)}};
  hash.addData(bytes); done+=bytes.size(); ctx.update(done,total);
 }
 if(f.size()!=total) return error("Source size changed during scan.");
 return {{"status","completed"},{"operation","image_read"},{"source",QFileInfo(path).absoluteFilePath()},{"readBytes",double(done)},
 {"sha256",QString::fromLatin1(hash.result().toHex())},{"elapsedMs",double(timer.elapsed())},{"scope","Readable image bytes only; no physical disk health verdict."}};
}
QJsonObject copyImage(const QString &source, const QString &dest, const Context &ctx) {
 if(!isRegularSource(source)) return error("Source must be a regular image file.");
 QFileInfo dst(dest);
 const QString target=dst.absoluteFilePath();
 if(dest.isEmpty() || dst.exists() || dst.isSymLink() || !dst.dir().exists() || target.startsWith("/dev/") || target.startsWith("//") || target.startsWith("\\\\"))
  return error("Destination must be a new file in an existing local directory; existing files are never overwritten.");
 QFile in(source),out(target);
 if(!in.open(QIODevice::ReadOnly) || !regularHandle(in)) return error("Cannot read source.");
 if(!out.open(QIODevice::WriteOnly|QIODevice::NewOnly)) return error("Cannot create destination exclusively.");
 auto abort=[&](QJsonObject result){out.close();out.remove();return result;};
 const auto total=in.size(); if(total<=0) return abort(error("Source is empty."));
 QCryptographicHash hash(QCryptographicHash::Sha256);
 qint64 done=0;
 while(done<total) {
  if(ctx.cancelled()) return abort(cancelled());
  auto b=in.read(qMin<qint64>(Chunk,total-done));
  if(b.isEmpty() || out.write(b)!=b.size()) return abort(error("Image copy failed; partial destination removed."));
  hash.addData(b); done+=b.size();ctx.update(done,total*2);
 }
 if(in.size()!=total || !syncFile(out)) return abort(error("Source changed or destination flush failed."));
 out.close();
 auto verify=scanImage(target,{ctx.cancel,[&](qint64 n,qint64){ctx.update(total+n,total*2);}});
 if(verify.value("status")!="completed") return abort(verify);
 auto checksum=QString::fromLatin1(hash.result().toHex());
 if(verify.value("sha256").toString()!=checksum) return abort(error("Destination verification failed; destination removed."));
 return {{"status","completed"},{"operation","image_copy"},{"source",QFileInfo(source).absoluteFilePath()},{"destination",target},
 {"copiedBytes",double(total)},{"sha256",checksum},{"destinationVerified",true},{"scope","Regular file copy only; not a failing-drive imager."}};
}
QByteArray testPattern(quint64 index, quint64 nonce, qsizetype size) {
 QByteArray out(size,Qt::Uninitialized);
 quint64 state=nonce ^ (index*0x9e3779b97f4a7c15ULL);
 for(qsizetype i=0;i<size;i+=8) {
  state+=0x9e3779b97f4a7c15ULL;
  quint64 z=state; z=(z^(z>>30))*0xbf58476d1ce4e5b9ULL;z=(z^(z>>27))*0x94d049bb133111ebULL;z^=z>>31;
  z=qToLittleEndian(z);
  memcpy(out.data()+i,&z,size_t(qMin<qsizetype>(8,size-i)));
 }
 return out;
}
QJsonObject verifyStorage(ProbeIO &io, quint64 blocks, qsizetype size, quint64 nonce, const Context &ctx) {
 if(!blocks || blocks>1048576 || size<=0 || size>Chunk) return error("Invalid test geometry.");
 quint64 written=0,verified=0,failed=0;QJsonArray errors;
 for(quint64 i=0;i<blocks;++i) {
  if(ctx.cancelled()) return cancelled();
  if(!io.write(i,testPattern(i,nonce,size))) return {{"status","error"},{"message","Write failed; capacity is not established."},{"writtenBytes",double(written)}};
  written+=quint64(size);ctx.update(qint64(i+1),qint64(blocks*2));
 }
 // Verify only after all writes: essential to detect wrapped addresses/fake capacity.
 for(quint64 i=0;i<blocks;++i) {
  if(ctx.cancelled()) return cancelled();
  if(io.read(i)==testPattern(i,nonce,size)) verified+=quint64(size);
  else {failed+=quint64(size);if(errors.size()<64) errors.append(double(i*quint64(size)));}
  ctx.update(qint64(blocks+i+1),qint64(blocks*2));
 }
 return {{"status",failed?"mismatch":"completed"},{"operation","directory_capacity_test"},{"writtenBytes",double(written)},
 {"verifiedBytes",double(verified)},{"failedBytes",double(failed)},{"failedOffsets",errors},
 {"scope","Only tested allocated file bytes; does not establish physical NAND capacity or certify the entire device."},
 {"message",failed?"Data mismatch: counterfeit capacity or storage fault suspected.":"Tested bytes matched; untested space remains unknown."}};
}
QJsonObject testDirectory(const QString &path, int mib, bool acknowledged, const Context &ctx) {
 if(!acknowledged) return error("Backup acknowledgement is required before writing test data.");
 QFileInfo dirInfo(path);
 if(!dirInfo.isDir() || dirInfo.isSymLink() || mib<1 || mib>1048576) return error("Select an existing directory and a valid test size.");
 const QString canonical=dirInfo.canonicalFilePath();
 QStorageInfo storage(canonical);storage.refresh();
 const qint64 requested=qint64(mib)*Chunk;
 if(!storage.isValid() || !storage.isReady() || storage.isReadOnly() || storage.bytesAvailable()<requested+64*Chunk)
  return error("Insufficient available space; reserve at least 64 MiB, or volume unavailable/read-only.");
 QTemporaryDir temp(QDir(canonical).filePath("NEXVARY-DC-test-XXXXXX"));
 if(!temp.isValid()) return error("Cannot create private test directory.");
 ProbeIO io;
 io.write=[&](quint64 index,const QByteArray &bytes){
  QFile f(temp.filePath(QString::number(index)+".dc-test"));
  return f.open(QIODevice::WriteOnly|QIODevice::NewOnly) && f.write(bytes)==bytes.size() && syncFile(f);
 };
 io.read=[&](quint64 index){QFile f(temp.filePath(QString::number(index)+".dc-test"));if(!f.open(QIODevice::ReadOnly))return QByteArray{};return f.readAll();};
 auto result=verifyStorage(io,quint64(mib),Chunk,QRandomGenerator::global()->generate64(),ctx);
 result.insert("directory",canonical);
 result.insert("reportedVolumeBytes",double(storage.bytesTotal()));
 // Qt removes only this newly-created private directory, never user files.
 const bool removed=temp.remove();
 result.insert("testFilesRemoved",removed);
 if(!removed) {result.insert("cleanupPath",temp.path());result.insert("cleanupWarning","Test files remain; remove the listed test directory manually.");}
 return result;
}
}
