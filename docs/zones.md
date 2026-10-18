# Les zones : codes, zone visible, changement de zone

Établi le 18.10.2026 dans l'index Ghidra (`tools/chercher.py`). Code : `src/core/AreaManager`,
test `tests/test_zones` (sans fichier du jeu), branché dans `outils/lancer_lua`.

## Les codes de zone

Une zone (*area*) est un intérieur ou l'extérieur, désignée par un petit entier. `gamemain`
en enregistre 45 avec `AreaRegisterAreaScript(code, script)` (Lua 0x5b7530 → `AreaRegisterAreaScript`
0x414f10) : le chemin du script est copié par `strncpy` dans une table `DAT_009e3440`, une
case de 0x6c octets par code, et un nom de 100 caractères ou plus est ignoré. `FUN_00414f00(&code)`
rend la case. Les codes vus dans `gamemain` :

| Code | Script | Code | Script |
|---|---|---|---|
| 0 | `MainMap` (l'extérieur, tout le campus et la ville) | 26, 29, 30, 50 | `SStores` (boutiques) |
| 2 | `SchoolHallways` (l'intérieur de l'école, bureau du principal compris) | 27 | `Boxing` |
| 4 | `Chem_Lab` | 32 | `PrepHouse` |
| 5 | `PrincipalOffice` | 33, 34, 39, 46, 56 | `StyleStores` |
| 6 | `Bio_Lab` | 35 | `Gdorm` |
| 8 | `Janitors` | 36 | `Tenements` |
| 9 | `Library` | 37 | `Funhouse` |
| 13 | `GymAndPool` | 38 | `Asylum` |
| 14 | `Bdorm` | 40 | `Observatory` |
| 15 | `Classroom` | 42 | `TGokart` |
| 16 | `Tattoos` | 43 | `Junkyard` |
| 17 | `ArtRoom` | 45 | `Midway` |
| 18 | `AutoShop` | 51, 52, 53 | `ImgRaceA/B/C` |
| 19 | `Auditorium` | 54 | `Warehouse` |
| 20 | `ChemPlant` | 55 | `FreakShow` |
| 23 | `StaffRoom` | 57, 59, 60, 61 | `SaveZones` |
| | | 62 | `BMXTrack` |

Les codes manquants (1, 3, 7, 10…) n'ont pas de script dans la version finale ; le moteur les
teste encore ici et là (`FUN_00455750` : la zone 50 ; `FUN_0041fdd0` : la zone 2 ;
`FUN_00455340` : 26 et 30).

## La zone visible

`DAT_00bd1008` est **la zone où le joueur est**, celle dont les scènes et les collisions sont
chargées. `AreaGetVisible()` (0x5b7300) la rend telle quelle. Une seule fonction l'écrit,
`FUN_00415750(&zone)` : si la zone change, `FUN_00667ff0()` ; puis `FUN_004540f0(zone)` (la table
des 500 scènes de placement, 0x1c octets chacune, dont l'octet de tête porte « active » en bit 0
et le code de zone à partir du bit 2 : les scènes de cette zone sont chargées), `FUN_00560ef0()`,
`FUN_004f35d0()`, et `DAT_00bd1008 = zone`. `AreaSetVisible` existe dans l'API Lua mais pointe
sur 0x824d30, la fonction d'affichage : elle ne fait rien.

## L'AREACODE d'un point n'est pas la zone visible

Chaque bloc `POINT`, `PATH` et `TRIGGER` des `.dat` porte un `AREACODE` (docs/trigger.md). Pour
une liste de points, le lecteur `FUN_006d59a0` l'écrit sur un octet en +0xc de l'entrée. Qui le
lit : **`PlayerSetPosPoint`** (`FUN_005d2310`) seulement, pour en faire **la zone de
destination** du joueur (−1 : rester dans la zone visible). `PlayerSetPosPointArea`
(`FUN_005d2410`) prend la zone donnée par le script, `PlayerSetPosXYZArea` aussi, et
`PlayerSetPosXYZ` (0x415fb0) garde la zone visible. Toutes les quatre posent l'octet +0x8dc du
gestionnaire (fondu immédiat) et appellent `FUN_00415640` : une demande, pas un déplacement.

Dans `1_01.dat`, les points du bureau du principal (`1_01_EndPoint`, `1_01_EndPrincipal`,
`1_01_EndPlayer`, `1_01_Principal`) disent `AREACODE 5`, mais le déclencheur `1_01_PrincOffice`
qui les entoure dit `AREACODE 2`, et le script attend `AreaGetVisible() == 2` (le
`PlayerIsInAreaXYZ` qui le précède est calculé mais n'entre pas dans le test compilé). La
raison est dans `AreaTran.dat` : la porte du bureau (`SchoolPrincipal`, zone 2 → zone 5) a sa
contrepartie **désactivée** (`Disabled_SV_PrincipalSchool`, en (−696.7, 229.7, 31.4), loin du
hall). Le bureau du principal était un intérieur à part (zone 5, `PrincipalOffice.lua`) ; dans la
version finale il fait partie de l'intérieur de l'école (zone 2), à (−633, −291), et les points
de `1_01.dat` ont gardé leur ancien code. Un script qui ferait `PlayerSetPosPoint(_1_01_EndPoint)`
enverrait le joueur en zone 5 : `1_01` ne le fait pas, il le fait marcher par les portes.

## Le changement de zone (FUN_00416e30)

Le gestionnaire est l'objet pointé par `PTR_DAT_009e4f4c`, initialisé par `FUN_00415ed0`.
`FUN_004176d0` le fait avancer à chaque image : d'abord la seconde machine (`FUN_00416c80`,
+0x868, un chargement forcé sans déplacement), puis celle du déplacement, `FUN_00416e30`, en
boucle tant qu'elle renvoie 0 (les états qui s'enchaînent dans une image).

