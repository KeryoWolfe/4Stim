"""
Writes 4Stim's MCM page (docs/MCM.md) from 4Stim.ini:

  Data/MCM/Config/4Stim/config.json    the pages
  Data/MCM/Config/4Stim/settings.ini   MCM's defaults: 4Stim.ini's values
  Data/MCM/Config/4Stim/keybinds.json  the hotkeys
  tools/mcm-texts.json                 the English of every "$4Stim_MCM..."
                                       label (make-translations.py adds them
                                       to 4Stim_en.txt)

  python tools/make-mcm.py

Labels and ranges are below (SETTINGS); each setting's help text is its
comment in 4Stim.ini. MCM's setting names carry a type letter: 4Stim's own
names already do (bLockScenes); OStim's get one with "4S_"
(b4S_SetEndOnOrgasm), and a choice of words is a dropdown index (i4S_sTheme):
the plugin reads them back as 4Stim.ini's (src/MCM.cpp).
"""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INI = os.path.join(ROOT, 'Data', 'F4SE', 'Plugins', '4Stim.ini')
OUT = os.path.join(ROOT, 'Data', 'MCM', 'Config', '4Stim')
TEXTS = os.path.join(ROOT, 'tools', 'mcm-texts.json')

# Must match src/MCM.cpp's CHOICES.
CHOICES = {
    'sTargetMode': ['Crosshair', 'Proximity'],
    'sHUDControls': ['Tabs', 'Columns'],
    'sTheme': ['Color', 'PipBoy'],
    'sMaleLight': ['nolight', 'facelight', 'facelightdim', 'crotchlight', 'crotchlightdim', 'asslight', 'asslightdim'],
    'sFemaleLight': ['nolight', 'facelight', 'facelightdim', 'crotchlight', 'crotchlightdim', 'asslight', 'asslightdim'],
}
CHOICE_LABELS = {
    'sTargetMode': ['Crosshair', 'Nearest'],
    'sHUDControls': ['Tabs', 'Columns (OStim)'],
    'sTheme': ['Color', 'Pip-Boy'],
    'sMaleLight': ['None', 'Face', 'Face, dim', 'Crotch', 'Crotch, dim', 'Rear', 'Rear, dim'],
    'sFemaleLight': ['None', 'Face', 'Face, dim', 'Crotch', 'Crotch, dim', 'Rear', 'Rear, dim'],
}

