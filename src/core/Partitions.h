#pragma once
#include <QFile>
#include <QList>
#include <QStringList>
namespace dc {
struct ImageVolume { qint64 offset=0, length=0; QString scheme; };
// Includes a raw volume candidate; every partition extent is bounded by the image.
QList<ImageVolume> imageVolumes(QFile &image, QStringList &warnings);
}
