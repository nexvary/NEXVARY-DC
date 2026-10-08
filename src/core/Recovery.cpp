#include "Recovery.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QStorageInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QtEndian>
namespace dc {
namespace {
quint32 crcUpdate(quint32 c,const QByteArray &data){for(uchar b:data){c^=b;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return c;}
// Validate structure in bounded buffers. A positive length is not a decoder verdict.
qint64 candidate(QFile &f,qint64 offset,bool png,const Context &ctx){
 if(!f.seek(offset))return 0;const auto start=f.read(png?8:3);if(start.size()!=(png?8:3))return 0;
 if(png){bool header=false,data=false;
  while(!ctx.cancelled()){
   auto h=f.read(8);if(h.size()!=8)return 0;auto n=qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(h.constData()));auto type=h.mid(4,4);
   if(quint64(n)+4>quint64(f.size()-f.pos())||(!header&&(type!="IHDR"||n!=13))||(type=="IHDR"&&(header||n!=13)))return 0;
   quint32 crc=crcUpdate(0xffffffff,type);qint64 left=n;QByteArray first;
   while(left>0&&!ctx.cancelled()){auto b=f.read(qMin<qint64>(1048576,left));if(b.isEmpty())return 0;if(first.isEmpty())first=b.left(13);crc=crcUpdate(crc,b);left-=b.size();}
   if(ctx.cancelled())return 0;auto sum=f.read(4);if(sum.size()!=4||(crc^0xffffffff)!=qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(sum.constData())))return 0;
   if(type=="IHDR"){header=true;if(!qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(first.constData()))||!qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(first.constData()+4)))return 0;}
   if(type=="IDAT")data=true;if(type=="IEND")return n==0&&data?f.pos()-offset:0;
  }
 }else {QByteArray tail;while(!ctx.cancelled()){
  auto b=f.read(1048576);if(b.isEmpty())return 0;auto buffer=tail+b;auto base=f.pos()-buffer.size();auto end=buffer.indexOf(QByteArray::fromHex("ffd9"));auto soi=buffer.indexOf(QByteArray::fromHex("ffd8ff"));
  if(soi>=0&&(end<0||soi<end))return 0;if(end>=0)return base+end+2-offset;tail=buffer.right(2);
 }}return 0;
}
}
QJsonObject recoverImage(const QString &source,const QString &directory,const Context &ctx) {
 auto error=[](const QString &s){return QJsonObject{{"status","error"},{"operation","file_recovery"},{"message",s}};};
 if(!isRegularSource(source))return error("Select a regular rescue image. Physical devices are not accepted by file recovery.");
 QFileInfo dst(directory);
 if(!dst.isDir() || dst.isSymLink())return error("Select an existing local destination folder on your recovery disk.");
 const auto canonical=dst.canonicalFilePath();
 if(canonical.startsWith("/dev/") || canonical.startsWith("//"))return error("Destination must be a local directory.");
 QFile input(source),reader(source);
 if(!input.open(QIODevice::ReadOnly) || !reader.open(QIODevice::ReadOnly) || input.size()<=0)return error("Cannot read source image.");
 const auto total=input.size();
 QTemporaryDir output(QDir(canonical).filePath("NEXVARY-Recovered-XXXXXX"));
 if(!output.isValid())return error("Cannot create a recovery folder.");
 output.setAutoRemove(false);
 QJsonArray files; qint64 scanned=0,skipUntil=0;QByteArray tail;QString status="completed",message;
 const auto png=QByteArray::fromHex("89504e470d0a1a0a"),jpg=QByteArray::fromHex("ffd8ff");
 while(scanned<total) {
  if(ctx.cancelled()){status="cancelled";break;}
  const auto block=input.read(qMin<qint64>(1048576,total-scanned));
  if(block.isEmpty()){status="error";message="Image read failed; recovered files were retained.";break;}
  const auto buffer=tail+block;const auto base=scanned-tail.size();
  for(qsizetype pos=0;pos<buffer.size()-2;++pos) {
   const auto offset=base+pos;if(offset<skipUntil)continue;
   const bool isPng=buffer.mid(pos,8)==png;
   if(!isPng && buffer.mid(pos,3)!=jpg)continue;
   auto length=candidate(reader,offset,isPng,ctx);if(length<=0)continue;
   QStorageInfo storage(output.path());storage.refresh();
   if(!storage.isValid() || storage.isReadOnly() || storage.bytesAvailable()<length+64*1024*1024){status="error";message="Insufficient recovery space; existing results retained.";break;}
   const auto name=QString("%1_%2.%3").arg(files.size()+1,6,10,QChar('0')).arg(offset).arg(isPng?"png":"jpg");
   QFile file(output.filePath(name));
   if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)||!reader.seek(offset)){status="error";message="Recovery destination/source open failed";break;}
   QCryptographicHash hash(QCryptographicHash::Sha256);qint64 copied=0;
   while(copied<length&&!ctx.cancelled()){auto bytes=reader.read(qMin<qint64>(1048576,length-copied));if(bytes.isEmpty()||file.write(bytes)!=bytes.size()){status="error";message="Recovery read/write failed";break;}hash.addData(bytes);copied+=bytes.size();}
   if(!file.flush()){status="error";message="Recovery flush failed";}file.close();QFile verify(file.fileName());QCryptographicHash readback(QCryptographicHash::Sha256);
   bool matches=verify.open(QIODevice::ReadOnly)&&readback.addData(&verify)&&verify.size()==copied&&readback.result()==hash.result();
   if(!matches){status="error";message="Recovered file readback failed";}
   if(ctx.cancelled())status="cancelled";
   files.append(QJsonObject{{"file",name},{"offset",double(offset)},{"bytes",double(copied)},{"expectedBytes",double(length)},{"sha256",QString::fromLatin1(hash.result().toHex())},{"readbackVerified",matches},{"condition",copied==length&&matches?"candidate":"partial"},{"validation",isPng?"PNG chunk CRC, decoder/content integrity unverified":"JPEG signature and end marker only"}});
   skipUntil=offset+length;
   if(status!="completed")break;
   if(files.size()>=100000){status="partial";message="100000 carving result limit reached; metadata recovery has a configurable file limit.";break;}
  }
  scanned+=block.size();tail=buffer.right(7);ctx.update(scanned,total);
  if(status!="completed")break;
 }
 if(ctx.cancelled() && status=="completed")status="cancelled";
 if(input.size()!=total){status="error";message="Image size changed during recovery.";}
 QJsonObject result{{"status",status},{"operation","file_recovery"},{"source",QFileInfo(source).absoluteFilePath()},{"destination",output.path()},{"scannedBytes",double(scanned)},{"recoveredCount",files.size()},{"files",files},{"message",message},{"scope","PNG and JPEG streaming signature carving, bounded by source-image length. Includes live and deleted contiguous data. Original names, folders, fragmentation and overwritten/TRIM data are not restored. JPEG files require visual verification."}};
 int completeCount=0,partialCount=0,candidateCount=0;for(const auto &value:files){auto condition=value.toObject().value("condition").toString();if(condition=="complete")++completeCount;else if(condition=="partial")++partialCount;else ++candidateCount;}result.insert("completeCount",completeCount);result.insert("partialCount",partialCount);result.insert("candidateCount",candidateCount);
 QFile manifest(output.filePath("manifest.json"));const auto json=QJsonDocument(result).toJson();
 if(!manifest.open(QIODevice::WriteOnly|QIODevice::NewOnly) || manifest.write(json)!=json.size() || !manifest.flush())result.insert("manifestWarning","Manifest write failed; export the operation report.");
 return result;
}
}