# key: (label, kind, options). kind: bool; int / float (min, max, step);
# choice. Keys not listed (hotkeys, bone names, slot lists) aren't on the
# pages: hotkeys are MCM's own (HOTKEYS), the rest stay in 4Stim.ini.
SETTINGS = {
    # General
    'bResumeScenes': ('Resume scenes after loading', 'bool'),
    'sTargetMode': ('Find the partner by', 'choice'),
    'fMaxDistance': ('Crosshair distance', 'float', (50, 2000, 10)),
    'fCrosshairCone': ('Crosshair cone (degrees)', 'float', (1, 45, 1)),
    'fProximityRadius': ('Nearest partner within', 'float', (50, 2000, 10)),
    'SetUseFreeCam': ('Free camera in scenes', 'bool'),
    'SetCameraSpeed': ('Free camera speed', 'float', (0.5, 20, 0.5)),
    'SetFreeCamFOV': ('Free camera field of view', 'float', (20, 120, 1)),
    'SetForceFirstPerson': ('First person after scenes', 'bool'),
    'SetUseFades': ('Fade at the start and end', 'bool'),
    'SetUseAutoFades': ('Fade between auto mode scenes', 'bool'),
    'SetUseIntroScenes': ('Start with intro scenes', 'bool'),
    'NPCSceneDuration': ('NPC scenes last (ms, 0 = no limit)', 'int', (0, 1800000, 15000)),
    'fTransitionLead': ('Transition lead (seconds)', 'float', (-2, 5, 0.05)),
    'SetOnlyGayAnimsInGayScenes': ('Roles for the intended sex only', 'bool'),
    'unrestrictedNavigation': ('Unrestricted navigation', 'bool'),
    'fActorRadius': ('List NPCs within', 'float', (200, 5000, 50)),
    'SetFurnitureSearchDistance': ('Furniture search distance (x100 units)', 'int', (0, 50, 1)),
    'fFurnitureHeight': ('Furniture up / down within', 'float', (20, 500, 10)),
    'SetEnableFurniture': ('Use furniture', 'bool'),
    'SetSelectFurniture': ('Ask where (furniture)', 'bool'),
    'SetResetClutter': ('Put back clutter', 'bool'),
    'SetResetClutterRadius': ('Clutter within (x100 units)', 'int', (1, 20, 1)),
    'bLogFurniture': ('Log furniture (for authors)', 'bool'),
    'SetResetPosition': ('NPCs go back where they stood', 'bool'),
    'bLockScenes': ('Lock actors on the spot', 'bool'),
    'bGuardScenes': ('Scene guard', 'bool'),
    'bLogAnimEvents': ('Log animation events (for authors)', 'bool'),
    # Roles
    'PlayerAlwaysDomStraight': ('You always lead (man and woman)', 'bool'),
    'PlayerAlwaysSubStraight': ('You always follow (man and woman)', 'bool'),
    'PlayerAlwaysDomGay': ('You always lead (same sex)', 'bool'),
    'PlayerAlwaysSubGay': ('You always follow (same sex)', 'bool'),
    'playerSelectRoleStraight': ('Choose your role (man and woman)', 'bool'),
    'playerSelectRoleGay': ('Choose your role (same sex)', 'bool'),
    'playerSelectRoleThreesome': ('Choose your role (three or more)', 'bool'),
    'futaUseMaleRole': ('Futa take men\'s roles', 'bool'),
    'futaUseMaleExcitement': ('Futa excitement at the male rate', 'bool'),
    'futaUseMaleClimax': ('Futa climax as men', 'bool'),
    'unequipStrapOnIfNotNeeded': ('Strap-ons off when not needed', 'bool'),
    # Alignment
    'alignmentGroupBySex': ('Alignment by sex', 'bool'),
    'alignmentGroupByHeight': ('Alignment by height', 'bool'),
    'alignmentGroupByHeels': ('Alignment by heels', 'bool'),
    'SetSchlongBending': ('No penis bending', 'bool'),
    'fPenisBendStep': ('Penis bend step (degrees)', 'float', (-45, 45, 0.5)),
    'iPenisBendAxis': ('Penis bend axis (0 x, 1 y, 2 z)', 'int', (0, 2, 1)),
    'bDisableEyeLook': ('No eye directions', 'bool'),
    'fLookMaxSide': ('Eyes sideways (degrees at 100)', 'float', (0, 60, 1)),
    'fLookMaxUp': ('Eyes up / down (degrees at 100)', 'float', (0, 60, 1)),
    # Scaling
    'SetScaling': ('No actor scaling', 'bool'),
    # HUD
    'bEnableHUD': ('HUD in scenes', 'bool'),
    'sHUDControls': ('HUD controls', 'choice'),
    'sTheme': ('HUD theme', 'choice'),
    # Excitement
    'bEnableExcitement': ('Excitement', 'bool'),
    'SetsexExcitementMult': ('Male excitement rate', 'float', (0, 5, 0.05)),
    'SetFemaleSexExcitementMult': ('Female excitement rate', 'float', (0, 5, 0.05)),
    'excitementDecayRate': ('Excitement decay per second', 'float', (0, 5, 0.05)),
    'excitementDecayGracePeriod': ('Decay grace period (ms)', 'int', (0, 30000, 500)),
    'postOrgasmExcitement': ('Excitement after a climax', 'float', (0, 100, 1)),
    'postOrgasmExcitementMax': ('Highest excitement after a climax', 'float', (0, 100, 1)),
    'SetAutoClimaxAnims': ('Climax animations', 'bool'),
    'endOnPlayerOrgasm': ('End at your climax', 'bool'),
    'SetEndOnOrgasm': ('End at a man\'s climax', 'bool'),
    'SetEndOnSubOrgasm': ('End at a woman\'s climax', 'bool'),
    'SetEndOnBothOrgasm': ('End when everyone climaxed', 'bool'),
    'endNPCSceneOnOrgasm': ('NPC scenes end at a climax', 'bool'),
    'fClimaxEndDelay': ('End after a climax (seconds)', 'float', (0, 30, 0.5)),
    'SetSlowMoOrgasms': ('Slow motion at a climax', 'bool'),
    'fClimaxShake': ('Climax camera shake', 'float', (0, 2, 0.05)),
    'SetBlurOrgasms': ('Climax blur', 'bool'),
    'fClimaxBlur': ('Climax blur strength', 'float', (0, 1, 0.05)),
    'fClimaxFlash': ('Climax edge glow strength', 'float', (0, 1, 0.05)),
    'fClimaxGlowSize': ('Climax edge glow size', 'float', (0.01, 0.5, 0.01)),
    'SetUseRumble': ('Climax controller rumble', 'bool'),
    # AutoMode
    'SetAIControl': ('Auto mode in your scenes', 'bool'),
    'bAutoModeNPC': ('Auto mode in NPC scenes', 'bool'),
    'autoModeAnimDurationMin': ('Shortest scene (ms)', 'int', (1000, 60000, 500)),
    'autoModeAnimDurationMax': ('Longest scene (ms)', 'int', (1000, 120000, 500)),
    'autoModeForeplayChance': ('Foreplay chance (%)', 'int', (0, 100, 1)),
    'autoModeForeplayThresholdMin': ('Foreplay until excitement (least)', 'int', (0, 100, 1)),
    'autoModeForeplayThresholdMax': ('Foreplay until excitement (most)', 'int', (0, 100, 1)),
    'autoModePulloutChance': ('Pull-out chance (%)', 'int', (0, 100, 1)),
    'autoModePulloutThresholdMin': ('Pull out at excitement (least)', 'int', (0, 100, 1)),
    'autoModePulloutThresholdMax': ('Pull out at excitement (most)', 'int', (0, 100, 1)),
    'navigationDistanceMax': ('Navigation steps', 'int', (1, 50, 1)),
    'autoModeLimitToNavigationDistance': ('Only scenes within those steps', 'bool'),
    'bAutoModeStandingOnFloor': ('Standing off furniture, lying on beds', 'bool'),
    'SetActorSpeedControl': ('Auto speed', 'bool'),
    'autoSpeedControlIntervalMin': ('Speed change every (ms, least)', 'int', (500, 30000, 500)),
    'autoSpeedControlIntervalMax': ('Speed change every (ms, most)', 'int', (500, 30000, 500)),
    'autoSpeedControlExcitementMin': ('Auto speed from excitement', 'int', (0, 100, 1)),
    'autoSpeedControlExcitementMax': ('Auto speed up to excitement', 'int', (0, 100, 1)),
    # Undress
    'bUndress': ('Undressing', 'bool'),
    'SetAlwaysUndressAtStart': ('Undress at the start', 'bool'),
    'SetPartialUndressing': ('Undress what actions need', 'bool'),
    'SetUndressIfNeed': ('Undress fully when needed', 'bool'),
    'bUndressPlayer': ('Undress you', 'bool'),
    'bRedress': ('Dress again afterwards', 'bool'),
    'fUndressItemDelay': ('Between items (seconds)', 'float', (0, 3, 0.05)),
    'SetAnimateRedress': ('Redress animations', 'bool'),
    # Expressions
    'noFacialExpressions': ('No facial expressions', 'bool'),
    'SetExpressionDurationMin': ('Expression lasts (ms, least)', 'int', (250, 20000, 250)),
    'SetExpressionDurationMax': ('Expression lasts (ms, most)', 'int', (250, 20000, 250)),
    # Sound
    'bSounds': ('Sounds', 'bool'),
    'moanIntervalMin': ('Moan every (ms, least)', 'int', (250, 20000, 250)),
    'moanIntervalMax': ('Moan every (ms, most)', 'int', (250, 20000, 250)),
    'moanVolume': ('Moan volume', 'float', (0, 2, 0.05)),
    'soundVolume': ('Sound volume', 'float', (0, 2, 0.05)),
    'iSoundPriority': ('Sound priority', 'int', (0, 255, 1)),
    # Lights
    'sMaleLight': ('Light for men', 'choice'),
    'sFemaleLight': ('Light for women', 'choice'),
    'SetOnlyLightInDark': ('Only in the dark', 'bool'),
    'fLightLevelDark': ('Dark below light level', 'float', (0, 200, 1)),
}

