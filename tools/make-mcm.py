"""
Writes 4Stim's MCM page (docs/MCM.md) from 4Stim.ini:

  Data/MCM/Config/4Stim/config.json    the pages
  Data/MCM/Config/4Stim/settings.ini   MCM's defaults: 4Stim.ini's values
  Data/MCM/Config/4Stim/keybinds.json  empty (the Controls page sets 4Stim.ini's keys)
  tools/mcm-texts.json                 the English of every MCM text, by key
                                       (make-translations.py adds them to
                                       4Stim_en.txt)

  python tools/make-mcm.py

The pages, headers, labels and help texts are OStim's (PAGES, OSTIM_KEYS:
OStim's keys as "$4stim_...", English from tools/ostim-mcm-english.json);
4Stim's own settings have their label below (SETTINGS) and their comment in
4Stim.ini as help. Kinds and ranges are in SETTINGS. MCM's setting names carry a type letter: 4Stim's own
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
    'sPlayerLight': ['default', 'nolight', 'facelight', 'facelightdim', 'crotchlight', 'crotchlightdim', 'asslight', 'asslightdim'],
}
PAD_KEYS = ['sPadHotkey', 'sPadNPCScene', 'sPadSpeedUp', 'sPadSpeedDown', 'sPadAutoMode', 'sPadPullOut', 'sPadEnd', 'sPadSearch', 'sPadAlign', 'sPadFreeCam', 'sPadHideUI']
PAD_BUTTONS = ['None', 'LB', 'RB', 'LT', 'RT', 'X', 'Y', 'LS', 'RS', 'Back']
PAD_LABELS = ['None', 'LB', 'RB', 'LT', 'RT', 'X', 'Y', 'Left stick (press)', 'Right stick (press)', 'Back / View']
for _k in PAD_KEYS:
    CHOICES[_k] = PAD_BUTTONS
CHOICES['sPadScheme'] = ['Modern', 'Classic']
CHOICES['sToySynchronization'] = ['player', 'male', 'female', 'scene']
CHOICES['sToyScaling'] = ['none', 'rate', 'value', 'speed']
CHOICE_LABELS = {
    'sTargetMode': ['Crosshair', 'Nearest'],
    'sHUDControls': ['Tabs', 'Columns (OStim)'],
    'sTheme': ['Color', 'Pip-Boy'],
    'sMaleLight': ['None', 'Face', 'Face, dim', 'Crotch', 'Crotch, dim', 'Rear', 'Rear, dim'],
    'sFemaleLight': ['None', 'Face', 'Face, dim', 'Crotch', 'Crotch, dim', 'Rear', 'Rear, dim'],
    'sPlayerLight': ['Default', 'None', 'Face', 'Face, dim', 'Crotch', 'Crotch, dim', 'Rear', 'Rear, dim'],
}
for _k in PAD_KEYS:
    CHOICE_LABELS[_k] = PAD_LABELS
CHOICE_LABELS['sPadScheme'] = ['4Stim Modern', 'OStim Classic']
CHOICE_LABELS['sToySynchronization'] = ['The player', 'The first man', 'The first woman', 'Anyone']
CHOICE_LABELS['sToyScaling'] = ['None (strongest)', 'Excitement rate', 'Excitement', 'Scene speed']

# key: (label, kind, options). kind: bool; int / float (min, max, step);
# choice; key (an MCM key input: "keycode,modifiers", a string); ms (OStim's
# milliseconds in 4Stim.ini, a slider in seconds: src/MCM.cpp's MILLISECONDS). Keys not listed
# (bone names, slot lists) aren't on the pages and stay in 4Stim.ini.
SETTINGS = {
    # Hotkeys (keyboard, as OStim's): MCM key inputs over 4Stim.ini's keys.
    'SetKeyMap': ('Start a scene / the HUD', 'key'),
    'keyNpcSceneStart': ('Start a scene without you', 'key'),
    'SetKeyUp': ('Faster', 'key'),
    'SetKeyDown': ('Slower', 'key'),
    'SetPullOut': ('Pull out', 'key'),
    'SetControlToggle': ('Auto mode on / off', 'key'),
    'SetFreeCamToggleKey': ('Free camera on / off', 'key'),
    'keySearch': ('Search scenes', 'key'),
    'keyAlignment': ('Align tab', 'key'),
    'keyHideUI': ('Hide the HUD', 'key'),
    'SetOsaUpKey': ('Up', 'key'),
    'SetOsaDownKey': ('Down', 'key'),
    'SetOsaLeftKey': ('Left', 'key'),
    'SetOsaRightKey': ('Right', 'key'),
    'SetOsaTogKey': ('Toggle (list / tabs)', 'key'),
    'SetOsaYesKey': ('Yes (choose)', 'key'),
    'SetOsaEndKey': ('End the scene', 'key'),
    # Controller
    'sPadScheme': ('Controller scheme', 'choice'),
    'sPadHotkey': ('Start a scene / the HUD', 'choice'),
    'sPadNPCScene': ('Start a scene without you', 'choice'),
    'sPadSpeedUp': ('Faster', 'choice'),
    'sPadSpeedDown': ('Slower', 'choice'),
    'sPadAutoMode': ('Auto mode on / off', 'choice'),
    'sPadPullOut': ('Pull out', 'choice'),
    'sPadEnd': ('End the scene', 'choice'),
    'sPadSearch': ('Search scenes', 'choice'),
    'sPadAlign': ('Align tab', 'choice'),
    'sPadFreeCam': ('Free camera on / off', 'choice'),
    'sPadHideUI': ('Hide the HUD', 'choice'),
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
    'addActorsAtStart': ('Add actors at start', 'bool'),
    'NPCSceneDuration': ('NPC scenes last (seconds, 0 = no limit)', 'ms', (0, 1800, 15)),
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
    'bEndOnDamage': ('End scenes when someone is hurt', 'bool'),
    'bStayWhenInterrupted': ('Stay put when a scene is broken off', 'bool'),
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
    'bErections': ('Erections', 'bool'),
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
    'fHUDFadeTime': ('Fade out after (seconds, 0 = never)', 'float', (0, 60, 1)),
    'enablePlayerBar': ('Excitement bar for you', 'bool'),
    'enableNpcBar': ('Excitement bars for NPCs', 'bool'),
    'SetAutoHideBar': ('Bars fade with the HUD', 'bool'),
    'SetMatchColorToGender': ('Bar colors by sex (else by role)', 'bool'),
    # Excitement
    'bEnableExcitement': ('Excitement', 'bool'),
    'SetsexExcitementMult': ('Male excitement rate', 'float', (0, 5, 0.05)),
    'SetFemaleSexExcitementMult': ('Female excitement rate', 'float', (0, 5, 0.05)),
    'excitementDecayRate': ('Excitement decay per second', 'float', (0, 5, 0.05)),
    'excitementDecayGracePeriod': ('Decay grace period (seconds)', 'ms', (0, 30, 0.5)),
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
    'SetForceAIForMasturbation': ('Auto mode in your solo scenes', 'bool'),
    'SetForceAIInConsensualScenes': ('Use auto mode in scenes with partners', 'bool'),
    'autoModeAnimDurationMin': ('Shortest scene (seconds)', 'ms', (1, 60, 0.5)),
    'autoModeAnimDurationMax': ('Longest scene (seconds)', 'ms', (1, 120, 0.5)),
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
    'autoSpeedControlIntervalMin': ('Speed change every (seconds, least)', 'ms', (0.5, 30, 0.5)),
    'autoSpeedControlIntervalMax': ('Speed change every (seconds, most)', 'ms', (0.5, 30, 0.5)),
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
    'SetExpressionDurationMin': ('Expression lasts (seconds, least)', 'ms', (0.25, 20, 0.25)),
    'SetExpressionDurationMax': ('Expression lasts (seconds, most)', 'ms', (0.25, 20, 0.25)),
    # Sound
    'bSounds': ('Sounds', 'bool'),
    'moanIntervalMin': ('Moan every (seconds, least)', 'ms', (0.25, 20, 0.25)),
    'moanIntervalMax': ('Moan every (seconds, most)', 'ms', (0.25, 20, 0.25)),
    'moanVolume': ('Moan volume', 'float', (0, 2, 0.05)),
    'soundVolume': ('Sound volume', 'float', (0, 2, 0.05)),
    'iSoundPriority': ('Sound priority', 'int', (0, 255, 1)),
    # Lights
    'sMaleLight': ('Light for men', 'choice'),
    'sFemaleLight': ('Light for women', 'choice'),
    'sPlayerLight': ('Your light', 'choice'),
    'SetOnlyLightInDark': ('Only in the dark', 'bool'),
    'fLightLevelDark': ('Dark below light level', 'float', (0, 200, 1)),
    # General
    'bUsePlaybackSpeed': ("Scenes' playback speeds", 'bool'),
    # Sex toys
    'bToysEnabled': ('Sex toys', 'bool'),
    'bIntiface': ('Use Intiface Central', 'bool'),
    'bIntifaceScan': ('Look for new devices on connecting', 'bool'),
    'sToySynchronization': ('Toys follow', 'choice'),
    'sToyScaling': ('Strength follows', 'choice'),
    'fToyMinScale': ('Weakest at or below', 'float', (0, 10, 0.05)),
    'fToyMaxScale': ('Strongest at or above', 'float', (0, 10, 0.05)),
    'fToyMinMagnitude': ('Weakest strength', 'float', (0, 1, 0.05)),
    'fToyMaxMagnitude': ('Strongest strength', 'float', (0, 1, 0.05)),
    'bToyPeaks': ('Pulse on peaks', 'bool'),
    'fToyMinPeak': ('Weakest pulse', 'float', (0, 1, 0.05)),
    'fToyMaxPeak': ('Strongest pulse', 'float', (0, 1, 0.05)),
    'bToyClimax': ('Pulse on climax', 'bool'),
    'fToyClimaxMagnitude': ('Climax pulse', 'float', (0, 1, 0.05)),
}
# The Undressing slots page (OStim's slot mask page), Fallout 4's slot names.
SLOT_NAMES = {30: 'Hair Top', 31: 'Hair Long', 32: 'FaceGen Head', 33: 'BODY', 34: 'L Hand', 35: 'R Hand', 36: '[U] Torso', 37: '[U] L Arm', 38: '[U] R Arm', 39: '[U] L Leg', 40: '[U] R Leg', 41: '[A] Torso', 42: '[A] L Arm', 43: '[A] R Arm', 44: '[A] L Leg', 45: '[A] R Leg', 46: 'Headband', 47: 'Eyes', 48: 'Beard', 49: 'Mouth', 50: 'Neck', 51: 'Ring', 52: 'Scalp', 53: 'Decapitation', 54: 'Unnamed', 55: 'Unnamed', 56: 'Unnamed', 57: 'Unnamed', 58: 'Unnamed', 59: 'Shield', 60: 'Pipboy', 61: 'FX'}
for _slot, _name in SLOT_NAMES.items():
    SETTINGS[f'bUndressSlot{_slot}'] = (f'{_slot} - {_name}', 'bool')

# OStim's MCM (OSexIntegrationMCM.psc): its pages in its order, its section
# headers, and its label and tooltip keys ("$ostim_x" as "$4stim_x", English
# from tools/ostim-mcm-english.json). 4Stim's own settings go on the page
# that fits, under OStim's headers or a header of 4Stim's, with keys in the
# same style ("$4stim_<setting in snake case>", help from 4Stim.ini). OStim's
# Actors and Addons pages aren't here: 4Stim has no per-actor MCM settings
# (they're HUD options) and no addon MCM settings.
#
# A page: (OStim page key, [items]); an item is a setting key or ('#', header key).
OSTIM_TEXTS = os.path.join(ROOT, 'tools', 'ostim-mcm-english.json')

PAGES = [
    ('page_general', [
        ('#', 'header_scenes'), 'SetResetPosition', 'SetUseFades', 'SetUseIntroScenes', 'addActorsAtStart',
        'bResumeScenes', 'bEndOnDamage', 'bStayWhenInterrupted', 'fTransitionLead', 'bUsePlaybackSpeed', 'bLockScenes', 'bGuardScenes',
        ('#', 'header_partner'), 'sTargetMode', 'fMaxDistance', 'fCrosshairCone', 'fProximityRadius', 'fActorRadius',
        ('#', 'header_lights'), 'sMaleLight', 'sFemaleLight', 'sPlayerLight', 'SetOnlyLightInDark', 'fLightLevelDark',
        ('#', 'header_hud'), 'bEnableHUD', 'sHUDControls', 'sTheme', 'fHUDFadeTime',
        ('#', 'header_save_load'), '@saveload',
    ]),
    ('page_controls', [
        ('#', 'header_ostim_keys'), 'SetKeyMap', 'keyNpcSceneStart', 'SetKeyUp', 'SetKeyDown', 'SetPullOut', 'SetControlToggle',
        'SetFreeCamToggleKey', 'keySearch', 'keyAlignment', 'keyHideUI', 'SetUseRumble',
        ('#', 'header_osa_keys'), 'SetOsaUpKey', 'SetOsaDownKey', 'SetOsaLeftKey', 'SetOsaRightKey', 'SetOsaTogKey', 'SetOsaYesKey', 'SetOsaEndKey',
        ('#', 'header_controller'), 'sPadScheme', *PAD_KEYS,
    ]),
    ('page_auto_control', [
        ('#', 'header_auto_speed_control'), 'SetActorSpeedControl', 'autoSpeedControlIntervalMin', 'autoSpeedControlIntervalMax',
        'autoSpeedControlExcitementMin', 'autoSpeedControlExcitementMax',
        ('#', 'header_npc_scenes'), 'NPCSceneDuration', 'endNPCSceneOnOrgasm', 'bAutoModeNPC',
        ('#', 'header_auto_navigation'), 'navigationDistanceMax',
        ('#', 'header_auto_mode_toggle'), 'SetAIControl', 'SetForceAIForMasturbation', 'SetForceAIInConsensualScenes',
        ('#', 'header_auto_mode_settings'), 'autoModeLimitToNavigationDistance', 'SetUseAutoFades', 'autoModeAnimDurationMin', 'autoModeAnimDurationMax',
        'autoModeForeplayChance', 'autoModeForeplayThresholdMin', 'autoModeForeplayThresholdMax', 'autoModePulloutChance',
        'autoModePulloutThresholdMin', 'autoModePulloutThresholdMax', 'bAutoModeStandingOnFloor',
    ]),
    ('page_camera', [
        ('#', 'header_freecam'), 'SetUseFreeCam', 'SetFreeCamFOV', 'SetCameraSpeed', 'SetForceFirstPerson',
    ]),
    ('page_excitement', [
        ('#', 'header_excitement_calculation'), 'bEnableExcitement', 'SetsexExcitementMult', 'SetFemaleSexExcitementMult', 'excitementDecayRate',
        'excitementDecayGracePeriod', 'postOrgasmExcitement', 'postOrgasmExcitementMax',
        ('#', 'header_excitement_bars'), 'enablePlayerBar', 'enableNpcBar', 'SetAutoHideBar', 'SetMatchColorToGender',
        ('#', 'header_orgasms'), 'endOnPlayerOrgasm', 'SetEndOnOrgasm', 'SetEndOnSubOrgasm', 'SetEndOnBothOrgasm', 'SetSlowMoOrgasms',
        'SetBlurOrgasms', 'SetAutoClimaxAnims', 'fClimaxEndDelay',
        ('#', 'header_climax_effects'), 'fClimaxShake', 'fClimaxBlur', 'fClimaxFlash', 'fClimaxGlowSize',
    ]),
    ('page_gender_roles', [
        ('#', 'header_animation_settings'), 'SetOnlyGayAnimsInGayScenes',
        ('#', 'header_player_roles'), 'PlayerAlwaysDomStraight', 'PlayerAlwaysSubStraight', 'PlayerAlwaysDomGay', 'PlayerAlwaysSubGay',
        'playerSelectRoleStraight', 'playerSelectRoleGay', 'playerSelectRoleThreesome',
        ('#', 'header_strap_ons'), 'unequipStrapOnIfNotNeeded',
        ('#', 'header_futanari'), 'futaUseMaleRole', 'futaUseMaleExcitement', 'futaUseMaleClimax',
    ]),
    ('page_furniture', [
        'SetEnableFurniture', 'SetSelectFurniture', 'SetFurnitureSearchDistance', 'fFurnitureHeight', 'SetResetClutter', 'SetResetClutterRadius',
    ]),
    ('page_undress', [
        'bUndress', 'SetAlwaysUndressAtStart', 'SetUndressIfNeed', 'SetPartialUndressing', 'SetAnimateRedress',
        'bUndressPlayer', 'bRedress', 'fUndressItemDelay',
        ('#', 'undress_slots_header'), *[f'bUndressSlot{s}' for s in range(30, 62)],
    ]),
    ('page_expression', [
        'SetExpressionDurationMin', 'SetExpressionDurationMax',
        ('#', 'header_eyes'), 'bDisableEyeLook', 'fLookMaxSide', 'fLookMaxUp',
    ]),
    ('page_sound', [
        ('#', 'header_moans'), 'moanIntervalMin', 'moanIntervalMax', 'moanVolume',
        ('#', 'header_sounds'), 'soundVolume', 'bSounds', 'iSoundPriority',
    ]),
    ('page_alignment', [
        'SetScaling', 'SetSchlongBending', 'bErections', 'fPenisBendStep', 'iPenisBendAxis',
        ('#', 'header_alignment_menu'), 'alignmentGroupBySex', 'alignmentGroupByHeight', 'alignmentGroupByHeels',
    ]),
    ('page_toys', [
        ('#', 'header_toys'), 'bToysEnabled', 'bIntiface', 'bIntifaceScan',
        ('#', 'header_toys_synchronization'), 'sToySynchronization',
        ('#', 'header_toys_scaling'), 'sToyScaling', 'fToyMinScale', 'fToyMaxScale', 'fToyMinMagnitude', 'fToyMaxMagnitude',
        'bToyPeaks', 'fToyMinPeak', 'fToyMaxPeak',
        ('#', 'header_toys_climax'), 'bToyClimax', 'fToyClimaxMagnitude',
    ]),
    ('page_debug', [
        'unrestrictedNavigation', 'noFacialExpressions', 'bLogAnimEvents', 'bLogFurniture',
    ]),
    ('page_about', ['@about']),
]

# 4Stim's headers (OStim's come from its file).
OWN_HEADERS = {
    'header_partner': 'Partner',
    'header_hud': 'HUD',
    'header_controller': 'Controller',
    'header_climax_effects': 'Climax Effects',
    'header_eyes': 'Eyes',
}

# Settings OStim has: its label key and tooltip key (None: OStim shows none,
# or 4Stim's setting works differently, so 4Stim's own tooltip, from
# 4Stim.ini, under a key of its own).
OSTIM_KEYS = {
    'SetResetPosition': ('reset_position', None),  # 4Stim doesn't move the player back
    'SetUseFades': ('use_fades', 'tooltip_fades'),
    'SetUseIntroScenes': ('use_intro_scenes', 'tooltip_use_intro_scenes'),
    'addActorsAtStart': ('add_actors_at_start', 'tooltip_add_actors_at_start'),
    'sMaleLight': ('male_light_mode', 'tooltip_male_light_mode'),
    'sFemaleLight': ('female_light_mode', 'tooltip_female_light_mode'),
    'sPlayerLight': ('player_light_mode', 'tooltip_player_light_mode'),
    'SetOnlyLightInDark': ('dark_light', 'tooltip_dark_light'),
    'SetKeyMap': ('main_key', None),  # 4Stim's hotkey: the picker, then the HUD's keys
    'keyNpcSceneStart': ('npc_start_key', 'tooltip_npc_start_key'),
    'SetKeyUp': ('speed_up_key', 'tooltip_speed_up_key'),
    'SetKeyDown': ('speed_down_key', 'tooltip_speed_down_key'),
    'SetPullOut': ('pullout_key', 'tooltip_pullout_key'),
    'SetControlToggle': ('control_toggle_key', None),  # on / off for the scene
    'SetFreeCamToggleKey': ('tfc_key', 'tooltip_tfc_key'),
    'keySearch': ('key_search_menu', 'tooltip_key_search_menu'),
    'keyAlignment': ('key_alignment_menu', None),  # saved at once, the Align tab
    'keyHideUI': ('key_hide_ui', 'tooltip_key_hide_ui'),
    'SetUseRumble': ('use_rumble', None),  # at a climax, not on thrusts
    'SetOsaUpKey': ('osaKeys_up', 'tooltip_osa_up'),
    'SetOsaDownKey': ('osaKeys_down', 'tooltip_osa_down'),
    'SetOsaLeftKey': ('osaKeys_left', 'tooltip_osa_left'),
    'SetOsaRightKey': ('osaKeys_right', 'tooltip_osa_right'),
    'SetOsaTogKey': ('osaKeys_tog', None),  # also between the list and the tabs
    'SetOsaYesKey': ('osaKeys_yes', 'tooltip_osa_yes'),
    'SetOsaEndKey': ('osaKeys_end', 'tooltip_osa_end'),
    'SetActorSpeedControl': ('auto_speed_control', 'tooltip_auto_speed_control'),
    'autoSpeedControlIntervalMin': ('auto_speed_control_interval_min', 'tooltip_auto_speed_control_interval_min'),
    'autoSpeedControlIntervalMax': ('auto_speed_control_interval_max', 'tooltip_auto_speed_control_interval_max'),
    'autoSpeedControlExcitementMin': ('auto_speed_control_excitement_min', 'tooltip_auto_speed_control_excitement_min'),
    'autoSpeedControlExcitementMax': ('auto_speed_control_excitement_max', 'tooltip_auto_speed_control_excitement_max'),
    'NPCSceneDuration': ('npc_scene_duration', 'tooltip_npc_scene_duration'),
    'endNPCSceneOnOrgasm': ('end_npc_scene_on_orgasm', 'tooltip_end_npc_scene_on_orgasm'),
    'navigationDistanceMax': ('navigation_distance_max', 'tooltip_navigation_distance_max'),
    'SetAIControl': ('use_auto_mode_always', 'tooltip_use_auto_mode_always'),
    'SetForceAIForMasturbation': ('use_auto_mode_solo', 'tooltip_use_auto_mode_solo'),
    # OStim's "vanilla" toggle is this setting (no dominant / submissive roles,
    # Thread::evaluateAutoMode); its dominant / submissive toggles are TODO in OStim's code.
    'SetForceAIInConsensualScenes': ('use_auto_mode_vanilla', 'tooltip_use_auto_mode_vanilla'),
    'autoModeLimitToNavigationDistance': ('auto_mode_limit_to_navigation_distance', 'tooltip_auto_mode_limit_to_navigation_distance'),
    'SetUseAutoFades': ('use_auto_mode_fades', 'tooltip_use_auto_mode_fades'),
    'autoModeAnimDurationMin': ('auto_mode_anim_duration_min', 'tooltip_auto_mode_anim_duration_min'),
    'autoModeAnimDurationMax': ('auto_mode_anim_duration_max', 'tooltip_auto_mode_anim_duration_max'),
    'autoModeForeplayChance': ('auto_mode_foreplay_chance', 'tooltip_auto_mode_foreplay_chance'),
    # OStim's tooltip keys spell it "treshold"; kept, so its translations fit.
    'autoModeForeplayThresholdMin': ('auto_mode_foreplay_threshold_min', 'tooltip_auto_mode_foreplay_treshold_min'),
    'autoModeForeplayThresholdMax': ('auto_mode_foreplay_threshold_max', 'tooltip_auto_mode_foreplay_treshold_max'),
    'autoModePulloutChance': ('auto_mode_pullout_chance', 'tooltip_auto_mode_pullout_chance'),
    'autoModePulloutThresholdMin': ('auto_mode_pullout_threshold_min', 'tooltip_auto_mode_pullout_treshold_min'),
    'autoModePulloutThresholdMax': ('auto_mode_pullout_threshold_max', 'tooltip_auto_mode_pullout_treshold_max'),
    'SetUseFreeCam': ('freecam', 'tooltip_auto_fc'),
    'SetFreeCamFOV': ('freecam_fov', 'tooltip_fov'),
    'SetCameraSpeed': ('freecam_speed', 'tooltip_fc_speed'),
    'SetForceFirstPerson': ('force_first', 'tooltip_force_first'),
    'SetsexExcitementMult': ('excitement_mult_male', 'tooltip_excitement_mult_male'),
    'SetFemaleSexExcitementMult': ('excitement_mult_female', 'tooltip_excitement_mult_female'),
    'excitementDecayRate': ('excitement_decay_rate', 'tooltip_excitement_decay_rate'),
    'excitementDecayGracePeriod': ('excitement_decay_grace_period', 'tooltip_excitement_decay_grace_period'),
    'postOrgasmExcitement': ('post_orgasm_excitement', 'tooltip_post_orgasm_excitement'),
    'postOrgasmExcitementMax': ('post_orgasm_excitement_max', 'tooltip_post_orgasm_excitement_max'),
    'enablePlayerBar': ('player_bar', 'tooltip_player_bar'),
    'enableNpcBar': ('npc_bar', 'tooltip_npc_bar'),
    'SetAutoHideBar': ('auto_hide_bar', None),  # with the HUD's fade, not after 15 s
    'SetMatchColorToGender': ('match_color_gender', 'tooltip_gendered_colors'),
    'endOnPlayerOrgasm': ('orgasm_end_player', 'tooltip_orgasm_end_player'),
    'SetEndOnOrgasm': ('orgasm_end_male', 'tooltip_orgasm_end_male'),
    'SetEndOnSubOrgasm': ('orgasm_end_female', 'tooltip_orgasm_end_female'),
    'SetEndOnBothOrgasm': ('orgasm_end_all', 'tooltip_orgasm_end_all'),
    'SetSlowMoOrgasms': ('slowmo_orgasm', 'tooltip_slowmo_orgasms'),
    'SetBlurOrgasms': ('blur_orgasm', 'tooltip_blur_orgasms'),
    'SetAutoClimaxAnims': ('auto_climax_anims', 'tooltip_auto_climax_anims'),
    'SetOnlyGayAnimsInGayScenes': ('intended_sex_only', 'tooltip_intended_sex_only'),
    'PlayerAlwaysDomStraight': ('always_dom_straight', 'tooltip_always_dom_straight'),
    'PlayerAlwaysSubStraight': ('always_sub_straight', 'tooltip_always_sub_straight'),
    'PlayerAlwaysDomGay': ('always_dom_gay', 'tooltip_always_dom_gay'),
    'PlayerAlwaysSubGay': ('always_sub_gay', 'tooltip_always_sub_gay'),
    'playerSelectRoleStraight': ('select_role_straight', 'tooltip_select_role_straight'),
    'playerSelectRoleGay': ('select_role_gay', 'tooltip_select_role_gay'),
    'playerSelectRoleThreesome': ('select_role_threesome', 'tooltip_select_role_threesome'),
    'unequipStrapOnIfNotNeeded': ('unequip_strap_on_if_not_needed', 'tooltip_unequip_strap_on_if_not_needed'),
    'futaUseMaleRole': ('futa_use_male_role', 'tooltip_futa_use_male_role'),
    'futaUseMaleExcitement': ('futa_use_male_excitement', 'tooltip_futa_use_male_excitement'),
    'futaUseMaleClimax': ('futa_use_male_orgasm', 'tooltip_futa_use_male_orgasm'),
    'SetEnableFurniture': ('use_furniture', 'tooltip_enable_furniture'),
    'SetSelectFurniture': ('select_furniture', 'tooltip_select_furniture'),
    'SetFurnitureSearchDistance': ('furniture_search_rad', 'tooltip_furniture_search_dist'),
    'SetResetClutter': ('reset_clutter', 'tooltip_reset_clutter'),
    'SetResetClutterRadius': ('reset_clutter_radius', 'tooltip_reset_clutter_radius'),
    'SetAlwaysUndressAtStart': ('undress_start', 'tooltip_always_undress'),
    'SetUndressIfNeed': ('undress_need', 'tooltip_undress_if_need'),
    'SetPartialUndressing': ('partial_undressing', 'tooltip_partial_undressing'),
    'SetAnimateRedress': ('animate_redress', 'tooltip_animate_redress'),
    'SetExpressionDurationMin': ('expression_duration_min', 'tooltip_expression_duration_min'),
    'SetExpressionDurationMax': ('expression_duration_max', 'tooltip_expression_duration_max'),
    'moanIntervalMin': ('moan_interval_min', 'tooltip_moan_interval_min'),
    'moanIntervalMax': ('moan_interval_max', 'tooltip_moan_interval_max'),
    'moanVolume': ('moan_volume', 'tooltip_moan_volume'),
    'soundVolume': ('sound_volume', 'tooltip_sound_volume'),
    'SetScaling': ('scaling', 'tooltip_scaling'),
    'SetSchlongBending': ('schlong_bending', 'tooltip_schlong_bending'),
    'alignmentGroupBySex': ('alignment_group_by_sex', 'tooltip_alignment_group_by_sex'),
    'alignmentGroupByHeight': ('alignment_group_by_height', 'tooltip_alignment_group_by_height'),
    'alignmentGroupByHeels': ('alignment_group_by_heels', 'tooltip_alignment_group_by_heels'),
    'sToySynchronization': ('toy_slot_synchronization_type', None),  # one setting for every toy, no "individual"
    'sToyScaling': ('toy_slot_scaling_type', None),
    'fToyMinScale': ('toy_slot_min_scale', 'tooltip_toy_slot_min_scale'),
    'fToyMaxScale': ('toy_slot_max_scale', 'tooltip_toy_slot_max_scale'),
    'fToyMinMagnitude': ('toy_slot_min_magnitude', 'tooltip_toy_slot_min_magnitude'),
    'fToyMaxMagnitude': ('toy_slot_max_magnitude', 'tooltip_toy_slot_max_magnitude'),
    'bToyPeaks': ('toy_slot_do_peaks', 'tooltip_toy_slot_do_peaks'),
    'fToyMinPeak': ('toy_slot_min_peak', 'tooltip_toy_slot_min_peak'),
    'fToyMaxPeak': ('toy_slot_max_peak', 'tooltip_toy_slot_max_peak'),
    'bToyClimax': ('toy_slot_do_climax', 'tooltip_toy_slot_do_climax'),
    'fToyClimaxMagnitude': ('toy_slot_climax_magnitude', 'tooltip_toy_slot_climax_magnitude'),
    'unrestrictedNavigation': ('unrestricted_navigation', 'tooltip_unrestricted_navigation'),
    'noFacialExpressions': ('no_facial_expressions', 'tooltip_no_facial_expressions'),
}

# Tooltips for OStim's settings that work differently in 4Stim (OSTIM_KEYS
# without a tooltip key), in OStim's style.
OWN_TOOLTIPS = {
    'SetResetPosition': 'After the scene ends reset NPCs to where they were when the scene started. The player isn\'t moved back.',
    'SetKeyMap': 'Out of a scene, opens the scene picker. In a scene, hands the HUD\'s keys back to the game, or takes them again.',
    'SetControlToggle': 'Turns auto mode on or off for the scene you\'re in or watching.',
    'keyAlignment': 'Opens the alignment menu (the HUD\'s Align tab); pressed again, goes back to navigation. Changes are saved at once.',
    'SetUseRumble': 'Rumble a controller when an actor climaxes, if a controller is being used.',
    'SetOsaTogKey': 'In the alignment menu this key changes actors. Elsewhere it moves between the HUD\'s list and its tab buttons.',
    'SetAutoHideBar': 'Hide the bars when the HUD fades out. If disabled the bars stay up for the whole scene.',
    'sToySynchronization': 'Changes which actor in the scene the toys are synchronized with, for every toy.',
    'sToyScaling': 'Changes how the toys\' power is calculated, for every toy.',
}

# The controller's buttons take the names of the keys they stand for.
PAD_LABEL_KEYS = {
    'sPadHotkey': 'main_key', 'sPadNPCScene': 'npc_start_key', 'sPadSpeedUp': 'speed_up_key', 'sPadSpeedDown': 'speed_down_key',
    'sPadAutoMode': 'control_toggle_key', 'sPadPullOut': 'pullout_key', 'sPadEnd': 'osaKeys_end', 'sPadSearch': 'key_search_menu',
    'sPadAlign': 'key_alignment_menu', 'sPadFreeCam': 'tfc_key', 'sPadHideUI': 'key_hide_ui',
}

# OStim's dropdown texts for choices 4Stim shares: (value -> OStim key).
OSTIM_CHOICES = {
    'sMaleLight': ['light_none', 'light_face', 'light_face_dim', 'light_crotch', 'light_crotch_dim', 'light_ass', 'light_ass_dim'],
    'sFemaleLight': ['light_none', 'light_face', 'light_face_dim', 'light_crotch', 'light_crotch_dim', 'light_ass', 'light_ass_dim'],
    'sPlayerLight': ['generic_default', 'light_none', 'light_face', 'light_face_dim', 'light_crotch', 'light_crotch_dim', 'light_ass', 'light_ass_dim'],
    'sToySynchronization': ['synchronization_type_player', 'synchronization_type_male', 'synchronization_type_female', 'synchronization_type_scene'],
    'sToyScaling': ['scaling_type_none', 'scaling_type_stimulation_rate', 'scaling_type_stimulation_value', 'scaling_type_speed'],
}

# OStim's English, made to fit 4Stim where it names Skyrim-only mods or
# OStim's own quirks (the key's meaning is the same).
OSTIM_ENGLISH_FIXES = {
    'tooltip_fov': 'Sets the field of view of the camera when in FreeCam mode.',  # OStim: "incompatible with Improved Camera"
    'tooltip_tfc_key': 'Toggle FreeCam during scenes involving the player.',  # OStim: Improved Camera advice
}


def snake(name):
    """bResumeScenes -> resume_scenes; NPCSceneDuration -> npc_scene_duration."""
    if re.match(r'^[bifs][A-Z]', name):
        name = name[1:]
    name = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1_\2', name)
    name = re.sub(r'([a-z0-9])([A-Z])', r'\1_\2', name)
    return name.lower()


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
            value = value.split(';', 1)[0].strip()  # an inline comment
            out[key] = (section, value, ' '.join(comment))
    return out


def mcm_key(key, kind):
    if kind == 'ms':
        return 'f4S_' + key  # OStim's milliseconds, shown in seconds
    if kind == 'key':
        return 's4S_' + key  # MCM's key input keeps "keycode,modifiers" in a string
    if kind == 'choice' and not re.match(r'^[bifs][A-Z]', key):
        return 'i4S_' + key
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
    if kind == 'key':
        return f'{int(value, 0)},0'
    if kind == 'ms':
        return f'{float(value) / 1000:g}'
    return value


def main():
    ini = read_ini()
    ostim = {k: v for k, v in json.load(open(OSTIM_TEXTS, encoding='utf-8')).items() if not k.startswith('_')}
    texts = {}

    def text(key, english):
        if key in texts and texts[key] != english:
            raise SystemExit(f'{key}: "{texts[key]}" vs "{english}"')
        texts[key] = english
        return key

    def ostim_text(key):
        """OStim's key as 4Stim's, with OStim's English (fixed where it must be)."""
        english = OSTIM_ENGLISH_FIXES.get(key, ostim[key]).replace('\\n', ' ').replace('OStim', '4Stim')
        return text('$4stim_' + key, english)

    def header(key):
        return {'type': 'section', 'text': ostim_text(key) if key in ostim else text('$4stim_' + key, OWN_HEADERS[key])}

    def button(key, english, tip, function):
        return {'text': text('$4stim_' + key, english), 'type': 'button', 'help': text('$4stim_tooltip_' + key, tip),
                'action': {'type': 'CallGlobalFunction', 'script': 'FourStimMCM', 'function': function, 'params': []}}

    defaults = {}
    pages = []
    for page_key, items in PAGES:
        content = []
        for item in items:
            if isinstance(item, tuple):
                content.append(header(item[1]))
                continue
            if item == '@saveload':
                # OStim's save and load settings, and 4Stim.ini.
                content.append({'text': text('$4stim_settings_note', 'Settings changed here are kept in Data\\MCM\\Settings\\4Stim.ini, over 4Stim.ini. Whichever of the two you changed last wins when the game starts. Only in 4Stim.ini: bone names (sPenisBendNode, sLeftEyeNode, sRightEyeNode) and sound flags (iSoundFlags).'), 'type': 'text'})
                content.append({'text': ostim_text('export'), 'type': 'button', 'help': text('$4stim_tooltip_export', 'Click this button to export the 4Stim MCM settings (to Documents\\My Games\\Fallout4\\4Stim\\mcmsettings.json).'),
                                'action': {'type': 'CallGlobalFunction', 'script': 'FourStimMCM', 'function': 'ExportSettings', 'params': []}})
                content.append({'text': ostim_text('import'), 'type': 'button', 'help': ostim_text('tooltip_import'),
                                'action': {'type': 'CallGlobalFunction', 'script': 'FourStimMCM', 'function': 'ImportSettings', 'params': []}})
                content.append(button('use_ini', 'Use 4Stim.ini\'s settings', 'Forgets what was changed here: 4Stim.ini\'s values again.', 'UseIniSettings'))
                continue
            if item == '@about':
                content.append({'text': '4Stim', 'type': 'text'})
                content.append({'text': text('$4stim_about_port', 'A port of OStim Standalone to Fallout 4.'), 'type': 'text'})
                content.append(header('authors'))
                content.append({'text': '4Stim: KeryoWolfe', 'type': 'text'})
                content.append({'text': 'OStim Standalone: Kannonfodder, VersuchDrei', 'type': 'text'})
                content.append({'text': 'OStim: Nem, Qudix, Sairion', 'type': 'text'})
                content.append(header('links'))
                content.append({'text': 'https://github.com/KeryoWolfe/4Stim', 'type': 'text'})
                content.append({'text': 'https://github.com/VersuchDrei/OStimNG', 'type': 'text'})
                continue
            key = item
            if key not in ini:
                raise SystemExit(f'{key} is not in 4Stim.ini')
            if key not in SETTINGS:
                raise SystemExit(f'{key} has no entry in SETTINGS')
            section, value, comment = ini[key]
            spec = SETTINGS[key]
            label, kind = spec[0], spec[1]
            name = mcm_key(key, kind)
            if key.startswith('bUndressSlot'):
                slot = key[len('bUndressSlot'):]
                label_key = text(f'$4stim_slot_{slot}', f'S. {slot} {SLOT_NAMES[int(slot)]}')
                help_key = text('$4stim_tooltip_undressing_about', 'Select which armor slots are stripped by 4Stim. These settings apply to both the player and NPCs. Anything covering an undressed slot comes off too, unless it has a NoStrip keyword. Take the names with a grain of salt, as many mods don\'t use the proper slots.')
            elif key in OSTIM_KEYS:
                label_suffix, tip = OSTIM_KEYS[key]
                label_key = ostim_text(label_suffix)
                if tip:
                    help_key = ostim_text(tip)  # its sliders show seconds too
                else:
                    # Not OStim's meaning: a key of 4Stim's, so OStim's translations don't put OStim's text here.
                    help_key = text(f'$4stim_tooltip_{label_suffix}_4stim', OWN_TOOLTIPS[key])
            elif key in PAD_KEYS:
                # The controller's buttons: the Controls page's names for the keys.
                label_key = ostim_text(PAD_LABEL_KEYS[key])
                help_key = text(f'$4stim_tooltip_{snake(key)}', comment or label)
            else:
                label_key = text(f'$4stim_{snake(key)}', label)
                help_key = text(f'$4stim_tooltip_{snake(key)}', comment or label)
            control = {
                'id': f'{name}:{section}',
                'text': label_key,
                'help': help_key,
                'action': {'type': 'CallGlobalFunction', 'script': 'FourStimMCM', 'function': 'SettingChanged', 'params': []},
            }
            if kind == 'key':
                # MCM's key input: a string "keycode,modifiers" (its
                # MCM_Menu splits valueString; an int setting broke the
                # pages after it). Keycodes are Windows virtual keys.
                control['type'] = 'keyinput'
                control['valueOptions'] = {'sourceType': 'ModSettingString', 'allowModifierKeys': False}
            elif kind == 'bool':
                control['type'] = 'switcher'
                control['valueOptions'] = {'sourceType': 'ModSettingBool'}
            elif kind == 'choice':
                if key in OSTIM_CHOICES:
                    options = [ostim_text(o) for o in OSTIM_CHOICES[key]]
                elif key in PAD_KEYS:
                    options = [text(f'$4stim_pad_button_{b.lower()}', o) for b, o in zip(PAD_BUTTONS, PAD_LABELS)]
                else:
                    options = [text(f'$4stim_{snake(key)}_{v.lower()}', o) for v, o in zip(CHOICES[key], CHOICE_LABELS[key])]
                control['type'] = 'dropdown'
                control['valueOptions'] = {'sourceType': 'ModSettingInt', 'options': options}
            else:
                low, high, step = spec[2]
                control['type'] = 'slider'
                control['valueOptions'] = {'min': low, 'max': high, 'step': step,
                                           'sourceType': 'ModSettingInt' if kind == 'int' else 'ModSettingFloat'}
            content.append(control)
            defaults.setdefault(section, []).append((name, mcm_value(key, kind, value)))
        pages.append({'pageDisplayName': ostim_text(page_key), 'content': content})

    # Every setting on a page once.
    placed = [i for _, items in PAGES for i in items if isinstance(i, str) and not i.startswith('@')]
    missing = sorted(set(SETTINGS) - set(placed))
    doubled = sorted({k for k in placed if placed.count(k) > 1})
    if missing or doubled:
        raise SystemExit(f'not on a page: {missing}; on two: {doubled}')

    config = {
        'modName': '4Stim',
        'displayName': '4Stim',
        'minMcmVersion': 1,
        'content': [
            {'type': 'spacer'},
            # The logo (Interface-src/logo/FourStimLogo.as, the picture
            # 4StimMCMLogo.png), built into Data/MCM/Config/4Stim/lib.swf by
            # Interface-src/build.bat.
            {'type': 'image', 'libName': '4Stim', 'className': 'FourStimLogo'},
        ],
        'pages': pages,
    }
    # The Add-Ons page's name: the plugin writes that page at startup from
    # add-ons' settings files (src/AddonSettings.cpp), so only its text is here.
    ostim_text('page_addons')
    # No MCM hotkeys any more: the Controls page sets 4Stim.ini's keys
    # (an empty list, so an older install's file doesn't keep them).
    keybinds = {'modName': '4Stim', 'keybinds': []}

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
    print(len(pages), 'page(s),', sum(len(e) for e in defaults.values()), 'setting(s),', len(texts), 'text(s)')


if __name__ == '__main__':
    main()
