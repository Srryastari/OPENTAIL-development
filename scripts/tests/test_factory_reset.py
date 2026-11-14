import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('reset',Path(__file__).resolve().parents[1]/'factory_reset.py')
reset=importlib.util.module_from_spec(spec);spec.loader.exec_module(reset)

class ResetTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.sha='a'*40
        for name in reset.IMAGES.values():
            p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'test-image')
        (self.root/'release_manifest.json').write_text(json.dumps({'git_sha':self.sha,'chip':'ESP32-S3 N16R8','flasher_offsets':reset.IMAGES}))
        files=['release_manifest.json',*reset.IMAGES.values()]
        (self.root/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256((self.root/n).read_bytes()).hexdigest()+'  '+n+'\n' for n in files))
        self.calls=[]
    def run_fake(self,args,**kw):self.calls.append(args)
    def test_validation_never_touches_device(self):
        reset.validate(self.root,self.sha);self.assertEqual([],self.calls)
        with self.assertRaises(ValueError):reset.flash(self.root,self.sha,'COM9',None,self.run_fake)
        self.assertEqual([],self.calls)
    def test_corruption_and_wrong_identity_never_erase(self):
        with self.assertRaises(ValueError):reset.flash(self.root,'b'*40,'COM9','ERASE-ALL',self.run_fake)
        (self.root/'opentail_core_alpha.bin').write_bytes(b'corrupted')
        with self.assertRaises(ValueError):reset.flash(self.root,self.sha,'COM9','ERASE-ALL',self.run_fake)
        self.assertEqual([],self.calls)
    def test_explicit_erase_precedes_complete_flash(self):
        reset.flash(self.root,self.sha,'COM9','ERASE-ALL',self.run_fake)
        self.assertEqual(2,len(self.calls));self.assertIn('erase_flash',self.calls[0])
        for offset,name in reset.IMAGES.items():
            i=self.calls[1].index(offset);self.assertEqual(name,self.calls[1][i+1])
    def test_failed_erase_stops_flash(self):
        def fail(args,**kw):
            self.calls.append(args);raise subprocess.CalledProcessError(1,args)
        with self.assertRaises(subprocess.CalledProcessError):reset.flash(self.root,self.sha,'COM9','ERASE-ALL',fail)
        self.assertEqual(1,len(self.calls))

if __name__=='__main__':unittest.main()
