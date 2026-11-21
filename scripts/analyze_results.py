#!/usr/bin/env python3
"""Paired benchmark audit, using only Python's standard library."""
import argparse
import csv
import json
import math
import random
import statistics
from collections import Counter
from pathlib import Path


def summary(values):
    if not values:
        return {"n": 0, "median": None, "q1": None, "q3": None, "iqr": None}
    values = sorted(values)
    def q(p):
        x = (len(values) - 1) * p
        i = int(x)
        return values[i] + (values[min(i+1, len(values)-1)] - values[i]) * (x-i)
    return {"n": len(values), "median": statistics.median(values),
            "q1": q(.25), "q3": q(.75), "iqr": q(.75)-q(.25)}


def mcnemar(b, c):
    n = b+c
    if not n:
        return {"b_a_only": b, "c_b_only": c, "p_value": 1.0, "method": "exact binomial"}
    # Accumulate binomial probabilities in log-space to avoid underflow for large n.
    k = min(b, c)
    logs = [math.lgamma(n+1)-math.lgamma(i+1)-math.lgamma(n-i+1)-n*math.log(2)
            for i in range(k+1)]
    largest = max(logs)
    log_tail = largest + math.log(sum(math.exp(x-largest) for x in logs))
    return {"b_a_only": b, "c_b_only": c, "p_value": min(1., 2*math.exp(log_tail)),
            "method": "exact binomial"}


def wilcoxon(differences):
    nonzero = sorted((abs(d), d > 0) for d in differences if d != 0)
    n = len(nonzero)
    if not n:
        return {"n_nonzero": 0, "statistic": 0, "p_value": 1., "method": "all differences zero"}
    ranks = []
    i = 0
    while i < n:
        j = i+1
        while j < n and nonzero[j][0] == nonzero[i][0]:
            j += 1
        # Integer representation of twice the average rank, handling ties exactly.
        for k in range(i, j):
            ranks.append((i+1+j, nonzero[k][1]))
        i = j
    positive = sum(r for r, sign in ranks if sign)
    total = sum(r for r, _ in ranks)
    observed = min(positive, total-positive)
    if n <= 100:
        counts = {0: 1}
        for r, _ in ranks:
            update = counts.copy()
            for s, count in counts.items():
                update[s+r] = update.get(s+r, 0)+count
            counts = update
        p = min(1., 2*sum(count for s, count in counts.items() if s <= observed)/(2**n))
        method = "exact sign-flip distribution, averaged ties, zero differences removed"
    else:
        variance = sum((r/2)**2 for r, _ in ranks)/4
        z = max(0., abs(positive/2-total/4)-.5)/math.sqrt(variance)
        p = math.erfc(z/math.sqrt(2))
        method = "normal approximation with tie variance and continuity correction"
    return {"n_nonzero": n, "statistic": observed/2, "p_value": p, "method": method}


def bootstrap_median_ci(values, seed=0, repetitions=2000):
    if not values:
        return None
    rng = random.Random(seed)
    medians = sorted(statistics.median(rng.choices(values, k=len(values))) for _ in range(repetitions))
    return [medians[int(.025*(repetitions-1))], medians[int(.975*(repetitions-1))]]


def load(path, timeout):
    rows = {}
    with open(path, newline='', encoding='utf-8') as source:
        reader = csv.DictReader(source)
        expected = {'instance_id', 'solver', 'status', 'validity', 'runtime', 'cost'}
        if not reader.fieldnames or not expected.issubset(reader.fieldnames):
            raise ValueError('CSV requires '+', '.join(sorted(expected)))
        for row in reader:
            key = (row['instance_id'], row['solver'])
            if not all(key) or key in rows:
                raise ValueError('empty or duplicate instance_id/solver: '+str(key))
            if row['status'] not in {'solved', 'timeout', 'invalid', 'error', 'infeasible', 'resource_limit'}:
                raise ValueError('unknown status: '+row['status'])
            if row['validity'].lower() not in {'true', 'false', '', 'null'}:
                raise ValueError('validity must be true, false, null or empty')
            runtime = float(row['runtime'])
            if not math.isfinite(runtime) or runtime < 0:
                raise ValueError('runtime must be finite and nonnegative')
            row['runtime'] = runtime
            row['success'] = (row['status'] == 'solved' and row['validity'].lower() == 'true'
                              and runtime < timeout)
            if row['status'] == 'solved' and row['validity'].lower() == 'true':
                cost = float(row['cost'])
                if not math.isfinite(cost) or cost < 0 or not cost.is_integer():
                    raise ValueError('solved row requires nonnegative integer SOC')
                row['cost'] = int(cost)
            rows[key] = row
    return rows


