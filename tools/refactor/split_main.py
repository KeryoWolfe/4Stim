#!/usr/bin/env python3
"""Split 4Stim's main.cpp: its anonymous namespace becomes namespace Core.

Core.h keeps every declaration in its original order: types, templates,
constants and inline helpers whole, globals as inline variables, and each
function definition as a prototype. Function bodies go to the .cpp chosen
for their line range (areas.txt), each including Core.h.

  split.py main.cpp areas.txt outdir         write the files
  split.py main.cpp --list                   list top-level entities
"""
import re
import sys
from pathlib import Path


def scan(text, start, depth0):
    """Yield (kind, a, b) pieces of text at nesting depth depth0 from start until the
    brace that closes it. kind 'gap' for whitespace/comments between entities."""
    i = start
    n = len(text)
    ent_start = None
    depth = depth0
    pending_block_end = None
    out = []
    while i < n:
        c = text[i]
        # comments and literals
        if text.startswith('//', i):
            j = text.find('\n', i)
            i = n if j < 0 else j
            continue
        if text.startswith('/*', i):
            i = text.find('*/', i) + 2
            continue
        m = re.match(r'R"([^(\s]*)\(', text[i:i + 40]) if c == 'R' and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == '_')) else None
        if m:
            end = text.find(')' + m.group(1) + '"', i)
            if ent_start is None:
                ent_start = i
            i = end + len(m.group(1)) + 2
            continue
        if c == '"' or (c == "'" and not (i > 0 and text[i - 1].isalnum())):
            if ent_start is None:
                ent_start = i
            q = c
            i += 1
            while text[i] != q:
                i += 2 if text[i] == '\\' else 1
            i += 1
            continue
        if c.isspace():
            i += 1
            continue
        if ent_start is None:
            if c == '}' and depth == depth0:
                return out, i  # closing brace of the enclosing block
            ent_start = i
        if c in '({[':
            depth += 1
        elif c in ')}]':
            depth -= 1
            if c == '}' and depth == depth0:
                # a block at our level closed: entity ends here unless a ';' follows (struct, braced init)
                j = i + 1
                k = j
                while k < n and text[k] in ' \t':
                    k += 1
                head = text[ent_start:i + 1]
                if k < n and text[k] == ';':
                    out.append((ent_start, k + 1))
                    i = k + 1
                else:
                    out.append((ent_start, i + 1))
                    i += 1
                ent_start = None
                continue
        elif c == ';' and depth == depth0:
            out.append((ent_start, i + 1))
            ent_start = None
        i += 1
    return out, i


def strip_comments(s):
    s = re.sub(r'//[^\n]*', '', s)
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    return s


def first_top_brace(code):
    depth = 0
    i = 0
    while i < len(code):
        c = code[i]
        if c in '"\'':
            q = c
            i += 1
            while code[i] != q:
                i += 2 if code[i] == '\\' else 1
        elif c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
        elif c == '{' and depth == 0:
            return i
        i += 1
    return -1


def has_top_eq(head):
    depth = 0
    for i, c in enumerate(head):
        if c in '([{<':
            depth += 1
        elif c in ')]}>':
            depth -= 1
        elif c == '=' and depth == 0 and head[i - 1] not in '=!<>' and (i + 1 >= len(head) or head[i + 1] != '='):
            if not re.search(r'operator\s*$', head[:i]):
                return True
    return False


FN_HEAD = re.compile(r'\)\s*(?:const\s*)?(?:noexcept(?:\([^)]*\))?\s*)?(?:->\s*[^{;]+?)?\s*$', re.S)


def classify(code):
    c = code.strip()
    if re.match(r'namespace\b', c):
        return 'type' if re.match(r'namespace\s+\w+\s*=', c) else 'namespace'
    if re.match(r'(template\b|struct\b|class\b|enum\b|union\b|using\b|typedef\b|static_assert\b|constexpr\b|consteval\b|inline\b|concept\b)', c):
        if re.match(r'(struct|class)\s+\w+\s*;$', c):
            return 'type'
        return 'type'
    b = first_top_brace(c)
    if b < 0:
        # ; terminated: prototype or variable
        body = c[:-1]
        if not has_top_eq(body) and FN_HEAD.search(body) and re.search(r'\w\s*\(', body):
            return 'proto'
        return 'var'
    head = c[:b]
    if not has_top_eq(head) and FN_HEAD.search(head) and re.search(r'\w\s*\(', head):
        return 'func'
    return 'var'


