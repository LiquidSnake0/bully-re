# Les scripts Lua

Les missions, les événements du campus, les cinématiques et l'horloge du jeu sont
écrits en Lua. Le jeu en livre 515 compilés dans `Scripts/Scripts.img` (+ 52 dans
`Stream/World.img`), tous au format `.lur`. Établi le 08.10.2026.

Code : la VM d'origine dans `tiers/lua-5.0.2`, la sonde `outils/sonde_lua.cpp`,
l'extracteur d'API `tools/tables_lua.py`, la liste `docs/api-lua-tables.tsv`.

## La VM : Lua 5.0.2, nombres en float

- `bully.exe` contient la chaîne `Lua 5.0.2` ; les messages d'erreur
  (« cannot resume dead coroutine »…) sont ceux de cette version.
- L'en-tête de chaque `.lur` : `1b 4c 75 61 50 01 04 04 04 06 08 09 09 04` puis le
  nombre test `3b af ef 4b`. Lu dans l'ordre de `lundump.c` : version 0x50,
  petit-boutiste, `int` 4, `size_t` 4, `Instruction` 4, champs 6/8/9/9 bits, et
  **`lua_Number` de 4 octets** : le jeu compile Lua avec des nombres en `float`
  (le test 31415926,5 en float vaut `0x4befaf3b`). Format d'affichage `%.10g`.
- Le code de 5.0.2 est repris tel quel (`tiers/lua-5.0.2`, licence MIT), avec deux
  retouches marquées `bully-re` pour lire des `.lur` 32 bits sur un hôte 64 bits :
  `Instruction` en `unsigned int` (`llimits.h`) et `size_t` lu sur 4 octets
  (`lundump.c`, `LUA_UNDUMP_SIZE_T32`). Options de compilation dans
  `tests/construire_tests.sh`.
- Résultat : **les 515 scripts se chargent dans la VM d'origine**. Le jeu n'a donc
  modifié ni les opcodes ni le format du bytecode.

## Les bibliothèques ouvertes

Le binaire range ses liaisons dans des tables `{ nom, fonction C }` finies par
`{ 0, 0 }` (le `luaL_reg` de Lua 5.0). `tools/tables_lua.py` les parcourt toutes :

- 5 tables dans `.rdata` : les bibliothèques standard **base** (avec coroutine),
  **table**, **math** et **debug**. Ni `string`, ni `io`, ni `os` : un script de
  Bully ne peut ni manipuler de chaînes avec `string.*` ni toucher aux fichiers.
- 55 tables dans `.data` : l'**API du moteur, 1 504 fonctions** (`docs/api-lua-tables.tsv`).

L'ancienne liste de 914 noms (`docs/api-lua.txt`) venait d'une source extérieure :
253 de ses noms (`CameraActive`, `CameraFadeTrack`…) n'ont pas de table, ce ne sont
pas des fonctions Lua ; et il lui manquait `ImportScript`, `GetTimer`,
`GetMissionCurrentAttemptCount`, `EffectRegisterInArea` et 839 autres (843 noms nouveaux ; les 661 restants sont communs).

## ImportScript et util.lua

`ImportScript` du moteur (0x5be5f0) cherche le fichier dans `Scripts.img` par son
nom seul (`Library/LibTable.lua` → `LibTable.lur`), le **compile et renvoie la
fonction sans l'exécuter**. `util.lur` l'enveloppe aussitôt :

```lua
shared = {}
local moteur = ImportScript
function ImportScript(f) moteur(f)() end
function GlobalImportScript(f) moteur(f)() end
function NS_ON() ImportScript = nil end
-- CreateNameSpace, ThreadNameSpace, KillNameSpace, NSCall
```

`CreateNameSpace(fichier)` donne à un script son propre environnement
(`setfenv`), dont les noms absents renvoient à `_G` (`setmetatable(env, {__index = _G})`),
puis le lance dans une coroutine. Une mission voit donc les globales de `main`
(`gPlayer`, posé par `main` avec `PlayerGetPedIndex()`) sans pouvoir les écraser.

## Ce que donne la sonde

`build/outils/sonde_lua` exécute le premier niveau de chaque script avec les
1 504 fonctions du moteur en bouchons. Avec `SONDE_AVANT=util.lur` :
**515 chargés, 509 exécutés jusqu'au bout**. Les 6 échecs sont des comparaisons
ou des calculs sur la valeur renvoyée par un bouchon (nil).

- `main` est défini par 460 scripts, `MissionSetup` par 253, `MissionCleanup` par 251.
- 36 bibliothèques importées ; les plus courantes : `LibTable` (66), `LibPed` (54),
  `RunMissionLib` (93), `LibPlayer` (15).
- `main.lur` est le script maître : il importe `SInitGl`, `Events`, `Scenarios`,
  `SCutscenes`, `SMissPass`…, et définit 263 fonctions.

## Reste à établir

- L'ordre de démarrage côté moteur : qui charge `util.lua`, puis `main`, et comment
  une mission est lancée (chaînes `util.lua`, `main`, `gamemain` près de 0x92cbe0).
- Les tables que le moteur pose lui-même : `MODELENUM`, `POINTLIST`, `PATH`, `TRIGGER`.
- Brancher les 1 504 fonctions, en commençant par celles des scripts d'ambiance.
