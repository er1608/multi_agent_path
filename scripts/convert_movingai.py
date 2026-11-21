#!/usr/bin/env python3
"""Convert one MovingAI map/scenario prefix into the TV4 interchange format."""
import argparse
from pathlib import Path


def convert(map_path, scen_path, agents):
    lines = Path(map_path).read_text().splitlines()
    if len(lines) < 5 or lines[0].strip() != 'type octile' or lines[3].strip() != 'map':
        raise ValueError('expected MovingAI type octile map')
    height, width = int(lines[1].split()[1]), int(lines[2].split()[1])
    grid = lines[4:]
    if len(grid) != height or any(len(row) != width for row in grid):
        raise ValueError('incorrect map dimensions')
    # Use binary terrain only. Reject swamp/water maps rather than silently
    # changing their transition semantics.
    if any(c not in '.G@OT' for row in grid for c in row):
        raise ValueError('unsupported terrain; only . G @ O T are supported in this binary grid model')
    grid = [''.join('.' if c in '.G' else '#' for c in row) for row in grid]
    scenarios = Path(scen_path).read_text().splitlines()
    if not scenarios or scenarios[0].strip() not in {'version 1', 'version 1.0'}:
        raise ValueError('expected MovingAI scenario version 1')
    rows = [line.split() for line in scenarios[1:] if line.strip()]
    if agents < 1 or len(rows) < agents:
        raise ValueError('not enough scenario rows or invalid agent count')
    positions = []
    for row in rows[:agents]:
        if len(row) != 9 or Path(row[1]).name != Path(map_path).name:
            raise ValueError('scenario map name mismatch or malformed row')
        if (int(row[2]), int(row[3])) != (width, height):
            raise ValueError('scenario dimensions mismatch')
        sx, sy, gx, gy = map(int, row[4:8])
        for x, y in ((sx, sy), (gx, gy)):
            if not (0 <= y < height and 0 <= x < width and grid[y][x] == '.'):
                raise ValueError('scenario position blocked or out of bounds')
        positions.append((sy, sx, gy, gx))
    if len({p[:2] for p in positions}) != agents or len({p[2:] for p in positions}) != agents:
        raise ValueError('duplicate start or goal')
    return '\n'.join([f'{height} {width}', *grid, str(agents),
                      *(' '.join(map(str, p)) for p in positions)])+'\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--map', required=True)
    parser.add_argument('--scen', required=True)
    parser.add_argument('--agents', type=int, required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    try:
        Path(args.output).write_text(convert(args.map, args.scen, args.agents))
    except (ValueError, IndexError, OSError) as error:
        parser.exit(2, str(error)+'\n')


if __name__ == '__main__':
    main()