def drop_defaults(head):
    """Remove default arguments from the outermost parameter list of a function head."""
    # find the parameter list: the last top-level (...) before qualifiers
    depth = 0
    open_i = close_i = None
    for i, c in enumerate(head):
        if c == '(':
            if depth == 0:
                open_i = i
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                close_i = i
    params = head[open_i + 1:close_i]
    out = []
    depth = 0
    skipping = False
    i = 0
    while i < len(params):
        c = params[i]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            depth -= 1
        elif c == '<' and i > 0 and (params[i - 1].isalnum() or params[i - 1] in '_:'):
            depth += 1
        elif c == '>' and depth > 0 and not skipping:
            depth -= 1
        elif c == '>' and skipping and depth > 0:
            depth -= 1
        if depth == 0 and c == '=' and not skipping:
            skipping = True
            while out and out[-1] == ' ':
                out.pop()
            i += 1
            continue
        if depth == 0 and c == ',':
            skipping = False
        if not skipping:
            out.append(c)
        i += 1
    return head[:open_i + 1] + ''.join(out) + head[close_i:]


def parse(text, start, depth, ns):
    ents, end = scan(text, start, depth)
    items = []
    prev = start
    fixed = []
    for a, b in ents:
        m = re.match(r'[ \t]*//[^\n]*', text[b:])
        if m:
            b += m.end()
        fixed.append((a, b))
    ents = fixed
    for a, b in ents:
        lead = text[prev:a]
        raw = text[a:b]
        code = strip_comments(raw)
        kind = classify(code)
        line = text.count('\n', 0, a) + 1
        item = {'kind': kind, 'lead': lead, 'text': raw, 'line': line, 'ns': ns}
        if kind == 'namespace':
            m = re.match(r'namespace\s*([\w:]*)\s*\{', raw)
            name = m.group(1)
            inner_start = a + m.end()
            sub, sub_end = parse(text, inner_start, depth + 1, ns + ([name] if name else []))
            item['name'] = name
            item['children'] = sub
            item['tail'] = text[sub_end + 1:b] if False else ''
            item['inner_lead_end'] = text[sub_end:b]  # whitespace + closing brace
        items.append(item)
        prev = b
    return items, end


def name_of(item):
    code = strip_comments(item['text'])
    b = first_top_brace(code)
    head = code[:b] if b >= 0 else code
    m = re.search(r'([~\w:]+|operator\s*\S+)\s*\((?!.*\b\w+\s*\()', head, re.S)
    if item['kind'] in ('func', 'proto'):
        depth = 0
        for i, c in enumerate(head):
            if c == '(':
                if depth == 0:
                    mm = re.search(r'([~\w]+)\s*$', head[:i])
                    return mm.group(1) if mm else '?'
                depth += 1
            elif c == ')':
                depth -= 1
    m = re.search(r'(\w+)\s*(?:=|\{|;|\[)', head)
    return m.group(1) if m else '?'


def decl_name(item):
    code = strip_comments(item['text']).strip()
    code = re.sub(r'^template\s*<', '<', code)
    if code.startswith('<'):
        d = 0
        for i, ch in enumerate(code):
            if ch == '<': d += 1
            elif ch == '>':
                d -= 1
                if d == 0:
                    code = code[i + 1:].strip()
                    break
    m = re.match(r'(?:\[\[[^\]]*\]\]\s*)?(?:struct|class|union|enum(?:\s+class)?)\s+(?:__declspec\([^)]*\)\s*)?(\w+)', code)
    if m:
        return m.group(1)
    m = re.match(r'using\s+(\w+)\s*=', code)
    if m:
        return m.group(1)
    if re.match(r'(using|static_assert|namespace)\b', code):
        return None
    b = first_top_brace(code)
    head = code[:b] if b >= 0 else code[:-1]
    if item['kind'] in ('type',) and re.search(r'\w\s*\(', head) and not has_top_eq(head):
        # template or constexpr / inline function
        d = 0
        for i, ch in enumerate(head):
            if ch == '(':
                mm = re.search(r'(\w+)\s*$', head[:i])
                return mm.group(1) if mm else None
        return None
    m = re.search(r'(\w+)\s*(?:=|\{|$|\[)', head.strip())
    return m.group(1) if m else None


def flatten(items):
    for it in items:
        yield it
        if it['kind'] == 'namespace':
            yield from flatten(it['children'])


