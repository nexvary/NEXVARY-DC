#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QtEndian>
#include "Recovery.h"
class RecoveryTests:public QObject {
 Q_OBJECT
 QByteArray png() const {return QByteArray::fromHex("89504e470d0a1a0a0000000d4948445200000001000000010802000000907753de0000000c49444154789c63f8cfc0000003010100c9fe92ef0000000049454e44ae426082");}
 void write(const QString &path,const QByteArray &b){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(b),qint64(b.size()));}
private slots:
 void recoverAcrossChunkBoundary(){
  QTemporaryDir d;const auto source=d.filePath("rescue.img");const auto bytes=QByteArray(1048572,'x')+png()+QByteArray(22,'x')+QByteArray::fromHex("ffd8ffe00004abcdffd9");write(source,bytes);
  auto r=dc::recoverImage(source,d.path());QCOMPARE(r.value("status").toString(),"completed");QCOMPARE(r.value("recoveredCount").toInt(),2);
  const auto files=r.value("files").toArray();QCOMPARE(files[0].toObject().value("offset").toInt(),1048572);
  QFile recovered(QDir(r.value("destination").toString()).filePath(files[0].toObject().value("file").toString()));QVERIFY(recovered.open(QIODevice::ReadOnly));QCOMPARE(recovered.readAll(),png());
  QFile original(source);QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),bytes);QVERIFY(QFile::exists(QDir(r.value("destination").toString()).filePath("manifest.json")));
  auto second=dc::recoverImage(source,d.path());QVERIFY(second.value("destination")!=r.value("destination"));
 }
 void rejectCorruptAndTruncated(){QTemporaryDir d;auto corrupt=png();corrupt[29]=char(corrupt[29]^1);write(d.filePath("bad.img"),corrupt+png().left(35)+QByteArray::fromHex("ffd8ffe00004abcd"));auto r=dc::recoverImage(d.filePath("bad.img"),d.path());QCOMPARE(r.value("recoveredCount").toInt(),0);}
 void preserveResultsOnCancel(){QTemporaryDir d;write(d.filePath("src.img"),png()+QByteArray(2*1048576,'x'));std::atomic_bool stop=false;auto r=dc::recoverImage(d.filePath("src.img"),d.path(),{&stop,[&](qint64,qint64){stop=true;}});QCOMPARE(r.value("status").toString(),"cancelled");QCOMPARE(r.value("recoveredCount").toInt(),1);QVERIFY(QFile::exists(QDir(r.value("destination").toString()).filePath("manifest.json")));}
 void fat32DeletedFileAndAllocatedRejection(){
  QTemporaryDir d;const auto source=d.filePath("fat32.img");QFile f(source);QVERIFY(f.open(QIODevice::ReadWrite));
  constexpr int sectors=32+2*513+65525;QVERIFY(f.resize(qint64(sectors)*512));
  QByteArray boot(512,0);qToLittleEndian<quint16>(512,reinterpret_cast<uchar*>(boot.data()+11));boot[13]=1;qToLittleEndian<quint16>(32,reinterpret_cast<uchar*>(boot.data()+14));boot[16]=2;qToLittleEndian<quint32>(sectors,reinterpret_cast<uchar*>(boot.data()+32));qToLittleEndian<quint32>(513,reinterpret_cast<uchar*>(boot.data()+36));qToLittleEndian<quint32>(2,reinterpret_cast<uchar*>(boot.data()+44));boot[510]=char(0x55);boot[511]=char(0xaa);QCOMPARE(f.write(boot),qint64(512));
  QByteArray table(513*512,0);qToLittleEndian<quint32>(0x0fffffff,reinterpret_cast<uchar*>(table.data()+8));QVERIFY(f.seek(32*512));QCOMPARE(f.write(table),qint64(table.size()));
  const qint64 data=(32+2*513)*512;QByteArray entry(512,0);entry[0]=char(0xe5);entry.replace(1,10,"ILE    TXT");entry[11]=char(0x20);qToLittleEndian<quint16>(3,reinterpret_cast<uchar*>(entry.data()+26));qToLittleEndian<quint32>(5,reinterpret_cast<uchar*>(entry.data()+28));QVERIFY(f.seek(data));QCOMPARE(f.write(entry),qint64(512));QCOMPARE(f.write("HELLO",5),qint64(5));f.flush();
  auto r=dc::recoverFat32(source,d.path());QCOMPARE(r.value("status").toString(),"completed");QCOMPARE(r.value("recoveredCount").toInt(),1);
  auto file=r.value("files").toArray()[0].toObject();QFile recovered(QDir(r.value("destination").toString()).filePath(file.value("file").toString()));QVERIFY(recovered.open(QIODevice::ReadOnly));QCOMPARE(recovered.readAll(),QByteArray("HELLO"));
  QByteArray used(4,0);qToLittleEndian<quint32>(0x0fffffff,reinterpret_cast<uchar*>(used.data()));QVERIFY(f.seek(32*512+12));QCOMPARE(f.write(used),qint64(4));f.flush();r=dc::recoverFat32(source,d.path());QCOMPARE(r.value("recoveredCount").toInt(),0);QCOMPARE(r.value("skippedEntries").toInt(),1);
 }
 void fat32UnsupportedImage(){QTemporaryDir d;write(d.filePath("not-fat.img"),png());QCOMPARE(dc::recoverFat32(d.filePath("not-fat.img"),d.path()).value("status").toString(),"error");}
 void rejectDevicesAndMissingDestination(){QTemporaryDir d;QCOMPARE(dc::recoverImage("/dev/null",d.path()).value("status").toString(),"error");write(d.filePath("src.img"),png());QCOMPARE(dc::recoverImage(d.filePath("src.img"),d.filePath("missing")).value("status").toString(),"error");}
};
QTEST_GUILESS_MAIN(RecoveryTests)
#include "test_recovery.moc"
