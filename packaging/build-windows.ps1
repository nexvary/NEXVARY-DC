param([string]$BuildDir = "build", [string]$OutputDir = "out/portable")
$ErrorActionPreference = "Stop"
$exe = Join-Path $BuildDir "Release/nexvary_dc.exe"
if (!(Test-Path $exe)) { throw "Build the Release configuration first: $exe" }
New-Item -ItemType Directory -Force $OutputDir | Out-Null
Copy-Item $exe $OutputDir -Force
Copy-Item README.md $OutputDir -Force
Copy-Item docs $OutputDir -Recurse -Force
& windeployqt --release --no-compiler-runtime --qmldir qml (Join-Path $OutputDir "nexvary_dc.exe")
if ($LASTEXITCODE -ne 0) { throw "Qt deployment failed" }
# windeployqt normally deploys qwindows only. The release smoke test uses offscreen.
$qtBin = Split-Path (Get-Command windeployqt).Source -Parent
$offscreen = Join-Path (Split-Path $qtBin -Parent) "plugins/platforms/qoffscreen.dll"
if (!(Test-Path $offscreen)) { throw "Qt offscreen plugin unavailable" }
Copy-Item $offscreen (Join-Path $OutputDir "platforms/qoffscreen.dll") -Force

# App-local Microsoft CRT: do not rely on a separately installed redistributable.
if (!$env:VCToolsRedistDir) { throw "Run packaging from an MSVC developer terminal" }
$crt = Get-ChildItem (Join-Path $env:VCToolsRedistDir "x64") -Directory -Filter "Microsoft.VC*.CRT" | Sort-Object Name -Descending | Select-Object -First 1
if (!$crt) { throw "Microsoft CRT redistributable directory unavailable" }
Copy-Item (Join-Path $crt.FullName "*.dll") $OutputDir -Force
foreach ($dll in @("msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll")) {
    if (!(Test-Path (Join-Path $OutputDir $dll))) { throw "Missing runtime dependency: $dll" }
}
Write-Host "Bundled app-local CRT from $($crt.Name)"

& ./packaging/bundle-engines.ps1 -OutputDir $OutputDir
Copy-Item NOTICE.md $OutputDir -Force
$qtLicense=Join-Path (Split-Path $qtBin -Parent) 'licenses'
if(Test-Path $qtLicense){Copy-Item $qtLicense (Join-Path $OutputDir 'licenses/Qt') -Recurse -Force}

New-Item -ItemType Directory -Force (Join-Path $OutputDir 'licenses/Qt') | Out-Null
Copy-Item packaging/Qt-*.txt (Join-Path $OutputDir 'licenses/Qt') -Force

New-Item -ItemType Directory -Force (Join-Path $OutputDir "recovery-tools") | Out-Null
Copy-Item scripts/boot_repair.py (Join-Path $OutputDir "recovery-tools/boot_repair.py") -Force

Copy-Item scripts/direct_rescue.py,scripts/relay_power.py,scripts/qualify_opensuperclone.py,scripts/osc_adapter_patch.py (Join-Path $OutputDir "recovery-tools") -Force
