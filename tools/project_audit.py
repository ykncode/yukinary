from pathlib import Path
import re, json, hashlib

ROOT = Path(__file__).resolve().parents[1]
SKIP = {'.git','trash','archive','temp_extract','build','dist','__pycache__'}
EXTS = {'.cpp','.c','.h','.hpp','.ykn','.md','.json','.toml','.py','.ps1','.bat','.sh','.txt'}

def files():
    for p in ROOT.rglob('*'):
        if not p.is_file() or any(part in SKIP for part in p.parts):
            continue
        if p.suffix.lower() in EXTS:
            yield p

def read(p):
    try: return p.read_text(encoding='utf-8', errors='replace')
    except Exception as e: return f'[[READ_ERROR {e}]]'

rows=[]; duplicates={}; patterns={}
for p in sorted(files()):
    text=read(p); lines=text.splitlines()
    rel=str(p.relative_to(ROOT)).replace('\\','/')
    h=hashlib.sha256(text.encode('utf-8','replace')).hexdigest()
    duplicates.setdefault(h, []).append(rel)
    rows.append({
        'path':rel,'ext':p.suffix.lower(),'bytes':p.stat().st_size,'lines':len(lines),
        'nonempty':sum(bool(x.strip()) for x in lines),'sha256':h[:12],
        'headings':[(i+1,x.strip()) for i,x in enumerate(lines) if re.match(r'^#{1,6}\\s+',x)],
        'todo':[(i+1,x.strip()) for i,x in enumerate(lines) if re.search(r'\\b(TODO|FIXME|HACK|XXX|BUG)\\b',x,re.I)],
        'includes':[(i+1,x.strip()) for i,x in enumerate(lines) if re.match(r'^\\s*#\\s*include',x)],
        'functions':sum(1 for x in lines if re.search(r'\)\s*(const|noexcept|override|\{|->|$)',x) and not x.strip().startswith('//')),
    })

out=ROOT/'audit_inventory.json'
out.write_text(json.dumps({'root':str(ROOT),'files':rows,'duplicate_groups':[v for v in duplicates.values() if len(v)>1]},ensure_ascii=False,indent=2),encoding='utf-8')

print('ROOT', ROOT)
print('FILE_COUNT', len(rows))
from collections import Counter
print('EXTENSIONS', Counter(r['ext'] for r in rows))
print('TOTAL_LINES', sum(r['lines'] for r in rows))
print('TOTAL_BYTES', sum(r['bytes'] for r in rows))
print('TOP_FILES_BY_LINES')
for r in sorted(rows,key=lambda x:x['lines'],reverse=True)[:40]: print(f"{r['lines']:7} {r['bytes']:9} {r['path']}")
print('DUPLICATES')
for g in duplicates.values():
    if len(g)>1: print(' | '.join(g))
print('TODO_COUNTS')
for r in rows:
    if r['todo']: print(r['path'],len(r['todo']),r['todo'][:8])
print('REPORT',out)
