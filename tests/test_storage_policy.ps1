$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../scripts/policy.ps1')
$d=[pscustomobject]@{Number=2;UniqueId='USB-42';SerialNumber='42';Size=8GB;IsBoot=$false;IsSystem=$false;IsOffline=$false;IsReadOnly=$false;BusType='USB'}
$r=[pscustomobject]@{number=2;uniqueId='USB-42';serial='42';bytes=8GB}
Assert-ExternalDisk $d $r
function MustReject([scriptblock]$Test) { $rejected=$false;try { &$Test } catch {$rejected=$true};if(!$rejected){throw 'Safety policy did not reject an unsafe request'} }
foreach($key in @('IsBoot','IsSystem','IsReadOnly','IsOffline')) { $d.$key=$true;MustReject {Assert-ExternalDisk $d $r};$d.$key=$false }
$d.BusType='SATA';MustReject {Assert-ExternalDisk $d $r};$d.BusType='USB'
$r.uniqueId='REPLACEMENT';MustReject {Assert-ExternalDisk $d $r};$r.uniqueId='USB-42'
$r.bytes=4GB;MustReject {Assert-ExternalDisk $d $r};$r.bytes=8GB
Assert-FileSystem 'FAT32' 8GB
MustReject {Assert-FileSystem 'FAT32' 64GB}
MustReject {Assert-FileSystem 'INVALID' 8GB}
$p=[pscustomobject]@{PartitionNumber=1;Offset=1MB;Size=4GB;IsBoot=$false;IsSystem=$false}
$pr=[pscustomobject]@{partition=1;offset=1MB;partitionBytes=4GB}
Assert-Partition $p $pr
$pr.offset=2MB;MustReject {Assert-Partition $p $pr}
$d | Add-Member -NotePropertyName PartitionStyle -NotePropertyValue 'RAW'
if((Get-EmptyDiskStyleAction $d $r 0 'GPT') -ne 'initialize'){throw 'RAW initialization decision failed'}
$d.PartitionStyle='MBR'
if((Get-EmptyDiskStyleAction $d $r 0 'MBR') -ne 'keep'){throw 'Empty initialized MBR must be retained'}
if((Get-EmptyDiskStyleAction $d $r 0 'GPT') -ne 'convert'){throw 'Empty MBR to GPT conversion decision failed'}
$d.PartitionStyle='GPT'
if((Get-EmptyDiskStyleAction $d $r 0 'MBR') -ne 'convert'){throw 'Empty GPT to MBR conversion decision failed'}
MustReject {Get-EmptyDiskStyleAction $d $r 1 'GPT'}
MustReject {Get-EmptyDiskStyleAction $d $r 0 'INVALID'}
$d.IsSystem=$true;MustReject {Get-EmptyDiskStyleAction $d $r 0 'GPT'};$d.IsSystem=$false
$r.serial='CHANGED';MustReject {Get-EmptyDiskStyleAction $d $r 0 'GPT'};$r.serial='42'
# Parse the complete production script without touching any disk.
$tokens=$null;$errors=$null
$null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot '../scripts/storage.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw ($errors | Out-String)}
Write-Host 'PowerShell safety policy and production syntax passed. No storage mutations executed.'
