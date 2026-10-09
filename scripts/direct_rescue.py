#!/usr/bin/env python3
"""Linux expert AHCI rescue adapter. Source commands: IDENTIFY and READ DMA EXT.
No firmware writes, controller unbind, arbitrary scripts or OS-driver override.
"""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import stat as file_stat
import subprocess
import tempfile
import time

CHUNK = 512 * 1024  # Fits OSC's single-page AHCI PRDT (< 1 MiB).


def digest(data):
    return hashlib.sha256(data).hexdigest()


def guard_controller(pci, port, sysfs=Path('/sys')):
    if not re.fullmatch(r'0000:[0-9a-f]{2}:[0-9a-f]{2}\.[0-7]', pci):
        raise ValueError('Expected canonical domain-0000 PCI address')
    if type(port) is not int or not 0 <= port < 32:
        raise ValueError('AHCI port must be 0..31')
    device = sysfs / 'bus/pci/devices' / pci
    if not device.is_dir() or (device / 'class').read_text().strip() != '0x010601':
        raise ValueError('Selected PCI device is not AHCI')
    if (device / 'driver').resolve().name != 'ahci':
        raise ValueError('Controller must remain registered with ahci; hide source ports at live boot')
    # Reject the entire controller if ANY child is exposed to the OS, including
    # mounted/system/swap/RAID/destination disks. Never automatically unbind it.
    for entry in (sysfs / 'class/block').iterdir():
        if device.resolve() in entry.resolve().parents:
            raise ValueError('Controller exposes a block device to the OS: ' + entry.name)
    for group in device.glob('iommu_group/devices/*'):
        if group.name != pci:
            raise ValueError('Shared IOMMU group is unsupported')
    # OSC uses physical DMA addresses, not an IOMMU API. Refuse mapped DMA.
    if (device / 'iommu').exists():
        raise ValueError('Active IOMMU unsupported by physical-DMA adapter')
    resources = (device / 'resource').read_text().splitlines()
    if len(resources) < 6:
        raise ValueError('AHCI BAR5 missing')
    first, last, flags = [int(x, 16) for x in resources[5].split()]
    if not first or last <= first or last > 0xffffffff or not flags & 0x200:
        raise ValueError('Adapter requires a 32-bit memory BAR5')
    return {'pci': pci, 'port': port, 'bar5': first, 'end': last,
            'vendor': (device / 'vendor').read_text().strip(),
            'device': (device / 'device').read_text().strip()}


def checked_text(text, maximum):
    if not isinstance(text, str) or not text or len(text) > maximum or not re.fullmatch(r'[A-Za-z0-9 ._+/-]+', text):
        raise ValueError('Unsupported ATA identity string')
    if text != text.strip(): raise ValueError('Use ATA identity without padding spaces')
    return text


def ata_text_check(label, offset, size, expected):
    # ATA fields may be left or right padded; compare the same trimmed identity
    # reported by upstream enumeration without discarding internal characters.
    return f'''seti ${label}_start = {offset}
seti ${label}_size = {size}
while ${label}_size > 0
 seti $character = buffer ${label}_start
 if $character != 32
  break
 endif
 seti ${label}_start = ${label}_start + 1
 seti ${label}_size = ${label}_size - 1
done
while ${label}_size > 0
 seti $last = ${label}_start + ${label}_size
 seti $last = $last - 1
 seti $character = buffer $last
 if $character != 32
  break
 endif
 seti ${label}_size = ${label}_size - 1
done
sets ${label} = buffer ${label}_start ${label}_size
sets $expected_{label} = "{expected}"
if ${label} != $expected_{label}
 exit 7
endif
'''


