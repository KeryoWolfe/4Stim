# Sound

Actors in a scene moan and make sounds when they climax, as in OStim, from **voice sets**. `bSounds=0` in `4Stim.ini` turns them off.

## Voice sets

Voice sets are in `Data\F4SE\Plugins\4Stim\Voice Sets\`, one per file, in OStim's format (OStim: `SKSE\Plugins\OStim\voice sets`):

```json
{
  "name": "Default (female)",
  "target": { "mod": "Fallout4.esm", "formid": "0x000001" },
  "moan": {
    "sound": [ { "file": "fx/4Stim/actor/moan/female", "expression": "moan" } ],
    "soundMuffled": [
      { "actions": ["blowjob", "deepthroating"], "file": "fx/4Stim/effects/suck", "moanIntervalOverride": 1.0 }
    ]
  },
  "climax": {
    "sound": [ { "file": "fx/4Stim/actor/climax/female", "expression": "climax" } ]
  }
}
```

- `target`: who uses it: an NPC or a voice type, by plugin and form ID. As in OStim, form ID `0x000000` is the default set for men and `0x000001` for women. `aliases` can list more NPCs or voice types.
- An actor gets their NPC's set, else their voice type's, else the default for their sex.
- `moan` / `climax`: what plays. `sound` normally, `soundMuffled` while the actor's mouth is busy (an action that has them `muffled`, such as oral). One entry is picked at random among those that fit.

Each entry is one of:

- **4Stim: `"file"`**, a sound file (`.wav` or `.xwm`) or a folder of them under `Data\Sound\`, one picked at random. No plugin is needed. Folders are read from loose files: sounds packed in a `.ba2` have to be listed one by one.
- **OStim's `"sound"`**, a sound descriptor form (`{ "mod": "MyMod.esp", "formid": "0x000800" }`), played with Papyrus `Sound.Play`.

And can have:

- `expression`: the event expression shown while it plays (docs/EXPRESSIONS.md).
- `moanIntervalOverride`: seconds to the next moan, in place of `moanIntervalMin` / `Max`.
- **4Stim: `actions`**, in place of OStim's perk `condition`: only while the actor takes part in one of these actions (OStim uses a perk to tell blowjob sounds apart).

## When

As in OStim:

- **Moans:** an actor moans if one of their actions says so (`moan` in `Actions\Default.json`), every `moanIntervalMin` to `moanIntervalMax` milliseconds (2500 / 4000), muffled if one says `muffled`.
- **Climax:** at a climax, its climax sound, with the climax expression.

`moanVolume` (0-1) sets how loud. Sounds follow the actor.

Sound files are played with Fallout 4's audio usage flags `iSoundFlags` (`0x1A`) and priority `iSoundPriority` (128). If no sound plays, `4Stim.log` says `Sound: couldn't load ...` for files the game couldn't open.

## Action sounds

Actions play sounds of their own, as OStim's: the slaps and strokes of sex. In an action file (docs/ACTIONS.md), `sounds` lists them, in OStim's format:

```json
"sounds": [ { "type": "peak", "file": "fx/4Stim/effects/thrust" } ]
```

- `type`: `peak` plays on each of the action's peaks (docs/EVENTS.md); `bonedistance` plays as the `actorBone` and `targetBone` move apart after being closest (`inverse`: after being furthest apart), once they've moved at least `minRange` units (4Stim, default 3), at least `minInterval` ms after the last (50 by default) and, with `maxInterval`, no more than that; `loop` plays one after another, `delay` ms apart.
- `sound` (OStim's descriptor form) or `file` (4Stim: a sound file or folder under `Data\Sound\`).
- `playerThreadOnly`: only in scenes with you. `muteWithActor` / `muteWithTarget`: none while the action's actor / target is muted.

They play on the action's actor at `soundVolume` (0-1, OStim's setting). A scene's action with `"muted": true` plays none. 4Stim's default actions play `effects\thrust` (slaps) on the peaks of vaginal and anal sex, and `effects\HJ` (strokes) on handjobs, footjobs and male masturbation. OStim uses one sound for all but handjobs; 4Stim's slap files don't suit strokes.

## Event reactions

A voice set can react to events (docs/EVENTS.md), as OStim's: `eventActorReactions`, `eventTargetReactions` and `eventPerformerReactions` map an event id to a reaction set like `moan`'s (`sound`, `soundMuffled`, with an `expression`). The reaction plays after the event's `reactionDelay`, cutting in over a moan; an event with no reaction of its own uses its supertype's. The default sets have none, as OStim's.

```json
"eventTargetReactions": {
	"spank": { "sound": [ { "file": "fx/4Stim/actor/moan/female", "expression": "moan" } ] }
}
```

## The default sets

4Stim's default sets use the OStim sounds in `4Stim Core\Sound\fx\4Stim\`: `actor\moan\female|male`, `actor\climax\female|male`, and `effects\suck` for muffled moans during oral.

## Not in yet

Dialogue (comments, climax comments, post-scene lines), `talk`.

## Muting

An actor can be muted for the rest of their scene: no moans, climax or event sounds (OStim's `OActor.Mute`). With Papyrus `FourStim.Mute` / `Unmute` / `IsMuted`, or the HUD's options (each actor's page, OPTIONS.md). An actor property's `mute` (a gag, ACTORS.md) mutes them too.
