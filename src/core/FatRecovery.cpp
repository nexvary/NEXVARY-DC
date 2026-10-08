#include "Recovery.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QStorageInfo>
#include <QCryptographicHash>
#include <QtEndian>
namespace dc {
namespace {
quint16 u16(const QByteArray &b,int p){return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(b.constData()+p));}
quint32 u32(const QByteArray &b,int p){return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(b.constData()+p));}
struct Fat {
 qint64 start=0,end=0,data=0,clusterBytes=0;
 quint32 clusters=0,root=0;QByteArray table;
 quint32 next(quint32 c) const {return c*4ULL+4<=quint64(table.size())?u32(table,int(c*4))&0x0fffffff:0x0ffffff7;}
 bool valid(quint32 c) const {return c>=2 && c<clusters+2;}
 qint64 offset(quint32 c) const {return data+qint64(c-2)*clusterBytes;}
};
bool geometry(QFile &f,qint64 start,qint64 extent,Fat &fat) {
 if(!f.seek(start))return false;const auto b=f.read(512);
 if(b.size()!=512 || uchar(b[510])!=0x55 || uchar(b[511])!=0xaa)return false;
 const auto sector=u16(b,11);const auto spc=uchar(b[13]);const auto reserved=u16(b,14);const auto count=uchar(b[16]);const auto sectors=u32(b,32);const auto size=u32(b,36);
 if(!QList<int>{512,1024,2048,4096}.contains(sector) || !spc || spc>128 || (spc&(spc-1)) || !reserved || count<1 || count>2 || u16(b,17)!=0 || u16(b,22)!=0 || !size || !sectors)return false;
 const auto overhead=qint64(reserved)+qint64(count)*size;
 if(overhead>=sectors || qint64(sectors)*sector>extent || start+qint64(sectors)*sector>f.size() || qint64(size)*sector>64*1024*1024)return false;
 const auto clusters=quint32((sectors-overhead)/spc);
 if(clusters<65525 || clusters>0x0fffffed || quint64(clusters+2)*4>quint64(size)*sector)return false;
 int active=0;const auto flags=u16(b,40);if(flags&0x80){active=flags&15;if(active>=count)return false;}
 if(!f.seek(start+(qint64(reserved)+qint64(active)*size)*sector))return false;
 auto table=f.read(qint64(size)*sector);if(table.size()!=qint64(size)*sector)return false;
 fat={start,start+qint64(sectors)*sector,start+overhead*sector,qint64(spc)*sector,clusters,u32(b,44)&0x0fffffff,table};
 return fat.valid(fat.root);
}
}
QJsonObject recoverFat32(const QString &source,const QString &directory,const Context &ctx) {
 auto error=[](const QString &m){return QJsonObject{{"status","error"},{"operation","fat32_recovery"},{"message",m}};};
 if(!isRegularSource(source) || !QFileInfo(directory).isDir() || QFileInfo(directory).isSymLink())return error("Choose a regular image and an existing local recovery directory.");
 const auto canonical=QFileInfo(directory).canonicalFilePath();if(canonical.startsWith("//") || canonical.startsWith("/dev/"))return error("Recovery destination must be local.");
 QFile f(source);if(!f.open(QIODevice::ReadOnly))return error("Cannot open image.");
 const auto imageSize=f.size();Fat fat;QList<Fat> volumes;
 if(geometry(f,0,imageSize,fat))volumes.append(fat);
 else {
  f.seek(0);const auto mbr=f.read(512);
  if(mbr.size()==512 && uchar(mbr[510])==0x55 && uchar(mbr[511])==0xaa)for(int i=0;i<4;i++) {
   const int p=446+i*16;const auto type=uchar(mbr[p+4]);if(type!=0x0b && type!=0x0c)continue;
   const auto start=qint64(u32(mbr,p+8))*512,extent=qint64(u32(mbr,p+12))*512;
   if(start>=512 && extent>0 && start<=imageSize && extent<=imageSize-start && geometry(f,start,extent,fat))volumes.append(fat);
  }
 }
 if(volumes.isEmpty())return error("No supported FAT32 volume found. Supports a FAT32 volume image or FAT32 primary MBR partitions; GPT, exFAT, NTFS and extended partitions are not supported.");
 QTemporaryDir output(QDir(canonical).filePath("NEXVARY-FAT32-XXXXXX"));if(!output.isValid())return error("Cannot create output folder.");output.setAutoRemove(false);
 QJsonArray files;int skipped=0,visited=0;QString status="completed",message;
 for(const auto &v:volumes) {
  QList<quint32> queue{v.root};QSet<quint32> directories,seen;
  while(!queue.isEmpty() && status=="completed") {
   const auto first=queue.takeFirst();if(directories.contains(first))continue;directories.insert(first);
   quint32 c=first;
   while(v.valid(c) && status=="completed") {
    if(ctx.cancelled()){status="cancelled";break;}
    if(seen.contains(c) || ++visited>100000){status="partial";message="Directory chain loop, cross-link or scan limit reached.";break;}
    seen.insert(c);if(!f.seek(v.offset(c))){status="error";message="Directory seek failed.";break;}
    const auto entries=f.read(v.clusterBytes);if(entries.size()!=v.clusterBytes){status="error";message="Directory read failed.";break;}
    bool last=false;
    for(int p=0;p+32<=entries.size();p+=32) {
     if(ctx.cancelled()){status="cancelled";break;}
     const auto e=entries.mid(p,32);const auto marker=uchar(e[0]),attr=uchar(e[11]);
     if(marker==0){last=true;break;}if(attr==0x0f || (attr&8))continue;
     const auto cluster=((quint32(u16(e,20))<<16)|u16(e,26))&0x0fffffff;
     if(marker!=0xe5){if((attr&16) && marker!='.' && v.valid(cluster))queue.append(cluster);if(queue.size()+directories.size()>100000){status="partial";message="Directory traversal limit reached.";break;}continue;}
     const auto size=u32(e,28);
     if((attr&16) || !size || size>64*1024*1024 || !v.valid(cluster)){++skipped;continue;}
     const auto blocks=(qint64(size)+v.clusterBytes-1)/v.clusterBytes;
     if(blocks>v.clusters || quint64(cluster)+blocks>quint64(v.clusters)+2){++skipped;continue;}
     bool free=true;for(qint64 i=0;i<blocks;i++)if(v.next(cluster+quint32(i))!=0){free=false;break;}
     // A deleted entry has no reliable chain; only attempt contiguous, still-free clusters.
     if(!free){++skipped;continue;}
     if(!f.seek(v.offset(cluster))){++skipped;continue;}const auto bytes=f.read(size);if(bytes.size()!=size){++skipped;continue;}
     QStorageInfo storage(output.path());storage.refresh();if(!storage.isValid() || storage.isReadOnly() || storage.bytesAvailable()<qint64(size)+64*1024*1024){status="error";message="Insufficient destination space.";break;}
     QString shortName="_"+QString::fromLatin1(e.mid(1,7)).trimmed();const auto ext=QString::fromLatin1(e.mid(8,3)).trimmed();if(!ext.isEmpty())shortName+="."+ext;
     for(auto &ch:shortName)if(!ch.isLetterOrNumber() && ch!='.' && ch!='_' && ch!='-')ch='_';
     const auto name=QString::number(files.size()+1)+"_"+shortName;QFile out(output.filePath(name));
     if(!out.open(QIODevice::WriteOnly|QIODevice::NewOnly) || out.write(bytes)!=bytes.size() || !out.flush()){out.close();out.remove();status="error";message="Destination write failed.";break;}out.close();
     const auto hash=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256);QFile verify(out.fileName());
     if(!verify.open(QIODevice::ReadOnly) || QCryptographicHash::hash(verify.readAll(),QCryptographicHash::Sha256)!=hash){verify.close();out.remove();status="error";message="Recovered file readback mismatch.";break;}
     files.append(QJsonObject{{"file",name},{"directoryEntryOffset",double(v.offset(c)+p)},{"offset",double(v.offset(cluster))},{"bytes",double(size)},{"sha256",QString::fromLatin1(hash.toHex())},{"validation","Deleted FAT32 entry; contiguous free-cluster candidate, content integrity unknown"}});
     if(files.size()>=10000){status="partial";message="10000 file limit reached.";break;}
    }
    ctx.update(v.offset(c),imageSize);if(last)break;c=v.next(c);if(c>=0x0ffffff8)break;
   }
  }
  if(status!="completed")break;
 }
 if(f.size()!=imageSize){status="error";message="Source image size changed.";}
 QJsonObject result{{"status",status},{"operation","fat32_recovery"},{"source",QFileInfo(source).absoluteFilePath()},{"destination",output.path()},{"recoveredCount",files.size()},{"skippedEntries",skipped},{"files",files},{"message",message},{"scope","FAT32 deleted contiguous files with still-free clusters. First short-name character is lost; long names, folder hierarchy and fragmented files are not restored. Free clusters do not prove data was not overwritten. Review recovered content."}};
 QFile manifest(output.filePath("manifest.json"));const auto json=QJsonDocument(result).toJson();if(!manifest.open(QIODevice::WriteOnly|QIODevice::NewOnly) || manifest.write(json)!=json.size() || !manifest.flush())result.insert("manifestWarning","Manifest failed; export report.");
 return result;
}
}
