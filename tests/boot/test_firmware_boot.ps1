$ErrorActionPreference='Stop'
. "$PSScriptRoot/../../scripts/firmware_boot.ps1"
$linux='{12345678-1111-2222-3333-123456789abc}'
$windows='{9dea862c-5cdd-4e70-acc1-f32b344d4795}'
$fixture=@"
Firmware Application
--------------------
identifier $linux
path \EFI\ubuntu\shimx64.efi

Windows Boot Manager
--------------------
identifier $windows
path \EFI\Microsoft\Boot\bootmgfw.efi
"@
$entries=@(Get-FirmwareEntries $fixture)
if($entries.Count -ne 1 -or $entries[0].id -cne $linux){throw 'Linux entry parsing failed'}
$localized=$fixture.Replace('identifier','معرّف').Replace('path','مسار')
if(@(Get-FirmwareEntries $localized).Count -ne 1){throw 'Localized firmware parsing failed'}
$order=Get-FirmwareOrder "identifier {a5a30fa2-3d06-4e9f-b5f4-a01df9d1fcba}`ndisplayorder $windows`n $linux"
if(($order -join ',') -cne "$windows,$linux"){throw 'Order parsing failed'}
$rejected=$false
try {Get-FirmwareOrder 'invalid'}catch{$rejected=$true}
if(!$rejected){throw 'Invalid order accepted'}
if((Get-TextHash $fixture) -ceq (Get-TextHash ($fixture+'changed'))){throw 'Identity hash failed'}
Write-Output 'Firmware parser and localized identity fixtures passed; no firmware writes performed.'
