#include "FilesystemRecovery.h"
#include "Partitions.h"
#include <QFile>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QStorageInfo>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QSet>
#include <QMap>
#include <QRegularExpression>
#include <QtEndian>
#include <limits>
#include <algorithm>
namespace dc {
namespace {
quint16 u16(const QByteArray &b,int p){return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(b.constData()+p));}
quint32 u32(const QByteArray &b,int p){return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(b.constData()+p));}
quint64 u64(const QByteArray &b,int p){return qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(b.constData()+p));}
QString utf16(const QByteArray &b,int p,int n){QString s;for(int i=0;i<n;++i)s+=QChar(u16(b,p+2*i));return s;}
QString safe(QString s){for(auto &c:s)if(c.unicode()<32||QString("/\\:*?\"<>|").contains(c))c='_';while(s.endsWith('.')||s.endsWith(' '))s.chop(1);if(s.isEmpty()||s=="."||s=="..")s="unnamed";if(QRegularExpression("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])($|\\.)",QRegularExpression::CaseInsensitiveOption).match(s).hasMatch())s="_"+s;return s.left(180);}
struct Run {qint64 offset,length;}; // offset -1 means logical zeros, never missing evidence.
struct Stream {QList<Run> runs; QByteArray resident; qint64 size=0,initialized=0;bool valid=false;mutable QList<qint64> ends;};
struct Image {
 QFile &f; ImageVolume v; qint64 cacheOffset=-1;QByteArray cache;
 QByteArray read(qint64 off,qint64 n){
  if(off<0||n<0||off>v.length||n>v.length-off||n>32*1024*1024)return {};
  if(n<=4096){if(cacheOffset<0||off<cacheOffset||off+n>cacheOffset+cache.size()){cacheOffset=(off/65536)*65536;if(!f.seek(v.offset+cacheOffset))return {};cache=f.read(qMin<qint64>(65536,v.length-cacheOffset));}if(off+n<=cacheOffset+cache.size())return cache.mid(off-cacheOffset,n);}
  if(!f.seek(v.offset+off))return {};return f.read(n);
 }
 QByteArray read(const Stream &s,qint64 pos,qint64 n){
  if(!s.valid||pos<0||n<0||pos>s.size||n>s.size-pos||n>32*1024*1024)return {};
  if(!s.resident.isNull())return s.resident.mid(pos,n);
  QByteArray out;out.reserve(n);
  if(s.ends.size()!=s.runs.size()){s.ends.clear();qint64 end=0;for(const auto &r:s.runs){end+=r.length;s.ends.append(end);}}
  auto it=std::upper_bound(s.ends.cbegin(),s.ends.cend(),pos);qsizetype index=it-s.ends.cbegin();qint64 logical=index?s.ends[index-1]:0;
  for(;index<s.runs.size();++index){const auto &r=s.runs[index];auto within=pos-logical,take=qMin(n-qint64(out.size()),r.length-within);if(take<=0)break;
   auto b=r.offset<0?QByteArray(take,0):read(r.offset+within,take);if(b.size()!=take)return {};out+=b;pos+=take;logical+=r.length;if(out.size()==n)break;}
  return out;
 }
};
struct Output {
 Image &im;QString root;const Context &ctx;const RecoveryOptions &options;QJsonArray files,issues;QString status="completed";int skipped=0;quint64 examined=0;
 void issue(const QString &name,const QString &why){++skipped;if(issues.size()<1000)issues.append(QJsonObject{{"name",name},{"reason",why}});}
 bool running(){if(ctx.cancelled())status="cancelled";return status=="completed";}
 void save(const QString &name,const QString &folder,const Stream &s,const QString &evidence,bool candidate=false){
  if(!running())return;if(files.size()>=options.maxFiles){status="partial";issue(name,"Configured file limit reached");return;}
  if(!s.valid||s.size<0||s.size>im.v.length){issue(name,"Invalid or incomplete allocation metadata");return;}
  QStorageInfo space(root);space.refresh();if(!space.isValid()||space.isReadOnly()||space.bytesAvailable()<64*1024*1024||s.size>space.bytesAvailable()-64*1024*1024){status="error";issue(name,"Insufficient destination space");return;}
  auto relative=folder+"/"+safe(name);auto path=QDir(root).filePath(relative);if(QFileInfo::exists(path)){relative=folder+"/"+QString::number(files.size()+1)+"_"+safe(name);path=QDir(root).filePath(relative);}
  if(!QDir().mkpath(QFileInfo(path).absolutePath())){status="error";issue(name,"Cannot create output directory");return;}
  QFile file(path);if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)){status="error";issue(name,"Cannot create output file");return;}
  QCryptographicHash hash(QCryptographicHash::Sha256);qint64 done=0;QString condition=candidate?"candidate":"complete";
  while(done<s.size){if(!running()){condition="partial";break;}const auto n=qMin<qint64>(1048576,s.size-done);
   QByteArray b;if(done>=s.initialized)b=QByteArray(n,0);else {auto valid=qMin(n,s.initialized-done);b=im.read(s,done,valid);if(b.size()==valid)b+=QByteArray(n-valid,0);}
   if(b.size()!=n){condition="partial";issue(name,"Source read failed");break;}if(file.write(b)!=n){condition="partial";status="error";issue(name,"Destination write failed");break;}
   hash.addData(b);done+=n;ctx.update(done,s.size);
  }
  if(!file.flush()){condition="partial";status="error";}file.close();
  QFile check(path);QCryptographicHash verify(QCryptographicHash::Sha256);bool verified=check.open(QIODevice::ReadOnly)&&verify.addData(&check)&&check.size()==done&&verify.result()==hash.result();
  if(!verified){condition="partial";status="error";}
  if(done<s.size)condition="partial";
  files.append(QJsonObject{{"file",relative},{"originalName",name},{"expectedBytes",double(s.size)},{"bytes",double(done)},{"sha256",QString::fromLatin1(hash.result().toHex())},{"readbackVerified",verified},{"condition",condition},{"contentIntegrity","unknown; compare with an original or inspect content"},{"evidence",evidence}});
 }
};
// NTFS USA fixups are mandatory before any attribute is interpreted.
bool fixup(QByteArray &r,int sector){
 if(r.size()<48||r.left(4)!="FILE"||sector<512||r.size()%sector)return false;int p=u16(r,4),n=u16(r,6);
 if(n!=r.size()/sector+1||p<40||p+2*n>r.size())return false;auto usn=r.mid(p,2);
 for(int i=1;i<n;++i){int end=i*sector-2;if(r.mid(end,2)!=usn)return false;r.replace(end,2,r.mid(p+2*i,2));}return true;
}
QList<QByteArray> attributes(const QByteArray &r,bool &ok){
 QList<QByteArray> out;ok=false;if(r.size()<48)return out;quint32 used=u32(r,24);int p=u16(r,20);if(used>quint32(r.size())||p<48||p%8)return out;
 while(quint64(p)+4<=used){if(u32(r,p)==0xffffffff){ok=true;break;}if(quint64(p)+24>used)break;auto n=u32(r,p+4);if(n<24||n%8||n>used-quint32(p))break;out<<r.mid(p,n);p+=int(n);}return out;
}
Stream ntStream(const QByteArray &a,qint64 cluster,qint64 volume,bool extent=false){
 Stream s;if(a.size()<24)return s;
 if(uchar(a[8])==0){auto n=u32(a,16);int p=u16(a,20);if(p<24||quint64(p)+n>quint64(a.size()))return s;s.resident=a.mid(p,n);if(n==0)s.resident=QByteArray("");s.size=s.initialized=n;s.valid=true;return s;}
 if(uchar(a[8])!=1||a.size()<64||(!extent&&u64(a,16)!=0)||(u16(a,12)&0x40ff))return s; // compressed/encrypted require a decoder
 auto size=u64(a,48),initialized=u64(a,56),highest=u64(a,24);int p=u16(a,32);if(size>quint64(volume)||initialized>size||p<64||p>=a.size())return s;
 qint64 lcn=0,total=0;bool terminated=false;
 while(p<a.size()){
  int h=uchar(a[p++]);if(!h){terminated=true;break;}int ns=h&15,os=h>>4;if(ns<1||ns>8||os>8||p+ns+os>a.size())return {};
  quint64 count=0,raw=0;for(int j=0;j<ns;++j)count|=quint64(uchar(a[p++]))<<(8*j);if(!count||count>quint64(volume/cluster)||count>quint64((volume-total)/cluster))return {};
  for(int j=0;j<os;++j)raw|=quint64(uchar(a[p++]))<<(8*j);
  if(os){if(os<8&&(raw&(quint64(1)<<(os*8-1))))raw|=(~quint64(0))<<(os*8);const auto delta=qint64(raw);
   if((delta>0&&lcn>std::numeric_limits<qint64>::max()-delta)||(delta<0&&delta < -lcn))return {};lcn+=delta;
   if(lcn<0||lcn>volume/cluster||count>quint64(volume/cluster-lcn))return {};}
  s.runs.append({os?lcn*cluster:-1,qint64(count)*cluster});total+=qint64(count)*cluster;
 }
 auto lowest=u64(a,16);if(lowest>quint64(volume/cluster)||!terminated||(!extent&&total<qint64(size))||!total||highest!=lowest+quint64(total/cluster)-1)return {};
 s.size=qint64(size);s.initialized=qint64(initialized);s.valid=true;return s;
}
struct NtName {QString name;quint64 parent=0;quint16 parentSequence=0,sequence=0;bool dir=false,live=false;};
bool ntfs(Image &im,Output &out){
 auto boot=im.read(0,512);if(boot.size()!=512||boot.mid(3,8)!="NTFS    ")return false;
 int sector=u16(boot,11),spc=uchar(boot[13]);if(!QList<int>{512,1024,2048,4096}.contains(sector)||!spc||(spc&(spc-1))||spc>128){out.issue("NTFS","Invalid geometry");return true;}
 qint64 cluster=qint64(sector)*spc;auto sectors=u64(boot,40),mftCluster=u64(boot,48);int code=qint8(boot[64]);qint64 record=code<0?(code>=-16?qint64(1)<<(-code):0):qint64(code)*cluster;
 if(!sectors||sectors>quint64(im.v.length/sector)||record<512||record>65536||record%sector||mftCluster>=sectors/spc){out.issue("NTFS","Invalid MFT geometry");return true;}
 im.v.length=qint64(sectors)*sector;
 auto first=im.read(qint64(mftCluster)*cluster,record);if(!fixup(first,sector)){out.issue("$MFT","Invalid fixup");return true;}bool ok=false;auto attrs=attributes(first,ok);Stream mft;
 if(ok)for(const auto &a:attrs){if(u32(a,0)==0x20){out.issue("$MFT","MFT attribute-list extensions unsupported");return true;}if(u32(a,0)==0x80&&a[9]==0)mft=ntStream(a,cluster,im.v.length);}
 if(!mft.valid||mft.initialized!=mft.size||mft.size%record){out.issue("$MFT","Missing or invalid data runlist");return true;}
 auto get=[&](quint64 id){auto r=im.read(mft,qint64(id)*record,record);if(!fixup(r,sector))r.clear();return r;};
 auto dataStream=[&](quint64 id,const QByteArray &base,const QList<QByteArray> &baseAttrs){
  QList<QByteArray> parts;QByteArray list;bool hasList=false;
  for(const auto &a:baseAttrs){if(u32(a,0)==0x80&&a[9]==0)parts.append(a);if(u32(a,0)==0x20){if(hasList)return Stream{};hasList=true;auto s=ntStream(a,cluster,im.v.length);if(!s.valid||s.size>1048576||s.initialized!=s.size)return Stream{};list=im.read(s,0,s.size);if(list.size()!=s.size)return Stream{};}}
  if(hasList){QSet<QString> refs;QList<QByteArray> listed;
   for(int pos=0;pos<list.size();){if(list.size()-pos<26)return Stream{};auto len=u16(list,pos+4);int names=uchar(list[pos+6]),nameOffset=uchar(list[pos+7]);if(len<26||len>list.size()-pos||(names&&(nameOffset<26||nameOffset+names*2>len)))return Stream{};
    if(u32(list,pos)==0x80&&!names){auto vcn=u64(list,pos+8),ref=u64(list,pos+16);quint64 owner=ref&0x0000ffffffffffffULL;quint16 sequence=ref>>48,attributeId=u16(list,pos+24);QString key=QString::number(owner)+":"+QString::number(attributeId);if(refs.contains(key)||refs.size()>=65536||owner>=quint64(mft.size/record))return Stream{};refs.insert(key);
     auto r=owner==id?base:get(owner);if(r.isEmpty()||u16(r,16)!=sequence||bool(u16(r,22)&1)!=bool(u16(base,22)&1))return Stream{};
     if(owner!=id){auto parent=u64(r,32);if((parent&0x0000ffffffffffffULL)!=id||(parent>>48)!=u16(base,16))return Stream{};}
     bool valid=false;auto as=attributes(r,valid);if(!valid)return Stream{};QByteArray found;
     for(const auto &a:as)if(u32(a,0)==0x80&&a[9]==0&&u16(a,14)==attributeId){if(!found.isEmpty())return Stream{};found=a;}
     if(found.isEmpty()||(found[8]==0?vcn!=0:found.size()<64||u64(found,16)!=vcn))return Stream{};listed.append(found);
    }pos+=len;
   }
   // Every base DATA extent must also be listed. Otherwise the evidence is
   // inconsistent and must not silently drop a local extent.
   for(const auto &a:parts){bool found=false;for(const auto &b:listed)if(a==b)found=true;if(!found)return Stream{};}parts=listed;
  }
  if(parts.isEmpty())return Stream{};if(parts.size()==1&&parts[0][8]==0)return ntStream(parts[0],cluster,im.v.length);
  for(const auto &a:parts)if(a.size()<64||a[8]!=1)return Stream{};
  std::sort(parts.begin(),parts.end(),[](const QByteArray &a,const QByteArray &b){return u64(a,16)<u64(b,16);});
  Stream joined;quint64 vcn=0;qint64 total=0;
  for(const auto &a:parts){if(u64(a,16)!=vcn)return Stream{};auto s=ntStream(a,cluster,im.v.length,true);if(!s.valid)return Stream{};if(!vcn){joined.size=s.size;joined.initialized=s.initialized;}
   for(const auto &run:s.runs){if(total>im.v.length-run.length||joined.runs.size()>=65536)return Stream{};joined.runs.append(run);total+=run.length;vcn+=quint64(run.length/cluster);}
  }
  if(total<joined.size)return Stream{};joined.valid=true;return joined;
 };
 Stream bitmap;auto br=get(6);if(!br.isEmpty()){auto ba=attributes(br,ok);if(ok)for(const auto &a:ba)if(u32(a,0)==0x80&&a[9]==0)bitmap=ntStream(a,cluster,im.v.length);}
 const auto count=qMin<qint64>(mft.size/record,out.options.maxRecords);if(count<mft.size/record){out.status="partial";out.issue("$MFT","Configured MFT record limit reached");return true;}
 QMap<quint64,NtName> names;
 for(qint64 i=0;i<count&&out.running();++i){auto r=get(i);if(r.isEmpty()){out.issue(QString::number(i),"Invalid MFT record/fixup");continue;}if(u64(r,32))continue;auto as=attributes(r,ok);if(!ok){out.issue(QString::number(i),"Invalid attribute bounds");continue;}
  NtName name;name.sequence=u16(r,16);name.live=u16(r,22)&1;name.dir=u16(r,22)&2;
  for(const auto &a:as)if(u32(a,0)==0x30&&a[8]==0){auto s=ntStream(a,cluster,im.v.length);auto b=s.resident;if(s.valid&&b.size()>=66&&66+2*uchar(b[64])<=b.size()&&(name.name.isEmpty()||uchar(b[65])!=2)){auto ref=u64(b,0);name.parent=ref&0x0000ffffffffffffULL;name.parentSequence=ref>>48;name.name=utf16(b,66,uchar(b[64]));}}
  if(!name.name.isEmpty())names.insert(i,name);out.ctx.update(i,count*2);
 }
 for(auto it=names.cbegin();it!=names.cend()&&out.running();++it){const auto &name=it.value();if(name.live||name.dir||it.key()<16)continue;
  auto r=get(it.key());auto as=attributes(r,ok);if(!ok){out.issue(name.name,"Record changed during scan");continue;}Stream data;
  data=dataStream(it.key(),r,as);
  if(!data.valid){out.issue(name.name,"Invalid attribute-list references, compression, encryption or invalid/missing data runs");continue;}
  bool free=true;if(data.resident.isNull()){
   if(!bitmap.valid||bitmap.initialized!=bitmap.size){out.issue(name.name,"Allocation bitmap unavailable");continue;}
   for(const auto &run:data.runs){if(run.offset<0)continue;quint64 c=run.offset/cluster,n=run.length/cluster;
    for(quint64 j=0;j<n&&out.running();){auto take=qMin<quint64>(n-j,32768);auto bit=c+j;auto b=im.read(bitmap,bit/8,(bit%8+take+7)/8);if(b.size()!=qint64((bit%8+take+7)/8)){free=false;break;}
     for(quint64 k=0;k<take;++k)if(uchar(b[(bit%8+k)/8])&(1<<((bit+k)%8))){free=false;break;}if(!free)break;j+=take;}if(!free)break;}
  }
  if(!free){out.issue(name.name,"Deleted data overlaps allocated clusters or bitmap is corrupt");continue;}
  QString folder="NTFS";QStringList parents;QSet<quint64> seen;auto parent=name.parent;auto seq=name.parentSequence;bool hierarchy=true;
  while(parent!=5){if(seen.contains(parent)||seen.size()>=64||!names.contains(parent)){hierarchy=false;break;}seen.insert(parent);auto p=names.value(parent);if(!p.dir||p.sequence!=seq){hierarchy=false;break;}parents.prepend(safe(p.name));parent=p.parent;seq=p.parentSequence;}
  if(!hierarchy)folder+="/_orphaned";else if(!parents.isEmpty())folder+="/"+parents.join('/');
  out.save(name.name,folder,data,"Deleted MFT record; USA verified; resident data or complete runlist with unallocated clusters. Complete means byte coverage, not proof against overwrite.");
 }
 return true;
}
bool exfat(Image &im,Output &out){
 auto b=im.read(0,512);if(b.size()!=512||b.mid(3,8)!="EXFAT   ")return false;
 int shift=uchar(b[108]),cshift=uchar(b[109]),nf=uchar(b[110]);if(shift<9||shift>12||cshift>25-shift||(nf!=1&&nf!=2)){out.issue("exFAT","Invalid geometry");return true;}
 qint64 sector=qint64(1)<<shift,cluster=sector<<cshift;quint64 sectors=u64(b,72);auto fat=u32(b,80),fatSize=u32(b,84),heap=u32(b,88),clusters=u32(b,92),root=u32(b,96);int active=(u16(b,106)&1);
 if(!sectors||sectors>quint64(im.v.length/sector)||!clusters||clusters>0xfffffff5||active>=nf||fat<24||quint64(fat)+quint64(nf)*fatSize>heap||quint64(heap)>=sectors||quint64(clusters)*cluster>(sectors-heap)*sector||quint64(clusters+2)*4>quint64(fatSize)*sector){out.issue("exFAT","Invalid volume bounds");return true;}
 im.v.length=qint64(sectors)*sector;
 auto boot=im.read(0,11*sector),sum=im.read(11*sector,sector);quint32 checksum=0;
 if(boot.size()!=11*sector||sum.size()!=sector){out.issue("exFAT","Truncated boot region");return true;}
 for(int i=0;i<boot.size();++i)if(i!=106&&i!=107&&i!=112)checksum=((checksum&1)?0x80000000:0)+(checksum>>1)+uchar(boot[i]);
 for(int i=0;i<sum.size();i+=4)if(u32(sum,i)!=checksum){out.issue("exFAT","Boot checksum mismatch");return true;}
 auto valid=[&](quint32 c){return c>=2&&quint64(c)<quint64(clusters)+2;};
 auto offset=[&](quint32 c){return qint64(heap)*sector+qint64(c-2)*cluster;};
 auto chain=[&](quint32 first,qint64 size,bool contiguous,bool directory){
  Stream s;if(size<0||size>im.v.length)return s;if(size==0&&!directory){s.valid=true;s.resident=QByteArray("");return s;}if(!valid(first))return s;
  if(contiguous){if(size<=0||quint64((size+cluster-1)/cluster)>quint64(clusters)+2-first)return s;s.runs<<Run{offset(first),((size+cluster-1)/cluster)*cluster};s.size=s.initialized=size;s.valid=true;return s;}
  QSet<quint32> seen;quint32 c=first;qint64 total=0;
  while(valid(c)){
   if(!out.running()||seen.contains(c)||seen.size()>=out.options.maxRecords)return Stream{};seen.insert(c);s.runs<<Run{offset(c),cluster};total+=cluster;
   if(size>0&&total>=size){s.size=s.initialized=size;s.valid=true;return s;}
   auto entry=im.read((qint64(fat)+qint64(active)*fatSize)*sector+qint64(c)*4,4);if(entry.size()!=4)return Stream{};c=u32(entry,0);
   if(c>=0xfffffff8){if(directory&&size==0){s.size=s.initialized=total;s.valid=true;}return s;}
  }return Stream{};
 };
 auto rootStream=chain(root,0,false,true);if(!rootStream.valid){out.issue("exFAT","Invalid root chain");return true;}Stream bitmap;
 for(qint64 p=0;p<rootStream.size&&out.running();p+=32){auto e=im.read(rootStream,p,32);if(e.size()!=32||e[0]==0)break;if(uchar(e[0])==0x81&&(uchar(e[1])&1)==active){auto len=u64(e,24);if(len<=quint64(im.v.length))bitmap=chain(u32(e,20),qint64(len),false,false);}}
 if(!bitmap.valid||bitmap.size<(qint64(clusters)+7)/8){out.issue("exFAT","Missing allocation bitmap");return true;}
 auto freeStream=[&](const Stream &s){for(const auto &run:s.runs){auto first=quint64((run.offset-qint64(heap)*sector)/cluster);auto n=quint64((run.length+cluster-1)/cluster);for(quint64 j=0;j<n;++j){if(!out.running())return false;auto bit=first+j;auto value=im.read(bitmap,qint64(bit/8),1);if(value.size()!=1||(uchar(value[0])&(1<<(bit%8))))return false;}}return true;};
 struct Dir {Stream stream;QString path;};QList<Dir> queue{{rootStream,"exFAT"}};QSet<qint64> dirs;quint64 entries=0;
 while(!queue.isEmpty()&&out.running()){
  auto dir=queue.takeFirst();if(dir.stream.runs.isEmpty()||dirs.contains(dir.stream.runs[0].offset))continue;dirs.insert(dir.stream.runs[0].offset);
  for(qint64 pos=0;pos+32<=dir.stream.size&&out.running();pos+=32){
   if(++entries>quint64(out.options.maxRecords)){out.status="partial";out.issue("exFAT","Configured directory entry limit reached");break;}
   auto e=im.read(dir.stream,pos,32);if(e.size()!=32){out.issue(dir.path,"Directory read failed");break;}int type=uchar(e[0]);if(!type)break;if((type&0x7f)!=5)continue;
   int secondary=uchar(e[1]);if(secondary<2||secondary>18||qint64(secondary+1)*32>dir.stream.size-pos){out.issue(dir.path,"Invalid directory entry set length");continue;}
   auto set=im.read(dir.stream,pos,qint64(secondary+1)*32);if(set.size()!=(secondary+1)*32)break;bool deleted=!(type&0x80),shape=true;
   for(int j=1;j<=secondary;++j){int t=uchar(set[j*32]);if(bool(t&0x80)==deleted||(t&0x7f)!=(j==1?0x40:0x41))shape=false;}
   if(!shape){out.issue(dir.path,"Invalid secondary entry types");continue;}
   // Deletion clears InUse; restore those bits only for checksum validation.
   auto normalized=set;for(int j=0;j<=secondary;++j)normalized[j*32]=char(uchar(normalized[j*32])|0x80);
   quint16 cs=0;for(int j=0;j<normalized.size();++j)if(j!=2&&j!=3)cs=quint16(((cs&1)?0x8000:0)+(cs>>1)+uchar(normalized[j]));
   if(cs!=u16(set,2)){out.issue(dir.path,"Directory set checksum mismatch");pos+=secondary*32;continue;}
   int names=uchar(set[35]);if(names<1||names>(secondary-1)*15){out.issue(dir.path,"Invalid name length");continue;}QString name;for(int j=2;j<=secondary;++j)name+=utf16(set,j*32+2,qMin(15,names-int(name.size())));
   auto size=u64(set,56),initialized=u64(set,40);bool directory=u16(set,4)&16;
   if(size>quint64(im.v.length)||initialized>size){out.issue(name,"Invalid stream size");continue;}
   if(directory&&initialized!=size){out.issue(name,"Uninitialized directory stream");continue;}
   auto stream=chain(u32(set,52),qint64(size),uchar(set[33])&2,directory);stream.initialized=qint64(initialized);
   if(!stream.valid){out.issue(name,"Missing, looping or invalid allocation chain");pos+=secondary*32;continue;}
   if(deleted&&!freeStream(stream)){out.issue(name,"Data clusters reallocated or bitmap unreadable");pos+=secondary*32;continue;}
   if(directory){if(queue.size()+dirs.size()>=65536){out.status="partial";out.issue(name,"Directory traversal limit");break;}queue.append({stream,dir.path+"/"+safe(name)});}
   else if(deleted)out.save(name,dir.path,stream,"Deleted exFAT set; normalized set checksum and allocation bitmap checked; NoFatChain extent or retained FAT chain.");
   pos+=secondary*32;
  }
 }
 return true;
}
bool fat32(Image &im,Output &out){
 auto b=im.read(0,512);if(b.size()!=512||uchar(b[510])!=85||uchar(b[511])!=170)return false;
 int sector=u16(b,11),spc=uchar(b[13]),reserved=u16(b,14),nf=uchar(b[16]);auto total=u32(b,32),fatSize=u32(b,36),root=u32(b,44)&0x0fffffff;
 if(!QList<int>{512,1024,2048,4096}.contains(sector)||!spc||spc>128||(spc&(spc-1))||!reserved||nf<1||nf>2||u16(b,17)||u16(b,22)||!fatSize)return false;
 auto overhead=quint64(reserved)+quint64(nf)*fatSize;if(overhead>=total||quint64(total)*sector>quint64(im.v.length))return false;
 auto clusters=quint32((total-overhead)/spc);if(clusters<65525||clusters>0x0fffffed||quint64(clusters+2)*4>quint64(fatSize)*sector)return false;
 qint64 cluster=qint64(spc)*sector,data=qint64(overhead)*sector;int active=(u16(b,40)&0x80)?u16(b,40)&15:0;if(active>=nf)return false;
 auto valid=[&](quint32 c){return c>=2&&c<clusters+2;};auto next=[&](quint32 c){auto x=im.read((qint64(reserved)+qint64(active)*fatSize)*sector+qint64(c)*4,4);return x.size()==4?u32(x,0)&0x0fffffff:quint32(0x0ffffff7);};
 auto off=[&](quint32 c){return data+qint64(c-2)*cluster;};
 auto chain=[&](quint32 c,qint64 size,bool directory){Stream s;QSet<quint32> seen;qint64 n=0;if(!size&&!directory){s.valid=true;s.resident=QByteArray("");return s;}
  while(valid(c)&&out.running()){if(seen.contains(c)||seen.size()>=out.options.maxRecords)return Stream{};seen.insert(c);s.runs<<Run{off(c),cluster};n+=cluster;auto link=next(c);
   if(!directory&&n>=size){if(link<0x0ffffff8)return Stream{};s.size=s.initialized=size;s.valid=true;return s;}
   if(link>=0x0ffffff8){if(directory){s.size=s.initialized=n;s.valid=true;}return s;}c=link;}return Stream{};};
 struct Entry {QString name,path;quint32 first;quint32 size;QSet<quint32> parentClusters;};struct Dir {quint32 first;QString path;bool deleted=false;QSet<quint32> parentClusters;};
 QList<Dir> queue{{root,"FAT32"}};QSet<quint32> dirs,owned;QList<Entry> deleted;quint64 visited=0;
 while(!queue.isEmpty()&&out.running()){
  auto dir=queue.takeFirst();if(dirs.contains(dir.first))continue;dirs.insert(dir.first);auto stream=chain(dir.first,0,true);if(!stream.valid){out.issue(dir.path,"Invalid directory allocation");continue;}
  if(dir.deleted){
   auto dot=im.read(stream,0,64);if(dot.size()!=64||dot.left(11)!=QByteArray(".          ")||dot.mid(32,11)!=QByteArray("..         ")||!(uchar(dot[11])&16)||!(uchar(dot[43])&16)||(((quint32(u16(dot,20))<<16)|u16(dot,26))&0x0fffffff)!=dir.first){out.issue(dir.path,"Deleted directory lacks valid dot entries and retained chain");continue;}
   for(const auto &r:stream.runs)dir.parentClusters.insert(quint32((r.offset-data)/cluster)+2);
  }else for(const auto &r:stream.runs)owned.insert(quint32((r.offset-data)/cluster)+2);
  QList<QByteArray> lfn;
  for(qint64 p=0;p+32<=stream.size&&out.running();p+=32){if(++visited>quint64(out.options.maxRecords)){out.status="partial";out.issue(dir.path,"Directory entry limit");break;}
   auto e=im.read(stream,p,32);if(e.size()!=32)break;int marker=uchar(e[0]),attr=uchar(e[11]);if(!marker)break;
   if(attr==15){if(lfn.size()<20)lfn.append(e);else lfn.clear();continue;}QString name;bool isDeleted=marker==0xe5;
   if(!lfn.isEmpty()){
    int checksum=uchar(lfn[0][13]);bool good=true;int expected=lfn.size();
    for(int i=0;i<lfn.size();++i){const auto &l=lfn[i];int ord=uchar(l[0]);if(uchar(l[13])!=checksum||l[12]!=0||u16(l,26)!=0||(isDeleted?ord!=0xe5:((ord&31)!=expected-i||bool(ord&64)!=(i==0))))good=false;}
    auto checkName=[&](int first){int sum=0;for(int i=0;i<11;++i)sum=(((sum&1)?128:0)+(sum>>1)+(i?uchar(e[i]):first))&255;return sum;};
    if(isDeleted){bool matches=false;for(int first=1;first<256;++first)if(first!=0xe5&&checkName(first)==checksum)matches=true;good&=matches;}else good&=checkName(marker)==checksum;
    if(good){for(int i=lfn.size()-1;i>=0;--i){const auto &l=lfn[i];for(int start:QList<int>{1,14,28})for(int j=0;j<(start==1?5:start==14?6:2);++j)name+=QChar(u16(l,start+2*j));}int end=name.indexOf(QChar(0));if(end>=0)name.truncate(end);if(name.contains(QChar(0xffff)))name.clear();}
   }lfn.clear();
   if(attr&8||marker=='.')continue;
   if(name.isEmpty()){name=QString::fromLatin1(e.left(8)).trimmed();if(isDeleted)name[0]='_';auto ext=QString::fromLatin1(e.mid(8,3)).trimmed();if(!ext.isEmpty())name+='.'+ext;}
   auto first=((quint32(u16(e,20))<<16)|u16(e,26))&0x0fffffff;auto size=u32(e,28);
   if(attr&16){bool gone=isDeleted||dir.deleted;if(valid(first)&&(!gone||next(first)>=2)){if(gone)queue.append({first,dir.path+"/"+safe(name),gone,dir.parentClusters});else queue.prepend({first,dir.path+"/"+safe(name),false,{}});}else if(gone)out.issue(name,"Deleted directory FAT chain was cleared or invalid");continue;}
   if(isDeleted||dir.deleted){deleted.append({name,dir.path,first,size,dir.parentClusters});continue;}
   auto allocated=chain(first,size,false);if(!allocated.valid&&size){out.issue(name,"Live allocation corrupt; refusing deleted recovery for this volume");return true;}
   for(const auto &r:allocated.runs)owned.insert(quint32((r.offset-data)/cluster)+2);
  }
 }
 for(const auto &e:deleted){if(!out.running())break;bool parentConflict=false;for(auto c:e.parentClusters)if(owned.contains(c))parentConflict=true;if(parentConflict){out.issue(e.name,"Deleted parent directory overlaps live allocation");continue;}Stream s;bool candidate=true;
  if(!e.size){s.valid=true;s.resident=QByteArray("");}
  else if(!valid(e.first)){out.issue(e.name,"Invalid first cluster");continue;}
  else if(next(e.first)==0){auto n=(quint64(e.size)+cluster-1)/cluster;if(n>quint64(clusters)+2-e.first){out.issue(e.name,"Extent outside volume");continue;}bool free=true;
   for(quint64 j=0;j<n&&out.running();++j)if(next(e.first+quint32(j))!=0||(owned.contains(e.first+quint32(j))||e.parentClusters.contains(e.first+quint32(j)))){free=false;break;}if(!free){out.issue(e.name,"Candidate overlaps allocated clusters");continue;}
   s.runs<<Run{off(e.first),qint64(n)*cluster};s.size=s.initialized=e.size;s.valid=true;
  }else {s=chain(e.first,e.size,false);if(!s.valid){out.issue(e.name,"Incomplete retained FAT chain");continue;}bool conflict=false;for(const auto &r:s.runs)if(owned.contains(quint32((r.offset-data)/cluster)+2)||e.parentClusters.contains(quint32((r.offset-data)/cluster)+2))conflict=true;if(conflict){out.issue(e.name,"Retained chain overlaps live files");continue;}}
  out.save(e.name,e.path,s,"Deleted FAT32 entry. Retained unreferenced FAT chain or contiguous free-cluster hypothesis; LFN association is tentative after deletion. Review content.",candidate);
 }
 return true;
}
}
QJsonObject recoverFilesystem(const QString &source,const QString &directory,const QString &filesystem,const Context &ctx,const RecoveryOptions &options){
 auto error=[](const QString &s){return QJsonObject{{"status","error"},{"operation","filesystem_recovery"},{"message",s}};};
 if(!isRegularSource(source)||!QFileInfo(directory).isDir()||QFileInfo(directory).isSymLink()||QFileInfo(directory).canonicalFilePath().startsWith("//"))return error("Choose a regular image and existing local directory.");
 if(options.maxFiles<1||options.maxFiles>1000000||options.maxRecords<1||options.maxRecords>10000000)return error("Invalid recovery limits.");
 if(filesystem!="NTFS"&&filesystem!="exFAT"&&filesystem!="FAT32")return error("Unknown filesystem mode.");
 QFile f(source);if(!f.open(QIODevice::ReadOnly))return error("Cannot open image read-only.");auto size=f.size();auto modified=QFileInfo(source).lastModified();
 QTemporaryDir directoryOut(QDir(directory).filePath("NEXVARY-Files-XXXXXX"));if(!directoryOut.isValid())return error("Cannot create output directory.");directoryOut.setAutoRemove(false);
 QStringList warnings;auto volumes=imageVolumes(f,warnings);QJsonArray files,issues;QString status="completed";int skipped=0,found=0;
 for(const auto &volume:volumes){Image im{f,volume};auto perVolume=options;perVolume.maxFiles=qMax(1,options.maxFiles-int(files.size()));Output output{im,directoryOut.path(),ctx,perVolume};bool matched=filesystem=="NTFS"?ntfs(im,output):filesystem=="exFAT"?exfat(im,output):fat32(im,output);if(matched)++found;
  for(auto value:output.files)files.append(value);for(auto value:output.issues)if(issues.size()<2000)issues.append(value);skipped+=output.skipped;
  if(output.status!="completed"){status=output.status;break;}if(files.size()>=options.maxFiles){status="partial";warnings<<"Configured file limit reached";break;}
 }
 if(!found)status="error";if(ctx.cancelled())status="cancelled";if(f.size()!=size||QFileInfo(source).lastModified()!=modified){status="error";warnings<<"Source changed while recovering";}
 if(status=="completed"&&skipped)status="partial";
 QJsonObject result{{"status",status},{"operation","filesystem_recovery"},{"filesystem",filesystem},{"destination",directoryOut.path()},{"source",QFileInfo(source).absoluteFilePath()},{"recoveredCount",files.size()},{"files",files},{"skippedEntries",skipped},{"issues",issues},{"warnings",QJsonArray::fromStringList(warnings)},{"matchedVolumes",found},{"scope","Read-only deleted-file recovery. complete means metadata byte coverage, not original-content authenticity. Overwritten/TRIM data cannot be recreated. Compressed/encrypted NTFS and external MFT attribute-list extensions are reported, not guessed."}};
 int completeCount=0,partialCount=0,candidateCount=0;for(const auto &value:files){auto condition=value.toObject().value("condition").toString();if(condition=="complete")++completeCount;else if(condition=="partial")++partialCount;else ++candidateCount;}result.insert("completeCount",completeCount);result.insert("partialCount",partialCount);result.insert("candidateCount",candidateCount);
 QSaveFile manifest(directoryOut.filePath("manifest.json"));auto json=QJsonDocument(result).toJson();if(!manifest.open(QIODevice::WriteOnly)||manifest.write(json)!=json.size()||!manifest.commit())result.insert("manifestWarning","Cannot save manifest; export report.");return result;
}
}
