param([string]$OutputDir='out/portable')
$ErrorActionPreference='Stop'
$version='9.9.2'
$binaryHash='01acb3176851a85824d9589c6514e3eb9771eb7f9d5ee58ed9b4e057bd21c7df'
$sourceHash='041f3937101583156270d766a78eee8c239e9b7705db500a6c669b01be53c30c'
$stage=Join-Path ([IO.Path]::GetTempPath()) ('dc-cdi-'+[guid]::NewGuid())
New-Item -ItemType Directory $stage | Out-Null
try {
 $binary=Join-Path $stage 'binary.zip';$source=Join-Path $stage 'source.zip'
 foreach($row in @(@{name='CrystalDiskInfo9_9_2.zip';path=$binary;hash=$binaryHash},@{name='CrystalDiskInfo9_9_2Src.zip';path=$source;hash=$sourceHash})) {
  & curl.exe --fail --location --retry 5 --retry-all-errors --max-time 300 "https://downloads.sourceforge.net/project/crystaldiskinfo/9.9.2/$($row.name)" --output $row.path
  if($LASTEXITCODE -ne 0){throw 'Official CrystalDiskInfo download failed'}
  if((Get-FileHash $row.path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $row.hash){throw 'Pinned CrystalDiskInfo archive hash mismatch'}
 }
 $target=Join-Path $OutputDir 'engines/crystaldiskinfo'
 New-Item -ItemType Directory -Force $target | Out-Null
 Expand-Archive $binary $target -Force
 $exe=Join-Path $target 'DiskInfo64.exe'
 $signature=Get-AuthenticodeSignature $exe
 if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'CrystalMark|Crystal Dew World|hiyohiyo'){throw ('Untrusted CrystalDiskInfo signature: '+$signature.Status+' '+$signature.SignerCertificate.Subject)}
 if((Get-Item $exe).VersionInfo.ProductVersion -notmatch '^9\.9\.2'){throw 'Unexpected CrystalDiskInfo executable version'}
 $files=Get-Content packaging/cdi-files.json -Raw | ConvertFrom-Json
 foreach($property in $files.PSObject.Properties){
  $file=Join-Path $target $property.Name
  if(!(Test-Path -LiteralPath $file) -or (Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $property.Value){throw ('Incomplete/modified CrystalDiskInfo package: '+$property.Name)}
 }
 # Preserve the upstream package unchanged, including dependent licenses and
 # the explicit AMD_RC2t7 version/hash declarations required by its license.
 Copy-Item packaging/cdi-files.json (Join-Path $target 'nexvary-files.json')
 $licenses=Join-Path $OutputDir 'licenses/CrystalDiskInfo'
 New-Item -ItemType Directory -Force $licenses | Out-Null
 Copy-Item (Join-Path $target 'License/*') $licenses -Recurse -Force
 Copy-Item $source (Join-Path $licenses 'CrystalDiskInfo9_9_2Src.zip')
 @{version=$version;binaryArchiveSHA256=$binaryHash;sourceArchiveSHA256=$sourceHash;signer=$signature.SignerCertificate.Subject;files=$files;distribution='Unmodified Standard Edition; MIT core plus accompanying dependency licenses; no charge for AMD_RC2t7 functionality'} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $licenses 'manifest.json') -Encoding utf8
 Write-Host 'Qualified and bundled CrystalDiskInfo 9.9.2 Standard Edition with all resources/licenses.'
}finally{Remove-Item $stage -Recurse -Force}
