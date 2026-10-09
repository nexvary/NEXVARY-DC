#pragma once
#include <QJsonObject>
#include <QString>
#include <atomic>
namespace dc {
QJsonObject readCrystalEngine(const QString &package,std::atomic_bool *cancel=nullptr);
QJsonObject launchCrystalEngine(const QString &package,const QString &stateDirectory,bool arabic);
}
