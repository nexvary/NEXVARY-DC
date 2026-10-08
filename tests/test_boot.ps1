$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
$root=Split-Path $PSScriptRoot -Parent
foreach($name in @('storage.ps1','policy.ps1','hybrid.ps1')) {
 $tokens=$null;$errors=$null
 $null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root "scripts/$name"),[ref]$tokens,[ref]$errors)
 if($errors.Count){throw ($errors | Out-String)}
}
$script=Get-Content (Join-Path $root 'scripts/hybrid.ps1') -Raw
$code=[regex]::Match($script,"(?s)Add-Type -TypeDefinition @'\r?\n(.*?)\r?\n'@").Groups[1].Value
if(!$code){throw 'Hybrid writer C# not found'}
Add-Type -TypeDefinition $code
$dir=Join-Path $env:TEMP ([guid]::NewGuid().ToString())
$null=New-Item -ItemType Directory $dir
try {
 $source=Join-Path $dir 'source.iso';$target=Join-Path $dir 'target.fixture'
 $bytes=New-Object byte[] 1048613
 for($i=0;$i -lt $bytes.Length;$i++){$bytes[$i]=[byte]($i%251)}
 [IO.File]::WriteAllBytes($source,$bytes)
 [IO.File]::WriteAllBytes($target,(New-Object byte[] 2097152))
 $hash=(Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant()
 $actual=[DcHybridWriter]::Write($source,$target,[string[]]@(),512,$bytes.Length,$hash)
 if($actual -cne $hash){throw 'Writer readback mismatch'}
 if((Get-FileHash $source -Algorithm SHA256).Hash.ToLowerInvariant() -cne $hash){throw 'Source modified'}
 $output=[IO.File]::ReadAllBytes($target)
 for($i=$bytes.Length;$i -lt [Math]::Ceiling($bytes.Length/512.0)*512;$i++){if($output[$i] -ne 0){throw 'Final sector was not zero padded'}}
 $before=(Get-FileHash $target).Hash
 $rejected=$false
 try {$null=[DcHybridWriter]::Write($source,$target,[string[]]@(),512,$bytes.Length,'wrong-hash')}catch{$rejected=$true}
 if(!$rejected -or (Get-FileHash $target).Hash -cne $before){throw 'Changed source guard failed'}
 . (Join-Path $root 'scripts/hybrid.ps1')
 $rejected=$false
 try {$null=Write-HybridImage ([pscustomobject]@{LogicalSectorSize=512;Size=2097152}) $source}catch{$rejected=$true}
 if(!$rejected){throw 'Non-hybrid source accepted'}
 $request=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes('{"action":"inventory"}'))
 $inventory=& (Join-Path $root 'scripts/storage.ps1') -RequestBase64 $request | ConvertFrom-Json
 if($inventory.status -ne 'completed' -or !$inventory.devices){throw 'Windows storage inventory unavailable'}
 Write-Host 'Boot helper parse, compilation, source guard, aligned writing and SHA-256 readback tests passed.'
} finally {Remove-Item $dir -Recurse -Force}
