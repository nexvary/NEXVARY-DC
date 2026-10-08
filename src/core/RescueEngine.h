#pragma once
#include "Operations.h"
namespace dc {
struct RescueOptions { bool resume=false; int sectorBytes=512; int retries=1; };
// Injectable read-only transport for deterministic fault/cancellation tests.
struct RescueSource {
 qint64 bytes=0;
 QString identity;
 std::function<QByteArray(qint64,qint64)> read;
};
QJsonObject rescueStream(RescueSource &source,const QString &destination,const RescueOptions &options,const Context &context={});
}
