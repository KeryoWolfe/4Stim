"""
Writes Data/Interface/Translations/4Stim_en.txt (docs/TRANSLATIONS.md): every
"$4Stim_..." text with its English, from

  - the plugin's TR("$4Stim_Key", "English") / TRF(...) calls (src/),
  - the movies' Strings.t("$4Stim_Key", "English") (Interface-src/),
  - the MCM page's labels (tools/mcm-texts.json, from make-mcm.py),
  - the HUD options files (Data/F4SE/Plugins/4Stim/Options/*.json: their
    "$4Stim_..." names and descriptions, with the English in "english").

As Fallout 4's translation files: UTF-16 LE with a byte order mark,
"$key<tab>text" lines. A translation is a copy named 4Stim_<language>.txt
(de, fr, es, it, pl, ptbr, ru, ja, cn...) with the texts translated.

  python tools/make-translations.py
"""
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Data', 'Interface', 'Translations', '4Stim_en.txt')

CALL = re.compile(r'\b(?:TR|TRF|Strings\.t)\(\s*"(\$4Stim_[A-Za-z0-9_]+)"\s*,\s*"((?:[^"\\]|\\.)*)"')


def unescape(text):
    return bytes(text, 'utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8')


def main():
    texts = {}
    problems = []

    def add(key, english, where):
        if key in texts and texts[key][0] != english:
            problems.append(f'{key}: "{texts[key][0]}" ({texts[key][1]}) vs "{english}" ({where})')
            return
        texts.setdefault(key, (english, where))

    for pattern in ('src/*.cpp', 'src/*.h', 'Interface-src/*.as', 'Interface-src/hud/*.as'):
        for path in sorted(glob.glob(os.path.join(ROOT, pattern))):
            if path.endswith('Translations.h'):
                continue  # its examples
            source = open(path, encoding='utf-8').read()
            for match in CALL.finditer(source):
                add(match.group(1), unescape(match.group(2)), os.path.relpath(path, ROOT))
    mcm = os.path.join(ROOT, 'tools', 'mcm-texts.json')
    if os.path.exists(mcm):
        for key, english in json.load(open(mcm, encoding='utf-8')).items():
            add(key, english, 'MCM')
    for path in sorted(glob.glob(os.path.join(ROOT, 'Data', 'F4SE', 'Plugins', '4Stim', 'Options', '*.json'))):
        data = json.load(open(path, encoding='utf-8'))
        for key, english in data.get('english', {}).items():
            add(key, english, os.path.basename(path))

    if problems:
        print('The same key with different English:', *problems, sep='\n  ')
        sys.exit(1)
    lines = [f'{key}\t{english}' for key, (english, _) in sorted(texts.items(), key=lambda kv: kv[0].lower())]
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'wb') as f:
        f.write(b'\xff\xfe')
        f.write(('\r\n'.join(lines) + '\r\n').encode('utf-16-le'))
    print(OUT, len(lines), 'text(s)')


if __name__ == '__main__':
    main()
