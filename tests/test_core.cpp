#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>
#include <QHash>
#include "Operations.h"
class CoreTests:public QObject {
 Q_OBJECT
private slots:
 void imageHashAndCopy(){
  QTemporaryDir d;QVERIFY(d.isValid());auto src=d.filePath("source.img");auto dst=d.filePath("copy.img");
  QByteArray bytes=dc::testPattern(19,42,2*1024*1024+17);QFile f(src);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size());f.close();
  auto scan=dc::scanImage(src);QCOMPARE(scan.value("status").toString(),"completed");QCOMPARE(scan.value("sha256").toString(),QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()));
  auto copy=dc::copyImage(src,dst);QCOMPARE(copy.value("status").toString(),"completed");QVERIFY(copy.value("destinationVerified").toBool());QCOMPARE(dc::scanImage(dst).value("sha256"),scan.value("sha256"));
  QCOMPARE(dc::copyImage(src,dst).value("status").toString(),"error");QCOMPARE(dc::copyImage(src,src).value("status").toString(),"error");
 }
 void rejectsDeviceAndSymlink(){
  QVERIFY(!dc::isRegularSource("/dev/null"));QCOMPARE(dc::scanImage("/dev/null").value("status").toString(),"error");
  QVERIFY(!dc::isRegularSource("\\\\.\\PhysicalDrive0"));
  QTemporaryDir d;QFile f(d.filePath("original"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("abc");f.close();
#ifndef Q_OS_WIN
  QVERIFY(QFile::link(f.fileName(),d.filePath("link")));QVERIFY(!dc::isRegularSource(d.filePath("link")));
#endif
 }
 void cancelledCopyRemovesPartial(){
  QTemporaryDir d;QFile f(d.filePath("src"));QVERIFY(f.open(QIODevice::WriteOnly));f.write(QByteArray(2*1024*1024,'x'));f.close();
  std::atomic_bool stop=false;dc::Context ctx{&stop,[&](qint64,qint64){stop=true;}};
  QCOMPARE(dc::copyImage(f.fileName(),d.filePath("dst"),ctx).value("status").toString(),"cancelled");QVERIFY(!QFile::exists(d.filePath("dst")));QVERIFY(QFile::exists(f.fileName()));
 }
 void detectsWrappedFakeCapacity(){
  QHash<quint64,QByteArray> media;
  dc::ProbeIO io{[&](quint64 i,const QByteArray &b){media[i%4]=b;return true;},[&](quint64 i){return media.value(i%4);}};
  auto r=dc::verifyStorage(io,16,4096,76);
  QCOMPARE(r.value("status").toString(),"mismatch");QCOMPARE(r.value("verifiedBytes").toDouble(),double(4*4096));QCOMPARE(r.value("failedBytes").toDouble(),double(12*4096));
 }
 void detectsCorruption(){
  QHash<quint64,QByteArray> media;
  dc::ProbeIO io{[&](quint64 i,const QByteArray &b){media[i]=b;return true;},[&](quint64 i){auto b=media.value(i);if(i==3)b[20]=char(b.at(20)^1);return b;}};
  auto r=dc::verifyStorage(io,8,4096,99);QCOMPARE(r.value("failedBytes").toDouble(),4096.0);QCOMPARE(r.value("verifiedBytes").toDouble(),7*4096.0);
 }
 void writeFailureNotCapacityVerdict(){
  dc::ProbeIO io{[](quint64,const QByteArray &){return false;},[](quint64){return QByteArray{};}};
  auto r=dc::verifyStorage(io,8,4096,1);QCOMPARE(r.value("status").toString(),"error");QVERIFY(!r.contains("verifiedBytes"));
 }
 void directoryTestPreservesUserFile(){
  QTemporaryDir d;QFile f(d.filePath("important.txt"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("keep my data");f.close();
  QCOMPARE(dc::testDirectory(d.path(),1,false).value("status").toString(),"error");
  auto r=dc::testDirectory(d.path(),1,true);QCOMPARE(r.value("status").toString(),"completed");QVERIFY(r.value("testFilesRemoved").toBool());
  QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("keep my data"));QCOMPARE(QDir(d.path()).entryList(QDir::Files).size(),1);
 }
 void invalidAndEmptyInputs(){QTemporaryDir d;QFile f(d.filePath("empty"));QVERIFY(f.open(QIODevice::WriteOnly));f.close();QCOMPARE(dc::scanImage(f.fileName()).value("status").toString(),"error");QCOMPARE(dc::testDirectory(d.path(),0,true).value("status").toString(),"error");}
};
QTEST_GUILESS_MAIN(CoreTests)
#include "test_core.moc"
