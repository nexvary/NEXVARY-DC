#include <QtTest>
#include <QJsonArray>
#include "StoragePolicy.h"
#include "MediaRead.h"
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>
class PolicyTests:public QObject {
 Q_OBJECT
 QJsonObject disk() const {return {{"number",2},{"uniqueId","USB-TEST-42"},{"serial","42"},{"bytes",8.0*1073741824},{"transport","USB"},{"external",true},{"partitions",QJsonArray{QJsonObject{{"partition",1},{"offset",1048576},{"partitionBytes",4.0*1073741824}}}}};}
 QJsonObject request(QJsonObject d) const {d.insert("action","format");d.insert("partition",1);d.insert("offset",1048576);d.insert("partitionBytes",4.0*1073741824);d.insert("filesystem","exFAT");return d;}
private slots:
 void validExternal(){auto d=disk();QVERIFY(dc::validateStorageRequest(d,request(d)).isEmpty());}
 void protectedDisks(){for(const auto &key:QStringList{"boot","system","readOnly","offline"}){auto d=disk();d.insert(key,true);QVERIFY(!dc::validateStorageRequest(d,request(d)).isEmpty());}auto d=disk();d.insert("transport","SATA");QVERIFY(!dc::validateStorageRequest(d,request(d)).isEmpty());}
 void replacementRejected(){auto d=disk();auto r=request(d);for(const auto &key:QStringList{"number","uniqueId","serial","bytes","offset","partitionBytes"}){auto changed=r;changed.insert(key,"replacement");QVERIFY(!dc::validateStorageRequest(d,changed).isEmpty());}}
 void partitionAndFilesystem(){auto d=disk();auto r=request(d);r.insert("partition",99);QVERIFY(!dc::validateStorageRequest(d,r).isEmpty());r=request(d);r.insert("filesystem","unsafe");QVERIFY(!dc::validateStorageRequest(d,r).isEmpty());r.insert("action","layout");r.insert("filesystem","FAT32");d.insert("bytes",64.0*1073741824);r.insert("bytes",d.value("bytes"));QVERIFY(!dc::validateStorageRequest(d,r).isEmpty());}
 void rescueRecordsUnreadableRanges(){
  QTemporaryDir dir;QFile input(dir.filePath("source.img"));QVERIFY(input.open(QIODevice::WriteOnly));QByteArray good(1048576,'X');QCOMPARE(input.write(good),qint64(good.size()));input.close();
  auto out=dir.filePath("rescue.img");auto result=dc::rescueMedia(input.fileName(),2097152,out);
  QCOMPARE(result.value("status").toString(),"mismatch");QCOMPARE(result.value("unreadableBytes").toDouble(),1048576.0);QVERIFY(result.value("imageReadbackVerified").toBool());QCOMPARE(result.value("unreadableRanges").toArray().size(),1);
  QFile rescued(out);QVERIFY(rescued.open(QIODevice::ReadOnly));QCOMPARE(rescued.read(1048576),good);QCOMPARE(rescued.readAll(),QByteArray(1048576,0));QVERIFY(QFile::exists(out+".readmap.json"));
  result=dc::rescueMedia(input.fileName(),1048576,out);QCOMPARE(result.value("status").toString(),"error");QCOMPARE(rescued.size(),qint64(2097152));
 }
 void readOnlyScanAndCancellation(){
  QTemporaryDir dir;QFile input(dir.filePath("source.img"));QVERIFY(input.open(QIODevice::WriteOnly));input.write(QByteArray(1048576,'A'));input.close();
  auto result=dc::scanMedia(input.fileName(),1048576);QCOMPARE(result.value("status").toString(),"completed");QCOMPARE(result.value("unreadableBytes").toInt(),0);QVERIFY(input.open(QIODevice::ReadOnly));QCOMPARE(input.readAll(),QByteArray(1048576,'A'));input.close();
  std::atomic_bool cancel=true;result=dc::rescueMedia(input.fileName(),1048576,dir.filePath("partial.img"),{&cancel,{}});QCOMPARE(result.value("status").toString(),"cancelled");QVERIFY(result.value("partialImageRetained").toBool());QVERIFY(QFile::exists(dir.filePath("partial.img.readmap.json")));
  result=dc::scanMedia(input.fileName(),513);QCOMPARE(result.value("status").toString(),"error");
 }
 void explicitHealth(){auto s=dc::summarizeSmart(QJsonObject{});QCOMPARE(s.value("health").toString(),"unknown");s=dc::summarizeSmart({{"smart_status",QJsonObject{{"passed",false}}},{"temperature",QJsonObject{{"current",42}}},{"nvme_smart_health_information_log",QJsonObject{{"media_errors",7}}}});QCOMPARE(s.value("health").toString(),"failed");QCOMPARE(s.value("temperature").toInt(),42);QCOMPARE(s.value("attributes").toArray().size(),1);}
};
QTEST_GUILESS_MAIN(PolicyTests)
#include "test_policy.moc"
