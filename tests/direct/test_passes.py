import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'scripts'))
import direct_rescue as dr
from rescue_map import export_map
from test_direct import Source


class PassTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.base = self.root/'base.img'
        self.final = self.root/'final.img'
        self.data = bytes(range(256))*6144
        self.source = Source(self.data)
        normal = self.source.read
        def read(offset, count):
            if count > 512 and offset == dr.CHUNK:
                self.source.calls.append((offset, count))
                raise OSError('Synthetic slow/unreadable chunk')
            return normal(offset, count)
        self.source.read = read
    def tearDown(self): self.tmp.cleanup()
    def fast(self):
        result = dr.rescue(self.source, self.base, fast_pass=True)
        self.assertEqual(result['badBytes'], dr.CHUNK)
        self.assertEqual(len(self.source.calls), 3)
        self.assertEqual(self.base.read_bytes()[:dr.CHUNK], self.data[:dr.CHUNK])
        self.assertEqual(self.base.read_bytes()[2*dr.CHUNK:], self.data[2*dr.CHUNK:])
        return result
    def test_fast_then_targeted_reverse_sha_base_preserved(self):
        self.fast()
        original = self.base.read_bytes()
        journal = Path(str(self.base)+'.dc-ahci.jsonl').read_bytes()
        self.source.calls.clear()
        result = dr.rescue(self.source, self.final, retry_from=self.base, reverse=True)
        self.assertEqual(result['badBytes'], 0)
        self.assertEqual(hashlib.sha256(self.final.read_bytes()).digest(), hashlib.sha256(self.data).digest())
        offsets = [x[0] for x in self.source.calls]
        self.assertEqual(offsets, list(range(2*dr.CHUNK-512, dr.CHUNK-1, -512)))
        self.assertEqual(self.base.read_bytes(), original)
        self.assertEqual(Path(str(self.base)+'.dc-ahci.jsonl').read_bytes(), journal)
        self.assertFalse(result['contentVerified'])
    def test_retry_cancel_resume_and_changed_strategy(self):
        self.fast(); self.source.calls.clear()
        result = dr.rescue(self.source, self.final, retry_from=self.base,
                           cancel=lambda:len(self.source.calls)>=5)
        self.assertTrue(result['cancelled']); self.assertEqual(result['bytes'], dr.CHUNK)
        before = self.final.read_bytes()
        with self.assertRaises(ValueError):
            dr.rescue(self.source, self.final, resume=True, retry_from=self.base, reverse=True)
        self.assertEqual(before, self.final.read_bytes())
        dr.rescue(self.source, self.final, resume=True, retry_from=self.base)
        self.assertEqual(self.final.read_bytes(), self.data)
    def test_base_corruption_refuses_new_output(self):
        self.fast()
        with self.base.open('r+b') as f: f.write(b'BAD')
        with self.assertRaises(ValueError):dr.rescue(self.source, self.final, retry_from=self.base)
        self.assertFalse(self.final.exists())
    def test_incomplete_base_and_same_destination_refused(self):
        dr.rescue(self.source,self.base,fast_pass=True,cancel=lambda:len(self.source.calls)>=1)
        with self.assertRaises(ValueError):dr.rescue(self.source,self.final,retry_from=self.base)
        self.assertFalse(self.final.exists())
        with self.assertRaises(ValueError):dr.rescue(self.source,self.base,retry_from=self.base)
        if os.name == 'posix':
            os.link(self.base,self.final)
            with self.assertRaises(ValueError):dr.rescue(self.source,self.final,True,retry_from=self.base)
    def test_export_deferred_success_and_unattempted(self):
        dr.rescue(self.source,self.base,fast_pass=True,cancel=lambda:len(self.source.calls)>=2)
        image_before = self.base.read_bytes()
        output = self.root/'domain.map'
        export_map(self.base, output)
        lines = [x.split() for x in output.read_text().splitlines() if not x.startswith('#')]
        self.assertEqual(lines[1:], [['0x0','0x80000','+'], ['0x80000','0x100000','?']])
        self.assertEqual(self.base.read_bytes(), image_before)
        with self.assertRaises(ValueError):export_map(self.base, output)
    def test_export_confirmed_failure(self):
        source=Source(self.data[:4096]);source.fail={1024}
        dr.rescue(source,self.base,retries=0,reverse=True)
        export_map(self.base,self.root/'failed.map')
        self.assertIn('0x400 0x200 -', (self.root/'failed.map').read_text())
        self.assertEqual([x[0] for x in source.calls[1:]],list(range(3584,-1,-512)))
    def test_corrupt_map_export_refused(self):
        self.fast()
        journal = Path(str(self.base)+'.dc-ahci.jsonl')
        lines=journal.read_text().splitlines();row=json.loads(lines[1]);row['bad']=[[0,dr.CHUNK+512]]
        lines[1]=json.dumps(row);journal.write_text('\n'.join(lines)+'\n')
        before=journal.read_bytes()
        with self.assertRaises(ValueError):export_map(self.base,self.root/'bad.map')
        self.assertFalse((self.root/'bad.map').exists());self.assertEqual(journal.read_bytes(),before)
    def test_timeout_allowlist_bounds(self):
        for value in [0,61,-1,True,1.5]:
            with self.assertRaises(ValueError):dr.read_script(self.source.identity,0,1,'/tmp/read.bin',value)
        self.assertIn('generaltimeout 2000000',dr.read_script(self.source.identity,0,1,'/tmp/read.bin',2))
    def test_base_changed_identity_and_plan_fail_before_mutation(self):
        self.fast()
        self.source.identity['serial']='OTHER'
        with self.assertRaises(ValueError):dr.rescue(self.source,self.final,retry_from=self.base)
        self.assertFalse(self.final.exists())
        with self.assertRaises(ValueError):dr.rescue(self.source,self.final,retry_from=self.base,fast_pass=True)

    @unittest.skipUnless(shutil.which('ddrescuelog'), 'GNU parser is installed by Linux CI')
    def test_export_accepted_by_gnu_parser(self):
        self.fast()
        output=self.root/'gnu.map'
        export_map(self.base,output)
        subprocess.run(['ddrescuelog','--show-status',str(output)],check=True,capture_output=True)


if __name__=='__main__': unittest.main()
