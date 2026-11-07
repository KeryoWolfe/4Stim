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
- An actor gets, as in OStim: the set chosen for them (below), else their NPC's, else their voice type's (an NPC without one has their race's default voice for their sex), else their race's, else the default for their sex. The defaults are for humans only (an adult with the `ActorTypeNPC` keyword), so a creature with no set of its own makes no sound, as in OStim. `4Stim.log` says which set each actor got and why.
- `climaxCommentSelf` / `climaxCommentOther`: OStim's climax comments, the sound part (`sound`, `soundMuffled`). An actor's `climaxCommentOther` plays 0.75 s after a partner climaxes, their `climaxCommentSelf` 1 s after their own climax sound (not for the player, as OStim). Each waits for the sound before it to end.
- `moan` / `climax`: what plays. `sound` normally, `soundMuffled` while the actor's mouth is busy (an action that has them `muffled`, such as oral). As in OStim, the **first** entry that fits plays: list specific entries first and a catch-all last. Variety comes from inside an entry (a folder of files, or a sound descriptor's own).

Each entry is one of:

- **4Stim: `"file"`**, a sound file (`.wav` or `.xwm`) or a folder of them under `Data\Sound\`, one picked at random. No plugin is needed. Folders are read from loose files: sounds packed in a `.ba2` have to be listed one by one.
- **OStim's `"sound"`**, a sound descriptor form (`{ "mod": "MyMod.esp", "formid": "0x000800" }`), played as OStim plays it (`BSAudioManager::GetSoundHandle`, following the actor): at `moanVolume` / `soundVolume`, stopped by muting, a climax or the scene's end, and its real length times the moans. It plays through the descriptor's own output model and category, with the usage flags `iSoundFlags`. On a game version where 4Stim can't find `GetSoundHandle` (OLDGEN.md), it falls back to Papyrus `Sound.Play`: no volume, can't be stopped, and counted as 2 seconds long.

And can have:

- `expression`: the event expression shown while it plays (docs/EXPRESSIONS.md).
- `moanIntervalOverride`: seconds to the next moan, in place of `moanIntervalMin` / `Max`.
- `condition` (OStim's): a perk (`{ "mod", "formid" }`) whose conditions must hold, with the actor as the subject and their primary partner as the target (the other side of the first action they're on; themselves if none). A perk that isn't loaded is logged and the entry plays without it, as OStim's.
- **4Stim: `actions`**: only while the actor takes part in one of these actions. No plugin needed; 4Stim's default sets use it for the sucking sounds, where OStim's use a perk.

## When

As in OStim:

- **Moans:** an actor moans if one of their actions says so (`moan` in `Actions\Default.json`), every `moanIntervalMin` to `moanIntervalMax` milliseconds (2500 / 4000), muffled if one says `muffled`.
- **Climax:** at a climax, its climax sound, with the climax expression.

`moanVolume` (0-1) sets how loud. Sounds follow the actor.

Sound files are played with Fallout 4's audio usage flags `iSoundFlags` (`0x1A`) and priority `iSoundPriority` (128). They play on the actor, through one of the game's sound output models that fade with distance (picked once, the one heard out to about 2000 units; `4Stim.log` names it), so an NPC scene is heard where it is, not wherever the player is. If no sound plays, `4Stim.log` says `Sound: couldn't load ...` for files the game couldn't open.

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

A voice set can react to events (docs/EVENTS.md), as OStim's: `eventActorReactions`, `eventTargetReactions` and `eventPerformerReactions` map an event id to a reaction set like `moan`'s (`sound`, `soundMuffled`, with an `expression`). The reaction's `reactionDelay` counts down once the actor's current moan has ended, then it plays, as OStim's; an event with no reaction of its own uses its supertype's. The default sets have none, as OStim's.

```json
"eventTargetReactions": {
	"spank": { "sound": [ { "file": "fx/4Stim/actor/moan/female", "expression": "moan" } ] }
}
```

## The default sets

4Stim's default sets use the OStim sounds in `4Stim Core\Sound\fx\4Stim\`: `actor\moan\female|male`, `actor\climax\female|male`, and `effects\suck` for muffled moans during oral.

## Not in yet

Dialogue (comments, the dialogue part of climax comments, post-scene lines), `talk`.

## Choosing a voice set

As in OStim, any voice set can be chosen for an actor, and the choice is saved with the game, per actor base (all copies of a generic NPC share it). In the HUD's Utility tab, each actor's page has a **Voice** page: "Default" (no choice: the order above), then every voice set but the two defaults, by name; the one in use has a `*`. Actors in a scene take a new choice at once. **Mute** (`mute.json`, as OStim's) makes an actor silent.

OStim puts this in its MCM ("Player voice" on the Sound page, "Actor voice" on the Actors page). Fallout 4's MCM can't list choices that depend on what's installed, so 4Stim's is in the HUD's options, available during a scene (the player's too). Papyrus: OStim's `OData` functions on `FourStim`: `GetVoiceSetPairs()` ("0", "Default", then each set's id and name), `GetVoiceSetName(aiFormID)`, `SetVoiceSet(aiFormID, asVoice)`, and by actor `SetActorVoiceSet(akActor, asVoice)` / `GetActorVoiceSetName(akActor)`. A set's id is its target's form ID as a number, as OStim's; a set without a target (4Stim's mute) is chosen by its file name.

## Muting

An actor can be muted for the rest of their scene: no moans, climax or event sounds (OStim's `OActor.Mute`). A moan cut off by muting takes its face with it. With Papyrus `FourStim.Mute` / `Unmute` / `IsMuted`, or the HUD's options (each actor's page, OPTIONS.md). An actor property's `mute` (a gag, ACTORS.md) mutes them too.
