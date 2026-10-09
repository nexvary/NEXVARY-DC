#pragma once
#include <QJsonObject>
#include <QString>
namespace dc {
// English /CopyExit text from the pinned upstream version. No device commands.
QJsonObject parseCrystalReport(const QString &text);
}
