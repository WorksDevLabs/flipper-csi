#!/usr/bin/env python3
"""Compare Flipper schema-1 CSV sessions in a self-contained HTML report.
No network, credentials, dependencies, or inferred detection-accuracy claims.
"""
import argparse
import csv
import html
import json
from pathlib import Path


def load(path):
    points = []
    filters = set()
    events = rejected = valid = 0
    last = None
    duration = moving = 0
    with Path(path).open(encoding='utf-8', newline='') as f:
        reader = csv.DictReader(f)
        required = {'schema', 'uptime_ms', 'score_milli', 'threshold_milli', 'state', 'age_ms', 'event'}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError('Expected CSI Radar SD CSV schema 1/2')
        for row in reader:
            try:
                if int(row['schema']) not in (1,2):
                    raise ValueError('schema')
                if int(row['schema']) == 2 and int(row.get('filter','-1')) not in (0,1):
                    raise ValueError('filter')
                t, score, threshold, state, age = (int(row[k]) for k in
                    ('uptime_ms', 'score_milli', 'threshold_milli', 'state', 'age_ms'))
                if not (0 <= t <= 0xFFFFFFFF and 0 <= score <= 100000 and
                        5 <= threshold <= 2000 and 0 <= state <= 6 and 0 <= age <= 0xFFFFFFFF):
                    raise ValueError('bounds')
            except (ValueError, TypeError, KeyError):
                rejected += 1
                last = None
                continue
            filters.add('F'+row['filter'] if int(row['schema'])==2 else 'legacy')
            fresh = state in (1, 2) and age < 2000
            # CSV is nominally 1Hz + transitions. Gaps are excluded, not interpolated.
            if last and fresh and last[2] and 0 <= ((t-last[0]) & 0xFFFFFFFF) < 2000:
                delta = (t-last[0]) & 0xFFFFFFFF
                duration += delta
                if state == 2 and last[1] == 2:
                    moving += delta
            if fresh and state == 2 and (not last or not last[2] or last[1] != 2):
                events += 1
            valid += fresh
            last = (t, state, fresh)
            points.append((t, score, threshold, fresh))
            # Bounded plot buffer; retain the full-stream counters.
            if len(points) >= 4096:
                points = points[::2]
    return dict(name=Path(path).name, filters=sorted(filters), events=events, rejected=rejected,
                valid_samples=valid, valid_seconds=round(duration/1000, 1),
                moving_seconds=round(moving/1000, 1), points=points)


def chart(points):
    if not points:
        return '<p>No valid rows.</p>'
    start, end = points[0][0], points[-1][0]
    ymax = max(300, *(max(p[1], p[2]) for p in points))
    def xy(p, index):
        return f'{20+760*(p[0]-start)/max(1,end-start):.1f},{180-160*p[index]/ymax:.1f}'
    segments = []
    segment = []
    for p in points:
        if p[3]:
            segment.append(xy(p, 1))
        elif segment:
            segments.append(' '.join(segment))
            segment = []
    if segment:
        segments.append(' '.join(segment))
    score = ''.join(f'<polyline points="{s}" fill="none" stroke="#58d7bb" stroke-width="2"/>' for s in segments)
    threshold = ' '.join(xy(p, 2) for p in points)
    return f'<svg viewBox="0 0 800 205" role="img" aria-label="Score and threshold trend">{score}<polyline points="{threshold}" fill="none" stroke="#f6bb64" stroke-dasharray="5 4"/><text x="20" y="201" fill="#acb9c9">Uptime {start/1000:.1f} to {end/1000:.1f} seconds</text></svg>'


def report(sessions):
    rows = ''.join('<tr><td>'+html.escape(s['name'])+' ['+html.escape('/'.join(s['filters']))+']</td>'+''.join(f'<td>{s[k]}</td>' for k in
        ('events', 'valid_seconds', 'moving_seconds', 'rejected'))+'</tr>' for s in sessions)
    panels = ''.join('<section><h2>'+html.escape(s['name'])+'</h2>'+chart(s['points'])+'</section>' for s in sessions)
    return '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>CSI Radar · Session comparison</title>
<style>body{background:#101923;color:#edf2f8;font:16px system-ui;max-width:1000px;margin:40px auto;padding:0 20px}h1{font-size:32px}p{color:#acb9c9;line-height:1.6}section{background:#182532;border:1px solid #304355;border-radius:14px;padding:20px;margin:24px 0}svg{width:100%}table{width:100%;border-collapse:collapse}td,th{text-align:left;border-bottom:1px solid #304355;padding:12px}small{color:#acb9c9}</style>
<h1>CSI Radar <small>/ Ultimate analysis</small></h1><p>Recorded measurements · schema 1/2 · offline report</p><section><h2>Session comparison</h2><table><tr><th>Session</th><th>Motion starts</th><th>Valid seconds</th><th>Moving seconds</th><th>Rejected rows</th></tr>'''+rows+'''</table></section>'''+panels+'''<p>Green: motion score. Amber: applied threshold. Valid duration includes adjacent fresh samples only; long gaps are excluded. Plot is reduced to at most 4096 points. Motion starts are observed transitions, including recovery into motion. Rotation files are analyzed separately. These metrics do not measure false alarms or detection accuracy without external ground-truth labels. No position, person count or static presence is inferred.</p></html>'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('sessions', type=Path, nargs='+')
    p.add_argument('--output', type=Path, default=Path('session-report.html'))
    args = p.parse_args()
    if len(args.sessions) > 8:
        p.error('At most 8 sessions per comparison')
    sessions = [load(path) for path in args.sessions]
    with args.output.open('x', encoding='utf-8') as f:
        f.write(report(sessions))
    print(json.dumps([{k:v for k,v in s.items() if k != 'points'} for s in sessions], indent=2))


if __name__ == '__main__':
    main()
