#pragma once
#include <QFile>
#include <QList>
#include <QStringList>
#include "Operations.h"
namespace dc {
class ReadCoverage {
 struct Gap {qint64 begin,end;};
 QList<Gap> m_gaps;
 QString m_path;
 qint64 m_size=0;
public:
 QString load(QFile &image,const Context &context,QStringList &warnings);
 qint64 missing(qint64 offset,qint64 length)const;
 QString path()const{return m_path;}
};
}
