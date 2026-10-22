#!/usr/bin/env python3
"""Package matched Ultimate test binaries/source with SHA256, excluding local builds."""
from pathlib import Path
import hashlib
import zipfile
root = Path(__file__).resolve().parents[1]
out = root/'release-output'
out.mkdir(exist_ok=True)
archive=out/'flipper-csi-v2.0.0.zip'
excluded={'.git','build','build-ultimate','dist','.vscode','__pycache__','release-output'}
files=[p for p in sorted(root.rglob('*')) if p.is_file()
       and not any(part in excluded or part.startswith('build-') for part in p.relative_to(root).parts)
       and p.name not in {'sdkconfig','sdkconfig.old','SHA256SUMS.txt'}
       and not p.name.endswith('.log')]
import json
receipt=json.loads((root/'release-inputs.json').read_text())
for name,identity in receipt['artifacts'].items():
    path=root/name
    assert path.stat().st_size==identity['bytes'], f'Size mismatch: {name}'
    assert hashlib.sha256(path.read_bytes()).hexdigest()==identity['sha256'], f'Hash mismatch: {name}'
assert (root/'ready/csi_radar.fap').stat().st_size==56108
if (root/'flipper_app/dist/csi_radar.fap').exists():
    assert (root/'ready/csi_radar.fap').read_bytes()==(root/'flipper_app/dist/csi_radar.fap').read_bytes(), 'Ready FAP must match the builder output'
manifest=''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(root).as_posix()}\n' for p in files)
(root/'SHA256SUMS.txt').write_text(manifest)
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for p in files+[root/'SHA256SUMS.txt']:
        z.write(p,Path('flipper-csi')/p.relative_to(root))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for line in manifest.splitlines():
        digest,name=line.split('  ',1)
        assert hashlib.sha256(z.read('flipper-csi/'+name)).hexdigest()==digest
(out/'SHA256SUMS.txt').write_text(''.join(
    hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n'
    for p in [root/'ready/csi_radar.fap',archive]))
import shutil
shutil.copyfile(root/'ready/csi_radar.fap',out/'csi_radar.fap')
print(archive)
print(f'{len(files)} files verified; {archive.stat().st_size} bytes')
