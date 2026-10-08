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
| `supertype` | no | The type this is a kind of. Scenes for the supertype can be played on this type too. |
| `priority` | no | When an object matches several types, the highest priority wins (default 0). Give subtypes a higher priority than their supertype. |
| `forms` | no | Base object types it can be: `FURN` (furniture, the default), `STAT` (static), `MSTT` (movable static), `ACTI` (activator). Many tables are statics. |
| `models` | one of `models` or `keywords` | It matches if its model path contains any of these (case-insensitive). |
| `excludeModels` | no | It doesn't match if its model path contains any of these. |
| `keywords` | one of `models` or `keywords` | It matches if its base object has any of these keywords (editor IDs). |
| `excludeKeywords` | no | It doesn't match if its base object has any of these keywords. |
| `minMarkers`, `maxMarkers` | no | How many furniture markers (places to sit, lie or lean) it must have. Only furniture has markers. For example, `"minMarkers": 2` tells a double bed from a single one. |
| `useMarker` | no | Place scenes at its first furniture marker (default `true`). Otherwise, or when it has no markers, at the object's own origin and facing. |
| `ignoreMarkerAxes` | no | Axes of the marker's position to ignore (`"x"`, `"y"`, `"z"`), so the scene sits on the object's center line along that axis instead. For example, `["z"]` keeps the actors on the floor in front of a chair. |
| `offset` | no | `[x, y, z]` added to the spot, in the object's own frame: x to its right, y forward, z up. Scaled with the object. |
| `rotation` | no | Degrees added to the scene's facing. |

A type with neither `models` nor `keywords` matches nothing by itself; it only exists as a supertype for others.