def analyze(path, a, b, timeout=60., seed=0):
    if a == b or not math.isfinite(timeout) or timeout <= 0:
        raise ValueError('choose distinct solvers and a positive finite timeout')
    rows = load(path, timeout)
    ids_a = {i for i, s in rows if s == a}
    ids_b = {i for i, s in rows if s == b}
    if not ids_a or ids_a != ids_b:
        raise ValueError('both solvers must have exactly the same nonempty instance set')
    pairs = [(rows[i, a], rows[i, b]) for i in sorted(ids_a)]
    jointly = [(x, y) for x, y in pairs if x['success'] and y['success']]
    differences = [x['runtime']-y['runtime'] for x, y in jointly]
    mc = mcnemar(sum(x['success'] and not y['success'] for x, y in pairs),
                 sum(y['success'] and not x['success'] for x, y in pairs))
    wc = wilcoxon(differences)
    tests = [mc, wc]
    order = sorted(range(2), key=lambda i: tests[i]['p_value'])
    adjusted = 0.
    for rank, i in enumerate(order):
        adjusted = max(adjusted, min(1., (2-rank)*tests[i]['p_value']))
        tests[i]['p_holm_two_tests'] = adjusted
    solvers = {}
    for solver in (a, b):
        group = [rows[i, solver] for i in sorted(ids_a)]
        solved = [r for r in group if r['success']]
        solvers[solver] = {'successes': len(solved), 'total': len(group),
            'success_rate': len(solved)/len(group),
            'raw_status_counts': dict(Counter(r['status'] for r in group)),
            'runtime_success_only': summary([r['runtime'] for r in solved]),
            'cost_success_only': summary([r['cost'] for r in solved])}
    return {'solvers': solvers, 'timeout_seconds': timeout, 'jointly_solved': len(jointly),
            'paired_runtime_difference_a_minus_b': summary(differences),
            'paired_median_difference_bootstrap_ci95': bootstrap_median_ci(differences, seed),
            'bootstrap_seed': seed, 'mcnemar': mc, 'wilcoxon_runtime': wc,
            'limitations': ['Runtime tests use jointly solved, independently validated instances only; selection bias applies.',
                'Instances are assumed independent; nested agent-count prefixes or repeated scenarios can violate this assumption.',
                'Wilcoxon assumes symmetric paired differences; bootstrap CI is descriptive and resamples instance pairs.',
                'Holm correction covers these two tests only; adjust across additional comparisons externally.',
                'CSV validity must come from the independent validator; this script cannot verify paths.',
                'Rows reaching the timeout are failures even if raw status says solved.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv')
    parser.add_argument('--a', required=True)
    parser.add_argument('--b', required=True)
    parser.add_argument('--timeout', type=float, default=60.)
    parser.add_argument('--seed', type=int, default=0)
    parser.add_argument('--output')
    args = parser.parse_args()
    try:
        result = analyze(args.csv, args.a, args.b, args.timeout, args.seed)
        text = json.dumps(result, indent=2, ensure_ascii=False, allow_nan=False)
        if args.output:
            Path(args.output).write_text(text+'\n', encoding='utf-8')
        print(text)
    except (ValueError, OSError, KeyError) as error:
        parser.exit(2, str(error)+'\n')


if __name__ == '__main__':
    main()
