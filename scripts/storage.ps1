param([Parameter(Mandatory)][string]$RequestBase64)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
Import-Module (Join-Path $PSHOME 'Modules/CimCmdlets/CimCmdlets.psd1') -ErrorAction Stop
Import-Module (Join-Path $PSHOME 'Modules/Storage/Storage.psd1') -ErrorAction Stop
$ProgressPreference='SilentlyContinue'
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
. (Join-Path $PSScriptRoot 'policy.ps1')
. (Join-Path $PSScriptRoot 'hybrid.ps1')
$r=([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($RequestBase64)) | ConvertFrom-Json)
function Emit($Value) { $Value | ConvertTo-Json -Depth 12 -Compress }
function FreshDisk { Get-Disk -Number ([int]$r.number) -ErrorAction Stop }
function Guard {
 $d=FreshDisk
 Assert-ExternalDisk $d $r
 $parts=@(Get-Partition -DiskNumber $d.Number -ErrorAction SilentlyContinue)
 $pageLetters=@(Get-CimInstance Win32_PageFileUsage -ErrorAction Stop | ForEach-Object { $_.Name.Substring(0,1) })
 foreach($p in $parts) { if($p.DriveLetter -and ([string]$p.DriveLetter -in $pageLetters)) { throw 'Disk contains an active pagefile.' } }
 foreach($path in @($PSScriptRoot, $env:USERPROFILE, $r.source, $r.appDirectory)) {
  if($path -and [IO.Path]::IsPathRooted([string]$path)) {
   $resolved=@(Get-Volume -FilePath ([string]$path) -ErrorAction Stop | Get-Partition -ErrorAction Stop)
   if($resolved | Where-Object { $_.DiskNumber -eq $d.Number }) { throw 'Disk contains the application, user profile or source image through a mounted path.' }
   $root=[IO.Path]::GetPathRoot([string]$path).TrimEnd('\').TrimEnd(':')
   if($root.Length -eq 1 -and ($parts | Where-Object { [string]$_.DriveLetter -eq $root })) { throw 'Disk contains the application, user profile or source image.' }
  }
 }
 return $d
}
try {
 if($r.action -eq 'inventory') {
  $ds=@(Get-Disk | ForEach-Object {
   $d=$_
   $ps=@(Get-Partition -DiskNumber $d.Number -ErrorAction SilentlyContinue | ForEach-Object {
    $v=$_ | Get-Volume -ErrorAction SilentlyContinue
    $fs='';$label='';if($v){$fs=[string]$v.FileSystem;$label=[string]$v.FileSystemLabel}
    @{ partition=[int]$_.PartitionNumber;offset=[long]$_.Offset;partitionBytes=[long]$_.Size;letter=[string]$_.DriveLetter;filesystem=$fs;label=$label;boot=[bool]$_.IsBoot;system=[bool]$_.IsSystem }
   })
   @{device="\\.\PHYSICALDRIVE$($d.Number)";number=[int]$d.Number;model=[string]$d.FriendlyName;serial=[string]$d.SerialNumber;uniqueId=[string]$d.UniqueId;bytes=[long]$d.Size;transport=[string]$d.BusType;style=[string]$d.PartitionStyle;boot=[bool]$d.IsBoot;system=[bool]$d.IsSystem;readOnly=[bool]$d.IsReadOnly;offline=[bool]$d.IsOffline;partitions=$ps;external=([string]$d.BusType -in @('USB','SD','MMC'))}
  })
  Emit @{status='completed';operation='disk_discovery';devices=$ds};exit 0
 }
 $admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
 if(!$admin) { throw 'Administrator permission required. Use the administrator button, then select and confirm again.' }
 $d=Guard
 if($r.action -notin @('format','delete','create','layout','check','repair','windows_usb','windows_bios_usb','linux_usb')) { throw 'Unsupported action.' }
 $partition=$null
 if($r.action -in @('format','delete','check','repair')) {
  $partition=Get-Partition -DiskNumber $d.Number -PartitionNumber ([int]$r.partition)
  Assert-Partition $partition $r
 }
 if($r.action -in @('format','create','layout')) { Assert-FileSystem ([string]$r.filesystem) $(if($partition){$partition.Size}elseif($r.action -eq 'create'){[long]$r.sizeMiB*1MB}else{[long]$d.Size}) }
 if($r.action -eq 'format') { $null=Guard;$partition | Format-Volume -FileSystem $r.filesystem -NewFileSystemLabel 'NEXVARY' -Force -Confirm:$false | Out-Null }
 elseif($r.action -eq 'delete') { $null=Guard;$partition | Remove-Partition -Confirm:$false }
 elseif($r.action -eq 'create') {
  if([long]$r.sizeMiB -lt 16 -or [long]$r.sizeMiB*1MB -gt [long]$d.LargestFreeExtent) { throw 'Requested partition must fit unallocated space and be at least 16 MiB.' }
  if([string]$d.PartitionStyle -eq 'RAW') { throw 'Initialize a partition layout first.' }
  $null=Guard
  $p=New-Partition -DiskNumber $d.Number -Size ([long]$r.sizeMiB*1MB) -AssignDriveLetter
  $p | Format-Volume -FileSystem $r.filesystem -NewFileSystemLabel 'NEXVARY' -Force -Confirm:$false | Out-Null
 }
 elseif($r.action -eq 'layout') {
  if($r.style -notin @('GPT','MBR')) { throw 'Invalid partition style.' }
  if($r.style -eq 'MBR' -and $d.Size -gt 2TB) { throw 'MBR disks above 2 TiB are not supported.' }
  $null=Guard
  Clear-Disk -Number $d.Number -RemoveData -RemoveOEM -Confirm:$false
  Initialize-Disk -Number $d.Number -PartitionStyle $r.style | Out-Null
  New-Partition -DiskNumber $d.Number -UseMaximumSize -AssignDriveLetter | Format-Volume -FileSystem $r.filesystem -NewFileSystemLabel 'NEXVARY' -Force -Confirm:$false | Out-Null
 }
 elseif($r.action -in @('check','repair')) {
  if(!$partition.DriveLetter) { throw 'A mounted drive letter is required.' }
  $null=Guard
  if($r.action -eq 'check') { Repair-Volume -DriveLetter $partition.DriveLetter -Scan | Out-Null }
  else { Repair-Volume -DriveLetter $partition.DriveLetter -OfflineScanAndFix | Out-Null }
 }
 elseif($r.action -eq 'linux_usb') {
  Emit (Write-HybridImage $d ([string]$r.source));exit 0
 }
 elseif($r.action -in @('windows_usb','windows_bios_usb')) {
  $bios=$r.action -eq 'windows_bios_usb'
  if(!(Test-Path -LiteralPath $r.source -PathType Leaf) -or [IO.Path]::GetExtension($r.source) -ine '.iso') { throw 'Select an existing Windows ISO.' }
  $image=Mount-DiskImage -ImagePath $r.source -PassThru
  $splitFolder=$null
  try {
   $vol=$image | Get-Volume
   if(!$vol.DriveLetter) { throw 'Unable to mount ISO.' }
   $source="$($vol.DriveLetter):\"
   if(!(Test-Path -LiteralPath (Join-Path $source 'efi\boot\bootx64.efi')) -or !(Test-Path -LiteralPath (Join-Path $source 'sources\boot.wim'))) { throw 'Only Windows x64 UEFI installation ISOs are supported.' }
   if($bios -and (!(Test-Path -LiteralPath (Join-Path $source 'boot\bootsect.exe')) -or !(Test-Path -LiteralPath (Join-Path $source 'bootmgr')))) { throw 'ISO lacks the BIOS boot loader or bootsect utility.' }
   if($bios -and $d.Size -gt 2TB) { throw 'BIOS MBR media above 2 TiB is unsupported.' }
   $files=@(Get-ChildItem -LiteralPath $source -File -Recurse)
   $tooLarge=@($files | Where-Object {$_.Length -ge 4GB -and $_.FullName -ine (Join-Path $source 'sources\install.wim')})
   if($tooLarge.Count) { throw 'ISO contains a large non-WIM file incompatible with FAT32.' }
   $total=($files | Measure-Object Length -Sum).Sum
   $partSize=[long][Math]::Min(31GB,[long]$d.Size-256MB)
   if($partSize -lt $total+512MB) { throw 'Not enough USB space for this ISO plus reserve.' }
   # Prepare split WIM before any target mutation, then verify every copied part.
   $largeWim=@($files | Where-Object {$_.Length -ge 4GB})
   $splitFiles=@()
   if($largeWim.Count) {
    $splitFolder=Join-Path ([IO.Path]::GetTempPath()) ('NEXVARY-WIM-'+[guid]::NewGuid().ToString())
    $tempDrive=New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($splitFolder))
    if($tempDrive.AvailableFreeSpace -lt $largeWim[0].Length+512MB){throw 'Insufficient temporary space to stage and verify split WIM before disk changes.'}
    $null=New-Item -ItemType Directory -Path $splitFolder
    & "$env:SystemRoot\System32\dism.exe" /English /Split-Image "/ImageFile:$($largeWim[0].FullName)" "/SWMFile:$(Join-Path $splitFolder 'install.swm')" /FileSize:3800 /CheckIntegrity | Out-Null
    if($LASTEXITCODE -ne 0){throw 'WIM split failed before target changes.'}
    $splitFiles=@(Get-ChildItem -LiteralPath $splitFolder -Filter '*.swm' -File)
    if(!$splitFiles.Count -or @($splitFiles | Where-Object {$_.Length -le 0 -or $_.Length -ge 4GB}).Count){throw 'Invalid split WIM output.'}
   }
   $null=Guard
   Clear-Disk -Number $d.Number -RemoveData -RemoveOEM -Confirm:$false
   Initialize-Disk -Number $d.Number -PartitionStyle $(if($bios){'MBR'}else{'GPT'}) | Out-Null
   $p=New-Partition -DiskNumber $d.Number -Size $partSize -AssignDriveLetter
   $p | Format-Volume -FileSystem FAT32 -NewFileSystemLabel 'NEXVARYBOOT' -Force -Confirm:$false | Out-Null
   if($bios) { $p | Set-Partition -IsActive $true }
   $target="$($p.DriveLetter):\"
   $verified=0L
   foreach($f in $files) {
    $relative=$f.FullName.Substring($source.Length);$dest=Join-Path $target $relative
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest)) | Out-Null
    if($f.Length -ge 4GB) {
     foreach($part in $splitFiles) {
      $partDest=Join-Path $target ('sources\'+$part.Name)
      Copy-Item -LiteralPath $part.FullName -Destination $partDest
      if((Get-FileHash -LiteralPath $part.FullName -Algorithm SHA256).Hash -cne (Get-FileHash -LiteralPath $partDest -Algorithm SHA256).Hash){throw "Split WIM readback mismatch: $($part.Name)"}
      $verified+=$part.Length
     }
    } else {
     Copy-Item -LiteralPath $f.FullName -Destination $dest
     if((Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash -cne (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash) { throw "USB readback mismatch: $relative" }
     $verified+=$f.Length
    }
   }
   if($bios) {
    & (Join-Path $source 'boot\bootsect.exe') /nt60 "$($p.DriveLetter):" /mbr | Out-Null
    if($LASTEXITCODE -ne 0) { throw 'BIOS boot-code installation failed; media is incomplete.' }
   }
   Emit @{status='completed';operation=$r.action;bootMode=$(if($bios){'BIOS + UEFI (MBR)'}else{'UEFI (GPT)'});message='Windows x64 installation media created. Copied files passed SHA-256 readback; split WIM staged before erasure and each part SHA-256 verified. Boot and Secure Boot compatibility not proven.';verifiedBytes=$verified;splitWimReadbackVerified=($splitFiles.Count -gt 0);bootTested=$false;secureBootTested=$false;device=$r.device}
   exit 0
  } finally { if($splitFolder -and (Test-Path -LiteralPath $splitFolder)){Remove-Item -LiteralPath $splitFolder -Recurse -Force -ErrorAction SilentlyContinue}; Dismount-DiskImage -ImagePath $r.source -ErrorAction SilentlyContinue | Out-Null }
 }
 Emit @{status='completed';operation=$r.action;message='Storage command completed. Refresh disks to view the new layout.';device=$r.device;partition=$r.partition}
} catch { Emit @{status='error';operation=[string]$r.action;message=$_.Exception.Message;device=$r.device};exit 1 }
