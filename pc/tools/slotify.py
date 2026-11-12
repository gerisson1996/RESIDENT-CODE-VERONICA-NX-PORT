#!/usr/bin/env python3
"""64-bit port aid: turn the pointer members of the game's structures into 32-bit slots.

The game overlays its structures on disc data and on shared work buffers and reads them at
numeric offsets, so they must keep the EE's layout on a 64-bit host. This tool rewrites

  - the member declarations:  T *name;  ->  ps2ptr_t name; /* T *name */
  - every use (found with libclang, on the 32-bit build's compile commands):
        read      X->f            ->  P32(T, X->f)
        write     X->f = v        ->  X->f = S32(v)
        X->f += n                 ->  P32_ADD(T, X->f, n)
        X->f++ / ++X->f / ...     ->  P32_POSTINC(T, X->f) / P32_PREINC(T, X->f) / ...
    (macros in pc/include/pc_ptr.h; all are plain casts on a 32-bit host)

Which structures: the names in STRUCTS (one per line, '#' comments).

  slotify.py scan  COMPILE_COMMANDS            list structures with pointer members and where they are declared
  slotify.py apply COMPILE_COMMANDS STRUCTS    rewrite declarations and uses in place; prints what needs hand editing

Run with the python that has clang.cindex (MSYS2: /clang64/bin/python). Idempotent only in the
sense that a converted member is no longer a pointer: run it once per structure.
"""
import json, os, re, shlex, sys, collections
import clang.cindex as ci
from clang.cindex import CursorKind as K, TypeKind as T

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..')).replace(chr(92), '/')
BS = chr(92)

def norm(p):
    return os.path.abspath(p).replace(BS, '/')

def in_project(path):
    p = norm(path)
    return p.startswith(ROOT + '/decomp/') or p.startswith(ROOT + '/pc/')

def tu_args(entry):
    args = shlex.split(entry['command'].replace(BS, '/'))[1:]
    out = []; skip = False
    src = entry['file'].replace(BS, '/')
    for a in args:
        if skip: skip = False; continue
        if a == '-o': skip = True; continue
        if a == '-c' or a == src or a in ('-fno-tree-vectorize', '-mfpmath=sse', '-Wno-return-mismatch'): continue
        out.append(a)
    out += ['--target=i686-w64-mingw32', '--sysroot=C:/msys64/mingw32',
            '-resource-dir', 'C:/msys64/clang64/lib/clang/23',
            '-Wno-error=int-conversion', '-Wno-error=incompatible-pointer-types',
            '-Wno-error=implicit-function-declaration', '-Wno-error=implicit-int', '-Wno-error=return-mismatch',
            '-Wno-error=incompatible-function-pointer-types', '-ferror-limit=0']
    return out

def record_name(c):
    """name of the structure a field belongs to (typedef name of an anonymous one), climbing out of nested anonymous records"""
    p = c.semantic_parent
    while p is not None and p.kind in (K.STRUCT_DECL, K.UNION_DECL):
        n = p.spelling or ''
        if not n or 'unnamed' in n or 'anonymous' in n:
            n = p.type.spelling or ''
            n = re.sub(r'^(struct|union)\s+', '', n)
        if n and 'unnamed' not in n and 'anonymous' not in n: return n
        p = p.semantic_parent
    return None

def record_names(c):
    """all names a field's structure goes by: its tag and the typedef naming it"""
    names = set()
    p = c.semantic_parent
    while p is not None and p.kind in (K.STRUCT_DECL, K.UNION_DECL):
        if p.spelling and 'unnamed' not in p.spelling and 'anonymous' not in p.spelling: names.add(p.spelling)
        t = re.sub(r'^(struct|union)\s+', '', p.type.spelling or '')
        if t and 'unnamed' not in t and 'anonymous' not in t: names.add(t)
        p = p.semantic_parent
    return names

def slot_type(t):
    """(element type, dims) if t is a pointer or an array of pointers, else None"""
    dims = []
    t = t.get_canonical() if t.kind != T.CONSTANTARRAY else t
    while t.kind == T.CONSTANTARRAY:
        dims.append(t.element_count); t = t.element_type
    c = t.get_canonical()
    if c.kind == T.POINTER: return t, dims
    return None

class Files:
    def __init__(self): self.text = {}; self.raw = {}
    def get(self, path):
        p = norm(path)
        if p not in self.text:
            self.raw[p] = open(p, 'rb').read(); self.text[p] = self.raw[p]   # bytes: libclang offsets are byte offsets
        return self.text[p]

