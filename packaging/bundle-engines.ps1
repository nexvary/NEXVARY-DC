param([string]$OutputDir='out/portable')
$ErrorActionPreference='Stop'
$cache=Join-Path ([IO.Path]::GetTempPath()) 'dc-smartmontools-7.5'
New-Item -ItemType Directory -Force $cache | Out-Null
$installer=Join-Path $cache 'smartmontools-7.5.win32-setup.exe'
Invoke-WebRequest 'https://github.com/smartmontools/smartmontools/releases/download/RELEASE_7_5/smartmontools-7.5.win32-setup.exe' -OutFile $installer
$expected='896337FCC253220614CF8CDBD5CF2321C5AA326A37A04160A672A281E6104C70'
if((Get-FileHash $installer -Algorithm SHA256).Hash -cne $expected){throw 'smartmontools installer checksum mismatch'}
$extract=Join-Path $cache 'extracted'
& 7z x $installer "-o$extract" -y | Out-Null
if($LASTEXITCODE -ne 0){throw 'Engine extraction failed'}
$smart=Get-ChildItem $extract -Recurse -Filter smartctl.exe | Select-Object -First 1
if(!$smart){throw 'Extracted smartctl.exe missing'}
$engine=Join-Path $OutputDir 'engines'
New-Item -ItemType Directory -Force $engine | Out-Null
Copy-Item $smart.FullName (Join-Path $engine 'smartctl.exe') -Force
Get-ChildItem $extract -Recurse -Filter '*.dll' | ForEach-Object { Copy-Item $_.FullName $engine -Force }
$licenses=Join-Path $OutputDir 'licenses/smartmontools'
New-Item -ItemType Directory -Force $licenses | Out-Null
Copy-Item packaging/SMARTMONTOOLS-COPYING $licenses -Force
Invoke-WebRequest 'https://github.com/smartmontools/smartmontools/releases/download/RELEASE_7_5/smartmontools-7.5.tar.gz' -OutFile (Join-Path $licenses 'smartmontools-7.5.tar.gz')
@{version='7.5';upstream='https://github.com/smartmontools/smartmontools';installerSha256=$expected;binarySha256=(Get-FileHash (Join-Path $engine 'smartctl.exe') -Algorithm SHA256).Hash;sourceSha256=(Get-FileHash (Join-Path $licenses 'smartmontools-7.5.tar.gz') -Algorithm SHA256).Hash} | ConvertTo-Json | Set-Content (Join-Path $licenses 'manifest.json') -Encoding utf8
& (Join-Path $engine 'smartctl.exe') --version
if($LASTEXITCODE -ne 0){throw 'Bundled SMART engine startup failed'}
Write-Host 'Verified smartmontools 7.5 bundled with license and corresponding source.'
