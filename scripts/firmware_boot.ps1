param([string]$RequestBase64)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

function Get-FirmwareEntries([string]$Text) {
 # /v expands well-known identifiers to GUIDs. Parse GUIDs/EFI paths rather
 # than localized field labels or descriptions.
 $result=@()
 foreach($block in ($Text -split '(?:\r?\n){2,}')) {
  $ids=[regex]::Matches($block,'\{[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}\}')
  $path=[regex]::Match($block,'(?im)\\EFI\\[^\r\n]+\.efi\s*$')
  if($ids.Count -gt 0 -and $path.Success) {
   $efi=$path.Value.Trim()
   if($efi -match '(?i)\\(?:shimx64|grubx64)\.efi$' -and $efi -notmatch '(?i)\\Microsoft\\') {
    $result+=@{id=$ids[0].Value.ToLowerInvariant();path=$efi}
   }
  }
 }
 return $result
}
function Get-FirmwareOrder([string]$Text) {
 $guid='\{[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}\}'
 if($Text -notmatch '\{a5a30fa2-3d06-4e9f-b5f4-a01df9d1fcba\}'){throw 'Firmware manager identity is missing.'}
 $field=[regex]::Match($Text,'(?im)^displayorder\s+'+$guid+'(?:\r?\n[ \t]+'+$guid+')*')
 if(!$field.Success){throw 'Firmware displayorder cannot be parsed safely in this system language.'}
 return @([regex]::Matches($field.Value,$guid) | ForEach-Object {$_.Value.ToLowerInvariant()})
}
function Invoke-Bcd([string[]]$Arguments) {
 $output=& "$env:SystemRoot\System32\bcdedit.exe" @Arguments 2>&1
 if($LASTEXITCODE -ne 0){throw "BCDEdit failed: $output"}
 return ($output -join "`n")
}
function Get-TextHash([string]$Text) {
 $h=[Security.Cryptography.SHA256]::Create()
 try {return ([BitConverter]::ToString($h.ComputeHash([Text.Encoding]::UTF8.GetBytes($Text)))).Replace('-','').ToLowerInvariant()}
 finally {$h.Dispose()}
}
if(!$RequestBase64){return} # Functions can be dot-sourced for fixture tests.
try {
 $request=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($RequestBase64)) | ConvertFrom-Json
 $admin=([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
 if(!$admin){throw 'Administrator permission is required to inspect firmware boot entries.'}
 $text=Invoke-Bcd @('/enum','firmware','/v')
 $orderText=Invoke-Bcd @('/enum','{fwbootmgr}','/v')
 $order=@(Get-FirmwareOrder $orderText)
 $entries=@(Get-FirmwareEntries $text)
 if($request.action -eq 'inventory') {
  @{status='completed';operation='firmware_boot_inventory';entries=$entries;message='Existing GRUB/shim firmware entries only. This does not recreate missing Linux EFI files.'} | ConvertTo-Json -Depth 8 -Compress
 } elseif($request.action -eq 'plan') {
  $chosen=@($entries | Where-Object {$_.id -ceq [string]$request.id})
  if($chosen.Count -ne 1 -or $order -notcontains [string]$request.id){throw 'Select one existing Linux entry already present in firmware boot order.'}
  $plan=@{mode='windows_uefi_order';id=$chosen[0].id;path=$chosen[0].path;firmwareHash=(Get-TextHash $text);orderHash=(Get-TextHash $orderText);order=$order;created=[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()}
  @{status='completed';operation='boot_repair_plan';plan=$plan;confirmation="BOOT $($plan.id)";message='Only firmware boot priority will change. Existing EFI loader availability and successful boot are not established.'} | ConvertTo-Json -Depth 8 -Compress
 } elseif($request.action -eq 'apply') {
  $plan=$request.plan
  if($plan.mode -cne 'windows_uefi_order' -or (Get-TextHash $text) -cne $plan.firmwareHash -or (Get-TextHash $orderText) -cne $plan.orderHash -or ([DateTimeOffset]::UtcNow.ToUnixTimeSeconds()-[long]$plan.created) -gt 180){throw 'Plan expired or firmware entries changed. Inspect again.'}
  if([string]$request.confirmation -cne "BOOT $($plan.id)"){throw 'Exact boot-entry confirmation is required.'}
  if(@($entries | Where-Object {$_.id -ceq $plan.id -and $_.path -ceq $plan.path}).Count -ne 1){throw 'Linux firmware entry changed.'}
  $backup=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) ('NEXVARY\Disk Care\BootBackups\'+[Guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Path $backup -Force | Out-Null
  $text | Set-Content (Join-Path $backup 'firmware-before.txt') -Encoding UTF8
  $orderText | Set-Content (Join-Path $backup 'order-before.txt') -Encoding UTF8
  $plan | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $backup 'plan.json') -Encoding UTF8
  $null=Invoke-Bcd @('/export',(Join-Path $backup 'windows-bcd-backup'))
  $null=Invoke-Bcd @('/set','{fwbootmgr}','displayorder',[string]$plan.id,'/addfirst')
  $after=@(Get-FirmwareOrder (Invoke-Bcd @('/enum','{fwbootmgr}','/v')))
  $expected=@([string]$plan.id)+@($order | Where-Object {$_ -cne [string]$plan.id})
  if(($after -join ',') -cne ($expected -join ',')){throw "Boot order verification failed. Review backup: $backup"}
  @{status='completed';operation='boot_repair';backup=$backup;mode='windows_uefi_order';bootTestedOnThisDevice=$false;message='Linux firmware entry moved first; all original order entries retained. Reboot to verify. Missing EFI files and damaged GRUB are not repaired by this operation.'} | ConvertTo-Json -Depth 8 -Compress
 } else {throw 'Unsupported firmware operation.'}
} catch {
 @{status='error';operation='boot_repair';message=$_.Exception.Message} | ConvertTo-Json -Compress
 exit 1
}
