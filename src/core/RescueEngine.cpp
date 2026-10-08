#include "RescueEngine.h"
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDir>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace dc {
namespace {
constexpr qint64 Chunk=1048576;
QString digest(const QByteArray &b){return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex());}
bool sync(QFile &f){if(!f.flush())return false;
#ifdef Q_OS_WIN
 return FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(f.handle())));
#else
 return ::fsync(f.handle())==0;
#endif
}
QString fileIdentity(QFile &f){
#ifdef Q_OS_WIN
 BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(reinterpret_cast<HANDLE>(_get_osfhandle(f.handle())),&info))return {};
 return QString("%1:%2:%3").arg(info.dwVolumeSerialNumber).arg(info.nFileIndexHigh).arg(info.nFileIndexLow);
#else
 struct stat s{};if(::fstat(f.handle(),&s)!=0||!S_ISREG(s.st_mode))return {};return QString("%1:%2").arg(quint64(s.st_dev)).arg(quint64(s.st_ino));
#endif
}
bool append(QFile &journal,const QJsonObject &entry){auto line=QJsonDocument(entry).toJson(QJsonDocument::Compact)+'\n';return journal.write(line)==line.size()&&sync(journal);}
}
QJsonObject rescueStream(RescueSource &source,const QString &destination,const RescueOptions &options,const Context &ctx){
 auto fail=[](const QString &m){return QJsonObject{{"status","error"},{"operation","media_rescue"},{"message",m}};};
 if(source.bytes<=0||source.bytes>((qint64(1)<<53)-4096)||!QList<int>{512,4096}.contains(options.sectorBytes)||source.bytes%options.sectorBytes||options.retries<0||options.retries>5||source.identity.isEmpty()||!source.read)return fail("Invalid source identity, size, sector or retry settings.");
 QFileInfo info(destination);if(info.isSymLink()||!QFileInfo(info.absolutePath()).isDir()||info.absoluteFilePath().startsWith("//"))return fail("Choose a local regular destination.");
 const auto mapPath=info.absoluteFilePath()+".readmap.jsonl";if(QFileInfo(mapPath).isSymLink())return fail("Read map must not be a symlink.");
 QFile output(info.absoluteFilePath()),journal(mapPath);qint64 processed=0,failed=0;bool rangesTruncated=false;QJsonArray bad;QCryptographicHash hash(QCryptographicHash::Sha256);QString error;
 auto addBad=[&](qint64 offset,qint64 bytes){failed+=bytes;if(!bad.isEmpty()){auto previous=bad.last().toObject();if(qint64(previous.value("offset").toDouble())+qint64(previous.value("bytes").toDouble())==offset){previous.insert("bytes",previous.value("bytes").toDouble()+bytes);bad.replace(bad.size()-1,previous);return;}}if(bad.size()<10000)bad.append(QJsonObject{{"offset",double(offset)},{"bytes",double(bytes)}});else rangesTruncated=true;};
 if(options.resume){
  if(!isRegularSource(destination)||!isRegularSource(mapPath)||!output.open(QIODevice::ReadWrite)||!journal.open(QIODevice::ReadWrite))return fail("Resume requires an existing regular image and its original read map.");
  const auto line=journal.readLine(65536);if(!line.endsWith('\n'))return fail("Invalid read-map header");auto header=QJsonDocument::fromJson(line).object();
  if(header.value("schema").toInt()!=1||header.value("identity").toString()!=source.identity||header.value("sourceBytes").toString()!=QString::number(source.bytes)||header.value("destination").toString()!=info.absoluteFilePath()||header.value("destinationId").toString()!=fileIdentity(output)||header.value("sectorBytes").toInt()!=options.sectorBytes)return fail("Source, destination identity, length or sector settings changed. Resume refused.");
  qint64 validEnd=journal.pos();
  while(!journal.atEnd()){
   if(ctx.cancelled())return QJsonObject{{"status","cancelled"},{"operation","media_rescue"},{"message","Resume validation cancelled; no image data changed."}};
   auto row=journal.readLine(1024*1024);if(!row.endsWith('\n')){if(!journal.atEnd())return fail("Oversized read-map row");break;}
   auto e=QJsonDocument::fromJson(row).object();bool number=false;auto offset=e.value("offset").toString().toLongLong(&number);int n=e.value("bytes").toInt();
   if(!number||offset!=processed||n!=qMin(Chunk,source.bytes-processed)||n<=0||!output.seek(offset))return fail("Read map has corrupt, missing or reordered ranges");auto block=output.read(n);if(block.size()!=n||digest(block)!=e.value("sha256").toString())return fail("Image/read-map SHA-256 mismatch; resume refused");
   if(!e.value("bad").isArray())return fail("Invalid bad-range map");qint64 end=offset;
   for(auto value:e.value("bad").toArray()){auto a=value.toArray();if(a.size()!=2)return fail("Invalid bad range");auto p=qint64(a[0].toDouble(-1)),len=qint64(a[1].toDouble(-1));if(p<end||len<=0||p%options.sectorBytes||len%options.sectorBytes||p>offset+n||len>offset+n-p||block.mid(p-offset,len)!=QByteArray(len,0))return fail("Bad range outside mapped zero-filled area");addBad(p,len);end=p+len;}
   hash.addData(block);processed+=n;validEnd=journal.pos();ctx.update(processed,source.bytes*2);
  }
  // A crash may leave uncommitted image bytes or a partial journal line. Only discard those tails after full validation.
  if(ctx.cancelled())return QJsonObject{{"status","cancelled"},{"operation","media_rescue"},{"message","Resume validation cancelled"}};
  if(!output.resize(processed)||!journal.resize(validEnd)||!output.seek(processed)||!journal.seek(validEnd))return fail("Cannot remove uncommitted checkpoint tails");
 }else{
  if(info.exists()||QFileInfo::exists(mapPath))return fail("Destination exists; select Resume explicitly or choose a new path.");
  if(!output.open(QIODevice::ReadWrite|QIODevice::NewOnly))return fail("Cannot create destination image");
  if(!journal.open(QIODevice::ReadWrite|QIODevice::NewOnly)){output.close();output.remove();return fail("Cannot create read map");}
  const auto id=fileIdentity(output);if(id.isEmpty())return fail("Cannot establish destination file identity");
  if(!append(journal,{{"schema",1},{"identity",source.identity},{"sourceBytes",QString::number(source.bytes)},{"destination",info.absoluteFilePath()},{"destinationId",id},{"sectorBytes",options.sectorBytes}}))return fail("Cannot persist checkpoint header");
 }
 QStorageInfo storage(info.absolutePath());storage.refresh();if(!storage.isValid()||storage.isReadOnly()||storage.bytesAvailable()<source.bytes-processed+64*1024*1024)return fail("Destination has insufficient remaining space plus reserve; checkpoint retained.");
 while(processed<source.bytes&&!ctx.cancelled()){
  const auto count=qMin(Chunk,source.bytes-processed);auto block=source.read(processed,count);QJsonArray chunkBad;
  if(block.size()!=count){block=QByteArray(count,0);for(qint64 p=0;p<count;p+=options.sectorBytes){if(ctx.cancelled())break;QByteArray part;
    for(int attempt=0;attempt<=options.retries&&!ctx.cancelled();++attempt){part=source.read(processed+p,options.sectorBytes);if(part.size()==options.sectorBytes)break;}
    if(part.size()==options.sectorBytes)block.replace(p,part.size(),part);else chunkBad.append(QJsonArray{double(processed+p),options.sectorBytes});
   }if(ctx.cancelled())break;
  }
  if(!output.seek(processed)||output.write(block)!=count||!sync(output)){error="Image write/sync failed; committed checkpoint retained.";break;}
  if(!append(journal,{{"offset",QString::number(processed)},{"bytes",int(count)},{"sha256",digest(block)},{"bad",chunkBad}})){error="Checkpoint commit failed. Resume revalidates the last committed prefix.";break;}
  for(auto value:chunkBad){auto a=value.toArray();addBad(qint64(a[0].toDouble()),qint64(a[1].toDouble()));}hash.addData(block);processed+=count;ctx.update(processed,source.bytes*2);
 }
 QString status=!error.isEmpty()?"error":ctx.cancelled()?"cancelled":failed?"mismatch":"completed";bool verified=false;
 if(processed==source.bytes&&error.isEmpty()&&!ctx.cancelled()){
  if(!output.seek(0))error="Cannot seek for readback";QCryptographicHash check(QCryptographicHash::Sha256);qint64 n=0;
  while(error.isEmpty()&&n<processed&&!ctx.cancelled()){auto b=output.read(qMin(Chunk,processed-n));if(b.isEmpty()){error="Readback failed";break;}check.addData(b);n+=b.size();ctx.update(processed+n,source.bytes*2);}
  verified=!ctx.cancelled()&&error.isEmpty()&&n==processed&&check.result()==hash.result();if(!verified){status=ctx.cancelled()?"cancelled":"error";if(error.isEmpty())error="Readback incomplete or mismatched";}
 }
 return {{"status",status},{"operation","media_rescue"},{"destination",info.absoluteFilePath()},{"readMap",mapPath},{"processedBytes",double(processed)},{"unreadableBytes",double(failed)},{"unreadableRanges",bad},{"rangeSummaryTruncated",rangesTruncated},{"allRangesInReadMap",true},{"granularityBytes",options.sectorBytes},{"retries",options.retries},{"resumed",options.resume},{"sha256Written",QString::fromLatin1(hash.result().toHex())},{"imageReadbackVerified",verified},{"partialImageRetained",processed<source.bytes},{"physicalRepairPerformed",false},{"message",error.isEmpty()?"Keep image and JSONL read map together. Unreadable sectors are zero-filled. Complete readback verifies the image copy, not original content or physical repair.":error}};
}
QJsonObject retryRescueStream(RescueSource &source,const QString &previous,const QString &destination,const RescueOptions &options,const Context &ctx){
 auto fail=[](const QString &m){return QJsonObject{{"status","error"},{"operation","media_retry"},{"message",m}};};
 if(source.bytes<=0||source.bytes>((qint64(1)<<53)-4096)||!source.read||source.identity.isEmpty()||!QList<int>{512,4096}.contains(options.sectorBytes)||options.retries<0||options.retries>5)return fail("Invalid source or retry options");
 if(!isRegularSource(previous)||!isRegularSource(previous+".readmap.jsonl")||QFileInfo(previous).absoluteFilePath()==QFileInfo(destination).absoluteFilePath())return fail("Choose an existing image/map and a different destination. Original evidence is preserved.");
 QFile image(previous),map(previous+".readmap.jsonl");
 if(!image.open(QIODevice::ReadOnly)||!map.open(QIODevice::ReadOnly)||image.size()!=source.bytes)return fail("A full-length prior image and original map are required; finish the first rescue before selective retry.");
 QFile existingDestination(destination);if(existingDestination.open(QIODevice::ReadOnly)&&fileIdentity(existingDestination)==fileIdentity(image))return fail("Destination aliases original image; refused");existingDestination.close();
 auto line=map.readLine(65536);if(!line.endsWith('\n'))return fail("Invalid map header");auto h=QJsonDocument::fromJson(line).object();
 if(h.value("schema").toInt()!=1||h.value("identity").toString()!=source.identity||h.value("sourceBytes").toString()!=QString::number(source.bytes)||h.value("destination").toString()!=QFileInfo(previous).absoluteFilePath()||h.value("destinationId").toString()!=fileIdentity(image)||h.value("sectorBytes").toInt()!=options.sectorBytes)return fail("Prior image/source/map identity or sector size changed; selective retry refused.");
 struct Range{qint64 begin,end;};QList<Range> bad; qint64 processed=0,badBytes=0;
 // Verify every committed chunk before opening a new destination or reading failed media.
 while(!map.atEnd()){
  if(ctx.cancelled())return QJsonObject{{"status","cancelled"},{"operation","media_retry"},{"message","Map validation cancelled; no source sectors retried."}};
  line=map.readLine(1024*1024);if(!line.endsWith('\n'))return fail("Partial or oversized map row; finish the original rescue first");
  auto e=QJsonDocument::fromJson(line).object();bool ok=false;auto offset=e.value("offset").toString().toLongLong(&ok);auto count=e.value("bytes").toInt();
  if(!ok||offset!=processed||count<=0||count!=qMin(Chunk,source.bytes-processed)||!image.seek(offset))return fail("Corrupt or out-of-bounds map");auto block=image.read(count);
  if(block.size()!=count||digest(block)!=e.value("sha256").toString()||!e.value("bad").isArray())return fail("Original image SHA-256/map mismatch");
  qint64 end=offset;
  for(auto value:e.value("bad").toArray()){
   auto a=value.toArray();if(a.size()!=2||!a[0].isDouble()||!a[1].isDouble())return fail("Invalid failed range");double pd=a[0].toDouble(-1),ld=a[1].toDouble(-1);
   if(pd<0||ld<=0||pd>source.bytes||ld>source.bytes||pd!=double(qint64(pd))||ld!=double(qint64(ld)))return fail("Invalid failed range number");qint64 pos=qint64(pd),len=qint64(ld);
   if(options.sectorBytes<=0||pos<end||pos%options.sectorBytes||len%options.sectorBytes||pos>offset+count||len>offset+count-pos||block.mid(pos-offset,len)!=QByteArray(len,0))return fail("Failed range outside recorded zero-filled sectors");
   if(!bad.isEmpty()&&bad.last().end==pos)bad.last().end=pos+len;else bad.append({pos,pos+len});badBytes+=len;end=pos+len;
   if(bad.size()>2000000)return fail("Too many disjoint failed ranges; split the recovery task");
  }
  processed+=count;ctx.update(processed,source.bytes*3);
 }
 if(processed!=source.bytes)return fail("Incomplete map; resume the original rescue first");
 if(bad.isEmpty())return fail("No unreadable sectors recorded; selective retry is unnecessary");
 qint64 sourceReadBytes=0;auto overlaps=[&](qint64 pos,qint64 count){
  auto it=std::lower_bound(bad.cbegin(),bad.cend(),pos,[](const Range &r,qint64 p){return r.end<=p;});return it!=bad.cend()&&it->begin<pos+count;
 };
 RescueSource selective{source.bytes,source.identity,[&](qint64 pos,qint64 count)->QByteArray{
  if(pos<0||count<=0||pos>source.bytes||count>source.bytes-pos)return {};
  if(overlaps(pos,count)){
   // Force the existing sector retry path; never read healthy physical sectors.
   if(count!=options.sectorBytes)return {};
   sourceReadBytes+=count;return source.read(pos,count);
  }
  if(!image.seek(pos))return {};return image.read(count);
 }};
 Context progress{ctx.cancel,[&](qint64 n,qint64){ctx.update(source.bytes+n,source.bytes*3);}};
 auto result=rescueStream(selective,destination,options,progress);result.insert("operation","media_retry");result.insert("previousImage",QFileInfo(previous).absoluteFilePath());result.insert("originalImagePreserved",true);result.insert("previousUnreadableBytes",double(badBytes));result.insert("physicalReadAttemptBytes",double(sourceReadBytes));
 if(result.value("imageReadbackVerified").toBool())result.insert("newlyRecoveredBytes",double(badBytes)-result.value("unreadableBytes").toDouble());
 return result;
}

}
