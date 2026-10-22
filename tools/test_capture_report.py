import tempfile
import unittest
from pathlib import Path
from capture_report import analyze, render
from replay import Replay
class CaptureTests(unittest.TestCase):
    def test_real_bin_mapping_and_report(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'a&b.log'
            p.write_text('\n'.join('$RAW,%d,-65,0,%s'%(i*100,bytes([20,10]*64).hex()) for i in range(250))+'\n$RAW,invalid\n')
            data=analyze(p,.12)
            self.assertEqual(data['accepted'],250)
            self.assertEqual(data['rejected'],1)
            self.assertEqual(len(data['waterfall']),200)
            self.assertEqual(len(data['waterfall'][0]),47)
            self.assertEqual(data['starts'],[0,0])
            self.assertIn('a&amp;b.log',render(data))
    def test_robust_sustained_motion(self):
        r=Replay(.12,'robust')
        for i in range(100): r.feed(i*100,[1.]*47)
        states=[r.feed(10000+i*100,[.5 if (j+i)%2 else 1.5 for j in range(47)]) for i in range(50)]
        self.assertIn('MOVING',states)
        self.assertEqual(r.feed(30000,[1.]*47),'WARMUP')
        self.assertEqual(r.window,[])
if __name__=='__main__': unittest.main()
