#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
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
 void smartPreferenceBounds(){
  Controller c;QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),20000);
  c.setSmartPreferences({{"hideSerial",true},{"refreshSeconds",60},{"pendingAlert",100}});
  QCOMPARE(c.smartPreferences().value("hideSerial").toBool(),true);QCOMPARE(c.smartPreferences().value("pendingAlert").toInt(),100);
  c.setSmartPreferences({{"refreshSeconds",0}});QCOMPARE(c.result().value("status").toString(),"error");QCOMPARE(c.smartPreferences().value("refreshSeconds").toInt(),60);
  c.setMonitoring(true);QVERIFY(!c.monitoring());
  c.setSmartPreferences({{"hideSerial",false},{"refreshSeconds",300},{"pendingAlert",1}});
 }
#ifndef Q_OS_WIN
 void smartMonitorIdentityAndReadFailure(){
  struct Environment {QByteArray old=qgetenv("PATH");~Environment(){qputenv("PATH",old);}} environment;
  QTemporaryDir dir;
  auto executable=[&](const QString &name,const QByteArray &text){QFile f(dir.filePath(name));if(!f.open(QIODevice::WriteOnly))return false;f.write(text);f.close();return f.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);};
  QVERIFY(executable("lsblk","#!/bin/sh\nprintf '%s' '{\"blockdevices\":[{\"path\":\"/dev/nexvary-test\",\"type\":\"disk\",\"serial\":\"TEST-SERIAL\",\"model\":\"SYNTHETIC FIXTURE\",\"size\":1048576}]}'\n"));
  auto smart=[&](const QString &serial){return QByteArray("#!/bin/sh\nprintf '%s' '")+QJsonDocument(QJsonObject{{"serial_number",serial},{"smart_status",QJsonObject{{"passed",true}}},{"ata_smart_attributes",QJsonObject{{"table",QJsonArray{QJsonObject{{"id",197},{"raw",QJsonObject{{"value",16}}}}}}}}}).toJson(QJsonDocument::Compact)+"'\n";};
  QVERIFY(executable("smartctl",smart("TEST-SERIAL")));qputenv("PATH",dir.path().toUtf8()+":"+environment.old);
  Controller c;QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),5000);QSignalSpy alerts(&c,&Controller::healthAlert);
  c.inspectHealth("/dev/nexvary-test");QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),5000);
  QCOMPARE(c.health().value("summary").toMap().value("health").toString(),"caution");QCOMPARE(c.health().value("summary").toMap().value("pending").toInt(),16);QVERIFY(!alerts.isEmpty());
  c.setSmartPreferences({{"pendingAlert",100}});QCOMPARE(c.health().value("summary").toMap().value("health").toString(),"caution");
  c.setMonitoring(true);QVERIFY(c.monitoring());
  QVERIFY(executable("smartctl",smart("TEST-CHANGED")));c.inspectHealth("/dev/nexvary-test");QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),5000);QVERIFY(!c.monitoring());
  c.setMonitoring(true);QVERIFY(c.monitoring());QVERIFY(executable("smartctl","#!/bin/sh\nprintf 'not-json'\nexit 1\n"));c.inspectHealth("/dev/nexvary-test");QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),5000);QVERIFY(!c.monitoring());QCOMPARE(c.health().value("summary").toMap().value("health").toString(),"unknown");
  c.setSmartPreferences({{"pendingAlert",1}});
 }
#endif
 void jobsHistoryAndExport(){
  QStandardPaths::setTestModeEnabled(true);
  QCoreApplication::setApplicationName("DC-controller-test");
  Controller c;QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),20000);
  QVERIFY(!c.volumes().isEmpty());
  c.executeStorage("fabricated-token","device",true);QCOMPARE(c.result().value("status").toString(),"error");
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
