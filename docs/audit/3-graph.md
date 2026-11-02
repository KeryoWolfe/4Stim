# Parity audit 3: scene graph, actions, furniture and data formats

Scope: OStim `skse/src/Graph/**`, `skse/src/Furniture/**`, the helpers they call (JsonFileLoader, LegacyUtil, GamePosition, GameObject markers, Trait/Condition), the thread code that consumes the graph (Core/Thread.cpp, Core/Thread/ThreadNavigation.cpp, ThreadFurniture.cpp, Threading/Thread/NodeHandler + Peak handlers, UI/Scene/SceneMenu.cpp), and the data files and READMEs under `data/SKSE/Plugins/OStim`. All 280 OStim base scene files, the 86 action files, 23 action tag files and 17 furniture type files were surveyed by script. On the 4Stim side: src/SceneRegistry.*, Actions.*, Peaks.cpp, Furniture.*, Events.*, the consumers in main.cpp and HUD.cpp, Data/F4SE/Plugins/4Stim/*, and docs SCENES / ACTIONS / FURNITURE / EVENTS / API / TODO.

OStim paths below are relative to `/tmp/ostim/OStimNG-main/skse/src/` unless they start with `data/`. 4Stim paths are relative to `/home/claude/4stim/`.

---

## 1. OStim's base scenes ship under different ids, so OStim add-on packs can't link to them
- **Class**: Different (not documented). Also Not considered: compatibility with third-party OStim packs.
- **Severity**: High
- **OStim**: The base scenes are `data/SKSE/Plugins/OStim/scenes/OStim2P/.../OStim2PStandingApartMF.json` and so on. The file name is the id (GraphTableSetupNodes.cpp:16-18), and the `OStim` prefix is reserved for them (scenes README). Add-on packs connect to the base graph by those ids: `"origin": "OStim2PStandingApartMF"` is how FlufyFox's hub joins, and other packs also put base ids in `destination`, `autoTransitions` and Papyrus scripts.
- **4Stim**: The packaged base animations are a 4Stim-layout pack whose ids all start with `4StimTest_`, for example `4StimTest_OStim2PStandingApartMF` in Data/F4SE/Plugins/4Stim/Scenes/4StimTestAnims.json. No base scene has OStim's id, and no code maps one id to the other (grepped src/ and tools/ for `4StimTest_` and `OStim2P`). Any unmodified OStim add-on's `origin`/`destination` into a base scene is dropped (SceneRegistry.cpp:887-898), and silently when the pack set `noWarnings`. docs/SCENES.md ("Hubs") presents the FlufyFox `origin: OStim2PStandingApartMF` example as if it works. tools/make-hubs.py rests on the same assumption.
- **Fallout 4 constraint?**: None. Scene ids are only strings.
- **Suggested fix**: Ship the base scenes under OStim's exact ids. The simplest way is OStim's one-file-per-scene layout under `Scenes\OStim\...`; a pack file with `"id": "OStim2PStandingApartMF"` also works. Keep the test pack's extra 4Stim fields. Add a warning when a navigation names an `OStim*` id that isn't loaded.

## 2. A scene whose furniture type doesn't exist is never offered; OStim plays it as a floor scene. OStim's single and double bed types are missing
- **Class**: Different + Missing
- **Severity**: High
- **OStim**: When a scene's `furniture` names an unknown type, OStim logs a warning and the scene falls back to `"none"`, so it plays as a floor scene (GraphTableSetupNodes.cpp:256-266; FurnitureTable.cpp `getFurnitureType(std::string)` returns `none`). 40 base scenes use `singlebed` or `doublebed`, which are subtypes of `bed`, which is listed individually and has supertype `none` (data/.../furniture types/singlebed.json, doublebed.json, bed.json). `bedroll`, `alchemytable`, `enchantingtable`, `tableleanmarker`, `wardrobe*` and `cookingpot` are also referenced by OStim content.
- **4Stim**: The type is stored as written (SceneRegistry.cpp:296-299). `FurnitureFits` then never matches it (main.cpp:1026-1034), so the scene can't be picked, navigated to or randomly selected, and the log says nothing. Fallout4.json defines only bed, mattress, chair, bench, table, cookingpot, shelf and wall. The test pack works only because its converter rewrote `singlebed`/`doublebed` to `bed` (4StimTestAnims.json), so OStim's scene files can't be used unmodified.
- **Fallout 4 constraint?**: None for the fallback. For single and double beds, Fallout 4 beds have one or two sleep markers, and Fallout4.json already has `minMarkers`/`maxMarkers`.
- **Suggested fix**: On an unknown furniture type, warn and treat the scene as having none, as OStim does. Add `singlebed` (bed, maxMarkers 1) and `doublebed` (bed, minMarkers 2) subtypes of `bed` to Fallout4.json, so OStim's `"furniture": "singlebed"` files work as written. Add stub types for the other OStim ids, or log them as unsupported.

## 3. Unknown action types and out-of-range roles are dropped, which shifts every later action index
- **Class**: Different (docs/SCENES.md says only "Unknown types are skipped with a warning", without naming it as a departure)
- **Severity**: Medium-High
- **OStim**: An action whose type has no file keeps its type string and gets the `default` action's attributes ("No action found for {} using default", GraphTableActions.cpp:232-239, GraphTableSetupNodes.cpp:444). A role index outside the scene is also kept (merges are guarded at Node.cpp `mergeNodeIntoActors`). `action.index` is always the position in the file, and `findAction("customtype")` still matches by string.
- **4Stim**: The action is removed from `scene.actions` (SceneRegistry.cpp:559-576). Every later action moves down one index. As a result:
  - `4StimPeak.<n>` annotations hit the wrong action (Peaks.cpp `Annotated`; main.cpp:7305).
  - The actor field `expressionAction` points at the wrong action (Expressions.cpp:308).
  - `GetSceneActionType`, `FindSceneAction` and `GetActionCount` disagree with OStim's indexes (main.cpp:4566-4600).
  - `HasAction("customtype")` is false for action types an add-on uses without shipping a file.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Keep the action, give it the `default` type's sides (Default.json has `default`, line 614), and keep the original type id for queries. Clamp or ignore out-of-range roles when merging, but keep the entry so indexes match the file.

## 4. Action tags are not implemented: no "action tags" files, tag roles are never merged, and inverted "-tag" tags were dropped
- **Class**: Missing (not documented)
- **Severity**: Medium
- **OStim**: `data/.../action tags/*.json` (23 files) give each tag per-role attributes: factions, stat factions and lists. `SetupActions` loads them first. Every tag `T` also creates an inverted `-T` with actor and target swapped (GraphTableActions.cpp:131-149). Each action's tags are resolved to these and merged into its roles with `mergeTags` (GraphTableActions.cpp:196-212; ActionAttributes.cpp:9-16; ActionActor::merge). OStim's own action files use inverted tags: vaginalsex and analsex have `-penilestimulation`; grindingfoot, grindingobject, grindingpenis and grindingthigh have `-vaginalstimulation`; tribbing has `-vulvalstimulation`; rubbingpenisagainstface has `-penilestimulation`.
- **4Stim**: There is no action-tag folder or loader (grepped src/ for "action tags" and ActionTag). Tags are plain strings (Actions.cpp `AddType`, `type.tags = LowerList`). Default.json leaves out every inverted tag, so `SceneHasActionTag(id, "-penilestimulation")` is false where OStim says true, and per-role tag semantics (who is penile-stimulated in vaginal sex) are lost. An OStim action file read as written keeps the `-` strings, but nothing gives them meaning. docs/ACTIONS.md doesn't mention action tag files.
- **Fallout 4 constraint?**: None. These are plain JSON and forms.
- **Suggested fix**: Load `Action Tags\*.json`, create the inverted `-tag` entries, and merge their roles into the action's sides. Restore the inverted tags in Default.json.

## 5. Action roles' factions, stat factions and lists, and custom data (ints, floats, strings...) are not read, and nothing can query them
- **Class**: Partial (factions and stats are documented as "skipped until the features they belong to come", ACTIONS.md "OStim's layout"; custom data isn't mentioned anywhere)
- **Severity**: Medium
- **OStim**: `parseActionActor` reads `faction`, `statFaction`, `playerStatFaction`, `climaxStatFaction`, `partnerClimaxStatFaction`, `playerClimaxStatFaction`, `playerPartnerClimaxStatFaction`, `playerStatList`, `playerClimaxStatList`, `playerPartnerClimaxStatList`, `ints`, `intLists`, `floats`, `floatLists`, `strings` (values lowercased) and `stringLists` (GraphTableActions.cpp:13-121). They merge into GraphActor (GraphActor.cpp:7-27). Known users: OCum reads `stringLists.cum` (vaginalsex target `["vagina"]`), OEndurance reads `floats.stamina`, and voice sets use the factions. OMetadata has about 180 functions over this data (GetCustomActionActorInt..., FindActionsForMate..., HasRequirement...).
- **4Stim**: `Actions::Side` has none of these fields (Actions.h:20-33). FourMetadata.psc has 48 functions against OMetadata's 284 (diffed function names). Missing besides the custom-data group: FindAnyAction, FindActions*, FindAction*ForPerformer/Mate/Participant, GetActionTags, HasRequirement, GetActorTagOverlap, GetSceneTagOverlap, GetAnimationId. docs/API.md lists FourMetadata's contents and doesn't say it's a subset.
- **Fallout 4 constraint?**: None for custom data, which is plain JSON. Factions need forms in a 4Stim plugin; owner decision (4Stim ships no esp).
- **Suggested fix**: Parse and keep custom data on the sides now, since it costs nothing and add-ons need it. Bind the OMetadata custom-data and FindAction* families. Track factions and stats as a separate feature.

## 6. Speed carry-over and defaultSpeed differ from OStim
- **Class**: Different (not documented as a departure)
- **Severity**: Medium
- **OStim**: `relativeSpeed` starts at -1 (Core/Thread.h:135). On every `ChangeNode`, a node with one speed gets speed 0. Otherwise, if `relativeSpeed < 0` (thread start), the node's `defaultSpeed` is used. Otherwise the new speed is `round(relativeSpeed * (speeds-1))`, where `relativeSpeed = speed / speeds.size()` is updated only on non-transition nodes (Thread.cpp:316-323, 485-491). So a scene starts at its `defaultSpeed`, and navigation, transitions, climax auto transitions and sequences map speed proportionally. Note OStim's quirk: top speed 3 of 4 lands on index 2.
- **4Stim**:
  - A new scene always plays `speeds[0]` (PlaySceneIdles main.cpp:4446-4447), so `defaultSpeed` is ignored at start. Only auto mode uses it (main.cpp:2366); SCENES.md describes `defaultSpeed` as auto mode only.
  - Navigation keeps the absolute speed, clamped (main.cpp:5737).
  - A transition's destination gets `carrySpeed`, the absolute speed before the transition (main.cpp:1387-1393, 2228).
  - Climax scenes are forced to speed 0 (main.cpp:3825).
  - OStim-format sequence entries are forced to the slowest speed (`speed` default 1 becomes 0, SceneRegistry.cpp:809). OStim sequence entries have no speed; they follow the relative speed.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Port OStim's `relativeSpeed` logic into one place (PlayOnActiveScene): defaultSpeed at start, proportional mapping elsewhere. Owner decides whether to copy the rounding quirk. Leave a sequence entry's speed unset unless the file gives one.

## 7. Navigation stays available during sequences (OStim hides it); fadeOnEntry is ignored on manual navigation
- **Class**: Different (the transition case is documented; the sequence case is described in SCENES.md but not called a departure; fadeOnEntry is described wrongly as OStim's behaviour)
- **Severity**: Medium
- **OStim**: SceneMenu::BuildMenuData shows no options while the node is a transition, the thread is in a sequence, or NO_PLAYER_CONTROL is set (UI/Scene/SceneMenu.cpp:130-133). `Thread::Navigate` warps with `fadeOnEntry`: a manual pick of a scene marked fadeOnEntry fades to black for 0.7 s and back after 0.55 s (Thread.cpp:198-204, ThreadNavigation.cpp:98-114).
- **4Stim**: During a sequence the HUD lists the options, and picking one stops the sequence (SCENES.md "Sequences"; AdvanceAutoplay main.cpp:2178-2186). `fadeOnEntry` is honoured only by auto mode (main.cpp:2336-2343). `NavigateSceneImpl` never fades (main.cpp:5704-5739). SCENES.md says fadeOnEntry is "OStim's: ... fade to black when auto mode moves to this scene", which is incomplete.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Hide navigation during sequences as OStim does, or document it as a departure. Fade on manual navigation into `fadeOnEntry` scenes, reusing the fade code in AutoGoTo.

## 8. Sequences are checked against the first scene's roles only; the furniture rule is stricter
- **Class**: Different
- **Severity**: Medium
- **OStim**: `Sequence::fulfilledBy` requires every scene's roles to accept the actors (Graph/Sequence.cpp:5-12), at random selection (GraphTableSequences.cpp:139-152) and at play (ThreadNavigation.cpp `playSequence`). At load each scene must have the same actor *types* as the first (`hasSameActorTpyes`). The furniture may be a sub- or supertype; the sequence takes the most specific (GraphTableSequences.cpp:55-63).
- **4Stim**: `seq.actors` copies the first scene's roles. Later scenes only merge their sex (SceneRegistry.cpp:774-796), so later scenes' requirements (penis, vagina...) and actor `type` are never checked. ListSequences filters on these partial roles (SceneRegistry.cpp:1131-1150), and sequence steps play without a fit check (PlayOnActiveScene main.cpp:2133-2139). As a result, a sequence can be offered to a cast that can't play its later scenes. Furniture must be identical strings (SceneRegistry.cpp:770). The sex-clash rejection is 4Stim's own rule.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Merge every scene's requirements into `seq.actors` and check `type` equality at load (or run Fits on each entry). Accept furniture types where one is a child of the other, as OStim does.

## 9. listIndividually means something else in 4Stim, and furniture isn't grouped by list type
- **Class**: Different (FURNITURE.md attributes the wrong meaning to OStim)
- **Severity**: Medium
- **OStim**: `findFurniture` groups each object under `getListTypeInternal()`: the type itself if `listIndividually`, otherwise the nearest ancestor that is. It keeps the nearest object per list type and skips list types with no scenes for this actor count (`hasNodes(master, actorCount)`) (Furniture/Furniture.cpp:14-59; FurnitureType.cpp `getListTypeInternal`). So single beds, double beds and bedrolls all show as one "Bed" row; bench gets its own row; tablelean shows as "Table". The thread's furniture type is still the specific one.
- **4Stim**: FindNear keeps one row per *own* type, and for `listIndividually` types every piece gets a row (Furniture.cpp:771-781; doc FURNITURE.md "listIndividually (list every piece of this type in the picker, not just the nearest)"). With OStim's type files that gives one row per subtype, and a row for every bed in a room.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Implement `getListTypeInternal` and group by it. Keep the nearest piece per list type. Fix the doc.

## 10. OStim furniture type `rotation` is in radians; 4Stim reads it as degrees
- **Class**: Different (a bug for OStim-layout files)
- **Severity**: Medium
- **OStim**: Furniture types README: "rotation (float): rotates the scene (in radians)". The data uses `3.14159` (bedroll, singlebed, doublebed, wardrobethick, wardrobethin). It is added straight to `center.r` (Furniture.cpp `getOffset`, ThreadFurniture.cpp:56-63).
- **4Stim**: `type.rotation = entry.value("rotation", 0.0F) * DEG` for all files, OStim layout included (Furniture.cpp:234). An OStim type with 3.14159 turns the scene by 3.14 degrees instead of 180.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: In files from `Furniture Types\` (OStim layout), read `rotation` as radians. Keep degrees for 4Stim's `types` files and say so in FURNITURE.md.

## 11. Furniture conditions are partly skipped, and the documented reason is weak; multiple anykeyword conditions are ORed
- **Class**: Documented departure (reason weak) + Different
- **Severity**: Medium
- **OStim**: Condition types are `anykeyword`, `keywordblacklist`, `formlist`, `formblacklist`, `markercount` (sit or sleep markers, `minZ`/`maxZ`, equals or greater) and `cellname`. All conditions must hold (FurnitureTable.cpp:61-210). Keywords are checked on the reference, not only its base.
- **4Stim**: Only `anykeyword` and `keywordblacklist` are read. Every `anykeyword` list is merged into one "any of" list, so two anykeyword conditions become an OR (Furniture.cpp:177-190). The others log "isn't supported in Fallout 4". FURNITURE.md says "OStim's other conditions are about Skyrim's furniture markers". That isn't so for `formlist`, `formblacklist` or `cellname`, which are engine-generic. Fallout 4 also has `BSFurnitureMarker` entries in the NIF's "FRN" extra data with sit and sleep types, which 4Stim already reads (Furniture.cpp:362-373). Keywords are checked on the base form only (`HasAnyKeyword` on the base).
- **Fallout 4 constraint?**: No hard constraint. FormLists, cell names and NIF furniture markers all exist in Fallout 4.
- **Suggested fix**: Implement `formlist`, `formblacklist` and `cellname` directly, and `markercount` from the FRN markers (animation type and z range). Keep conditions as a list ANDed together, as OStim does.

## 12. Annotations missing: Undress, Redress, UndressPartial, RedressPartial, RemoveWeapons, AddWeapons, SetLooking, UnsetLooking, ResetLooking
- **Class**: Missing (partly documented: TODO.md plans to turn `OStimUndressPartial` into 4Stim-only `undress` steps; the rest isn't mentioned)
- **Severity**: Medium
- **OStim**: Thread.cpp:857-899 handles `OStimUndress`, `OStimRedress`, `OStimUndressPartial.<hex mask>`, `OStimRedressPartial.<hex mask>`, `OStimRemoveWeapons`, `OStimAddWeapons`, `OStimSetLooking.<left>[,<up>]`, `OStimUnsetLooking` and `OStimResetLooking`. These are all listed in data/.../list of annotations.txt.
- **4Stim**: The dispatcher handles only `4StimClimax`, `4StimEvent`, `4StimPeak`, `4StimPlayExpression`, `4StimBendSoS`, `4StimResetSoS` and `4StimEquipObject`/`4StimUnequipObject` (main.cpp:7263-7345). EVENTS.md's annotation table lists only those. The scene-file `undress` list (SceneRegistry.cpp:443-462) is a 4Stim-only replacement with no redress or weapons counterpart.
- **Fallout 4 constraint?**: None. Animation annotations reach the BSAnimationGraphEvent sink 4Stim already hooks. Skyrim slot masks need the existing slot mapping (Actions.cpp `FalloutSlotsFor`). Eye look maps to EyeLook.cpp.
- **Suggested fix**: Add the nine handlers under the 4Stim prefix. Map Skyrim slot masks through FalloutSlotsFor. SetLooking, UnsetLooking and ResetLooking go to EyeLook, with ResetLooking going back to the scene's lookUp and lookLeft.

## 13. Default.json differs from OStim's action data in ways the docs don't mention
- **Class**: Different
- **Severity**: Medium-Low
- **OStim** vs **4Stim** (data/.../actions/*.json vs Data/F4SE/Plugins/4Stim/Actions/Default.json, diffed by script):
  - `malemasturbation` peak: OStim has inverse false (closest hand-to-genital); 4Stim has `"inverse": true` (Default.json, around 1510). EVENTS.md mentions only the handjob as inverse.
  - `strokingtail` target: OStim has no requirement (it uses `equipObject: tail`); 4Stim adds `requirements: ["tail"]`, which hides those scenes from everyone (ACTIONS.md: nobody has `tail`).
  - `bathing` target: OStim requires `inwater`; 4Stim dropped it, so bathing scenes play on dry land.
  - `equipObject` dropped: `reading` (`book`) and the tail actions (`tail`).
  - Peaks dropped for `analtailsex` and `vaginaltailsex`.
  - Inverted tags dropped (see finding 4).
- **Fallout 4 constraint?**: Tails, partly. Books and in-water checks exist in Fallout 4.
- **Suggested fix**: Regenerate Default.json from OStim's files with only the documented substitutions (bones, sounds, slots), and list each deliberate change in ACTIONS.md.

## 14. Furniture types: `faction` and `offset*Global` are not read; in-use furniture and the `NoStim` keyword are not excluded
- **Class**: Missing (not documented)
- **Severity**: Low-Medium
- **OStim**:
  - `faction` (one or a list) on a furniture type. Actors are put in the factions of the type and all its supertypes while the scene is on it (FurnitureTable.cpp:34-50; FurnitureType.cpp `getFactions`; ThreadFurniture.cpp:92; ThreadActor.cpp:534). Every base type has one, for voice-set and dialogue conditions.
  - `offsetXGlobal`, `offsetYGlobal` and `offsetZGlobal`: global variables added to the offsets (used by singlebed and doublebed, Furniture.cpp `getOffset`).
  - `findFurniture` skips objects whose base has the `NoStim` keyword (Furniture.cpp:22-25) and, with `inUseCheck`, objects in use, for example an NPC sitting (`object.isInUse()`, FurnitureTable.cpp:232-239).
- **4Stim**: ParseType reads none of the four fields (Furniture.cpp:153-241). FindNear excludes only furniture another 4Stim scene is on. There's no in-use or keyword check (Furniture.cpp:733-800).
- **Fallout 4 constraint?**: Factions and globals need forms (owner decision on a plugin). Furniture in use (`IsOccupied`, or the marker-in-use flags) and keyword checks are available in Fallout 4.
- **Suggested fix**: Skip occupied furniture and a `NoStim` keyword now. Parse `faction` and `offset*Global` as `{mod, formid}` and apply them when forms are available.

## 15. Navigation building: no duplicate removal, no furniture-compatibility drop, transition chains checked only one step, default label and icon differ
- **Class**: Different
- **Severity**: Low-Medium
- **OStim**: `addNavigations` (GraphTableNodes.cpp:37-133):
  - It drops a link whose destination is already linked (`alreadyExists`, :68-78), so `origin` plus `destination` written twice gives one row.
  - It drops links between furniture types where neither is a child of the other (:56-59).
  - It resolves transition chains into `Navigation.nodes`. `Navigation::fulfilledBy` checks every node, and the menu checks furniture against the final node (SceneMenu.cpp:136-139).
  - The menu row's title is the final destination's `scene_name` (SceneMenu.cpp:140).
  - With no `icon`, it picks one from the destination's tags and actions (`LegacyUtil::getIcon`: missionary_mf, blowjob_mf... or placeholder, LegacyUtil.cpp:472-501).
- **4Stim**:
  - No duplicate check (SceneRegistry.cpp:887-909), so duplicate rows appear when two packs link the same pair.
  - Only actor counts are compared at load.
  - The HUD checks only the immediate `to` scene (HUD.cpp:1018-1021, `FocusedCanPlay(*dest)`), so a transition into a scene the cast can't play is offered.
  - With no `description`, the label is the transition's own name, not the final scene's (SceneRegistry.cpp:905-907).
  - With no icon, the row has none (HUD.cpp:1022 falls back to the 4Stim-only scene `icon`).
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Add the `alreadyExists` and furniture checks. Store the settled destination with each navigation and run FocusedCanPlay on it. Use the settled scene's name as the default label. Port `getIcon`'s tag and action table as the default icon.

## 16. Graph lookups the API relies on are missing or behave differently
- **Class**: Partial (API.md admits the ForAll and actor/target variants are missing, but says `FindRandomScene` "covers them", which it can't: it has no action-role parameter)
- **Severity**: Low-Medium
- **OStim**:
  - `OLibrary.GetScenesInRange(id, actors, distance)` is a breadth-first search over navigations, sorted by name (Node/NodeNavigation.cpp:5-31; ScriptAPI/LibraryScript.cpp:20-38).
  - Random selection matches actors in the order given (`Node::fulfilledBy` index by index, Node.cpp:63-77), from a list keyed by master furniture type and actor count (GraphTableNodes.cpp:10-34, 183-209).
  - `GetRandomScene*ActionForActor`, `ForTarget` and `ForActorAndTarget`, and `MultiActorTag...ForAll`.
  - `OEvent.IsChildOf` (Papyrus/PapyrusEvent.h:9-10).
  - `OData.ReloadScene(sceneId)`: reloads one scene in place (GraphTableReloadNode.cpp; OData.psc:64-70).
- **4Stim**: FourLibrary.psc lacks 58 of OLibrary's functions (diffed), including GetScenesInRange and every ForActor/ForTarget/ForAll and Superload variant. `RandomScene` accepts any role order (main.cpp:7440-7451 via AssignRoles), so it can return a scene the given order doesn't fit, which OStim never does. There's no IsChildOf for events and no OData script. Reload is all-or-nothing (`FourStim.ReloadScenes`), which is a superset but not under OStim's name.
- **Fallout 4 constraint?**: None.
- **Suggested fix**: Add GetScenesInRange using main.cpp's `Reachable`. Add action role parameters to FindRandomScene and wrap the missing names. Respect the given actor order in the OLibrary wrappers. Add `FourEvent.IsChildOf`, and a `ReloadScene(id)` that reloads everything.

## 17. Smaller format gaps when reading OStim files unmodified
- **Class**: Different
- **Severity**: Low
- An action without `actor`: OStim warns, and the roles stay -1, so the action applies to no one. 4Stim defaults to role 0 (SceneRegistry.cpp:569).
- Navigation `border` is validated as hex in both, but 4Stim treats `ffffff` as "no border" (HUD.cpp:140-144) while OStim draws a white border. That's a rendering choice; noted only.
- OStim's README calls the furniture field `"multiply scale"`, but its code reads `multiplyScale`; 4Stim matches the code. Fine.
- Furniture type `name` given as an OStim translation key (`$ostim_furniture_bed`) shows the id unless 4Stim's translations define the key (Furniture.cpp:156-159). OStim's furniture keys aren't in 4Stim's translation files (unverified; Translations auditor).
- OStim also loads legacy OSex XML scenes (`Data\Meshes\0SA\mod\0Sex\scene`, Util/LegacyUtil.cpp:412-470, called from Core.cpp:51). No Fallout 4 content exists in that format, so this is Not considered but harmless. Mention it in SCENES.md.

---

## Checked and matching
- Scene fields read with OStim's names and defaults: name (translated, `$<id>` fallback), modpack, length, destination, transition-level `origin` with priority, description, icon, border and noWarnings, navigations (destination, origin, priority, description, icon, border, noWarnings), speeds (animation becomes `<name>_<index>` Idle editor IDs, playbackSpeed, displaySpeed), defaultSpeed (parsed), noRandomSelection, fadeOnEntry, furniture, offset {x,y,z,r in degrees} added to every actor, scaleOffsetWithFurniture, tags (lowercased for OStim files), scene and actor autoTransitions.
- Actor fields: type, intendedSex (male, female, otherwise any), sosBend and tngBend, scale, scaleHeight (120.748), underlyingExpression, expressionOverride, expressionAction, animationIndex (default = index), singleSpeed, noStrip, lookUp/lookDown and lookLeft/lookRight precedence, offset, requirements, tags, feetOnGround (default: standing or squatting tag).
- Action fields in scenes: type through aliases, actor, target and performer defaults (performer and target = actor), muted, doPeaks, peaksAnnotated. Requirements, moan, talk, muffled and equipObject merge into roles, and the first expressionOverride wins, as in GraphActor::merge and getOverrideExpressions.
- Automatic tags: `transition`; `gay` and `lesbian` only with 2 or more actors and every intendedSex given.
- Navigation: a transition's own `navigations` are ignored; links are sorted by priority with a stable sort; noWarnings silences missing-scene warnings; actor-count mismatch is dropped; `{n}` labels are filled with actor names.
- Transitions move on after `length` (plus the documented fTransitionLead and IdleStop) and chain. Climax auto transition uses the actor-level `climax` key only, as OStim's ThreadActorClimax.
- Peaks: bonedistance with 1 s initial cooldown, running min and max, 10%/90% thresholds, inverse, multi-bone minimum distance. Loop interval scaled by displaySpeed. Annotated peaks gated by doPeaks and peaksAnnotated. Peak sounds fire on peaks. minRange is a documented 4Stim addition.
- Events: one file per event, roles stimulation, maxStimulation and reactionDelay, sound, camera shake and rumble clamped, tags, supertype inheritance, `4StimEvent` 2-arg and 4-arg payloads.
- Furniture: supertype chain `IsA` equals OStim's isChildOf, including `"none"` as the supertype that lets floor scenes play on beds; priority picks the type; ignoreMarkerOffsetX/Y/Z; offsetX/Y/Z in the object frame scaled by object scale; multiplyScale; scene random selection on a bed rejects standing roles; the furniture-change flow (intro or idle).
- Random selection excludes transitions and noRandomSelection, as getRandomNode and getRandomNodeInRange do.
- Role matching (`Fits`) matches ActorCondition::fulfills: type equality first, unrestrictedNavigation, intendedSexOnly with "any" on either side, requirements subset.
- Files: scenes folder searched recursively with id = file stem; actions, events and sequences non-recursive; furniture types recursive. A later duplicate wins with a warning, deterministic where OStim's order isn't.
- Hot reload: re-reads actions and scenes; running scenes hold shared pointers and look up by id, so it's safe mid-scene (a superset of OStim's single-scene reload).
