"""Windows production USB qualification in an isolated Linux/KVM guest.

All disk preparation occurs inside the VM. Host targets are new regular files;
no physical host disks, network or redistributed Microsoft images are used.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time

URL = 'https://software-static.download.prss.microsoft.com/dbazure/888969d5-f34g-4e03-ac9d-1f9786c66749/26100.1742.240906-0331.ge_release_svc_refresh_CLIENT_LTSC_EVAL_x64FRE_en-us.iso'
SHA = '67cec5865eaa037a72ddc633a717a10a2bed50778862267223ddb9c60ef5da68'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def command(args, **kwargs):
    print('fixture command:', args[0], flush=True)
    return subprocess.run(args, check=True, **kwargs)


def text(path, value):
    Path(path).write_bytes(value.replace('\r\n', '\n').replace('\n', '\r\n').encode('ascii'))


def deployment(mode):
    """Disk zero is the one isolated 64 GiB guest disk; never host DiskPart."""
    rows = ['select disk 0', 'clean', 'convert '+('mbr' if mode == 'bios' else 'gpt')]
    if mode == 'bios':
        rows += ['create partition primary size=512', 'format fs=ntfs quick label=DC_BOOT', 'assign letter=S', 'active']
    else:
        rows += ['create partition efi size=260', 'format fs=fat32 quick label=DC_ESP', 'assign letter=S', 'create partition msr size=16']
    rows += ['create partition primary', 'format fs=ntfs quick label=DC_WINDOWS', 'assign letter=W', 'exit']
    batch = r'''@echo off
wpeinit
for %%d in (C D E F G H I J K L M N O P Q R S T U V W Y Z) do if exist %%d:\DCPROOF.MRK set PROOF=%%d:
if not defined PROOF goto fail
set LOG=%PROOF%\deploy-proof.txt
echo DC_WINPE_DEPLOY_START > "%LOG%"
for %%d in (D E F G H I J K L M N O P Q R T U V W Y Z) do (
 if exist %%d:\sources\install.wim set INSTALL=%%d:
 if exist %%d:\dc-payload.marker set PAYLOAD=%%d:
)
if not defined INSTALL goto fail
if not defined PAYLOAD goto fail
diskpart /s X:\dc-diskpart.txt > COM1 2>&1
if errorlevel 1 goto fail
dism /Apply-Image /ImageFile:%INSTALL%\sources\install.wim /Index:1 /ApplyDir:C:\ /CheckIntegrity > COM1 2>&1
if errorlevel 1 goto fail
bcdboot C:\Windows /s S: /f FIRMWARE > COM1 2>&1
if errorlevel 1 goto fail
BIOSCODE
mkdir C:\Nexvary
copy /y %PAYLOAD%\storage.ps1 C:\Nexvary\ >nul
copy /y %PAYLOAD%\policy.ps1 C:\Nexvary\ >nul
copy /y %PAYLOAD%\hybrid.ps1 C:\Nexvary\ >nul
copy /y %PAYLOAD%\windows_guest.ps1 C:\dc-guest.ps1 >nul
copy /y %PAYLOAD%\source.iso C:\source.iso > COM1 2>&1
if errorlevel 1 goto fail
mkdir C:\Windows\Panther
copy /y %PAYLOAD%\unattend.xml C:\Windows\Panther\unattend.xml >nul
copy /y %PAYLOAD%\dc-proof.cmd C:\dc-proof.cmd >nul
echo MODE>C:\dc-mode.txt
echo DC_WINPE_DEPLOY_OK > COM1
wpeutil shutdown
goto :eof
:fail
echo DC_WINPE_DEPLOY_ERROR > COM1
wpeutil shutdown
'''
    bios = r'%INSTALL%\boot\bootsect.exe /nt60 S: /mbr > COM1 2>&1\nif errorlevel 1 goto fail' if mode == 'bios' else ''
    batch = batch.replace('FIRMWARE', 'BIOS' if mode == 'bios' else 'UEFI').replace('BIOSCODE', bios.replace('\\n', '\n')).replace('MODE', mode)
    batch = batch.replace('C:\\', 'W:\\').replace('> COM1', '>> "%LOG%"')
    return '\n'.join(rows)+'\n', batch


def qmp_capture(out, phase, press_key=False):
    try:
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(5)
            client.connect(str(out/'qmp.sock'))
            stream = client.makefile('rwb')
            def request(row):
                stream.write(json.dumps(row).encode()+b'\n'); stream.flush()
                while True:
                    answer = json.loads(stream.readline())
                    if 'error' in answer: raise RuntimeError(answer['error'])
                    if 'return' in answer: return answer
            stream.readline()
            request({'execute':'qmp_capabilities'})
            if press_key:
                request({'execute':'send-key', 'arguments':{'keys':[{'type':'qcode','data':'spc'}], 'hold-time':100}})
            else:
                request({'execute':'screendump', 'arguments':{'filename':str(out/(phase+'.ppm'))}})
    except Exception as exc:
        print('VM screenshot:', exc, flush=True)


def guest(out, mode, phase, drives, minutes):
    args = ['qemu-system-x86_64', '-machine','q35,smm=on', '-accel','kvm', '-cpu','host,vmx=off,svm=off', '-m','4096', '-smp','2', '-display','none', '-nic','none', '-rtc','base=utc', '-serial','file:'+str(out/(phase+'-serial.txt')), '-qmp','unix:'+str(out/'qmp.sock')+',server=on,wait=off', *drives]
    if mode != 'bios':
        vars_file = out/'vars.fd'
        shutil.copyfile('/usr/share/OVMF/OVMF_VARS_4M.'+('ms.' if mode == 'secureboot' else '')+'fd', vars_file)
        args += ['-global','driver=cfi.pflash01,property=secure,value=on', '-drive','if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.secboot.fd', '-drive','if=pflash,format=raw,file='+str(vars_file)]
    with (out/(phase+'-qemu.log')).open('wb') as log:
        process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic()+minutes*60
            next_proof = time.monotonic()+15
            while process.poll() is None and time.monotonic() < deadline:
                time.sleep(2)
                if phase == 'deploy':
                    serial = out/(phase+'-serial.txt')
                    if not serial.exists() or 'DC_WINPE_DEPLOY_START' not in serial.read_text(errors='replace'):
                        # Official Windows DVD may ask for a key within five seconds.
                        qmp_capture(out, phase, press_key=True)
                if time.monotonic() >= next_proof:
                    qmp_capture(out, phase)
                    serial = out/(phase+'-serial.txt')
                    print('VM phase:', phase, 'proof tail:', serial.read_text(errors='replace')[-2000:] if serial.exists() else '', flush=True)
                    next_proof = time.monotonic()+60
            if process.poll() is None:
                qmp_capture(out, phase)
                raise TimeoutError(phase+' Windows VM timeout')
            if process.returncode: raise RuntimeError((out/(phase+'-qemu.log')).read_text(errors='replace'))
        finally:
            if process.poll() is None: process.terminate(); process.wait(timeout=15)
    proof = (out/(phase+'-serial.txt')).read_text(errors='replace')
    if phase in ('deploy', 'usb'):
        # Minimal WinPE does not always enumerate the legacy COM driver. A new
        # disposable FAT USB provides independent in-guest proof and diagnostics.
        row = subprocess.run(['mtype','-i',str(out/'proof.img'),'::'+phase+'-proof.txt'], capture_output=True, text=True)
        if row.returncode == 0:
            (out/(phase+'-guest.txt')).write_text(row.stdout)
            proof += '\n'+row.stdout
    print(proof[-6000:], flush=True)
    return proof


def require(proof, *markers):
    for marker in markers:
        if marker not in proof: raise AssertionError('Missing guest proof: '+marker)
    if 'DC_WINDOWS_VM_ERROR=' in proof or 'DC_WINPE_DEPLOY_ERROR' in proof: raise AssertionError(proof[-6000:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['bios','uefi','secureboot'], required=True)
    mode = parser.parse_args().mode
    if os.environ.get('GITHUB_ACTIONS') != 'true': raise RuntimeError('Ephemeral CI runner only')
    if not os.access('/dev/kvm', os.R_OK|os.W_OK): raise RuntimeError('KVM unavailable; cannot attest this fixture')
    out = Path('out/windows-vm-'+mode).resolve(); out.mkdir(parents=True, exist_ok=False)
    work = Path(os.environ['RUNNER_TEMP'])/('dc-kvm-'+mode); work.mkdir(exist_ok=False)
    try:
        proof_disk = out/'proof.img'
        with proof_disk.open('xb') as stream: stream.truncate(16*1024**2)
        command(['mkfs.vfat','-F','16','-n','DCPROOF',str(proof_disk)])
        marker = work/'DCPROOF.MRK'; marker.write_text('isolated guest proof disk\n')
        command(['mcopy','-i',str(proof_disk),str(marker),'::DCPROOF.MRK'])
        proof_usb = ['-drive','file='+str(proof_disk)+',format=raw,if=none,id=dcproof','-device','usb-storage,drive=dcproof,serial=DC_PROOF_ONLY,removable=on']
        if shutil.disk_usage(work).free < 35*1024**3: raise OSError('At least 35 GiB of ephemeral disk free space required')
        iso = work/'evaluation.iso'
        command(['curl','--fail','--location','--retry','5','--retry-all-errors','--retry-max-time','600','--output',str(iso),URL])
        if sha(iso) != SHA: raise ValueError('Evaluation fixture hash mismatch')
        payload = work/'payload'; payload.mkdir()
        os.link(iso, payload/'source.iso')
        text(payload/'dc-payload.marker', 'isolated fixture\n')
        for source in ['scripts/storage.ps1','scripts/policy.ps1','scripts/hybrid.ps1','tests/vm/windows_guest.ps1']:
            shutil.copyfile(source, payload/Path(source).name)
        shutil.copyfile('tests/vm/windows_unattend.xml', payload/'unattend.xml')
        text(payload/'dc-proof.cmd', '@echo off\nmode COM1: baud=115200 parity=n data=8 stop=1\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\\dc-guest.ps1\n')
        payload_iso = work/'payload.iso'
        command(['genisoimage','-udf','-allow-limited-size','-iso-level','3','-J','-R','-o',str(payload_iso),str(payload)])
        iso_tree = work/'iso'; iso_tree.mkdir()
        # Microsoft evaluation ISO stores files in UDF; its ISO9660 view is only a README.
        command(['7z','x','-tudf','-y','-o'+str(iso_tree),str(iso)])
        hook = work/'hook'; hook.mkdir()
        diskpart, batch = deployment(mode)
        text(hook/'dc-diskpart.txt', diskpart); text(hook/'dc-deploy.cmd', batch)
        (hook/'Windows/System32').mkdir(parents=True)
        text(hook/'Windows/System32/winpeshl.ini', '[LaunchApps]\n%SYSTEMROOT%\\System32\\cmd.exe, /c X:\\dc-deploy.cmd\n')
        command(['wimlib-imagex','update',str(iso_tree/'sources/boot.wim'),'2','--check','--command',"add '"+str(hook)+"' '/'" ])
        boot_iso = work/'deploy.iso'
        command(['genisoimage','-udf','-allow-limited-size','-iso-level','3','-J','-R','-b','boot/etfsboot.com','-no-emul-boot','-boot-load-size','8','-eltorito-alt-boot','-e','efi/microsoft/boot/efisys.bin','-no-emul-boot','-o',str(boot_iso),str(iso_tree)])
        shutil.rmtree(iso_tree)
        shutil.rmtree(payload); iso.unlink()
        disks = {}
        for name, size in [('installed',64),('prepared-usb',16),('small-usb',1)]:
            path = work/(name+'.img')
            with path.open('xb') as stream: stream.truncate(size*1024**3)
            disks[name] = path
        system = ['-drive','file='+str(disks['installed'])+',format=raw,if=ide,index=0']
        usb = ['-device','qemu-xhci','-drive','file='+str(disks['prepared-usb'])+',format=raw,if=none,id=dcusb','-device','usb-storage,drive=dcusb,serial=DC_BOOT_TARGET,removable=on']
        cds = ['-boot','order=d','-drive','file='+str(boot_iso)+',format=raw,media=cdrom,if=ide,index=2,readonly=on','-drive','file='+str(payload_iso)+',format=raw,media=cdrom,if=ide,index=3,readonly=on']
        proof = guest(out, mode, 'deploy', system+cds+['-device','qemu-xhci']+proof_usb, 30)
        require(proof, 'DC_WINPE_DEPLOY_OK')
        boot_iso.unlink(); payload_iso.unlink()
        small = ['-drive','file='+str(disks['small-usb'])+',format=raw,if=none,id=dcsmall','-device','usb-storage,drive=dcsmall,serial=DC_SMALL_TARGET,removable=on']
        proof = guest(out, mode, 'installed', system+usb+small+proof_usb, 40)
        require(proof, 'DC_INSTALLED_WINDOWS_BOOT_OK','DC_SYSTEM_DISK_REFUSED','DC_USB_IDENTITY_REFUSED','DC_USB_SPACE_REFUSED','DC_USB_PREPARED=','DC_USB_TEST_HOOK_READY', 'DC_SECURE_BOOT_'+('ON' if mode == 'secureboot' else 'OFF'))
        reference = sha(disks['prepared-usb'])
        protected_usb = [x.replace(',id=dcusb',',snapshot=on,id=dcusb') for x in usb]
        proof = guest(out, mode, 'usb', ['-boot','order=c']+protected_usb+proof_usb, 15)
        require(proof, 'DC_WINDOWS_USB_WINPE_OK','DC_USB_SECURE_BOOT_'+('ON' if mode == 'secureboot' else 'OFF'))
        if sha(disks['prepared-usb']) != reference: raise ValueError('Prepared reference USB changed during boot')
        (out/'result.json').write_text(json.dumps(dict(mode=mode, acceleration='KVM', installedWindowsBoot=True, secureBoot=mode=='secureboot', evaluationISO_SHA256=SHA, dcUSBPreparationTested=True, usbWindowsPEBoot=True, systemDiskRefused=True, changedIdentityRefused=True, insufficientSpaceRefused=True, preparedUSB_SHA256=reference, testHook='WinPE batch shell; signed EFI loader unchanged'), indent=2))
    finally:
        # The FAT scratch image is test evidence only; retain extracted text, not
        # arbitrary guest disk contents, and never publish Microsoft images.
        (out/'proof.img').unlink(missing_ok=True)
        (out/'vars.fd').unlink(missing_ok=True)
        shutil.rmtree(work)


if __name__ == '__main__': main()
