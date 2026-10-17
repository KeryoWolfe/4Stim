# Furniture types

A scene with `"furniture": "<type>"` (see `SCENES.md`) is played on a piece of furniture of that type. Furniture types say which objects in the world count as that type, and where on them a scene goes. They're defined in JSON files in `Data\F4SE\Plugins\4Stim\Furniture\`. 4Stim ships `Fallout4.json` for the base game's objects; mods can add their own files. Files are read in name order, and a type with the same `id` as an earlier one replaces it. They're re-read every time the picker opens, so edits apply without restarting the game.

## How furniture is found

When the picker opens for a new scene, 4Stim looks at the objects within `SetFurnitureSearchDistance` of the player (OStim's steps: (value + 1) x 100 units, so the default 15 is 1600 units) (and `fFurnitureHeight` up or down, so the floor above doesn't count), in `4Stim.ini`. It gives each object its most specific matching type (highest `priority`) and keeps the nearest piece of each type (every piece, for types with `listIndividually`), and for a type anchored on `"wall"`, the nearest wall (below). Furniture another running scene is on is left out. A scene for type T is listed if there's a piece of type T or of a subtype of T (a type whose `supertype` chain leads to T). For example, scenes for `chair` can be played on a `bench`, since a bench is a kind of chair. When one is picked, the actors are moved onto the nearest piece it fits.