**La demande** (`FUN_00415640(position, cap, &zone, caméra)`) est refusée si un changement est
en cours (`AreaIsLoading`, 0x415230 : +0x864, +0x86c, +0x868 ou +0x86d non nuls) ; sinon elle
est rangée en +0x878 (position), +0x884 (cap), +0x888 (zone), +0x88c / +0x890 (une direction de
caméra, facultative), et +0x86c est posé.

**Les portes** : à l'état 0, sans demande, le gestionnaire parcourt ses paires (déclencheur,
liste de points) en +0x10 (le compte en +8) ; si le joueur est dans le déclencheur
(`FUN_006d6e50`), il demande lui-même le premier point de la liste, avec le cap du point et
**la zone de l'octet +0xc de la liste**, et le second point comme direction de caméra. C'est à
ça que sert l'AREACODE : la porte `MainSchool` (zone 0) mène aux points dont l'AREACODE dit 2.

**Les états** (+0x864) :

| État | Ce qui se passe | Puis |
|---|---|---|
| 0 | une demande (+0x86c) est prise | 1 |
| 1 | même zone : 7 si l'objet script de la zone existe (`FUN_005dbb60`), sinon 5 ; autre zone : 5 si l'écran est déjà noir (`FUN_004f11a0() == 1`) ou si +0x85f, sinon 2 | |
| 2 | avec +0x8dc (un script) : `CameraFade(durée par défaut, vers le noir)` → 3 ; sans (une porte) : compte à rebours +0x8ec = 14 images → 4 | |
| 3 | attend l'écran noir ; une image de plus pour `FUN_0055e700` | 5 |
| 4 | le compte à rebours, puis `CameraFade(0, vers le noir)` | 3 |
| 5 | +0x8dc remis à 0 ; `FUN_005dbb20` : les objets script marqués « zone » (+0x2b09) sont à retirer | 6 |
| 6 | quand `FUN_005dbae0` n'en voit plus : `FUN_005dc020(script de la zone, 1)` = `FUN_005dbf90(nom, 1)`, l'objet script créé, son premier niveau exécuté, son `main` lancé, marqué « zone » | 8 |
| 7 | | 10 |
| 8 | attend +0x870 : le `main` du script de zone appelle `F_PreDATInit()` (SGlFunc.lua), qui appelle **`AreaSignalAreaTransitionReadyToLoad()`** (0x5b6f40, `DAT_00bccdf0`) puis attend `AreaIsLoading()` | 9 |
| 9 | `FUN_004151e0` (le joueur arrêté), **`FUN_00415750` : la zone visible change**, `FUN_0048b330`, `FUN_00416ae0` (le monde vidé : piétons, véhicules, explosions), **`FUN_004162f0` : le joueur posé** à +0x878 avec son cap, caméra remise ; +0x854 = le secteur du joueur, +0x850 = maintenant + un délai | 0xb (0xd avec +0x85f) |
| 10 | le joueur posé sans changer de zone | 0xd |
| 0xb | quand le délai est passé ou le secteur chargé : `CameraFade(durée, retour)` | 0xc |
| 0xc | le fondu assez avancé (`DAT_00c3cd28` contre `DAT_020c8910 + 0x24`) | 0xd |
| 0xd | +0x86c, +0x870, +0x394 remis à zéro | 0 |

