$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$serial=[IO.Ports.SerialPort]::new('COM1',115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
$serial.Open()
function Proof([string]$text){
 $serial.WriteLine($text);$serial.BaseStream.Flush()
}
function Request($disk,[string]$action) {
 return @{number=[int]$disk.Number;device="\\.\PHYSICALDRIVE$($disk.Number)";uniqueId=[string]$disk.UniqueId;serial=[string]$disk.SerialNumber;bytes=[long]$disk.Size;action=$action;source='C:\source.iso';appDirectory='C:\Nexvary';partition=0;offset=0;partitionBytes=0;filesystem='FAT32';style='MBR'}
}
function Storage($request) {
 $base=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes(($request | ConvertTo-Json -Compress)))
 $lines=& powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Nexvary\storage.ps1 -RequestBase64 $base
 $code=$LASTEXITCODE
 $row=($lines -join "`n") | ConvertFrom-Json
 return @{code=$code;row=$row}
}
function DiskProof($disk) {
 # Windows can report a blank removable QEMU disk as MBR before our helper runs.
 # Compare actual metadata and bytes against that baseline, rather than RAW.
 $current=Get-Disk -Number $disk.Number
 $parts=@(Get-Partition -DiskNumber $disk.Number -ErrorAction SilentlyContinue | Select-Object PartitionNumber,Offset,Size,Type)
 $stream=[IO.FileStream]::new("\\.\PHYSICALDRIVE$($disk.Number)",[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
 $hash=[Security.Cryptography.SHA256]::Create()
 try {
  $proof=@()
  foreach($offset in @([long]0,[long]($current.Size-1MB))) {
   [void]$stream.Seek($offset,[IO.SeekOrigin]::Begin)
   $buffer=[byte[]]::new(1MB);$done=0
   while($done -lt $buffer.Length){$read=$stream.Read($buffer,$done,$buffer.Length-$done);if(!$read){throw 'Short guard-proof read'};$done+=$read}
   $proof+=[Convert]::ToBase64String($hash.ComputeHash($buffer))
  }
  return ([ordered]@{style=[string]$current.PartitionStyle;size=[long]$current.Size;parts=$parts;hashes=$proof} | ConvertTo-Json -Compress -Depth 5)
 }finally{$stream.Dispose();$hash.Dispose()}
}
try {
 Proof 'DC_INSTALLED_WINDOWS_BOOT_OK'
 $secure=(Get-ItemProperty HKLM:\SYSTEM\CurrentControlSet\Control\SecureBoot\State -ErrorAction SilentlyContinue).UEFISecureBootEnabled
 Proof $(if($secure -eq 1){'DC_SECURE_BOOT_ON'}else{'DC_SECURE_BOOT_OFF'})
 $sourceHash=(Get-FileHash C:\source.iso -Algorithm SHA256).Hash.ToLowerInvariant()
 if($sourceHash -ne '67cec5865eaa037a72ddc633a717a10a2bed50778862267223ddb9c60ef5da68'){throw 'Original evaluation ISO changed in transit'}
 Proof ('DC_ORIGINAL_ISO_SHA256='+$sourceHash)
 $mode=(Get-Content C:\dc-mode.txt -Raw).Trim()
 Proof 'DC_DISK_ENUMERATION_BEGIN'
 $disks=@(Get-Disk)
 Proof ('DC_DISK_ENUMERATION='+($disks | Select-Object Number,BusType,Size,PartitionStyle,UniqueId,SerialNumber,IsBoot,IsSystem | ConvertTo-Json -Compress))
 $system=@($disks | Where-Object {$_.IsBoot -or $_.IsSystem})[0]
 $usb=@($disks | Where-Object {[string]$_.BusType -eq 'USB' -and [long]$_.Size -eq 16GB})
 $small=@($disks | Where-Object {[string]$_.BusType -eq 'USB' -and [long]$_.Size -eq 1GB})
 if($usb.Count -ne 1 -or $small.Count -ne 1){throw 'Expected two isolated emulated USB targets'}
 $action=if($mode -eq 'bios'){'windows_bios_usb'}else{'windows_usb'}
 Proof 'DC_SYSTEM_GUARD_BEGIN'
 $guard=Storage (Request $system 'layout')
 if($guard.code -eq 0 -or $guard.row.status -ne 'error'){throw 'Production system-disk guard did not reject'}
 Proof 'DC_SYSTEM_DISK_REFUSED'
 $usbBefore=DiskProof $usb[0]
 $smallBefore=DiskProof $small[0]
 $request=Request $usb[0] $action
 $request.serial='CHANGED_IDENTITY'
 $guard=Storage $request
 if($guard.code -eq 0 -or $guard.row.status -ne 'error'){throw 'Changed identity was accepted'}
 if((DiskProof $usb[0]) -ne $usbBefore){throw 'Identity rejection modified target'}
 Proof 'DC_USB_IDENTITY_REFUSED'
 $guard=Storage (Request $small[0] $action)
 if($guard.code -eq 0 -or $guard.row.status -ne 'error' -or $guard.row.message -notmatch 'space'){throw 'Insufficient destination space was not rejected'}
 if((DiskProof $small[0]) -ne $smallBefore){throw 'Space rejection modified target'}
 Proof 'DC_USB_SPACE_REFUSED'
 Proof 'DC_USB_PREPARATION_BEGIN'
 $prepared=Storage (Request $usb[0] $action)
 if($prepared.code -ne 0 -or $prepared.row.status -ne 'completed' -or $prepared.row.verifiedBytes -le 0){throw ('Production USB preparation failed: '+($prepared | ConvertTo-Json -Depth 10))}
 Proof ('DC_USB_PREPARED='+($prepared.row | ConvertTo-Json -Compress -Depth 10))
 $partition=@(Get-Partition -DiskNumber $usb[0].Number | Where-Object {$_.DriveLetter})[0]
 $root="$($partition.DriveLetter):\"
 $efi=Join-Path $root 'efi\boot\bootx64.efi'
 $efiBefore=(Get-FileHash $efi -Algorithm SHA256).Hash
 # Test-only PE shell hook gives serial proof after the signed Windows kernel
 # actually boots. EFI loaders and executable Windows PE components stay intact.
 $mount='C:\dc-pe-mount'
 New-Item -ItemType Directory $mount | Out-Null
 $usbWim=Join-Path $root 'sources\boot.wim'
 # Service a writable NTFS copy: ISO files retain read-only attributes and
 # DISM read/write mounts must not depend on the target FAT/removable volume.
 $wim='C:\dc-usb-boot.wim'
 Copy-Item -LiteralPath $usbWim -Destination $wim
 (Get-Item -LiteralPath $wim).IsReadOnly=$false
 $dismOutput=& dism.exe /Mount-Image /ImageFile:$wim /Index:2 /MountDir:$mount /LogPath:C:\dc-usb-dism.log 2>&1
 $mountCode=$LASTEXITCODE
 foreach($line in $dismOutput){Proof ('DC_DISM_MOUNT='+[string]$line)}
 if($mountCode -ne 0){
  foreach($line in (Get-Content C:\dc-usb-dism.log -Tail 30 -ErrorAction SilentlyContinue)){Proof ('DC_DISM_LOG='+$line)}
  throw ('WinPE proof hook mount failed: '+$mountCode)
 }
 try {
 @'
@echo off
wpeinit
wpeutil UpdateBootInfo
for %%d in (C D E F G H I J K L M N O P Q R S T U V W Y Z) do if exist %%d:\DCPROOF.MRK set PROOF=%%d:
if not defined PROOF goto finish
set LOG=%PROOF%\usb-proof.txt
echo DC_WINDOWS_USB_WINPE_OK > "%LOG%"
reg query HKLM\SYSTEM\CurrentControlSet\Control /v PEFirmwareType >> "%LOG%"
reg query HKLM\SYSTEM\CurrentControlSet\Control\SecureBoot\State /v UEFISecureBootEnabled >> "%LOG%"
reg query HKLM\SYSTEM\CurrentControlSet\Control\SecureBoot\State /v UEFISecureBootEnabled | find "0x1" >nul
if errorlevel 1 (echo DC_USB_SECURE_BOOT_OFF >> "%LOG%") else (echo DC_USB_SECURE_BOOT_ON >> "%LOG%")
:finish
wpeutil shutdown
'@ | Set-Content (Join-Path $mount 'dc-pe-proof.cmd') -Encoding ascii
 @'
[LaunchApps]
%SYSTEMROOT%\System32\cmd.exe, /c X:\dc-pe-proof.cmd
'@ | Set-Content (Join-Path $mount 'Windows\System32\winpeshl.ini') -Encoding ascii
 $dismOutput=& dism.exe /Unmount-Image /MountDir:$mount /Commit /LogPath:C:\dc-usb-dism.log 2>&1
 $commitCode=$LASTEXITCODE
 foreach($line in $dismOutput){Proof ('DC_DISM_COMMIT='+[string]$line)}
 if($commitCode -ne 0){throw ('WinPE proof hook commit failed: '+$commitCode)}
 (Get-Item -LiteralPath $usbWim).IsReadOnly=$false
 Copy-Item -LiteralPath $wim -Destination $usbWim -Force
 if((Get-FileHash $wim -Algorithm SHA256).Hash -ne (Get-FileHash $usbWim -Algorithm SHA256).Hash){throw 'Test hook WIM copy readback mismatch'}
 }catch{
 & dism.exe /Unmount-Image /MountDir:$mount /Discard | Out-Null
 throw
 }
 if((Get-FileHash $efi -Algorithm SHA256).Hash -ne $efiBefore){throw 'Signed EFI loader changed'}
 Proof 'DC_USB_TEST_HOOK_READY'
}catch{
 Proof ('DC_WINDOWS_VM_ERROR='+$_.Exception.Message)
}finally{
 $serial.Dispose()
 shutdown.exe /s /t 5
}
