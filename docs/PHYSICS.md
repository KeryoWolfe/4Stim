# Physics swaps

Bodies with SMP physics (through [Fallout 4 FSMP](https://www.nexusmods.com/fallout4/)) get their physics from an XML file named in the body mesh. A physics swap tells 4Stim to swap that file for another one for the length of a scene, and back when the scene ends. For example, A-Body's floppy genital physics (`MaleBody.xml`) becomes its rigid version (`MaleBodyHard.xml`). The swap goes through FSMP's `DynamicHDT.SwapPhysicsFile`, the same call A-Body makes for AAF scenes, wrapped in 4Stim's `FourStimPhysics` script. FSMP must be installed (and its `DynamicHDT.psc` source is needed to compile `FourStimPhysics.psc`). Each swap's result is logged: `now uses` when it worked, `not swapped` when FSMP found no active physics using the `from` file on that actor (not loaded, or beyond `maxActiveActors` in FSMP's `configs.xml`).

Swaps are defined in JSON files in `Data\F4SE\Plugins\4Stim\Physics\`. 4Stim ships `A-Body.json`, for A-Body, the male body. (TWB, the female body, has no swap yet.)

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
| `toReceiving` | no | The file for an actor in any role but the first (in OStim's scenes the first role is the giving one). Defaults to `to`. |
| `sceneTags` | no | Only swap in scenes with one of these tags, e.g. `["sexual"]`. Default: every scene. |

Each actor gets at most one swap (the first that matches), when their scene starts. It stays for the whole scene, including scene changes, and is swapped back when it ends.
