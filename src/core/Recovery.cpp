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
constexpr qint64 Limit=64*1024*1024;
quint32 crc(const QByteArray &data) {
 quint32 c=0xffffffff;
 for(unsigned char b:data){c^=b;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u & (0u-(c&1)));}
 return c^0xffffffff;
}
// PNG chunks are checked including CRC; JPEG boundaries are candidates, not a decoder verdict.
QByteArray candidate(QFile &f,qint64 offset,bool png,const Context &ctx) {
 if(!f.seek(offset))return {};
 QByteArray bytes=f.read(png?8:3);
 if(png) {
  bool header=false,data=false;
  while(bytes.size()<=Limit-12 && !ctx.cancelled()) {
   const auto h=f.read(8);if(h.size()!=8)return {};
   const auto n=qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(h.constData()));
   if(n>quint32(Limit-bytes.size()-12))return {};
   const auto type=h.mid(4,4);
   if(!header && (type!="IHDR" || n!=13))return {};
   const auto body=f.read(n);const auto sum=f.read(4);
   if(body.size()!=n || sum.size()!=4 || crc(type+body)!=qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(sum.constData())))return {};
   if(type=="IHDR"){if(header)return {};header=true;if(qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(body.constData()))==0 || qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(body.constData()+4))==0)return {};}
   if(type=="IDAT")data=true;
   bytes+=h;bytes+=body;bytes+=sum;
   if(type=="IEND")return n==0 && data?bytes:QByteArray{};
  }
 } else {
  while(bytes.size()<Limit && !ctx.cancelled()) {
   const auto b=f.read(qMin<qint64>(65536,Limit-bytes.size()));if(b.isEmpty())return {};
   const auto previous=bytes.size();bytes+=b;
   const auto end=bytes.indexOf(QByteArray::fromHex("ffd9"),qMax<qsizetype>(3,previous-1));
   if(end>=0){bytes.truncate(end+2);return bytes;}
   // A second SOI suggests a truncated candidate; do not merge distinct images.
   if(bytes.indexOf(QByteArray::fromHex("ffd8ff"),3)>=0)return {};
  }
 }
 return {};
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
   auto bytes=candidate(reader,offset,isPng,ctx);if(bytes.isEmpty())continue;
   QStorageInfo storage(output.path());storage.refresh();
   if(!storage.isValid() || storage.isReadOnly() || storage.bytesAvailable()<bytes.size()+64*1024*1024){status="error";message="Insufficient recovery space; existing results retained.";break;}
   const auto name=QString("%1_%2.%3").arg(files.size()+1,6,10,QChar('0')).arg(offset).arg(isPng?"png":"jpg");
   QFile file(output.filePath(name));
   if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly) || file.write(bytes)!=bytes.size() || !file.flush()){file.close();file.remove();status="error";message="Recovery write failed; previous results retained.";break;}
   file.close();QFile verify(file.fileName());
   const auto hash=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);
   if(!verify.open(QIODevice::ReadOnly) || QCryptographicHash::hash(verify.readAll(),QCryptographicHash::Sha256)!=hash){verify.close();file.remove();status="error";message="Recovered file readback failed.";break;}
   files.append(QJsonObject{{"file",name},{"offset",double(offset)},{"bytes",bytes.size()},{"sha256",QString::fromLatin1(hash.toHex())},{"validation",isPng?"PNG chunk CRC":"JPEG signature and end marker only"}});
   skipUntil=offset+bytes.size();
   if(files.size()>=10000){status="partial";message="Recovery stopped at 10000 files; split the image for additional recovery.";break;}
  }
  scanned+=block.size();tail=buffer.right(7);ctx.update(scanned,total);
  if(status!="completed")break;
 }
 if(ctx.cancelled() && status=="completed")status="cancelled";
 if(input.size()!=total){status="error";message="Image size changed during recovery.";}
 QJsonObject result{{"status",status},{"operation","file_recovery"},{"source",QFileInfo(source).absoluteFilePath()},{"destination",output.path()},{"scannedBytes",double(scanned)},{"recoveredCount",files.size()},{"files",files},{"message",message},{"scope","PNG and JPEG signature carving, up to 64 MiB each. Includes live and deleted contiguous data. Original names, folders, fragmentation and overwritten/TRIM data are not restored. JPEG files require visual verification."}};
 QFile manifest(output.filePath("manifest.json"));const auto json=QJsonDocument(result).toJson();
 if(!manifest.open(QIODevice::WriteOnly|QIODevice::NewOnly) || manifest.write(json)!=json.size() || !manifest.flush())result.insert("manifestWarning","Manifest write failed; export the operation report.");
 return result;
}
}
