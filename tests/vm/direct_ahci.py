"""Real MMIO/DMA reads through patched OSC inside an isolated QEMU AHCI guest.
Host image is read-only; guest RAM destination. Reuse live RAM tree from repair VM.
"""
import glob
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time

out = Path('out/direct-vm').resolve();out.mkdir(parents=True,exist_ok=True)
ram = Path('out/boot-repair-vm/ram').resolve()
assert ram.is_dir(), 'Run repair_boot.py first to prepare the live RAM environment'
engine = Path('out/osc-direct/build/src/opensuperclone/opensuperclone').resolve()
assert engine.is_file(), 'Build the --direct adapter first'

copied=set()
def binary(source):
    source=Path(source)
    if str(source) in copied:return
    copied.add(str(source))
    target=ram/str(source).lstrip('/');target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,target);target.chmod(0o755)
    text=subprocess.run(['ldd',str(source)],capture_output=True,text=True).stdout
    for word in text.split():
        if word.startswith('/') and Path(word).is_file():binary(word)

binary(engine)
# Runtime engine must have the same hash as its qualification report.
shutil.copyfile(engine,ram/'engine');(ram/'engine').chmod(0o755)
shutil.copyfile('out/osc-direct/qualification.json',ram/'qualification.json')
for name in ['direct_rescue','relay_power']:
    shutil.copyfile('scripts/'+name+'.py',ram/(name+'.py'))
kernel=Path(sorted(glob.glob('/boot/vmlinuz-*'))[-1]);version=kernel.name.removeprefix('vmlinuz-')
for line in subprocess.check_output(['modprobe','--set-version',version,'--show-depends','ahci'],text=True).splitlines():
    if not line.startswith('insmod '):continue
    source=Path(line.split()[1]);relative=Path(*source.parts[source.parts.index(version)+1:]);target=ram/'lib/modules'/version/relative;target.parent.mkdir(parents=True,exist_ok=True)
    if target.suffix=='.zst':
        with target.with_suffix('').open('wb') as stream:subprocess.run(['zstd','-dc',str(source)],stdout=stream,check=True)
    else:shutil.copyfile(source,target)
subprocess.run(['depmod','--basedir',str(ram),version],check=True)
data=bytes(range(256))*8192
source=out/'source.img';source.write_bytes(data)
overlay=out/'source.qcow2'
subprocess.run(['qemu-img','create','-f','qcow2','-F','raw','-b',str(source),str(overlay)],check=True)
expected=hashlib.sha256(data).hexdigest()
(ram/'direct-guest.py').write_text('''import hashlib,json,pathlib,sys
sys.path.insert(0,'/')
import direct_rescue as dc
pci=[x.name for x in pathlib.Path('/sys/bus/pci/devices').iterdir() if (x/'class').read_text().strip()=='0x010601']
assert len(pci)==1,pci
identity={'pci':pci[0],'port':0,'serial':'DC_TEST_SERIAL','model':'QEMU HARDDISK','firmware':'2.5+','bytes':2097152}
source=dc.AHCI('/engine','/qualification.json',identity)
calls=[0]
read=source.read
def counted(offset,count):
 calls[0]+=1
 return read(offset,count)
source.read=counted
result=dc.rescue(source,'/tmp/image.bin',cancel=lambda:calls[0]>=1)
assert result['cancelled'] and result['bytes']==dc.CHUNK,result
result=dc.rescue(source,'/tmp/image.bin',resume=True)
assert not result['cancelled'] and result['badBytes']==0,result
actual=hashlib.sha256(pathlib.Path('/tmp/image.bin').read_bytes()).hexdigest()
assert actual=='EXPECTED',actual
# Hardware identity change is rejected before a data read.
source.identity['serial']='WRONG'
try:
 source.read(0,512)
except ValueError:pass
else:raise AssertionError('Changed source accepted')
print('DC_DIRECT_AHCI_OK='+json.dumps({'sha256':actual,'resume':True,'identityRejected':True,'bytes':2097152}),flush=True)
'''.replace('EXPECTED',expected))
(ram/'init').write_text('''#!/bin/sh
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox mount -t devtmpfs devtmpfs /dev
modprobe ahci
python3 /direct-guest.py
sync
poweroff -f
''');(ram/'init').chmod(0o755)
initrd=out/'direct.cpio.gz'
with initrd.open('wb') as stream:
    subprocess.run(['bash','-c','find . -print0 | cpio --null -o --format=newc 2>/dev/null | gzip -1'],cwd=ram,stdout=stream,check=True)
log=out/'serial.txt'
with log.open('wb') as stream:
    process=subprocess.Popen(['qemu-system-x86_64','-machine','q35','-accel','tcg','-m','1024','-display','none',
         '-serial','stdio','-no-reboot','-nic','none','-kernel',str(kernel),'-initrd',str(initrd),
         '-append','console=ttyS0 rdinit=/init iommu=off iomem=relaxed libata.force=1:disable',
         '-drive','if=none,id=source,format=qcow2,file='+str(overlay),
         '-device','ide-hd,drive=source,bus=ide.0,serial=DC_TEST_SERIAL'],stdout=stream,stderr=subprocess.STDOUT)
    try:
        deadline=time.monotonic()+240
        while process.poll() is None and time.monotonic()<deadline:time.sleep(1)
        if process.poll() is None:process.kill()
        process.wait()
    finally:
        if process.poll() is None:process.kill();process.wait()
text=log.read_text(errors='replace')
assert 'DC_DIRECT_AHCI_OK=' in text,text[-14000:]
assert hashlib.sha256(source.read_bytes()).hexdigest()==expected,'Source changed'
extents=json.loads(subprocess.check_output(['qemu-img','map','--output=json',str(overlay)],text=True))
assert not any(x.get('depth')==0 and x.get('data') for x in extents),'Source write allocated an overlay data cluster'
result={'sourceWritesObserved':False,'transport':'OpenSuperClone direct AHCI MMIO/DMA','sourceSHA256':expected,'sourceUnchanged':True,
        'readBytes':len(data),'cancelResumeTested':True,'sourceIdentityRejected':True,'physicalHardwareTested':False}
(out/'results.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
