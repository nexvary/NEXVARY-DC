#include "CrystalReport.h"
#include <QJsonArray>
#include <QStringList>
#include <utility>
#include <QRegularExpression>
namespace dc {
QJsonObject parseCrystalReport(const QString &input) {
 if(input.size()>16*1024*1024 || !input.contains("CrystalDiskInfo 9.9.2"))
  return {{"status","error"},{"message","Unexpected CrystalDiskInfo report version or size."}};
 const auto lines=QString(input).replace("\r\n","\n").split('\n');
 QJsonArray drives;QJsonObject current,fields;QJsonArray attributes;
 auto finish=[&]{
  if(current.isEmpty())return;
  QJsonObject summary{{"model",fields.value("Model")},{"serial",fields.value("Serial Number")},{"firmware",fields.value("Firmware")},{"interface",fields.value("Interface")},{"features",fields.value("Features")},{"transferMode",fields.value("Transfer Mode")},{"driveLetters",fields.value("Drive Letter")}};
  auto integer=[&](const QString &key)->QJsonValue{auto m=QRegularExpression("^[ ]*([0-9]+)").match(fields.value(key).toString());return m.hasMatch()?QJsonValue(m.captured(1).toDouble()):QJsonValue();};
  summary.insert("hours",integer("Power On Hours"));summary.insert("powerCycles",integer("Power On Count"));summary.insert("rotationRate",integer("Rotation Rate"));summary.insert("temperature",integer("Temperature"));
  const auto health=fields.value("Health Status").toString();QString state=health.startsWith("Good")?"passed":health.startsWith("Caution")?"caution":health.startsWith("Bad")?"failed":"unknown";
  bool risk=state=="caution"||state=="failed";
  const bool isNvme=fields.value("Interface").toString().contains("NVM",Qt::CaseInsensitive);
  for(auto item:attributes){auto a=item.toObject();const auto id=a.value("hexId").toString();const auto n=a.value("raw").toDouble();if(!isNvme){if(id=="05")summary.insert("reallocated",n);if(id=="C5")summary.insert("pending",n);if(id=="C6")summary.insert("uncorrectable",n);}risk|=a.value("warning").toBool();}
  if(state=="passed"&&risk)state="caution";
  summary.insert("health",state);summary.insert("rescueFirst",risk);summary.insert("warningIndicatorsPresent",risk);summary.insert("attributes",attributes);summary.insert("upstreamHealth",health);
  current.insert("summary",summary);current.insert("fields",fields);drives.append(current);current={};fields={};attributes={};
 };
 QRegularExpression heading("^ \\(([0-9]{2,3})\\) (.+)$"), field("^\\s*([^:]+?)\\s*:\\s*(.*?)\\s*$");
 QRegularExpression ata("^([0-9A-F]{2})\\s+([0-9_]{3})\\s+([0-9_]{3})\\s+([0-9_]{3})\\s+([0-9A-F]{12,16})\\s+(.+)$");
 QRegularExpression nvme("^([0-9A-F]{2})\\s+([0-9A-F]{12,32})\\s+(.+)$");
 for(qsizetype i=0;i<lines.size();++i){
  const auto line=lines[i];auto h=heading.match(line);
  if(h.hasMatch()&&i+1<lines.size()&&lines[i+1].startsWith("------------")){finish();current={{"index",h.captured(1).toInt()},{"model",h.captured(2)},{"engine","CrystalDiskInfo 9.9.2"}};continue;}
  if(current.isEmpty())continue;
  auto f=field.match(line);if(f.hasMatch()){fields.insert(f.captured(1).trimmed(),f.captured(2));continue;}
  auto a=ata.match(line.trimmed());auto n=nvme.match(line.trimmed());
  if(!a.hasMatch()&&!n.hasMatch())continue;
  const QString id=a.hasMatch()?a.captured(1):n.captured(1), hex=a.hasMatch()?a.captured(5):n.captured(2);
  bool ok=false;const auto count=hex.toULongLong(&ok,16);
  const bool isNvme=fields.value("Interface").toString().contains("NVM",Qt::CaseInsensitive);
  const bool warning=ok&&count>0&&(isNvme?QStringList{"01","0E"}.contains(id):QStringList{"05","C5","C6"}.contains(id));
  QJsonObject row{{"hexId",id},{"id",id.toInt(nullptr,16)},{"name",a.hasMatch()?a.captured(6):n.captured(3)},{"rawHex",hex},{"rawText",hex},{"warning",warning}};
  // Preserve huge NVMe values as hex instead of truncating them to a double.
  if(ok&&count<=9007199254740991ULL)row.insert("raw",double(count));
  if(a.hasMatch()){for(auto pair:{std::pair{"value",2},std::pair{"worst",3},std::pair{"threshold",4}}){auto v=a.captured(pair.second);bool valid=false;int number=v.remove('_').toInt(&valid);if(valid)row.insert(pair.first,number);}}
  attributes.append(row);
 }
 finish();
 return {{"status","completed"},{"operation","crystal_read"},{"engine","CrystalDiskInfo 9.9.2"},{"drives",drives},{"rawText",input},{"deviceMetadataDetected",!drives.isEmpty()},{"message",drives.isEmpty()?"Engine completed but detected no supported disks; health remains unknown.":"SMART data imported from the bundled upstream engine; unsupported devices remain unknown."}};
}
}
