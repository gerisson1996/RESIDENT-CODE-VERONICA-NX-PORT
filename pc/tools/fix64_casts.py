#!/usr/bin/env python3
"""64-bit port aid: rewrite the pointer<->integer casts GCC reports in a build log.

  fix64_casts.py BUILD.log [--dry] [--only=SUBSTRING]   (--only: just the files whose path contains it)

For every "cast from pointer to integer of different size" at FILE:LINE:COL the cast
"(int)EXPR" becomes "(int)PS2_ADDR(EXPR)"; for every "cast to pointer from integer of different
size", "(T*)EXPR" becomes "(T*)PS2_HOST(EXPR)" (pc/include/pc_ptr.h). EXPR is the cast's operand
(a unary expression), found by scanning the source line. Sites it cannot parse (operand
continues on the next line, column not on a cast) are listed for hand editing.
"""
import re, sys, collections

ERR = re.compile(r'^(.*?):(\d+):(\d+): error: cast (from pointer to integer|to pointer from integer) of different size')
ID = re.compile(r'[A-Za-z_]\w*')
NUM = re.compile(r'(0[xX][0-9A-Fa-f]+|\d+\.?\d*([eE][-+]?\d+)?)[uUlLfF]*')

def close_paren(s, i):
    """s[i] == '(' -> index of its ')', or -1"""
    d = 0
    q = None
    while i < len(s):
        c = s[i]
        if q:
            if c == chr(92): i += 1
            elif c == q: q = None
        elif c in '\'"': q = c
        elif c in '([': d += 1
        elif c in ')]':
            d -= 1
            if d == 0: return i
        i += 1
    return -1

def skip_ws(s, i):
    while i < len(s) and s[i] in ' \t': i += 1
    return i

def operand_end(s, i):
    """index just past the unary expression starting at s[i], or -1"""
    i = skip_ws(s, i)
    while True:   # prefix operators and further casts
        if s.startswith(('++', '--'), i): i = skip_ws(s, i + 2); continue
        if i < len(s) and s[i] in '&*-~!+': i = skip_ws(s, i + 1); continue
        if i < len(s) and s[i] == '(':
            j = close_paren(s, i)
            if j < 0: return -1
            k = skip_ws(s, j + 1)
            inner = s[i + 1:j].strip()
            # a cast: "(type)" followed by something that starts an operand
            if k < len(s) and (s[k].isalnum() or s[k] in '_(&*~!\'"-') and re.fullmatch(r'(const\s+|unsigned\s+|signed\s+|struct\s+|volatile\s+)*[A-Za-z_]\w*(\s+(int|char|short|long))*[\s\*]*', inner) and not (s[k] in '-&*' and not inner.endswith('*') and not re.fullmatch(r'(unsigned|signed|int|char|short|long|float|double|u_int|u_long|u_char|u_short|Uint32|Sint32|Uint16|Sint16|Uint8|Sint8|uintptr_t|intptr_t|size_t|[\w ]*\*)[\w \*]*', inner)):
                i = k; continue
        break
    if i >= len(s): return -1
    if s[i] == '(':
        j = close_paren(s, i)
        if j < 0: return -1
        i = j + 1
    elif s[i] in '\'"':
        q = s[i]; i += 1
        while i < len(s) and s[i] != q:
            if s[i] == chr(92): i += 1
            i += 1
        i += 1
    else:
        m = ID.match(s, i) or NUM.match(s, i)
        if not m: return -1
        i = m.end()
    while True:   # postfix
        k = skip_ws(s, i)
        if s.startswith('->', k): m = ID.match(s, skip_ws(s, k + 2)); 
        elif k < len(s) and s[k] == '.' : m = ID.match(s, skip_ws(s, k + 1))
        else: m = None
        if m: i = m.end(); continue
        if k < len(s) and s[k] in '[(':
            j = close_paren(s, k)
            if j < 0: return -1
            i = j + 1; continue
        if s.startswith(('++', '--'), k): i = k + 2; continue
        return i

def main():
    log = sys.argv[1]; dry = '--dry' in sys.argv
    sites = collections.defaultdict(set)
    for line in open(log, encoding='utf-8', errors='replace'):
        m = ERR.match(line.strip())
        if m: sites[m.group(1)].add((int(m.group(2)), int(m.group(3)), 'PS2_ADDR' if m.group(4).startswith('from') else 'PS2_HOST'))
    done = 0; manual = []
    only = [a[7:] for a in sys.argv if a.startswith('--only=')]
    for path, ss in sorted(sites.items()):
        if only and not any(o in path.replace(chr(92), '/') for o in only): continue
        raw = open(path, 'rb').read()
        nl = b'\r\n' if b'\r\n' in raw else b'\n'
        lines = raw.decode('latin-1').split(nl.decode())
        byline = collections.defaultdict(list)
        for ln, col, fn in ss: byline[ln].append((col, fn))
        for ln, lst in byline.items():
            s = lines[ln - 1]; ins = []; bad = False
            for col, fn in lst:
                # GCC's column is in display cells: tabs expand to the next multiple of 8
                disp = 0; i = 0
                while i < len(s) and disp < col - 1:
                    disp = (disp // 8 + 1) * 8 if s[i] == '\t' else disp + 1
                    i += 1
                if i >= len(s) or s[i] != '(': manual.append((path, ln, col, fn, 'not at a cast')); bad = True; continue
                j = close_paren(s, i)
                e = operand_end(s, j + 1) if j >= 0 else -1
                if e < 0: manual.append((path, ln, col, fn, 'operand not on this line')); bad = True; continue
                if s[j + 1:e].strip().startswith(fn + '('): continue   # already done
                ins.append((j + 1, fn + '(')); ins.append((e, ')'))
            # closing parens before opening ones at the same offset
            for pos, txt in sorted(ins, key=lambda t: (-t[0], t[1] != ')')):
                s = s[:pos] + txt + s[pos:]
            if ins: lines[ln - 1] = s; done += len(ins) // 2
        if not dry: open(path, 'wb').write(nl.decode().join(lines).encode('latin-1'))
    print('rewritten: %d casts in %d files' % (done, len(sites)))
    for m in sorted(manual): print('MANUAL %s:%d:%d %s (%s)' % m)

if __name__ == '__main__':
    main()