# Pages: (title, [(ini section, [keys])]).
PAGES = [
    ('General', [('General', ['sTargetMode', 'fMaxDistance', 'fCrosshairCone', 'fProximityRadius', 'fActorRadius', 'SetUseIntroScenes',
                              'NPCSceneDuration', 'bResumeScenes', 'SetResetPosition', 'fTransitionLead', 'bLockScenes', 'bGuardScenes', 'bLogAnimEvents'])]),
    ('Camera', [('General', ['SetUseFreeCam', 'SetCameraSpeed', 'SetFreeCamFOV', 'SetForceFirstPerson', 'SetUseFades', 'SetUseAutoFades'])]),
    ('Furniture', [('General', ['SetEnableFurniture', 'SetSelectFurniture', 'SetFurnitureSearchDistance', 'fFurnitureHeight', 'SetResetClutter',
                                'SetResetClutterRadius', 'bLogFurniture'])]),
    ('Roles', [('General', ['SetOnlyGayAnimsInGayScenes', 'unrestrictedNavigation']),
               ('Roles', ['PlayerAlwaysDomStraight', 'PlayerAlwaysSubStraight', 'PlayerAlwaysDomGay', 'PlayerAlwaysSubGay', 'playerSelectRoleStraight',
                          'playerSelectRoleGay', 'playerSelectRoleThreesome', 'futaUseMaleRole', 'futaUseMaleExcitement', 'futaUseMaleClimax',
                          'unequipStrapOnIfNotNeeded'])]),
    ('Alignment', [('Alignment', ['alignmentGroupBySex', 'alignmentGroupByHeight', 'alignmentGroupByHeels', 'SetSchlongBending', 'fPenisBendStep',
                                  'iPenisBendAxis', 'bDisableEyeLook', 'fLookMaxSide', 'fLookMaxUp']),
                   ('Scaling', ['SetScaling'])]),
    ('HUD', [('HUD', ['bEnableHUD', 'sHUDControls', 'sTheme'])]),
    ('Excitement', [('Excitement', ['bEnableExcitement', 'SetsexExcitementMult', 'SetFemaleSexExcitementMult', 'excitementDecayRate',
                                    'excitementDecayGracePeriod', 'postOrgasmExcitement', 'postOrgasmExcitementMax', 'SetAutoClimaxAnims',
                                    'endOnPlayerOrgasm', 'SetEndOnOrgasm', 'SetEndOnSubOrgasm', 'SetEndOnBothOrgasm', 'endNPCSceneOnOrgasm',
                                    'fClimaxEndDelay'])]),
    ('Climax effects', [('Excitement', ['SetSlowMoOrgasms', 'fClimaxShake', 'SetBlurOrgasms', 'fClimaxBlur', 'fClimaxFlash', 'fClimaxGlowSize',
                                        'SetUseRumble'])]),
    ('Auto mode', [('AutoMode', ['SetAIControl', 'bAutoModeNPC', 'autoModeAnimDurationMin', 'autoModeAnimDurationMax', 'autoModeForeplayChance',
                                 'autoModeForeplayThresholdMin', 'autoModeForeplayThresholdMax', 'autoModePulloutChance', 'autoModePulloutThresholdMin',
                                 'autoModePulloutThresholdMax', 'navigationDistanceMax', 'autoModeLimitToNavigationDistance', 'bAutoModeStandingOnFloor',
                                 'SetActorSpeedControl', 'autoSpeedControlIntervalMin', 'autoSpeedControlIntervalMax', 'autoSpeedControlExcitementMin',
                                 'autoSpeedControlExcitementMax'])]),
    ('Undressing', [('Undress', ['bUndress', 'SetAlwaysUndressAtStart', 'SetPartialUndressing', 'SetUndressIfNeed', 'bUndressPlayer', 'bRedress',
                                 'fUndressItemDelay', 'SetAnimateRedress'])]),
    ('Faces and sound', [('Expressions', ['noFacialExpressions', 'SetExpressionDurationMin', 'SetExpressionDurationMax']),
                         ('Sound', ['bSounds', 'moanIntervalMin', 'moanIntervalMax', 'moanVolume', 'soundVolume', 'iSoundPriority'])]),
    ('Lights', [('Lights', ['sMaleLight', 'sFemaleLight', 'SetOnlyLightInDark', 'fLightLevelDark'])]),
]

