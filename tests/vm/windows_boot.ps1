param([ValidateSet('bios','uefi','secureboot')][string]$Mode)
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -ne 'true'){throw 'This fixture is restricted to an ephemeral GitHub Actions runner'}
# Ephemeral virtual disk only. Never select a host physical disk.
$out=Join-Path $env:RUNNER_TEMP ('dc-windows-'+$Mode)
if(Test-Path $out){throw 'Refuse to replace VM evidence'}
New-Item -ItemType Directory $out | Out-Null
$iso=Join-Path $out 'evaluation.iso'
$url='https://software-static.download.prss.microsoft.com/dbazure/888969d5-f34g-4e03-ac9d-1f9786c66749/26100.1742.240906-0331.ge_release_svc_refresh_CLIENT_LTSC_EVAL_x64FRE_en-us.iso'
& curl.exe --fail --location --retry 3 --output $iso $url
if($LASTEXITCODE -ne 0){throw 'Official evaluation ISO download failed'}
# TLS download from Microsoft; pin additionally prevents a changed fixture.
$expected='67cec5865eaa037a72ddc633a717a10a2bed50778862267223ddb9c60ef5da68'
if((Get-FileHash $iso -Algorithm SHA256).Hash.ToLower() -ne $expected){throw 'Evaluation fixture hash mismatch'}
$vmDrive=Get-PSDrive -PSProvider FileSystem | Where-Object {$_.Free -gt 28GB -and $_.Root -match '^[A-Z]:\\$'} | Sort-Object Free -Descending | Select-Object -First 1
if(!$vmDrive){throw 'Windows VM needs an ephemeral volume with at least 28 GiB free'}
$vmRoot=Join-Path $vmDrive.Root ('dc-vm-'+$env:GITHUB_RUN_ID+'-'+$Mode)
New-Item -ItemType Directory $vmRoot | Out-Null
$vhd=Join-Path $vmRoot 'installed.vhd'
$usb=Join-Path $out 'prepared-usb.img'
$smallUSB=Join-Path $out 'small-usb.img'
foreach($fixture in @(@{path=$usb;bytes=16GB},@{path=$smallUSB;bytes=1GB})){
 [IO.File]::WriteAllBytes($fixture.path,[byte[]]@())
 & fsutil.exe sparse setflag $fixture.path | Out-Null
 if($LASTEXITCODE -ne 0){throw 'Cannot create sparse virtual USB'}
 $stream=[IO.File]::OpenWrite($fixture.path)
 try{$stream.SetLength($fixture.bytes)}finally{$stream.Dispose()}
}
$mounted=$false
function DiskPart([string[]]$commands){
 $file=Join-Path $out 'diskpart.txt';$commands | Set-Content $file -Encoding ascii
 $result=& diskpart.exe /s $file 2>&1
 $result | Add-Content (Join-Path $out 'diskpart.log')
 if($LASTEXITCODE -ne 0){throw 'DiskPart failed'}
}
if((Test-Path 'Y:\') -or (Test-Path 'Z:\')){throw 'Fixture drive letters occupied'}
$isoDisk=Mount-DiskImage -ImagePath $iso -PassThru
try {
 $isoVolume=$isoDisk | Get-Volume
 $wim=($isoVolume.DriveLetter+':\sources\install.wim')
 if(!(Test-Path $wim)){throw 'Evaluation WIM unavailable'}
 $partition=if($Mode -eq 'bios'){'mbr'}else{'gpt'}
 $commands=@("create vdisk file=`"$vhd`" maximum=65536 type=expandable","select vdisk file=`"$vhd`"",'attach vdisk',"convert $partition")
 if($Mode -eq 'bios'){
  $commands+=@('create partition primary size=512','format fs=ntfs quick label=DC_BOOT','assign letter=Y','active')
 }else{
  $commands+=@('create partition efi size=260','format fs=fat32 quick label=DC_ESP','assign letter=Y','create partition msr size=16')
 }
 $commands+=@('create partition primary','format fs=ntfs quick label=DC_WINDOWS','assign letter=Z')
 DiskPart $commands
 if(!(Test-Path 'Y:\') -or !(Test-Path 'Z:\')){throw 'Fixture partitions not mounted'}
 $mounted=$true
 & dism.exe /Apply-Image /ImageFile:$wim /Index:1 /ApplyDir:Z:\ /CheckIntegrity | Tee-Object (Join-Path $out 'dism.log')
 if($LASTEXITCODE -ne 0){throw 'Evaluation image apply failed'}
 $firmware=if($Mode -eq 'bios'){'BIOS'}else{'UEFI'}
 & bcdboot.exe Z:\Windows /s Y: /f $firmware | Tee-Object (Join-Path $out 'bcdboot.log')
 if($LASTEXITCODE -ne 0){throw 'Fixture BCD creation failed'}
 if($Mode -eq 'bios'){
  & ($isoVolume.DriveLetter+':\boot\bootsect.exe') /nt60 Y: /mbr
  if($LASTEXITCODE -ne 0){throw 'Fixture BIOS bootstrap failed'}
 }
 New-Item -ItemType Directory -Force 'Z:\Nexvary' | Out-Null
 Copy-Item scripts/storage.ps1,scripts/policy.ps1,scripts/hybrid.ps1 'Z:\Nexvary'
 Copy-Item tests/vm/windows_guest.ps1 'Z:\dc-guest.ps1'
 Copy-Item $iso 'Z:\source.iso'
 $Mode | Set-Content 'Z:\dc-mode.txt' -Encoding ascii
 # FirstLogonCommands are executed by the installed OS, not WinPE. Offline image
 # deployment tests Windows kernel/OOBE/autologon; it does NOT test DC USB creation.
 New-Item -ItemType Directory -Force 'Z:\Windows\Panther' | Out-Null
 @'
<?xml version="1.0" encoding="utf-8"?>
<unattend xmlns="urn:schemas-microsoft-com:unattend">
 <settings pass="specialize"><component name="Microsoft-Windows-Shell-Setup" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS"><ComputerName>DC-VM</ComputerName><TimeZone>UTC</TimeZone></component></settings>
 <settings pass="oobeSystem"><component name="Microsoft-Windows-International-Core" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS"><InputLocale>0409:00000409</InputLocale><SystemLocale>en-US</SystemLocale><UILanguage>en-US</UILanguage><UserLocale>en-US</UserLocale></component>
 <component name="Microsoft-Windows-Shell-Setup" processorArchitecture="amd64" publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS">
 <OOBE><HideEULAPage>true</HideEULAPage><HideOnlineAccountScreens>true</HideOnlineAccountScreens><HideWirelessSetupInOOBE>true</HideWirelessSetupInOOBE><ProtectYourPC>3</ProtectYourPC><SkipMachineOOBE>true</SkipMachineOOBE><SkipUserOOBE>true</SkipUserOOBE></OOBE>
 <UserAccounts><LocalAccounts><LocalAccount xmlns:wcm="http://schemas.microsoft.com/WMIConfig/2002/State" wcm:action="add"><Name>DCTest</Name><Group>Administrators</Group><Password><Value>DC_Disposable_VM_123!</Value><PlainText>true</PlainText></Password></LocalAccount></LocalAccounts></UserAccounts>
 <AutoLogon><Enabled>true</Enabled><Username>DCTest</Username><LogonCount>1</LogonCount><Password><Value>DC_Disposable_VM_123!</Value><PlainText>true</PlainText></Password></AutoLogon>
 <FirstLogonCommands><SynchronousCommand xmlns:wcm="http://schemas.microsoft.com/WMIConfig/2002/State" wcm:action="add"><Order>1</Order><CommandLine>cmd.exe /c C:\dc-proof.cmd</CommandLine><Description>Installed Windows VM proof</Description></SynchronousCommand></FirstLogonCommands>
 </component></settings>
</unattend>
'@ | Set-Content 'Z:\Windows\Panther\unattend.xml' -Encoding utf8
 @'
@echo off
mode COM1: baud=115200 parity=n data=8 stop=1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\dc-guest.ps1
'@ | Set-Content 'Z:\dc-proof.cmd' -Encoding ascii
}finally{
 if($mounted){DiskPart @("select vdisk file=`"$vhd`"",'detach vdisk')}
 Dismount-DiskImage -ImagePath $iso
}
$qemu='C:\Program Files\qemu\qemu-system-x86_64.exe'
if(!(Test-Path $qemu)){throw 'QEMU missing'}
$serial=Join-Path $out 'serial.txt'
$arguments=@('-machine','q35,smm=on','-accel','tcg','-cpu','max','-m','4096','-smp','2','-display','none','-serial',"file:$serial",'-nic','none','-drive',"file=$vhd,format=vpc,if=ide",'-rtc','base=utc','-device','qemu-xhci','-drive',"file=$usb,format=raw,if=none,id=dcusb",'-device','usb-storage,drive=dcusb,serial=DC_BOOT_TARGET,removable=on','-drive',"file=$smallUSB,format=raw,if=none,id=dcsmall",'-device','usb-storage,drive=dcsmall,serial=DC_SMALL_TARGET,removable=on')
if($Mode -ne 'bios'){
 $code=(Resolve-Path 'out/firmware/OVMF_CODE_4M.secboot.fd').Path
 $vars=Join-Path $out 'vars.fd'
 $template=if($Mode -eq 'secureboot'){'out/firmware/OVMF_VARS_4M.ms.fd'}else{'out/firmware/OVMF_VARS_4M.fd'}
 Copy-Item $template $vars
 $arguments+=@('-global','driver=cfi.pflash01,property=secure,value=on','-drive',"if=pflash,format=raw,readonly=on,file=$code",'-drive',"if=pflash,format=raw,file=$vars")
}
$p=Start-Process $qemu -ArgumentList $arguments -PassThru -RedirectStandardError (Join-Path $out 'qemu.log')
try {
 $deadline=(Get-Date).AddMinutes(45)
 while(!$p.HasExited -and (Get-Date) -lt $deadline){Start-Sleep -Seconds 5;$p.Refresh()}
 if(!$p.HasExited){$p.Kill();throw 'Installed Windows VM boot timeout'}
 $text=Get-Content $serial -Raw
 if($text -notmatch 'DC_INSTALLED_WINDOWS_BOOT_OK'){throw 'No installed Windows first-logon proof'}
 foreach($marker in @('DC_SYSTEM_DISK_REFUSED','DC_USB_IDENTITY_REFUSED','DC_USB_SPACE_REFUSED','DC_USB_PREPARED=','DC_USB_TEST_HOOK_READY')){if($text -notmatch $marker){throw "Missing production USB fixture proof: $marker"}}
 if($Mode -eq 'secureboot' -and $text -notmatch 'DC_SECURE_BOOT_ON'){throw 'Guest did not confirm enabled Secure Boot'}
 if($Mode -ne 'secureboot' -and $text -notmatch 'DC_SECURE_BOOT_OFF'){throw 'Unexpected guest Secure Boot state'}
 $hashBefore=(Get-FileHash $usb -Algorithm SHA256).Hash
 $peSerial=Join-Path $out 'usb-serial.txt'
 $peArgs=@('-machine','q35,smm=on','-accel','tcg','-cpu','max','-m','4096','-smp','2','-display','none','-serial',"file:$peSerial",'-nic','none','-boot','order=c','-device','qemu-xhci','-drive',"file=$usb,format=raw,if=none,id=dcusb,snapshot=on",'-device','usb-storage,drive=dcusb,serial=DC_BOOT_TARGET,removable=on')
 if($Mode -ne 'bios'){
  Copy-Item $template $vars -Force
  $peArgs+=@('-global','driver=cfi.pflash01,property=secure,value=on','-drive',"if=pflash,format=raw,readonly=on,file=$code",'-drive',"if=pflash,format=raw,file=$vars")
 }
 $p=Start-Process $qemu -ArgumentList $peArgs -PassThru -RedirectStandardError (Join-Path $out 'usb-qemu.log')
 $deadline=(Get-Date).AddMinutes(20)
 while(!$p.HasExited -and (Get-Date) -lt $deadline){Start-Sleep -Seconds 5;$p.Refresh()}
 if(!$p.HasExited){$p.Kill();throw 'Prepared virtual USB boot timeout'}
 $pe=Get-Content $peSerial -Raw
 if($pe -notmatch 'DC_WINDOWS_USB_WINPE_OK'){throw 'Prepared production USB did not reach Windows PE'}
 if($Mode -eq 'secureboot' -and $pe -notmatch 'DC_USB_SECURE_BOOT_ON'){throw 'Prepared USB guest did not confirm Secure Boot'}
 if($Mode -ne 'secureboot' -and $pe -notmatch 'DC_USB_SECURE_BOOT_OFF'){throw 'Unexpected prepared USB Secure Boot state'}
 if((Get-FileHash $usb -Algorithm SHA256).Hash -ne $hashBefore){throw 'USB boot modified protected reference image'}
 @{mode=$Mode;installedWindowsBoot=$true;secureBoot=($Mode -eq 'secureboot');evaluationISO_SHA256=$expected;dcUSBPreparationTested=$true;usbWindowsPEBoot=$true;systemDiskRefused=$true;changedIdentityRefused=$true;insufficientSpaceRefused=$true;preparedUSB_SHA256=$hashBefore.ToLower();testHook='WinPE batch shell; signed EFI loader unchanged'} | ConvertTo-Json | Set-Content (Join-Path $out 'result.json')
}finally{
 if(!$p.HasExited){$p.Kill()}
 New-Item -ItemType Directory -Force "out/windows-vm-$Mode" | Out-Null
 Copy-Item "$out/*.log","$out/*.txt","$out/*.json" "out/windows-vm-$Mode" -ErrorAction SilentlyContinue
 # Do not redistribute Microsoft's evaluation ISO or Windows virtual disk.
 Remove-Item $vhd,$iso,$usb,$smallUSB -Force -ErrorAction SilentlyContinue
}
