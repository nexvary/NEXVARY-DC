#pragma once
#include "Operations.h"
namespace dc {
// Signature carving from a regular rescue image; never writes to the source.
QJsonObject recoverFat32(const QString &source,const QString &directory,const Context &context={});
QJsonObject recoverImage(const QString &source,const QString &directory,const Context &context={});
}
