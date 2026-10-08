#include "MediaRead.h"
#include <QFile>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QStorageInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <windows.h>
#include <winioctl.h>
#endif
namespace dc {
namespace {
constexpr qint64 Chunk=1024*1024;
QJsonObject fail(const QString &message){return {{"status","error"},{"message",message}};}
class Reader {
#ifdef Q_OS_WIN
 HANDLE handle=INVALID_HANDLE_VALUE;
#endif
 QFile file;
public:
 ~Reader(){
#ifdef Q_OS_WIN
 if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);
#endif
 }
 bool open(const QString &path){
 if(isRegularSource(path)){file.setFileName(path);return file.open(QIODevice::ReadOnly);}
#ifdef Q_OS_WIN
 if(!QRegularExpression(R"(^\\\\\.\\PHYSICALDRIVE[0-9]+$)",QRegularExpression::CaseInsensitiveOption).match(path).hasMatch())return false;
 handle=CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);return handle!=INVALID_HANDLE_VALUE;
#else
 if(!path.startsWith("/dev/") || QFileInfo(path).isSymLink())return false;
 file.setFileName(path);return file.open(QIODevice::ReadOnly);
#endif
 }
 int logicalSector(){
#ifdef Q_OS_WIN
  if(handle!=INVALID_HANDLE_VALUE){QByteArray data(2048,0);DWORD got=0;if(DeviceIoControl(handle,IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,nullptr,0,data.data(),DWORD(data.size()),&got,nullptr)&&got>=sizeof(DISK_GEOMETRY_EX))return int(reinterpret_cast<const DISK_GEOMETRY_EX*>(data.constData())->Geometry.BytesPerSector);return 0;}
#endif
  return 512;
 }
 QString identity(qint64 expected){
  if(file.isOpen()&&isRegularSource(file.fileName())){
   QFileInfo info(file);if(file.size()!=expected)return {};QCryptographicHash hash(QCryptographicHash::Sha256);
   for(qint64 pos:QList<qint64>{0,qMax<qint64>(0,expected/2-512),qMax<qint64>(0,expected-1024)}){if(!file.seek(pos))return {};hash.addData(file.read(qMin<qint64>(1024,expected-pos)));}
   return info.canonicalFilePath()+":"+QString::number(expected)+":"+QString::number(info.lastModified().toMSecsSinceEpoch())+":"+QString::fromLatin1(hash.result().toHex());
  }
#ifdef Q_OS_WIN
  GET_LENGTH_INFORMATION length{};DWORD got=0;if(!DeviceIoControl(handle,IOCTL_DISK_GET_LENGTH_INFO,nullptr,0,&length,sizeof(length),&got,nullptr)||length.Length.QuadPart!=expected)return {};
  STORAGE_PROPERTY_QUERY query{};query.PropertyId=StorageDeviceProperty;query.QueryType=PropertyStandardQuery;QByteArray data(16384,0);
  if(!DeviceIoControl(handle,IOCTL_STORAGE_QUERY_PROPERTY,&query,sizeof(query),data.data(),DWORD(data.size()),&got,nullptr)||got<sizeof(STORAGE_DEVICE_DESCRIPTOR))return {};
  auto *d=reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(data.constData());if(!d->SerialNumberOffset||d->SerialNumberOffset>=got)return {};
  auto serial=data.mid(d->SerialNumberOffset,got-d->SerialNumberOffset);int end=serial.indexOf(char(0));if(end<0)return {};serial.truncate(end);serial=serial.trimmed();if(serial.isEmpty())return {};
  return QString::fromLatin1(serial)+":"+QString::number(expected);
#else
  return {};
#endif
 }
 qint64 read(qint64 offset,char *data,qint64 count){
 if(file.isOpen()){if(!file.seek(offset))return -1;return file.read(data,count);}
#ifdef Q_OS_WIN
 LARGE_INTEGER pos;pos.QuadPart=offset;if(!SetFilePointerEx(handle,pos,nullptr,FILE_BEGIN))return -1;
 DWORD got=0;if(!ReadFile(handle,data,DWORD(count),&got,nullptr))return -1;return got;
#else
 if(!file.seek(offset))return -1;return file.read(data,count);
#endif
 }
};
bool outputIsSeparate(const QString &source,const QString &dest,bool resume=false){
 auto info=QFileInfo(dest);if((info.exists()&&!resume) || info.isSymLink() || !QFileInfo(info.absolutePath()).isDir())return false;
#ifdef Q_OS_WIN
 wchar_t volume[MAX_PATH];const auto folder=QDir::toNativeSeparators(info.absolutePath());
 if(!GetVolumePathNameW(reinterpret_cast<LPCWSTR>(folder.utf16()),volume,MAX_PATH))return false;
 QString root=QString::fromWCharArray(volume);if(root.size()<2 || root[1]!=':')return false;
 const QString volumeDevice="\\\\.\\"+root.left(2);
 HANDLE h=CreateFileW(reinterpret_cast<LPCWSTR>(volumeDevice.utf16()),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
 if(h==INVALID_HANDLE_VALUE)return false;
 QByteArray extents(16384,0);DWORD got=0;bool ok=DeviceIoControl(h,IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,nullptr,0,extents.data(),DWORD(extents.size()),&got,nullptr);CloseHandle(h);
 if(!ok || got<sizeof(VOLUME_DISK_EXTENTS))return false;
 auto *e=reinterpret_cast<VOLUME_DISK_EXTENTS*>(extents.data());bool parsed=false;auto n=source.mid(source.lastIndexOf('E',-1,Qt::CaseInsensitive)+1).toUInt(&parsed);if(!parsed)return false;
 if(e->NumberOfDiskExtents>(DWORD(extents.size())-offsetof(VOLUME_DISK_EXTENTS,Extents))/sizeof(DISK_EXTENT))return false;
 for(DWORD i=0;i<e->NumberOfDiskExtents;++i)if(e->Extents[i].DiskNumber==n)return false;
 return true;
#else
 Q_UNUSED(source);return false; // Physical rescue destination mapping on Linux not implemented yet.
#endif
}
QJsonObject perform(const QString &device,qint64 bytes,const QString &dest,const Context &context){
 const bool copy=!dest.isEmpty();
 if(bytes<=0 || bytes%512!=0)return fail("Invalid physical disk length.");
 if(copy && !isRegularSource(device) && !outputIsSeparate(device,dest))return fail("Choose a new destination on a different physical disk. Destination mapping could not be verified.");
 if(copy && QStorageInfo(QFileInfo(dest).absolutePath()).bytesAvailable()<bytes+64*1024*1024)return fail("Destination has insufficient free space for the full image and reserve.");
 Reader input;if(!input.open(device))return fail("Cannot open the enumerated disk read-only. Administrator permission may be required.");
 QFile output(dest),map(dest+".readmap.json");
 if(copy){if(!map.open(QIODevice::WriteOnly|QIODevice::NewOnly))return fail("Recovery map already exists or cannot be created.");if(!output.open(QIODevice::WriteOnly|QIODevice::NewOnly)){map.close();map.remove();return fail("Destination exists or cannot be created.");}}
 QByteArray buffer(Chunk,0);QJsonArray badRanges;QCryptographicHash hash(QCryptographicHash::Sha256);
 qint64 processed=0,failed=0;bool cancelled=false;QString error;
 while(processed<bytes){
  if(context.cancel && context.cancel->load()){cancelled=true;break;}
  qint64 count=qMin(Chunk,bytes-processed);const qint64 read=input.read(processed,buffer.data(),count);
  if(read!=count){buffer.fill(0);failed+=count;if(badRanges.size()<10000)badRanges.append(QJsonObject{{"offset",double(processed)},{"bytes",double(count)}});}
  if(copy){if(output.write(buffer.constData(),count)!=count){error="Image destination write failed.";break;}hash.addData(QByteArrayView(buffer.constData(),count));}
  processed+=count;if(context.progress)context.progress(processed,copy?bytes*2:bytes);
 }
 QJsonObject result{{"status",cancelled?"cancelled":!error.isEmpty()?"error":failed?"mismatch":"completed"},{"operation",copy?"media_rescue":"surface_read"},{"device",device},{"processedBytes",double(processed)},{"unreadableBytes",double(failed)},{"unreadableRanges",badRanges},{"granularityBytes",double(Chunk)},{"rangeListTruncated",failed/Chunk>10000},{"physicalRepairPerformed",false}};
 if(!error.isEmpty())result.insert("message",error);
 if(copy){
  if(!output.flush()){result.insert("status","error");result.insert("message","Image destination flush failed.");}
  output.close();auto expected=hash.result().toHex();
  result.insert("destination",dest);result.insert("partialImageRetained",processed<bytes);result.insert("sha256Written",QString::fromLatin1(expected));
  if(error.isEmpty() && result.value("status")!="error")result.insert("message",failed?"Image contains zero-filled unreadable ranges. Read map identifies them; this is not a complete data recovery.":"Read-only media image created. Partial images and read maps are retained when interrupted.");
  if(!cancelled && error.isEmpty() && result.value("status")!="error"){
   QFile check(dest);QCryptographicHash readback(QCryptographicHash::Sha256);qint64 verified=0;
   if(!check.open(QIODevice::ReadOnly)){result.insert("status","error");result.insert("message","Cannot verify image destination.");}
   else {while(!check.atEnd()){if(context.cancel && context.cancel->load()){cancelled=true;break;}auto block=check.read(Chunk);if(block.isEmpty() && check.error()!=QFileDevice::NoError){error="Image readback failed.";break;}readback.addData(block);verified+=block.size();if(context.progress)context.progress(bytes+verified,bytes*2);}
    bool matches=!cancelled && error.isEmpty() && verified==processed && readback.result().toHex()==expected;
    result.insert("imageReadbackVerified",matches);result.insert("verifiedBytes",double(verified));if(!matches){result.insert("status",cancelled?"cancelled":"error");result.insert("message","Image readback incomplete or mismatched.");}
   }
  }
  auto json=QJsonDocument(result).toJson();if(map.write(json)!=json.size() || !map.flush()){result.insert("status","error");result.insert("message","Recovery map could not be saved.");}map.close();
 }
 return result;
}
}
QJsonObject scanMedia(const QString &device,qint64 bytes,const Context &context){return perform(device,bytes,{},context);}
QJsonObject rescueMedia(const QString &device,qint64 bytes,const QString &dest,const Context &context,const RescueOptions &options){
 if(!isRegularSource(device)&&!outputIsSeparate(device,dest,options.resume))return fail("Destination disk separation could not be verified.");
 if(QFileInfo(device).canonicalFilePath()==QFileInfo(dest).canonicalFilePath()&&QFileInfo(dest).exists())return fail("Source and destination must differ.");
 Reader reader;if(!reader.open(device))return fail("Cannot open source read-only.");
 if(!isRegularSource(device)&&reader.logicalSector()!=options.sectorBytes)return fail("Selected sector size does not match the actual logical sector size.");
 QString identity=reader.identity(bytes);if(identity.isEmpty())return fail("Cannot verify source identity and actual length.");
 RescueSource source{bytes,identity,[&](qint64 offset,qint64 count){QByteArray b(count,0);auto n=reader.read(offset,b.data(),count);if(n!=count)return QByteArray{};return b;}};
 return rescueStream(source,dest,options,context);
}
}
