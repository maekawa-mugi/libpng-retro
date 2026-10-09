"""Host regression tests for strict SPR timing/checksum log interpretation."""
import unittest
from analyze_spr import parse

class SprLogTests(unittest.TestCase):
    def setUp(self):
        self.good = [
            "SPR_META,EE_MMI,16KiB,mode=RAM_ROW_AUX_BOTH_XFER,samples=6,reps=16,timer=clock_ticks",
            "SPR_HEADER,kernel,width,mode,ticks,ram_over_spr,status,output_checked",
            "SPR_CASE,up-mmi,512,ram,100,1.00000,MEASURED,PASS",
            "SPR_CASE,up-mmi,512,spr_row,80,1.25000,MEASURED,PASS",
            "SPR_CASE,up-mmi,512,spr_aux,95,1.05263,MEASURED,PASS",
            "SPR_CASE,up-mmi,512,spr_both,74,1.35135,MEASURED,PASS",
            "SPR_CASE,up-mmi,512,spr_xfer,145,0.68966,MEASURED,PASS",
            "SPR_RESULT,PASS,cases=1,timer_na=0,sink=3",
            "TEST: OK! code=0",
        ]

    def test_valid(self):
        samples, problems, count = parse(self.good)
        self.assertEqual(count, 1)
        self.assertEqual(len(samples), 5)
        self.assertFalse(problems)
        self.assertAlmostEqual(samples[1].ratio, 1.25)

    def test_reject_corrupt_output(self):
        _, problems, _ = parse(self.good + ["SPR_FAIL,up-mmi,512,spr_row,output"])
        self.assertTrue(any("correctness failure" in x for x in problems))

    def test_reject_truncated(self):
        _, problems, _ = parse(self.good[:-2])
        self.assertTrue(problems)

    def test_reject_missing_mode(self):
        lines = [x for x in self.good if "spr_xfer" not in x or x.startswith("SPR_META")]
        _, problems, _ = parse(lines)
        self.assertTrue(any("missing RAM" in x for x in problems))

    def test_zero_timer_explicit(self):
        lines = [x.replace(",80,1.25000,MEASURED,PASS", ",0,0.00000,TIMER_NA,PASS")
                 for x in self.good]
        _, problems, _ = parse(lines)
        self.assertFalse(problems)

    def test_reject_false_global_success(self):
        lines = [x.replace("TEST: OK! code=0", "TEST: FAIL! code=1")
                 for x in self.good]
        _, problems, _ = parse(lines)
        self.assertTrue(any("TEST" in x for x in problems))

if __name__ == "__main__":
    unittest.main()
