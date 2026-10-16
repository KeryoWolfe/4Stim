# Lights

As OStim does, 4Stim can light the actors in a scene: a light on each one's face, crotch or behind, bright or dim, so scenes in dark places can be seen.

## Choosing a light

In `4Stim.ini`, `[Lights]`:

| Setting | Default | Meaning |
|---|---|---|
| `sMaleLight` / `sFemaleLight` | `nolight` | The light men / women get: a file name in `Data\F4SE\Plugins\4Stim\Lights\` without `.json`. |
| `SetOnlyLightInDark` | 0 | 1 = only when it's dark where the scene starts (OStim's setting). |
| `fLightLevelDark` | 20 | Below this light level counts as dark: the game's own measure, as for stealth. 20 is OStim's (Skyrim's scale); `4Stim.log` gives the level at each scene start, at the first actor and at you (`Lights: light level ...`), so you can tune it. It's the game's stealth measure, so lamps nearby count: a settlement at night can be well over 20. |

The lights that come with 4Stim are OStim's: `facelight`, `facelightdim`, `crotchlight`, `crotchlightdim`, `asslight`, `asslightdim`, and `nolight` (OStim's default: none). In OStim the light is chosen per sex (or per NPC) in its MCM; 4Stim has no MCM, so it's per sex in the ini.

As in OStim, actors get their light when the scene starts (with `SetOnlyLightInDark`, if it's dark where the first actor stands), keep it through the scene, and lose it when it ends.

## Light files

```json
{
	"name": "Face light",
	"light": { "mod": "Fallout4.esm", "formid": "0x01ED2D" },
	"node": "HEAD",
	"offset": { "x": 0, "y": 30, "z": 5 },
	"brightness": 1.0
}
```

| Field | Meaning |
|---|---|
| `name` | Its name. |
| `light` | The light (a LIGH record) to place, by plugin and form ID. None: no light (`nolight`). |
| `node` | The bone it follows: `HEAD`, `Pelvis`, `Chest`... |
| `offset` | From that bone, in game units: `x` to the actor's right, `y` forward, `z` up (by the way the actor faces). |
| `brightness` | The light's dimmer: 1 = as the light is made, 0.4 for the dim ones. |

OStim's lights are armor items with a light in their mesh. 4Stim has no plugin of its own, so its lights are game lights placed next to the actor and moved with the bone every frame; the default ones are Fallout 4's Pip-Boy light (`defaultPipboyLight`), dimmed for the dim versions. Any LIGH record from any plugin can be used instead. The lights are removed before every save and put back after, so none end up in your saves.

## Not in yet

Lights per NPC, the check for actors who already have a light from another mod (OStim's), and changing lights from the HUD.
