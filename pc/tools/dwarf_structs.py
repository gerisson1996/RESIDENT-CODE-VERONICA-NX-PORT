#!/usr/bin/env python3
"""64-bit port aid: struct layouts of a binary from its DWARF (objdump --dwarf=info on stdin).

  objdump --dwarf=info recvx.exe | dwarf_structs.py > layout.tsv

One line per named struct/union (or typedef of an anonymous one):
  name <TAB> byte_size <TAB> member:offset:size_or_ptr ...   (pointer members are marked '*')
Compare the 32-bit and the 64-bit file with dwarf_diff (see --diff A.tsv B.tsv).
"""
import re, sys

def parse(f):
    dies = {}; stack = []; cur = None
    hdr = re.compile(r'^ <(\d+)><([0-9a-f]+)>: Abbrev Number: \d+ \((DW_TAG_\w+)\)')
    att = re.compile(r'^\s+<[0-9a-f]+>\s+(DW_AT_\w+)\s*:\s*(.*)$')
    for line in f:
        m = hdr.match(line)
        if m:
            depth = int(m.group(1)); off = int(m.group(2), 16)
            cur = {'tag': m.group(3)[7:], 'off': off, 'kids': []}
            dies[off] = cur
            del stack[depth:]
            if depth and stack and len(stack) >= depth: stack[depth - 1]['kids'].append(cur)
            stack.append(cur)
            continue
        if line.startswith(' <') and 'Abbrev Number: 0' in line: cur = None; continue
        m = att.match(line)
        if m and cur is not None:
            k, v = m.group(1)[6:], m.group(2).strip()
            if k == 'name': v = v.split('): ')[-1] if v.startswith('(') else v
            elif k == 'type': mm = re.search(r'<0x([0-9a-f]+)>', v); v = int(mm.group(1), 16) if mm else None
            elif k in ('byte_size', 'data_member_location'):
                try: v = int(v.split()[0], 0)
                except ValueError: v = None
            cur[k] = v
    return dies

def is_ptr(dies, t, depth=0):
    d = dies.get(t)
    while d is not None and depth < 20:
        if d['tag'] == 'pointer_type': return True
        if d['tag'] in ('typedef', 'const_type', 'volatile_type', 'array_type'): d = dies.get(d.get('type')); depth += 1; continue
        return False
    return False

def dump(dies):
    out = {}
    def emit(name, d):
        if d.get('byte_size') is None: return
        mem = []
        for k in d['kids']:
            if k['tag'] != 'member' or 'name' not in k: continue
            mem.append('%s:%s%s' % (k['name'], k.get('data_member_location', 0), '*' if is_ptr(dies, k.get('type')) else ''))
        out[name] = (d['byte_size'], mem)
    for d in dies.values():
        if d['tag'] in ('structure_type', 'union_type') and 'name' in d: emit(d['name'], d)
        elif d['tag'] == 'typedef' and 'name' in d:
            t = dies.get(d.get('type'))
            if t is not None and t['tag'] in ('structure_type', 'union_type') and 'name' not in t: emit(d['name'], t)
    return out

if len(sys.argv) > 1 and sys.argv[1] == '--diff':
    def load(p):
        r = {}
        for l in open(p):
            a = l.rstrip('\n').split('\t'); r[a[0]] = (int(a[1]), a[2].split() if len(a) > 2 else [])
        return r
    A, B = load(sys.argv[2]), load(sys.argv[3])
    for n in sorted(A):
        if n in B and (A[n][0] != B[n][0] or A[n][1] != B[n][1]):
            ptrs = [m.split(':')[0] for m in A[n][1] if m.endswith('*')]
            print('%s\t%d\t%d\t%s' % (n, A[n][0], B[n][0], ' '.join(ptrs)))
else:
    for n, (sz, mem) in sorted(dump(parse(sys.stdin)).items()):
        print('%s\t%d\t%s' % (n, sz, ' '.join(mem)))
