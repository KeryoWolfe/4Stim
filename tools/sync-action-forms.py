"""
Copies OStim NG's API form fields (docs/API.md, "Factions, form lists and
keywords") into 4Stim's action data, pointed at 4Stim.esl:

  - each action side's "faction", "statFaction", "playerStatFaction",
    "climaxStatFaction", "partnerClimaxStatFaction", "playerClimaxStatFaction",
    "playerPartnerClimaxStatFaction", "playerStatList", "playerClimaxStatList"
    and "playerPartnerClimaxStatList", from OStim's actions\\<id>.json into
    Data\\F4SE\\Plugins\\4Stim\\Actions\\Default.json (actions OStim doesn't
    have are left alone);
  - OStim's action tags (action tags\\<tag>.json) into
    Data\\F4SE\\Plugins\\4Stim\\Action Tags\\.

"mod": "OStim.esp" becomes "4Stim.esl" (same form IDs, tools/make-api-plugin.py)
and the "info" editor IDs read "4Stim" for "OStim".

  python tools/sync-action-forms.py <OStim NG's data\\SKSE\\Plugins\\OStim folder>
"""
import glob, json, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, 'Data', 'F4SE', 'Plugins', '4Stim')
FIELDS = ['faction', 'statFaction', 'playerStatFaction', 'climaxStatFaction', 'partnerClimaxStatFaction',
          'playerClimaxStatFaction', 'playerPartnerClimaxStatFaction', 'playerStatList', 'playerClimaxStatList',
          'playerPartnerClimaxStatList']
SIDES = ['actor', 'target', 'performer']


def repoint(value):
    if isinstance(value, list):
        return [repoint(v) for v in value]
    if isinstance(value, dict):
        out = dict(value)
        if str(out.get('mod', '')).lower() == 'ostim.esp':
            out['mod'] = '4Stim.esl'
        if 'info' in out:
            out['info'] = str(out['info']).replace('OStim', '4Stim')
        return out
    return value


def forms_of(side):
    return {f: repoint(side[f]) for f in FIELDS if f in side}


def main(ostim):
    actions = {os.path.splitext(os.path.basename(p))[0].lower(): json.load(open(p, encoding='utf-8-sig'))
               for p in glob.glob(os.path.join(ostim, 'actions', '*.json'))}
    path = os.path.join(DATA, 'Actions', 'Default.json')
    data = json.load(open(path, encoding='utf-8'))
    touched = 0
    for action in data['actions']:
        source = actions.get(action['id'])
        if not source:
            continue
        for side in SIDES:
            forms = forms_of(source.get(side, {}))
            if not forms:
                continue
            target = action.setdefault(side, {})
            for f in FIELDS:
                target.pop(f, None)
            target.update(forms)
            touched += 1
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(json.dumps(data, indent='\t', ensure_ascii=False) + '\n')
    print(path, touched, 'side(s)')

    out_dir = os.path.join(DATA, 'Action Tags')
    os.makedirs(out_dir, exist_ok=True)
    count = 0
    for p in sorted(glob.glob(os.path.join(ostim, 'action tags', '*.json'))):
        tag = json.load(open(p, encoding='utf-8-sig'))
        out = {side: forms_of(tag[side]) for side in SIDES if isinstance(tag.get(side), dict) and forms_of(tag[side])}
        with open(os.path.join(out_dir, os.path.basename(p).lower()), 'w', encoding='utf-8', newline='\n') as f:
            f.write(json.dumps(out, indent='\t', ensure_ascii=False) + '\n')
        count += 1
    print(out_dir, count, 'tag(s)')


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1])
