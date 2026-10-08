#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
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
 void rejectDevicesAndMissingDestination(){QTemporaryDir d;QCOMPARE(dc::recoverImage("/dev/null",d.path()).value("status").toString(),"error");write(d.filePath("src.img"),png());QCOMPARE(dc::recoverImage(d.filePath("src.img"),d.filePath("missing")).value("status").toString(),"error");}
};
QTEST_GUILESS_MAIN(RecoveryTests)
#include "test_recovery.moc"
