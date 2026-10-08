#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QtEndian>
#include "FilesystemRecovery.h"
#include "Partitions.h"
#include "RescueEngine.h"
namespace {
void p16(QByteArray &b,int p,quint16 v){qToLittleEndian(v,reinterpret_cast<uchar*>(b.data()+p));}
void p32(QByteArray &b,int p,quint32 v){qToLittleEndian(v,reinterpret_cast<uchar*>(b.data()+p));}
void p64(QByteArray &b,int p,quint64 v){qToLittleEndian(v,reinterpret_cast<uchar*>(b.data()+p));}
void name16(QByteArray &b,int p,const QString &s){for(int i=0;i<s.size();++i)p16(b,p+i*2,s[i].unicode());}
QByteArray resident(quint32 type,QByteArray b){int len=(24+b.size()+7)&~7;QByteArray a(len,0);p32(a,0,type);p32(a,4,len);p32(a,16,b.size());p16(a,20,24);a.replace(24,b.size(),b);return a;}
QByteArray nonresident(QByteArray runs,qint64 size,qint64 clusters){int len=(64+runs.size()+7)&~7;QByteArray a(len,0);p32(a,0,0x80);p32(a,4,len);a[8]=1;p64(a,24,clusters-1);p16(a,32,64);p64(a,40,clusters*512);p64(a,48,size);p64(a,56,size);a.replace(64,runs.size(),runs);return a;}
QByteArray record(int id,bool live,bool dir,const QString &name,int parent,QByteArray data){
 QByteArray r(1024,0);r.replace(0,4,"FILE");p16(r,4,48);p16(r,6,3);p16(r,16,1);p16(r,20,56);p16(r,22,(live?1:0)|(dir?2:0));p32(r,28,1024);p32(r,44,id);
 QByteArray fn(66+name.size()*2,0);p64(fn,0,quint64(parent)|(quint64(1)<<48));fn[64]=char(name.size());fn[65]=1;name16(fn,66,name);auto attrs=resident(0x30,fn)+data;int end=56+attrs.size();r.replace(56,attrs.size(),attrs);p32(r,end,0xffffffff);p32(r,24,end+8);
 p16(r,48,0xbeef);r.replace(50,2,r.mid(510,2));r.replace(52,2,r.mid(1022,2));p16(r,510,0xbeef);p16(r,1022,0xbeef);return r;
}
QByteArray ntImage(){
 QByteArray b(512*256,0);b.replace(3,8,"NTFS    ");p16(b,11,512);b[13]=1;p64(b,40,256);p64(b,48,4);b[64]=char(-10);b[510]=85;b[511]=char(170);
 auto put=[&](int i,const QByteArray &r){b.replace(2048+i*1024,1024,r);};
 for(int i=0;i<20;++i)put(i,record(i,true,false,"system",5,resident(0x80,"")));
 put(0,record(0,true,false,"$MFT",5,nonresident(QByteArray::fromHex("11280400"),20*1024,40)));
 put(5,record(5,true,true,"root",5,{}));put(6,record(6,true,false,"$Bitmap",5,resident(0x80,QByteArray(32,0))));
 put(16,record(16,true,true,QString::fromUtf8("مجلد"),5,{}));
 put(17,record(17,false,false,"resident.txt",16,resident(0x80,"resident recovered")));
 put(18,record(18,false,false,"fragmented.bin",16,nonresident(QByteArray::fromHex("11015011020a00"),1200,3)));
 b.replace(80*512,512,QByteArray(512,'A'));b.replace(90*512,688,QByteArray(688,'B'));return b;
}
QByteArray exImage(){
 QByteArray b(128*512,0);b.replace(3,8,"EXFAT   ");p64(b,72,128);p32(b,80,24);p32(b,84,1);p32(b,88,25);p32(b,92,100);p32(b,96,2);p16(b,104,0x100);b[108]=9;b[109]=0;b[110]=1;b[510]=85;b[511]=char(170);
 for(int c:QList<int>{2,3,4,5,7,9})p32(b,24*512+c*4,0xffffffff);p32(b,24*512+7*4,9);
 // bitmap at cluster 3, root at 2; only metadata clusters allocated.
 int root=25*512;b[root]=char(0x81);p32(b,root+20,3);p64(b,root+24,13);b[26*512]=3;
 auto entry=[&](int pos,const QString &name,int first,int size,bool contiguous){QByteArray e(96,0);e[0]=char(0x85);e[1]=2;p16(e,4,32);e[32]=char(0xc0);e[33]=contiguous?3:1;e[35]=char(name.size());p64(e,40,size);p32(e,52,first);p64(e,56,size);e[64]=char(0xc1);name16(e,66,name);quint16 sum=0;for(int i=0;i<e.size();++i)if(i!=2&&i!=3)sum=quint16(((sum&1)?0x8000:0)+(sum>>1)+uchar(e[i]));p16(e,2,sum);e[0]=5;e[32]=0x40;e[64]=0x41;b.replace(pos,96,e);};
 entry(root+32,"contiguous.txt",4,600,true);entry(root+128,"fragment.bin",7,700,false);
 b.replace(27*512,600,QByteArray(600,'C'));b.replace(30*512,512,QByteArray(512,'D'));b.replace(32*512,188,QByteArray(188,'E'));
 quint32 sum=0;for(int i=0;i<11*512;++i)if(i!=106&&i!=107&&i!=112)sum=((sum&1)?0x80000000:0)+(sum>>1)+uchar(b[i]);for(int i=11*512;i<12*512;i+=4)p32(b,i,sum);return b;
}
}
class AdvancedTests:public QObject {
 Q_OBJECT
 void write(const QString &p,const QByteArray &b){QFile f(p);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(b),qint64(b.size()));}
 QByteArray read(const QString &p){QFile f(p);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
 void compareFile(const QJsonObject &r,int i,const QByteArray &expected){auto entry=r.value("files").toArray()[i].toObject();auto b=read(QDir(r.value("destination").toString()).filePath(entry.value("file").toString()));QCOMPARE(b,expected);QCOMPARE(entry.value("sha256").toString(),QString::fromLatin1(QCryptographicHash::hash(expected,QCryptographicHash::Sha256).toHex()));QVERIFY(entry.value("readbackVerified").toBool());}
private slots:
 void ntfsResidentAndFragmented(){QTemporaryDir d;write(d.filePath("n.img"),ntImage());auto r=dc::recoverFilesystem(d.filePath("n.img"),d.path(),"NTFS");QCOMPARE(r.value("status").toString(),"completed");QCOMPARE(r.value("recoveredCount").toInt(),2);compareFile(r,0,"resident recovered");compareFile(r,1,QByteArray(512,'A')+QByteArray(688,'B'));QVERIFY(r.value("files").toArray()[0].toObject().value("file").toString().contains(QString::fromUtf8("مجلد")));QCOMPARE(read(d.filePath("n.img")),ntImage());}
 void ntfsCorruptFixupAndBounds(){QTemporaryDir d;auto b=ntImage();b[2048+18*1024+510]=0;write(d.filePath("n.img"),b);auto r=dc::recoverFilesystem(d.filePath("n.img"),d.path(),"NTFS");QCOMPARE(r.value("recoveredCount").toInt(),1);QCOMPARE(r.value("status").toString(),"partial");b=ntImage();p64(b,48,999999);write(d.filePath("n.img"),b);r=dc::recoverFilesystem(d.filePath("n.img"),d.path(),"NTFS");QCOMPARE(r.value("recoveredCount").toInt(),0);}
 void ntfsRejectReallocated(){QTemporaryDir d;auto b=ntImage();QByteArray bitmap(32,0);bitmap[10]=1;b.replace(2048+6*1024,1024,record(6,true,false,"$Bitmap",5,resident(0x80,bitmap)));write(d.filePath("n.img"),b);auto r=dc::recoverFilesystem(d.filePath("n.img"),d.path(),"NTFS");QCOMPARE(r.value("recoveredCount").toInt(),1);QCOMPARE(r.value("skippedEntries").toInt(),1);}
 void exfatContiguousAndFragmented(){QTemporaryDir d;write(d.filePath("e.img"),exImage());auto r=dc::recoverFilesystem(d.filePath("e.img"),d.path(),"exFAT");QCOMPARE(r.value("status").toString(),"completed");QCOMPARE(r.value("recoveredCount").toInt(),2);compareFile(r,0,QByteArray(600,'C'));compareFile(r,1,QByteArray(512,'D')+QByteArray(188,'E'));QCOMPARE(read(d.filePath("e.img")),exImage());}
 void exfatRejectChecksumAndReallocation(){QTemporaryDir d;auto b=exImage();b[25*512+32+2]^=1;write(d.filePath("e.img"),b);auto r=dc::recoverFilesystem(d.filePath("e.img"),d.path(),"exFAT");QCOMPARE(r.value("recoveredCount").toInt(),1);b=exImage();b[26*512]=7;write(d.filePath("e.img"),b);r=dc::recoverFilesystem(d.filePath("e.img"),d.path(),"exFAT");QCOMPARE(r.value("recoveredCount").toInt(),1);}
 void ebrCycleAndBounds(){QTemporaryDir d;QByteArray b(512*100,0);b[510]=85;b[511]=char(170);b[450]=15;p32(b,454,10);p32(b,458,90);b[10*512+510]=85;b[10*512+511]=char(170);b[10*512+450]=7;p32(b,10*512+454,1);p32(b,10*512+458,20);b[10*512+466]=15;p32(b,10*512+470,0);write(d.filePath("p.img"),b);QFile f(d.filePath("p.img"));QVERIFY(f.open(QIODevice::ReadOnly));QStringList warnings;auto v=dc::imageVolumes(f,warnings);QCOMPARE(v.size(),2);QCOMPARE(v[1].offset,qint64(11*512));QVERIFY(!warnings.isEmpty());}
 void rescueCancelResumeAndTail(){QTemporaryDir d;QByteArray original(3*1048576,'R');dc::RescueSource s{original.size(),"fixture-1",[&](qint64 p,qint64 n){return original.mid(p,n);}};std::atomic_bool cancel=false;auto dest=d.filePath("rescue.img");auto r=dc::rescueStream(s,dest,{}, {&cancel,[&](qint64,qint64){cancel=true;}});QCOMPARE(r.value("status").toString(),"cancelled");QCOMPARE(r.value("processedBytes").toDouble(),1048576.0);
  QFile tail(dest);QVERIFY(tail.open(QIODevice::Append));tail.write("uncommitted");tail.close();QFile map(dest+".readmap.jsonl");QVERIFY(map.open(QIODevice::Append));map.write("{partial");map.close();cancel=false;r=dc::rescueStream(s,dest,{true,512,1});QCOMPARE(r.value("status").toString(),"completed");QVERIFY(r.value("imageReadbackVerified").toBool());QCOMPARE(read(dest),original);
 }
 void rescueRejectIdentityAndCorruption(){QTemporaryDir d;QByteArray original(1048576,'R');dc::RescueSource s{original.size(),"fixture-1",[&](qint64 p,qint64 n){return original.mid(p,n);}};auto dest=d.filePath("r.img");QCOMPARE(dc::rescueStream(s,dest,{}).value("status").toString(),"completed");s.identity="other-disk";QCOMPARE(dc::rescueStream(s,dest,{true}).value("status").toString(),"error");s.identity="fixture-1";QFile f(dest);QVERIFY(f.open(QIODevice::ReadWrite));f.write("X");f.close();QCOMPARE(dc::rescueStream(s,dest,{true}).value("status").toString(),"error");}
 void rescueSectorFallbackAndRetries(){QTemporaryDir d;int attempts=0;QByteArray original(1048576,'S');dc::RescueSource s{original.size(),"fault-fixture",[&](qint64 p,qint64 n){if(n>512)return QByteArray{};if(p==512){++attempts;return QByteArray{};}return original.mid(p,n);}};auto r=dc::rescueStream(s,d.filePath("r.img"),{false,512,2});QCOMPARE(r.value("status").toString(),"mismatch");QCOMPARE(r.value("unreadableBytes").toInt(),512);QCOMPARE(attempts,3);original.replace(512,512,QByteArray(512,0));QCOMPARE(read(d.filePath("r.img")),original);QVERIFY(r.value("imageReadbackVerified").toBool());}
 void rescueInsufficientSpaceAndInvalidOptions(){QTemporaryDir d;dc::RescueSource s{qint64(1)<<60,"huge",[](qint64,qint64){return QByteArray{};}};QCOMPARE(dc::rescueStream(s,d.filePath("r.img"),{}).value("status").toString(),"error");QCOMPARE(dc::rescueStream(s,d.filePath("z.img"),{false,0,1}).value("status").toString(),"error");}
};
QTEST_GUILESS_MAIN(AdvancedTests)
#include "test_advanced.moc"
