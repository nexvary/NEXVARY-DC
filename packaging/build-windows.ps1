param([string]$BuildDir = "build", [string]$OutputDir = "out/portable")
$ErrorActionPreference = "Stop"
$exe = Join-Path $BuildDir "Release/nexvary_dc.exe"
if (!(Test-Path $exe)) { throw "Build the Release configuration first: $exe" }
New-Item -ItemType Directory -Force $OutputDir | Out-Null
Copy-Item $exe $OutputDir -Force
Copy-Item README.md $OutputDir -Force
Copy-Item docs $OutputDir -Recurse -Force
& windeployqt --release --qmldir qml (Join-Path $OutputDir "nexvary_dc.exe")
if ($LASTEXITCODE -ne 0) { throw "Qt deployment failed" }
