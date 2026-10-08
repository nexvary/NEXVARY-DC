#include "Recovery.h"
#include "FilesystemRecovery.h"
namespace dc {
QJsonObject recoverFat32(const QString &s,const QString &d,const Context &c){auto r=recoverFilesystem(s,d,"FAT32",c);r.insert("operation","fat32_recovery");return r;}
}
