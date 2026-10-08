import importlib.util
import pathlib
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('repair', pathlib.Path(__file__).parents[2] / 'scripts/boot_repair.py')
repair = importlib.util.module_from_spec(spec)
spec.loader.exec_module(repair)


class RepairTests(unittest.TestCase):
    def test_firmware_order(self):
        self.assertEqual(repair.firmware_order('BootCurrent: 0001\nBootOrder: 0002,0001\n'), ['0002', '0001'])
        with self.assertRaises(ValueError):
            repair.firmware_order('garbage')

    def test_snapshot_hash_and_links(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d) / 'root'
            (root / 'boot/grub').mkdir(parents=True)
            (root / 'boot/grub/grub.cfg').write_text('original')
            out = pathlib.Path(d) / 'backup'
            out.mkdir()
            rows = repair.snapshot(root, None, out)
            self.assertEqual(rows[0]['sha256'], repair.sha(out / 'grub/grub.cfg'))
            (root / 'boot/grub/escape').symlink_to('/etc/passwd')
            with self.assertRaises(ValueError):
                repair.snapshot(root, None, out)

    def test_no_space_before_writes(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            (root / 'boot/grub').mkdir(parents=True)
            (root / 'boot/grub/grub.cfg').write_text('source')
            out = root / 'backup'
            out.mkdir()
            with patch.object(repair.shutil, 'disk_usage', return_value=type('Usage', (), {'free': 0})()):
                with self.assertRaises(ValueError):
                    repair.snapshot(root, None, out)
            self.assertEqual(list(out.iterdir()), [])

    def test_stale_identity_and_wrong_confirmation(self):
        fresh = {'token': 'current', 'created': repair.time.time(), 'disk': {'path': '/dev/test'},
                 'root': '/mount', 'esp': '', 'mode': 'bios', 'backup': '/backup'}
        with patch.object(repair.os, 'geteuid', return_value=0, create=True), patch.object(repair, 'plan', return_value=fresh), patch.object(repair, 'snapshot') as copy:
            stale = {**fresh, 'token': 'old'}
            with self.assertRaises(ValueError):
                repair.apply(stale, 'REPAIR /dev/test')
            with self.assertRaises(ValueError):
                repair.apply(fresh, 'REPAIR /dev/other')
            with self.assertRaises(ValueError):
                repair.apply({**fresh, 'created': repair.time.time() - 181}, 'REPAIR /dev/test')
            copy.assert_not_called()

    def test_privilege_and_path_guard(self):
        with patch.object(repair.os, 'geteuid', return_value=1000, create=True):
            with self.assertRaises(ValueError):
                repair.apply({}, '')
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            (root / 'boot').symlink_to('/etc')
            with self.assertRaises(ValueError):
                repair.regular(root, 'boot/passwd')


if __name__ == '__main__':
    unittest.main()
