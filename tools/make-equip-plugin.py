"""
Writes 4Stim's optional equip object plugin (docs/ACTORS.md): an ESL-flagged
Fallout 4 plugin with one armor (ARMO + its ARMA) per equip object, for
women (the female model only), on a biped slot clothes don't use.

  python tools/make-equip-plugin.py <out.esp>

The armors are listed below; their form IDs stay fixed (the equip object
files in Data\\F4SE\\Plugins\\4Stim\\Equip Objects\\ point at them).
"""
import struct, sys

PLUGIN = '4Stim - Equip Objects.esp'
HUMAN_RACE = 0x00013746
# Biped slots (30-61): free on vanilla clothes, outside 4Stim's undress slots.
STRAPON_SLOT = 57
TONGUE_SLOT = 58

# (armor form ID, addon form ID, editor ID, name, female model, male model
# (None: women only), slot); models under Meshes\.
ARMORS = [
    (0x801, 0x802, '4Stim_OStimStrapOn', 'Strap-on (OStim, Calyps)', r'4Stim\EquipObjects\OStimStrapOn.nif', None, STRAPON_SLOT),
]
# OStim's tongues (Egilvar's): each kind and its "cum" variant, women's and
# men's models.
TONGUES = [
    ('NormalTongue', 'Tongue'), ('NormalTonguePiercing', 'Tongue, pierced'),
    ('LongTongue', 'Long tongue'), ('LongTonguePiercing', 'Long tongue, pierced'), ('LongTongueDoublePiercing', 'Long tongue, double-pierced'),
    ('SnakeTongue', 'Snake tongue'), ('SnakeTonguePiercing', 'Snake tongue, pierced'), ('SnakeTongueDoublePiercing', 'Snake tongue, double-pierced'),
]
for n, (mesh, name) in enumerate(TONGUES):
    base = 0x810 + n * 4
    path = '4Stim\\EquipObjects\\tongues\\'
    ARMORS.append((base, base + 1, '4Stim_' + mesh, name, path + mesh + '.nif', path + mesh + '_m.nif', TONGUE_SLOT))
    ARMORS.append((base + 2, base + 3, '4Stim_' + mesh + 'Cum', name + ' (cum)', path + mesh + 'Cum.nif', path + mesh + 'Cum_m.nif', TONGUE_SLOT))


def sub(tag, data):
    return tag.encode() + struct.pack('<H', len(data)) + data


def zstr(text):
    return text.encode('cp1252') + b'\0'


def record(tag, form_id, body, flags=0):
    return tag.encode() + struct.pack('<IIIIHH', len(body), flags, form_id, 0, 131, 0) + body


def group(label, records):
    data = b''.join(records)
    return b'GRUP' + struct.pack('<I', 24 + len(data)) + label.encode() + struct.pack('<iHHHH', 0, 0, 0, 131, 0) + data


def main(out):
    master_count = 1
    fid = lambda local: (master_count << 24) | local
    arma, armo = [], []
    for armor, addon, edid, name, model, male, slot in ARMORS:
        bod2 = struct.pack('<I', 1 << (slot - 30))
        arma.append(record('ARMA', fid(addon), b''.join([
            sub('EDID', zstr(edid + '_AA')),
            sub('BOD2', bod2),
            sub('RNAM', struct.pack('<I', HUMAN_RACE)),
            sub('DNAM', bytes(12)),
        ] + ([sub('MOD2', zstr(male))] if male else []) + [
            sub('MOD3', zstr(model)),
        ])))
        armo.append(record('ARMO', fid(armor), b''.join([
            sub('EDID', zstr(edid)),
            sub('OBND', bytes(12)),
            sub('FULL', zstr(name)),
            sub('BOD2', bod2),
            sub('RNAM', struct.pack('<I', HUMAN_RACE)),
            sub('DESC', b'\0'),
            sub('INDX', struct.pack('<H', 0)),
            sub('MODL', struct.pack('<I', fid(addon))),
            sub('DATA', struct.pack('<IfI', 0, 0.5, 0)),
            sub('FNAM', bytes(8)),
        ])))
    count = len(arma) + len(armo) + 2  # records and groups
    next_id = max(max(a, b) for a, b, *_ in ARMORS) + 1
    header = record('TES4', 0, b''.join([
        sub('HEDR', struct.pack('<fII', 1.0, count, next_id)),
        sub('CNAM', zstr('4Stim')),
        sub('SNAM', zstr("4Stim's equip objects (docs/ACTORS.md): OStim's strap-on (Calyps, refit to TWB) and tongues (Egilvar).")),
        sub('MAST', zstr('Fallout4.esm')),
        sub('DATA', bytes(8)),
    ]), flags=0x200)  # light plugin (ESL)
    with open(out, 'wb') as f:
        f.write(header + group('ARMO', armo) + group('ARMA', arma))
    print(out, len(ARMORS), 'armor(s)')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else PLUGIN)
