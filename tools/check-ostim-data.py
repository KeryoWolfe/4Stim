"""
Checks OStim NG's own data, unchanged, against what 4Stim reads (the parity
audit's "run OStim content unmodified" test, docs/PARITY_AUDIT.md):

  - every field in OStim's data files (scenes, actions, action tags, actor
    properties, equip objects, events, facial expressions, furniture types,
    options, voice sets) that 4Stim's loader for that kind never names, and
    whether OStim's own code names it (a field neither reads is only data);
  - every id OStim's files refer to (scene action types, furniture types)
    that 4Stim doesn't have, and OStim ids 4Stim's own files don't define.

  python tools/check-ostim-data.py <OStim's SKSE\\Plugins\\OStim folder> [<OStim's skse\\src folder>]

A field named in the loader can still be read wrongly; this only finds
fields skipped outright. "info" fields (OStim's comments) and maps whose keys
are ids (25 or more distinct keys) are left out.
"""
import collections
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src')
DATA = os.path.join(ROOT, 'Data', 'F4SE', 'Plugins', '4Stim')
LOADERS = {
    'action tags': ['Actions.cpp'], 'actions': ['Actions.cpp'], 'actor properties': ['ActorProperties.cpp'],
    'equip objects': ['EquipObjects.cpp'], 'events': ['Events.cpp'], 'facial expressions': ['Expressions.cpp'],
    'furniture types': ['Furniture.cpp'], 'options': ['Options.cpp'], 'scenes': ['SceneRegistry.cpp', 'YamlJSON.cpp'],
    'voice sets': ['Sounds.cpp'],
}
SHARED = ['Sounds.cpp']  # Sounds::ReadForm: every {"mod", "formid"} field
BUILT = {'offsetxglobal', 'offsetyglobal', 'offsetzglobal'}  # names the loaders put together (Furniture.cpp)
STRING = re.compile(r'"([A-Za-z_][A-Za-z0-9_ ]*)"')


def strings(paths):
    out = set()
    for path in paths:
        out |= {s.lower() for s in STRING.findall(open(path, encoding='utf-8', errors='ignore').read())}
    return out


def load(path):
    return json.load(open(path, encoding='utf-8-sig'))


def walk(value, path, out, name):
    if isinstance(value, dict):
        for key, inner in value.items():
            out[(path, key)].add(name)
            walk(inner, f'{path}.{key}' if path else key, out, name)
    elif isinstance(value, list):
        for inner in value:
            walk(inner, path + '[]', out, name)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    ostim = sys.argv[1]
    ostim_src = strings(glob.glob(os.path.join(sys.argv[2], '**', '*.[ch]*'), recursive=True)) if len(sys.argv) > 2 else None
    problems = 0
    print('Fields 4Stim never reads')
    for kind, files in LOADERS.items():
        folder = os.path.join(ostim, kind)
        if not os.path.isdir(folder):
            continue
        known = strings([os.path.join(SRC, f) for f in files + SHARED if os.path.exists(os.path.join(SRC, f))]) | BUILT
        fields = collections.defaultdict(set)
        for path in glob.glob(os.path.join(folder, '**', '*.json'), recursive=True):
            try:
                walk(load(path), '', fields, os.path.basename(path))
            except Exception as e:
                print(f'  {kind}: {os.path.basename(path)} unreadable ({e})')
        children = collections.defaultdict(set)
        for parent, key in fields:
            children[parent].add(key)
        for (parent, key), names in sorted(fields.items()):
            if key == 'info' or len(children[parent]) >= 25 or key.lower() in known:
                continue
            read = '' if ostim_src is None else ('; OStim reads it' if key.lower() in ostim_src else '; OStim doesn\'t read it either')
            print(f'  {kind}: {parent + "." if parent else ""}{key} ({len(names)} file(s), e.g. {sorted(names)[0]}{read})')
            problems += ostim_src is None or key.lower() in ostim_src

    print('Ids OStim refers to that 4Stim lacks')
    actions = load(os.path.join(DATA, 'Actions', 'Default.json'))['actions']
    action_ids = {a['id'].lower() for a in actions} | {al.lower() for a in actions for al in a.get('aliases', [])}
    furniture = set()
    for path in glob.glob(os.path.join(DATA, 'Furniture', '*.json')):
        types = load(path).get('types', [])
        furniture |= {(t if isinstance(t, str) else t.get('id', '')).lower() for t in types}
    for path in glob.glob(os.path.join(ostim, 'actions', '*.json')):
        if os.path.splitext(os.path.basename(path))[0].lower() not in action_ids:
            print(f'  action {os.path.basename(path)} isn\'t in Actions\\Default.json')
            problems += 1
    for path in glob.glob(os.path.join(ostim, 'scenes', '**', '*.json'), recursive=True):
        scene = load(path)
        for action in scene.get('actions', []):
            if isinstance(action, dict) and str(action.get('type', '')).lower() not in action_ids:
                print(f'  scene {os.path.basename(path)}: action type "{action.get("type")}"')
                problems += 1
        if scene.get('furniture') and str(scene['furniture']).lower() not in furniture:
            print(f'  scene {os.path.basename(path)}: furniture type "{scene["furniture"]}"')
            problems += 1
    print(f'{problems} thing(s) to look at')


if __name__ == '__main__':
    main()
