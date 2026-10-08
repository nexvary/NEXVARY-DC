#include "StoragePolicy.h"
#include <QJsonArray>
namespace dc {
QString validateStorageRequest(const QJsonObject &d,const QJsonObject &r) {
 const auto action=r.value("action").toString();
 if(!QStringList{"format","delete","create","layout","check","repair","windows_usb","windows_bios_usb","linux_usb"}.contains(action))return "Unsupported storage action.";
 if(d.value("boot").toBool() || d.value("system").toBool() || d.value("offline").toBool() || d.value("readOnly").toBool())return "System, offline or read-only disk is protected.";
 if(!d.value("external").toBool() || !QStringList{"USB","SD","MMC"}.contains(d.value("transport").toString()))return "Only external USB/SD/MMC media can be changed. Internal disks are protected.";
 if(d.value("uniqueId").toString().trimmed().isEmpty())return "Stable disk identity is unavailable.";
 for(const auto &key:QStringList{"number","uniqueId","serial","bytes"})if(d.value(key)!=r.value(key))return "Disk identity changed; refresh and confirm again.";
 QJsonObject partition;
 if(QStringList{"format","delete","check","repair"}.contains(action)) {
  for(auto p:d.value("partitions").toArray())if(p.toObject().value("partition")==r.value("partition"))partition=p.toObject();
  if(partition.isEmpty())return "Select an existing partition.";
  for(const auto &key:QStringList{"offset","partitionBytes"})if(partition.value(key)!=r.value(key))return "Partition changed; refresh and confirm again.";
  if(partition.value("boot").toBool() || partition.value("system").toBool())return "System partition is protected.";
 }
 if(QStringList{"format","create","layout"}.contains(action)) {
  auto fs=r.value("filesystem").toString();
  if(!QStringList{"NTFS","exFAT","FAT32"}.contains(fs))return "Unsupported filesystem.";
  const double size=action=="format"?partition.value("partitionBytes").toDouble():action=="create"?r.value("sizeMiB").toDouble()*1048576:d.value("bytes").toDouble();
  if(fs=="FAT32" && size>32.0*1073741824)return "Windows FAT32 formatting is limited to 32 GiB.";
  if(action=="create" && (size<16.0*1048576 || size>d.value("bytes").toDouble()))return "Invalid partition size.";
 }
 if(action=="layout" && !QStringList{"GPT","MBR"}.contains(r.value("style").toString()))return "Unsupported partition style.";
 if(action=="layout" && r.value("style")=="MBR" && d.value("bytes").toDouble()>2.0*1099511627776)return "MBR above 2 TiB is unsupported.";
 return {};
}
QJsonObject summarizeSmart(const QJsonObject &raw) {
 QJsonObject s;
 const auto smart=raw.value("smart_status").toObject();
 s.insert("health",smart.contains("passed")?(smart.value("passed").toBool()?"passed":"failed"):"unknown");
 s.insert("model",raw.value("model_name").toString(raw.value("scsi_model_name").toString()));
 s.insert("serial",raw.value("serial_number"));s.insert("firmware",raw.value("firmware_version"));
 s.insert("temperature",raw.value("temperature").toObject().value("current"));
 s.insert("hours",raw.value("power_on_time").toObject().value("hours"));
 QJsonArray attrs;
 for(auto item:raw.value("ata_smart_attributes").toObject().value("table").toArray()) {
  const auto a=item.toObject();const int id=a.value("id").toInt();
  if(QList<int>{5,9,187,188,194,197,198,199}.contains(id))attrs.append(QJsonObject{{"id",id},{"name",a.value("name")},{"raw",a.value("raw").toObject().value("value")},{"value",a.value("value")},{"threshold",a.value("thresh")},{"whenFailed",a.value("when_failed")}});
 }
 const auto nvme=raw.value("nvme_smart_health_information_log").toObject();
 for(const auto &key:QStringList{"critical_warning","percentage_used","media_errors","num_err_log_entries","available_spare"})if(nvme.contains(key))attrs.append(QJsonObject{{"name",key},{"raw",nvme.value(key)}});
 s.insert("attributes",attrs);
 return s;
}
}
