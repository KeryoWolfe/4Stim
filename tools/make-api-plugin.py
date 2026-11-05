"""
Writes 4Stim.esl (docs/API.md, "Factions, form lists and keywords"): a light
Fallout 4 plugin holding 4Stim's counterparts of the API forms in OStim NG's
OStim.esp, which dialogue, condition and voice-set add-ons read:

  - factions: in a scene (OStimExcitementFaction, OStimActorCountFaction...),
    doing an action now (OStimVaginalSexActorFaction...), on a furniture type
    (OStimBedFaction...), and the stats (OStat...: how often an actor has done
    something, climaxed doing it, done it with the player...);
  - form lists: who has been in a scene, who has done what with the player
    (OStat...List);
  - keywords: NoStim, OStimNoStrip.

Each keeps OStim's form ID (they're all in 0x800-0xFFF, a light plugin's
range), so a data file that names {"mod": "OStim.esp", "formid": ...} finds
the same form in 4Stim.esl, and the editor ID with "OStim" read as "4Stim"
(the project's naming rule; "OStat" and "NoStim" stay). 4Stim's plugin fills
them in during scenes (src/ApiForms.cpp). OStim's 137 globals are its MCM
settings, which 4Stim keeps in 4Stim.ini: left out.

  python tools/make-api-plugin.py <OStim.esp> [out.esl]

OStim.esp is in OStim NG's repository (data/OStim.esp, GPL-3.0).
"""
import struct, sys, zlib

PLUGIN = '4Stim.esl'
KEEP = ('FACT', 'FLST', 'KYWD')


def sub(tag, data):
    return tag.encode() + struct.pack('<H', len(data)) + data


def zstr(text):
    return text.encode('cp1252') + b'\0'


def record(tag, form_id, body, flags=0):
    return tag.encode() + struct.pack('<IIIIHH', len(body), flags, form_id, 0, 131, 0) + body


def group(label, records):
    data = b''.join(records)
    return b'GRUP' + struct.pack('<I', 24 + len(data)) + label.encode() + struct.pack('<iHHHH', 0, 0, 0, 131, 0) + data


def read_records(path):
    """(type, local form ID, editor ID) of OStim.esp's factions, lists and keywords."""
    data = open(path, 'rb').read()
    out = []

    def walk(off, end):
        while off < end:
            tag = data[off:off + 4].decode('latin1')
            size = struct.unpack_from('<I', data, off + 4)[0]
            if tag == 'GRUP':
                walk(off + 24, off + size)
                off += size
                continue
            flags, form_id = struct.unpack_from('<II', data, off + 8)
            body = data[off + 24:off + 24 + size]
            off += 24 + size
            if tag not in KEEP:
                continue
            if flags & 0x40000:  # compressed
                body = zlib.decompress(body[4:])
            edid, p = '', 0
            while p < len(body):
                field = body[p:p + 4].decode('latin1')
                length = struct.unpack_from('<H', body, p + 4)[0]
                if field == 'EDID':
                    edid = body[p + 6:p + 6 + length].rstrip(b'\0').decode('cp1252')
                    break
                p += 6 + length
            out.append((tag, form_id & 0xFFF, edid))

    header_size = struct.unpack_from('<I', data, 4)[0]
    walk(24 + header_size, len(data))
    return out


def main(source, out):
    found = read_records(source)
    if not found:
        sys.exit('no factions, lists or keywords in ' + source)
    master_count = 1  # Fallout4.esm, for the editor's sake (nothing refers to it)
    fid = lambda local: (master_count << 24) | local
    groups = {tag: [] for tag in KEEP}
    for tag, local, edid in sorted(found, key=lambda r: r[1]):
        if local < 0x800:
            sys.exit(f'{edid}: form ID {local:03X} is outside a light plugin\'s range')
        name = edid.replace('OStim', '4Stim')
        if tag == 'FACT':
            body = sub('EDID', zstr(name)) + sub('DATA', struct.pack('<I', 0))
        elif tag == 'FLST':
            body = sub('EDID', zstr(name))
        else:  # KYWD
            body = sub('EDID', zstr(name)) + sub('CNAM', bytes(4)) + sub('TNAM', struct.pack('<I', 0))
        groups[tag].append(record(tag, fid(local), body))
    count = len(found) + sum(1 for tag in KEEP if groups[tag])
    next_id = max(local for _, local, _ in found) + 1
    header = record('TES4', 0, b''.join([
        sub('HEDR', struct.pack('<fII', 1.0, count, min(next_id, 0xFFF))),
        sub('CNAM', zstr('4Stim')),
        sub('SNAM', zstr("4Stim's API forms (docs/API.md): OStim NG's factions, form lists and keywords for add-ons' conditions, filled in by 4Stim during scenes.")),
        sub('MAST', zstr('Fallout4.esm')),
        sub('DATA', bytes(8)),
    ]), flags=0x200)  # light plugin (ESL)
    with open(out, 'wb') as f:
        f.write(header + b''.join(group(tag, groups[tag]) for tag in ('KYWD', 'FACT', 'FLST') if groups[tag]))
    print(out, ', '.join(f'{len(groups[t])} {t}' for t in KEEP))


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else PLUGIN)
