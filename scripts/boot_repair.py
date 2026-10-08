#!/usr/bin/env python3
"""Offline x86 GRUB repair. Run from Linux live media, not the installed OS.

Uses the live distribution's GRUB tools; never formats or deletes partitions.
Mounted ext4 root and optional FAT ESP must be explicitly selected. Plans are
revalidated before writes. Secure Boot and encrypted/LVM roots are unsupported.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import time


def run(args):
    p = subprocess.run(args, capture_output=True, text=True, timeout=180,
                       env={**os.environ, 'PATH': '/usr/sbin:/usr/bin:/sbin:/bin', 'LC_ALL': 'C'})
    if p.returncode:
        raise RuntimeError('Command failed: ' + args[0] + '\n' + p.stderr[-3000:])
    return p.stdout


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def flatten(nodes):
    for node in nodes:
        yield node
        yield from flatten(node.get('children', []))


def inventory():
    nodes = list(flatten(json.loads(run(['lsblk', '--json', '--bytes', '--paths',
        '--output', 'PATH,TYPE,SIZE,RO,LOG-SEC,SERIAL,WWN,UUID,PARTUUID,PARTTYPE,PKNAME,FSTYPE,PTTYPE,MOUNTPOINTS']))['blockdevices']))
    for node in nodes:
        if node['type'] in ('disk', 'loop', 'part'):
            # Minimal live RAM environments may have no udev property cache.
            try:
                properties = dict(line.split('=', 1) for line in run(['blkid', '-p', '-o', 'export', node['path']]).splitlines() if '=' in line)
            except RuntimeError:
                properties = {}
            for key, prop in [('uuid', 'UUID'), ('partuuid', 'PART_ENTRY_UUID'), ('parttype', 'PART_ENTRY_TYPE'), ('pttype', 'PTTYPE')]:
                if not node.get(key):
                    node[key] = properties.get(prop)
            node['ptuuid'] = properties.get('PTUUID')
        if node['type'] == 'part':
            node['partn'] = int((pathlib.Path('/sys/class/block') / pathlib.Path(node['path']).name / 'partition').read_text())
    return nodes


def mounted(path, filesystem):
    path = pathlib.Path(path).resolve(strict=True)
    data = json.loads(run(['findmnt', '--json', '--mountpoint', str(path),
                          '--output', 'TARGET,SOURCE,FSTYPE,OPTIONS']))['filesystems'][0]
    if data['fstype'] not in filesystem or pathlib.Path(data['target']).resolve() != path:
        raise ValueError('Select an exact mounted partition of the supported filesystem.')
    if 'rw' not in data['options'].split(','):
        raise ValueError('Selected repair mount must be writable. The plan itself does not write.')
    return path, os.path.realpath(data['source'])


def regular(root, relative):
    path = root / relative
    if any(p.is_symlink() for p in [path, *path.parents] if p != root and root in p.parents):
        raise ValueError('Symlink in boot path: ' + str(path))
    if path.is_symlink() or not path.is_file() or root not in path.resolve().parents:
        raise ValueError('Missing or unsafe file: ' + str(path))
    return path


def secure_boot():
    for p in pathlib.Path('/sys/firmware/efi/efivars').glob('SecureBoot-*'):
        if p.read_bytes()[4:5] == b'\x01':
            return True
    return False


def firmware_order(output):
    m = re.search(r'^BootOrder:\s*([0-9A-Fa-f,]+)$', output, re.M)
    if not m:
        raise ValueError('Cannot read firmware boot order.')
    return m.group(1).upper().split(',')


def on_disk(source, disk, nodes):
    """Follow device-mapper parents too; a live system root is never writable."""
    source = os.path.realpath(source)
    seen = set()
    while source and source not in seen:
        if source == disk:
            return True
        seen.add(source)
        source = nodes.get(source, {}).get('pkname')
    return False


def plan(root, esp, disk, mode, backup):
    if mode not in ('bios', 'uefi'):
        raise ValueError('Unsupported boot mode.')
    root, source = mounted(root, ('ext4',))
    # A live recovery session is required; never mutate the running root disk.
    running = os.path.realpath(run(['findmnt', '-n', '-o', 'SOURCE', '/']).strip())
    nodes = inventory()
    by_path = {n['path']: n for n in nodes}
    target = by_path.get(os.path.realpath(disk))
    part = by_path.get(source)
    if not target or target['type'] not in ('disk', 'loop') or target['ro']:
        raise ValueError('Target must be a writable whole disk.')
    if not part or part['type'] != 'part' or not part.get('uuid') or not part.get('partuuid'):
        raise ValueError('Root must be a plain partition with stable UUID and PARTUUID.')
    if part.get('pkname') != target['path']:
        raise ValueError('Root partition is not on the selected disk.')
    if running in by_path and by_path[running]['type'] not in ('part', 'disk', 'loop'):
        raise ValueError('Running root uses a complex block layout; use a RAM-based live environment.')
    if running == source or on_disk(running, target['path'], by_path):
        raise ValueError('Boot repair requires live media; the running system disk is protected.')
    if os.uname().machine != 'x86_64':
        raise ValueError('Only x86_64 live Linux is supported.')
    regular(root, 'etc/os-release')
    config = regular(root, 'boot/grub/grub.cfg')
    # Separate /boot, Btrfs subvolumes, device mapper and custom layouts need
    # a dedicated implementation; never silently write the wrong mount.
    mounts = json.loads(run(['findmnt', '--json', '--output', 'TARGET,SOURCE']))['filesystems']
    for m in flatten(mounts):
        if str(m.get('target', '')) == str(root / 'boot'):
            raise ValueError('Separate /boot is not supported by this repair path.')
    if config.stat().st_size > 16 * 1024 * 1024:
        raise ValueError('GRUB configuration is too large.')
    run(['grub-script-check', str(config)])
    if not any((root / 'boot').glob('vmlinuz*')):
        raise ValueError('No installed Linux kernel found.')
    backup = pathlib.Path(backup).resolve(strict=True)
    backup_mount = json.loads(run(['findmnt', '--json', '--target', str(backup),
                                 '--output', 'SOURCE']))['filesystems'][0]['source']
    if on_disk(backup_mount, target['path'], by_path):
        raise ValueError('Backup must be on a different disk or live RAM filesystem.')
    if root == backup or root in backup.parents or not backup.is_dir():
        raise ValueError('Backup must be outside the repaired root.')
    for tool in ('grub-install', 'grub-script-check'):
        if not shutil.which(tool, path='/usr/sbin:/usr/bin:/sbin:/bin'):
            raise ValueError('Live environment lacks ' + tool)
    result = {'version': 1, 'mode': mode, 'root': str(root), 'rootDevice': source,
              'disk': target, 'rootIdentity': part, 'backup': str(backup),
              'configSha256': sha(config), 'esp': '', 'espIdentity': None,
              'firmware': '', 'created': int(time.time()), 'secureBoot': False}
    if mode == 'bios':
        if target.get('pttype') != 'dos' or target.get('log-sec') != 512:
            raise ValueError('BIOS repair currently supports MBR disks only.')
        # Preserve partition table and all embedding bytes that GRUB may use.
        with open(target['path'], 'rb') as f:
            head = f.read(1024 * 1024)
        if len(head) != 1024 * 1024 or head[510:512] != b'\x55\xaa':
            raise ValueError('Invalid MBR.')
        starts = [int.from_bytes(head[p+8:p+12], 'little') for p in range(446, 510, 16) if head[p+4]]
        if not starts or min(starts) < 2048:
            raise ValueError('BIOS repair requires a 1 MiB embedding gap.')
        result['headSha256'] = hashlib.sha256(head).hexdigest()
    else:
        if not pathlib.Path('/sys/firmware/efi').exists() or secure_boot():
            raise ValueError('Boot live media in UEFI mode with Secure Boot disabled for this path.')
        esp, esp_source = mounted(esp, ('vfat',))
        e = by_path.get(esp_source)
        if not e or e.get('pkname') != target['path'] or not e.get('uuid') or not e.get('partuuid'):
            raise ValueError('ESP must be a partition on the selected disk.')
        if str(e.get('parttype', '')).lower() not in ('c12a7328-f81f-11d2-ba4b-00a0c93ec93b', '0xef'):
            raise ValueError('Selected FAT partition is not an EFI System Partition.')
        result.update(esp=str(esp), espIdentity=e, firmware=run(['efibootmgr', '-v']))
        firmware_order(result['firmware'])
        windows = esp / 'EFI/Microsoft/Boot/bootmgfw.efi'
        if windows.exists():
            regular(esp, 'EFI/Microsoft/Boot/bootmgfw.efi')
            if not re.fullmatch(r'[0-9A-Fa-f-]+', e['uuid']):
                raise ValueError('Invalid ESP UUID.')
            result['windowsLoaderSha256'] = sha(windows)
    stable = {k: v for k, v in result.items() if k != 'created'}
    result['token'] = hashlib.sha256(json.dumps(stable, sort_keys=True).encode()).hexdigest()
    return result


def snapshot(root, esp, destination):
    """Copy only regular files/directories; reject links and special files."""
    records = []
    for label, tree in [('grub', root / 'boot/grub'), ('efi', esp / 'EFI' if esp else None)]:
        if tree is None or not tree.exists():
            continue
        for path in sorted(tree.rglob('*')):
            if path.is_symlink() or not (path.is_dir() or path.is_file()):
                raise ValueError('Unsafe boot tree entry: ' + str(path))
            if path.is_file():
                relative = pathlib.Path(label) / path.relative_to(tree)
                records.append((path, relative, path.stat().st_size))
    required = sum(r[2] for r in records) + 16 * 1024 * 1024
    if shutil.disk_usage(destination).free < required:
        raise ValueError('Insufficient backup space; no repair writes made.')
    manifest = []
    for source, relative, length in records:
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        with open(target, 'r+b') as f:
            os.fsync(f.fileno())
        digest = sha(source)
        if sha(target) != digest:
            raise ValueError('Backup verification failed; no repair writes made.')
        manifest.append({'path': str(relative), 'bytes': length, 'sha256': digest})
    return manifest


def apply(request, confirmation):
    if getattr(os, 'geteuid', lambda: -1)() != 0:
        raise ValueError('Repair requires root in a live Linux session.')
    fresh = plan(request['root'], request['esp'], request['disk']['path'], request['mode'], request['backup'])
    if fresh['token'] != request['token'] or time.time() - request['created'] > 180:
        raise ValueError('Plan expired or disk/configuration/firmware identity changed; inspect again.')
    if confirmation != 'REPAIR ' + fresh['disk']['path']:
        raise ValueError('Type the exact repair confirmation.')
    root = pathlib.Path(fresh['root'])
    esp = pathlib.Path(fresh['esp']) if fresh['esp'] else None
    destination = pathlib.Path(tempfile.mkdtemp(prefix='NEXVARY-boot-', dir=fresh['backup']))
    destination.chmod(0o700)
    records = snapshot(root, esp, destination)
    if fresh['mode'] == 'bios':
        with open(fresh['disk']['path'], 'rb') as f:
            head = f.read(1024 * 1024)
        if hashlib.sha256(head).hexdigest() != fresh['headSha256']:
            raise ValueError('Disk boot area changed.')
        (destination / 'bios-first-MiB.bin').write_bytes(head)
    (destination / 'plan.json').write_text(json.dumps(fresh, indent=2))
    (destination / 'manifest.json').write_text(json.dumps(records, indent=2))
    os.sync()
    # Revalidate immediately before first target write, after backup creation.
    if plan(fresh['root'], fresh['esp'], fresh['disk']['path'], fresh['mode'], fresh['backup'])['token'] != fresh['token']:
        raise ValueError('Source changed during backup; inspect again.')
    try:
        command = ['grub-install', '--boot-directory=' + str(root / 'boot')]
        if fresh['mode'] == 'bios':
            command += ['--target=i386-pc', fresh['disk']['path']]
        else:
            command += ['--target=x86_64-efi', '--efi-directory=' + str(esp),
                        '--bootloader-id=NEXVARY', '--no-nvram']
        run(command)
        config = root / 'boot/grub/grub.cfg'
        if fresh.get('windowsLoaderSha256'):
            # Append a deterministic entry; preserve the distribution's existing
            # Linux entries and avoid enabling global os-prober side effects.
            text = config.read_text()
            if '# NEXVARY Windows UEFI entry' not in text:
                text += "\n# NEXVARY Windows UEFI entry\nmenuentry 'Windows Boot Manager (NEXVARY)' {\n  insmod part_gpt\n  insmod part_msdos\n  insmod fat\n  insmod chain\n  search --no-floppy --fs-uuid --set=root " + fresh['espIdentity']['uuid'] + "\n  chainloader /EFI/Microsoft/Boot/bootmgfw.efi\n}\n"
                config.write_text(text)
        run(['grub-script-check', str(config)])
        if esp:
            regular(esp, 'EFI/NEXVARY/grubx64.efi')
            if fresh.get('windowsLoaderSha256') and sha(esp / 'EFI/Microsoft/Boot/bootmgfw.efi') != fresh['windowsLoaderSha256']:
                raise RuntimeError('Windows loader changed unexpectedly.')
            before = run(['efibootmgr', '-v'])
            existing = re.findall(r'^Boot([0-9A-Fa-f]{4})', before, re.M)
            run(['efibootmgr', '--create', '--disk', fresh['disk']['path'], '--part', str(fresh['espIdentity']['partn']),
                 '--label', 'NEXVARY Linux', '--loader', '\\EFI\\NEXVARY\\grubx64.efi'])
            after = run(['efibootmgr', '-v'])
            new = set(re.findall(r'^Boot([0-9A-Fa-f]{4})', after, re.M)) - set(existing)
            if len(new) != 1:
                raise RuntimeError('Firmware did not create exactly one boot entry.')
            entry = next(iter(new)).upper()
            order = [entry] + [n for n in firmware_order(fresh['firmware']) if n != entry]
            run(['efibootmgr', '--bootorder', ','.join(order)])
            if firmware_order(run(['efibootmgr', '-v'])) != order:
                raise RuntimeError('Firmware boot order verification failed.')
        os.sync()
        result = {'status': 'completed', 'operation': 'boot_repair', 'backup': str(destination),
                  'mode': fresh['mode'], 'configSha256': sha(config), 'bootTestedOnThisDevice': False,
                  'secureBoot': False, 'message': 'GRUB installed; configuration checked. Reboot to verify both systems. Backup retained.'}
    except Exception as error:
        # Retain exact pre-repair files and firmware metadata. Do not blindly
        # restore raw boot sectors after a failed external command.
        result = {'status': 'error', 'operation': 'boot_repair', 'backup': str(destination),
                  'message': str(error), 'requiresReview': True}
    (destination / 'result.json').write_text(json.dumps(result, indent=2))
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--request', required=True, help='JSON request file, action plan/apply')
    args = p.parse_args()
    try:
        request = json.loads(pathlib.Path(args.request).read_text())
        if request['action'] == 'plan':
            result = plan(request['root'], request.get('esp', ''), request['disk'], request['mode'], request['backup'])
            result = {'status': 'completed', 'operation': 'boot_repair_plan', 'plan': result,
                      'confirmation': 'REPAIR ' + result['disk']['path']}
        elif request['action'] == 'apply':
            result = apply(request['plan'], request['confirmation'])
        else:
            raise ValueError('Unsupported action.')
    except Exception as error:
        result = {'status': 'error', 'operation': 'boot_repair', 'message': str(error)}
    print(json.dumps(result))
    return 0 if result['status'] == 'completed' else 1


if __name__ == '__main__':
    sys.exit(main())
