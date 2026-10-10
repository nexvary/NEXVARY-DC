$ErrorActionPreference='Stop'
. "$PSScriptRoot/../../scripts/windows_recovery.ps1"
function Assert-Rejected([scriptblock]$Action) {
 $rejected=$false
 try{& $Action | Out-Null}catch{$rejected=$true}
 if(!$rejected){throw 'Unsafe recovery request accepted.'}
}
if((Get-RecoveryRoot 'd:') -cne 'D:\'){throw 'Root normalization failed.'}
foreach($bad in @('D:\Windows','\\server\share','D:relative','D:\..\','D:\;format C:','')){Assert-Rejected {Get-RecoveryRoot $bad}}
$w=@{root='D:\';diskId='target';protected=$false;filesystem='NTFS'}
$b=@{root='S:\';diskId='target';protected=$false;filesystem='FAT32';style='GPT';gpt='{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}';active=$false}
$dest=@{root='E:\';diskId='backup';free=1GB}
Assert-RecoveryLayout $w $b $dest 'UEFI'
$w.protected=$true;Assert-Rejected {Assert-RecoveryLayout $w $b $dest 'UEFI'};$w.protected=$false
$b.protected=$true;Assert-Rejected {Assert-RecoveryLayout $w $b $dest 'UEFI'};$b.protected=$false
$b.diskId='other';Assert-Rejected {Assert-RecoveryLayout $w $b $dest 'UEFI'};$b.diskId='target'
$dest.diskId='target';Assert-Rejected {Assert-RecoveryLayout $w $b $dest 'UEFI'};$dest.diskId='backup'
$dest.free=1MB;Assert-Rejected {Assert-RecoveryLayout $w $b $dest 'UEFI'};$dest.free=1GB
$b.gpt='basic-data';Assert-Rejected {Assert-RecoveryLayout $w $b $dest 'UEFI'};$b.gpt='{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}'
Assert-Rejected {Assert-RecoveryLayout $w $b $dest 'ALL'}
Assert-Rejected {Assert-RecoveryLayout $w $b $dest 'BIOS'}
$b.style='MBR';$b.filesystem='NTFS';$b.active=$true
Assert-RecoveryLayout $w $b $dest 'BIOS'
$b.active=$false;Assert-Rejected {Assert-RecoveryLayout $w $b $dest 'BIOS'}
$s=[ordered]@{windows=$w;boot=$b;backup=$dest;mode='UEFI';task='bcdboot'}
$c=Get-RecoveryCommand $s 'E:\logs'
if($c.exe -ne 'bcdboot.exe' -or ($c.args -join '|') -cne 'D:\Windows|/s|S:\|/f|UEFI|/v'){throw 'Wrong offline BCDBoot arguments.'}
foreach($task in @('sfc_verify','sfc_repair')) {
 $s.task=$task;$c=Get-RecoveryCommand $s 'E:\logs'
 if($c.exe -ne 'sfc.exe' -or $c.args -notcontains '/offwindir=D:\Windows' -or $c.args -notcontains '/offbootdir=S:\'){throw 'Wrong offline SFC arguments.'}
}
$s.task='format';Assert-Rejected {Get-RecoveryCommand $s 'E:\logs'}
$hash=Get-RecoveryHash $s;$w.diskId='changed'
if((Get-RecoveryHash $s) -ceq $hash){throw 'Changed disk accepted.'}
$originalSnapshot=${function:Get-RecoverySnapshot}
# Exercise the same apply guard used by the production entry point.
$script:fixture=$s
function Get-RecoverySnapshot($Request){return $script:fixture}
$s.task='bcdboot';$w.diskId='target'
$apply=@{plan=@{mode='windows_offline';created=[DateTimeOffset]::UtcNow.ToUnixTimeSeconds();snapshotHash=(Get-RecoveryHash $s);request=@{}};confirmation='REPAIR D:\ bcdboot';dataPreserved=$true}
$null=Assert-RecoveryApply $apply
$apply.dataPreserved=$false;Assert-Rejected {Assert-RecoveryApply $apply};$apply.dataPreserved=$true
$apply.confirmation='wrong';Assert-Rejected {Assert-RecoveryApply $apply};$apply.confirmation='REPAIR D:\ bcdboot'
$apply.plan.created-=301;Assert-Rejected {Assert-RecoveryApply $apply};$apply.plan.created+=301
$w.diskId='changed';Assert-Rejected {Assert-RecoveryApply $apply};$w.diskId='target'
$s.bootState=@('changed-boot-file');Assert-Rejected {Assert-RecoveryApply $apply};$s.Remove('bootState')
# Backup/readback and failure-before-tool-execution on ordinary temp fixtures.
$temp=Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString('N'))
try {
 New-Item -ItemType Directory -Path "$temp\source\Boot","$temp\backup" -Force | Out-Null
 Set-Content -LiteralPath "$temp\source\Boot\BCD" -Value 'abc' -NoNewline -Encoding Ascii
 if((Get-RecoveryFileHash "$temp\source\Boot\BCD") -cne 'BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD'){throw 'SHA-256 reference vector failed.'}
 $s.mode='BIOS';$s.boot.root="$temp\source\";$s.backup.root="$temp\backup\"
 $saved=Backup-RecoveryFiles $s
 if((Get-RecoveryFileHash "$saved\Boot\BCD") -cne (Get-RecoveryFileHash "$temp\source\Boot\BCD")){throw 'Backup mismatch.'}
 # Execute the real snapshot function with a filesystem fixture and mocked OS inventory.
 New-Item -ItemType Directory -Path "$temp\windows\Windows\System32\Config" -Force | Out-Null
 foreach($f in @('SYSTEM','BCD-Template')){Set-Content -LiteralPath "$temp\windows\Windows\System32\Config\$f" -Value 'fixture'}
 Set-Content -LiteralPath "$temp\windows\Windows\System32\ntoskrnl.exe" -Value 'fixture'
 $script:volumes=@{}
 $script:volumes.w=[ordered]@{root="$temp\windows\";diskId='target';protected=$false;filesystem='NTFS';free=1GB}
 $script:volumes.b=[ordered]@{root="$temp\source\";diskId='target';protected=$false;filesystem='NTFS';style='MBR';gpt='';active=$true;free=1GB}
 $script:volumes.e=[ordered]@{root="$temp\backup\";diskId='backup';free=1GB}
 function Get-RecoveryVolume($Path){return ([ordered]@{} + $script:volumes[$Path])}
 function Get-BitLockerVolume {return @{LockStatus='Unlocked'}}
 $fixtureRequest=@{windowsRoot='w';bootRoot='b';backupRoot='e';mode='BIOS';task='bcdboot'}
 $actual=& $originalSnapshot $fixtureRequest
 if(@($actual.bootState).Count -ne 1 -or !$actual.templateHash){throw 'Actual snapshot incomplete.'}
 $originalHash=Get-RecoveryHash $actual
 Set-Content -LiteralPath "$temp\source\Boot\BCD" -Value 'changed-store'
 if((Get-RecoveryHash (& $originalSnapshot $fixtureRequest)) -ceq $originalHash){throw 'Boot change did not invalidate snapshot.'}
 function Get-BitLockerVolume {return @{LockStatus='Locked'}}
 Assert-Rejected {& $originalSnapshot $fixtureRequest}
 $manifest=Get-Content "$saved\backup-manifest.json" -Raw | ConvertFrom-Json
 if(@($manifest.files).Count -ne 1 -or $manifest.automaticRollback){throw 'Incorrect backup report.'}
} finally {Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue}
Write-Output 'Offline Windows recovery policy, argument and backup fixtures passed. No physical disk writes or Microsoft repair tools executed.'
