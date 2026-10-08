#!/usr/bin/env python3
"""Build a pinned upstream engine and probe ONLY help/version; never open disks.
No kernel driver is installed or loaded and no upstream source is linked into DC.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

UPSTREAM = 'https://github.com/ISpillMyDrink/OpenSuperClone.git'
COMMIT = '87a25d257e44337ae15f863e59b0062311a5c329'  # stable v2.5.0


def run(args, **kwargs):
    return subprocess.check_output(args, text=True, timeout=600, **kwargs)


def qualify(output, direct=False):
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    source, build = output / 'source', output / 'build'
    if source.exists() or build.exists():
        raise ValueError('Choose a new output folder; prior evidence is never replaced')
    run(['git', 'clone', '--no-checkout', UPSTREAM, str(source)])
    run(['git', '-C', str(source), 'checkout', '--detach', COMMIT])
    actual = run(['git', '-C', str(source), 'rev-parse', 'HEAD']).strip()
    if actual != COMMIT:
        raise ValueError('Upstream commit mismatch')
    license_text = (source / 'LICENSE').read_text()
    if 'Version 2, June 1991' not in license_text:
        raise ValueError('Expected GPLv2 notice absent; re-review license')
    if direct:
        from osc_adapter_patch import apply
        apply(source)
        (output / 'adapter.patch').write_text(run(['git','-C',str(source),'diff']))
    run(['cmake', '-S', str(source), '-B', str(build), '-DCMAKE_BUILD_TYPE=Release'])
    run(['cmake', '--build', str(build), '--parallel', '2'])
    executable = build / 'src/opensuperclone/opensuperclone'
    version = run([str(executable), '--version'])
    help_text = run([str(executable), '--help'])
    if 'OpenSuperClone' not in version or '2.5' not in version or '--tool' not in help_text:
        raise ValueError('Engine identity/help mismatch')
    tested = ['source_build','version','help']
    if direct:
        from direct_rescue import read_script
        identity = dict(model='QEMU HARDDISK',serial='DC_TEST_SERIAL',firmware='2.5+',bytes=2097152)
        script = output / 'read-check.osc'
        script.write_text(read_script(identity,0,2048,'/tmp/dc_read_check.bin'))
        command = [str(executable),'--tool','--check','--file',str(script)]
        # Upstream requires root even for syntax checking; --check executes no I/O.
        if os.geteuid() != 0:
            command = ['sudo','-n',*command]
        run(command)
        tested.append('generated_read_script_syntax')
    report = dict(directAdapter=1 if direct else 0, upstream=UPSTREAM, commit=actual, license='GPL-2.0',
                  binarySHA256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                  version=version, tested=tested,
                  diskAccessTested=False, directAHCITested=False, relayTested=False,
                  kernelDriverBuilt=False, kernelDriverLoaded=False,
                  linkedIntoNexvary=False, firmwareRepairCertified=False)
    (output / 'qualification.json').write_text(json.dumps(report, indent=2))
    (output / 'help.txt').write_text(help_text)
    (output / 'LICENSE').write_text(license_text)
    run(['git', '-C', str(source), 'archive', '--format=tar.gz',
         '--output=' + str(output / 'upstream-source.tar.gz'), actual])
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--direct', action='store_true', help='Build restricted AHCI adapter and retain GPL source patch')
    args = parser.parse_args()
    qualify(args.output, args.direct)
