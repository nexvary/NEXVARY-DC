param([string]$RequestBase64)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

function Get-RecoveryRoot([string]$Path) {
 if($Path -notmatch '^[A-Za-z]:\\?$'){throw 'Use a mounted local drive root, for example D:\. Network, relative and directory paths are refused.'}
 return $Path.Substring(0,1).ToUpperInvariant()+':\'
}
function Get-RecoveryVolume([string]$Path) {
 $root=Get-RecoveryRoot $Path
 $part=@(Get-Partition -DriveLetter $root[0] -ErrorAction Stop)
 if($part.Count -ne 1){throw 'Ambiguous partition.'}
 $disk=Get-Disk -Number $part[0].DiskNumber -ErrorAction Stop
 $vol=Get-Volume -DriveLetter $root[0] -ErrorAction Stop
 if($disk.IsOffline -or $disk.IsReadOnly -or $part[0].IsReadOnly -or $vol.HealthStatus -ne 'Healthy'){throw 'Offline, read-only or unhealthy target. Rescue to healthy storage first.'}
 if([string]::IsNullOrWhiteSpace([string]$disk.UniqueId)){throw 'No stable disk identity.'}
 return [ordered]@{root=$root;diskId=[string]$disk.UniqueId;serial=[string]$disk.SerialNumber;diskBytes=[long]$disk.Size;diskNumber=[int]$disk.Number;style=[string]$disk.PartitionStyle;partition=[int]$part[0].PartitionNumber;offset=[long]$part[0].Offset;bytes=[long]$part[0].Size;volumeId=[string]$vol.UniqueId;filesystem=[string]$vol.FileSystem;gpt=[string]$part[0].GptType;active=[bool]$part[0].IsActive;protected=[bool]($disk.IsBoot -or $disk.IsSystem -or $part[0].IsBoot -or $part[0].IsSystem);free=[long]$vol.SizeRemaining}
}
function Assert-RecoveryLayout($Windows,$Boot,$Backup,[string]$Mode) {
 if($Mode -notin @('UEFI','BIOS')){throw 'Choose UEFI or BIOS explicitly.'}
 if($Windows.protected -or $Boot.protected){throw 'Running system or boot disk is protected. Use another computer or compatible recovery environment.'}
 if($Windows.diskId -cne $Boot.diskId){throw 'Windows and boot partitions must be on the same target disk.'}
 if($Backup.diskId -ceq $Windows.diskId){throw 'Backup must be on a different physical disk.'}
 if($Windows.filesystem -ne 'NTFS'){throw 'Windows target must be unlocked NTFS.'}
 if($Mode -eq 'UEFI' -and ($Boot.style -ne 'GPT' -or $Boot.filesystem -ne 'FAT32' -or $Boot.gpt.Trim('{}') -ine 'c12a7328-f81f-11d2-ba4b-00a0c93ec93b')){throw 'UEFI requires a GPT EFI System Partition formatted FAT32.'}
 if($Mode -eq 'BIOS' -and ($Boot.style -ne 'MBR' -or !$Boot.active -or $Boot.filesystem -ne 'NTFS')){throw 'BIOS requires an existing active NTFS partition on MBR. This tool does not change active flags or boot sectors.'}
 if($Backup.free -lt 512MB){throw 'Backup destination requires at least 512 MiB free.'}
}
function Assert-RecoveryPath([string]$Path,[string]$Root) {
 $current=$Path
 while($current.Length -gt $Root.Length) {
  if((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Redirected recovery path refused.'}
  $current=Split-Path -Parent $current
 }
}
function Get-RecoveryBootState($Snapshot) {
 $roots=if($Snapshot.mode -eq 'UEFI'){@('EFI\Microsoft\Boot','EFI\Boot')}else{@('Boot','bootmgr')}
 $state=@()
 foreach($relative in $roots) {
  $source=Join-Path $Snapshot.boot.root $relative
  if(!(Test-Path -LiteralPath $source)){continue}
  Assert-RecoveryPath $source $Snapshot.boot.root
  $item=Get-Item -LiteralPath $source -Force
  $items=if($item.PSIsContainer){@(Get-ChildItem -LiteralPath $source -Recurse -Force -ErrorAction Stop)}else{@($item)}
  foreach($entry in ($items | Sort-Object FullName)) {
   if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Redirected boot file refused.'}
   if(!$entry.PSIsContainer){$state+=@{path=$entry.FullName.Substring($Snapshot.boot.root.Length);hash=(Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash}}
  }
 }
 return ,$state
}
function Get-RecoverySnapshot($Request) {
 $windows=Get-RecoveryVolume ([string]$Request.windowsRoot)
 $boot=Get-RecoveryVolume ([string]$Request.bootRoot)
 $backup=Get-RecoveryVolume ([string]$Request.backupRoot)
 Assert-RecoveryLayout $windows $boot $backup ([string]$Request.mode)
 $locked=@(Get-BitLockerVolume -MountPoint $windows.root -ErrorAction Stop)
 if($locked.Count -ne 1 -or [string]$locked[0].LockStatus -ne 'Unlocked'){throw 'Windows BitLocker state is unavailable or locked. Unlock it outside this tool first.'}
 $win=Join-Path $windows.root 'Windows'
 if($win -ieq $env:SystemRoot){throw 'The running Windows installation is protected.'}
 foreach($file in @('System32\Config\SYSTEM','System32\Config\BCD-Template','System32\ntoskrnl.exe')) {
  $path=Join-Path $win $file
  if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing offline Windows evidence: $file"}
  Assert-RecoveryPath $path $windows.root
  if((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse points are not supported.'}
 }
 if((Get-Item -LiteralPath $win).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Windows directory is redirected.'}
 $hash=Get-FileHash -LiteralPath (Join-Path $win 'System32\Config\BCD-Template') -Algorithm SHA256
 # Free space may change naturally. It is checked independently, not part of identity.
 $windows.Remove('free');$boot.Remove('free');$backup.Remove('free')
 $snapshot=[ordered]@{windows=$windows;boot=$boot;backup=$backup;templateHash=$hash.Hash;mode=[string]$Request.mode;task=[string]$Request.task}
 $snapshot.bootState=Get-RecoveryBootState $snapshot
 return $snapshot
}
function Get-RecoveryHash($Value) {
 $sha=[Security.Cryptography.SHA256]::Create()
 try{return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes(($Value | ConvertTo-Json -Depth 12 -Compress))))).Replace('-','').ToLowerInvariant()}
 finally{$sha.Dispose()}
}
function Get-RecoveryCommand($Snapshot,[string]$LogDirectory) {
 $win=[string]$Snapshot.windows.root+'Windows'
 switch($Snapshot.task) {
  'bcdboot' {return @{exe='bcdboot.exe';args=@($win,'/s',$Snapshot.boot.root,'/f',$Snapshot.mode,'/v')}}
  'sfc_verify' {return @{exe='sfc.exe';args=@('/verifyonly',('/offbootdir='+$Snapshot.boot.root),('/offwindir='+$win),('/offlogfile='+($LogDirectory.TrimEnd('\')+'\sfc.log')))}}
  'sfc_repair' {return @{exe='sfc.exe';args=@('/scannow',('/offbootdir='+$Snapshot.boot.root),('/offwindir='+$win),('/offlogfile='+($LogDirectory.TrimEnd('\')+'\sfc.log')))}}
  default {throw 'Unsupported recovery operation.'}
 }
}
function Backup-RecoveryFiles($Snapshot) {
 $dir=Join-Path $Snapshot.backup.root ('NEXVARY-Boot-'+[guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory -Path $dir -ErrorAction Stop | Out-Null
 $files=@()
 try {
  $roots=if($Snapshot.mode -eq 'UEFI'){@('EFI\Microsoft\Boot','EFI\Boot')}else{@('Boot','bootmgr')}
  foreach($relative in $roots) {
   $source=Join-Path $Snapshot.boot.root $relative
   if(!(Test-Path -LiteralPath $source)){continue}
   $item=Get-Item -LiteralPath $source -Force
   Assert-RecoveryPath $source $Snapshot.boot.root
   $items=if($item.PSIsContainer){@($item)+@(Get-ChildItem -LiteralPath $source -Recurse -Force -ErrorAction Stop)}else{@($item)}
   foreach($entry in $items) {
    if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Redirected boot files refused.'}
    if($entry.PSIsContainer){continue}
    $rel=$entry.FullName.Substring($Snapshot.boot.root.Length)
    $dest=Join-Path $dir $rel
    New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
    $before=(Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $entry.FullName -Destination $dest -Force -ErrorAction Stop
    if((Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash -cne $before){throw 'Backup readback mismatch.'}
    $files+=@{path=$rel;sha256=$before;bytes=$entry.Length}
   }
  }
  @{snapshot=$Snapshot;files=$files;created=[DateTimeOffset]::UtcNow.ToString('o');automaticRollback=$false} | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $dir 'backup-manifest.json') -Encoding UTF8
  return $dir
 } catch {throw "Backup failed before repair; retained $dir. $($_.Exception.Message)"}
}
function Invoke-RecoveryTool($Command,[string]$Directory) {
 $exe=Join-Path $env:SystemRoot ('System32\'+$Command.exe)
 if(!(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Required Microsoft recovery tool unavailable.'}
 # Argument arrays bypass shell command parsing. Never terminate a mutation halfway through.
 $arguments=[string[]]$Command.args
 $output=& $exe @arguments 2>&1
 $code=$LASTEXITCODE
 $output | Out-File -LiteralPath (Join-Path $Directory 'tool-output.txt') -Encoding UTF8
 return @{exitCode=$code;output=($output -join "`n")}
}
function Assert-RecoveryApply($Request) {
 $plan=$Request.plan
 $age=[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()-[long]$plan.created
 if($plan.mode -ne 'windows_offline' -or $age -lt 0 -or $age -gt 300){throw 'Repair plan expired.'}
 $snapshot=Get-RecoverySnapshot $plan.request
 if((Get-RecoveryHash $snapshot) -cne $plan.snapshotHash){throw 'Target identity or boot files changed; inspect again.'}
 if($Request.confirmation -cne ('REPAIR '+$snapshot.windows.root+' '+$snapshot.task) -or $Request.dataPreserved -ne $true){throw 'Exact confirmation and data-preservation acknowledgement required.'}
 $null=Get-RecoveryCommand $snapshot '<backup-directory>'
 return $snapshot
}
if(!$RequestBase64){return}
$backupDirectory=$null
try {
 $request=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($RequestBase64)) | ConvertFrom-Json
 $admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
 if(!$admin){throw 'Administrator permission required.'}
 if($request.action -eq 'plan') {
  $snapshot=Get-RecoverySnapshot $request
  $command=Get-RecoveryCommand $snapshot '<backup-directory>'
  $plan=@{mode='windows_offline';request=$request;snapshotHash=(Get-RecoveryHash $snapshot);created=[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()}
  $confirmation='REPAIR '+$snapshot.windows.root+' '+$snapshot.task
  @{status='completed';operation='windows_recovery_plan';plan=$plan;target=$snapshot;command=$command;confirmation=$confirmation;message='Review target and preserve user data first. Boot-file backup is not a full disk backup. BCDBoot /s does not add a UEFI NVRAM entry. BIOS boot sectors are not repaired.'} | ConvertTo-Json -Depth 14 -Compress
 } elseif($request.action -eq 'apply') {
  $snapshot=Assert-RecoveryApply $request
  $backupDirectory=Backup-RecoveryFiles $snapshot
  if((Get-RecoveryHash (Get-RecoverySnapshot $plan.request)) -cne $plan.snapshotHash){throw 'Target changed during backup.'}
  $result=Invoke-RecoveryTool (Get-RecoveryCommand $snapshot $backupDirectory) $backupDirectory
  $status=if($result.exitCode -eq 0){'completed'}else{'error'}
  # An exit code is tool execution evidence, never proof that the computer boots.
  @{status=$status;operation='windows_recovery';backup=$backupDirectory;engine=$snapshot.task;engineExitCode=$result.exitCode;message='Read tool-output.txt and sfc.log; boot and repair outcomes require separate verification. No automatic rollback.';rebootVerified=$false} | ConvertTo-Json -Depth 8 -Compress
 } else {throw 'Unknown action.'}
} catch {
 @{status='error';operation='windows_recovery';message=$_.Exception.Message;backup=$backupDirectory} | ConvertTo-Json -Depth 8 -Compress
 exit 1
}