def read_script(identity, lba, count, output, timeout_seconds=10):
    # Fixed data-in commands only. No user-provided script/path interpolation.
    model = checked_text(identity['model'], 40)
    serial = checked_text(identity['serial'], 20)
    firmware = checked_text(identity['firmware'], 8)
    sectors = identity['bytes'] // 512
    if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= 60:
        raise ValueError('Read timeout must be 1..60 seconds')
    if type(lba) is not int or type(count) is not int or not 0 <= lba < sectors or not 1 <= count <= 1024 or count > sectors-lba:
        raise ValueError('Read outside source or transfer limit')
    if not re.fullmatch(r'/[A-Za-z0-9_./-]+', output):
        raise ValueError('Unsafe temporary path')
    check = '''seti $failed = $ata_return_status & 0x21
if $failed != 0
 exit 5
endif
if $command_status != 0
 exit 6
endif
'''
    return f'''buffersize 512
setreadpio
ata28cmd 0 0 0 0 0 0xa0 0xec
{check}wordflipbuffer 0 512
{ata_text_check('serial',20,20,serial)}{ata_text_check('model',54,40,model)}{ata_text_check('firmware',46,8,firmware)}wordflipbuffer 0 512
seti $capacity = buffer 200 qw
if $capacity != {sectors}
 exit 7
endif
seti $sectorword = buffer 212 w
seti $validsector = $sectorword & 0xc000
if $validsector = 0x4000
 seti $longsector = $sectorword & 0x1000
 if $longsector != 0
  exit 8
 endif
endif
buffersize {count*512}
clearbuffer
setreaddma
generaltimeout {timeout_seconds*1000000}
ata48cmd 0 {count} {(lba>>32)&65535} {(lba>>16)&65535} {lba&65535} 0xe0 0x25
{check}if $data_transferred != {count*512}
 exit 9
endif
sets $output = "{output}"
writebuffer $output 0 0 {count*512}
echo "DC_READ_OK"
exit 0
'''


