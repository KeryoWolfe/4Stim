# Parity audit 4: sound, voice sets, sex toys, lights, climax-to-sound/toy links

Scope: OStim `skse/src/Sound/**`, `skse/src/SexToys/**`, `Core/ThreadActor/ThreadActorSound.cpp`, `ThreadActorClimax.cpp`, the sound bits of `Core/Thread.cpp`, `data/SKSE/Plugins/OStim/voice sets*`, light equip objects and `options/facelight.json`, MCM sound page (`OSexIntegrationMCM.psc` `DrawSoundPage`). 4Stim `src/Sounds.cpp`, `src/Peaks.cpp` (action-sound players), `src/SexToys.cpp`, `src/Lights.cpp`, `src/Excitement.cpp`, `src/main.cpp` (ClimaxNow, settings), `include/4Stim/PluginInterface.h`, `Data/F4SE/Plugins/4Stim/{Voice Sets,Lights}`, `docs/SOUND.md`, `LIGHTS.md`, `API.md`, `EXCITEMENT.md`.

OStim paths are relative to `/tmp/ostim/OStimNG-main/`, 4Stim paths to `/home/claude/4stim/`.

---

## 1. Voice-set choice is a cut-down version of OStim's: no user choice, no race, no race default voice, creatures get human sounds

