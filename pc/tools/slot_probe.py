import sys, json, shlex, os
import clang.cindex as ci
from clang.cindex import CursorKind as K, TypeKind as T
cc = json.load(open('build32/compile_commands.json'))
want = sys.argv[1]
e = [c for c in cc if c['file'].replace(chr(92), '/').endswith(want)][0]
args = [a for a in shlex.split(e['command'].replace(chr(92), '/'))[1:]]
out = []; skip = False
for a in args:
    if skip: skip = False; continue
    if a in ('-o',): skip = True; continue
    if a == '-c' or a.endswith(want) or a in ('-fno-tree-vectorize', '-mfpmath=sse', '-Wno-return-mismatch'): continue
    out.append(a)
out += ['--target=i686-w64-mingw32', '--sysroot=C:/msys64/mingw32', '-Wno-error=int-conversion', '-Wno-error=incompatible-pointer-types',
        '-Wno-error=implicit-function-declaration', '-Wno-error=implicit-int', '-Wno-error=return-mismatch', '-Wno-error=incompatible-function-pointer-types', '-ferror-limit=0']
idx = ci.Index.create()
tu = idx.parse(e['file'], args=out)
errs = [d for d in tu.diagnostics if d.severity >= 3]
print('errors:', len(errs))
for d in errs[:15]: print('  ', d.location.file, d.location.line, d.spelling[:150])
n = 0
def walk(c, depth=0):
    global n
    for ch in c.get_children():
        if ch.kind == K.MEMBER_REF_EXPR:
            r = ch.referenced
            if r is not None and r.kind == K.FIELD_DECL:
                t = r.type.get_canonical()
                while t.kind == T.CONSTANTARRAY: t = t.element_type.get_canonical()
                if t.kind == T.POINTER and ch.location.file and ch.location.file.name.replace(chr(92),'/').endswith(want):
                    n += 1
                    if n <= 12: print(ch.location.line, ch.location.column, r.spelling, '|', r.type.spelling, '| in', r.semantic_parent.spelling or r.semantic_parent.type.spelling, '| parent', c.kind.name)
        walk(ch, depth + 1)
walk(tu.cursor)
print('pointer member refs:', n)
