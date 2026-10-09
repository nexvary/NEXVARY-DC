Set-StrictMode -Version Latest
function Assert-ExternalDisk($Disk, $Request) {
 if ($Disk.IsBoot -or $Disk.IsSystem -or $Disk.IsOffline -or $Disk.IsReadOnly) { throw 'Protected, offline or read-only disk. No changes made.' }
 if ([string]$Disk.BusType -notin @('USB','SD','MMC')) { throw 'Write operations require an external USB/SD/MMC device. Internal disks are protected.' }
 if ([string]::IsNullOrWhiteSpace([string]$Disk.UniqueId)) { throw 'Disk has no stable identifier.' }
 if ([int]$Disk.Number -ne [int]$Request.number -or [string]$Disk.UniqueId -cne [string]$Request.uniqueId -or [string]$Disk.SerialNumber -cne [string]$Request.serial -or [long]$Disk.Size -ne [long]$Request.bytes) { throw 'Disk identity changed. Refresh and confirm again.' }
}
function Assert-Partition($Partition, $Request) {
 if ([int]$Partition.PartitionNumber -ne [int]$Request.partition -or [long]$Partition.Offset -ne [long]$Request.offset -or [long]$Partition.Size -ne [long]$Request.partitionBytes) { throw 'Partition changed. Refresh and confirm again.' }
 if ($Partition.IsBoot -or $Partition.IsSystem) { throw 'System partition is protected.' }
}
function Assert-FileSystem([string]$FileSystem, [long]$Size) {
 if ($FileSystem -notin @('NTFS','exFAT','FAT32')) { throw 'Unsupported filesystem.' }
 if ($FileSystem -eq 'FAT32' -and $Size -gt 32GB) { throw 'FAT32 formatting is limited to 32 GiB. Select exFAT/NTFS or a smaller partition.' }
}
function Get-EmptyDiskStyleAction($Disk, $Request, [int]$PartitionCount, [string]$Style) {
 Assert-ExternalDisk $Disk $Request
 if($Style -notin @('GPT','MBR') -or $PartitionCount -ne 0) { throw 'Partition conversion requires a verified empty disk and GPT/MBR style.' }
 if($Style -eq 'MBR' -and [long]$Disk.Size -gt 2TB) { throw 'MBR disks above 2 TiB are not supported.' }
 $current=[string]$Disk.PartitionStyle
 if($current -eq 'RAW') { return 'initialize' }
 if($current -notin @('GPT','MBR')) { throw 'Unknown current partition style.' }
 if($current -eq $Style) { return 'keep' }
 return 'convert'
}
