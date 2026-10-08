"""Boot an official Alpine hybrid ISO as a virtual hard disk in BIOS and UEFI.
This is reference-image firmware coverage, not Windows installer/Secure Boot proof.
"""
import hashlib
import base64
import struct
import zlib
import json
import pathlib
import shutil
import socket
import subprocess
import time
import urllib.request

out = pathlib.Path('out/vm')
out.mkdir(parents=True, exist_ok=True)
url = 'https://dl-cdn.alpinelinux.org/alpine/v3.22/releases/x86_64/alpine-virt-3.22.1-x86_64.iso'
iso = out / 'alpine.iso'
with urllib.request.urlopen(url, timeout=90) as response, iso.open('wb') as target:
    shutil.copyfileobj(response, target)
with urllib.request.urlopen(url + '.sha256', timeout=30) as response:
    expected = response.read().decode().split()[0].lower()
actual = hashlib.sha256(iso.read_bytes()).hexdigest()
assert actual == expected, 'Reference ISO hash mismatch'
results = []
for mode in ('bios', 'uefi'):
    monitor = out / (mode + '.sock')
    command = ['qemu-system-x86_64', '-machine', 'q35', '-accel', 'tcg', '-m', '512',
               '-display', 'none', '-serial', 'file:' + str(out / (mode + '-serial.txt')),
               '-qmp', 'unix:' + str(monitor) + ',server=on,wait=off',
               '-drive', 'file=' + str(iso) + ',format=raw,if=ide,snapshot=on',
               '-no-reboot', '-nic', 'none']
    if mode == 'uefi':
        firmware = pathlib.Path('/usr/share/OVMF/OVMF_CODE_4M.fd')
        assert firmware.exists(), 'OVMF firmware missing'
        command += ['-drive', 'if=pflash,format=raw,readonly=on,file=' + str(firmware)]
    log = (out / (mode + '-qemu.txt')).open('wb')
    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
    passed = False
    try:
        for _ in range(30):
            if monitor.exists():
                break
            if process.poll() is not None:
                raise RuntimeError('QEMU exited: ' + mode)
            time.sleep(1)
        with socket.socket(socket.AF_UNIX) as conn:
            conn.settimeout(5)
            conn.connect(str(monitor))
            channel = conn.makefile('rwb', buffering=0)
            json.loads(channel.readline())
            def qmp(command, arguments=None):
                request = {'execute': command}
                if arguments is not None:
                    request['arguments'] = arguments
                channel.write((json.dumps(request)+'\n').encode())
                while True:
                    response = json.loads(channel.readline())
                    if 'error' in response:
                        raise RuntimeError(response['error'])
                    if 'return' in response:
                        return response['return']
            qmp('qmp_capabilities')
            for attempt in range(18):
                time.sleep(5)
                screenshot = (out / (mode + '.ppm')).resolve()
                qmp('screendump', {'filename': str(screenshot)})
                if screenshot.exists():
                    text = subprocess.check_output(['tesseract', str(screenshot), 'stdout'], stderr=subprocess.DEVNULL, text=True)
                    (out / (mode + '-screen.txt')).write_text(text)
                    if 'login:' in text.lower() or 'welcome to alpine' in text.lower():
                        passed = True
                        break
            qmp('quit')
        if process.poll() is None:
            process.terminate()
        process.wait(timeout=15)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        log.close()
    if (out / (mode + '.ppm')).exists():
        raw = (out / (mode + '.ppm')).read_bytes()
        magic, width, height, rest = raw.split(None, 3)
        maximum, pixels = rest.split(b'\n', 1)
        w, h = int(width), int(height)
        assert magic == b'P6' and maximum == b'255' and len(pixels) == w*h*3
        def chunk(kind, value):
            return struct.pack('>I', len(value))+kind+value+struct.pack('>I', zlib.crc32(kind+value)&0xffffffff)
        scan = b''.join(b'\0'+pixels[y*w*3:(y+1)*w*3] for y in range(h))
        png = b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', w,h,8,2,0,0,0))+chunk(b'IDAT', zlib.compress(scan))+chunk(b'IEND',b'')
        (out / (mode+'.png')).write_bytes(png)
        print('DC_VM_'+mode.upper()+'_PNG='+base64.b64encode(png).decode())
        print((out / (mode+'-screen.txt')).read_text())
    results.append({'mode': mode, 'referenceIsoSha256': actual, 'reachedAlpineLogin': passed,
                    'secureBoot': False, 'scope': 'reference hybrid ISO as virtual hard disk, software TCG'})
    (out / 'results.json').write_text(json.dumps(results, indent=2))
    assert passed, mode + ' did not reach an identifiable Alpine login screen'
iso.unlink()
