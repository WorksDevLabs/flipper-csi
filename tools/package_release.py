#!/usr/bin/env python3
"""Package the pinned release without build folders or private local settings."""
from pathlib import Path
import hashlib
import zipfile

root = Path(__file__).resolve().parents[1]
out = root / 'release-output'
out.mkdir(exist_ok=True)
archive = out / 'flipper-csi-v1.0.0.zip'
excluded = {'.git', 'build', 'dist', '.vscode', '__pycache__', 'release-output'}
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for file in sorted(root.rglob('*')):
        rel = file.relative_to(root)
        if not file.is_file() or any(p in excluded for p in rel.parts):
            continue
        if file.name in {'sdkconfig', 'sdkconfig.old'}:
            continue
        z.write(file, Path('flipper-csi') / rel)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
files = [root / 'ready/csi_radar.fap', archive]
(out / 'SHA256SUMS.txt').write_text(''.join(
    hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n' for p in files
))
print(archive)
