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
| Gamebryo NIF 20.3.0.9 | 5,724 files, 286,403 known blocks decoded | [docs/nif.md](docs/nif.md) |
| Streaming ids, pools | id ranges, 28 pools with entry sizes | [docs/streaming.md](docs/streaming.md), [docs/pools.md](docs/pools.md) |

Some findings along the way: 247 of the NIF files are **big-endian**, left over
from a console export; Bully ships **Lua 5.0**, not 5.1; and the `.idb` format
is the GTA IDE text format compiled to binary, with the section tags stored
backwards.

## Building

No game code is needed to compile what is here:

```sh
./construire.sh              # compiles every source file to an object
./tests/construire_tests.sh  # builds the host tests
BULLY_DATA=/path/to/Bully build/tests/test_nif
```

`construire.sh` uses `g++` only, no build system. The tests need a real
installation of the game to read from; they never write to it.

## Method

1. **Map first.** RTTI, virtual tables, file formats, Lua bindings.
2. **Reimplement by subsystem**, starting with what can be tested without a
   renderer: data loaders, `CPlaceable`/`CEntity`, vehicles.
3. **Verify** each piece against the real files, with a test that fails loudly.
4. **Gamebryo is rebuilt as needed**, only the parts the game actually calls.

## Tools

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

Early. Everything that loads from disk is done and tested; nothing renders yet.
Next up: the `.nft` texture format, then drawing a first NIF mesh.

See [docs/journal.md](docs/journal.md) for the running log (in French).

## Legal

This project contains no code, assets or data from Bully: Scholarship Edition.
It is an independent reimplementation written for interoperability and study,
distributed under the GPL-3.0. Bully is a trademark of Take-Two Interactive;
this project is not affiliated with or endorsed by them.
