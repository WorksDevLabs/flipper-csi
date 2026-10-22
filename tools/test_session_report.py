import tempfile
import unittest
from pathlib import Path
from session_report import load, report

class ReportTests(unittest.TestCase):
    def test_gap_and_escaping(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'a&b.csv'
            p.write_text('schema,uptime_ms,score_milli,threshold_milli,state,age_ms,event\n'
                         '1,0,10,120,1,0,SAMPLE\n1,1000,200,120,2,0,SAMPLE\n'
                         '1,2000,220,120,2,0,SAMPLE\n1,9000,220,120,2,9000,SAMPLE\n'
                         '1,10000,20,120,1,0,SAMPLE\n1,11000,999,120,2,0,SAMPLE\n')
            s = load(p)
            self.assertEqual(s['events'], 2)
            self.assertEqual(s['valid_seconds'], 3)
            self.assertEqual(s['moving_seconds'], 1)
            self.assertIn('a&amp;b.csv', report([s]))
    def test_schema2_filter(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'new.csv'
            p.write_text('schema,uptime_ms,score_milli,threshold_milli,state,age_ms,event,filter\n'
                         '2,0,10,120,1,0,START,1\n2,1000,200,120,2,0,STATE,1\n')
            s=load(p)
            self.assertEqual(s['filters'],['F1'])
            self.assertEqual(s['events'],1)
    def test_bad_schema(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'bad.csv'; p.write_text('wrong\n1\n')
            with self.assertRaises(ValueError): load(p)

if __name__ == '__main__': unittest.main()