- **Class**: Missing / Different
- **Severity**: High (players can't pick a voice for themselves or an NPC as in OStim; addon voice sets that target a race or rely on race default voices never get used)
- **OStim**: `SoundTable::getVoiceSet(GameActor)` `skse/src/Sound/SoundTable.cpp:104-150` checks, in order:
  1. the **user's choice**, saved in the cosave per actor base (`Serialization::getVoiceSet`, `Serial/Manager.cpp:43-58`, saved at 322, loaded at 386-410). It is set from the MCM: the Sound page's "Player voice" (`OSexIntegrationMCM.psc:2515`, base 0x7) and the Actors page's "Actor voice" (`:2864`, `OpenVoiceSetMenu` / `SetVoiceSet` / `SetVoiceSetToDefault` `:3216-3229`), through `OData.GetVoiceSetPairs` / `GetVoiceSetName` / `SetVoiceSet` (`Papyrus/PapyrusData.h:57-67`, `SoundTable.cpp:153-202`). The list includes OStim's `mute.json` set, which targets the voice type `OStimVoiceMute` (OStim.esp 0xEED), so choosing it makes an actor silent.
  2. the actor base;
  3. the voice type from `GameActor::getVoice()`, which falls back to the **race's default voice type for the actor's sex** when the NPC has none (`GameAPI/GameActor.cpp:124-131`);
  4. the **race** (`SoundTable.cpp:133`);
  5. the default male/female set, but **only for humans** (`isHuman`: not a child, has the NPC keyword, not the mannequin race, `GameActor.cpp:120-122`). Non-humans get an empty set, so creatures make no sound (`SoundTable.cpp:139-149`).
- **4Stim**: `VoiceFor` `src/Sounds.cpp:258-279` checks the NPC, then `npc->voiceType` (no race-default fallback), then the default by sex for everyone, creatures included. There is no race lookup, no saved per-actor choice, no voice-set list or name API, no MCM or HUD control for any of it, and no mute set. `docs/SOUND.md:26` describes the shorter chain as if it were OStim's. Grepped `VoiceSet`, `voice set`, `GetVoiceSetPairs` in src/, Data/Scripts, Data/MCM: only `Sounds.cpp`.
- **Fallout 4 constraint?** None. `TESRace::defaultVoiceType[2]` exists (`lib/commonlibf4/include/RE/T/TESRace.h:96`), the player base is 0x7 here too, and F4SE has a cosave. The choice could go in the HUD options per actor (as mute already does) and on an MCM page for the player.
- **Suggested fix**: add the race step and the race-default-voice fallback; return no set for non-humans (ActorTypeNPC keyword, not a child). Save a per-base voice-set choice in the cosave and expose `GetVoiceSetPairs` / `GetVoiceSetName` / `SetVoiceSet` on `FourStim`. Add "Player voice" to the MCM, "Voice" to each actor's HUD options page, and ship a `mute.json`. Its OStim target is a voice type; a 4Stim-only `"none": true` set would avoid needing a plugin.

## 2. Picking a sound entry: 4Stim picks at random, OStim takes the first that fits; OStim's `condition` is silently ignored

- **Class**: Different (not documented as a departure)
- **Severity**: High for voice-set authors: an OStim-style list (specific entries first, a catch-all last) behaves differently.
- **OStim**: `getSoundSet` / `getDialogueSet` (`skse/src/Sound/SoundUtil.cpp`, shown at `SoundTable.cpp:370-388` in the cat) return the **first** entry whose perk `condition` is met (or that has none). Randomness only comes from inside the sound descriptor. Authors rely on order: conditional entries first, then an unconditional fallback. The condition is evaluated with the actor as subject and their primary partner as target (`ThreadActorSound.cpp:134,162-164`).
- **4Stim**: `PlayReaction` `src/Sounds.cpp:290-308` collects every entry that fits and picks one **uniformly at random** (line 301). `docs/SOUND.md:27` says so but doesn't flag it as a departure. With an OStim-ordered list such as `[{actions:[blowjob], file:suck}, {file:moan}]`, the fallback plays about half the time during a blowjob. Any OStim `"condition"` field is dropped without a warning (`ReadEntries` `Sounds.cpp:148-178` never reads it), so a conditional entry becomes unconditional, which is the opposite of what the author meant.
- **Fallout 4 constraint?** None for the order. For conditions, FO4 perks carry `perkConditions` (`lib/commonlibf4/include/RE/B/BGSPerk.h:29`) and `TESCondition::IsTrue(actionRef, targetRef)` exists (`RE/T/TESCondition.h:19`), so OStim's perk-condition mechanism can be reproduced. The documented reason for `actions` ("OStim uses a perk", SOUND.md:38) isn't a Fallout 4 limit. A plugin is needed only for 4Stim's own default conditions, and `actions` can stay as an extra.
- **Suggested fix**: take the first entry that fits, as OStim does, and keep folder/file randomness inside an entry. Support `"condition"` (perk form → `perkConditions.IsTrue(actor, partner)`), and log a warning for any entry whose condition can't be resolved instead of treating it as always true.

## 3. Voice-set format: OStim's legacy syntax, single-object forms and many fields aren't read, and nothing is logged

- **Class**: Partial
- **Severity**: Medium
- **OStim**: `SoundTable::setup` `SoundTable.cpp:13-83`:
  - **Legacy syntax** (`"moan": {"mod","formid"}`, `"moanMuffled"`, `"climax"`, `"climaxMuffled"`, `"moanExpression"`, `"climaxExpression"`, lines 46-51). OStim's own `defaultmale.json` uses it. It gives the default expressions `moan` / `climax`.
  - Any of `sound`, `soundMuffled` or `dialogue` may be a **single object** instead of an array (`loadSoundSets` 212-217, `loadDialogueSets` 267-272).
  - The fields `voice`, `dialogue`, `climaxCommentSelf`, `climaxCommentOther` and `postSceneDialogue`, and on entries `dialogueThreshold`.
  - A file without `target` is warned about and skipped (16-19).
- **4Stim**: `ReadEntries` `Sounds.cpp:152` only reads arrays, and `ReadReactionSet` 180-188 only reads `sound` / `soundMuffled`. A legacy-format file or a single-object entry loads as an **empty** set, and the file still counts as loaded (`++count`, line 250). The fields 4Stim lacks (`dialogue`, `climaxComment*`, `postSceneDialogue`, `voice`, `condition`) are dropped without a log line.
- **Fallout 4 constraint?** None.
- **Suggested fix**: read the legacy keys and single-object forms. For every OStim field not yet supported, write `Sound: <file>: "<field>" isn't supported yet` to the log. Warn when `target` is missing or unresolved.

## 4. Climax comments and other-actor climax reactions are missing, including the sound-only kind

- **Class**: Missing (SOUND.md:79-81 lists "climax comments" under dialogue, but they aren't only dialogue)
- **Severity**: Medium
- **OStim**: `ThreadActor::playClimaxSound` `Core/ThreadActor/ThreadActorSound.cpp:188-217`:
  - Every **other** actor gets `reactToClimax` (236-248): their `climaxCommentOther` reaction set plays 750 ms later.
  - The climaxing actor queues `climaxCommentSelf` 1000 ms after their climax sound ends (NPCs only, 200-216).
  - Both are full reaction sets: `sound`, `soundMuffled` and `dialogue`. The dialogue countdown is forced to 0 so the dialogue part always plays (195, 211, 242).
  - The sound-only form works without any dialogue support.
- **4Stim**: `Sounds::Climax` `src/Sounds.cpp:521-539` plays only the actor's own `climax` set. `climaxCommentSelf` / `climaxCommentOther` aren't parsed (`Load` 224-229) and nothing notifies partners.
- **Fallout 4 constraint?** None for the sound part.
- **Suggested fix**: load both sets. In `Climax`, queue `climaxCommentOther` (0.75 s) on every other actor in the scene through the existing `reaction` / `reactionIn` slot, and `climaxCommentSelf` (1 s after the climax sound ends, not for the player).

## 5. Dialogue / `talk` isn't implemented (documented as "not in yet")

- **Class**: Missing
- **Severity**: Medium. OStim ships no dialogue itself, but every dialogue voice pack depends on it, and post-scene lines are part of the base behaviour for addon sets.
- **OStim**:
  - `playSound` `ThreadActorSound.cpp:130-177`: on a moan tick with the action's `talk` set (or always for events and climax), the dialogue countdown is decremented, and the first fitting `dialogue` set plays (`SayTo`, or `sayAs` with the set's `voice`, `GameAPI/GameDialogue.cpp`) once the countdown is at or below the set's `dialogueThreshold`.
  - `canTalk` (83-119) blocks dialogue for muted or muffled actors, for the player unless `playerDialogue`, and under `preventSameVoiceCrossTalk`.
  - Settings with defaults (`MCM/MCMTable.h:264-273`): `maleDialogueCountdownMin/Max` 3/6, `femaleDialogueCountdownMin/Max` 1/3, `playerDialogue` 1, `preventSameVoiceCrossTalk` 0.
  - Post-scene dialogue `Core/ThreadActor.cpp:398-433`: the first non-player actor of each voice speaks to the player or their partner through `OSKSE.SayPostDialogue` with a 1-2 s delay, unless the thread has `NO_POST_DIALOGUE`.
  - Papyrus `OActorUtil.SayTo` / `SayAs` (`OActorUtil.psc:82,94`).
- **4Stim**: `talk` is parsed and merged (`src/Actions.cpp:107`, `SceneRegistry.cpp:599`) but nothing reads it. None of the settings, the countdown, post-scene lines or the Papyrus functions exist. Grepped `dialogue`, `SayTo`, `talk` in src/ and Data/Scripts.
- **Fallout 4 constraint?** Partly. FO4 Papyrus `ObjectReference.Say(Topic, Actor akActorToSpeakAs, bool, ObjectReference akTarget)` covers SayTo and speaking as another actor. "Say with another voice type" needs OStim's .fuz-path trick, and FO4's voice path layout is the same idea (`Sound\Voice\<plugin>\<voicetype>\<id>.fuz`): unverified. FO4's player is voiced, so `playerDialogue` is meaningful and its default of 1 may need rethinking (the player's lines can trigger the dialogue camera: unverified).
- **Suggested fix**: implement in OStim's order: dialogue sets plus countdown plus `canTalk` (muffled, muted, player, cross-talk), then post-scene dialogue on scene end, then `SayAs`. Add the six settings under OStim's names.

