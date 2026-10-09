param([string]$Portable='out/portable',[string]$Output='out/crystal-proof')
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force $Output | Out-Null
$root=(Resolve-Path $Portable).Path
$report=Join-Path (Resolve-Path $Output).Path 'read-report.json'
$p=Start-Process (Join-Path $root 'nexvary_dc.exe') -ArgumentList @('--crystal-engine-proof',"`"$report`"") -PassThru
if(!$p.WaitForExit(150000)){$p.Kill();throw 'Native bundled-engine read timeout'}
if($p.ExitCode -ne 0){if(Test-Path $report){Get-Content $report -Raw|Write-Host};throw 'Native engine staging/read/report failed'}
$read=Get-Content $report -Raw|ConvertFrom-Json
if($read.status -ne 'completed' -or $read.engine -ne 'CrystalDiskInfo 9.9.2'){throw 'Engine proof identity mismatch'}
# The runner may expose no SMART-capable drives. Empty/unknown data is valid;
# it does not qualify physical controller support or prove a healthy disk.
$stage=Join-Path ([IO.Path]::GetTempPath()) ('dc-cdi-ui-'+[guid]::NewGuid())
New-Item -ItemType Directory $stage | Out-Null
$p=$null
try {
 Copy-Item (Join-Path $root 'engines/crystaldiskinfo/*') $stage -Recurse
 [IO.File]::WriteAllText((Join-Path $stage 'DiskInfo.ini'),"[Setting]`r`nLanguage=English`r`nAutoAamApm=0`r`nResident=0`r`nStartup=0`r`nAutoRefresh=0`r`n",[Text.Encoding]::Unicode)
 $p=Start-Process (Join-Path $stage 'DiskInfo64.exe') -WorkingDirectory $stage -PassThru
 if($p.WaitForExit(4000)){throw 'Original advanced panel exited during startup'}
 Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CrystalPanelProof {
 public delegate bool WindowCallback(IntPtr window, IntPtr param);
 [DllImport("user32.dll")] public static extern bool EnumWindows(WindowCallback callback, IntPtr param);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
 public static int Visible(int pid, bool close) {
  int count=0; EnumWindows((window,param)=>{uint owner;GetWindowThreadProcessId(window,out owner);if(owner==pid&&IsWindowVisible(window)){count++;if(close)PostMessage(window,0x10,IntPtr.Zero,IntPtr.Zero);}return true;},IntPtr.Zero);return count;
 }
}
'@
 $deadline=[DateTime]::UtcNow.AddSeconds(40)
 while([CrystalPanelProof]::Visible($p.Id,$false) -eq 0 -and [DateTime]::UtcNow -lt $deadline){if($p.HasExited){throw 'Advanced panel process exited'};Start-Sleep -Milliseconds 300}
 if([CrystalPanelProof]::Visible($p.Id,$false) -le 0){throw 'Advanced panel produced no visible window'}
 $windows=[CrystalPanelProof]::Visible($p.Id,$true)
 if(!$p.WaitForExit(10000)){throw 'Advanced panel did not close normally'}
 @{version='9.9.2';nativeReportParsed=$true;advancedVisibleWindows=$windows;advancedNormalClose=$true;automaticAamApmDisabled=$true;physicalDeviceSupportTested=$false;mailSendingTested=$false;hardwareControlTested=$false} | ConvertTo-Json | Set-Content (Join-Path $Output 'result.json') -Encoding utf8
 Write-Host 'CrystalDiskInfo full package, native import and original visible panel passed; no physical-device/mail/control qualification claimed.'
}finally{if($p -and !$p.HasExited){$p.Kill();$p.WaitForExit()};Remove-Item $stage -Recurse -Force}