# MCM's hotkeys: (id, label, FourStim.PressKey action, help).
HOTKEYS = [
    ('Hotkey', 'Start a scene / the HUD', 'hotkey', 'Starts a scene with whoever you look at, or in a scene focuses the HUD (4Stim.ini: SetKeyMap).'),
    ('NPCScene', 'Start a scene without you', 'npcscene', 'The picker for a scene without you (4Stim.ini: keyNpcSceneStart, or Shift + the hotkey).'),
    ('SpeedUp', 'Faster', 'speedup', 'The scene you\'re in one speed faster (4Stim.ini: SetKeyUp).'),
    ('SpeedDown', 'Slower', 'speeddown', 'The scene you\'re in one speed slower (4Stim.ini: SetKeyDown).'),
    ('AutoMode', 'Auto mode on / off', 'automode', 'Auto mode for the scene you\'re in or watching (4Stim.ini: SetControlToggle).'),
    ('PullOut', 'Pull out', 'pullout', 'OStim\'s pull-out key (4Stim.ini: SetPullOut).'),
    ('End', 'End the scene', 'end', 'Ends the scene you\'re in or watching (4Stim.ini: SetOsaEndKey).'),
    ('Search', 'Search scenes', 'search', 'Every scene for these actors (4Stim.ini: keySearch).'),
    ('Align', 'Align tab', 'align', 'Opens the HUD\'s Align tab (4Stim.ini: keyAlignment).'),
    ('FreeCam', 'Free camera on / off', 'freecam', 'In and out of the free camera in a scene (4Stim.ini: SetFreeCamToggleKey).'),
    ('HideUI', 'Hide the HUD', 'hideui', 'Hides or shows the HUD (4Stim.ini: keyHideUI).'),
]


