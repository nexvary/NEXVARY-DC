#include <QGuiApplication>
#include <QIcon>
#include <QFontMetrics>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>
#include <QStandardPaths>
#include "Controller.h"
#include "StoragePolicy.h"
#include "CrystalEngine.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <cstdio>
namespace { bool uiWarnings=false;
void messages(QtMsgType type,const QMessageLogContext &,const QString &message){
 auto utf8=message.toUtf8();std::fprintf(stderr,"%s\n",utf8.constData());
 if(type==QtWarningMsg && (message.contains("QML") || message.contains("QQml") || message.contains("qrc:")))uiWarnings=true;
}
}

int main(int argc,char **argv) {
 qInstallMessageHandler(messages);
 QQuickStyle::setStyle("Basic");
 QGuiApplication app(argc,argv);
 app.setWindowIcon(QIcon(":/assets/icons/brand.png"));
 app.setOrganizationName("NEXVARY");app.setApplicationName("Disk Care");app.setApplicationVersion(QStringLiteral(DC_APP_VERSION));
 const auto args=app.arguments();
 const int crystalProof=args.indexOf("--crystal-engine-proof");
 if(crystalProof>=0){
  if(crystalProof+1>=args.size())return 2;
  const auto result=dc::readCrystalEngine(QDir(QCoreApplication::applicationDirPath()).filePath("engines/crystaldiskinfo"));
  QFile output(args[crystalProof+1]);if(!output.open(QIODevice::WriteOnly|QIODevice::NewOnly))return 2;
  const auto bytes=QJsonDocument(result).toJson(QJsonDocument::Indented);if(output.write(bytes)!=bytes.size()||!output.flush())return 2;
  return result.value("status")=="completed"?0:1;
 }
 const bool smoke=args.contains("--smoke-test");
 if(smoke) QStandardPaths::setTestModeEnabled(true);
 Controller controller;
 QQmlApplicationEngine engine;
 engine.rootContext()->setContextProperty("backend",&controller);
 QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&app,[]{QCoreApplication::exit(1);},Qt::QueuedConnection);
 if(smoke) QObject::connect(&engine,&QQmlEngine::warnings,&app,[](const QList<QQmlError>&){QCoreApplication::exit(3);});
 engine.loadFromModule("Nexvary.DC","Main");
 if(engine.rootObjects().isEmpty())return 1;
 auto *root=engine.rootObjects().first();
 if(args.contains("--font-proof")) {
  const QFontMetrics metrics(root->property("font").value<QFont>());
  for(char32_t c:U"A9\u0627\u0644\u0639\u0631\u0628\u064a\u0629")if(c && !metrics.inFontUcs4(c)){std::fprintf(stderr,"Missing UI glyph U+%04X\n",unsigned(c));return 4;}
 }
 if(args.contains("--english"))root->setProperty("arabic",false);
 if(args.contains("--selective-retry")){root->setProperty("diskRescueWorkspace",true);root->setProperty("selectiveRetryEnabled",true);}
 if(args.contains("--windows-recovery"))root->setProperty("windowsRecoveryMode",true);
 if(args.contains("--smart-proof")){
  const QJsonObject raw{{"model_name","SYNTHETIC TEST — ST1000DM010-2EP102"},{"serial_number","TEST-C5-16"},{"firmware_version","1001"},{"smart_status",QJsonObject{{"passed",true}}},{"temperature",QJsonObject{{"current",33}}},{"power_on_time",QJsonObject{{"hours",11862}}},{"power_cycle_count",3124},{"device",QJsonObject{{"protocol","ATA"}}},{"ata_smart_attributes",QJsonObject{{"table",QJsonArray{QJsonObject{{"id",197},{"name","Current_Pending_Sector"},{"value",100},{"worst",100},{"thresh",0},{"raw",QJsonObject{{"value",16},{"string","16"}}}},QJsonObject{{"id",5},{"name","Reallocated_Sector_Ct"},{"raw",QJsonObject{{"value",0}}}},QJsonObject{{"id",198},{"name","Offline_Uncorrectable"},{"raw",QJsonObject{{"value",0}}}}}}}}};
  root->setProperty("healthProof",QJsonObject{{"status","completed"},{"engine","LABELLED SYNTHETIC FIXTURE"},{"summary",dc::summarizeSmart(raw)}}.toVariantMap());
 }
 if(args.contains("--boot-repair"))root->setProperty("bootRepairMode",true);
 const int page=args.indexOf("--page");
 if(page>=0 && page+1<args.size())root->setProperty("page",qBound(0,args[page+1].toInt(),9));
 if(args.contains("--compact")){root->setProperty("width",980);root->setProperty("height",700);}
 if(args.contains("--confirmation-proof") && smoke)QTimer::singleShot(500,root,[root]{QMetaObject::invokeMethod(root,"previewConfirmation");});
 if(smoke)QTimer::singleShot(2000,&app,[]{QCoreApplication::exit(uiWarnings?3:0);});
 const int shot=args.indexOf("--screenshot");
 if(shot>=0 && shot+1<args.size())QTimer::singleShot(2500,&app,[&engine,&app,args,shot]{auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());app.exit(window && window->grabWindow().save(args[shot+1])&&!uiWarnings?0:2);});
 return app.exec();
}
