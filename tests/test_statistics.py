import importlib.util
import tempfile
import unittest
from pathlib import Path


def module(name):
    spec = importlib.util.spec_from_file_location(name, Path('scripts')/(name+'.py'))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


stats = module('analyze_results')
convert = module('convert_movingai')


class StatisticsTests(unittest.TestCase):
    def test_known_exact_pvalues(self):
        self.assertAlmostEqual(stats.mcnemar(0, 5)['p_value'], .0625)
        self.assertEqual(stats.mcnemar(0, 0)['p_value'], 1)
        self.assertAlmostEqual(stats.wilcoxon([1, 2, 3, 4, 5])['p_value'], .0625)
        self.assertAlmostEqual(stats.wilcoxon([1, 1, 1, 1, 1])['p_value'], .0625)
        self.assertEqual(stats.wilcoxon([0, 0])['p_value'], 1)
        self.assertEqual(stats.wilcoxon([-1, 1])['p_value'], 1)

    def test_summary_and_pairing(self):
        self.assertEqual(stats.summary([1, 2, 3, 4])['iqr'], 1.5)
        result = stats.analyze('data/results_example.csv', 'CBS', 'ECBS')
        self.assertEqual(result['jointly_solved'], 2)
        self.assertEqual(result['solvers']['CBS']['success_rate'], .5)
        self.assertEqual(result['solvers']['ECBS']['success_rate'], .75)
        self.assertEqual(result['mcnemar']['c_b_only'], 1)

    def test_timeout_boundary_and_invalid(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d)/'r.csv'
            path.write_text('instance_id,solver,status,validity,runtime,cost\n'
                            'x,A,solved,true,60,4\nx,B,solved,false,1,\n')
            result = stats.analyze(path, 'A', 'B')
            self.assertEqual(result['jointly_solved'], 0)
            self.assertEqual(result['solvers']['A']['success_rate'], 0)
            path.write_text('instance_id,solver,status,validity,runtime,cost\n'
                            'x,A,solved,true,1,4\nx,A,solved,true,1,4\n')
            with self.assertRaises(ValueError):
                stats.analyze(path, 'A', 'B')

    def test_movingai_coordinates(self):
        with tempfile.TemporaryDirectory() as d:
            m, s = Path(d)/'test.map', Path(d)/'test.scen'
            m.write_text('type octile\nheight 2\nwidth 3\nmap\n...\n.@.\n')
            s.write_text('version 1\n0 test.map 3 2 0 0 2 1 3\n')
            result = convert.convert(m, s, 1)
            self.assertIn('0 0 1 2', result)
            self.assertIn('.#.', result)
            with self.assertRaises(ValueError):
                convert.convert(m, s, 2)
