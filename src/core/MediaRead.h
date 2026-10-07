#pragma once
#include "Operations.h"
namespace dc {
// Read-only physical-media scan. Caller must provide an enumerated device and size.
QJsonObject scanMedia(const QString &device,qint64 bytes,const Context &context={});
// Best-effort image: unreadable chunks become zeros and are recorded explicitly.
QJsonObject rescueMedia(const QString &device,qint64 bytes,const QString &destination,const Context &context={});
}
