#pragma once
#include <QJsonObject>
#include <QString>
namespace dc {
QString validateStorageRequest(const QJsonObject &disk,const QJsonObject &request);
QJsonObject summarizeSmart(const QJsonObject &raw);
}
