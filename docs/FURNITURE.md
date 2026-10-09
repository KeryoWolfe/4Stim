# Furniture types

A scene with `"furniture": "<type>"` (see `SCENES.md`) is played on a piece of furniture of that type. Furniture types say which objects in the world count as that type, and where on them a scene goes. They're defined in JSON files in `Data\F4SE\Plugins\4Stim\Furniture\`. 4Stim ships `Fallout4.json` for the base game's objects; mods can add their own files. Files are read in name order, and a type with the same `id` as an earlier one replaces it. They're re-read every time the picker opens, so edits apply without restarting the game.

## How furniture is found

When the picker opens for a new scene, 4Stim looks at the objects within `fFurnitureRadius` of the player (and `fFurnitureHeight` up or down, so the floor above doesn't count), in `4Stim.ini`. It gives each object its most specific matching type (highest `priority`) and keeps the nearest piece of each type. A scene for type T is listed if there's a piece of type T or of a subtype of T (a type whose `supertype` chain leads to T). For example, scenes for `chair` can be played on a `bench`, since a bench is a kind of chair. When one is picked, the actors are moved onto the nearest piece it fits.

With `bLogFurniture=1`, every object looked at is written to `4Stim.log`, with its form type, model path, number of furniture markers and the type it got, plus where each furniture scene is placed. That's the way to find out what to put in `models` for a mod's furniture, and to tune offsets.

## File format

```json
{
	"types": [
		{
			"id": "bed",
			"name": "Bed",
			"forms": ["FURN"],
			"models": ["bed", "mattress", "sleepingbag"],
			"excludeModels": ["bedside", "nightstand"],
			"minMarkers": 1,
			"ignoreMarkerAxes": ["x"],
			"offset": [0, 0, 0],
			"rotation": 0
		}
	]
}
```

| Field | Required | Meaning |
|---|---|---|
| `id` | yes | The type's ID, used in scene files (case-insensitive). |
| `name` | no | Display name, shown in the picker. Defaults to the ID. |
| `supertype` | no | The type this is a kind of. Scenes for the supertype can be played on this type too. `"none"` means scenes without any `furniture` (the ordinary floor scenes) can be played on it too, the way OStim treats beds: during a bed scene, navigation and search also offer the floor scenes, played on the bed. |
| `priority` | no | When an object matches several types, the highest priority wins (default 0). Give subtypes a higher priority than their supertype. |
| `forms` | no | Base object types it can be: `FURN` (furniture, the default), `STAT` (static), `MSTT` (movable static), `ACTI` (activator). Many tables are statics. |
| `models` | one of `models` or `keywords` | It matches if its model path contains any of these (case-insensitive). |
| `excludeModels` | no | It doesn't match if its model path contains any of these. |
| `keywords` | one of `models` or `keywords` | It matches if its base object has any of these keywords (editor IDs). |
| `excludeKeywords` | no | It doesn't match if its base object has any of these keywords. |
| `minMarkers`, `maxMarkers` | no | How many furniture markers (places to sit, lie or lean) it must have. Only furniture has markers. For example, `"minMarkers": 2` tells a double bed from a single one. |
| `anchor` | no | Where scenes go: `"marker"` (the default) at one of its furniture markers, read from its loaded model; `"origin"` at the object's own origin and facing; `"edge"` on an edge of its bounding box (below). Without markers, `"marker"` falls back to the origin. Everything found (markers, bounds, the spot) is logged whenever a furniture scene is placed. Or `"center"`: in the middle of its bounding box, on top of it, turned lengthwise (toward the end farther from the player), for lying on a mattress or a sleeping bag. |
| `marker` | no | Which marker to use, counting from 0 (default 0). |
| `edgeSide` | no | `"edge"`: a `"long"` side (default) or a `"short"` one. Of the two, the one nearer the player is used. |
| `facing` | no | `"edge"`: the scene faces `"out"` from the object (default; sitting on a bed's edge) or `"in"` toward it (standing at a table). |
| `edgeInset` | no | `"edge"`: how far in from the edge the spot is (default 0); negative is outside the object. |
| `onFloor` | no | `"edge"` or `"center"`: put the spot at the bottom of its bounds, where it stands (tables, benches), whatever height its origin is at. |
| `markerHeight` | no | `"edge"` or `"center"`: take the height from the marker (when the model has one) instead of `offset`'s z, e.g. a bed's mattress. |
| `ignoreMarkerAxes` | no | `"marker"`: axes of the marker's position to ignore (`"x"`, `"y"`, `"z"`), so the scene sits on the object's center line along that axis instead. For example, `["z"]` keeps the actors on the floor in front of a chair. |
| `offset` | no | `[x, y, z]` added to the spot, in the object's own frame: x to its right, y forward, z up. Scaled with the object. |
| `floorScenes` | no | For a type whose chain reaches `"none"` (it also takes scenes made without furniture): which of those scenes it takes, by their actors' `tags` (SCENES.md). `needTags`: at least one actor has one of these. `excludeTags`: no actor has any of these. Scenes made for this furniture type are not affected. |
| `rotation` | no | Degrees added to the scene's facing. |

A type with neither `models` nor `keywords` matches nothing by itself; it only exists as a supertype for others.

## How the shipped types place scenes

The OStim animations in the test pack share one origin per kind of furniture, worked out from the clips themselves:

- **Bed:** on the mattress at a long edge, facing out: in sitting scenes the pelvis is at the origin and the feet hang about 25 units forward and 33 down. So `bed` is anchored on the edge nearer the player, 15 in, at the mattress's height (the sleep marker's when the model has one, else 32 up). 
- **Mattress:** mattresses and sleeping bags lie on the ground, so bed scenes (sitting on the edge, standing beside it) don't work on them. `mattress` is its own type with supertype `none`: it takes every scene made without furniture (lying, kneeling, standing...), centered on the mattress, lengthwise, 2 up. Like OStim's bedrolls in Skyrim: floor scenes, not the scenes for sitting on a bed's edge. (To limit a type to some of those scenes, see `floorScenes`.)
- **Bench:** on the floor under the seat, facing out: the pelvis is 38 above the origin. So `bench` is anchored 18 in from the long edge nearer the player, on the floor.
- **Table:** on the floor at the table's edge, facing the table: one actor bends over it in front of the origin, the other stands 50 behind. So `table` is anchored 5 outside the long edge nearer the player, facing in.

A scene whose own animation starts somewhere else can shift itself with `furnitureOffset` (see `SCENES.md`).
