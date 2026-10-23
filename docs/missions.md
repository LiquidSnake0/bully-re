# Les missions : la banque d'arbres d'actions

La liste des missions n'est pas dans un script Lua : c'est un arbre d'actions,
`Act/Missions.act` (`Missions.cat` dans `Act/Act.img`), le même format que les
comportements des personnages (docs/cat.md). `tools/missions.py` en sort la table.

## Chargement (bully.exe)

- `CGame::Initialise` (0x42ee90, profilé « CGameInitialise ») appelle
  `FUN_006ac5d0("MissionBank", "Act/Missions.act")` juste après l'écran de chargement.
- `FUN_006ac5d0` est une méthode du gestionnaire de missions. Elle charge l'arbre,
  puis `FUN_006ab7e0` le parcourt et range chaque nœud qui porte une piste
  `MissionData` (classe `CMissionData`).
- Champs du gestionnaire relevés dans `FUN_006ac5d0` :

| Champ | Contenu |
|---|---|
| +0x4 | la banque chargée |
| +0x4b8 | tableau des `MissionData`, une entrée de 0x4c octets par mission (la piste copiée) |
| +0x684 | tableau des nœuds de mission (pointeurs) |
| +0x688 | état de chaque mission, 6 octets par mission (le bit 0 de l'octet +4 est lu par `FUN_006aadc0`) |
| +0x744 | nombre de missions |
| +0x754 | nombre de missions de type 0 ou 3, avec l'octet 37 nul et l'octet 74 non nul |
| +0x740 | banque prête |

## La forme d'une mission

`Missions.cat` est la racine : une banque `MissionBank`, une mission factice, et
onze références vers les sous-arbres `chap1` à `chap5`, `classes`, `Minigames`,
`Punish`, `Tutorials`, `Doors` et `Debug`, dans cet ordre. `src/core/MissionBank.cpp`
suit ces références comme `FUN_006ac5d0` ; `tools/missions.py` aussi. Chaque mission
est un nœud jouable :

```
n 1_01                                   ← le nom de la mission (MissionGetName)
   · MissionData 24='1.01 - Welcome To Bullworth Pt 1', 28='chap1\1_01.lua', 32='1_01_TITLE', 40=0 …
  n active                               ← gardé par MissionCanLaunch dans la mission factice
     · MissionButtonTrigger 24='1_01_Corona', 48='BUT_LAUNCH' …
    n RUN
      l Succeed
      l fail
```

Les conditions d'entrée du nœud (`si MissionCond`, `si MissionCond TimeCond`)
disent quand la mission devient lançable (« Les conditions », plus bas).

## Les conditions

Le nœud garde, pour chaque condition, un décalage vers un enregistrement des données.
`FUN_005fa910` passe l'enregistrement à la fabrique `FUN_0061a4f0`, qui cherche le premier
mot (le `HashString` du nom de classe) dans un registre de 419 classes (rempli par
`FUN_0064b090` : nom, création, construction sur place) et appelle la **construction sur
place à +4** : l'objet est bâti sur les données du fichier, sa table virtuelle écrite à +4 de
l'enregistrement. Le champ +k de l'objet est donc le mot +4+k de l'enregistrement.
L'évaluation est l'emplacement 1 de la table virtuelle (`export/rtti.json`).

| Classe (nom du registre) | Construction | Évaluation | Champs |
|---|---|---|---|
| `CMissionCond` (`MissionCond`) | `FUN_0063a170` → `FUN_006a7d50` | `FUN_006a7db0` | +4 le nom haché de la mission, +8 son indice (−2 tant qu'il n'est pas cherché) |
| `CTimeCond` (`TimeCond`) | `FUN_0063a270` | `FUN_006a7380` → `FUN_00425cf0` | +4 heure de début, +8 heure de fin, +0xc minute de début, +0x10 minute de fin |
| `ConditionFalse` (`False`) | `FUN_00638950` | `FUN_006094f0` : rend 0 | — |

- **`MissionCond` est vraie quand la mission nommée a réussi au moins une fois** : l'indice
  est cherché une fois par `FUN_006a9e70` (le premier nœud de ce nom ; −1 rend faux), puis
  `FUN_006aa660` donne l'entrée de 6 octets (+0x688 du gestionnaire) et la condition rend
  `mot +2 ≠ 0`. Ce mot est le **compte des réussites** : `FUN_006ac910`, la fin de mission,
  l'incrémente quand la mission a réussi, et `FUN_006a9ff0` compte les missions où il n'est
  pas nul (+0x748 du gestionnaire). Deux noms sont à part (21.10, sur l'assembleur de `FUN_006a7db0` : « CurrentMission » est
  haché en 0x020c3594, « CurrentClassMission » en 0x020c3590, et l'indice est alors recalculé à chaque
  fois) : **`CurrentMission`** est la mission en cours (`FUN_006aa630` : le second emplacement du
  gestionnaire, +0x5a0, s'il joue, sinon le premier, +0x4bc), **`CurrentClassMission`** la première
  mission de cours active (`FUN_006a9ed0` : bit 0 de l'état posé, type 1).
- **`TimeCond` est vraie quand l'heure du jeu est dans [début, fin[** : `FUN_00425cf0` compte
  en minutes depuis minuit (`DAT_00bceb4b` heures, `DAT_00bceb4a` minutes) et laisse la plage
  passer minuit quand la fin précède le début.

**La liste** : le gestionnaire (`FUN_006ad130`) fait tester au nœud sa liste de conditions
(emplacement +0x4c du nœud), que **`FUN_005fb9b0`** parcourt. Chaque condition a un genre,
l'emplacement 2 de sa table virtuelle : 0 pour une condition (`FUN_00824d30`), 1 pour `ConditionNOT`
(`FUN_006f2ad0`), 2 pour `ConditionOR` (`FUN_00765c70`) ; NOT et OR s'évaluent vrais
(`FUN_00609b30`), ce sont des opérateurs. Le résultat part à vrai ; chaque élément est évalué, et
après un élément de genre 0 le résultat devient sa valeur, après un NOT son contraire, après un OR
la suite est sautée jusqu'au prochain opérateur ; un résultat faux arrête la liste, sauf si
l'élément suivant est un OR, qui est alors sauté. `A B OR C` vaut donc `(A et B) ou C` quand A est
vrai, mais faux dès que A est faux : c'est le moteur. Dans la même boucle, une mission déjà réussie
(mot +2 non nul) n'est réactivée que si l'octet +0x2c de sa `MissionData` (champ 44) est posé : 171
missions sont ainsi rejouables, ni `1_01` ni `1_02`.

La chaîne du chapitre 1, lue dans `chap1.cat` : `1_02` attend `1_01`, `1_02A` attend `1_02`,
`1_02B` attend `1_02B Force Run Dummy`, `1_02C` attend `1_02B`, `1_03` attend `1_02C` et
`1_02B` entre 8 h et 19 h. `CMissionBank::Evaluer` et `Activable` refont les trois classes ;
`tests/test_missions` suit la chaîne et la plage de nuit. Sur les 333 missions jouables qui ont des
conditions, 195 s'évaluent entièrement avec ces trois classes ; les autres attendent
`ConditionGroup` (402 conditions : `ConditionGroupCondition`, `FUN_005fbaf0`, qui évalue un groupe nommé
par la même `FUN_005fb9b0`, la référence n'étant pas encore résolue), et une vingtaine de classes plus rares. **`DayCond`** (`CDayCond`, `FUN_006a7480` → `FUN_006a8b30` sur l'objet
0x020c35e8) compare son champ +4 au jour du calendrier (+0x58 de l'objet, `DAT_020c3640`, ce que
`GetCurrentDay`, 0x5c0c00, rend ; 0 au démarrage, posé entre 0 et 4 par `FUN_0061d320`) ; avec l'octet +8,
le jour est pris modulo le +0x18 de **`TimeCycleButes`** (`DAT_020c88ac`, trouvé par son nom par
`FUN_0072ad00` parmi les pistes « Butes » ; dans `Globals.cat`, position 24 = 5, le diviseur que
`GetCurrentDay(true)` écrit en dur). Les 59 `DayCond` des missions demandent toutes le modulo.

## Les champs de `MissionData`

La position est le décalage dans l'objet de la piste (docs/cat.md, « Les pistes »).

| Position | Sens | Comment on le sait |
|---|---|---|
| 24 | titre de travail | lisible |
| 28 | script Lua de la mission | lisible, `chapN\nom.lua` |
| 32 | clé du titre affiché (`1_01_TITLE`) | lisible, présente dans `Config/Text` |
| 40 | **type** : 0 histoire et courses, 1 cours, 3 boulots et courses de quartier, 4 tutoriels, 5 mini-jeux | répartition sur les 499 entrées ; `FUN_006ac110` traite le type 4 à l'heure, `FUN_006aac70` le type 1 |
| 36 | **afficher le titre au lancement** : pour les types 0, 1 et 3, si l'octet vaut 1 et que la clé du champ 32 n'est pas vide, `FUN_00688050` affiche le titre | `FUN_006ad3e0`, cas 1 et 2 ; d'où les 282 missions à 1 (ce n'est pas « lancer au début de la partie ») |
| 37, 74 | entrent dans le compte +0x754 | `FUN_006ac5d0` |

## Le compte : 499 entrées, 360 lançables

Le tableau +0x4b8 a une entrée **par piste `MissionData`**, et `FUN_006ab7e0` parcourt
tout ce que `Missions.cat` cite, `Doors.cat` compris. `tools/missions.py --types` et
`build/tests/test_missions` tombent sur le même compte :

| Fichier | Entrées | Fichier | Entrées |
|---|---|---|---|
| Missions | 1 (la mission factice) | Minigames | 45 (`ArcadeRace3D` porte deux pistes) |
| chap1 … chap5 | 33, 37, 21, 14, 21 | Punish | 22 |
| classes | 69 | Tutorials | 116, **aucune avec un script** |
| Doors | 17, toutes `none.lua` : des verrous de portes, pas des missions | Debug | 103 |

Par type : 148 d'histoire et courses, 64 cours, 101 boulots et courses, 114 tutoriels,
72 mini-jeux. **360 ont un script** (champ 28 non vide et autre que `none.lua`) : ce
sont celles que `ForceStartMission` peut lancer ; les 139 autres (tutoriels, portes,
cours factices « DO NOT RUN ») n'existent que pour leurs conditions et leurs pistes.

L'ancien écart « 360 côté C++, 473 côté Python » venait de deux défauts de
`missions.py` : il ignorait `Doors.cat` (17) et perdait une piste `MissionData` quand
une autre piste la précédait sous le même nœud (9) ; le C++ ne comptait que les
missions avec script. Les deux lisent maintenant les 499.

## Ce qui lance une mission

- Côté scripts, la primitive est **`ForceStartMission(nom)`**. La bibliothèque de
  test `RunMissionLib` le montre : `RunMission(nom)` attend la fin du chargement
  (`IsStreamingBusy`), puis appelle `ForceStartMission(nom)` ; `Test1_01` fait
  `RunMission("1_01")`. Le jeu l'appelle aussi pour les missions de fin
  d'histoire (`MainMap` pour 5_07, `SGlFunc` pour 6_01 et 3_08).
- Côté banque, une mission devient lançable quand ses conditions sont vraies ; le
  joueur la démarre en entrant dans son marqueur (`MissionButtonTrigger`).
- **Le gestionnaire, à chaque image** (`FUN_006ad130`, objet 0x020c3ca0, lu dans
  `export/fonctions.jsonl`) parcourt les 499 entrées : si les conditions d'entrée du
  nœud sont vraies (aucune condition : toujours), la mission est **activée** (bit 1 de
  l'octet +4 de son entrée de 6 octets en +0x688 ; `FUN_006ab2d0` joue le nœud
  `Active`, donc son `MissionButtonTrigger`, le corona) ; activée, elle **démarre
  seule** (`FUN_006acfa0`) si l'octet +0x33 de `MissionData` (champ 51) est posé et
  qu'aucune mission ne tourne. `SystemAllowMissionManagerToForceRunMissions` (0x5d5110,
  7 octets : `mov byte [0x020c441d], 1`) pose +0x77d du gestionnaire, qui autorise ce
  démarrage pour les missions déjà activées ; `gamemain` l'appelle après ses `DATLoad`.
- **La première mission** : `1_01` est la seule mission de `chap1` sans condition, mais
  son champ 51 vaut 0 (il vaut 1 pour `1_02`, `1_02A`… qui s'enchaînent seules) : elle
  ne démarre pas seule, elle attend le joueur dans son corona. Sa piste
  `MissionButtonTrigger` dit `24='1_01_Corona', 28=2.0, 32=1, 36=0, 40=3, 48='BUT_LAUNCH'`
  là où presque toutes les autres disent `28=1.0, 32=1, 36=1, 40=25`. **Les positions des
  attributs sont les décalages dans l'objet** que `FUN_006a8650`, la mise à jour de la
  piste, lit à chaque image (18.10.2026) : +0x1c (champ 28) est **le demi-côté du pavé**
  où le joueur doit être, testé par `FUN_005c2910(1, 0, 3, x, y, z, r, r, r, …)`, le même
  pavé fermé que `PlayerIsInAreaXYZ` : 2 m pour `1_01` (19 coronas sur 212), 1 m
  ailleurs ; +0x20 (32) le type du marqueur, +0x24 (36) l'octet qui autorise sa taille
  +0x28 (40, un entier : 25, 23, et 3 pour `1_01`) ; +0x30 (48) la clé de texte de
  l'invite (`BUT_LAUNCH`, `BUT_MISSION` pour la mission factice), affichée par
  `FUN_0068f420` une fois dans le pavé ; +0x44 la position du point, +0x50 son lacet.
  Dans le pavé, et si la mission est lançable (`FUN_006aa840`) et le joueur libre
  (`FUN_0050de00`, `FUN_00473e80`, `FUN_004ce540`, deux `FUN_004d32a0` : non lus), le
  moteur lit la manette (`FUN_007386f0(10, 0)`, `FUN_007387b0(9, 0)`) et appelle
  **`FUN_006acfa0(mission, 0)`**, le joueur passant à
  `/Global/SimpleLoco/Mission/InvulnerableIdle`. Ce n'était donc pas un « mode » :
  `28` est une longueur. `1_01_Corona` est une liste de points de `MisTrig.dat` (que
  `gamemain` charge, `DATLoad("MisTrig.DAT", 1)`), en (297.3, −71.5, 4.9), à 24,8 m de la
  grille où `gamemain` pose le joueur à la nouvelle partie (`SystemShouldMovePlayer` →
  `PlayerSetPosXYZArea(272.68, −73.13, 6.97, 0)`) : le joueur sort du bus, marche jusqu'au
  marqueur, et `1_01` commence (son `MissionSetup` le ramène à la grille et joue la
  cinématique `1-1-1`). `tests/test_missions` vérifie les champs (conditions, champ 51,
  corona, demi-côté, marqueur, taille, invite) ; 27 missions jouables n'ont aucune
  condition (1_01, 25 mini-jeux, 1 punition).
- `SMissPass.F_LaunchingMission` prépare l'état du monde selon la mission lancée
  (`MissionGetIndex`, `1_01`, `1_02A`…) : à lire quand on branchera le lancement.
- **Dans `lancer_lua`**, `--nouvelle-partie` rejoue tout cela sans `--mission` :
  `SystemShouldMovePlayer`, `PlayerSetPosXYZArea`, `AreaTransitionXYZ`,
  `SystemAllowMissionManagerToForceRunMissions`, `IsMissionCompleated` (les réussites
  de la partie), `MissionGetIndex` sont réels, et le gestionnaire réduit active les
  missions sans condition et lance celle dont le joueur est dans le pavé du corona
  (demi-côté du champ 28 ; l'invite et la manette ne sont pas jouées). Avec `--aller 1_01_Corona@3`, le joueur marche au
  marqueur : **`1_01` démarre à 3,0 s par le gestionnaire, et se termine réussie à 13,5 s**, puis **`1_02` s'active et démarre seule**
  (`--sans-cinematiques --entrer _1_01_BULLYTRIG@7 --zone 2@9 --aller _1_01_ENDPOINT@11`) ; `MainMap.lua` est
  chargé à 0,17 s par la première demande de zone de `gamemain`. Les conditions sont
  évaluées par `CMissionBank::Activable` (« Les conditions ») ; une mission qui en porte une
  d'une classe pas encore lue reste inactive, et une mission réussie n'est plus reprise par l'outil.

## Le déroulement d'une mission (bully.exe)

La machine à états du gestionnaire est `FUN_006ad3e0`, appelée à chaque image :

1. `FUN_006acba0` : l'objet script de la mission est créé à partir du champ 28
   (`FUN_005dbf90`, le CreateNameSpace des scripts), le thread `MissionSetup` est
   lancé (`FUN_005d9d20`) et tourne jusqu'à cinq images de suite, tant que
   `FUN_005d8db0("MissionSetup")` répond qu'il n'est pas fini (état du thread ≠ 4).
2. Cas 2 : `MissionSetup` fini, `FUN_006abf20` lance le thread `main` sur le même
   objet script.
3. Cas 9 : `main` fini, `FUN_006abed0` lance `MissionCleanup` et pose le drapeau
   de fin de l'objet script (+0x1150).
4. Cas 10 : `MissionCleanup` fini, la mission est terminée.

## Dans lancer_lua

`build/outils/lancer_lua 60 --mission 1_01` charge la banque par `CMissionBank`
(499 entrées, 360 avec un script), lance `main.lua` puis, à 1 s,
`ForceStartMission("1_01")`, et suit la machine à états ci-dessus. Une mission sans
script est refusée avec son nom.
`MissionSucceed` et `MissionFail` arrêtent `main` ; `MissionActive`,
`MissionActiveSpecific`, `MissionGetCurrentName` répondent pour de vrai.
`RequestModel`, `WeaponRequestModel` et `VehicleRequestModel` renvoient « chargé »
(le moteur renvoie vrai quand l'état de diffusion vaut 1 ; l'outil n'a pas de
diffusion). `DATLoad("1_01.DAT", 2)` et les tables `POINTLIST`, `PATH`, `TRIGGER`
sont réels (docs/trigger.md) : `PedCreatePoint(85, 534, 2)` reçoit maintenant le
handle de `_1_01_CHEERPOINTS` et non plus un bouchon. Les globales qui ne sont pas des
fonctions du moteur valent nil (voir plus bas, « Ce qui la bloquait »).

Les piétons des scripts sont un pool de 24 (`src/peds/ScriptPedPool`, le joueur en 0
comme `PlayerGetPedIndex` le dit, `tests/test_scriptpeds`) : `PedCreatePoint`,
`PedCreateXYZ`, `PedDelete`, `PedIsValid`, `PedIsPlayer`, `PedGetPosXYZ`, `PedSetPosXYZ`,
`PedGetHeading`, `PedFaceXYZ`, `PedIsDead`, `PedGetHealth`, `PedSetHealth`,
`PlayerGetHealth` répondent dessus ; un piéton qu'une mission laisse derrière elle est
libéré à la fin de la mission (le moteur le note à l'objet script, `FUN_005d94d0`).

Un coup passe par `FUN_0047b6b0` puis `FUN_004782a0` (`CScriptPedPool::Blesser`) : rien si la cible
a le drapeau 12 (+0x1344) ou est invulnérable (le drapeau 30, +0x138c : `PedSetInvulnerable`, 0x5c9160, et
`PlayerSetInvulnerable`, 0x5d1810, pour le joueur), ou si l'attaquant est le joueur (type 13) et la cible
invulnérable au joueur (le drapeau 20, +0x1364, `PedSetInvulnerableToPlayer`, 0x5c9200) ; sinon la santé (+0x1cb8) baisse sans passer sous 0 ni sous
la santé minimale (+0x1ca8, `PedSetMinHealth`, 0x5c8ef0). Dans `1_02`, le bully de l'entraînement (`GN_Bully05`) frappe
le joueur rendu invulnérable, et le joueur garde ses 150 points de vie.

Les piétons ont un geste à la fois, joué image par image (`CScriptPedPool::Avancer`) :
`PedMoveToPoint(ped, allure, liste, n)` et `PedMoveToXYZ(ped, allure, x, y, z)` font
marcher (allure 0 et 1), courir (2) ou sprinter (3 : les deux élèves de `_1_01_KIDRUN`)
vers un point, à 1,4 / 2 / 3,5 / 5 m/s (ordres de grandeur, à mesurer sur le jeu) ;
`PedStop` arrête ; `PedAttack(attaquant, cible, style)` lance une poursuite à la course
puis un coup de 10 points de vie toutes les 0,8 s à 1,2 m, jusqu'à la mort de la
cible (`PedIsDead`), le style n'étant pas joué ; `PedSetHealth(ped, 0)` tue ;
`PedMakeAmbient` rend le piéton à la population (il n'est plus ramassé avec sa
mission). Les arrivées, les coups et les morts passent au journal. Sur `1_01`, les
trois premiers personnages, rendus ambiants, marchent vers `_1_01_CHEERPOINTS` et y
arrivent entre 10 et 14 s ; l'élève qui sprinte vers le point 3 de `_1_01_KIDRUN`
n'y est pas encore quand la mission se termine, et son poursuivant (3,5 m/s contre
5) ne l'a pas rattrapé.

Résultat pour `1_01` : `MissionSetup` (chargement de l'arbre `Act/Conv/1_01.act`,
`DATLoad("1_01.DAT", 2)`, joueur placé, transition de zone) fini en 0,2 s ; `main`
coupe le contrôle du joueur, règle la météo, charge sept groupes d'animations et 17
modèles, règle les personnages uniques, la réputation des six factions, place le
joueur, vide la zone, crée les trois premiers personnages (`PedCreatePoint(85, …)`,
`99`, `145` sur `_1_01_CHEERPOINTS` et `_1_01_SECSPAWN`), les tourne vers le joueur,
leur pose `/Global/1_01/Talking`, rend le contrôle, pose le marqueur et l'objectif
`1_01_StandGround`, puis **attend que le joueur entre dans `_1_01_BULLYTRIG`**
(`PlayerIsInTrigger(483)` à chaque image). C'est le jeu qui attend le joueur, plus un
bouchon qui échoue.

**Jouer le joueur depuis l'outil.** `PlayerIsInTrigger`, `PedIsInTrigger` (la zone du
déclencheur, `CTriggerZone::Contient`), `PlayerSetPosPointArea`, `PlayerIsInAreaXYZ`
(un pavé de demi-côté « rayon », `FUN_005c2910`) et `AreaGetVisible` (`DAT_00bd1008`,
la zone où est le joueur) sont réels. Trois options font faire au joueur ce que le
moteur attend de la manette : `--entrer DECLENCHEUR@s` le pose dans un déclencheur,
`--aller LISTE[,n]@s` (ou `x,y,z@s`) à un point, `--zone N@s` lui fait passer une porte.
`1_01` lu avec `tools/lur.py` (fonction `CheckObjs`, ligne 684) :

    build/outils/lancer_lua 14 --mission 1_01 --sans-cinematiques --entrer _1_01_BULLYTRIG@4 --zone 2@6 --aller _1_01_ENDPOINT@8

(sans `--sans-cinematiques`, la cinématique `1-1-1` se joue pour de vrai pendant `MissionSetup`,
61,7 s, et tout se décale d'autant : docs/cinematiques.md.)

1. Objectif `1_01_StandGround` ; le joueur entre dans `_1_01_BULLYTRIG` → le premier
   élève redevient ambiant, un bully (modèle 69) est créé sur `_1_01_FIGHTBULLIES` et
   `PedAttack(5, 4, 3)` lance la bagarre (que l'outil ne joue pas).
2. `AreaGetVisible() == 2` (l'intérieur de l'école ; le `PlayerIsInAreaXYZ` sur
   `_1_01_ENDPOINT` qui précède est calculé mais n'entre pas dans le test compilé ; l'AREACODE
   5 du point est un reste de l'ancien intérieur du bureau, docs/zones.md) → la secrétaire
   (59) est créée sur `_1_01_SECRETARYINOFFICE`, la population revient. La zone change par
   une porte (`--zone 2@6`) : 14 images, un fondu, `MainMap.lua` retiré, `SchoolHallways.lua`
   chargé, et la zone visible passe à 2 à 7,66 s (`src/core/AreaManager`).
3. Puis deux élèves (69 et 102) sur `_1_01_KIDRUN`, `PedMoveToPoint`, `PedAttack`, et
   le joueur attendu dans `_1_01_CHECKOBJS1` (les deux élèves redeviennent ambiants).
4. Le joueur dans `_1_01_PRINCOFFICE` (le point 1 de `_1_01_ENDPOINT` est déjà dedans)
   et `gMissionSuccess` pas encore posé → contrôle coupé, `/Global/1_01/WaveToSec`,
   `SoundPlayScriptedSpeechEvent(M_1_01, 101)`, attente de `SoundSpeechPlaying`, puis
   `gMissionSuccess = true`, `gMissionRunning = false` : la boucle de `main` s'arrête,
   `L_StopMonitoringTriggers`, `L_CutsceneFade`, **`MissionSucceed(true, false, false)`**.

**`1_01` se termine, réussie, à 10,5 s** (0,80 s de parole de Jimmy à la secrétaire, docs/son.md) :
`main fini, MissionCleanup`, le nettoyage
rend les modèles uniques, décharge les animations, rend le contrôle, et les cinq
piétons laissés par la mission sont libérés.

Ce qui la bloquait n'était pas une fonction du moteur mais une globale de script :
`lancer_lua` répondait à toute globale inconnue par un bouchon (une table, donc
vraie), et `if gMissionSuccess then` était vrai avant même que la mission ne pose la
variable. Dans le jeu, `_G` ne contient que les 1 508 fonctions de `CreateLuaState`
et ce que `util.lua` pose ; tout autre nom vaut nil. `src/core/LuaApi` lit
`docs/api-lua-tables.tsv` et `IndexGlobal` ne bouchonne plus que ces noms-là
(`tests/test_luaapi`). Sur cette partie, trois globales de script sont lues avant
d'être posées, et valent nil comme il faut : `gMissionSuccess`, `misObj`, `F_StoreSweep`.

## `1_02` après `1_02_FightMsg` (23.10.2026)

Lu dans `1_02.lur` (`tools/lur.py`) : quand la leçon des coups au sol réussit, `F_MonitorTraining1` (stade 7) affiche
`1_02_FightMsg`, pose la santé minimale du bully (`GN_Bully05`, modèle 146) à 5 (`PedSetMinHealth`), sa colonne 8 à
100 (`GameSetPedStat`), `bCreateCrowd`, et passe la main à `F_MonitorTraining2`. Jusque-là, `T_MonitorBullyHealth`
remontait sa santé à la moitié de sa santé de départ quand elle passait dessous. `F_MonitorTraining2` attend que
`PedGetHealth(bully) == 5` : objectif `1_02_HumiliateHim`, `TutorialStart("HUMILIATEX")` ; puis que le joueur joue une
humiliation (`…/Humiliate_Me/5-8_IndianBurn`, `5-8_HitSelf`, `5-8_NoogieSpit` ou `…/WaitForVictim/EmergencyAnim`) :
`PlayerSetControl(0)`, santé minimale à -1 ; il attend la fin de l'humiliation, une santé nulle, que le bully joue
`/Global/HitTree/KnockOuts`, fond au noir et appelle `F_SetupRussellFight`, qui joue la cinématique (`F_DoCutscene`)
et pose l'état `passed`. `F_HealthTutorial` répond 0 ou 1 selon `bBullyNISLaunched`.

Avec l'outil (docs/cat.md, « Les dégâts, la santé et l'humiliation ») :

    G=/Global/Actions/Grapples/Front/Grapples
    build/outils/lancer_lua 95 --mission 1_02 --sans-cinematiques --porte DT_ischool_FrontDoorL@3 --entrer _GARYTRIGGER@6 \
      --jouer /Global/HitTree/Standing/BlockHits@42 --jouer /Global/GlobalLoco/Locomotion/Combat/0_BLOCK_0:Act/Globals/GlobalLoco.act@45 \
      --demander 0@46 --sequence @48 --sequence @51 --sequence @53 --jouer $G/GrappleAttempt/GrappleSuccess/GIVE@54 \
      --sequence @56 --jouer $G/GrappleMoves/GrappleStrikes/HitC/Charge@57 --jouer $G/GrappleAttempt/GrappleSuccess/GIVE@60 \
      --sequence @62 --sequence @63 --jouer $G/GrappleMoves/DirectionalPush/PushFwd@64 --sequence @66 \
      --jouer /Global/TrainingPlayer/Attacks/GroundAttacks/GroundAttacks/Strikes/HeavyAttacks@67 \
      --jouer /Global/TrainingPlayer/Attacks/Strikes/LightAttacks@72,73,74,75,76,77,78,79,80,81,82 \
      --jouer /Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init:Act/Player.act@84

(`LANCER_JOURNAL=300000` pour voir les objectifs : le journal s'arrête à 400 lignes.) Le coup de pied au sol de 67 s
ôte 8 points (95 → 87) et `1_02_FightMsg` s'affiche à 67,2 s ; le bully se relève (`BellyUpGetUp`, 67,9 s) et revient
à `Fight_Tutorial` ; chaque jab (`JAB`, 8 points) le fait réagir debout (`Standing/Melee`, qui exécute `Base/HitBase` :
sa `DamageHandle` porte les dégâts) ; au onzième, il est à 5 points : **`1_02_HumiliateHim` à 82,1 s**,
`TutorialStart("HUMILIATEX")` à 83,2 s. À 84 s, l'humiliation commence (`SetTargetAsGrappleTarget`) : le joueur joue
`WaitForVictim` et le bully descend dans `Humiliate_Me`, mais tombe dans la branche `Fail` (`HumiliationGroupIs` n'est
pas lue) et fait jouer `FAIL_PLAYER` au joueur, que le script n'attend pas : `1_02` s'arrête là.

## Suite

1. Ce que `1_01` ne joue pas encore pour de vrai : les nœuds d'action (`PedSetActionNode` cherche le nœud dans
   l'arbre global, docs/cat.md, mais ne le joue pas) ; et les vraies vitesses de marche. Les cinématiques se jouent (docs/cinematiques.md),
   `CameraFade` est une durée qui court (docs/zones.md).
2. Fait : +0xc est l'AREACODE (docs/trigger.md), et la zone d'un point n'est que la
   destination d'une téléportation ; la zone visible est celle du gestionnaire de zones
   (docs/zones.md). Restent les portes du gestionnaire (les paires de +0x10) et qui les enregistre.
3. Fait : `MissionCond`, `TimeCond`, `ConditionFalse` (« Les conditions ») ; `1_02` s'enchaîne.
   Restent `ConditionGroup`, `DayCond`, `CurrentMission` ; ce que `FUN_006a8650` demande à la manette avant de lancer (`FUN_007386f0`, `FUN_007387b0`),
   et les champs 52, 56 et 60 de `MissionButtonTrigger` (1, 95, 81 partout).
