Equip objects: armor items 4Stim puts on actors during scenes (strap-ons,
tongues...) and takes away when the scene ends. One .json file per object,
in OStim's format; see docs/ACTORS.md in the 4Stim repository.

{
	"type": "strapon",
	"name": "My strap-on",
	"default": true,
	"mod": "MyStrapOns.esp",
	"formid": "0x800",
	"variants": {
		"wet": { "mod": "MyStrapOns.esp", "formid": "0x801" }
	}
}

4Stim comes with none: Fallout 4 has no strap-on or tongue of its own. Make
the armor non-playable and use it for nothing else.
