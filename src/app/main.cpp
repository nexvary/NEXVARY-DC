#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>
#include <QStandardPaths>
#include "Controller.h"
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
 app.setOrganizationName("NEXVARY");app.setApplicationName("Disk Care");app.setApplicationVersion("0.2.0");
 const auto args=app.arguments();
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
 if(args.contains("--english"))root->setProperty("arabic",false);
 const int page=args.indexOf("--page");
 if(page>=0 && page+1<args.size())root->setProperty("page",qBound(0,args[page+1].toInt(),9));
 if(args.contains("--compact")){root->setProperty("width",980);root->setProperty("height",700);}
 if(args.contains("--confirmation-proof") && smoke)QTimer::singleShot(500,root,[root]{QMetaObject::invokeMethod(root,"previewConfirmation");});
 if(smoke)QTimer::singleShot(2000,&app,[]{QCoreApplication::exit(uiWarnings?3:0);});
 const int shot=args.indexOf("--screenshot");
 if(shot>=0 && shot+1<args.size())QTimer::singleShot(2500,&app,[&engine,&app,args,shot]{auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());app.exit(window && window->grabWindow().save(args[shot+1])&&!uiWarnings?0:2);});
 return app.exec();
}
