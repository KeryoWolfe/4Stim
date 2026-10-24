# Physics swaps

Bodies with SMP physics (through [Fallout 4 FSMP](https://www.nexusmods.com/fallout4/)) get their physics from an XML file named in the body mesh. A physics swap tells 4Stim to swap that file for another one for the length of a scene, and back when the scene ends. For example, A-Body's floppy genital physics (`MaleBody.xml`) becomes its rigid version (`MaleBodyHard.xml`). The swap goes through FSMP's `DynamicHDT.SwapPhysicsFile`, the same call A-Body makes for AAF scenes, wrapped in 4Stim's `FourStimPhysics` script. FSMP must be installed (and its `DynamicHDT.psc` source is needed to compile `FourStimPhysics.psc`). Each swap's result is logged: `now uses` when it worked, `not swapped` when FSMP found no active physics using the `from` file on that actor (not loaded, or beyond `maxActiveActors` in FSMP's `configs.xml`).

Swaps are defined in JSON files in `Data\F4SE\Plugins\4Stim\Physics\`. 4Stim ships `A-Body.json`, for A-Body, a male body. Other bodies (male: BodyTalk, Super Hero Bodies, Muscular Body, Atomic Muscle; female: TWB, CBBE, JaneBod, Fusion Girl, Atomic Beauty; both: Enhanced Vanilla Bodies) need their own swap files, written the same way, if their SMP setup has states to swap between.

```json
{
	"swaps": [
		{
			"id": "A-Body",
			"sex": "male",
			"from": "meshes\\A-Body\\body\\MaleBody.xml",
			"to": "meshes\\A-Body\\body\\MaleBodyHard.xml",
			"toReceiving": "meshes\\A-Body\\body\\MaleBodyHardBottom.xml"
		}
	]
}
```

| Field | Required | Meaning |
|---|---|---|
| `id` | no | A name for the log. |
| `sex` | no | Whose body: `"male"`, `"female"` or `"any"` (default). |
| `from` | yes | The physics file the body normally uses, as its mesh names it. |
| `to` | yes | The file to use during a scene. |
| `toReceiving` | no | The file for an actor on the receiving end: the target of someone's action that needs their anus or vagina (anal or vaginal sex, pegging, fingering, rimming...). In a scene without such an action, every role but the first (OStim's scenes put the giving one first). Defaults to `to`. |
| `sceneTags` | no | Only swap in scenes with one of these tags, e.g. `["sexual"]`. Default: every scene. |
| `toSuffix` | instead of `to` | Suffix syntax: the actor's own physics file with this before `.xml` (`"Hard"`: `MaleBody.xml` becomes `MaleBodyHard.xml`). `from` is then optional. |
| `toReceivingSuffix` | no | The same for the receiving end (`"HardBottom"`). Also works with a full-path `from`. |

### Suffix syntax

Instead of naming every file, a swap can give suffixes. 4Stim then looks at the physics files the actor's meshes name (FSMP's `HDT Skinned Mesh Physics Object`), takes the first one that has a suffixed version (loose or in an archive), and swaps it for that, so one entry covers a body's variants and any body that names its files the same way:

```json
{
	"swaps": [
		{
			"id": "Hard physics, any body",
			"sex": "male",
			"toSuffix": "Hard",
			"toReceivingSuffix": "HardBottom"
		}
	]
}
```

With a `from`, only that file is swapped (`"from": "meshes\\A-Body\\body\\MaleBody.xml", "toSuffix": "Hard"`). A file that already ends with the suffix is left alone, and an actor with no file that has a suffixed version gets the next swap that fits (the log says which files it had). `toReceivingSuffix` falls back to `toSuffix`'s file when the body has no receiving version.

Each actor gets at most one swap (the first that matches; a suffix swap only matches if the actor has a file with that suffixed version), 1 second after their scene starts. It's swapped back 2 seconds after the scene ends, once they're out of its last pose, and a second later FSMP rebuilds their physics from the reference pose (`DynamicHDT.ResetPhysics`; everyone in the scene gets that rebuild, swapped or not).

FSMP does every swap and rebuild on the game's main thread, and the game waits for it, so several at once were long freezes, and two crash logs had the game crash inside one. 4Stim spreads them out: a scene's swaps wait a second (out of the scene's busiest frames, where undressing, lights and the camera all start) and go one actor a second; after a scene, the swaps back and rebuilds go one actor at a time, a second or more apart. Each swap is one `SwapPhysicsFile` call once a spelling of the file names has worked (4Stim tries up to three spellings the first time), with FSMP's verbose logging off. Swapping back while the body was still in the scene's pose could leave a penis stretched and stuck at an angle. A new scene within those 2 seconds keeps the swap. On a scene change, an actor whose swap has a `toReceiving` switches between it and `to` if they went from giving to receiving or back (a man who was on top, now bottoming).
