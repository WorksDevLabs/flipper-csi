#!/usr/bin/env python3
"""Offline real-RAW waterfall and classic/robust comparison. No accuracy claims.
Uses decimated RAW (<=10Hz); not a bit-exact live 20Hz replay.
"""
import argparse
import html
import json
from pathlib import Path
from replay import parse_raw, Replay
from session_report import chart

def analyze(path, threshold):
    engines = [Replay(threshold, mode) for mode in ('classic','robust')]
    points = [[],[]]
    peaks = [0.0,0.0]
    starts = [0,0]
    previous = ['WARMUP','WARMUP']
    waterfall = []
    accepted = rejected = 0
    with Path(path).open(encoding='utf-8', errors='replace') as source:
        for line in source:
            try:
                sample = parse_raw(line)
            except (ValueError, OverflowError):
                rejected += 1
                continue
            if sample is None:
                continue
            stamp, rssi, amplitudes = sample
            accepted += 1
            for i, engine in enumerate(engines):
                state = engine.feed(stamp, amplitudes)
                starts[i] += state == 'MOVING' and previous[i] != 'MOVING'
                previous[i] = state
                peaks[i] = max(peaks[i],engine.score)
                points[i].append((stamp,round(engine.score*1000),round(threshold*1000),state!='WARMUP'))
                if len(points[i]) >= 4096:
                    points[i] = points[i][::2]
            waterfall.append([round(min(4,v),3) for v in amplitudes])
            if len(waterfall) > 200:
                waterfall.pop(0)
    return dict(source=Path(path).name,accepted=accepted,rejected=rejected,peaks=peaks,
                starts=starts,points=points,waterfall=waterfall)

def render(data):
    title=html.escape(data['source'])
    rows=[]
    for y,values in enumerate(data['waterfall']):
        for x,value in enumerate(values):
            level=max(0,min(255,round(value*64)))
            rows.append(f'<rect x="{x*12}" y="{y*2}" width="12" height="2" fill="rgb({level},{level},{level})"/>')
    heat='<svg viewBox="0 0 564 400" role="img" aria-label="Actual normalized subcarrier amplitude">'+''.join(rows)+'</svg>'
    sections=''.join(f'<section><h2>{mode}</h2><p>Motion starts: {data["starts"][i]} · Peak score: {data["peaks"][i]:.4f}</p>{chart(data["points"][i])}</section>' for i,mode in enumerate(('Classic','Robust median/MAD')))
    return '''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>CSI Radar capture analysis</title><style>body{background:#101923;color:#edf2f8;font:16px system-ui;max-width:960px;margin:40px auto;padding:20px}section{background:#182532;padding:24px;margin:24px 0;border-radius:12px}svg{width:100%;max-height:420px}p{line-height:1.6;color:#acb9c9}</style>'''+f'<h1>CSI Radar / Capture analysis</h1><p>{title} · Accepted {data["accepted"]} · Rejected {data["rejected"]}</p>'+sections+'<section><h2>Recorded subcarriers / last 200 valid samples</h2>'+heat+'</section><p>Waterfall shows actual normalized amplitudes of 47 LLTF data bins; darkness encodes amplitude 0..4, newest row below. No location is measured. Green: estimated score. Amber: threshold. RAW is decimated to at most 10Hz; live processing is 20Hz. This diagnostic comparison does not establish accuracy or false alarms without independent labels. Timing gaps reset replay. The robust filter remains optional until real-room comparisons establish benefit.</p>'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture',type=Path)
    p.add_argument('--threshold',type=float,default=0.12)
    p.add_argument('--output',type=Path,default=Path('capture-report.html'))
    args=p.parse_args()
    if not 0.005<=args.threshold<=2: p.error('threshold must be .005..2')
    data=analyze(args.capture,args.threshold)
    with args.output.open('x',encoding='utf-8') as f: f.write(render(data))
    print(json.dumps({k:v for k,v in data.items() if k not in ('points','waterfall')},indent=2))
if __name__=='__main__': main()
