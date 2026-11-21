#!/usr/bin/env python3
"""Sequential tiny-oracle runner with checkpoint CSV and input/config fingerprints."""
import argparse
import csv
import hashlib
import json
import math
import os
import platform
import subprocess
import time
from pathlib import Path

FIELDS = ['instance_id', 'solver', 'status', 'validity', 'runtime', 'cost',
          'certified_lb', 'expanded', 'generated', 'peak_memory_bytes', 'fingerprint', 'message']


def run(manifest, cli, output, timeout=10., max_states=500000):
    if not math.isfinite(timeout) or timeout <= 0 or max_states <= 0:
        raise ValueError('timeout and max_states must be positive')
    manifest, cli, output = Path(manifest).resolve(), Path(cli).resolve(), Path(output)
    with manifest.open(newline='') as source:
        reader = csv.DictReader(source)
        if not reader.fieldnames or not {'instance_id', 'instance'}.issubset(reader.fieldnames):
            raise ValueError('manifest requires instance_id,instance')
        jobs = list(reader)
    if not jobs or any(not j['instance_id'] for j in jobs) or len({j['instance_id'] for j in jobs}) != len(jobs):
        raise ValueError('empty or duplicate instance ids')
    binary_hash = hashlib.sha256(cli.read_bytes()).hexdigest()
    for job in jobs:
        job['path'] = (manifest.parent/job['instance']).resolve()
        raw = job['path'].read_bytes()+json.dumps([timeout, max_states, binary_hash]).encode()
        job['fingerprint'] = hashlib.sha256(raw).hexdigest()
    existing = {}
    if output.exists():
        with output.open(newline='') as source:
            reader = csv.DictReader(source)
            if reader.fieldnames != FIELDS:
                raise ValueError('checkpoint schema mismatch')
            for row in reader:
                if row['instance_id'] in existing:
                    raise ValueError('duplicate checkpoint rows')
                existing[row['instance_id']] = row
    expected = {j['instance_id']: j for j in jobs}
    for key, row in existing.items():
        if key not in expected or row['fingerprint'] != expected[key]['fingerprint']:
            raise ValueError('checkpoint input/config/binary changed; use a new output file')
    cpu_info = Path('/proc/cpuinfo').read_text() if Path('/proc/cpuinfo').exists() else ''
    mem_info = Path('/proc/meminfo').read_text() if Path('/proc/meminfo').exists() else ''
    model = next((line.split(':', 1)[1].strip() for line in cpu_info.splitlines()
                  if line.startswith('model name')), platform.processor())
    memory = next((line.split(':', 1)[1].strip() for line in mem_info.splitlines()
                   if line.startswith('MemTotal')), None)
    metadata = {'cpu_model': model, 'system_memory': memory, 'python': platform.python_version(),
                'created_unix': time.time(), 'platform': platform.platform(),
                'cpu_count_logical': os.cpu_count(), 'concurrency': 1,
                'timeout_seconds': timeout, 'max_states': max_states,
                'binary_sha256': binary_hash, 'manifest': str(manifest),
                'note': 'Tiny oracle only, not the official CBS/ECBS benchmark.'}
    Path(str(output)+'.metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    with output.open('a', newline='') as target:
        writer = csv.DictWriter(target, fieldnames=FIELDS)
        if target.tell() == 0:
            writer.writeheader()
        for job in jobs:
            if job['instance_id'] in existing:
                continue
            process = subprocess.run([str(cli), '--mode', 'oracle', '--instance', str(job['path']),
                                      '--timeout', str(timeout), '--max-states', str(max_states)],
                                     capture_output=True, text=True, timeout=timeout+30)
            text = process.stdout or process.stderr
            result = json.loads(text)
            row = {k: result.get(k, '') for k in FIELDS}
            row.update(instance_id=job['instance_id'], solver='joint_state_oracle',
                       fingerprint=job['fingerprint'])
            row['validity'] = 'true' if result.get('validity') is True else ''
            writer.writerow(row)
            target.flush()
            print(job['instance_id'], result.get('status'), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', default='data/tiny_manifest.csv')
    parser.add_argument('--cli', default='build/tv4_cli')
    parser.add_argument('--output', default='build/oracle_batch.csv')
    parser.add_argument('--timeout', type=float, default=10.)
    parser.add_argument('--max-states', type=int, default=500000)
    args = parser.parse_args()
    try:
        run(args.manifest, args.cli, args.output, args.timeout, args.max_states)
    except (ValueError, OSError, KeyError, subprocess.TimeoutExpired) as error:
        parser.exit(2, str(error)+'\n')


if __name__ == '__main__':
    main()
