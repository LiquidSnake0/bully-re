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

`CreateLuaState` (0x5db260, lue avec REA) monte l'état Lua du jeu, dans cet ordre :

1. `lua_open` ; `__onerror`, `_ALERT` et le gestionnaire de panique pointent tous
   sur 0x824d30 (la fonction qui sert aussi de `print`).
2. Cinq bibliothèques standard : **base** (avec coroutine), **table**, **math**,
   **debug** et une cinquième (0x7423c0). Ni `string`, ni `io`, ni `os` : un script
   de Bully ne touche ni aux chaînes avec `string.*` ni aux fichiers.
3. Seuil du ramasse-miettes à 1 000 000 000 : il ne passe presque jamais.
4. **58 tables de l'API du moteur, 1 508 fonctions** (`docs/api-lua-tables.tsv`),
   chacune passée à 0x5dafe0. Ce sont des `luaL_reg` de Lua 5.0 : paires
   `{ nom, fonction C }` finies par `{ 0, 0 }`. `tools/tables_lua.py` relit les 58
   appels dans le code de `CreateLuaState`, puis chaque table.
5. Cinq énumérations posées par 0x5db040 avec leur fonction de lecture :
   `PATH` (0x5db220), `TRIGGER` (0x5db0f0), `POINTLIST` (0x5db130),
   `MODELENUM` (0x5db1d0) et la chaîne de 0x901d18 (0x5db170).
6. **`util.lua`** chargé et exécuté (0x5d8ae0), puis un passage complet du
   ramasse-miettes (seuil 0) avant de remettre le seuil haut.

L'ancienne liste de 914 noms (`docs/api-lua.txt`) venait d'une source extérieure :
253 de ses noms (`CameraActive`, `CameraFadeTrack`…) n'ont pas de table, ce ne sont
pas des fonctions Lua ; et il lui manquait `ImportScript`, `GetTimer`,
`GetMissionCurrentAttemptCount`, `EffectRegisterInArea` et 843 autres (847 noms nouveaux ; 661 sont communs aux deux listes).

## Le démarrage

`FUN_005dc190` (seul appelant de `CreateLuaState`, lu avec REA) :

1. ferme l'ancien état s'il existe (0x5db830) ;
2. lit `Scripts\Scripts.DIR` (l'annuaire de `Scripts.img`) ;
3. `CreateLuaState` (ci-dessus), qui finit par exécuter `util.lua` dans `_G` ;
4. `FUN_005dbf90("main.lua", 0)` : crée l'objet script de `main` ;
5. `FUN_005d9d20("gamemain")` : lance la fonction `gamemain` de `main.lur` en thread.

Le moteur tient une **pile d'objets script** (0x2b0c octets chacun, tableau en
+0x6b64, nombre en +0x6b84, script courant en +0x6b88). `FUN_005dbf90(fichier, lancer)`
réutilise l'objet du fichier s'il existe (0x5dbb60), sinon le crée (0x5d8950),
l'empile, exécute son premier niveau (0x5d9c50) et, si `lancer` vaut 1, démarre sa
fonction `main` : c'est ainsi qu'une mission démarre. `main.lua` est chargé avec 0,
puis c'est `gamemain` qui est lancé.

Un thread (`FUN_005d8e40`) est un appel protégé, `__onerror` en gestionnaire, à la
fonction Lua `ThreadNameSpace(fichier, fonction)` de `util.lua`. Elle renvoie une
coroutine, rangée dans un emplacement de 0x44 octets de l'objet script (indice en
+0x1148). Le moteur reprend ensuite ces coroutines image par image : c'est la
boucle à recréer pour que `Wait` fonctionne.

## La boucle des threads

Lue avec REA le 09.10.2026.

- **`Wait(ms)`** (0x5bfa70) lit le délai, prend le thread courant du script courant
  (gestionnaire en 0xd02850, script courant 0x5db7e0, thread courant 0x5d8a30), écrit
  l'heure de réveil `CTimer` (0xc1a9b4) + délai à l'offset +0x1c du thread, sauf si elle
  vaut -1, puis rend la main avec `lua_yield` (0x73fda0).
- **`CreateThread("F_Nom")`** (0x5bf2f0) appelle `ThreadNameSpace(fichier, "F_Nom")` dans
  le script courant (0x5d9d20), marque l'octet +0x14 du nouveau thread, et renvoie son numéro.
