#include "ReadCoverage.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <cmath>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <limits>
namespace dc {
namespace {
bool number(const QJsonValue &value,qint64 &out){
 if(value.isString()){bool ok=false;out=value.toString().toLongLong(&ok);return ok&&out>=0;}
 if(!value.isDouble())return false;const double n=value.toDouble();
 if(!std::isfinite(n)||n<0||n>=double(std::numeric_limits<qint64>::max()))return false;
 out=qint64(n);return double(out)==n;
}
}
QString ReadCoverage::load(QFile &image,const Context &ctx,QStringList &warnings){
 m_gaps.clear();m_path.clear();m_size=image.size();
 const auto source=QFileInfo(image.fileName()).absoluteFilePath();QStringList maps;
 for(const auto &suffix:QStringList{".readmap.jsonl",".dc-ahci.jsonl"})if(QFileInfo::exists(source+suffix))maps.append(source+suffix);
 if(maps.isEmpty())return {};
 if(maps.size()!=1)return "Multiple adjacent rescue maps; resolve the ambiguity before recovery.";
 const auto path=maps.first();QFileInfo info(path);if(!info.isFile()||info.isSymLink())return "Rescue map must be a regular local file.";
 QFile map(path);if(!map.open(QIODevice::ReadOnly))return "Cannot open rescue map.";
 const auto mapSize=map.size();const auto mapTime=info.lastModified();
 auto line=map.readLine(65537);if(!line.endsWith('\n')||line.size()>65536)return "Invalid rescue map header.";
 QJsonParseError parse;auto header=QJsonDocument::fromJson(line,&parse).object();
 if(parse.error!=QJsonParseError::NoError||header.value("schema").toInt()!=1)return "Unsupported/corrupt rescue map schema.";
 const bool direct=header.value("identity").isObject();qint64 declared=0,sector=512;
 if(!number(direct?header.value("identity").toObject().value("bytes"):header.value("sourceBytes"),declared)||declared<=0||m_size>declared)return "Rescue map source/image size mismatch.";
 if(!direct&&!number(header.value("sectorBytes"),sector))return "Missing rescue sector size.";
 if(sector<512||sector>65536||(sector&(sector-1))||declared%sector)return "Invalid rescue sector size.";
 if(header.value("destination").toString().isEmpty())return "Missing rescue image identity.";
 if(header.value("destination").toString()!=source)warnings.append("Image/map moved: content SHA-256 is verified; original path/inode is not used as a resume authorization.");
 qint64 position=0;
 auto add=[&](qint64 first,qint64 end){if(!m_gaps.isEmpty()&&m_gaps.last().end==first)m_gaps.last().end=end;else m_gaps.append({first,end});};
 while(!map.atEnd()){
  if(ctx.cancelled())return "Rescue map verification cancelled.";
  line=map.readLine(1048577);
  if(!line.endsWith('\n')){if(line.size()>1048576||!map.atEnd())return "Rescue map row exceeds limit.";warnings.append("Torn map tail ignored; uncommitted image bytes remain unknown.");break;}
  if(line.size()>1048576)return "Rescue map row exceeds limit.";
  auto row=QJsonDocument::fromJson(line,&parse).object();qint64 offset=0,count=0;
  if(parse.error!=QJsonParseError::NoError||!number(row.value("offset"),offset)||!number(row.value(direct?"length":"bytes"),count)||offset!=position||count<=0||count>1048576||count%sector||offset>m_size||count>m_size-offset||!row.value("bad").isArray())return "Rescue map is discontinuous or outside image bounds.";
  const auto phase=row.value("phase").toString("sector-checked");if(direct&&phase!="sector-checked"&&phase!="deferred")return "Unknown rescue phase.";
  if(!image.seek(offset))return "Cannot verify mapped image bytes.";auto block=image.read(count);
  const auto sha=row.value("sha256").toString().toLower();
  if(!QRegularExpression("^[0-9a-f]{64}$").match(sha).hasMatch()||block.size()!=count||QString::fromLatin1(QCryptographicHash::hash(block,QCryptographicHash::Sha256).toHex())!=sha)return "Rescue image/map SHA-256 mismatch.";
  qint64 end=position;
  for(auto entry:row.value("bad").toArray()){
   const auto pair=entry.toArray();qint64 first=0,length=0;
   if(pair.size()!=2||!number(pair[0],first)||!number(pair[1],length)||first<end||first%sector||length<=0||length%sector||first>position+count||length>position+count-first)return "Invalid missing-sector range.";
   if(block.mid(first-position,length)!=QByteArray(length,0))return "Missing-sector range is not zero-filled as declared.";
   add(first,first+length);end=first+length;if(m_gaps.size()>2000000)return "Too many disjoint missing ranges.";
  }
  position+=count;ctx.update(position,m_size);
 }
 if(map.size()!=mapSize||QFileInfo(path).lastModified()!=mapTime||image.size()!=m_size)return "Image/map changed during verification.";
 if(position<m_size)add(position,m_size);
 m_path=path;warnings.append("Rescue map SHA-256 verified. Missing/deferred source bytes are not complete recovered data.");return {};
}
qint64 ReadCoverage::missing(qint64 offset,qint64 length)const{
 if(m_path.isEmpty()||length<=0)return 0;
 if(offset<0||offset>m_size||length>m_size-offset)return length;
 const auto end=offset+length;qint64 sum=0;
 auto it=std::lower_bound(m_gaps.cbegin(),m_gaps.cend(),offset,[](const Gap &g,qint64 p){return g.end<=p;});
 for(;it!=m_gaps.cend()&&it->begin<end;++it)sum+=qMin(end,it->end)-qMax(offset,it->begin);
 return sum;
}
}
