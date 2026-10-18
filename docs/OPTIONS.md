# Options in the HUD

OStim's in-scene options menu, in the HUD's Utility tab: pages of options, per actor or for the scene, each calling a Papyrus function. 4Stim reads **OStim's options files** in `Data\F4SE\Plugins\4Stim\Options\` (subfolders too; OStim's `SKSE\Plugins\OStim\options`), so an add-on's options work the same way.

4Stim's own, in `Options\4Stim.json`: **Actors**, a page for each actor in the scene with **Undress / Dress**, **Mute / Unmute**, and their **Strap-on** and **Tongue** (on / off, and which one they wear, from the equip objects there are: ACTORS.md). The one an actor wears is marked `*`.

## Using it

In a scene, the Utility tab lists its own entries (Change furniture..., the Utility files' entries), then the options files' top pages. Choosing a page opens it: the tab then shows only that page, with **Back** at the top. A different scene goes back to the top.

## File format

As OStim's:

```json
{
	"pages": [
		{ "name": "MyMod", "description": "My mod", "icon": "4Stim/symbols/climax", "parent": null },
		{ "name": "MyModActor", "description": "My mod: {actor}", "parent": "MyMod", "repeat": "actor" }
	],
	"options": [
		{
			"name": "Blush",
			"page": "MyModActor",
			"states": {
				"0": { "description": "Make {actor} blush", "icon": "4Stim/symbols/arrow_up", "border": "ff00ff", "function": "MyModOptions/Blush" }
			}
		}
	]
}
```

| Field | Meaning |
|---|---|
| page `name` | Its id (a page already defined by an earlier file is skipped, as OStim). |
| page `description` | What the row says; `{actor}` is the actor's name on a per-actor page. |
| page `sex` | 4Stim's: `"female"` or `"male"` shows the page only for an actor of that sex (its own actor, or the one of the page above). 4Stim's strap-on page is for women (futa women too). |
| page `icon`, `border` | The row's icon (a path under `Data\Interface\4Stim\Icons\`; OStim's `OStim/...` paths are taken as `4Stim/...`) and its border color (`rrggbb`). 4Stim's: on a per-actor page, `{sex}` in the icon is the actor's `m` or `f` (`4Stim/symbols/gender_{sex}`); OStim shows one icon for every actor. |
| page `parent` | The page it's in; `null` for the top. |
| page `repeat` | `"actor"`: one page per actor in the scene. |
| option `page` | The page it's in. |
| option `repeat` | `"actor"`: one option per actor; `"equipObject"` with `equipType`: a `toggle` option, then an `object` option for every equip object of that type. |
| option `states` | What it shows and does: each state's `description`, `icon`, `border` and `function` (`"Script/Function"`, a global function). |

The function gets, as OStim's: `(String asState)`, on an actor's page or a per-actor option `(String asState, Actor akActor)`, and for an `object` option `(String asState, Actor akActor, String asObjectID)`. In a description, `{actor}` is the actor's name (you, as "yourself") and `{type}` the equip object's name.

Which state shows: OStim shows the first (its `stateFaction` isn't used in OStim NG either), and an `equipObject` option's `toggle` / `object`. 4Stim adds `"state"` on an option, to show `"on"` or `"off"` by what's true now: `"stripped"` (undressed by 4Stim), `"muted"`, or `"equipped:<type>"` (wearing an equip object of that type). The state's function decides what to do (4Stim's `FourStimOptions.psc` toggles).

Texts starting with `$4Stim_` are translated (TRANSLATIONS.md); a file's `"english"` (key to text) gives their English. A `$` text of another mod's is shown as it is.

## Papyrus for options

`FourStim.Mute` / `Unmute` / `IsMuted` (OStim's `OActor` ones), `FourStim.UndressActor` / `RedressActor`, `FourStim.EquipObject` / `UnequipObject` / `IsObjectEquipped`, and 4Stim's `FourStim.SetEquipObjectID(akActor, asType, asID)` / `GetEquipObjectID` (which object of a type an actor wears, until a game is loaded).
