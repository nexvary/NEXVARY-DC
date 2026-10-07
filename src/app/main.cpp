#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>
#include <QStandardPaths>
#include "Controller.h"
int main(int argc,char **argv) {
 QQuickStyle::setStyle("Basic");
 QGuiApplication app(argc,argv);
 app.setOrganizationName("NEXVARY");app.setApplicationName("Disk Care");app.setApplicationVersion("0.1.0");
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
 if(page>=0 && page+1<args.size())root->setProperty("page",qBound(0,args[page+1].toInt(),6));
 if(args.contains("--compact")){root->setProperty("width",980);root->setProperty("height",700);}
 if(smoke)QTimer::singleShot(2000,&app,&QCoreApplication::quit);
 const int shot=args.indexOf("--screenshot");
 if(shot>=0 && shot+1<args.size())QTimer::singleShot(2500,&app,[&engine,&app,args,shot]{auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());app.exit(window && window->grabWindow().save(args[shot+1])?0:2);});
 return app.exec();
}
