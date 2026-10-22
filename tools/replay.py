#!/usr/bin/env python3
"""Offline diagnostic replay for CSI Radar RAW schema 1 (standard Python only).
RAW is decimated to <=10 Hz; estimates are not a bit-exact live 20 Hz replay.
No real-world accuracy can be inferred without labeled captures.
"""
import argparse
import csv
import json
import math
import statistics
from pathlib import Path

BINS = [i for i in range(64) if (2 <= i <= 26 or 38 <= i <= 63) and i not in (7, 21, 43, 57)]


def parse_raw(line):
    if not line.startswith('$RAW,'):
        return None
    parts = line.strip().split(',')
    if len(parts) != 5:
        raise ValueError('RAW needs 5 fields')
    stamp, rssi, skip = map(int, parts[1:4])
    if not 0 <= stamp <= 0xFFFFFFFF or not -127 <= rssi <= 0 or skip not in (0, 4):
        raise ValueError('RAW field out of range')
    if len(parts[4]) != 256:
        raise ValueError('RAW needs 256 hex digits')
    raw = bytes.fromhex(parts[4])
    if len(raw) != 128:
        raise ValueError('RAW needs 128 bytes')
    iq = [b if b < 128 else b-256 for b in raw]
    amplitudes = [math.hypot(iq[2*i], iq[2*i+1]) for i in BINS]
    clipped = sum(iq[2*i] in (-128, 127) or iq[2*i+1] in (-128, 127) for i in BINS)
    mean = sum(amplitudes)/len(amplitudes)
    if mean < 1 or clipped > len(BINS)//4:
        raise ValueError('invalid energy or clipped sample')
    return stamp, rssi, [a/mean for a in amplitudes]


class Replay:
    def __init__(self, threshold=0.12, mode="classic"):
        if not 0.005 <= threshold <= 2:
            raise ValueError('threshold must be 0.005..2')
        if mode not in ("classic", "robust"):
            raise ValueError("mode must be classic or robust")
        self.mode = mode
        self.threshold = threshold
        self.reset()

    def reset(self):
        self.window = []
        self.baseline = None
        self.left = 40
        self.score = 0.0
        self.moving = False
        self.last = None
        self.above = self.below = None

    def feed(self, stamp, amplitudes):
        dt = ((stamp-self.last) & 0xFFFFFFFF) if self.last is not None else 50
        if dt >= 2000:
            self.reset()
        self.last = stamp
        if self.baseline is None:
            self.baseline = amplitudes[:]
            self.left -= 1
            return 'WARMUP'
        if self.left:
            self.baseline = [b+0.1*(a-b) for a, b in zip(amplitudes, self.baseline)]
            self.left -= 1
            return 'STILL' if not self.left else 'WARMUP'
        raw = [min(abs(a-b)/(b+0.05), 2.0) for a, b in zip(amplitudes, self.baseline) if b >= 0.15]
        value = sum(raw)/len(raw) if raw else 0
        if self.mode == "robust" and raw:
            median = sorted(raw)[len(raw)//2]
            deviations = sorted(abs(v-median) for v in raw)
            mad = deviations[len(deviations)//2]
            limit = median+3*max(mad, 0.02)
            value = sum(min(v, limit) for v in raw)/len(raw)
            self.window = (self.window+[value])[-3:]
            if len(self.window) == 3:
                value = statistics.median(self.window)
        dt = min(dt, 250)
        self.score += (1-math.exp(-dt/225))*(value-self.score)
        self.baseline = [b+(1-math.exp(-dt/5000))*(a-b) for a, b in zip(amplitudes, self.baseline)]
        if self.score > self.threshold:
            self.below = None
            if self.above is None:
                self.above = stamp
            if ((stamp-self.above) & 0xFFFFFFFF) >= 100:
                self.moving = True
        elif self.score < 0.7*self.threshold:
            self.above = None
            if self.below is None:
                self.below = stamp
            if ((stamp-self.below) & 0xFFFFFFFF) >= 700:
                self.moving = False
        else:
            self.above = self.below = None
        return 'MOVING' if self.moving else 'STILL'


def analyze(source, output, threshold, mode="classic"):
    replay = Replay(threshold, mode)
    accepted = rejected = events = moving = 0
    maximum = 0.0
    previous = 'WARMUP'
    with Path(source).open(encoding='utf-8', errors='replace') as src, Path(output).open('x', newline='', encoding='utf-8') as dst:
        writer = csv.writer(dst)
        writer.writerow(['schema', 'ms', 'rssi', 'score_estimate', 'threshold', 'state'])
        for line in src:
            try:
                sample = parse_raw(line)
            except (ValueError, OverflowError):
                rejected += 1
                continue
            if sample is None:
                continue
            stamp, rssi, amp = sample
            state = replay.feed(stamp, amp)
            accepted += 1
            moving += state == 'MOVING'
            events += state == 'MOVING' and previous != 'MOVING'
            maximum = max(maximum, replay.score)
            previous = state
            writer.writerow([1, stamp, rssi, f'{replay.score:.6f}', threshold, state])
    return dict(schema=1, filter=mode, source=str(source), accepted=accepted, rejected=rejected,
                moving_samples=moving, motion_starts=events, max_score=round(maximum, 6),
                threshold=threshold, caveat='Decimated RAW diagnostic; not bit-exact live replay. No accuracy claim.')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('captures', nargs='+', type=Path)
    p.add_argument('--filter', choices=['classic','robust','both'], default='classic')
    p.add_argument('--threshold', type=float, default=0.12)
    p.add_argument('--output-dir', type=Path, default=Path('replay-output'))
    args = p.parse_args()
    if not 0.005 <= args.threshold <= 2:
        p.error('threshold must be 0.005..2')
    args.output_dir.mkdir(parents=True, exist_ok=True)
    results = []
    modes = ['classic','robust'] if args.filter == 'both' else [args.filter]
    jobs = [(capture, mode, args.output_dir/(capture.stem+('-'+mode if args.filter == 'both' else '')+'.csv'))
            for capture in args.captures for mode in modes]
    outputs = [job[2] for job in jobs]+[args.output_dir/'summary.json']
    if len(set(outputs)) != len(outputs) or any(path.exists() for path in outputs):
        p.error('Output collision: choose a new output directory')
    for capture, mode, output in jobs:
        result = analyze(capture, output, args.threshold, mode)
        results.append(result)
        print(json.dumps(result, ensure_ascii=False))
    with (args.output_dir/'summary.json').open('x', encoding='utf-8') as f:
        json.dump(results, f, indent=2, ensure_ascii=False)


if __name__ == '__main__':
    main()
