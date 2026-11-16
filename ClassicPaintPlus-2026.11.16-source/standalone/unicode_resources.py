"""Give rc.exe Unicode input, avoiding UTF-8 corruption in localized captions.

Sources stay readable UTF-8. Only local .rc includes are expanded; SDK headers,
conditional language macros and the resource compiler's normal syntax remain.
"""
from pathlib import Path
import re, sys

root, output = Path(sys.argv[1]).resolve(), Path(sys.argv[2])
def expand(path, stack=()):
    if path in stack:
        raise RuntimeError(f'Recursive resource include: {path}')
    text=path.read_text('utf-8-sig')
    text=re.sub(r'^\s*#pragma\s+code_page\([^\n]*\)\s*$', '', text, flags=re.M)
    def include(match):
        name=match[1]
        if not name.lower().endswith('.rc'):
            return match[0]
        child=(path.parent/name).resolve()
        child.relative_to(root)
        return '\n'+expand(child,stack+(path,))+'\n'
    return re.sub(r'^\s*#include\s+"([^"]+)"\s*$',include,text,flags=re.M)

data=('// Generated from the UTF-8 resource sources.\n'+expand(root/'rsrc.rc')).encode('utf-16')
if not output.exists() or output.read_bytes()!=data:
    output.write_bytes(data)