def main():
    src = Path(sys.argv[1]).read_text(encoding='utf-8')
    m = re.search(r'^namespace\s*\n\{', src, re.M)
    ns_open = m.end()
    items, ns_end = parse(src, ns_open, 0, [])
    # ns_end: index of the anonymous namespace's closing brace
    if sys.argv[2] == '--list':
        for it in flatten(items):
            print(f"{it['line']:6} {'/'.join(it['ns']) or '-':28} {it['kind']:9} {name_of(it)}")
        return
    areas = []
    for line in Path(sys.argv[2]).read_text().splitlines():
        line = line.split('#')[0].strip()
        if not line:
            continue
        lo, hi, path = line.split()
        areas.append((int(lo), int(hi), path))
    outdir = Path(sys.argv[3])

    def area_for(line):
        for lo, hi, path in areas:
            if lo <= line <= hi:
                return path
        raise SystemExit(f'no area for line {line}')

    # ---- which types and globals only one file uses: they move into it ----
    lines = src.split('\n')
    total = len(lines)
    ns_end_line = src.count('\n', 0, ns_end) + 1
    owner = {}
    flat = [it for it in flatten(items) if it['kind'] != 'namespace']
    for it in flat:
        it['end'] = it['line'] + it['text'].count('\n')
        if it['kind'] == 'func':
            rb = first_top_brace(it['text'])
            head_end = it['line'] + it['text'][:rb].count('\n')
            for l in range(it['line'], head_end + 1):
                owner[l] = 'header'
            for l in range(head_end + 1, it['end'] + 1):
                owner[l] = area_for(it['line'])
        else:
            for l in range(it['line'], it['end'] + 1):
                owner[l] = 'header'
    for l in range(ns_end_line, total + 1):
        owner[l] = 'main.cpp'
    cands = []
    for it in flat:
        if it['kind'] in ('type', 'var') and not it['ns']:
            nm = decl_name(it)
        elif it['kind'] in ('func', 'proto'):
            nm = name_of(it)
        else:
            nm = None
        if nm and nm != '?' and len(nm) > 2:
            pat = re.compile(r'\b' + re.escape(nm) + r'\b')
            refs = [i + 1 for i, t in enumerate(lines) if pat.search(re.sub(r'//.*', '', t))]
            it['refs'] = [r for r in refs if not (it['line'] <= r <= it['end'])]
            it['dname'] = nm
            if it['kind'] == 'func':
                rb = first_top_brace(it['text'])
                it['head_end'] = it['line'] + it['text'][:rb].count('\n')
            cands.append(it)
    # a function's own declarations and definitions don't count as uses of it
    spans = {}
    for it in cands:
        if it['kind'] in ('func', 'proto'):
            spans.setdefault(it['dname'], []).append((it['line'], it.get('head_end', it['end'])))
    funchome = {}
    for it in cands:
        if it['kind'] == 'func':
            funchome.setdefault(it['dname'], area_for(it['line']))
    for it in cands:
        if it['kind'] in ('func', 'proto'):
            it['refs'] = [r for r in it['refs'] if not any(a <= r <= b for a, b in spans[it['dname']])]
    changed = True
    while changed:
        changed = False
        for it in cands:
            if it.get('local'):
                continue
            owners = {owner.get(r, 'header') for r in it['refs']}
            if it['kind'] == 'proto' and it['dname'] in funchome:
                home = funchome[it['dname']]
                if owners <= {home}:
                    it['local'] = home
                    for l in range(it['line'], it['end'] + 1):
                        owner[l] = home
                    changed = True
                continue
            if it['kind'] == 'func':
                home = area_for(it['line'])
                if owners <= {home}:
                    it['local'] = home
                    for l in range(it['line'], it['head_end'] + 1):
                        owner[l] = home
                    changed = True
                continue
            if len(owners) == 1 and 'header' not in owners:
                f = owners.pop()
                it['local'] = f
                for l in range(it['line'], it['end'] + 1):
                    owner[l] = f
                changed = True
    for it in sorted([c for c in cands if not c.get('local') and c['kind'] in ('type','var')], key=lambda c: -c['text'].count('\n'))[:15]:
        print('  big kept:', it['kind'], it['dname'], it['line'], it['text'].count('\n'), sorted({owner.get(r, 'header') + ':' + str(r) for r in it['refs']})[:8])
    moved = [(it['dname'], it['local']) for it in cands if it.get('local')]
    print('local:', len(moved), 'kept in Core.h:', len(cands) - len(moved))

    header = []          # lines of Core.h body
    bodies = {}          # path -> list of (ns, text)

    def emit(items, ns, indent):
        for it in items:
            lead = it['lead']
            if it['kind'] == 'namespace':
                if not it['name']:
                    # nested anonymous namespace: flattened into Core
                    emit(it['children'], ns, indent)
                    continue
                header.append(lead + f"namespace {it['name']}\n" + '\t' * indent + '{')
                emit(it['children'], ns + [it['name']], indent + 1)
                header.append('\n' + '\t' * indent + '}')
                continue
            if it['kind'] == 'func':
                code = it['text']
                rb = first_top_brace(code)
                head = code[:rb].rstrip()
                body = code[rb:]
                h = re.sub(r'^(\s*)static\s+', r'\1', head)
                proto = re.sub(r'\s+', ' ', strip_comments(h)).strip()
                if not it.get('local'):
                    header.append('\n' + '\t' * indent + proto + ';')
                definition = drop_defaults(h)
                path = area_for(it['line'])
                bodies.setdefault(path, []).append((ns, lead, definition + code[len(head):rb] + body))
                continue
            if it.get('local') and it['kind'] != 'func':
                bodies.setdefault(it['local'], []).append((ns, lead, it['text']))
                continue
            if it['kind'] == 'var':
                t = it['text']
                t2 = re.sub(r'^static\s+', '', t)
                if not re.match(r'(inline|extern|constexpr|thread_local\s+inline)\b', t2):
                    t2 = 'inline ' + t2
                header.append(lead + t2)
                continue
            # type, proto: as they are
            header.append(lead + it['text'])

    emit(items, [], 1)
    pre = src[:m.start()]
    post = src[ns_end + 1:]
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / 'pre.txt').write_text(pre)
    (outdir / 'post.txt').write_text(post)
    (outdir / 'header_body.txt').write_text(''.join(header))
    import json
    (outdir / 'bodies.json').write_text(json.dumps({k: v for k, v in bodies.items()}))
    print('header lines', ''.join(header).count('\n'))
    for k, v in bodies.items():
        print(k, len(v), sum(x[2].count('\n') + x[1].count('\n') for x in v))