## 6. Climax and event reactions cut the current moan off; OStim waits for it to finish

- **Class**: Different (SOUND.md:67 even says "cutting in over a moan" next to "as OStim's")
- **Severity**: Medium (audible: moans are chopped mid-sound, and climax events fire earlier than in OStim)
- **OStim**:
  - `ThreadActor::climax` `ThreadActorClimax.cpp:55-64`: if a moan is playing or the actor is talking, the **whole climax** (counter, `ostim_actor_orgasm` event, end timer, sound) waits in `awaitingClimaxInner` until it stops (`loopClimax` 18-21).
  - `loopSound` `ThreadActorSound.cpp:11-56`: an event reaction's `eventTimer` only starts counting once the current moan or line has ended (lines 26-32 return first).
- **4Stim**:
  - `ClimaxNow` `src/main.cpp:3756-3786` climaxes at once, and `Sounds::Climax` `Sounds.cpp:529-531` stops the playing moan.
  - In `Sounds::Tick` 546-557, a due reaction stops the moan and plays immediately, and its delay counts down while the moan plays.
- **Fallout 4 constraint?** None: 4Stim already tracks `playing`.
- **Suggested fix**: add `Sounds::IsMakingSound(id)`. Make ClimaxNow defer (OStim's `awaitingClimaxInner`) while it's true. In Tick, only count `reactionIn` down when `!playing`, and don't stop the moan.

## 7. OStim-format sound descriptors are second-class: no volume, can't be stopped, 2-second guess

- **Class**: Partial / Different
- **Severity**: Medium (every voice set or action written OStim's way, with `"sound": {mod, formid}`, goes through this path)
- **OStim**: `GameSound::play` (`GameAPI/GameSound.cpp`): `BSAudioManager::GetSoundHandle(handle, descriptor, 0x10)`, `SetObjectToFollow`, `SetVolume(moanVolume or soundVolume)`, `Play`. `isPlaying` comes from the real handle, which gates moan timing (`lastMoan`), the climax wait and loop sounds.
- **4Stim**: `Sounds::Play` `src/Sounds.cpp:457-469` calls Papyrus `FourStimScene.PlaySoundForm` (`Data/Scripts/Source/User/FourStimScene.psc:241-247`, `Sound.Play`):
  - `moanVolume` and `soundVolume` are ignored;
  - `Playing::Stop` (365-371) can't stop it, so mute, climax and scene end leave it playing;
  - "playing" is a fixed **2 s** estimate (468), so the next moan's timer starts too early or too late;
  - `loop` action sounds overlap or gap.

  SOUND.md:32 doesn't mention any of this.
- **Fallout 4 constraint?** None found. CommonLibF4 has `BSAudioManager::GetSoundHandle(BSSoundHandle&, const BSISoundDescriptor*, float distance, uint32 flags, ...)` (`lib/commonlibf4/include/RE/B/BSAudioManager.h:56`, ID 2267105 in `RE/IDs.h:515`). Whether the old-gen runtime has an address for it is unverified (check `compat/`).
- **Suggested fix**: play descriptors natively with `GetSoundHandle(handle, descriptor, 0, flags)` plus `SetObjectToFollow`, `SetStaticAttenuation` for volume, and `Play`, the same as files. Keep the Papyrus path only as an old-gen fallback, and document its limits.

## 8. Muting, or an event reaction with nothing to play, leaves the moan face stuck

- **Class**: Different (bug)
- **Severity**: Medium (visible: the actor holds the moan expression for the rest of the scene or until the next sound)
- **OStim**: when a moan ends, `clearEventExpression` runs in `loopSound` (`ThreadActorSound.cpp:26-31`). Muting doesn't touch sounds that are already playing.
- **4Stim**: the event expression is cleared only in `Tick`'s `if (state.playing)` branch (`Sounds.cpp:559-568`). Two paths set `playing = false` without clearing `eventExpression`:
  - `SetMuted` (579-591): after muting, `Tick` skips the actor (570) and the expression is held for good (`Expressions::SetEvent(..., a_timed=false)` keeps it "until ClearEvent", `src/Expressions.h:42-44`).
  - The reaction path (552-556): if the reaction plays nothing (muffled with no `soundMuffled`, or no entry fits), `StartReaction` sets no new expression and the old moan face remains.
- **Fallout 4 constraint?** None.
- **Suggested fix**: whenever a playing sound is stopped, also clear `eventExpression` and call `Expressions::ClearEvent`. A helper `StopSound(id, state)` would cover both cases.

## 9. The default sets and "muffled" sound filtering differ from OStim's

- **Class**: Different (undocumented)
- **Severity**: Low
- **OStim**:
  - `defaultfemale.json` plays the suck sound only under perk `OStimBlowjobCondition` (0xEA6: GetInFaction `OStimBlowjobActorFaction` 0xE52). Only the **actor (mouth) role** of `blowjob` and `deepthroating` puts an actor in that faction (`actions/blowjob.json` actor `faction`).
  - Other muffled actions (kissing, cunnilingus, licking...) are silent for women.
  - `defaultmale.json` (legacy syntax) has **no muffled sound**, so men are silent while muffled.
- **4Stim**:
  - Both default sets play suck sounds for `blowjob`, `deepthroating`, `suckingnipple`, `penilelicking`, `testicularlicking` (`Data/F4SE/Plugins/4Stim/Voice Sets/*.json`).
  - The `actions` filter matches if the actor is in **any** role of the action: actor, target or performer (`Sounds::Enter` `Sounds.cpp:514-517`). So the receiver of a blowjob who is muffled by kissing plays suck sounds.
  - SOUND.md:77 doesn't say this differs from OStim.
- **Fallout 4 constraint?** None.
- **Suggested fix**: match OStim: female set blowjob and deepthroating only, mouth role only; male set without `soundMuffled`. Allow a role in `actions`, for example `"blowjob:actor"`, or better use conditions (finding 2). Note it in SOUND.md if the extra sounds are kept on purpose.

## 10. The climax expression shows even when no climax sound plays

- **Class**: Different
- **Severity**: Low
- **OStim**: the climax expression comes only from the sound set that plays (`playSound` → `setEventExpression(set->expression)`, `ThreadActorSound.cpp:167-169`). In new syntax an entry without `expression` has none (`loadReactionSet` passes `""`, `SoundTable.cpp:340`); in legacy syntax it defaults to `climax` (50). With no set (a non-human, or muffled with no `soundMuffled`), the face doesn't change.
- **4Stim**: `StartReaction` `Sounds.cpp:318` falls back to `"climax"` when the entry has no expression. `Climax` 525/537 shows the timed climax face when there is no voice set or the actor is muted.
- **Fallout 4 constraint?** None.
- **Suggested fix**: decide on purpose. If keeping it, document it in SOUND.md / EXPRESSIONS.md as 4Stim's.

## 11. Sex toy scaling inputs don't match OStim's formulas

- **Class**: Different (API.md:122 presents them as OStim's)
- **Severity**: Low
- **OStim**: `ToyHandler::update` `SexToys/ToyHandler.cpp:42-69`:
  - `STIMULATION_RATE` = `baseExcitementInc * (1 + relativeSpeed)`, where `baseExcitementInc` is "without speed or MCM modifier" (`Core/ThreadActor.h:249`);
  - `STIMULATION_VALUE` = raw excitement, 0-100;
  - `SPEED` = `relativeSpeed` = speed / speeds.size() (`Core/Thread.h:96`), which never reaches 1.
- **4Stim**: `SexToys::Update` `src/SexToys.cpp:126-141`:
  - rate = `Excitement::Rate` = base × speedMod × **sex multiplier × actor multiplier** (`src/Excitement.cpp:37-40, 58`);
  - value = excitement / 100 (comment at 131 only);
  - speed = speed / (count − 1).
- **Fallout 4 constraint?** None.
- **Suggested fix**: use OStim's formulas (rate without the sex and MCM multipliers; speed / count). If excitement is kept on a 0-1 scale on purpose (OStim's raw 0-100 against 0-1 defaults is effectively always at maximum), say so in API.md as a departure.

## 12. Lights: the equip-object API doesn't reach them; doc is out of date

- **Class**: Documented departure (the API bridge is missing) / doc error
- **Severity**: Low
- **OStim**:
  - Lights are equip objects of type `"light"` (`equip objects/facelight.json` etc., `nolight` is `"default": true`).
  - They're applied at thread start if `!onlyLightInDark || GetActor(0) light level < 20`, and only to actors without a light (`hasLight`) (`Core/Thread.cpp:143-150`).
  - They're chosen per actor base, per race or per sex, saved (`Trait/TraitTable.cpp:257-290`), and controlled through `SetEquipObjectID(actor,"light",id)`, `OSANative.SetFaceLight` / `ApplyFaceLight` (`Papyrus/PapyrusActor.h:209-220`) and the `options/facelight.json` page (`equipType: "light"`).
  - `futaUseMaleLight` (0xE25) exists but nothing uses it (`MCM/MCMTable.cpp:55`), so it needs no port.
- **4Stim**: lights are a separate system (`src/Lights.cpp`, per sex only, `TypeFor` 95-108). This is documented (`docs/ACTORS.md:129`, `LIGHTS.md` "Not in yet": per NPC, `hasLight`, HUD). However, `FourStim.SetEquipObjectID(actor, "light", ...)`, `EquipObject(actor, "light")` and an OStim-style options file with `equipType: "light"` silently do nothing (`src/EquipObjects.cpp` has no "light"). Per-race choice isn't listed among the missing items. `docs/LIGHTS.md:15` still says "4Stim has no MCM, so it's per sex in the ini", but the MCM has a Lights page (`Data/MCM/Config/4Stim/config.json:2378`).
- **Fallout 4 constraint?** Placing LIGH refs is a reasonable FO4 replacement. Routing the equip-object API type `"light"` to `Lights` has no constraint.
- **Suggested fix**: route type `"light"` in the EquipObjects Papyrus and options paths to `Lights` (choose or toggle per actor). Add per-race to "Not in yet". Fix the LIGHTS.md MCM sentence.

## 13. bonedistance action sound: the fix to OStim's target-bone bug is only noted in the code

- **Class**: Different (undocumented outside code)
- **Severity**: Low
- **OStim**: `BoneDistanceSoundType` reads the **target's** bones from `"actorBone"` (`Sound/ActionSound/SoundType.cpp:62`, bug), so OStim content is effectively tested with the actor's bone names looked up on the target.
- **4Stim**: reads `"targetBone"` (`src/Actions.cpp:224-226`, comment only). SOUND.md's action-sound section doesn't mention it. 4Stim also adds `minRange` (documented).
- **Fallout 4 constraint?** None.
- **Suggested fix**: add one line to SOUND.md / ACTIONS.md: "OStim reads actorBone for both (a slip); 4Stim reads targetBone".

## 14. The API factions OStim voice-set conditions rely on don't exist

- **Class**: Not considered (for sound); overlaps the API audit
- **Severity**: Low now, Medium once conditions (finding 2) are supported
- **OStim**: voice-set and dialogue `condition` perks test the factions OStim keeps up to date: `OStimExcitementFaction`, `OStimTimesClimaxedFaction`, `OStimTimeUntilClimaxFaction`, `OStimActorCountFaction`, `OStimSchlongifiedFaction`, `OStimInWaterFaction` (`list of API forms.txt`; set in `Core/ThreadActor.cpp:45-49`, `ThreadActorClimax.cpp:139-148`), plus each action's role `faction` (e.g. `blowjob.json` 0xE52). This is how a voice pack plays different moans at high excitement or after a climax.
- **4Stim**: none of them; 4Stim has no plugin (grepped `ExcitementFaction`, `TimesClimaxed`, `Schlongified`). Only the Papyrus getters (`GetTimeUntilClimax` etc., `docs/EXCITEMENT.md:33-35`).
- **Fallout 4 constraint?** These need a plugin, or 4Stim-native conditions. A JSON-side alternative would be `"minExcitement"`, `"maxExcitement"` and `"minTimesClimaxed"` on entries.
- **Suggested fix**: when adding conditions, either ship a small ESL with OStim's factions, or add 4Stim entry filters for excitement and times climaxed. Document whichever is chosen.

---

## Checked and matching

- **Voice set files**: folder of JSON files, not recursive (OStim `Util/JsonFileLoader.h:7-26`). `name` is translated, falling back to the file name. Targets 0 and 1 are the default male and female sets regardless of mod, so OStim's `Skyrim.esm` 0x1 targets still work (`Sounds.cpp:110-112`). `aliases` are honoured.
- **Moan timing**: interval random between `moanIntervalMin` / `moanIntervalMax` (2500 / 4000 ms, OStim `MCMTable.h:264-265`). It counts only after the current sound ends, and an entry's `moanIntervalOverride` (seconds) replaces it (`ThreadActorSound.cpp:26-31,122-128,172-176` vs `Sounds.cpp:323,559-575`). The ini is in ms with unit conversion for the MCM (`src/MCM.cpp:70-81`, `main.cpp:348`).
- **When actors moan**: only while an action says `moan`. They're muffled when an action says `muffled` or the `muffle` actor property applies. Muffled with no `soundMuffled` gives silence, as OStim. Flags are merged across actions with OR (`Graph/GraphActor.cpp:8-10` vs `SceneRegistry.cpp:598-600`).
- **Event reactions**: `eventActorReactions` / `eventTargetReactions` / `eventPerformerReactions` with supertype fallback, played even when the action has no `moan` (OStim `ignoreChecks`). Timing differs (finding 6).
- **Muting**: `Mute` / `Unmute` / `IsMuted`, and the actor property `mute`. 4Stim follows OStim's documented intent ("won't make any sounds", `actor properties README.txt:18`, `OActor.psc:164-176`). OStim's code only blocks talking and `muteWithActor` sounds, and its `Unmute` calls `mute()` (`Papyrus/PapyrusThreadActor.h:148-151`); 4Stim doesn't copy those bugs.
- **Action sounds**: types `peak`, `bonedistance` and `loop`. Fields `muteWithActor` / `muteWithTarget` / `playerThreadOnly`, and the scene action's `muted`. bonedistance has the 1 s initial cooldown, the 10% / 90% threshold, `minInterval` 50 ms default and optional `maxInterval`; loop has `delay` plus 150 ms lead, with the first sound after `delay`. All play on the action's actor at `soundVolume`, and players are rebuilt on scene change (OStim `Thread.cpp:291-314`, `ActionSound/*` vs `src/Peaks.cpp:150-330`).
- **Volume settings**: `moanVolume` and `soundVolume` (default 1) apply to file sounds; descriptor sounds ignore them (finding 7).
- **Sex toy plugin interface**: `"SexToys"` interface, ABI 1, with `registerReloadListener` / `addToy`. `SexToy` and `SexToyReloadListener` have the same virtual layout and comments (`include/4Stim/PluginInterface.h:209-261` vs `PluginInterface/SexToys/*.h`). All toy calls run on detached threads.
- **Toy reloads**: on save load and when the player's thread starts. Toys are cleared before listeners run, and groups are rebuilt after.
- **Toy driving**: toys only in the player's scene. Slots come from actions' `toySlot` (OStim reads `"toySlot"` into a list). One toy per slot, in action and role order. Synchronization types player / male / female / scene: the first actor of that sex, not futa-aware, as OStim.
- **Toy updates**: on each peak, using the average of the last five peak intervals; peak timing resets on a speed change. Without peaks, an update every 1 s. `getUpdateInterval` cooldown applies, and a climax sets a 1000 ms cooldown.
- **Toy defaults**: no scaling (max), magnitude 0.1-0.5, peaks 0.6-1.0, climax 1.0. They equal OStim's "individual at every level" result (`SexToys/Settings/*.h`).
- **Toy calls OStim documents but never makes**: `turnOn` / `turnOff` and the climax `peak`. OStim never calls them (no caller of `ToyWrapper::turnOn` or `ToyGroup::climax`). 4Stim calls them as the interface comments describe. This is arguably the intended behaviour, but worth a line in API.md.
- **Toy menu**: per-toy and per-slot settings and OStim's toy menu are missing, but documented (`API.md:130`, `SexToys.h:24-28`).
- **Lights**: `SetOnlyLightInDark` default 0, the first actor's light level, given once at scene start and removed at end. The 20 → `fLightLevelDark` 80 rescale and the LIGH-ref approach are documented in LIGHTS.md.
- **Intiface Central client**: 4Stim-only, documented as a departure (`API.md:126-128`). Not compared further, since OStim has no counterpart.
