# Parity audit 2: per-actor systems (OStim NG ThreadActor, ActorProperties, Trait, Alignment vs 4Stim)

Scope: undress/redress, excitement and climax, facial expressions and eye look, penis bend, heels, scaling, actor properties and ActorCondition, equip objects, alignment keys, freeze/AI, per-actor sound hooks in ThreadActor.
OStim paths are relative to `/tmp/ostim/OStimNG-main/skse/src/` unless they start with `data/`. 4Stim paths are relative to `/home/claude/4stim/`.
Line numbers are from the files as they are now. Nothing was built or run.

---

## 1. Expression files that name actions by alias never match, so OStim's own oral, licking and tickling faces don't show
- **Class**: Different (undocumented)
- **Severity**: High
- **OStim**: When it loads expression files, OStim resolves each `actionActors` / `actionTargets` name through `GraphTable::getActionAlias` (`Trait/TraitTable.cpp:50-60`). OStim's shipped expression files name 9 actions by an alias, not by the id: `cunnilingus` (id `vulvaleating`), `lickingvagina` (`vulvallicking`), `rubbingclitoris` (`vulvalrubbing`), `lickingpenis` (`penilelicking`), `lickingtesticles` (`testicularlicking`), `suckingnipples` (`suckingnipple`), `lickingnipples` (`lickingnipple`), `kissingfeet` (`kissingfoot`), `ticklingfeet` (`ticklingfoot`). Examples are `data/.../facial expressions/stimulated1.json` and `licking1.json`. I checked this with a script against the `aliases` fields in `data/.../actions/*.json`.
- **4Stim**: `src/Expressions.cpp:263` keys the tables by the raw lowercased name from the file. `NodeExpressions` then looks them up by the action's id (`Find(g_byActionTarget, a_action.type->id)`, lines 297 and 302). 4Stim's action ids are OStim's (`Data/F4SE/Plugins/4Stim/Actions/Default.json` has `vulvaleating`, `penilelicking`, ... with `cunnilingus` and the others as aliases). The 80 expression files shipped in `Data/F4SE/Plugins/4Stim/Facial Expressions/` are OStim's, unchanged. Result: every scene with these actions falls through to the next action, or to `default`. Cunnilingus, rimming the nipples, licking and foot tickling get the generic face instead of the "stimulated" / "licking" sets.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: In `LoadLocked`, map each actionActors/actionTargets name through `Actions::Find(name)->id` when the action exists, as OStim does with `getActionAlias`.

