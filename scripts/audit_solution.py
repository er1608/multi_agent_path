#!/usr/bin/env python3
"""Independently audit a solver JSON file with the TV4 validator and optional oracle."""
import argparse
import json
import math
import subprocess
import tempfile
from pathlib import Path


def invoke(cli, *args):
    process = subprocess.run([str(cli), *map(str, args)], capture_output=True, text=True)
    text = process.stdout.strip() or process.stderr.strip()
    try:
        return json.loads(text)
    except json.JSONDecodeError as error:
        raise ValueError('CLI did not return JSON: '+text) from error


def audit(cli, instance, solution, use_oracle=False, timeout=10., w=None):
    data = json.loads(Path(solution).read_text())
    paths = data.get('paths')
    if data.get('status') != 'solved':
        raise ValueError('audit_solution expects a claimed solved result with paths')
    if not isinstance(paths, list) or not paths:
        raise ValueError('paths must be a nonempty list')
    lines = [str(len(paths))]
    for path in paths:
        if not isinstance(path, list) or not path:
            raise ValueError('every path must be nonempty')
        coords = []
        for p in path:
            if not isinstance(p, list) or len(p) != 2 or any(type(x) is not int or x < 0 for x in p):
                raise ValueError('coordinates must be nonnegative integer [row,col] pairs')
            coords.extend(p)
        lines.append(' '.join(map(str, [len(path), *coords])))
    with tempfile.TemporaryDirectory() as d:
        file = Path(d)/'paths.txt'
        file.write_text('\n'.join(lines)+'\n')
        check = invoke(cli, '--mode', 'validate', '--instance', instance, '--paths', file)
    if check.get('status') == 'error':
        raise ValueError(check['message'])
    result = {'validation': check, 'reported_cost_matches': None, 'oracle': None,
              'gap': None, 'w': w, 'bounded_suboptimality_verified': None}
    if not check.get('validity'):
        return result
    reported = data.get('cost')
    result['reported_cost_matches'] = type(reported) is int and reported == check['cost']
    lb = invoke(cli, '--mode', 'lb', '--instance', instance)
    if lb.get('status') != 'bounded':
        raise ValueError('validated solution contradicts independent LB: '+str(lb))
    bound, kind = lb['certified_lb'], 'lb'
    if use_oracle:
        oracle = invoke(cli, '--mode', 'oracle', '--instance', instance, '--timeout', timeout)
        result['oracle'] = oracle
        if oracle.get('status') == 'solved':
            bound, kind = oracle['cost'], 'optimum'
        elif oracle.get('status') in {'error', 'invalid_input', 'infeasible'}:
            raise ValueError('oracle failed or contradicts validated paths: '+str(oracle))
    result['gap'] = invoke(cli, '--mode', 'gap', '--soc', check['cost'],
                           '--bound', bound, '--bound-kind', kind)
    if w is not None:
        if not math.isfinite(w) or w < 1:
            raise ValueError('w must be finite and >= 1')
        if kind == 'optimum':
            result['bounded_suboptimality_verified'] = check['cost'] <= w*bound+1e-9
        elif check['cost'] <= w*bound+1e-9:
            # LB <= optimum: this is sufficient to certify the bound, not necessary.
            result['bounded_suboptimality_verified'] = True
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli', default='build/tv4_cli')
    parser.add_argument('--instance', required=True)
    parser.add_argument('--solution', required=True)
    parser.add_argument('--oracle', action='store_true')
    parser.add_argument('--timeout', type=float, default=10.)
    parser.add_argument('--w', type=float)
    parser.add_argument('--output')
    args = parser.parse_args()
    try:
        result = audit(Path(args.cli).resolve(), args.instance, args.solution,
                       args.oracle, args.timeout, args.w)
        text = json.dumps(result, indent=2, ensure_ascii=False, allow_nan=False)
        if args.output:
            Path(args.output).write_text(text+'\n')
        print(text)
        if not result['validation']['validity'] or not result['reported_cost_matches'] or \
                result['bounded_suboptimality_verified'] is False:
            return 2
    except (ValueError, OSError, KeyError, TypeError) as error:
        parser.exit(2, str(error)+'\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
