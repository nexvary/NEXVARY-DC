function Write-HybridImage($Disk, [string]$Source) {
 if (!(Test-Path -LiteralPath $Source -PathType Leaf) -or [IO.Path]::GetExtension($Source) -ine '.iso') { throw 'Select an existing hybrid Linux ISO.' }
 $sourceItem=Get-Item -LiteralPath $Source
 if ($sourceItem.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Source symlinks are not supported.' }
 $sector=[int]$Disk.LogicalSectorSize
 if ($sector -notin @(512,4096)) { throw 'Unsupported disk sector size.' }
 if ($sourceItem.Length -lt 32774 -or [Math]::Ceiling($sourceItem.Length / [double]$sector)*$sector -gt $Disk.Size) { throw 'ISO is empty, too small or exceeds target capacity.' }
 $input=[IO.File]::Open($Source,'Open','Read','Read')
 try {
  $header=New-Object byte[] 512
  if($input.Read($header,0,512) -ne 512 -or $header[510] -ne 0x55 -or $header[511] -ne 0xaa) { throw 'ISO has no hybrid MBR signature; raw writing cannot make this ISO bootable.' }
  $code=$false;for($i=0;$i -lt 440;$i++){if($header[$i] -ne 0){$code=$true;break}}
  $partition=$false;for($i=0;$i -lt 4;$i++){if($header[446+$i*16+4] -ne 0){$partition=$true}}
  $null=$input.Seek(32769,'Begin');$id=New-Object byte[] 5;$null=$input.Read($id,0,5)
  if(!$code -or !$partition -or [Text.Encoding]::ASCII.GetString($id) -cne 'CD001') { throw 'Expected ISO9660 hybrid image with MBR boot code and partition table.' }
 } finally {$input.Dispose()}
 $expected=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()
 # Require access to every filesystem volume so none can remain mounted during raw writes.
 $volumes=@(Get-Partition -DiskNumber $Disk.Number -ErrorAction Stop | Get-Volume -ErrorAction Stop | ForEach-Object {
  if(!$_.UniqueId -or !([string]$_.UniqueId).StartsWith('\\?\Volume{')) { throw 'Cannot identify all target volumes for exclusive locking.' }
  ([string]$_.UniqueId).TrimEnd('\')
 })
 Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Collections.Generic;
using Microsoft.Win32.SafeHandles;
public static class DcHybridWriter {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern SafeFileHandle CreateFile(string p,uint a,uint s,IntPtr z,uint c,uint f,IntPtr t);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool DeviceIoControl(SafeFileHandle h,uint c,IntPtr i,uint n,IntPtr o,uint m,out uint b,IntPtr x);
 static void Control(SafeFileHandle h,uint code){uint n;if(!DeviceIoControl(h,code,IntPtr.Zero,0,IntPtr.Zero,0,out n,IntPtr.Zero))throw new Win32Exception(Marshal.GetLastWin32Error());}
 public static string Write(string source,string device,string[] volumes,int sector,long expectedLength,string expectedHash) {
  var locks=new List<SafeFileHandle>();
  try {
   foreach(var path in volumes){var h=CreateFile(path,0xc0000000,3,IntPtr.Zero,3,0,IntPtr.Zero);if(h.IsInvalid){h.Dispose();throw new Win32Exception(Marshal.GetLastWin32Error());}locks.Add(h);Control(h,0x90018);Control(h,0x90020);}
   using(var input=new FileStream(source,FileMode.Open,FileAccess.Read,FileShare.Read)) {
    if(input.Length!=expectedLength)throw new IOException("Source size changed before writing.");
    using(var sha=SHA256.Create()){if(BitConverter.ToString(sha.ComputeHash(input)).Replace("-","").ToLowerInvariant()!=expectedHash)throw new IOException("Source hash changed before writing.");}input.Position=0;
    using(var handle=CreateFile(device,0xc0000000,3,IntPtr.Zero,3,0x80000000,IntPtr.Zero)) {
     if(handle.IsInvalid)throw new Win32Exception(Marshal.GetLastWin32Error());
     using(var target=new FileStream(handle,FileAccess.ReadWrite,1048576,false)) {
      var buffer=new byte[1048576];long left=expectedLength;
      while(left>0){int count=(int)Math.Min(buffer.Length,left),got=0;while(got<count){int n=input.Read(buffer,got,count-got);if(n==0)throw new IOException("Source read failed.");got+=n;}int padded=((count+sector-1)/sector)*sector;if(padded>count)Array.Clear(buffer,count,padded-count);target.Write(buffer,0,padded);left-=count;}
      target.Flush(true);target.Position=0;
      using(var sha=SHA256.Create()){left=expectedLength;while(left>0){int count=(int)Math.Min(buffer.Length,left),aligned=((count+sector-1)/sector)*sector,got=0;while(got<aligned){int n=target.Read(buffer,got,aligned-got);if(n==0)throw new IOException("Target readback failed.");got+=n;}sha.TransformBlock(buffer,0,count,buffer,0);left-=count;}sha.TransformFinalBlock(new byte[0],0,0);return BitConverter.ToString(sha.Hash).Replace("-","").ToLowerInvariant();}
     }
    }
   }
  } finally {foreach(var h in locks)h.Dispose();}
 }
}
'@
 $null=Guard
 $actual=[DcHybridWriter]::Write($Source,[string]$r.device,[string[]]$volumes,$sector,[long]$sourceItem.Length,$expected)
 if($actual -cne $expected) { throw 'Hybrid image readback mismatch. Target is incomplete; recreate media.' }
 Update-HostStorageCache -ErrorAction SilentlyContinue
 return @{status='completed';operation='linux_usb';device=$r.device;sha256=$expected;verifiedBytes=[long]$sourceItem.Length;imageReadbackVerified=$true;message='Hybrid ISO written and SHA-256 readback verified. BIOS/UEFI support depends on the selected image; physical boot has not been tested.'}
}