As in OStim, two settings in `4Stim.ini` decide whether you're asked: with `SetSelectFurniture=1` (the default) the picker's "Where?" step lists "Right here" and each piece of furniture near that a scene fits the cast on; with `SetSelectFurniture=0` there's no question, and the scene goes on the nearest of those pieces, or right here if there's none (OStim's `findFurniture`). `SetEnableFurniture=0` leaves furniture out altogether. Without the "Where?" step, "Browse all scenes" is only in a running scene (the search key).

OStim's furniture type files work too: one type per file in `Data\F4SE\Plugins\4Stim\Furniture Types\` (subfolders too), the file name being the id, as OStim's `furniture types\` folder. They're read after the files in `Furniture\`, so they replace a type with the same id. OStim's fields are read: `name`, `supertype`, `priority`, `listIndividually` (list every piece of this type in the picker, not just the nearest), `offsetX` / `offsetY` / `offsetZ`, `rotation`, `multiplyScale` (actors on it are scaled by this, as OStim's), `ignoreMarkerOffsetX` / `Y` / `Z`, and the `anykeyword` / `keywordblacklist` conditions (keywords as `{"mod", "formid"}`). OStim's other conditions are about Skyrim's furniture markers and are skipped with a warning, so a type from OStim also needs 4Stim's `models` or `keywords` to recognize anything. All of 4Stim's fields below work in these files too, and OStim's in 4Stim's.

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
| `forms` | no | Base object types it can be: `FURN` (furniture, the default), `STAT` (static), `MSTT` (movable static), `ACTI` (activator), `CONT` (container: shelves with storage). Many tables are statics. |
| `models` | one of `models` or `keywords` | It matches if its model path contains any of these (case-insensitive). |
| `excludeModels` | no | It doesn't match if its model path contains any of these. |
| `keywords` | one of `models` or `keywords` | It matches if its base object has any of these keywords (editor IDs). |
| `excludeKeywords` | no | It doesn't match if its base object has any of these keywords. |
| `minMarkers`, `maxMarkers` | no | How many furniture markers (places to sit, lie or lean) it must have. Only furniture has markers. For example, `"minMarkers": 2` tells a double bed from a single one. |
| `anchor` | no | Where scenes go: `"marker"` (the default) at one of its furniture markers, read from its loaded model; `"origin"` at the object's own origin and facing; `"edge"` on an edge of its bounding box (below). Without markers, `"marker"` falls back to the origin. Everything found (markers, bounds, the spot) is logged whenever a furniture scene is placed. Or `"center"`: in the middle of its bounding box, on top of it, turned lengthwise (toward the end farther from the player), for lying on a mattress or a sleeping bag. Or `"wall"`: no object at all, the nearest wall (below). |
| `wallDistance` | no | `"wall"`: how far from you a wall may be (default 300 units). |
| `marker` | no | Which marker to use, counting from 0 (default 0). |
| `edgeSide` | no | `"edge"`: a `"long"` side (default) or a `"short"` one. Of the two, the one nearer the player is used. |
| `facing` | no | `"edge"`: the scene faces `"out"` from the object (default; sitting on a bed's edge) or `"in"` toward it (standing at a table). |
| `edgeInset` | no | `"edge"`: how far in from the edge the spot is (default 0); negative is outside the object. |
| `checkWalls` | no | `"edge"`: before taking the side nearer the player, check there is room outside it (line-of-sight rays 70 units out at 40, 100 and 150 above its floor and just over its top, looking past the furniture itself: walls, other furniture and clutter block them, actors don't; the least room counts). If that side is blocked and the other is freer, the other side is used. Default `true`. |
| `onFloor` | no | `"edge"` or `"center"`: put the spot at the bottom of its bounds, where it stands (tables, benches), whatever height its origin is at. |
| `markerHeight` | no | `"edge"` or `"center"`: take the height from the marker (when the model has one) instead of `offset`'s z, e.g. a bed's mattress. |
| `ignoreMarkerAxes` | no | `"marker"`: axes of the marker's position to ignore (`"x"`, `"y"`, `"z"`), so the scene sits on the object's center line along that axis instead. For example, `["z"]` keeps the actors on the floor in front of a chair. |
| `offset` | no | `[x, y, z]` added to the spot, in the object's own frame: x to its right, y forward, z up. Scaled with the object. |
| `floorScenes` | no | For a type whose chain reaches `"none"` (it also takes scenes made without furniture): which of those scenes it takes, by their actors' `tags` (SCENES.md). `needTags`: at least one actor has one of these. `excludeTags`: no actor has any of these. Scenes made for this furniture type are not affected. |
| `rotation` | no | Degrees added to the scene's facing. |

A type with neither `models` nor `keywords` matches nothing by itself; it only exists as a supertype for others (unless it's anchored on a wall).

## Walls

OStim's `wall` type is Skyrim's wall-lean markers, which Fallout 4 has hardly any of, so 4Stim finds walls instead (a Fallout 4 departure): a type with `"anchor": "wall"` is the nearest wall around you, within `wallDistance`. Rays go out in 24 directions at two heights (50 and 110 units up); where both hit the same upright surface (not a door, not an actor), that's a wall, and two more rays either side give its direction. The nearest wall that's free in front is taken: rays at five heights (15 to 170), straight and 20 to each side, must all reach it (so an open shelf, whose gaps let a ray or two through to the wall behind, doesn't count), and the spot can't be in or within 30 of any furniture near. The scene's spot is at the foot of the wall, facing out from it, moved by the type's `offset` (y: out from the wall) and `rotation`. Tall furniture (a big shelf, a wardrobe) counts as a wall too. With `bLogFurniture=1` the wall found is logged. A wall scene in a save starts again at the same spot.

## Changing furniture mid-scene

As OStim's furniture selection during a scene: the HUD's Utility tab has **Change furniture...** (and the picker has it among a scene's options when the HUD is off). It lists "None" (stay) and the furniture near the scene that any scene fits its actors on, nearest first; picking one moves the scene there, with a random scene tagged `intro` (`SetUseIntroScenes`) or `idle` that fits (on a bed one with no one standing, as OStim), falling back to any scene that fits. With you in it, it happens behind a fade (`SetUseFades`) and the free camera starts again where you are. A sequence playing stops. As in OStim, this doesn't take a scene off furniture back onto the floor ("None" just stays).

In OStim the start key does this during a scene; in 4Stim that key opens the HUD's controls, so it's a Utility entry (a Fallout 4 departure). Scripts: `FourStim.ChangeFurniture` and `FourStim.OpenFurnitureMenu` (below).

## Furniture in use, and clutter

While a scene is on a piece of furniture, no one else can use it: its activation is blocked and its prompt hidden (OStim's `disableUse`), until the scene leaves it. Furniture that something else had blocked already is left as it was. OStim also makes the furniture private (owned by an empty faction from OStim.esp); 4Stim has no plugin and doesn't change owners.

With `SetResetClutter=1` (default), the loose items within `SetResetClutterRadius` x 100 units of the spot (OStim's clutter: misc items, weapons, armor, books, notes, food and chems, keys, ammo, movable statics) are noted before the actors are moved onto the furniture, and when the scene leaves it (it ends, or moves to other furniture), the ones that moved or turned are put back where they lay. Departure from OStim: OStim's clutter reset sends every item in range back to its editor location, and that code is switched off in OStim NG; 4Stim only puts back what the scene moved, so things you put down yourself stay. Both survive a save: the furniture is unblocked and the clutter put back on load (or kept for a scene that starts again).

## Papyrus

In `FourStim.psc` (OStim's `OFurniture` and `OThread` furniture functions):

- `GetFurnitureType(ObjectReference)`, `IsFurnitureChildOf(String asSuperType, String asSubType)`
- `GetSceneFurniture(Actor)` (None off furniture or at a wall) and `GetSceneFurnitureType(Actor)`
- `ChangeFurniture(Actor, ObjectReference, String asSceneID = "")`, `OpenFurnitureMenu()`

Scripts registered for scene events get `FourStim_OnFurnitureChange(Actor[] akActors, ObjectReference akFurniture, String asType, String asSceneID)` after a change (OStim's `ostim_thread_furniturechanged`; akFurniture is None for a wall).

## Floor scenes on beds

A scene without furniture of its own can play on furniture whose type chain reaches `none` (beds, as in OStim). On a type anchored on an edge (`"anchor": "edge"`, beds), such a floor scene doesn't use the edge spot: it lies along the middle of the bed, at the edge spot's height (the type's `offset` z, or the marker's height), heading toward the end farther from the player. A scene that moves between bed scenes and floor scenes switches between the two spots.

## How the shipped types place scenes

The OStim animations in the test pack share one origin per kind of furniture, worked out from the clips themselves:

- **Bed:** on the mattress at a long edge, facing out: in sitting scenes the pelvis is at the origin and the feet hang about 25 units forward and 33 down. So `bed` is anchored on the edge nearer the player, 15 in, at the mattress's height (the sleep marker's when the model has one, else 32 up). 
- **Mattress:** mattresses and sleeping bags lie on the ground, so bed scenes (sitting on the edge, standing beside it) don't work on them. `mattress` is its own type with supertype `none`: it takes every scene made without furniture (lying, kneeling, standing...), centered on the mattress, lengthwise, 2 up. Like OStim's bedrolls in Skyrim: floor scenes, not the scenes for sitting on a bed's edge. (To limit a type to some of those scenes, see `floorScenes`.)
- **Bench:** on the floor under the seat, facing out: the pelvis is 38 above the origin. So `bench` is anchored 18 in from the long edge nearer the player, on the floor.
- **Table:** on the floor at the table's edge, facing the table: one actor bends over it in front of the origin, the other stands 50 behind. So `table` is anchored 5 outside the long edge nearer the player, facing in.
- **Shelf:** models with "shelf", "shelves" or "bookcase" (statics, containers...): on the floor 15 in front of the long side nearer you, backs to the shelf. A first guess: tune in game.
- **Wall:** the nearest wall within 300, the spot 15 out from it, backs to it. A first guess too.

A scene whose own animation starts somewhere else can shift itself with `furnitureOffset` (see `SCENES.md`).