class AHCI:
    def __init__(self, engine, qualification, identity, timeout_seconds=10):
        self.identity = identity
        if type(timeout_seconds) is not int or not 1 <= timeout_seconds <= 60:
            raise ValueError('Read timeout must be 1..60 seconds')
        self.timeout_seconds = timeout_seconds
        for key, size in [('serial',20), ('model',40), ('firmware',8)]:
            checked_text(identity[key], size)
        if type(identity['bytes']) is not int or identity['bytes'] <= 0 or identity['bytes'] % 512 or identity['bytes'] > (1<<48)*512:
            raise ValueError('Invalid 512-byte source size')
        self.engine = Path(engine).resolve()
        report = json.loads(Path(qualification).read_text())
        if report.get('directAdapter') != 1 or digest(self.engine.read_bytes()) != report['binarySHA256']:
            raise ValueError('Engine is not the exact qualified adapter binary')
        self.topology = guard_controller(identity['pci'], identity['port'])

    def validate(self):
        self._transfer(0,512,identify_only=True)

    def read(self, offset, count):
        return self._transfer(offset,count)

    def _transfer(self, offset, count, identify_only=False):
        if type(offset) is not int or type(count) is not int or offset % 512 or count % 512:
            raise ValueError('Unaligned source read')
        if guard_controller(self.identity['pci'], self.identity['port']) != self.topology:
            raise ValueError('Controller topology changed')
        with tempfile.TemporaryDirectory(prefix='dc_ahci_', dir='/tmp') as tmp:
            output, script = Path(tmp)/'data.bin', Path(tmp)/'read.osc'
            content = read_script(self.identity, offset//512, count//512, str(output), self.timeout_seconds)
            if identify_only:
                prefix, separator, _ = content.partition('\nbuffersize 512\nclearbuffer')
                if not separator: raise ValueError('Invalid generated identify script')
                content = prefix+'\necho "DC_IDENTITY_OK"\nexit 0\n'
            script.write_text(content)
            env = dict(os.environ, DC_AHCI_PCI=self.identity['pci'],
                       DC_AHCI_PORT=str(self.identity['port']), DC_AHCI_SERIAL=self.identity['serial'])
            # Engine handles ATA timeout and normal cleanup. Do not SIGKILL active
            # DMA: that can leave command pointers referencing freed host memory.
            result = subprocess.run([str(self.engine), '--tool', '--ahci', '--file', str(script)],
                                    input='', capture_output=True, text=True, env=env,
                                    start_new_session=True)
            if result.returncode == 7 or 'DC_IDENTITY_CHANGED' in result.stderr:
                raise ValueError('ATA source identity changed')
            marker = 'DC_IDENTITY_OK' if identify_only else 'DC_READ_OK'
            if result.returncode or marker not in result.stdout or (not identify_only and not output.exists()):
                raise OSError('AHCI read failed: ' + result.stderr[-1000:])
            if identify_only: return b''
            data = output.read_bytes()
            if len(data) != count:
                raise OSError('Short DMA transfer')
            return data


def restore_progress(destination, identity, repair_tail=True, strategy=None):
    destination = Path(destination)
    journal = Path(str(destination)+'.dc-ahci.jsonl')
    stat = destination.stat()
    if not file_stat.S_ISREG(stat.st_mode) or not journal.is_file() or journal.is_symlink():
        raise ValueError('Resume requires a regular image and journal, never a raw disk')
    size = identity.get('bytes')
    if type(size) is not int or size <= 0 or size % 512 or size > (1<<48)*512:
        raise ValueError('Invalid source capacity in map')
    position, bad = 0, 0
    with journal.open('rb') as source, destination.open('rb') as image:
        first = source.readline(65536)
        header = json.loads(first)
        expected = {'schema': 1, 'identity': identity, 'destination': str(destination.resolve()),
                    'device': stat.st_dev, 'inode': stat.st_ino}
        if strategy: expected['strategy'] = strategy
        if header != expected:
            raise ValueError('Resume source/destination identity mismatch')
        valid = len(first)
        while True:
            line = source.readline(CHUNK)
            if not line: break
            if not line.endswith(b'\n'):
                if source.read(1): raise ValueError('Journal row exceeds limit')
                break
            row = json.loads(line)
            if row.get('phase', 'sector-checked') not in ('sector-checked', 'deferred'):
                raise ValueError('Unknown rescue phase')
            length = row['length']
            if type(length) is not int or row['offset'] != position or not 0 < length <= CHUNK or length % 512 or position+length > identity['bytes']:
                raise ValueError('Map exceeds source or is discontinuous')
            if digest(image.read(length)) != row['sha256']:
                raise ValueError('Previous image checksum mismatch')
            end = position
            for first, size in row['bad']:
                if type(first) is not int or type(size) is not int or first < end or first % 512 or size <= 0 or size % 512 or first+size > position+length:
                    raise ValueError('Bad-sector map is invalid')
                end = first+size
                bad += size
            position += length
            valid += len(line)
    # No mutation before all identities, map entries and checksums pass.
    if repair_tail:
        with journal.open('r+b') as stream:
            stream.truncate(valid)
            stream.flush(); os.fsync(stream.fileno())
        with destination.open('r+b') as stream:
            stream.truncate(position)
            stream.flush(); os.fsync(stream.fileno())
    return position, bad


class CancelledRead(Exception):
    pass


def recover_sectors(source, position, data, ranges, retries, cancel, cycle, reverse):
    failed = []
    ordered = reversed(ranges) if reverse else ranges
    for first, size in ordered:
        sectors = range(first, first+size, 512)
        for sector in reversed(sectors) if reverse else sectors:
            if cancel(): raise CancelledRead()
            for attempt in range(retries+1):
                try:
                    value = source.read(sector, 512)
                    if len(value) != 512: raise OSError('Short sector')
                    data[sector-position:sector-position+512] = value
                    break
                except OSError:
                    if cycle and attempt < retries: cycle()
            else:
                failed.append(sector)
    # Map order is ascending even when the actual read order is reversed.
    bad = []
    for sector in sorted(failed):
        if bad and bad[-1][0]+bad[-1][1] == sector: bad[-1][1] += 512
        else: bad.append([sector, 512])
    return bytes(data), bad


def rescue(source, destination, resume=False, retries=1, cancel=lambda: False, cycle=None,
           on_progress=lambda _: None, fast_pass=False, retry_from=None, reverse=False):
    if type(retries) is not int or not 0 <= retries <= 5:
        raise ValueError('Retries must be 0..5')
    if fast_pass and retry_from:
        raise ValueError('Choose fast pass or targeted retry, not both')
    identity = source.identity
    # Probe actual ATA identity before creating/truncating any output or journal.
    if hasattr(source,'validate'): source.validate()
    destination = Path(destination).resolve()
    journal = Path(str(destination)+'.dc-ahci.jsonl')
    base = None
    strategy = None
    if retry_from:
        base = Path(retry_from).resolve()
        if base == destination or (destination.exists() and os.path.samefile(base, destination)):
            raise ValueError('Targeted retry requires a separate destination image')
        # A damaged/changed base is refused before output mutation. Never truncate it.
        base_journal = Path(str(base)+'.dc-ahci.jsonl')
        with base_journal.open('rb') as stream:
            base_header = json.loads(stream.readline(65536))
            stream.seek(0)
            base_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
        covered, _ = restore_progress(base, identity, repair_tail=False,
                                     strategy=base_header.get('strategy'))
        if covered != identity['bytes']:
            raise ValueError('Finish/resume the base pass before targeted retry')
        strategy = {'retryBase':str(base), 'mapSHA256':base_hash, 'reverse':bool(reverse)}
    elif fast_pass:
        strategy = {'fastPass':True}
    elif reverse:
        strategy = {'reverse':True}
    if resume:
        position, bad_bytes = restore_progress(destination, identity, strategy=strategy)
    else:
        position, bad_bytes = 0, 0
        if destination.exists() or journal.exists():
            raise ValueError('Destination/map already exists; use resume or another path')
    if shutil.disk_usage(destination.parent).free < identity['bytes']-position+CHUNK:
        raise OSError('Insufficient destination space')
    if not resume:
        with destination.open('xb') as image:
            image.flush(); os.fsync(image.fileno())
        stat = destination.stat()
        with journal.open('x') as progress:
            header = {'schema':1, 'identity':identity, 'destination':str(destination),
                      'device':stat.st_dev, 'inode':stat.st_ino}
            if strategy: header['strategy'] = strategy
            progress.write(json.dumps(header)+'\n')
            progress.flush(); os.fsync(progress.fileno())
        if os.name == 'posix':
            fd = os.open(destination.parent, os.O_RDONLY)
            try: os.fsync(fd)
            finally: os.close(fd)
    with contextlib.ExitStack() as stack:
        image = stack.enter_context(destination.open('r+b'))
        progress = stack.enter_context(journal.open('a'))
        if base:
            base_image = stack.enter_context(base.open('rb'))
            base_map = stack.enter_context(Path(str(base)+'.dc-ahci.jsonl').open('rb'))
            header = json.loads(base_map.readline(65536))
            stat = os.fstat(base_image.fileno())
            expected = {'schema':1, 'identity':identity, 'destination':str(base),
                        'device':stat.st_dev, 'inode':stat.st_ino}
            if base_header.get('strategy'): expected['strategy'] = base_header['strategy']
            if header != expected:
                raise ValueError('Base image identity changed')
            while base_image.tell() < position:
                row = json.loads(base_map.readline(CHUNK))
                base_image.seek(row['length'], 1)
        image.seek(position)
        while position < identity['bytes'] and not cancel():
            length = min(CHUNK, identity['bytes']-position)
            bad = []
            phase = 'sector-checked'
            try:
                if base:
                    row = json.loads(base_map.readline(CHUNK))
                    if row['offset'] != position or row['length'] != length:
                        raise ValueError('Base chunk layout changed')
                    data = bytearray(base_image.read(length))
                    if digest(data) != row['sha256']:
                        raise ValueError('Base image changed during targeted retry')
                    data, bad = recover_sectors(source, position, data, row['bad'],
                                                retries, cancel, cycle, reverse)
                else:
                    try:
                        data = source.read(position, length)
                        if len(data) != length: raise OSError('Short chunk')
                    except OSError:
                        if fast_pass:
                            data, bad, phase = bytes(length), [[position, length]], 'deferred'
                        else:
                            data, bad = recover_sectors(source, position, bytearray(length),
                                                        [[position, length]], retries, cancel, cycle, reverse)
            except CancelledRead:
                return {'cancelled':True, 'bytes':position, 'badBytes':bad_bytes}
            if image.write(data) != length: raise OSError('Short output write')
            image.flush(); os.fsync(image.fileno())
            image.seek(position)
            if digest(image.read(length)) != digest(data): raise OSError('Output readback mismatch')
            row = {'offset':position,'length':length,'sha256':digest(data),'bad':bad,'phase':phase}
            progress.write(json.dumps(row)+'\n')
            progress.flush(); os.fsync(progress.fileno())
            position += length
            bad_bytes += sum(size for _,size in bad)
            on_progress({'bytes':position,'total':identity['bytes'],'badBytes':bad_bytes})
        if base:
            base_map.seek(0)
            if hashlib.file_digest(base_map, 'sha256').hexdigest() != base_hash:
                raise ValueError('Base map changed during targeted retry')
    return {'cancelled':position < identity['bytes'], 'bytes':position, 'badBytes':bad_bytes,
            'unreadableOrDeferredBytes':bad_bytes,
            'fastPass':bool(fast_pass), 'targetedRetry':bool(base),
            'contentVerified':False, 'status':'read-complete' if not bad_bytes else 'incomplete'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', required=True)
    parser.add_argument('--qualification', required=True)
    parser.add_argument('--identity', required=True, help='JSON: pci, port, serial, model, firmware, bytes')
    parser.add_argument('--output', required=True)
    parser.add_argument('--relay', help='Optional isolated source-power relay JSON')
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--retries', type=int, default=1)
    parser.add_argument('--timeout-seconds', type=int, default=10, help='ATA command timeout 1..60; not a process-kill deadline')
    parser.add_argument('--fast-pass', action='store_true', help='Defer failed chunks; gather readable chunks first')
    parser.add_argument('--retry-from', help='Verified complete earlier image/map; recover only its missing sectors into a NEW image')
    parser.add_argument('--reverse', action='store_true', help='Read failed sectors in reverse order within each chunk')
    parser.add_argument('--export-map', help='Write a new ddrescue-format map after this pass')
    args = parser.parse_args()
    if os.name != 'posix' or os.geteuid() != 0: raise ValueError('Requires root in a live Linux environment')
    stopped = [False]
    # Child session is isolated from terminal cancellation; parent stops after DMA cleanup.
    signal.signal(signal.SIGINT, lambda *_: stopped.__setitem__(0, True))
    signal.signal(signal.SIGTERM, lambda *_: stopped.__setitem__(0, True))
    source = AHCI(args.engine, args.qualification, json.loads(Path(args.identity).read_text()), args.timeout_seconds)
    cycle = None
    if args.relay:
        from relay_power import RelayPower
        relay = RelayPower.connect(json.loads(Path(args.relay).read_text()))
        def cycle():
            if relay.cycles < relay.max_cycles:
                relay.cycle()
    import sys
    last_progress = [0.0]
    def progress(row):
        if time.monotonic()-last_progress[0] >= 10 or row['bytes'] == row['total']:
            print(json.dumps(row),file=sys.stderr,flush=True)
            last_progress[0] = time.monotonic()
    result = rescue(source,args.output,args.resume,args.retries,lambda:stopped[0],cycle,progress,
                    args.fast_pass,args.retry_from,args.reverse)
    if args.export_map:
        from rescue_map import export_map
        export_map(args.output,args.export_map)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
