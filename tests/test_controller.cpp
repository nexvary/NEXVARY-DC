#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include "Controller.h"
class ControllerTests:public QObject {
 Q_OBJECT
private slots:
 void initTestCase(){QStandardPaths::setTestModeEnabled(true);QCoreApplication::setApplicationName("DC-controller-test");}
#ifndef Q_OS_WIN
 void cancelsSlowDiscovery(){
  QTemporaryDir d;QFile stub(d.filePath("lsblk"));QVERIFY(stub.open(QIODevice::WriteOnly));
  stub.write("#!/bin/sh\nsleep 10\nprintf '{\"blockdevices\":[]}'\n");stub.close();QVERIFY(stub.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
  const auto old=qgetenv("PATH");qputenv("PATH",d.path().toUtf8()+":"+old);
  {Controller c;QVERIFY(c.busy());QTest::qWait(100);c.cancel();QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),3000);QCOMPARE(c.result().value("status").toString(),"cancelled");}
  qputenv("PATH",old);
 }
#endif
 void jobsHistoryAndExport(){
  QStandardPaths::setTestModeEnabled(true);
  QCoreApplication::setApplicationName("DC-controller-test");
  Controller c;QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),20000);
  QVERIFY(!c.volumes().isEmpty());
  c.inspectHealth("untrusted-device");QCOMPARE(c.result().value("status").toString(),"error");
  QTemporaryDir d;QFile f(d.filePath("source.img"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("valid image data");f.close();
  c.scanImage(f.fileName());QVERIFY(c.busy());
  c.copyImage(f.fileName(),d.filePath("should-not-start"));
  QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),5000);QCOMPARE(c.result().value("operation").toString(),"image_read");QCOMPARE(c.result().value("status").toString(),"completed");QVERIFY(!QFile::exists(d.filePath("should-not-start")));
  QVERIFY(!c.history().isEmpty());
  auto report=d.filePath("report.json");QVERIFY(c.exportReport(report));QVERIFY(!c.exportReport(report));
  QFile out(report);QVERIFY(out.open(QIODevice::ReadOnly));QVERIFY(QJsonDocument::fromJson(out.readAll()).isObject());
  c.testCapacity(d.path(),1,false);QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),5000);QCOMPARE(c.result().value("status").toString(),"error");
  c.testCapacity(d.path(),1,true);QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),5000);QCOMPARE(c.result().value("status").toString(),"completed");QCOMPARE(c.result().value("verifiedBytes").toDouble(),1048576.0);
 }
};
QTEST_GUILESS_MAIN(ControllerTests)
#include "test_controller.moc"
