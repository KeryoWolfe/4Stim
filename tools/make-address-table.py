"""
Makes 4Stim's address table for an older Fallout 4 version (docs/OLDGEN.md):
Data/F4SE/Plugins/4Stim/Addresses/version-<game version>.txt, one
"<AE Address Library ID> <RVA>" per line, for every ID CommonLibF4 names
(lib/commonlibf4/include/RE/IDs*.h) and every raw REL::ID in its sources.

Sources, later ones winning:
  --database f4rd-runtime.bin   CommonLibF4RD's Runtime Database: the RVAs it
                                knows outright for that version
  --mapping  <plugin>.mapping   a CommonLibF4RD plugin's complete mapping,
                                written on that game version (it adds the
                                addresses the database finds by pattern)

  python tools/make-address-table.py --version 1.10.163.0 --database <path> [--mapping <path>]

Prints how many IDs it has an RVA for, and which of 4Stim's own
(src/compat/RequiredIDs.h) it lacks.
"""
import argparse, mmap, os, re, struct, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAGIC = b'F4RDBIN\0'


def library_ids():
    ids = set()
    inc = os.path.join(REPO, 'lib', 'commonlibf4', 'include', 'RE')
    for name in ('IDs.h', 'IDs_VTABLE.h', 'IDs_RTTI.h', 'IDs_NiRTTI.h'):
        text = open(os.path.join(inc, name), encoding='utf-8', errors='ignore').read()
        for body in re.findall(r'inline constexpr [^\n]*?\{([^}]*)\}', text):
            ids.update(int(x) for x in re.findall(r'\d+', body))
    for root in (os.path.join(REPO, 'lib', 'commonlibf4', 'include'), os.path.join(REPO, 'lib', 'commonlibf4', 'src')):
        for d, _, files in os.walk(root):
            for f in files:
                if f.endswith(('.h', '.cpp')) and not f.startswith('IDs'):
                    text = open(os.path.join(d, f), encoding='utf-8', errors='ignore').read()
                    ids.update(int(x) for x in re.findall(r'REL::ID\s*[\({]\s*(\d+)', text))
    return ids


def required_ids():
    text = open(os.path.join(REPO, 'src', 'compat', 'RequiredIDs.h'), encoding='utf-8').read()
    return [(int(i), n) for i, n in re.findall(r'\{\s*(\d+),\s*"([^"]+)"\s*\}', text)]


def from_database(path, version, wanted):
    f = open(path, 'rb')
    m = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
    off = 0
    if m[:8] != MAGIC:
        count = struct.unpack_from('<Q', m, 0)[0]
        off = 8 + count * 16
        if m[off:off + 8] != MAGIC:
            sys.exit('not a CommonLibF4RD runtime database: ' + path)
    b = memoryview(m)[off:]
    rc, kc = struct.unpack_from('<I', b, 24)[0], struct.unpack_from('<I', b, 32)[0]
    ro, ko = struct.unpack_from('<Q', b, 48)[0], struct.unpack_from('<Q', b, 64)[0]
    key = tuple(int(x) for x in version.split('.'))
    out = {}

    def index(i):
        lo, hi = 0, rc
        while lo < hi:
            mid = (lo + hi) // 2
            if struct.unpack_from('<Q', b, ro + 40 * mid)[0] < i:
                lo = mid + 1
            else:
                hi = mid
        return lo if lo < rc and struct.unpack_from('<Q', b, ro + 40 * lo)[0] == i else None

    for i in wanted:
        r = index(i)
        if r is None:
            continue
        _, _, _, fk, nk = struct.unpack_from('<QIIII', b, ro + 40 * r)
        for j in range(nk):
            e = ko + 16 * (fk + j)
            if struct.unpack_from('<4H', b, e) == key:
                out[i] = struct.unpack_from('<I', b, e + 8)[0]
    return out


def from_mapping(path, wanted):
    out = {}
    for line in open(path, encoding='utf-8', errors='ignore'):
        parts = line.split()
        if len(parts) >= 2 and parts[0].isdigit():
            i = int(parts[0])
            if i in wanted:
                out[i] = int(parts[1], 0)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--version', required=True, help='game version, e.g. 1.10.163.0')
    ap.add_argument('--database')
    ap.add_argument('--mapping', action='append', default=[])
    ap.add_argument('--out')
    a = ap.parse_args()
    wanted = library_ids()
    table = {}
    if a.database:
        table.update(from_database(a.database, a.version, wanted))
    for path in a.mapping:
        table.update(from_mapping(path, wanted))
    out = a.out or os.path.join(REPO, 'Data', 'F4SE', 'Plugins', '4Stim', 'Addresses',
                                'version-' + a.version.replace('.', '-') + '.txt')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w', newline='\n') as f:
        f.write('# 4Stim address table for Fallout 4 %s (docs/OLDGEN.md): AE Address Library ID -> RVA.\n' % a.version)
        f.write('# Made by tools/make-address-table.py from the CommonLibF4RD Runtime Database%s.\n'
                % (' and a CommonLibF4RD mapping' if a.mapping else ''))
        for i in sorted(table):
            f.write('%d 0x%X\n' % (i, table[i]))
    print('%s: %d of %d IDs' % (out, len(table), len(wanted)))
    missing = [(i, n) for i, n in required_ids() if i not in table]
    print('4Stim needs %d; missing %d' % (len(required_ids()), len(missing)))
    for i, n in missing:
        print('  missing %s (%d)' % (n, i))


if __name__ == '__main__':
    main()
