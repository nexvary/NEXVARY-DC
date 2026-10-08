#pragma once
#include "Operations.h"
namespace dc {
struct RecoveryOptions { int maxFiles=100000; int maxRecords=2000000; };
QJsonObject recoverFilesystem(const QString &source,const QString &directory,const QString &filesystem,
 const Context &context={},const RecoveryOptions &options={});
}