def read_ini():
    """Each key: (section, value, comment above it)."""
    out = {}
    section, comment = '', []
    for raw in open(INI, encoding='utf-8'):
        line = raw.strip()
        if not line:
            comment = []
            continue
        if line.startswith('['):
            section, comment = line[1:-1], []
        elif line.startswith(';'):
            comment.append(line[1:].strip())
        elif '=' in line:
            key, value = [p.strip() for p in line.split('=', 1)]
            out[key] = (section, value, ' '.join(comment))
    return out


def mcm_key(key, kind):
    if kind == 'choice':
        return 'i4S_' + key
    if re.match(r'^[bifs][A-Z]', key):
        return key
    return {'bool': 'b', 'int': 'i', 'float': 'f'}[kind] + '4S_' + key


def mcm_value(key, kind, value):
    if kind == 'choice':
        options = [o.lower() for o in CHOICES[key]]
        return str(options.index(value.lower())) if value.lower() in options else '0'
    if kind == 'bool':
        return '0' if value.strip() in ('0', 'false') else '1'
    return value


def main():
    ini = read_ini()
    texts = {}

    def text(key, english):
        texts[key] = english
        return key

    defaults = {}
    pages = []
    for title, groups in PAGES:
        slug = re.sub(r'[^A-Za-z]', '', title)
        content = []
        for section, keys in groups:
            for key in keys:
                if key not in ini:
                    raise SystemExit(f'{key} is not in 4Stim.ini')
                if key not in SETTINGS:
                    raise SystemExit(f'{key} has no entry in SETTINGS')
                ini_section, value, comment = ini[key]
                if ini_section != section:
                    raise SystemExit(f'{key} is in [{ini_section}], not [{section}]')
                spec = SETTINGS[key]
                label, kind = spec[0], spec[1]
                name = mcm_key(key, kind)
                control = {
                    'id': f'{name}:{section}',
                    'text': text(f'$4Stim_MCM_{key}', label),
                    'help': text(f'$4Stim_MCM_{key}_Help', comment or label),
                    'action': {'type': 'CallGlobalFunction', 'script': 'FourStimMCM', 'function': 'SettingChanged', 'params': []},
                }
                if kind == 'bool':
                    control['type'] = 'switcher'
                    control['valueOptions'] = {'sourceType': 'ModSettingBool'}
                elif kind == 'choice':
                    control['type'] = 'dropdown'
                    control['valueOptions'] = {
                        'sourceType': 'ModSettingInt',
                        'options': [text(f'$4Stim_MCM_{key}_{i}', o) for i, o in enumerate(CHOICE_LABELS[key])],
                    }
                else:
                    low, high, step = spec[2]
                    control['type'] = 'slider'
                    control['valueOptions'] = {'min': low, 'max': high, 'step': step,
                                               'sourceType': 'ModSettingInt' if kind == 'int' else 'ModSettingFloat'}
                content.append(control)
                defaults.setdefault(section, []).append((name, mcm_value(key, kind, value)))
        pages.append({'pageDisplayName': text(f'$4Stim_MCM_Page{slug}', title), 'content': content})

    # Hotkeys: the MCM's own (keybinds.json); 4Stim.ini's keys work as well.
    hot = [{'text': text('$4Stim_MCM_HotkeysNote', 'These are the MCM\'s own hotkeys. The keys in 4Stim.ini work too: bind each action in one place only.'), 'type': 'text'},
           {'type': 'spacer'}]
    for hid, label, action, help_text in HOTKEYS:
        hot.append({'id': hid, 'text': text(f'$4Stim_MCM_Key{hid}', label), 'type': 'hotkey', 'help': text(f'$4Stim_MCM_Key{hid}_Help', help_text),
                    'valueOptions': {'allowModifierKeys': True}})
    pages.append({'pageDisplayName': text('$4Stim_MCM_PageHotkeys', 'Hotkeys'), 'content': hot})

    # Export / import / 4Stim.ini.
    pages.append({'pageDisplayName': text('$4Stim_MCM_PageSettings', 'Save and load'), 'content': [
        {'text': text('$4Stim_MCM_SettingsNote', 'Settings changed here are kept in Data\\MCM\\Settings\\4Stim.ini, over 4Stim.ini. Whichever of the two you changed last wins when the game starts.'), 'type': 'text'},
        {'text': text('$4Stim_MCM_IniOnly', 'Only in 4Stim.ini: the undress slots (sUndressSlots), bone names (sPenisBendNode, sLeftEyeNode, sRightEyeNode) and sound flags (iSoundFlags).'), 'type': 'text'},
        {'type': 'spacer'},
        {'text': text('$4Stim_MCM_Export', 'Export settings'), 'type': 'button',
         'help': text('$4Stim_MCM_Export_Help', 'The settings in effect to Documents\\My Games\\Fallout4\\4Stim\\mcmsettings.json, as OStim exports its own.'),
         'action': {'type': 'CallGlobalFunction', 'script': 'FourStimMCM', 'function': 'ExportSettings', 'params': []}},
        {'text': text('$4Stim_MCM_Import', 'Import settings'), 'type': 'button',
         'help': text('$4Stim_MCM_Import_Help', 'The settings from that file.'),
         'action': {'type': 'CallGlobalFunction', 'script': 'FourStimMCM', 'function': 'ImportSettings', 'params': []}},
        {'text': text('$4Stim_MCM_UseIni', 'Use 4Stim.ini\'s settings'), 'type': 'button',
         'help': text('$4Stim_MCM_UseIni_Help', 'Forgets what was changed here: 4Stim.ini\'s values again.'),
         'action': {'type': 'CallGlobalFunction', 'script': 'FourStimMCM', 'function': 'UseIniSettings', 'params': []}},
    ]})

    config = {
        'modName': '4Stim',
        'displayName': '4Stim',
        'minMcmVersion': 1,
        'content': [
            {'type': 'spacer'},
            {'text': '4Stim', 'type': 'text', 'align': 'center'},
            {'text': text('$4Stim_MCM_Intro', 'OStim NG for Fallout 4. Every setting is also in Data\\F4SE\\Plugins\\4Stim.ini.'), 'type': 'text', 'align': 'center'},
        ],
        'pages': pages,
    }
    keybinds = {'modName': '4Stim', 'keybinds': [
        {'id': hid, 'desc': label, 'action': {'type': 'CallGlobalFunction', 'script': 'FourStimMCM', 'function': 'PressKey', 'params': [action]}}
        for hid, label, action, _ in HOTKEYS]}

    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, 'config.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(config, f, indent='\t', ensure_ascii=False)
        f.write('\n')
    with open(os.path.join(OUT, 'keybinds.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(keybinds, f, indent='\t', ensure_ascii=False)
        f.write('\n')
    with open(os.path.join(OUT, 'settings.ini'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('; 4Stim\'s MCM defaults: 4Stim.ini\'s shipped values (tools/make-mcm.py).\n')
        for section, entries in defaults.items():
            f.write(f'\n[{section}]\n')
            for name, value in entries:
                f.write(f'{name}={value}\n')
    with open(TEXTS, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(texts, f, indent='\t', ensure_ascii=False)
        f.write('\n')
    print(len(pages), 'page(s),', sum(len(e) for e in defaults.values()), 'setting(s),', len(HOTKEYS), 'hotkey(s),', len(texts), 'text(s)')


if __name__ == '__main__':
    main()
