# bully-re

*English version. [Version française](README.fr.md).*

A from-scratch reimplementation of the **Bully: Scholarship Edition** (PC, 2008)
engine, written by reading the original executable — in the spirit of
[re3](https://github.com/GTAmodding/re3) and reVC for the GTA games.

Bully was built by Rockstar Vancouver on Rockstar North's GTA codebase, running
on Gamebryo 2.3 instead of RenderWare. That makes the GTA decompilations a
usable dictionary for roughly half the engine, and leaves the other half —
Gamebryo, and Bully's own action-tree AI — to be worked out from the binary.

**The game is not included and never will be.** This repository holds
hand-written code, analysis tools and documentation only. You need your own
copy of the game (Steam or disc) for anything here to run. Nothing derived from
the executable — no decompiler output, no extracted assets — is committed.

## What is known so far

- `bully.exe` v1.0: 8 MB, unprotected, RTTI intact, Gamebryo 2.3, Lua 5.
- 20,819 functions, 4.4 MB of code, **1,522 classes** rebuilt from RTTI
  ([docs/classes.md](docs/classes.md)), 66 inheritance roots.
- The object model is GTA's: `CPlaceable → CEntity → CPhysical → CVehicle →
  CBike`, `CPed`, `CPlayerPed`. Virtual method names and slot order from
  re3/reVC are the starting point — and are verified, not assumed, because the
  order does drift.
- On top sits Bully's own action-tree system (`ActionNode`, `Condition*`,
  `*Track`, `*Objective`) driving characters and activities.
- The binary keeps **914 Lua API names** in `{name, C function}` tables. That
  bridge names engine functions for free and is the single most productive
  lever in the project ([docs/api-lua.txt](docs/api-lua.txt)).

## What is reimplemented, and verified byte for byte

Every data loader has a host test (`tests/construire_tests.sh`, then
`BULLY_DATA=<game root> build/tests/test_…`) that reads the real game files and
compares the result against values taken by hand from the originals.

| Subsystem | Coverage | Notes |
|---|---|---|
| `.img` / `.dir` archives | `World.img` 11,980 entries | [src/core/CdStream.h](src/core/CdStream.h) |
| Binary model definitions `.idb` | 13 sections, all 77 files | [docs/idb.md](docs/idb.md) |
| `handling.cfg`, `*.dat` | cars, bikes, boats, gearbox, game-unit conversion | [src/vehicles](src/vehicles) |
| Collision COL3 / COL2 / COLL | 488 files, 3,863 models, exact sizes | [docs/collision.md](docs/collision.md) |
| Binary placements `Ipl$` | 85 files, 11 section types | [src/core/IplFile.h](src/core/IplFile.h) |
| Gamebryo NIF 20.3.0.9 | 5,724 files, 363,085 known blocks decoded | [docs/nif.md](docs/nif.md) |
| Textures `.nft` | 4,469 files, all 35,635 textures decoded (DXT1, DXT5, RGB, RGBA, palette) | [docs/nft.md](docs/nft.md) |
| Streaming ids, pools | id ranges, 28 pools with entry sizes | [docs/streaming.md](docs/streaming.md), [docs/pools.md](docs/pools.md) |
| World grid and lists | 36x36 sectors, 32-bit list nodes, 10 entity pools | [docs/world.md](docs/world.md) |
| NIF node transforms | rotation convention settled against 3,842 collision boxes | [docs/nif.md](docs/nif.md) |

Some findings along the way: 247 of the NIF files are **big-endian**, left over
from a console export; Bully ships **Lua 5.0**, not 5.1; and the `.idb` format
is the GTA IDE text format compiled to binary, with the section tags stored
backwards.

The biggest departure from GTA is the world's linked lists. Where re3 has a
three-pointer node, Bully packs a whole node into **one 32-bit word**: 4 bits
of entity pool, 14 bits of index in that pool, 14 bits for the next node's
index. A sector therefore stores handles, not pointers, and fits five lists in
20 bytes. See [docs/world.md](docs/world.md).

## Building

No game code is needed to compile what is here:

```sh
./construire.sh              # compiles every source file to an object
./tests/construire_tests.sh  # builds the host tests
BULLY_DATA=/path/to/Bully build/tests/test_nif
```

`construire.sh` uses `g++` only, no build system. Most tests need a real
installation of the game to read from; they never write to it. The seven tests
that read no game file (`tests/sans_jeu.sh`) run on every push and pull request
(`.github/workflows/tests.yml`), after compiling every source and every test.

## Method

1. **Map first.** RTTI, virtual tables, file formats, Lua bindings.
2. **Reimplement by subsystem**, starting with what can be tested without a
   renderer: data loaders, `CPlaceable`/`CEntity`, vehicles.
3. **Verify** each piece against the real files, with a test that fails loudly.
4. **Gamebryo is rebuilt as needed**, only the parts the game actually calls.

## Tools

- lip sync `.lip` files, 493 files and 40,882 lines, 2 bits per frame read exactly as `bully.exe` does ([docs/lip.md](docs/lip.md));
- `tools/ghidra/` — headless scripts: inventory, RTTI export, per-vtable
  decompilation, Lua binding extraction, string cross-references.
- `tools/indexer.py` + `tools/chercher.py` — build a local SQLite index of your
  own analysis output and query it by offset, global, string, caller or
  full text. The index is never committed.
- `tools/generer_squelettes.py` — generates `docs/classes.md` and
  `src/squelettes/` from the RTTI dump.

Analysis runs in a container (`blacktop/ghidra`); the Ghidra project stays
outside the repository.

## Status

**8.3 % of `bully.exe`'s code recreated** (1303 of 20,819 functions), 12.9 %
understood (code, docs, names). Breakdown in [docs/avancement.md](docs/avancement.md),
regenerated on every commit by `tools/avancement.py`.

Early. Everything that loads from disk is done and tested. The first visible
step is there: `outils/nif2obj` exports any model with its textures to OBJ,
following the game's own chain (model → texture dictionary → NIF node tree →
DXT decode), `outils/rendu` draws one with a dependency-free software
rasterizer, and `outils/scene` draws a whole zone from the game's own
placement files (`docs/ipl.md`). `outils/visite` walks through a zone in
real time, first person, still fully software-rendered at 400 × 240 (the New
3DS top screen) with perspective-correct texturing, near-plane clipping and
the game's own alpha modes; SDL2 only opens the window
(`docs/rendu-temps-reel.md`). Press F to walk instead of fly: the game's own
collision volumes carry you on the floor, up steps and against walls.
Outdoors, neighbouring zones load and unload as you move (`outils/carte`
maps the 85 placement files), so you can go from the school to the
business district. `--pietons n` places the game's pedestrians, deformed by
their own skeleton and **animated** with the game's own animations (the
`.agr` animation format is worked out, [docs/agr.md](docs/agr.md), in French):
they **walk** around, on the floor and against the walls.

The game's own **Lua scripts run** in `outils/lancer_lua`, on the original Lua 5.0.2
VM: `main.lua` and its threads frame by frame, the mission bank read from the
action trees, and the first mission, `1_01`, played to `MissionSucceed` — its
cutscene `1-1-1` from `Cuts.img` (61.7 s), the area changes through the engine's
own state machine (fade, area scripts unloaded and loaded, doors paired by
`DATInit`), the scripted speech, the pedestrians it creates and moves. Since 21.10,
on a new game, **`1_02` follows on its own** after `1_01` (mission node conditions
read: `MissionCond`, `TimeCond`, `DayCond`, `NOT`, `OR`), its narration is preloaded and
lasts as long as the real samples in `Speech.bin`, the player leaves the school
through a `DT_` door, and the mission runs up to the training fight with Russell.
Since 22.10 the player's action controller descends into the tree that `PedSetActionTree` sets
(the 68 global action trees are loaded at start-up, as the engine does), so the training starts,
and with `--jouer` (block, then `0_BLOCK_0`) the blocking lesson goes through. Since the second session of 22.10
the action nodes' conditions are judged (`ConditionGroup`, `WeaponEquiped`, `ActionRequest`, `ActionNodePlayTime`,
`TargetPlaying`, `LuaCond`) and each pedestrian has an action controller that steps back into its tree when the
played node ends: the button-sequence lesson, the grapple and its charge go through (`--demander`, `--sequence`). Since the third session of 22.10 the passes come from the tracks themselves, as in the engine: `Opportunity` (and its random latch), `Sequence` and `PlayOnTarget`, with their priority rule; the pushed bully plays `PushFwd/RCV` and the grapple lesson ends. Since the fourth session of 22.10 hits land: `Collision` tracks (the hit reaches the locked target, as the tool has no bones), the target's reaction through the hit tree (`ConditionFlag`, `MePlaying`), a `Sequence` that leaves the set tree, and animation ends read from `MAINPED.HXD`; once pushed, the bully falls, and the heavy ground kick makes him play `GroundHitHeavy`: the ground-strike lesson goes through and `1_02` shows its fight objective. Since the session of 23.10 hits hurt: an `Execute` adds the target node's tracks to the controller's own list, as the engine does, and the `DamageHandle` of `HitTree/…/Base/HitBase` takes the received hit's damage; peds get their `pedstats.dat` entry (health, regeneration, humiliation threshold), the `SetHealth`, `SetPedFlags` and `SetTargetAsGrappleTarget` tracks are played and eleven hit conditions are judged (`HitTime`, `Health`, `IsHealthAtHumiliationLevel`…); in `1_02` the bully gets back up, eleven jabs bring him down to 5 health and the mission asks to humiliate him (`1_02_HumiliateHim`). Since the evening of 23.10 **`1_02` ends passed** (119 s of play): the victim's humiliation group (`+0xd44`, picked by size once its health drops below half, through the `ManageTargets` track) is read by `HumiliationGroupIs`; an `Opportunity`'s own conditions are judged (the file word reads 0, the second back-reference table points to a condition list: 951 of the 3,080 boot-time opportunities have one); animation groups (`LoadAnimationGroup`, `AnimationGroupLoaded`), held speech lines (`SpeechTrack`, `SpeechStartTrack`, `IsSpeechPreloaded`), `TargetRelativePosition`, `FaceTarget` and `SeekEntityPosition` are played, and `PedFlags` is judged. The bully plays the real `5-8_NoogieSpit` humiliation, loses his last points through `HumiliateFlee`, plays `HitTree/KnockOuts`, and `F_SetupRussellFight` runs the Russell scene before `MissionSucceed`. Since the night of 23.10 the bully's **AI tree is played** instead of a scripted attack gesture: `PedSetAITree` sets a second controller per ped, `PedAttack` pushes a real `AttackPedObjective` onto the ped's objective queue, eighteen AI conditions are judged (`Objective`, `AcquireTarget`, `JoinCombatZone`, `InCombatZone`, `Interval`, `WeightedRandom`, `ActionNodeRequest`…; `LuaCond` now reads the script's result as a number, so `0.0` is false), and the `ManageTargets`, `CombatLoco`, `ActionNodeRequest`, `spawn` (with its own controller), `ClearCurrentObjective` and `FleeObjective` tracks are played; a node lives while one of its tracks with byte +8 is alive, as in the engine. In `1_02` the bully keeps to his 1.5 m combat zone, his `CombatActions` loops, the humiliation group comes from the `ManageTargets` update, the humiliation hands control back to the player's tree, and the fleeing bully falls without hitting the player: `1_02` still ends passed (119.03 s). Since 26.10 the action trees' **mounted references** are read: a referenced file (`Locomotion.act`, used by 42 files) exists once under `/Global`, and its parent depends on the context key, the name of the top-level tree (`FUN_005f6960`, `FUN_005f6800`, `FUN_005f6290`); `LuaCond` calls its function inside the mission's namespace (`NSCall`): during the block lesson the bully really attacks the player (`Offense/Short/Strikes`, through `Locomotion.act`'s `ActionOpps`); ped type and sex, the motion intention, chapter and season, and sixteen more conditions are read. `1_02` still ends passed (119.03 s). Since 27.10 the attack phase held by the `Collision` track (`CollisionStart`, `AttackPhase`, `AttackTimer`), the HUD components, the camera manager, the controllers and the social state are read, with sixteen more conditions; `CombatLoco`'s speed comes from its step (`FUN_00625970`): the bully reaches his zone at a sprint. `1_02` still ends passed (119.03 s). On the evening of 27.10 the AI's speed rules are read (inputs cleared by `FUN_005f38e0` and the `IDLE` track, the navigation's speed `FUN_005e4a60`, `CombatLoco`'s four sub-states, stopping at the small radius: the bully circles the player facing him), AI peds press buttons (`ActionRequest` reads the raw button), and **the bully blocks**: button 12 sends him into `Defence/Block`, and a hit taken there goes to `HitTree/Standing/BlockHits`; a taken `Opportunity` ends its node even without a target node. Eight more conditions are judged (`WeaponNeedInit`, `ConversationActive`, `OnMission`, `MissionCond`, `PedIsInOutfit` with the player's wardrobe, `AttackerConditionGroup`…). `1_02` ends passed at 121.94 s, with fifteen jabs. On the night of 27.10 a ped no longer dies at zero health but through the `Death` track, as in the engine; `1_02` has no unread condition left (`MissionUnlocked`, `ParametricTime`, `PropTargetInteractive`), ped vision (the cone and range from `pedstats.dat`) and grapple strikes (`DamageTarget`) are read, and **`1_03` starts**: `PedFollowPath` and the `FollowPath` track walk the player and Davis along their paths, up to the chase the tool does not play yet. On 02.11 **`1_03` gets as far as the garage**: the tool plays the chase (`--suivre`, `--viser`), a received hit notifies the scripts (`RegisterPedEventHandler`), path-node actions, ped flags, stats and attitudes are read, and the AI picks its target as the engine does (`FUN_004992a0`); `1_02` still ends passed (121.94 s). On 04.11 **`1_03` ends passed** (148.90 s): trigger events (`RegisterTriggerEventHandler`, tracked every frame for every ped), the action trees' `Script` track (the garage door being opened calls into the mission), the player's own trees (`/Global/Player`, `/Global/PlayerAI`), `PlayerSetControl`, tethers, ignored stimuli and door state are recreated; with the tool's counterattack the player beats the bullies, opens both garage doors, beats the waves and hits Davis on his barrels until `CutsceneDavisBegs` and `MissionSucceed`. On 05.11 **`1_04` ends passed** (81.05 s): weapons (the ObjectButes weapon types, the inventory's shared pool, the weapon in hand taken through `Weapons/SelectActions` and its tracks), projectiles (`WeaponFire`, its ballistic launch speed, the marble and the brick), breakable objects from the `.ipb` spec entries with their hash events, prop action trees streamed from `Stream/World.img`, global event handlers and pickups are recreated; the player picks the slingshot, breaks three bus windows, climbs the tree and shoots the six jocks during their jumping jacks. `1_03` can now end with bricks thrown at Davis from afar (148.04 s), and `1_02` takes its real path, no longer a stub's (132.92 s). In the night of 05 to 06.11, **`1_05` ("Save Algie") reaches its third stage**: the prefects' patrols (the patrol path manager: the mission looped on `AreaDisablePatrolPath`), `PedIsHit`, the ally group (`PedRecruitAlly`, `PedDismissAlly`, eight conditions), the `TetherLoco` track that makes Algie follow, allies going through doors with the player, `PedSetMissionCritical`, `PedFindInAreaXYZ` and four pedestrian and speech functions are recreated; the player beats the first bully and the right-hand gang, enters the school with Algie, takes him to the second-floor toilet stall, beats the ambush, and Algie, hands washed, follows him to his locker. On 06.11 (second session), **`1_05` ends passed** (185.20 s): the triggers' animated objects (the locker: its PropButes sheet from `PropGlbl.cat`, its own action controller, `PAnimSetActionNode`, prop flags), `TargetProp` and a ped's survey of nearby props, the `Sync` and `PropInteract` tracks, the end-of-mission screen, mission objectives and a `PlayOnTarget` that waits for its target are recreated: Algie searches his locker, gets his books back and pays the player. `1_06_01` starts through its mission's `Run` node and waits for the player at the hobo's, and animated doors now go through their object. On 06.11 (third session), **`1_06_01` ("Get Help") ends passed** (61.81 s): the collectibles manager (`colctbls.dat`), the player's items, the pickups pool, the collectibles of the `.ipb` placements turned into pickups by the population, pickups taken on contact, and the button history of the fighting lessons with the `ActionRegisterButtonHistory` track are recreated: the player picks the transistor up on the heights, gives it to the hobo and passes the charged-punch lesson. `1_07` ("Defend Bucky") starts and gets past `PAnimRequest`. In the fourth session of 06.11, **`1_07` gets as far as the garage door**: scripted animated objects (`PAnimCreate`), `PedFindInAreaObject`, persistent entities and radar blips are recreated; the player joins Bucky, beats the four bullies and lifts the garage door. In the fifth session of 06.11, **`1_07` ends passed** (151.47 s): `PropTargetInteractive` (Bucky uses the toolbox), pickups dropped by peds, the by-kind target of `FaceTarget` and `SeekEntityPosition`, the secondary health bar, `PAnimDelete` and seven ped and player functions are recreated; the player beats the second wave, and the mission plays the hobo, the skateboard gift and the end screen. In the sixth session of 06.11 the rest of the weapons begins: the weapon charge (`WeaponSetCharge`, `WeaponIncrementCharge`, read by `WeaponFire`), weapon flags (`SetWeaponFlags`, `WeaponFlags`, `PedSetWeaponFlag`) and the mission timer (`MissionTimerStartFixed`, `MissionTimerPause`) are recreated. In the seventh session of 06.11 and the first of 07.11 it goes on: the action controller's time step, held buttons (`--tenir`, the slingshot's `Charge` sheet really played in `1_04`), shot settings and damage, the weapon object, `AttackTimerReset`, `HasRangedWeapon`, `DetectProjectile` and `DetectGrenade`, and the player's upper body in its own controllers (holding the fire button alone fires the first pellet of `1_04`). In the second session of 07.11, entities following a path are recreated (the path reference, the 0xd13730 manager, `PAnimFollowPath` and its four siblings, node callbacks), along with `IsPlayerFatigued`, leading a moving target, and the 32-slot projectile table with projectile lifetimes. In the third session of 07.11, `AreaClearAllPeds` is wired in (the mission commands are redone with it), along with `PedClearHitRecord`, `PedSetMinEngage` and the `EngageDistance` condition, the player's auto-aim flag (`WeaponPlayerFireAutoTarget`) and the aim offsets of `WeaponFire`. In the first session of 09.11, the player's aiming is rebuilt in full (`FUN_00662a40`): forced shot destinations (`PedSetProjDest`), where the target comes from (the track's +0x31 byte, the aim component and its fallback), the pad's button 10, the point in front of a shooter without a target, the aim error on targets that are not peds, and `PedTargetPAnim`; along with projectile lifetimes (`PedSetProjectileLifetime`), the projectiles' water test and spawn tracks that die with an empty controller. In the first session of 11.11, the action controller empties itself when a node ends with nothing to follow (`FUN_005f49a0`) and a node's length only comes from the tracks that hold it (`FUN_005f4230`): the slingshot's upper body starts again on the next button press and `1_04` breaks its three windows with button 8 alone (17.56 s, 19.54 s and 21.55 s); `1_05` now ends at 180.31 s and `1_07` at 150.88 s. Along with them, what happens to a projectile on contact: the impact (`FUN_004d3860`) and its explosion, the contact (`FUN_0046ad90`), the explosion table, a projectile's end (`FUN_004d45e0`) and impact marks (`FUN_00560f70`). In the first session of 12.11, a projectile's physics in the world (`CVolProjectile`): flight under gravity (`FUN_004d2160`, `FUN_00469760`; +0x124 is in metres per step), air (`FUN_004698c0`), rest (`FUN_00469a20`), the bounce (0.75 against a pedestrian, the elasticity +0x164 otherwise), the contact pair (`FUN_0046b720`), the per-frame control (`FUN_004d2290`), the push (`FUN_004d1ec0`), braking (`FUN_004cf350`), the dart that sticks and the deflected frisbee; the tool no longer removes a projectile on contact and `1_04` writes its bounces off the three windows (17.52 s, 19.50 s and 21.52 s). The mission tool still has no ground under its projectiles: rest and braking only show in the tests.
The tool plays the player (`--entrer`, `--aller`, `--zone`, `--porte`, `--jouer`, `--demander`, `--sequence`); everything
else answers like the engine does, or says it is a stub. See `docs/lua.md`, `docs/missions.md`,
`docs/zones.md`, `docs/cinematiques.md` (in French).

See [docs/journal.md](docs/journal.md) for the running log (in French).

## Next

See [docs/suite.md](docs/suite.md) (in French): the first open item is the next piece of work.

## Legal

This project contains no code, assets or data from Bully: Scholarship Edition.
It is an independent reimplementation written for interoperability and study,
distributed under the GPL-3.0. Bully is a trademark of Take-Two Interactive;
this project is not affiliated with or endorsed by them.
