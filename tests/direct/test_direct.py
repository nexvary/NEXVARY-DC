import hashlib
import os
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
import direct_rescue as dr
from relay_power import RelayPower


class Source:
    def __init__(self,data):
        self.data=data
        self.identity={'pci':'0000:00:02.0','port':0,'serial':'DC_TEST','model':'DC FIXTURE','firmware':'1.0','bytes':len(data)}
        self.calls=[]
        self.fail=set()
    def read(self,offset,count):
        self.calls.append((offset,count))
        if count>512 and self.fail or offset in self.fail: raise OSError('Injected unreadable sector')
        return self.data[offset:offset+count]


class USB:
    def __init__(self): self.id=b'ABCDE'; self.bits=3; self.sent=[]; self.fail=None
    def ctrl_transfer(self,typ,request,value,index,data,timeout):
        if request==1: return self.id+b'\0\0'+bytes([self.bits])
        self.sent.append(bytes(data))
        if self.fail==len(self.sent): raise OSError('USB disconnected')
        bit=1<<(data[1]-1)
        self.bits=self.bits|bit if data[0]==0xff else self.bits&~bit
        return 8


class DirectTests(unittest.TestCase):
    def setUp(self): self.tmp=tempfile.TemporaryDirectory(); self.path=Path(self.tmp.name)/'image.bin'
    def tearDown(self): self.tmp.cleanup()
    def test_stream_sha_resume_cancel_crash_tail(self):
        source=Source(bytes(range(256))*8192)
        result=dr.rescue(source,self.path,cancel=lambda:len(source.calls)>0)
        self.assertTrue(result['cancelled']); self.assertEqual(result['bytes'],dr.CHUNK)
        with self.path.open('ab') as f:f.write(b'uncommitted')
        with Path(str(self.path)+'.dc-ahci.jsonl').open('ab') as f:f.write(b'{"torn"')
        result=dr.rescue(source,self.path,resume=True)
        self.assertEqual(result['bytes'],len(source.data));self.assertFalse(result['cancelled'])
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).digest(),hashlib.sha256(source.data).digest())
    def test_sector_fallback_bad_map(self):
        source=Source(bytes(range(256))*16);source.fail={1024}
        result=dr.rescue(source,self.path,retries=2)
        expected=bytearray(source.data);expected[1024:1536]=bytes(512)
        self.assertEqual(self.path.read_bytes(),expected);self.assertEqual(result['badBytes'],512)
        self.assertEqual(source.calls.count((1024,512)),3)
        row=json.loads(Path(str(self.path)+'.dc-ahci.jsonl').read_text().splitlines()[1])
        self.assertEqual(row['bad'],[[1024,512]])
    def test_corrupt_image_and_changed_identity_do_not_modify(self):
        source=Source(b'x'*1024);dr.rescue(source,self.path)
        journal=Path(str(self.path)+'.dc-ahci.jsonl'); original=journal.read_bytes()
        source.identity['serial']='CHANGED'
        with self.assertRaises(ValueError):dr.rescue(source,self.path,True)
        self.assertEqual(journal.read_bytes(),original)
        source.identity['serial']='DC_TEST';self.path.write_bytes(b'z'*1024)
        with self.assertRaises(ValueError):dr.rescue(source,self.path,True)
        self.assertEqual(self.path.read_bytes(),b'z'*1024)
    def test_bounds_corrupt_map_destination_space(self):
        source=Source(b'x'*1024)
        with patch.object(dr.shutil,'disk_usage',return_value=type('Usage',(),{'free':1})()):
            with self.assertRaises(OSError):dr.rescue(source,self.path)
        self.assertFalse(self.path.exists())
        dr.rescue(source,self.path)
        journal=Path(str(self.path)+'.dc-ahci.jsonl'); lines=journal.read_text().splitlines();row=json.loads(lines[1]);row['length']=2048;lines[1]=json.dumps(row)
        journal.write_text('\n'.join(lines)+'\n');before=journal.read_bytes()
        with self.assertRaises(ValueError):dr.rescue(source,self.path,True)
        self.assertEqual(journal.read_bytes(),before)
        for lba,count in [(-1,1),(2,1),(0,2049),(0,0)]:
            with self.assertRaises(ValueError):dr.read_script(source.identity,lba,count,'/tmp/data.bin')
    def test_read_command_allowlist_and_injection(self):
        source=Source(bytes(4096));script=dr.read_script(source.identity,3,2,'/tmp/data.bin')
        self.assertIn('ata48cmd 0 2 0 0 3 0xe0 0x25',script)
        self.assertIn('ata28cmd 0 0 0 0 0 0xa0 0xec',script)
        source.identity['serial']='evil"\nwrite'
        with self.assertRaises(ValueError):dr.read_script(source.identity,0,1,'/tmp/data.bin')
    @unittest.skipUnless(os.name == "posix", "Linux sysfs controller protection")
    def test_controller_system_and_shared_protection(self):
        root=Path(self.tmp.name);device=root/'bus/pci/devices/0000:00:02.0';device.mkdir(parents=True)
        for key,value in {'class':'0x010601','vendor':'0x8086','device':'0x2922','resource':'0 0 0\n'*5+'0xf0000000 0xf0001fff 0x200\n'}.items():(device/key).write_text(value)
        (device/'driver').symlink_to(root/'drivers/ahci');(root/'class/block').mkdir(parents=True)
        dr.guard_controller('0000:00:02.0',0,root)
        child=device/'ata/block/sda';child.mkdir(parents=True);(root/'class/block/sda').symlink_to(child)
        with self.assertRaises(ValueError):dr.guard_controller('0000:00:02.0',0,root)
    def test_relay_bounded_identity_and_restore(self):
        device=USB();relay=RelayPower(device,'ABCDE',{'5v':1,'12v':2},sleep=lambda _:None)
        relay.cycle();self.assertEqual(device.bits,3);self.assertEqual([x[:2] for x in device.sent],[b'\xfd\1',b'\xfd\2',b'\xff\1',b'\xff\2'])
        with self.assertRaises(OSError):relay.cycle()
        device=USB();device.fail=2;relay=RelayPower(device,'ABCDE',{'5v':1,'12v':2},max_cycles=2,sleep=lambda _:None)
        with self.assertRaises(OSError):relay.cycle()
        self.assertEqual(device.bits,3)
        device.id=b'WRONG'
        with self.assertRaises(ValueError):relay.cycle()
    def test_identity_change_is_fatal_not_bad_sector(self):
        source=Source(bytes(1024));source.read=lambda *_:(_ for _ in ()).throw(ValueError('changed'))
        with self.assertRaises(ValueError):dr.rescue(source,self.path)
        self.assertEqual(self.path.stat().st_size,0)


if __name__=='__main__':unittest.main()