def main():
    mode = sys.argv[1]
    cc = json.load(open(sys.argv[2]))
    wanted = set()
    if mode == 'apply':
        wanted = {l.split('#')[0].strip() for l in open(sys.argv[3])} - {''}
    only = sys.argv[4:] if mode == 'apply' else sys.argv[3:]
    files = Files()
    idx = ci.Index.create()
    structs = collections.defaultdict(lambda: [set(), set()])      # scan: name -> [decl files, fields]
    ins = collections.defaultdict(set)         # file -> {(offset, order, text, site)}; order: length of the wrapped text (outer wraps open first), -1 closes
    repl = collections.defaultdict(dict)       # file -> {(start, end): text}
    manual = set()
    decl_stmts = collections.defaultdict(dict) # file -> {stmt start: [(name, dims, typetext)]}
    seen_tu = 0

    def add_ins(f, off, order, text, site): ins[f].add((off, order, text, site))
    def src(f, a, b): return files.get(f)[a:b].decode('latin-1')

    def selected(field):
        return bool(record_names(field) & wanted)

    def type_text(field):
        st = slot_type(field.type)
        t = st[0]
        s = t.spelling
        if 'unnamed' in s or 'anonymous' in s: return None
        return s

    def handle_ref(chain):
        """chain: cursors from the TU down to a MEMBER_REF_EXPR naming a selected field"""
        m = chain[-1]; field = m.referenced
        f = m.extent.start.file
        if f is None or not in_project(f.name): return
        fn = norm(f.name)
        a, b = m.extent.start.offset, m.extent.end.offset
        text = src(fn, a, b)
        where = '%s:%d' % (fn[len(ROOT) + 1:], m.extent.start.line)
        if not re.search(r'(->|\.)\s*' + re.escape(field.spelling) + r'\s*$', text):
            manual.add((where, field.spelling, 'use inside a macro: ' + ' '.join(text.split())[:60])); return
        ty = type_text(field)
        if ty is None: manual.add((where, field.spelling, 'unnamed pointee type')); return
        # L: the pointer-typed lvalue (subscripts of an array member included)
        i = len(chain) - 1
        ndims = len(slot_type(field.type)[1])
        while ndims and i > 0 and chain[i - 1].kind == K.ARRAY_SUBSCRIPT_EXPR and chain[i - 1].extent.start.offset == a:
            i -= 1; ndims -= 1
        if ndims:
            manual.add((where, field.spelling, 'array member used without all subscripts')); return
        L = chain[i]; la, lb = L.extent.start.offset, L.extent.end.offset
        # skip parentheses and implicit nodes that add nothing but '(' ')'
        def bare(c):
            t = src(fn, c.extent.start.offset, c.extent.end.offset)
            return t.replace('(', '').replace(')', '').strip() == src(fn, la, lb).replace('(', '').replace(')', '').strip()
        j = i
        while j > 0 and chain[j - 1].kind in (K.PAREN_EXPR, K.UNEXPOSED_EXPR) and chain[j - 1].extent.start.file is not None \
                and norm(chain[j - 1].extent.start.file.name) == fn and bare(chain[j - 1]):
            j -= 1
        Lx = chain[j]; P = chain[j - 1] if j > 0 else None
        xa, xb = Lx.extent.start.offset, Lx.extent.end.offset
        site = (la, lb)
        def wrap(): add_ins(fn, la, lb - la, 'P32(%s, ' % ty, site); add_ins(fn, lb, -1, ')', site)
        if P is None: wrap(); return
        if P.extent.start.file is None or norm(P.extent.start.file.name) != fn: wrap(); return
        pa, pb = P.extent.start.offset, P.extent.end.offset
        kids = list(P.get_children())
        if P.kind in (K.BINARY_OPERATOR, K.COMPOUND_ASSIGNMENT_OPERATOR) and len(kids) == 2 and kids[0].extent.start.offset == xa and kids[0].extent.end.offset == xb:
            ra, rb = kids[1].extent.start.offset, kids[1].extent.end.offset
            op = src(fn, xb, ra).strip()
            if kids[1].extent.start.file is None or norm(kids[1].extent.start.file.name) != fn or ra < xb:
                manual.add((where, field.spelling, 'assignment with a macro on the right')); return
            if op == '=':
                add_ins(fn, ra, rb - ra + 0.5, 'S32(', site); add_ins(fn, rb, -1, ')', site); return
            if op in ('+=', '-='):
                opoff = files.get(fn).index(op.encode(), xb)
                add_ins(fn, xa, pb - pa, 'P32_ADD(%s, ' % ty, site)
                repl[fn][(opoff, opoff + 2)] = ', ' if op == '+=' else ', -('
                add_ins(fn, rb, -1, ')' if op == '+=' else '))', site); return
            if op in ('==', '!=', '<', '>', '<=', '>=', '&&', '||', '-', '+', ',', '&', '|'):
                wrap(); return
            manual.add((where, field.spelling, 'operator ' + op)); return
        if P.kind == K.UNARY_OPERATOR:
            pre = src(fn, pa, xa).strip(); post = src(fn, xb, pb).strip()
            if pre == '&': manual.add((where, field.spelling, 'address of the member is taken')); return
            if pre in ('++', '--'):
                opoff = files.get(fn).index(pre.encode(), pa)
                repl[fn][(opoff, opoff + 2)] = ('P32_PREINC(%s, ' if pre == '++' else 'P32_PREDEC(%s, ') % ty
                add_ins(fn, xb, -1, ')', site); return
            if post in ('++', '--'):
                opoff = files.get(fn).index(post.encode(), xb)
                add_ins(fn, xa, pb - pa, ('P32_POSTINC(%s, ' if post == '++' else 'P32_POSTDEC(%s, ') % ty, site)
                repl[fn][(opoff, opoff + 2)] = ')'; return
            wrap(); return
        if P.kind == K.CSTYLE_CAST_EXPR:
            ct = P.type.get_canonical()
            if ct.kind not in (T.POINTER,) and src(fn, pa, xa).strip().startswith('('):
                return   # (int)X->f: the slot already is the PS2 address
            wrap(); return
        if P.kind == K.CXX_UNARY_EXPR: return   # sizeof
        wrap()

    def walk(c, chain):
        for ch in c.get_children():
            chain.append(ch)
            if ch.kind == K.MEMBER_REF_EXPR:
                r = ch.referenced
                if r is not None and r.kind == K.FIELD_DECL and slot_type(r.type) and selected(r):
                    handle_ref(chain)
            walk(ch, chain)
            chain.pop()

    def collect_fields(c):
        for ch in c.get_children():
            if ch.kind in (K.STRUCT_DECL, K.UNION_DECL, K.TYPEDEF_DECL):
                collect_fields(ch)
            elif ch.kind == K.FIELD_DECL:
                st = slot_type(ch.type)
                f = ch.extent.start.file
                if st and f is not None and in_project(f.name):
                    fn = norm(f.name)
                    if mode == 'scan':
                        for n in record_names(ch): structs[n][0].add(fn[len(ROOT) + 1:]); structs[n][1].add(ch.spelling)
                    elif selected(ch):
                        d = decl_stmts[fn].setdefault(ch.extent.start.offset, {})
                        d[ch.spelling] = (st[1], ch.extent.end.offset)
                # nested anonymous struct members
                collect_fields(ch)

    for e in cc:
        p = norm(e['file'])
        if not in_project(p) or not p.endswith('.c'): continue
        if only and not any(p.endswith(o) for o in only): continue
        tu = idx.parse(e['file'], args=tu_args(e))
        seen_tu += 1
        errs = [d for d in tu.diagnostics if d.severity >= 3]
        if errs: print('note: %s: %d parse errors, first: %s' % (p[len(ROOT) + 1:], len(errs), errs[0].spelling[:100]), file=sys.stderr)
        for top in tu.cursor.get_children():
            f = top.location.file
            if f is None or not in_project(f.name): continue
            if top.kind in (K.STRUCT_DECL, K.UNION_DECL, K.TYPEDEF_DECL): collect_fields(top)
            if mode == 'apply' and top.kind in (K.FUNCTION_DECL, K.VAR_DECL):
                chain = [top]; walk(top, chain)
        sys.stderr.write('.'); sys.stderr.flush()
    sys.stderr.write('\n')

    if mode == 'scan':
        for n, (fs, fields) in sorted(structs.items()):
            print('%s\t%s\t%s' % (n, ','.join(sorted(fs)), ' '.join(sorted(fields))))
        return

    # declarations: whole statement "T *a, *b[3];" -> "ps2ptr_t a, b[3]; /* ... */"
    for fn, stmts in decl_stmts.items():
        data = files.get(fn)
        for start, fields in stmts.items():
            end = data.index(b';', max(e for _, e in fields.values()))
            orig = ' '.join(data[start:end].decode('latin-1').split())
            # every declarator of the statement must be one of the pointer members
            ndecl = len(re.findall(r',', re.sub(r'\([^()]*\)', '', re.sub(r'\([^()]*\)', '', orig)))) + 1
            if ndecl != len(fields) or '{' in orig:
                manual.add(('%s:%d' % (fn[len(ROOT) + 1:], data.count(b'\n', 0, start) + 1), ' '.join(fields), 'declaration needs hand editing: ' + orig[:70])); continue
            decls = ', '.join(n + ''.join('[%d]' % d for d in dims) for n, (dims, _) in sorted(fields.items(), key=lambda kv: kv[1][1]))
            repl[fn][(start, end)] = 'ps2ptr_t %s; /* %s */' % (decls, orig.replace('/*', '').replace('*/', ''))
            # swallow the statement's own ';'
            repl[fn][(start, end + 1)] = repl[fn].pop((start, end))

    total = 0
    for fn in sorted(set(ins) | set(repl)):
        data = files.get(fn)
        edits = [(off, off, order, t) for (off, order, t, _) in ins[fn]] + [(a, b, 0, t) for (a, b), t in repl[fn].items()]
        # apply from the end; at one offset closing texts (negative order) go before opening ones
        edits.sort(key=lambda e: (-e[0], e[2]))
        for a, b, order, t in edits:
            data = data[:a] + t.encode('latin-1') + data[b:]
        open(fn, 'wb').write(data); total += len(edits)
    print('%d translation units, %d edits in %d files' % (seen_tu, total, len(set(ins) | set(repl))))
    for m in sorted(manual): print('MANUAL %s  %s  (%s)' % m)

if __name__ == '__main__':
    main()
