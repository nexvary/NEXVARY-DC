#!/usr/bin/env python3
"""Export verified NEXVARY direct rescue progress to a GNU ddrescue domain map.
'+' denotes successful reads, not verified source-file integrity. '?' is deferred
or unattempted; '-' is sector-read failure. Does not mutate the image/journal.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
from direct_rescue import CHUNK, restore_progress


def export_map(image, output):
    image, output = Path(image).resolve(), Path(output)
    journal = Path(str(image)+'.dc-ahci.jsonl')
    with journal.open('rb') as stream:
        header = json.loads(stream.readline(65536))
        stream.seek(0)
        map_sha = hashlib.file_digest(stream, 'sha256').hexdigest()
    identity = header['identity']
    position, _ = restore_progress(image, identity, repair_tail=False,
                                   strategy=header.get('strategy'))
    if output.exists(): raise ValueError('Export destination already exists')
    with journal.open('rb') as source, output.open('x') as target:
        try:
            source.readline(65536)
            target.write('# NEXVARY read map; successful reads do not establish file integrity\n')
            target.write(f'0x{position:X} ? 1\n')
            pending = None
            def emit(offset, size, status):
                nonlocal pending
                if not size: return
                if pending and pending[2] == status and pending[0]+pending[1] == offset:
                    pending[1] += size
                else:
                    if pending: target.write(f'0x{pending[0]:X} 0x{pending[1]:X} {pending[2]}\n')
                    pending = [offset, size, status]
            offset = 0
            while offset < position:
                row = json.loads(source.readline(CHUNK))
                end = row['offset']+row['length']
                for first, length in row['bad']:
                    emit(offset, first-offset, '+')
                    emit(first, length, '?' if row.get('phase') == 'deferred' else '-')
                    offset = first+length
                emit(offset, end-offset, '+')
                offset = end
            emit(position, identity['bytes']-position, '?')
            if pending: target.write(f'0x{pending[0]:X} 0x{pending[1]:X} {pending[2]}\n')
            source.seek(0)
            if hashlib.file_digest(source, 'sha256').hexdigest() != map_sha:
                raise ValueError('Journal changed during export')
            target.flush(); os.fsync(target.fileno())
        except BaseException:
            target.close()
            output.unlink()
            raise
    return {'bytes':position, 'total':identity['bytes'], 'sourceContentVerified':False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    print(json.dumps(export_map(args.image, args.output)))
