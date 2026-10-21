import tempfile
import unittest
from pathlib import Path
from replay import Replay, parse_raw, analyze

class ReplayTests(unittest.TestCase):
    def test_bounds_and_gain(self):
        raw=bytes([20,10]*64).hex()
        a=parse_raw('$RAW,100,-65,4,'+raw)
        b=parse_raw('$RAW,200,-65,0,'+bytes([40,20]*64).hex())
        self.assertEqual(a[2],b[2])
        for line in ['$RAW,-1,-65,0,'+raw, '$RAW,1,-999,0,'+raw, '$RAW,1,-65,2,'+raw, '$RAW,1,-65,0,00']:
            with self.assertRaises(ValueError): parse_raw(line)
        self.assertIsNone(parse_raw('$CSI2,0,0'))
    def test_motion_and_recovery(self):
        r=Replay(0.12)
        for i in range(100): self.assertNotEqual(r.feed(i*100,[1.0]*47),'MOVING')
        states=[r.feed(10000+i*100,[0.5 if (j+i)%2 else 1.5 for j in range(47)]) for i in range(50)]
        self.assertIn('MOVING',states)
        for i in range(100): state=r.feed(15000+i*100,[1.0]*47)
        self.assertEqual(state,'STILL')
        self.assertEqual(r.feed(30000,[1.0]*47),'WARMUP')
    def test_output_is_non_destructive(self):
        with tempfile.TemporaryDirectory() as d:
            source=Path(d)/'capture.log';dest=Path(d)/'out.csv'
            source.write_text('$RAW,100,-65,0,'+bytes([20,10]*64).hex()+'\n$RAW,invalid\n')
            result=analyze(source,dest,0.12)
            self.assertEqual((result['accepted'],result['rejected']),(1,1))
            with self.assertRaises(FileExistsError): analyze(source,dest,0.12)
if __name__=='__main__': unittest.main()