Le fondu est celui de la caméra (`CameraFade` 0x4f1980 ; en Lua, `CameraFade(ms, sens)` 0x5b8ea0 :
sens 0 vers le noir, 1 retour, durée négative = celle du moteur en `DAT_020c8910 + 0x18` ou
`+ 0x1c`). `FUN_004f11a0` dit où il en est : 0 écran rendu, 1 noir, 2 en cours.

**`AreaTransitionXYZ` et `AreaTransitionPoint` ne sont pas des fonctions du moteur** : elles
sont dans `SGlFunc.lua` (importé dans `_G` par `GlobalImportScript`) : `PlayerSetPosXYZArea` /
`PlayerSetPosPointArea`, `Wait(5)`, puis `while AreaIsLoading() do Wait(0) end`, puis
`Wait(100)`. `ManagedPlayerSetPosPoint` fait pareil autour de `PlayerSetPosPoint`.

## Recréé

`CAreaManager` (`src/core/AreaManager`) tient la table des scripts, la zone visible, la demande
et la machine ci-dessus, image par image, et dit ce qu'elle attend du monde (fondu, scripts à
retirer, script à charger, joueur à poser). Ce qui n'est pas recréé : les portes de +0x10 (dans
`lancer_lua`, `--zone N@s` joue la porte en demandant la zone N depuis la position du joueur),
le délai de l'état 0xb (constante non lue, on passe tout de suite), la seconde machine. Le test
suit les quatre chemins : nouvelle partie (même zone, script non chargé), téléportation par un
script (fondu immédiat), porte (14 images puis fondu), même zone avec script.

Dans `lancer_lua`, `PlayerSetPosPoint`, `PlayerSetPosPointArea`, `PlayerSetPosXYZArea`,
`PlayerSetPosXYZ` demandent ; `AreaIsLoading`, `AreaGetVisible`, `AreaRegisterAreaScript`,
`AreaSignalAreaTransitionReadyToLoad` et `CameraFade` sont réels ; **les scripts de zone sont
chargés pour de vrai** avec leur `main` (`MainMap.lua` à la nouvelle partie, `SchoolHallways.lua`
quand le joueur passe la porte de l'école), et retirés au changement suivant. Sur `1_01` :

    build/outils/lancer_lua 14 --mission 1_01 --entrer _1_01_BULLYTRIG@4 --zone 2@6 --aller _1_01_ENDPOINT@8

`MissionSetup` demande la grille (`AreaTransitionPoint(0, …)`) : zone 0 déjà visible, `MainMap.lua`
chargé, son `main` signale à 1,12 s, le joueur est posé ; la porte à 6,0 s : 14 images, fondu au
noir fini à 7,52 s, `MainMap.lua` retiré, `SchoolHallways.lua` chargé et son signal à 7,66 s, **la
zone visible passe à 2**, l'écran revient à 8,66 s ; la secrétaire est créée dès que la zone vaut
2, et la mission se termine réussie à 9,67 s.
