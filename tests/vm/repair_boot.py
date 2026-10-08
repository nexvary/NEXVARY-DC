"""Real GRUB repair followed by firmware boot under QEMU, BIOS and UEFI.

Small ext4 Linux fixture, live RAM root, real GRUB tools and efibootmgr. This
proves kernel handoff after repair, not a full Windows installation or Secure Boot.
Run as root on an ephemeral Ubuntu CI runner with loop devices.
"""
import glob
import json
import os
import pathlib
import shutil
import subprocess
import time

out = pathlib.Path('out/boot-repair-vm').resolve()
out.mkdir(parents=True, exist_ok=True)
ram = out / 'ram'
ram.mkdir(exist_ok=True)


def call(args):
    return subprocess.check_output([str(a) for a in args], text=True).strip()


copied = set()
def binary(path):
    path = pathlib.Path(path)
    if str(path) in copied:
        return
    copied.add(str(path))
    target = ram / str(path).lstrip('/')
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, target)
    target.chmod(0o755)
    result = subprocess.run(['ldd', str(path)], capture_output=True, text=True)
    for word in result.stdout.split():
        if word.startswith('/') and pathlib.Path(word).is_file():
            binary(word)


for tool in ['python3', 'lsblk', 'findmnt', 'efibootmgr', 'grub-install', 'grub-script-check',
             'grub-mkimage', 'grub-probe', 'grub-bios-setup', 'grub-editenv', 'grub-mkrelpath', 'modprobe']:
    executable = shutil.which(tool)
    if not executable and tool == 'grub-bios-setup':
        executable = '/usr/lib/grub/i386-pc/grub-bios-setup'
    assert executable, 'Missing tool: ' + tool
    binary(executable)
shutil.copytree('/usr/lib/python3.12', ram / 'usr/lib/python3.12', dirs_exist_ok=True)
for extension in (ram / 'usr/lib/python3.12').rglob('*.so'):
    result = subprocess.run(['ldd', str(extension)], capture_output=True, text=True)
    for word in result.stdout.split():
        if word.startswith('/') and pathlib.Path(word).is_file():
            binary(word)
for directory in ['/usr/lib/grub', '/usr/share/grub']:
    shutil.copytree(directory, ram / directory.lstrip('/'), dirs_exist_ok=True)
kernel = pathlib.Path(sorted(glob.glob('/boot/vmlinuz-*'))[-1])
version = kernel.name.removeprefix('vmlinuz-')
modules = ram / 'lib/modules' / version
modules.mkdir(parents=True, exist_ok=True)
for path in pathlib.Path('/lib/modules', version).glob('modules.*'):
    shutil.copyfile(path, modules / path.name)
for module in ['virtio_pci', 'virtio_blk', 'ext4', 'vfat', 'efivarfs']:
    deps = call(['modprobe', '--set-version', version, '--show-depends', module])
    for line in deps.splitlines():
        if line.startswith('insmod '):
            path = pathlib.Path(line.split()[1]); target = ram / str(path).lstrip('/')
            target.parent.mkdir(parents=True, exist_ok=True); shutil.copyfile(path, target)
for name in ['bin', 'dev', 'proc', 'sys', 'tmp', 'run', 'mnt/linux', 'mnt/esp', 'backup', 'etc']:
    (ram / name).mkdir(parents=True, exist_ok=True)
shutil.copyfile('/bin/busybox', ram / 'bin/busybox')
(ram / 'bin/busybox').chmod(0o755)
for name in ['sh', 'mount', 'umount', 'mkdir', 'sleep', 'poweroff', 'cat', 'sync']:
    (ram / 'bin' / name).symlink_to('busybox')
(ram / 'etc/mtab').symlink_to('/proc/mounts')
shutil.copyfile('scripts/boot_repair.py', ram / 'repair.py')
(ram / 'repair-guest.py').write_text('''import json,pathlib,subprocess,sys
sys.path.insert(0,'/')
import repair
uefi=pathlib.Path('/sys/firmware/efi').exists()
mode='uefi' if uefi else 'bios'
subprocess.run(['mount','/dev/vda2' if uefi else '/dev/vda1','/mnt/linux'],check=True)
if uefi:
 subprocess.run(['mount','/dev/vda1','/mnt/esp'],check=True)
 subprocess.run(['mount','-t','efivarfs','efivarfs','/sys/firmware/efi/efivars'],check=True)
 # Existing Windows firmware entry is a test placeholder; it must be preserved.
 repair.run(['efibootmgr','-c','-d','/dev/vda','-p','1','-L','Windows Boot Manager','-l',r'\\EFI\\Microsoft\\Boot\\bootmgfw.efi'])
before=repair.run(['efibootmgr','-v']) if uefi else ''
p=repair.plan('/mnt/linux','/mnt/esp' if uefi else '', '/dev/vda',mode,'/backup')
r=repair.apply(p,'REPAIR /dev/vda')
print('DC_REPAIR_RESULT='+json.dumps(r),flush=True)
assert r['status']=='completed',r
if uefi:
 after=repair.run(['efibootmgr','-v'])
 assert 'Windows Boot Manager' in after
 assert 'Windows Boot Manager (NEXVARY)' in pathlib.Path('/mnt/linux/boot/grub/grub.cfg').read_text()
 assert repair.sha('/mnt/esp/EFI/Microsoft/Boot/bootmgfw.efi')==p['windowsLoaderSha256']
subprocess.run(['sync'],check=True)
print('DC_REPAIR_OK_'+mode.upper(),flush=True)
''')
(ram / 'init').write_text('''#!/bin/sh
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /run
mount -t tmpfs tmpfs /tmp
modprobe virtio_pci
modprobe virtio_blk
modprobe ext4
modprobe vfat
modprobe efivarfs
sleep 2
if cat /proc/cmdline | /bin/busybox grep -q nexvary.installed; then
 echo DC_INSTALLED_KERNEL_BOOT_OK
else
 /usr/bin/python3 /repair-guest.py
fi
sync
poweroff -f
''')
(ram / 'init').chmod(0o755)
initrd = out / 'initrd.img'
with open(initrd, 'wb') as f:
    subprocess.run(['bash', '-c', 'find . -print0 | cpio --null -o --format=newc | gzip -1'], cwd=ram, stdout=f, check=True)