def assemble(outdir, srcdir):
    import json
    outdir = Path(outdir)
    pre = (outdir / 'pre.txt').read_text()
    post = (outdir / 'post.txt').read_text()
    hb = (outdir / 'header_body.txt').read_text()
    bodies = json.loads((outdir / 'bodies.json').read_text())
    srcdir = Path(srcdir)
    includes = pre.rstrip()
    header = ('#pragma once\n\n'
              '// What used to be main.cpp\'s anonymous namespace (roadmap 2b.3): every type,\n'
              '// constant, setting and piece of shared state, and a declaration of every\n'
              '// function, in their old order. The functions themselves are in src/Core/\n'
              '// and src/Natives/ by area, as main.cpp\'s "----" sections had them.\n\n'
              + includes + '\n\nnamespace Core\n{' + hb.rstrip() + '\n}\n')
    (srcdir / 'Core').mkdir(exist_ok=True)
    (srcdir / 'Natives').mkdir(exist_ok=True)
    (srcdir / 'Core' / 'Core.h').write_text(header)

    def body_text(entries):
        out = []
        cur = None
        for ns, lead, text in entries:
            if ns != cur:
                if cur is not None:
                    out.append('\n}\n')
                out.append('\nnamespace ' + '::'.join(['Core'] + ns) + '\n{')
                cur = ns
            depth = len(ns)
            block = lead + text
            if out and out[-1].endswith('{'):
                block = '\n' + block.lstrip('\n')
            if depth:
                block = re.sub(r'(?m)^\t{%d}' % depth, '', block)
            out.append(block)
        out.append('\n}\n')
        return ''.join(out)

    for path, entries in bodies.items():
        if path == 'main.cpp':
            continue
        text = '#include "Core/Core.h"\n' + body_text(entries)
        # natives that call the Core function of the same name named it with a
        # leading :: (the anonymous namespace's members were global then)
        text = re.sub(r'(?<![\w:])::(StartAutoMode|StopAutoMode|SplitCSV)\(', r'Core::\1(', text)
        (srcdir / path).write_text(text)
    main_body = body_text(bodies['main.cpp'])
    main = ('#include "Core/Core.h"\n\n'
            '// The plugin\'s entry points; everything else is in src/Core/ and\n'
            '// src/Natives/, with Core/Core.h declaring what they share.\n'
            + main_body + '\nusing namespace Core;\n' + post)
    (srcdir / 'main.cpp').write_text(main)


if __name__ == '__main__':
    if sys.argv[1] == '--assemble':
        assemble(sys.argv[2], sys.argv[3])
    else:
        main()