## 2. Transitions take the destination scene's per-role values: scale, heels, look, penis bend, offset and alignment
- **Class**: Different (only the alignment part is documented, and not as a departure)
- **Severity**: Medium
- **OStim**: Every node, transitions included, applies its own `GraphActor`. `changeNode` (`Core/ThreadActor.cpp:183-233`) sets the heel state from `graphActor->feetOnGround`, the eye override from `graphActor->eyeballModifierOverride`, and the scale from `graphActor->scale` / `scaleHeight` (`ThreadActorAnimation.cpp:29-51`). `alignActors` then looks up `Alignments::getActorAlignment(key, m_currentNode, i)`, keyed by the transition's own `scene_id`, with that node's `sosBend` as the default (`Alignment/Alignments.cpp:101-146`). `alignActor` adds the transition's own `offset` (`Core/Thread.cpp:371-403`).
- **4Stim**: `LockScene`, `SpotOf` and `RefreshActors` (`src/main.cpp:2644-2790`) all use `SceneRegistry::Settled(scene)`, the destination of a transition (`src/SceneRegistry.cpp:995-1005`). Settled is used for `offset`, `penisBend`, `lookUp`/`lookLeft`, `scale`, `scaleHeight` and `feetOnGround` (both the heels and the alignment key's heel term). A transition's own values in its scene file are ignored. Example: a kneel-to-stand transition keeps the heel state and look of the scene it lands in. `docs/ALIGNMENT.md` says only "A transition uses the alignment of the scene it goes to", not that the scene-file fields are ignored, and doesn't call it a departure.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Use the transition's own role fields, as OStim does. If keeping the destination's alignment is wanted, limit `Settled` to the alignment lookup and document it as a departure.

## 3. Undress, weapon and looking annotations are missing
- **Class**: Missing
- **Severity**: Medium (High for animations converted from OStim that rely on them)
- **OStim**: `Core/Thread.cpp:856-901` handles these annotations, documented in `data/SKSE/Plugins/OStim/list of annotations.txt`:
  - `OStimUndress` and `OStimRedress`
  - `OStimUndressPartial.<hexmask>` and `OStimRedressPartial.<hexmask>`
  - `OStimRemoveWeapons` and `OStimAddWeapons`
  - `OStimSetLooking.<x>,<y>`, `OStimUnsetLooking` and `OStimResetLooking`
- **4Stim**: The annotation handler (`src/main.cpp:7263-7344`) knows only Climax, Event, Peak, PlayExpression, BendSoS/ResetSoS and Equip/UnequipObject. `docs/EVENTS.md:58-71` lists those and doesn't say the others are missing. Animated undressing is done with a scene-file `undress` list instead (`docs/UNDRESS.md`), and `docs/TODO.md:33` expects the converter to turn annotations into that list. An animation that carries its own `4StimUndressPartial` (or the redress, weapon or looking annotations) does nothing.
- **Fallout 4 constraint?**: None. The handler already runs for 4Stim tags.
- **Suggested fix**:
  - Add `4StimUndress`, `4StimRedress`, `4StimUndressPartial.<hex>` and `4StimRedressPartial.<hex>`, mapping OStim's Skyrim mask through the UNDRESS.md table.
  - Add `4StimRemoveWeapons` and `4StimAddWeapons`.
  - Add `4StimSetLooking`, `4StimUnsetLooking` and `4StimResetLooking`, driving `EyeLook::Set`.
  - List them in EVENTS.md.

## 4. The look modifiers in expressions (8-11) are dropped, though eye bones now exist; the look also has no layering, no random look-down and no smoothing
- **Class**: Documented departure (the reason is now weak) plus Missing
- **Severity**: Medium
- **OStim**: Expressions carry eyeball modifiers 8-11 (down, left, right, up). OStim's `climax1-4.json` roll the eyes up (modifier 11, 60-100), so the climax face does it. The layering:
  - Event and override expressions that set eyeballs take over from the scene's look direction. The scene's look takes over from the underlying expression (`ThreadActorExpression.cpp:211-230`, `375-413`; `Core/ThreadActor.cpp:199-204`).
  - While an override (oral) expression is active, each underlying re-pick has a 1-in-5 chance to make the actor look down (`setLooking({{8,100}})`). The next re-pick resets it (`ThreadActorExpression.cpp:29-38`).
  - Eye changes are eased at speed 5 per 20 ms (`applyEyeballOverride`).
- **4Stim**: `src/Expressions.cpp:172` drops types 8-11 ("no morphs in Fallout 4"), and `docs/EXPRESSIONS.md` gives that as the reason. `src/EyeLook.cpp` now turns the eye bones, but it's fed only from scene-file look fields (`src/main.cpp:2742-2746`). It isn't layered under event or override expressions, has no random look-down, and turns the bones instantly with no easing.
- **Fallout 4 constraint?**: No longer. EyeLook gives a channel for modifiers 8-11.
- **Suggested fix**:
  - Parse 8-11 into an eye layer.
  - Resolve it as OStim does (override > event > scene look > underlying) and feed the winner to `EyeLook::Set`.
  - Add the 20% look-down during overrides.
  - Ease toward the target, about 250 points/s.
  - Update EXPRESSIONS.md.

## 5. Actor conditions (type, requirements, penis) are cached instead of read fresh each time
- **Class**: Different (undocumented)
- **Severity**: Medium
- **OStim**: `ActorCondition::create` re-reads actor properties every time it's called (`Trait/Condition.cpp:19-57`). `Thread::getActorConditions` calls it on every navigation and filter (`Core/Thread.cpp:347-354`). Conditions based on worn items, factions or in-water follow the actor's state live.
- **4Stim**: Role matching, ordering and the picker use `ActorProperties::Cached` (`src/main.cpp:979-986`, `5916`, `5942`, `7558`). The cache is refreshed only when the picker scans (`Remember`, `src/main.cpp:6079-6095`) and cleared only on `Reload` (`src/ActorProperties.cpp:503`, `518`). An actor who starts a scene through the API without the picker is cached forever once seen.
  - A futa who puts on or takes off the equippable penis (the documented way to get futa, ACTORS.md) keeps the old `penis`, type and requirements in the navigation filters.
  - 4Stim's default undress slots 54-58 can strip such an item mid-scene (UNDRESS.md mentions this for body parts).
  - Meanwhile `HasPenis` (strap-ons, climax) is read live, so the two can disagree within one scene.
- **Fallout 4 constraint?**: None. The cache exists only because the picker lists are built off the main thread.
- **Suggested fix**: Refresh the cache for a scene's actors on every scene change on the main thread (`Remember` in `RefreshActors`). Also refresh on equip and unequip events, or at least at scene start for API-started scenes.

## 6. Climax stalling: actor and thread stalls are merged, `Climax()` clears the stall, and a stalled climax is dropped
- **Class**: Different (undocumented)
- **Severity**: Medium
- **OStim**:
  - The actor flag `stallClimax` and the thread flag `thread->getStallClimax()` are separate (`ThreadActorClimax.cpp:13-53`).
  - `IsClimaxStalled(actor, checkThread)` can report either (`Papyrus/PapyrusThreadActor.h`).
  - `orgasm()` on a stalled actor sets excitement to 100 and queues the climax (`awaitingOrgasm`, `setTimeUntilClimax(0)`). It fires as soon as both flags clear (`loopClimax`).
  - `Climax(actor, ignoreStall=true)` climaxes without touching the stall flag.
- **4Stim**:
  - `T_StallClimax` and `T_PermitClimax` set and clear every actor's own flag (`src/main.cpp:7847-7861`). `PermitClimax` on the thread therefore wipes a stall a mod set on one actor. Its `permitActors` argument is ignored.
  - `FourStim.Climax` always calls `SetStalled(false)` (`src/main.cpp:4705-4711`), so `FourActor.Climax(Act, true)` permanently un-stalls the actor.
  - `FourActor.Climax(Act)` on a stalled actor just returns (`Data/Scripts/Source/User/FourActor.psc:49-54`), so the request is lost instead of queued.
- **Fallout 4 constraint?**: None.
- **Suggested fix**:
  - Keep a separate thread stall flag in `ActiveScene` and check `actor || thread` in `Excitement::Tick`.
  - Give `Climax` a "pending" state that fires when the stall is lifted.
  - Have ignore-stall leave the flag alone.

## 7. NPC-only scenes ignore SetEndOnOrgasm, SetEndOnSubOrgasm and SetEndOnBothOrgasm
- **Class**: Different (undocumented)
- **Severity**: Low
- **OStim**: `ThreadActorClimax.cpp:113-134` (climaxInner): the all / player / male / female end rules apply to every thread, NPC threads included. `endNPCSceneOnOrgasm` is an extra rule on top for NPC threads.
- **4Stim**: `src/main.cpp:3795-3804`: in a scene without the player, only `endNPCScenes` counts. With `endNPCSceneOnOrgasm=0` and `SetEndOnOrgasm=1`, OStim ends the NPC scene on a male climax and 4Stim doesn't. `docs/EXCITEMENT.md` says "Scenes without the player use endNPCSceneOnOrgasm", which describes the code but not the difference from OStim.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Evaluate the general rules for every scene, then OR in `endNPCScenes` for NPC scenes.

## 8. A climax cuts off the moan instead of waiting for it to end
- **Class**: Different (undocumented)
- **Severity**: Low
- **OStim**: `climax()` waits until the current moan or dialogue line has finished (`awaitingClimaxInner`, `ThreadActorClimax.cpp:55-64`, `18-21`) before `climaxInner` runs. The count, event, speed drop, effects and end timer all wait too. Excitement is frozen meanwhile.
- **4Stim**: `ClimaxNow` runs at once (`src/main.cpp:3756-3806`), and `Sounds::Climax` stops the playing moan (`src/Sounds.cpp:529-531`).
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Hold the climax in `awaitingClimax` until `Sounds` reports the actor's current sound finished, as OStim does.

## 9. Per-actor action stimulation overrides are missing
- **Class**: Missing
- **Severity**: Medium
- **OStim**: `ActionAttributes::getStimulation` and `getMaxStimulation(role, actor)` first check a per-actor override, saved in the co-save by base form ID (`Graph/Action/ActionAttributes.cpp:55-63`, `Serial/Manager.cpp:62-100`). The overrides are set from the MCM's excitement page per actor and action (`data/Scripts/Source/OSexIntegrationMCM.psc:2872-2913`) and from Papyrus `OData.Get/Set/ResetActionStimulation` and `...MaxStimulation` (`Papyrus/PapyrusData.h:78-124`).
- **4Stim**: `src/Excitement.cpp` `Enter` reads only `a_side.stimulation` / `maxStimulation` from the action type. Grepping for ActionStimulation found nothing anywhere.
- **Fallout 4 constraint?**: None. The co-save already exists.
- **Suggested fix**: Add a per-base-form override map (role -> action -> value), saved in the co-save, with the OData functions and an MCM or HUD page.

## 10. OStim's API factions are missing and the gap isn't documented
- **Class**: Missing
- **Severity**: Medium (it breaks condition-based add-ons ported from OStim)
- **OStim**: Each scene actor is put in factions whose ranks track their state (`Core/ThreadActor.cpp:44-50`, `ThreadActorExcitement.cpp`, `ThreadActorClimax.cpp:139-149`, `data/.../API factions README.txt`):
  - `OStimExcitementFaction` (rank = excitement)
  - `OStimTimesClimaxedFaction`
  - `OStimTimeUntilClimaxFaction` (-1, seconds, or 101)
  - `OStimSchlongifiedFaction`
  - `OStimActorCountFaction`
  - `OStimInWaterFaction`

  Expression files can also read faction ranks (`faction` / `factionFallback` on a modifier, `Trait/FacialExpression.cpp:6-28`).
- **4Stim**: None of these factions exist. Grepping `faction` in `docs/` finds only ACTIONS.md's note that action factions are "skipped until the features they belong to come". The per-actor API factions aren't mentioned. `src/Expressions.cpp` `ReadModifier` ignores `faction` and `factionFallback`.
- **Fallout 4 constraint?**: 4Stim has no plugin. The closest equivalents are factions in an optional ESL, or a `FourStim.GetExcitement`-style API, which exists but can't be used in CK conditions.
- **Suggested fix**: Document the gap in API.md. If an ESL is ever added, create these factions and keep the ranks updated from the scene clock. Read `faction` / `factionFallback` from expression files when the faction resolves.

## 11. Expression API: PlayExpression is timed though its docs say "until cleared"; SetExpressionsEnabled and HasExpressionOverride are missing
- **Class**: Different + Missing
- **Severity**: Low to Medium
- **OStim**:
  - `OActor.PlayExpression` sets an untimed event expression, held until `ClearExpression`, and returns its duration (`Papyrus/PapyrusThreadActor.h`, PlayExpression).
  - `SetExpressionsEnabled(actor, enabled, allowOverride)` sets `NO_UNDERLYING_EXPRESSION` / `NO_OVERRIDE_EXPRESSION` per actor (`Core/ThreadActorFlag.h`, `ThreadActorExpression.cpp:237-239`).
  - OStim.esp's `noFacialExpressionsFaction` blocks underlying expressions per actor (`Trait/TraitTable.cpp:74`, `ThreadActorExpression.cpp:97`).
  - `HasExpressionOverride` exists.
- **4Stim**: `AM_PlayExpression` calls `Expressions::SetEvent(..., true)`, which is timed and returns a bool (`src/main.cpp:8221-8224`). `FourActor.psc:125` says "Plays an event expression ... until cleared". There's nothing for per-actor enable/disable or HasExpressionOverride.
- **Fallout 4 constraint?**: None for the API. The faction would need a plugin.
- **Suggested fix**: Make PlayExpression untimed and return the duration (as OStim). Add per-actor flags in `Expressions::State`, with SetExpressionsEnabled and HasExpressionOverride.

## 12. The climax blur plays for anyone's climax, not only the player's
- **Class**: Different (undocumented)
- **Severity**: Low
- **OStim**: The blur (`OStimNutEffect`) plays only when the player is the one climaxing (`ThreadActorClimax.cpp:84-89`). Slow motion, camera shake and rumble play for any climax in the player's thread.
- **4Stim**: `PlayClimaxEffects` (`src/main.cpp:3617-3647`, blur at 3633) blurs for every climax in a scene with the player or being watched. `docs/EXCITEMENT.md` lists the blur with the other effects without saying it's only the player's in OStim.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Pass "is the player climaxing" to `PlayClimaxEffects` and gate the blur on it.

## 13. Climax doesn't drain stamina
- **Class**: Missing
- **Severity**: Low
- **OStim**: `actor.damageActorValue(STAMINA, 250)` (`ThreadActorClimax.cpp:80`).
- **4Stim**: Nothing found (grepped Stamina, DamageValue).
- **Fallout 4 constraint?**: None. FO4 has Stamina (Action Points are separate; OStim's intent is the "exhausted" stat, so Fallout 4's AP is debatable).
- **Suggested fix**: Damage AP or Stamina on climax, or document why not.

## 14. After a climax, excitement follows body sex where OStim uses whether the actor has a penis
- **Class**: Different (edge cases)
- **Severity**: Low
- **OStim**: After a climax (`ThreadActorClimax.cpp:68-75`), excitement is set to -3. If the actor has no penis, or is a female with `!futaUseMaleExcitement`, it is set to `min(post*times, max)` instead. The end rule's male test is `!female || schlong && futaUseMaleClimax` (`:128`): a male body with no penis still counts as male.
- **4Stim**: `Excitement::Climaxed` (`src/Excitement.cpp:155`) uses the rate sex from `MaleExcitement`, which comes from the body. A male body made penis-less by an actor property gets 0 (OStim: the female post-climax value). The reset is 0, not -3. `MaleClimax` (`src/ActorProperties.cpp`) requires a penis, so a male body without one counts as female for `SetEndOnSubOrgasm` (OStim: male).
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Pass "has a penis" into Excitement for the post-climax rule. Mirror OStim's `!female || (penis && futa)` test for the end rule.

## 15. Undressing: no wig handling, no Papyrus override hook, and smaller differences
- **Class**: Missing / Different
- **Severity**: Low to Medium
- **OStim**:
  - `undressWigs` (off by default) leaves hair-slot items whose mesh uses the hair tint shader on (`Util/FormUtil.cpp:7-36`, `Core/ThreadActor.cpp:122`, `575`).
  - `OUndress.UsePapyrusUndressing()` lets an add-on take over all undressing and redressing in Papyrus (`data/Scripts/Source/OUndress.psc`, `Core/ThreadActor.cpp:98-108`, `145-158`, `351-364`, `552-561`, `605-619`).
  - Undress at start happens when the actor is added (`initContinue`), whatever the first scene says.
  - An item is stripped when any of its slots is in the undress mask and any is in the action's mask (`canUndress` plus `overlaps`).
- **4Stim**:
  - There's no wig check or setting. Hair slots 30/31 aren't in the defaults, so only a wig that also claims an undressed slot is affected.
  - There's no Papyrus override hook: `FourStimUndress.psc` is internal.
  - A scene-level `noStrip` (`src/Undress.cpp:344`) also blocks `SetAlwaysUndressAtStart`.
  - The action's slots are first cut down to the allowed slots (`Strip`, `Allowed(slot)`). An item covering an allowed slot plus an action slot that isn't allowed (outfit 33 + neck 50, action "neck") stays on, where OStim strips it.
  - `SetRemoveWeaponsWithSlot` isn't read. That is covered by the documented "weapons always come off".
- **Fallout 4 constraint?**: A wig test would need FO4's hair shader flag (unverified). The rest isn't constrained.
- **Suggested fix**: Add a `UsePapyrusUndressing`-style hook (for example, a Papyrus global the plugin reads at load). Let at-start undressing ignore scene noStrip. Test overlap on the item's full slot mask. Consider `undressWigs`.

## 16. Equip object choices are per reference, without OStim's "random" / "default" ids or race / sex levels
- **Class**: Partial (race / sex is documented; the rest isn't)
- **Severity**: Low
- **OStim**: `TraitTable::getEquipObject` (`Trait/TraitTable.cpp:257-300`) looks up the choice in this order: the actor's base form ID, then the race form ID, then sex (0x0 / 0x1), then the type's default, then random. Each level can store `"random"` or `"default"`.
- **4Stim**: `EquipObjects::Choose` and `SetChosen` (`src/EquipObjects.cpp`, `g_chosen[a_id]` at 357) key choices by the reference form ID, reject `random` / `default`, and have no race or sex level. Generic NPCs spawned from one base don't share a choice. Also `docs/ACTORS.md` says choices last "until a game is loaded", but they are saved in the co-save (`src/main.cpp:9039`, `9068-9077`).
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Key by base form ID, add race and sex entries and the `random` / `default` ids, and fix the doc.

## 17. An actor property `sex` other than "male" / "female" means "no penis" instead of "decide by body"
- **Class**: Different
- **Severity**: Low
- **OStim**: `GameSexAPI::fromString` (`GameAPI/GameSex.cpp:6-16`) turns any other value into AGENDER. That entry still wins on priority, and `hasSchlong` then falls back to the body (`Util/CompatibilityTable.cpp:64-69`).
- **4Stim**: `src/ActorProperties.cpp:335`: anything that isn't "male" means no penis.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Store a tri-state: male, female, or "by body".

## 18. An override expression set that doesn't exist doesn't fall through to the next action
- **Class**: Different
- **Severity**: Low
- **OStim**: `Node::getOverrideExpressions` (`Graph/Node.cpp:283-315`) checks the role's own `expressionOverride` first, then each action's role `expressionOverride` in turn. It skips any set that has no expressions loaded.
- **4Stim**: At load, `SceneRegistry.cpp:601-602` copies only the first non-empty action override into the role. `src/Expressions.cpp:517` then looks up just that one set. If it's missing (an add-on's set not installed), there's no override, even when a later action's set exists. An `expression` actor property with an unknown set also blocks the scene's own override (OStim: `actorExpressions` would be null and fall back).
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Keep the ordered list of candidate sets and take the first one that resolves.

## 19. Expression parsing details
- **Class**: Different
- **Severity**: Low
- **OStim**: A `"phonemes": []` key still claims the mouth slot and zeroes it (`Trait/TraitTable.cpp:160-166`). Each applied layer resets that slot's untouched entries to 0 (`ThreadActorExpression.cpp:255-372`).
- **4Stim**: `src/Expressions.cpp` `ReadGender` sets the mouth bit only when the list isn't empty. The morph `max` merge is documented.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Set the slot bit when the key is present.

## 20. Docs say excitement isn't saved, but it is
- **Class**: Different (docs)
- **Severity**: Low
- **OStim**: Doesn't save excitement (threads end on load).
- **4Stim**: `docs/EXCITEMENT.md` ends with "it isn't saved". But `OnGameSaved` writes each actor's excitement (`src/main.cpp:8994`) and a resumed scene restores it (`src/main.cpp:2000-2002`). Times climaxed and the stall flags aren't saved.
- **Fallout 4 constraint?**: n/a
- **Suggested fix**: Fix the doc, or also save the climax count and stall so a resumed scene is consistent.

## 21. Unverified: NPC height may leave out race height and Fallout 4's heightMax
- **Class**: Not considered (unverified)
- **Severity**: Low
- **OStim**: Scales by `GetActorBase()->GetHeight()` (`GameAPI/GameActor.h:52`). Whether CommonLibSSE's `TESNPC::GetHeight` includes the race's height wasn't checked: no CommonLibSSE source is here.
- **4Stim**: `Scaling::NPCHeight` uses `TESNPC::height` only (`src/Scaling.cpp:213-217`). Fallout 4's `TESNPC` also has `heightMax` (`lib/commonlibf4/include/RE/T/TESNPC.h:185`), and races have male / female height in their DATA record, which CommonLibF4 doesn't expose. It wasn't checked whether the game randomizes per-reference height between `height` and `heightMax`. If it does, actors with a range are scaled wrongly.
- **Fallout 4 constraint?**: Possibly.
- **Suggested fix**: Check in game, by comparing an NPC's rendered skeleton height with `height`, and include the race and heightMax terms if they apply.

---

## Checked and matching
- **Excitement rate:** strongest stimulation plus 10% of the others, speed mod `1 + speed/speeds`, ceiling from the highest maxStimulation among actions that give anything, ceiling 0 when nothing does, and decay after the grace period. `excitementDecayGracePeriod` is converted from milliseconds (`src/main.cpp:359`). A rising tick resets the grace. Climax at 100 even when above the ceiling.
- **Sex multipliers and futa:** `futaUseMaleExcitement`, `futaUseMaleRole` (role sex Any) and the strap-on rule (penis-requiring roles; off in vagina-requiring roles or with `unequipStrapOnIfNotNeeded`).
- **ActorCondition matching** (`SceneRegistry.cpp:1038-1061`): type first, then `unrestrictedNavigation`, intended sex, and requirements. A null actor meets everything.
- **Actor property priority:** stable sort by priority, first match wins; a higher priority `false` overrides a lower `true`. Expression, mute and muffle properties are applied (Expressions.cpp:516, Sounds.cpp:506-509). Equip objects are applied at scene start.
- **Equip objects:** changes between scenes (diff old and new), variants with durations (including OStim's "same variant only extends" rule), items given are removed at the end and after a load, `phonemeObjects` with `phonemeObjectThreshold`, and undressing leaves equip objects on.
- **Expressions:** the underlying pick order (role `underlyingExpression`, `expressionAction`, actions in order, target before actor, then `default`). Re-picks every SetExpressionDurationMin/Max. Easing speeds 1/3/5 per 20 ms (50/150/250 per second). Gender by body. The actor property expression wins over the scene's. The face is reset on leave. `relativeSpeed = speed / speeds.size()`.
- **Alignment key:** `<sex><height>x<heels>` joined with `&`, the N/M/F letters, int(scale*100) and int(heels*100), OStim's grouping defaults, the file layout and fields, and the default `sosBend` from the scene. Penis bend is clamped to -10..9 with -10 flaccid. `OStimBendSoS` / `OStimResetSoS` offsets, reset on scene change.
- **Scaling:** `role.scale * alignment.scale * furnitureScaleMult / NPC height`, times `scaleHeight/(scaleHeight+heels)` with feet on the ground. `SetScaling` gives base scale times the multiplier. The original scale comes back at the end. `feetOnGround` defaults to the `standing` / `squatting` tags.
- **Undress basics:** the slot mask setting, NoStrip keywords, actor `noStrip`, `fullStrip` versus partial (`SetUndressIfNeed` / `SetPartialUndressing`), redress at the end, and redress animations by part (torso, feet, hands, head) for NPCs only.
- **Actor lock:** head tracking off, foot IK off and restrained (FourStimScene.psc:143-163), the do-nothing package hook, and position reset (documented departure for the player).
- **Climax effects present:** slow motion 0.3x for 2.5 s (player scene), shake, rumble, and `SetAutoClimaxAnims` with the climax annotation or a scene change. A climax with a penis drops the speed to the slowest. `endOnPlayer`, male, female and all.
- **Muted / muffled actor properties** reach Sounds.