kernel = pathlib.Path(sorted(glob.glob('/boot/vmlinuz-*'))[-1])


def vm(args, log, marker, seconds=120):
    with open(log, 'wb') as stream:
        p = subprocess.Popen(['qemu-system-x86_64', '-accel', 'tcg', '-m', '768', '-display', 'none',
             '-serial', 'stdio', '-no-reboot', '-nic', 'none', *args], stdout=stream, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + seconds
            while p.poll() is None and time.monotonic() < deadline:
                time.sleep(1)
            if p.poll() is None:
                p.kill()
            p.wait()
        finally:
            if p.poll() is None:
                p.kill(); p.wait()
    text = pathlib.Path(log).read_text(errors='replace')
    if marker:
        assert marker in text, text[-10000:]
    return text


results=[]
for mode in ['bios','uefi']:
    disk=out/(mode+'.img')
    with disk.open('wb') as f:
        f.truncate(384*1024*1024)
    call(['parted','-s',disk,'mklabel','msdos' if mode=='bios' else 'gpt'])
    if mode=='bios':
        call(['parted','-s',disk,'mkpart','primary','ext4','1MiB','100%'])
    else:
        call(['parted','-s',disk,'mkpart','ESP','fat32','1MiB','65MiB'])
        call(['parted','-s',disk,'set','1','esp','on'])
        call(['parted','-s',disk,'mkpart','Linux','ext4','65MiB','100%'])
    loop=call(['losetup','--find','--show','--partscan',disk])
    root=out/'root';root.mkdir(exist_ok=True)
    esp=out/'esp';esp.mkdir(exist_ok=True)
    try:
        call(['mkfs.ext4','-q','-F',loop+('p1' if mode=='bios' else 'p2')])
        call(['mount',loop+('p1' if mode=='bios' else 'p2'),root])
        (root/'boot/grub').mkdir(parents=True)
        (root/'etc').mkdir()
        (root/'etc/os-release').write_text('ID=nexvary-test-fixture\n')
        shutil.copyfile(kernel,root/'boot/vmlinuz')
        shutil.copyfile(initrd,root/'boot/initrd.img')
        (root/'boot/grub/grub.cfg').write_text('''serial --unit=0 --speed=115200
terminal_input serial
terminal_output serial
set timeout=0
menuentry 'NEXVARY Linux fixture' {
 linux /boot/vmlinuz console=ttyS0 nexvary.installed=1
 initrd /boot/initrd.img
}
''')
        if mode=='uefi':
            call(['mkfs.vfat', '-F','32',loop+'p1'])
            call(['mount',loop+'p1',esp])
            (esp/'EFI/Microsoft/Boot').mkdir(parents=True)
            (esp/'EFI/Microsoft/Boot/bootmgfw.efi').write_bytes(b'Windows-loader-placeholder-not-a-Windows-boot-test')
            call(['umount',esp])
        call(['umount',root])
    finally:
        subprocess.run(['umount',str(root)],capture_output=True)
        subprocess.run(['umount',str(esp)],capture_output=True)
        call(['losetup','--detach',loop])
    # There is deliberately no GRUB boot code/EFI loader yet: Windows-overwrite
    # and missing-loader condition. Actual repair runs inside a live RAM guest.
    firmware=[]
    if mode=='uefi':
        variables=out/'OVMF_VARS.fd'
        shutil.copyfile('/usr/share/OVMF/OVMF_VARS_4M.fd',variables)
        firmware=['-drive','if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd',
                  '-drive','if=pflash,format=raw,file='+str(variables)]
    drive=['-drive','if=virtio,format=raw,file='+str(disk)]
    before=vm(firmware+drive,out/(mode+'-before.txt'),None,20)
    assert 'DC_INSTALLED_KERNEL_BOOT_OK' not in before
    vm(firmware+drive+['-kernel',kernel,'-initrd',initrd,'-append','console=ttyS0 rdinit=/init'],
       out/(mode+'-repair.txt'),'DC_REPAIR_OK_'+mode.upper(),180)
    vm(firmware+drive,out/(mode+'-after.txt'),'DC_INSTALLED_KERNEL_BOOT_OK',120)
    results.append({'mode':mode,'unrepairedDiskBooted':False,'repairSucceeded':True,
                    'firmwareBootReachedKernel':True,'secureBoot':False,'windowsBootTested':False})
    (out/'results.json').write_text(json.dumps(results,indent=2))
    print(json.dumps(results[-1]))
