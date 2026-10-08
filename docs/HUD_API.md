# Customizing the 4Stim HUD

The in-scene HUD is a Scaleform movie the 4Stim plugin talks to through a small,
fixed API. The plugin never draws anything itself, so anything on screen can be
changed or replaced. There are four ways to do it, from least to most work:

| What you want | What you make | Flash needed |
|---|---|---|
| Different colors, size, position, hidden parts | A **theme** JSON file | No |
| Your own logo animation or icons | A **logo movie** or image files | Logo only |
| Extra actions in the Utility tab | A **utility** JSON file + a Papyrus function | No |
| A completely different HUD | A **HUD movie** that implements this API | Yes |

Mods that only need to react to scenes (sound packs, dialogue, stats) don't need
the HUD at all: see [Scene events](#scene-events).

**API version: 1.** This number goes up whenever something in this document
changes in a way that can break an existing custom HUD.

---

## 1. Themes

Theme files live in:

```
Data\Interface\4Stim\Themes\YourTheme.json
```

The active theme is chosen in `Data\F4SE\Plugins\4Stim.ini`:

```ini
[HUD]
sTheme=YourTheme        ; file name without .json
```

The built-in `Color` and `PipBoy` themes are ordinary theme files in that
folder. Copy one and edit it. Any field you leave out keeps the `Color` theme's
value, so a theme only needs what it changes. This is the whole Pip-Boy theme:

```json
{
	"name": "Pip-Boy",
	"tint": "#1AFF80",
	"colors": {
		"disc": "#04110A",
		"tabBackground": "#000E07D9",
		"tabSelectedText": "#04110A",
		"navBackground": "#000E07E6",
		"navSelectedText": "#1AFF80",
		"iconTile": "#00000099",
		"meterTrack": "#1AFF802E"
	},
	"tintLogo": true
}
```

`Color.json` lists every field with its default value. After editing a theme,
`cgf "FourStim.ReloadScenes"` in the console reloads it without restarting.

| Field | Meaning |
|---|---|
| `tint` | Optional. One color for all text, accents, separators, meters and the logo. Colors the theme sets in `colors` still win. |
| `layout.anchor` | Screen corner the HUD sits in: `bottomLeft` (default) or `bottomRight` (the layout is mirrored). |
| `layout.offsetX/Y` | Pixels on a 1280×720 reference screen, moved in from the anchor corner. |
| `layout.scale` | Overall size. `1.0` is the default. |
| `layout.opacity` | `0.0` to `1.0` for the whole HUD. |
| `colors.*` | `"#RRGGBB"` or `"#RRGGBBAA"` (alpha last). Fields that are lists in `Color.json` take one color or several. Several make a gradient, in order. |
| `show.*` | `false` hides that part: `tabs`, `navigation`, `logo` (with its disc), `actorMeters`, `speedMeter`. |
| `logoMovie` | Path under `Data\Interface\` of the logo movie (see [Logo movie](#logo-movie)). |
| `tintLogo` | `true` recolors a logo movie that has no `setTheme` with the `accent` color. |
| `font` | One of the game's font names (`$MAIN_Font`, `$MAIN_Font_Bold`, `$HandwrittenFont`, `$Terminal_Font`, ...). **Themes can't add new fonts.** Fallout 4 loads fonts for every menu at once through `fonts_en.txt`, and changing that affects the whole game. |

The colors:

| Color | Used for |
|---|---|
| `text`, `textMuted` | Labels and names; headers and small labels. |
| `accent` | The selected row's bar, the selected tab's border, the speed number. |
| `separator` | The L-shaped lines and the header line. |
| `ring`, `disc` | The corner disc's ring and its inside. |
| `tabBackground`, `tabBorder` | Tab buttons. |
| `tabSelected`, `tabSelectedText` | The selected tab's fill and its icon and label. |
| `navBackground` | The list background (it fades out to the right and at the top). |
| `navSelected`, `navSelectedText` | The selected row's glow and its text. |
| `iconTile` | The square behind each row's icon. |
| `meterTrack` | The empty part of every meter. |
| `meterMale`, `meterFemale`, `meterOther` | Actor meters, by the actor's sex. |
| `speed` | Speed meter segments, slowest to fastest. |

---

## 2. Logo movie and icons

### Logo movie

The spinning logo in the corner is its own movie, `Data\Interface\4Stim\HUDLogo.swf`.
The HUD only loads it, scales it and places it. What it shows and how it moves
is up to the movie. To make your own:

- **Stage:** 256 × 256, with the art centered on the stage. The HUD scales it to
  fit inside the corner disc. Keep important art inside a 220 px circle.
- **Animation:** anything that runs by itself: a timeline, frame scripts or code.
  It loops for as long as the scene lasts.
- **Frame rate:** the HUD runs at 60 fps. A movie made at another rate will
  play faster or slower, so animate in code by elapsed time if exact speed matters.
- **Tools:** Adobe Animate, JPEXS FFDec or the Flex SDK all work. Target Flash
  Player 11 / SWF version 13 or lower, ActionScript 3.

The movie can define any of these functions on its root. Each one is optional.
The HUD calls only the ones that exist.

| Function | Called when |
|---|---|
| `setTheme(theme:Object):void` | On load and on theme change. `theme` is the parsed theme JSON. If missing and `tintLogo` is true, the HUD tints the movie with `accent`. |
| `onSceneChanged(sceneID:String, actorCount:int):void` | The scene starts or the player navigates. |
| `onSpeedChanged(level:int, count:int):void` | The speed changes. `level` is 0 for the slowest. |
| `setPaused(paused:Boolean):void` | A game menu pauses the game or closes. If missing, the HUD calls `stop()` / `play()` on the movie's root instead. |

To use your logo, either replace `HUDLogo.swf`, or ship it under your own name
and point a theme's `logoMovie` at it. Using your own name means two HUD mods
don't overwrite each other's files.

### Tab icons

`Data\Interface\4Stim\Icons\Tabs\Search.swf`, `Align.swf`, `Utility.swf`,
`Navigation.swf`: 64 × 64 stage, centered, drawn in white. They're movies too,
so they can animate. The HUD tints them with the theme's `text` color, or
`tabSelectedText` on the selected tab.

### Position icons

Scenes and navigation entries can name an icon in their scene JSON (see
`SCENES.md`):

```json
{ "id": "MyPack_Kneel", "name": "Kneel", "icon": "MyPack\\Kneel", ... }
```

```json
"navigations": [ { "to": "MyPack_Kneel", "label": "kneel", "icon": "MyPack\\KneelArrow" } ]
```

Icons are `.dds` images or `.swf` movies under `Data\Interface\4Stim\Icons\`,
loose or in a BA2. A name without an extension means `.dds`:
`"4Stim/positional/standup_f"` is `Icons\4Stim\positional\standup_f.dds`.
The HUD shows a `.dds` through F4SE's `MountImage` (its texture loader reads
paths relative to `Data\Textures\`, so the HUD passes `..\Interface\...`), and
any DDS format the game reads works (BC1/BC3/BC7 or uncompressed, with alpha); 128 × 128 is a good
size. A `.swf` icon (written out in full, `"MyPack/Kneel.swf"`) can be
animated. A navigation entry shows its own `icon` if it has one, otherwise the
destination scene's icon, otherwise an empty tile.

---

## 3. Utility tab entries

Any mod can add actions to the Utility tab with a JSON file in:

```
Data\F4SE\Plugins\4Stim\Utility\YourMod.json
```

```json
{
	"entries": [
		{
			"id": "YourMod_Undress",
			"label": "Undress {1}",
			"icon": "YourMod\\Undress.swf",
			"script": "YourModUtility",
			"function": "Undress",
			"actors": [2],
			"order": 100
		}
	]
}
```

| Field | Required | Meaning |
|---|---|---|
| `id` | yes | Unique. Prefix it with your mod name. |
| `label` | yes | Shown in the list. `{0}`, `{1}` ... are replaced by the actor names, as in navigation labels. |
| `icon` | no | Same as position icons. |
| `script`, `function` | yes | A **global** Papyrus function to call when the entry is picked. |
| `actors` | no | Only show the entry in scenes with these actor counts. Default: all. |
| `order` | no | Sort key. Lower first. Default 1000. Ties sort by label. |

The function must have exactly this signature:

```papyrus
Function Undress(Actor[] akActors, String asSceneID) Global
```

`akActors` holds the scene's actors in role order. The HUD stays open. The function
can end or change the scene through `FourStim`/`FourStimScene` if it needs to.

---

## 4. Writing a replacement HUD

### The movie

- File: `Data\Interface\FourStimHUDMenu.swf`. The menu name is `FourStimHUDMenu`.
- The plugin looks for the menu object at `root1.Menu_mc`. Your document class
  creates a `Menu_mc` child and puts every function below on it. You can
  also put them on the root itself; the plugin tries the root if `Menu_mc` is missing.
- `Menu_mc` must have a public `BGSCodeObj` created **in the constructor**:
  `public var BGSCodeObj:Object = new Object();`. The game fills it with the
  plugin's functions. If it's `null` or created later, nothing reaches the plugin.
- The HUD does not pause the game. It is shown for every scene the player is in,
  and for NPC scenes the player watches with the free camera.
- Coordinates: build for 1280 × 720. The movie is shown with "show all"
  scaling, so on wider screens there's extra room left and right of the
  stage; `SetScreen` tells you the whole visible area.

### Startup order

1. The game loads the movie. Your constructor runs.
2. The plugin attaches its functions to `BGSCodeObj`, calls `GetApiVersion()`,
   and sends everything: `SetScreen`, `SetTheme`, `SetScene`, `SetActors`,
   `SetSpeed`, `SetNavigation`, `SetUtility`, `SetFocus`. If the version doesn't
   match, it logs a warning in `4Stim.log` and carries on. Missing functions are
   skipped, with one warning each.
3. Call `BGSCodeObj.Ready()` once it exists: check `BGSCodeObj.Ready != null`
   on each `ENTER_FRAME` until it does. It doesn't exist yet in your
   constructor. The plugin answers by sending everything again.
4. After that, the plugin sends everything again whenever the scene changes
   (start, navigation, speed), and `SetFocus` / `SetPaused` / `SetScreen` when
   those change. Expect the same data more than once, and don't restart
   animations when nothing actually changed.

### Plugin → movie

Every function is on `Menu_mc`. All of them are required except where marked.

| Function | Meaning |
|---|---|
| `GetApiVersion():int` | Return the API version you were written for (`1`). |
| `SetScreen(left:Number, top:Number, right:Number, bottom:Number):void` | The visible area in your movie's coordinates. On a 16:9 screen it's 0, 0, 1280, 720; wider screens have a negative `left` and a `right` past 1280. |
| `SetTheme(theme:Object):void` | The parsed theme JSON, with defaults filled in. |
| `SetScene(scene:Object):void` | `{ id, name, actorCount, tags:Array }`. |
| `SetActors(actors:Array):void` | One object per role, in role order: `{ role:int, name:String, sex:"male"\|"female"\|"other", isPlayer:Boolean, meter:Number }`. `meter` is `0.0`–`1.0`, or `-1` when nothing fills the meters (always `-1` until the excitement system exists). |
| `SetSpeed(level:int, count:int):void` | `level` 0 = slowest, `count` = number of speeds (1 if the scene has only one). |
| `SetNavigation(entries:Array):void` | `{ id, label, icon, kind:"scene"\|"end" }` in display order. Labels already have actor names filled in. The `"end"` entry ("End scene") is always last. `icon` is a path under `Data\Interface\` ready for a `Loader` (e.g. `4Stim/Icons/MyPack/Kneel.dds`; mount a `.dds` with F4SE's `MountImage` and load it as `img://<name>`, see `hud\F4SE.as`), or empty. |
| `SetUtility(entries:Array):void` | `{ id, label, icon }`, already sorted and filtered for this scene. `icon` as above. |
| `SetFocus(focused:Boolean):void` | `true` while the HUD has the navigation keys. It gets them as soon as it opens; the 4Stim hotkey (or `Cancel`) gives them back to the game, and the hotkey takes them again. While not focused the HUD is display-only. |
| `SetPaused(paused:Boolean):void` | A game menu opened or closed over the HUD. |
| `ProcessUserEvent(name:String, down:Boolean):Boolean` | Input while focused: `down` is `true` on press and `false` on release. Return `true` if you used it. Holding `Up` or `Down` repeats the press, so you don't need your own repeat timer. The names and their keys are below. |
| `SetMeters(values:Array):void` | *Optional.* Faster update of just the actor meters, one `Number` per role, without resending names. |

Input names while the HUD is focused:

| Name | Keyboard | Controller |
|---|---|---|
| `Up`, `Down`, `Left`, `Right` | Arrow keys | D-pad |
| `Accept` | Enter | A |
| `Cancel` | Esc, Backspace | B |
| `PrevTab`, `NextTab` | (use `Left`/`Right`) | LB, RB |
| `SpeedUp`, `SpeedDown` | (the speed keys work directly) | Y, X |

### Movie → plugin (`BGSCodeObj`)

| Call | Effect |
|---|---|
| `Ready()` | The movie is set up. See startup order. |
| `Navigate(id:String)` | Move to the scene with this ID (from a `"scene"` navigation entry). |
| `EndScene()` | End the scene (the `"end"` entry). |
| `ChangeSpeed(delta:int)` | `+1` faster, `-1` slower. The plugin answers with `SetSpeed`. |
| `RunUtility(id:String)` | Run a Utility entry. |
| `OpenSearch()` | Open the scene picker, filtered to this scene's actors. |
| `ReleaseFocus()` | Give input back to the game (the plugin answers with `SetFocus(false)`). |
| `Log(message:String)` | Write a line to `4Stim.log`, tagged with your movie. |

**Planned, not in API version 1:** the Align tab (moving actors and the scene
spot), and the excitement and orgasm meters. A custom HUD can show these tabs
and parts, but the plugin won't send data for them yet. These will be added
without breaking version 1 HUDs.

### Rules a replacement HUD must follow

- **Never pause and never steal input while not focused.** The player has to be
  able to use the free camera.
- **Always offer a way to end the scene** while focused (`EndScene`), and make
  `Cancel` call `ReleaseFocus`. Otherwise players can get stuck.
- Don't keep scene data between scenes. The plugin sends everything again for
  each new scene.

---

## Scene events

Scripts can be told about every scene without touching the UI. A script on a
Quest or an ObjectReference registers with:

```papyrus
FourStim.RegisterForSceneEvents(self)
```

(A ReferenceAlias isn't a form: from an alias script, pass `GetOwningQuest()`
and put the functions on the quest's script.) The plugin then calls these
functions on every script attached to that form, if they have them:

```papyrus
Function FourStim_OnSceneStart(Actor[] akActors, String asSceneID)
Function FourStim_OnSceneChange(Actor[] akActors, String asOldID, String asNewID)
Function FourStim_OnSpeedChange(Actor[] akActors, String asSceneID, int aiLevel, int aiCount)
Function FourStim_OnSceneEnd(Actor[] akActors, String asSceneID)
```

`akActors` is in role order, and `aiLevel` is 0 for the slowest speed. Start
and end fire for every scene started through 4Stim; change and speed fire for
the focused scene (the one the player is in or watching), since that's the only
one they can happen to. Calls run asynchronously. A slow handler doesn't hold up the scene, but the scene
may already have moved on by the time your handler runs.

Registrations aren't saved in version 1, so register again on every game load
(`OnPlayerLoadGame` on a player alias). Saving them with the game is planned
along with the scene claim registry.
