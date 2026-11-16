"""Create reproducible portable/source archives; never include a user's INI."""
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED
import argparse
import hashlib
import json
import re
import struct

root = Path(__file__).resolve().parents[1]
version = re.search(r'CLASSIC_PAINT_PLUS_VERSION_TEXT "([0-9.]+)"',
                    (root/'version.h').read_text()).group(1)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--x64', required=True, type=Path, help='x64 ClassicPaintPlus.exe')
parser.add_argument('--x86', required=True, type=Path, help='x86 ClassicPaintPlus.exe')
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
out = args.output.resolve()
if out == root or root in out.parents:
    parser.error('Use an output directory outside the source tree')
out.mkdir(parents=True, exist_ok=True)
timestamp = tuple(map(int, version.split('.'))) + (0, 0, 0)
docs = ['README.md', 'README.ru.md', 'BUILDING.md', 'CHANGELOG.md', 'AUTHORS.txt',
        'NOTICE.txt', 'COPYING', 'JPEG-NOTICES.txt', 'IMAGE-NOTICES.txt',
        'PAINT-SERVICE-DATA.txt']
manuals = ['ClassicPaintPlus-help.html', 'ClassicPaintPlus-help.en.html']

def archive(path, entries):
    with ZipFile(path, 'w', ZIP_DEFLATED, compresslevel=9) as z:
        for name, source in sorted(entries):
            assert not name.startswith('/') and '..' not in Path(name).parts
            entry = ZipInfo(name, timestamp)
            entry.compress_type = ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            z.writestr(entry, source.read_bytes())
    with ZipFile(path) as z:
        assert z.testzip() is None
        assert not any(Path(n).name.lower() == 'config.ini' for n in z.namelist())
    return {'file': path.name, 'bytes': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

assets = []
for arch, executable, machine in [('x64', args.x64, 0x8664), ('x86', args.x86, 0x14c)]:
    data = executable.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[pe:pe+4] == b'PE\0\0' and struct.unpack_from('<H', data, pe+4)[0] == machine
    prefix = f'ClassicPaintPlus-{version}-{arch}/'
    entries = [(prefix+'ClassicPaintPlus.exe', executable)]
    entries += [(prefix+name, root/name) for name in docs]
    entries += [(prefix+name, root/'userguide'/name) for name in manuals]
    assets.append(archive(out/f'ClassicPaintPlus-{version}-windows-{arch}.zip', entries))

excluded = {'.git', '.vs', '__pycache__', 'build', 'out'}
entries = []
for path in root.rglob('*'):
    if not path.is_file(): continue
    relative = path.relative_to(root)
    if any(part in excluded for part in relative.parts): continue
    if path.name.lower() == 'config.ini' or path.suffix.lower() in {'.pyc', '.obj', '.pdb', '.ilk', '.log', '.user', '.suo'}: continue
    if relative.as_posix() in ('userguide/RPaint+help.html', 'userguide/RPaint+help.en.html'): continue
    entries.append((f'ClassicPaintPlus-{version}-source/'+relative.as_posix(), path))
assets.append(archive(out/f'ClassicPaintPlus-{version}-source.zip', entries))
(out/'SHA256SUMS.txt').write_text(''.join(f"{a['sha256']}  {a['file']}\n" for a in assets), encoding='ascii')
(out/'RELEASE-NOTES.md').write_bytes((root/'RELEASE-NOTES.md').read_bytes())
(out/'release-manifest.json').write_text(json.dumps({'version': version, 'tag': 'v'+version,
    'title': 'Classic Paint Plus '+version, 'assets': assets}, indent=2)+'\n', encoding='utf-8')
for asset in assets: print(asset['file'], asset['bytes'], asset['sha256'])
