import importlib.util
import itertools
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

CLI = Path(os.environ.get('TV4_CLI', 'build/tv4_cli')).resolve()
ROOT = Path(__file__).resolve().parents[1]


def script(name):
    spec = importlib.util.spec_from_file_location(name, ROOT/'scripts'/(name+'.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


@unittest.skipUnless(CLI.is_file(), 'build tv4_cli first')
class CLITests(unittest.TestCase):
    def invoke(self, *args):
        p = subprocess.run([str(CLI), *map(str, args)], capture_output=True, text=True)
        return p.returncode, json.loads(p.stdout or p.stderr)

    def test_roundtrip_audit_and_gap(self):
        with tempfile.TemporaryDirectory() as d:
            solution, paths = Path(d)/'result.json', Path(d)/'paths.txt'
            code, result = self.invoke('--mode', 'oracle', '--instance', 'data/tiny.mapf',
                                       '--output', solution, '--paths-output', paths)
            self.assertEqual(code, 0)
            self.assertEqual(result['cost'], 4)
            code, check = self.invoke('--mode', 'validate', '--instance', 'data/tiny.mapf', '--paths', paths)
            self.assertEqual(code, 0)
            self.assertEqual(check['cost'], 4)
            audited = script('audit_solution').audit(CLI, 'data/tiny.mapf', solution, True, w=1.01)
            self.assertTrue(audited['bounded_suboptimality_verified'])
            self.assertTrue(audited['reported_cost_matches'])
            result['cost'] = 9
            solution.write_text(json.dumps(result))
            audited = script('audit_solution').audit(CLI, 'data/tiny.mapf', solution)
            self.assertFalse(audited['reported_cost_matches'])
        code, result = self.invoke('--mode', 'gap', '--soc', 12, '--bound', 8)
        self.assertEqual(code, 0)
        self.assertEqual(result['relative_gap'], .5)
        self.assertFalse(result['exact'])

    def test_bad_input_and_infeasible(self):
        code, result = self.invoke('--mode', 'oracle', '--instance', 'data/swap.mapf')
        self.assertEqual(code, 0)
        self.assertEqual(result['status'], 'infeasible')
        self.assertIsNone(result['cost'])
        code, result = self.invoke('--mode', 'oracle', '--instance', 'data/tiny.mapf', '--timeout', 'nan')
        self.assertEqual(code, 2)
        self.assertEqual(result['status'], 'invalid_input')
        code, result = self.invoke('--unknown', 'x')
        self.assertEqual(code, 2)
        self.assertEqual(result['status'], 'error')
        with tempfile.TemporaryDirectory() as d:
            path = Path(d)/'bad.mapf'
            path.write_text('1 2\n..\n1\n0 0 0 1\nextra\n')
            code, result = self.invoke('--mode', 'oracle', '--instance', path)
            self.assertEqual(code, 2)

    def test_checkpoint_and_changed_config(self):
        with tempfile.TemporaryDirectory() as d:
            output = Path(d)/'batch.csv'
            batch = script('run_batch')
            batch.run('data/tiny_manifest.csv', CLI, output)
            before = output.read_bytes()
            batch.run('data/tiny_manifest.csv', CLI, output)
            self.assertEqual(before, output.read_bytes())
            with self.assertRaises(ValueError):
                batch.run('data/tiny_manifest.csv', CLI, output, max_states=100)

    def test_all_two_agent_pairs_against_path_enumeration(self):
        # Independent reference: enumerate individual timed paths, then pair them.
        # This does not use the oracle's joint-state/commitment transition model.
        cells = [(0, 0), (0, 1), (1, 0), (1, 1)]
        def paths(start, goal, limit=5):
            found = []
            def visit(path):
                if path[-1] == goal and (len(path) == 1 or path[-2] != goal):
                    found.append(path)
                if len(path) > limit:
                    return
                for p in cells:
                    if sum(abs(x-y) for x, y in zip(p, path[-1])) <= 1:
                        visit(path+[p])
            visit([start])
            return sorted(found, key=len)
        bank = {(s, g): paths(s, g) for s in cells for g in cells}
        def valid(a, b):
            for t in range(max(len(a), len(b))):
                ai, bi = a[min(t, len(a)-1)], b[min(t, len(b)-1)]
                if ai == bi:
                    return False
                if t and a[min(t-1, len(a)-1)] == bi and b[min(t-1, len(b)-1)] == ai:
                    return False
            return True
        with tempfile.TemporaryDirectory() as d:
            file = Path(d)/'enumerate.mapf'
            for starts in itertools.permutations(cells, 2):
                for goals in itertools.permutations(cells, 2):
                    minimum = 100
                    for a in bank[starts[0], goals[0]]:
                        for b in bank[starts[1], goals[1]]:
                            cost = len(a)+len(b)-2
                            if cost >= minimum:
                                break
                            if valid(a, b):
                                minimum = cost
                    # All costs <=5: enumerating per-agent paths <=5 includes every
                    # potential solution cheaper than the independently found one.
                    self.assertLessEqual(minimum, 5)
                    text = '2 2\n..\n..\n2\n'+'\n'.join(
                        ' '.join(map(str, (*s, *g))) for s, g in zip(starts, goals))+'\n'
                    file.write_text(text)
                    code, result = self.invoke('--mode', 'oracle', '--instance', file)
                    self.assertEqual(code, 0)
                    self.assertEqual(result['status'], 'solved')
                    self.assertEqual(result['cost'], minimum, (starts, goals))
