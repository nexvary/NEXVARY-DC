#include "Partitions.h"
#include <QSet>
#include <QtEndian>
namespace dc {
namespace {
quint32 le32(const QByteArray &b,int p){return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(b.constData()+p));}
quint64 le64(const QByteArray &b,int p){return qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(b.constData()+p));}
quint32 crc32(const QByteArray &b){quint32 c=~0u;for(uchar x:b){c^=x;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
bool extended(int t){return t==5||t==15||t==0x85;}
}
QList<ImageVolume> imageVolumes(QFile &f,QStringList &warnings){
 QList<ImageVolume> out{{0,f.size(),"volume"}};quint64 sectorBytes=512,sectors=quint64(f.size()/512);
 auto add=[&](quint64 lba,quint64 count,const QString &scheme){
  if(!lba||!count||lba>=sectors||count>sectors-lba){warnings<<"Partition outside image";return;}
  for(const auto &v:out)if(v.offset>0&&qint64(lba*sectorBytes)<v.offset+v.length&&v.offset<qint64((lba+count)*sectorBytes)){warnings<<"Overlapping partition extent rejected";return;}
  out.append({qint64(lba*sectorBytes),qint64(count*sectorBytes),scheme});
 };
 if(!f.seek(0))return out;auto m=f.read(512);if(m.size()!=512||uchar(m[510])!=85||uchar(m[511])!=170)return out;
 bool protective=false;for(int p=446;p<510;p+=16)protective|=uchar(m[p+4])==0xee;
 if(protective){
  // Try primary, then backup header. Both header and table CRC are mandatory.
  bool accepted=false;
  for(quint64 bytes:QList<quint64>{512,4096}){
   sectorBytes=bytes;sectors=quint64(f.size()/qint64(bytes));
  for(quint64 headerLba:QList<quint64>{1,sectors?sectors-1:0}){
   if(!f.seek(qint64(headerLba*sectorBytes)))continue;auto h=f.read(qint64(sectorBytes));
   if(quint64(h.size())!=sectorBytes||h.left(8)!="EFI PART"||le32(h,8)!=0x10000||le32(h,20)!=0)continue;
   auto hs=le32(h,12),expected=le32(h,16);if(hs<92||hs>sectorBytes)continue;
   auto header=h.left(hs);for(int j=16;j<20;++j)header[j]=0;
   if(crc32(header)!=expected||le64(h,24)!=headerLba)continue;
   auto lba=le64(h,72);auto n=le32(h,80),size=le32(h,84);quint64 len=quint64(n)*size;
   if(!n||n>1048576||size<128||size>4096||size%128||len>64*1024*1024||lba>=sectors||len>quint64(f.size())-lba*sectorBytes)continue;
   if(!f.seek(qint64(lba*sectorBytes)))continue;auto table=f.read(qint64(len));if(quint64(table.size())!=len||crc32(table)!=le32(h,88))continue;
   const auto first=le64(h,40),last=le64(h,48);if(first>last||last>=sectors)continue;
   for(quint32 i=0;i<n;++i){int p=int(quint64(i)*size);if(table.mid(p,16)==QByteArray(16,0))continue;auto a=le64(table,p+32),b=le64(table,p+40);if(a<first||b>last||b<a){warnings<<"Invalid GPT extent";continue;}add(a,b-a+1,"GPT");}
   accepted=true;if(headerLba!=1)warnings<<"Using backup GPT; primary invalid";break;
  }
   if(accepted)break;
  }
  if(!accepted)warnings<<"GPT header/table checksum or bounds invalid";
  return out; // Never reinterpret protective/hybrid entries after a rejected GPT.
 }
 for(int p=446;p<510;p+=16){int type=uchar(m[p+4]);quint64 base=le32(m,p+8),count=le32(m,p+12);if(!type)continue;
  if(!extended(type)){add(base,count,"MBR");continue;}
  if(!base||base>=sectors||!count||count>sectors-base){warnings<<"Invalid extended container";continue;}
  quint64 next=base;QSet<quint64> seen;
  while(next>=base&&next<base+count){
   if(seen.contains(next)||seen.size()>=4096){warnings<<"EBR cycle or traversal limit";break;}seen.insert(next);
   if(!f.seek(qint64(next*512)))break;auto e=f.read(512);if(e.size()!=512||uchar(e[510])!=85||uchar(e[511])!=170){warnings<<"Invalid EBR";break;}
   quint64 rel=le32(e,454),len=le32(e,458);
   if(uchar(e[450])&&!extended(uchar(e[450]))&&rel&&rel<base+count-next&&len<=base+count-next-rel)add(next+rel,len,"MBR logical");
   else if(uchar(e[450]))warnings<<"Logical partition outside extended container";
   if(!uchar(e[466]))break;if(!extended(uchar(e[466]))){warnings<<"Invalid EBR link";break;}
   quint64 link=le32(e,470);if(!link||link>=count){warnings<<"EBR link outside container";break;}next=base+link;
  }
 }
 return out;
}
}