- **`LuaScript_Update`** (0x5da960, nom tiré de son marqueur de profilage) tourne à chaque
  image pour chaque script. Pour chaque thread à partir du 1 (le 0 est le premier niveau) :
  un thread nouveau ou en cours passe en « arrêt demandé » si son heure de réveil est
  négative ; s'il est réveillé (heure passée) ou en arrêt demandé, le moteur pose la globale
  **`Alive`** (fausse en cas d'arrêt demandé), note le thread courant (+0x114c) et appelle
  `lua_resume` (0x73ffc0). Une coroutine finie (pile vide) ou en erreur est retirée
  (0x5d9d30, qui appelle `KillNameSpace` pour le thread 0) ; en arrêt demandé, seule une
  fonction nommée `MissionCleanup` a le droit de continuer.
- L'emplacement d'un thread fait 0x44 octets à partir de +0x48 de l'objet script :
  coroutine +0x00, référence +0x04, état +0x10 (0 nouveau, 1 en cours, 2 arrêt demandé,
  3 tué, 4 fini), drapeau +0x14, compteur +0x18, réveil +0x1c, nom de la fonction +0x24.
  Nombre de threads en +0x1148.
- L'objet script naît vide (0x5d8950 : état, compteurs, nom du fichier avec `\` changé en
  `/`) ; 0x5d9c50 appelle **`CreateNameSpace(fichier)`** de util.lua dans un appel protégé
  (0x5d8d40), qui exécute le premier niveau et renvoie la coroutine du thread 0.

### Recréée : `build/outils/lancer_lua`

`outils/lancer_lua.cpp` refait ce chemin hors du moteur : util.lua dans `_G`, l'objet
script de `main.lua`, le thread `gamemain`, puis une boucle à 33 ms par image avec la
logique de `LuaScript_Update`. `Wait`, `CreateThread`, `TerminateThread`, `GetTimer` et
l'horloge (`ClockGet`, `ClockSet`, `ClockSetTickRate`, sur `CHorloge`) sont réels ; le reste
de l'API est en bouchons qui journalisent l'appel. Les bouchons répondent comme dans une
nouvelle partie, **de façon provisoire**, d'après le nom : `Is…`, `Has…`, `Should…` →
`false` ; `Get…`, `…Count` → `0` ; et `HasStoryModeBeenSelected` → `true` (le joueur a
choisi « Histoire » au menu).

Résultat sur 2 minutes de jeu, **sans erreur** :

1. Premier niveau de `main.lua` : les imports (`SInitGl`, `Events`, `Scenarios`…).
2. `gamemain` : 45 scripts de zones enregistrés (`AreaRegisterAreaScript`), exclusions de
   collision, 7 points de sauvegarde, 26 fichiers `.DAT` (`DATLoad`), tenues, horloge réglée
   (`ClockSet`, `ClockSetTickRate`), météo, création du joueur (`PlayerCreateXYZ`), puis
   attente du mode Histoire.
3. Ensuite les effets (136), les lueurs de fenêtres (271), les générateurs de voitures (35),
   les points de réapparition, la population, et **quatre threads** : `F_CheckPOI`,
   `F_AlarmThread`, `T_PhotographyStimulus`, `F_DanceCowDance`.
4. Tous tournent ensuite image par image (`AreaGetVisible`, `MissionActive`,
   `WeaponEquipped`, `ClockGet`…). Aucune mission ne démarre : rien ne la déclenche encore.

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
avec son propre `ImportScript` qui exécute la bibliothèque dans cet environnement ;
`GlobalImportScript` garde la version de `util`, qui exécute dans `_G`.
`gPlayer` est posé dans `SInitGl` (0) puis dans `main` (`PlayerGetPedIndex()`).

## Ce que donne la sonde

`build/outils/sonde_lua` exécute le premier niveau de chaque script avec les
1 508 fonctions du moteur en bouchons. Avec `SONDE_AVANT=util.lur` :
**515 chargés, 509 exécutés jusqu'au bout**. Les 6 échecs sont des comparaisons
ou des calculs sur la valeur renvoyée par un bouchon (nil).

- `main` est défini par 460 scripts, `MissionSetup` par 253, `MissionCleanup` par 251.
- 36 bibliothèques importées ; les plus courantes : `LibTable` (66), `LibPed` (54),
  `RunMissionLib` (93), `LibPlayer` (15).
- `main.lur` est le script maître : il importe `SInitGl`, `Events`, `Scenarios`,
  `SCutscenes`, `SMissPass`…, et définit 263 fonctions.

## Reste à établir

- Ce qui lance la première mission, et les 1 508 fonctions à brancher pour de vrai, en
  commençant par celles que `gamemain` appelle (liste ci-dessus).
- Qui voit quoi : les globales de `main` vivent dans l'espace de noms de `main` ;
  vérifier comment une mission voit `gPlayer` (`GlobalImportScript` exécute dans `_G`).
- Ce que renvoient les cinq énumérations (`MODELENUM.x` → numéro de modèle ?).
- Brancher les 1 508 fonctions, en commençant par celles des scripts d'ambiance.
