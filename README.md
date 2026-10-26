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

**3.6 % of `bully.exe`'s code recreated** (546 of 20,819 functions), 7.8 %
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
played node ends: the button-sequence lesson, the grapple and its charge go through (`--demander`, `--sequence`). Since the third session of 22.10 the passes come from the tracks themselves, as in the engine: `Opportunity` (and its random latch), `Sequence` and `PlayOnTarget`, with their priority rule; the pushed bully plays `PushFwd/RCV` and the grapple lesson ends. Since the fourth session of 22.10 hits land: `Collision` tracks (the hit reaches the locked target, as the tool has no bones), the target's reaction through the hit tree (`ConditionFlag`, `MePlaying`), a `Sequence` that leaves the set tree, and animation ends read from `MAINPED.HXD`; once pushed, the bully falls, and the heavy ground kick makes him play `GroundHitHeavy`: the ground-strike lesson goes through and `1_02` shows its fight objective. Since the session of 23.10 hits hurt: an `Execute` adds the target node's tracks to the controller's own list, as the engine does, and the `DamageHandle` of `HitTree/…/Base/HitBase` takes the received hit's damage; peds get their `pedstats.dat` entry (health, regeneration, humiliation threshold), the `SetHealth`, `SetPedFlags` and `SetTargetAsGrappleTarget` tracks are played and eleven hit conditions are judged (`HitTime`, `Health`, `IsHealthAtHumiliationLevel`…); in `1_02` the bully gets back up, eleven jabs bring him down to 5 health and the mission asks to humiliate him (`1_02_HumiliateHim`). Since the evening of 23.10 **`1_02` ends passed** (119 s of play): the victim's humiliation group (`+0xd44`, picked by size once its health drops below half, through the `ManageTargets` track) is read by `HumiliationGroupIs`; an `Opportunity`'s own conditions are judged (the file word reads 0, the second back-reference table points to a condition list: 951 of the 3,080 boot-time opportunities have one); animation groups (`LoadAnimationGroup`, `AnimationGroupLoaded`), held speech lines (`SpeechTrack`, `SpeechStartTrack`, `IsSpeechPreloaded`), `TargetRelativePosition`, `FaceTarget` and `SeekEntityPosition` are played, and `PedFlags` is judged. The bully plays the real `5-8_NoogieSpit` humiliation, loses his last points through `HumiliateFlee`, plays `HitTree/KnockOuts`, and `F_SetupRussellFight` runs the Russell scene before `MissionSucceed`. Since the night of 23.10 the bully's **AI tree is played** instead of a scripted attack gesture: `PedSetAITree` sets a second controller per ped, `PedAttack` pushes a real `AttackPedObjective` onto the ped's objective queue, eighteen AI conditions are judged (`Objective`, `AcquireTarget`, `JoinCombatZone`, `InCombatZone`, `Interval`, `WeightedRandom`, `ActionNodeRequest`…; `LuaCond` now reads the script's result as a number, so `0.0` is false), and the `ManageTargets`, `CombatLoco`, `ActionNodeRequest`, `spawn` (with its own controller), `ClearCurrentObjective` and `FleeObjective` tracks are played; a node lives while one of its tracks with byte +8 is alive, as in the engine. In `1_02` the bully keeps to his 1.5 m combat zone, his `CombatActions` loops, the humiliation group comes from the `ManageTargets` update, the humiliation hands control back to the player's tree, and the fleeing bully falls without hitting the player: `1_02` still ends passed (119.03 s).
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
