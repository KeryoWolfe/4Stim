Add-on settings, in OStim's format (its SKSE\Plugins\OStim\settings folder):
an add-on that doesn't want an MCM page of its own puts its settings here,
one .json file per add-on, each kept in one of its global variables. 4Stim
shows them on its MCM page's "Add-Ons" page; see docs/MCM.md in the 4Stim
repository.

{
	"name": "My Add-on",
	"settings": [
		{
			"name": "Enable the thing",
			"tooltip": "Turns the thing on.",
			"type": "toggle",
			"defaultValue": true,
			"variable": { "mod": "MyAddon.esp", "formid": "0x800" }
		},
		{
			"name": "How much",
			"type": "slider",
			"minValue": 0, "maxValue": 10, "valueStep": 0.5, "defaultValue": 5,
			"variable": { "mod": "MyAddon.esp", "formid": "0x801" }
		},
		{
			"name": "Which one",
			"type": "dropdown",
			"options": ["First", "Second"], "defaultIndex": 0,
			"variable": { "mod": "MyAddon.esp", "formid": "0x802" }
		},
		{
			"name": "A key",
			"type": "keymap",
			"defaultKey": 0,
			"variable": { "mod": "MyAddon.esp", "formid": "0x803" }
		}
	]
}

Toggles set the global to 0 or 1, dropdowns to the index picked, keymaps to
the key's code (Fallout 4's, as the MCM gives it). The page is written when
the game starts, so a new or changed file shows after a restart.
