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
`TutorialStart("HUMILIATEX")` à 83,2 s.

Le soir du 23.10 (docs/cat.md, « L'humiliation »), la même commande, avec 125 secondes au lieu de 95, mène `1_02` au
bout : au septième jab (76,16 s, 47 points sur 95, sous la moitié), le bully reçoit le groupe d'humiliation 0
(`HUMIL_5-8_A`, tiré pour un Medium). À 84 s, l'humiliation commence : le joueur joue `WaitForVictim` et rejoint sa
victime (`SeekEntityPosition`), le bully descend dans `Humiliate_Me/5-8_NoogieSpit/Success/Load`, demande le groupe
d'animations et retient sa ligne (`HUMILIATION_ANIM` 3). À 84,08 s, les conditions de `RCV` passent (groupe chargé,
ligne prête, le joueur à 0,97 m devant lui) : le bully joue `RCV` et fait jouer `GV` au joueur, que le script attend
(`…/Humiliate_Me/5-8_NoogieSpit`) ; sa ligne part à 84,41 s. `GV/END` (89,30 s) le renvoie dans `RCV/END`, qui exécute
`HumiliateFlee` : 500 points ôtés, il en avait 5 ; il joue `HitTree/KnockOuts` à 93,56 s, l'écran passe au noir et
`F_SetupRussellFight` joue la rencontre avec Russell (`EnterNIS` à 96,13 s, `MeetRussell`, `ExitNIS` à 119,10 s) puis
**`MissionSucceed` : `1_02` se termine réussie à 119,13 s**.

    build/outils/lancer_lua 125 --mission 1_02 … (les mêmes options)

Sans les conditions de `RCV`, le joueur passe en `WaitForVictim/EmergencyAnim` au bout de 5 s (89,03 s) et le script
l'accepte aussi : `1_02` réussit alors à 117,38 s.

La nuit du 23.10 (docs/cat.md, « Les arbres de l'IA »), la même commande joue l'arbre de l'IA du bully à la place du
geste `PedAttack` : `PedSetAITree(1, "/Global/AI_FightTutorial", …)` et `PedAttack(1, 0, 3)` (un objectif d'attaque)
le font entrer dans `Combat/…/CombatLoco` à 30,06 s ; il se tient à 1,5 m du joueur (la zone de combat 0), son
`CombatActions` boucle (`ControlNode`, `PostHit/OnGround`, `PostHit/Hit` ; `Offense` attend `F_CanBullyAttack`), et c'est
la mise à jour de `ManageTargets` qui lui donne le groupe 0 à 76,20 s, au septième jab. À 89,40 s, `GV/END`, qu'aucune
piste ne retient, rend la main à l'arbre du joueur ; le bully joue `RCV/END`, perd son objectif d'attaque, reçoit un
`FleeObjective` (`HumiliateFlee`), se retourne et tombe en avant (`KnockOuts/Rear/Rear`, 93,65 s) sans toucher le joueur
resté dans son dos ; la rencontre avec Russell suit et **`MissionSucceed` : `1_02` se termine réussie à 119,03 s**.

    build/outils/lancer_lua 125 --mission 1_02 … (les mêmes options ; LANCER_IA=1 pour les contrôleurs des spawn)

Limites : le bully ne court pas pendant `RunStart` (la piste `Locomotion` exécute une intention de mouvement que rien ne
pose dans `AI_FightTutorial` pour l'objectif 22) ; l'intention `Short` de `CombatActions` ne fait pas jouer
`Offense/Short` à son arbre d'actions (les passages viennent des `ActionOpps` des références montées, la pile du
contexte) ; un coup ne touche qu'une cible devant l'attaquant (supposé, faute des sphères des os).

Le 26.10 (docs/cat.md, « Les références montées ») : `LuaCond` appelait la fonction dans `_G`, où les noms d'une mission
n'existent pas (`CreateNameSpace`) : `F_CanBullyAttack` et `F_CanBlock` valaient toujours nil, donc 0. Appelée par `NSCall` dans
l'espace de noms de `1_02.lua`, comme `FUN_005d8000`, `F_CanBullyAttack` rend 1 pendant la leçon de parade : de 31,12 s à
42,60 s, l'intention `Short` de `CombatActions` fait passer l'`Opportunity` `../../../Offense` des `ActionOpps` de
`Locomotion.act` (montées sous `Fight_Tutorial/Default_KEY`) et le bully enchaîne `Offense/Short/Strikes` : le moulinet
(`WINDMILL_R`, puis `Windmill_L`, puis le coup lourd `WINDMILL_HEAVY_R`), dont les coups portent sur le joueur (invulnérable,
il réagit dans `HitTree/Standing`) ; la leçon passe ensuite à la prise (`F_CanBullyAttack` à 0). `1_02` reste réussie à
119,03 s.

Le 27.10 (docs/cat.md, « Les conditions lues le 27.10.2026 ») : la même commande ; les conditions du HUD, des manettes, des
armes, du social, des caméras, `CollisionStart`, `AttackPhase`, `AttackTimer`, `Attacked`, `FightClass`, `RangeToFocus` se
décident. Ce qui change dans la partie : le joueur passe par `GrappleOpps` après chaque `GIVE` (54,19 et 60,19 s, `Controller`
lu), par `Combat/CombatTransition/CombatIdle_Out` après la parade (45,01 s, `AttackTimer`), et le bully qui gagne sa zone joue
`Locomotion/Free/Sprint` au lieu de `RUN` (30,10 s : la vitesse 1,5 du pas de `CombatLoco`). `CameraGetActive` rend 1 et l'écran
est large de 22,80 à 29,40 s. `1_02` reste réussie à 119,03 s, `1_01` à 10,46 s ; au bilan de fin ne restent que `WeaponNeedInit`
(762), `MissionCond` (28), `ConversationActive` (19), `PedIsInOutfit` (17), `OnMission`, `PropTargetInteractive` et
`ParametricTime`.

Le 27.10 au soir (docs/cat.md, « La vitesse de l'intention, lue le 27.10.2026 (soir) ») : l'approche de `CombatLoco` s'arrête au
petit rayon (0,9 m, `FUN_0062c740`), plus au grand ; le bully se tient plus près du joueur, qui le rejoint plus tôt pour
l'humiliation (`GV` à 84,12 s, `RCV/END` à 89,33 s) : **`1_02` se termine réussie à 118,93 s**. Pendant `RunStart`, le joueur est sous
le petit rayon : la vitesse de l'intention est 0 dans le jeu aussi, le bully tombe sur place. `MissionCond` et `OnMission` sont jugées dans les arbres
d'actions : la charge de la prise (57 s) se lâche d'elle-même (`Charge/Release`, 57,09 s), le cours de lutte `C_Wrestling_1` n'étant
pas réussi. Avec `AttackerConditionGroup` et la fin demandée par une `Opportunity` sans nœud (docs/cat.md), le bully pare quatre
jabs (73,16, 82,14, 84,15, 86,16 s) ; la commande devient :

    build/outils/lancer_lua 130 --mission 1_02 … (les mêmes options, sauf :)
      --jouer /Global/TrainingPlayer/Attacks/Strikes/LightAttacks@72,73,74,75,76,77,78,79,80,81,82,83,84,85,86 \
      --jouer /Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init:Act/Player.act@87

et **`1_02` se termine réussie à 121,94 s** (`1_02_HumiliateHim` après le jab de 85,14 s, 7 → 5 points).

Le 03.11 (la condition `stat` et son tirage, 0x5ffff0 : la colonne de pedstats.dat devient une chance en pour cent), le bully pare
selon sa colonne de parade : six ou sept jabs sur quinze. La commande allonge la série et repousse l'humiliation :

    build/outils/lancer_lua 140 --mission 1_02 … (les mêmes options, sauf :)
      --jouer /Global/TrainingPlayer/Attacks/Strikes/LightAttacks@72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89 \
      --jouer /Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init:Act/Player.act@90

Le bully tombe à 5 points à 89,13 s (`1_02_HumiliateHim`), `RCV/END` à 95,37 s, `KnockOuts/Rear/Rear` à 99,40 s, puis la rencontre
avec Russell : **`1_02` se termine réussie à 124,97 s**.

**Le vrai chemin (05.11.2026).** Jusque-là, la cinématique des bullies (la fonction 35 de `1_02.lur`, lancée à 22,27 s) venait d'un
bouchon : `T_MonitorBullies` (la fonction 64, créée par `F_SetupBoysDormBullies` à 6,01 s) lance `F_FireBullyNIS` quand
`PedGetWhoHitMeLast` d'un des trois bullies rend le joueur, et le bouchon rendait 0, la poignée du joueur. Réelle (0x5c8c60), elle ne
rend rien tant que personne ne les a frappés. La fonction 8 lance aussi la cinématique quand le joueur est dans `_1_02_RUSSELTEASE` :
la commande y entre à 22,3 s. La cinématique part à 22,31 s, une image plus tard, et le combat tire ses parades autrement ; la série de
jabs va jusqu'à 96 s et l'humiliation part à 98 s :

    build/outils/lancer_lua 160 --mission 1_02 --sans-cinematiques --porte DT_ischool_FrontDoorL@3 --entrer _GARYTRIGGER@6 \
      --entrer _1_02_RUSSELTEASE@22.3 … (les options de la parade, de la prise et du coup au sol, comme plus haut) \
      --jouer /Global/TrainingPlayer/Attacks/Strikes/LightAttacks@72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96 \
      --jouer /Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init:Act/Player.act@98

Le bully tombe à 5 points à 93,16 s (`1_02_HumiliateHim`), `RCV/END` à 103,32 s, `KnockOuts/Rear/Rear` à 107,55 s : **`1_02` se termine
réussie à 132,92 s**.

## `1_03` (27.10.2026, nuit)

`1_03` (« The Setup ») s'enchaîne après `1_02C` et `1_02B`, entre 8 h et 19 h (« Les conditions »). Lancée seule
(`build/outils/lancer_lua 60 --mission 1_03 --sans-cinematiques`), son `MissionSetup` se joue jusqu'au bout (1,25 s : la cinématique
`1-03` chargée puis sautée, `1_03.DAT`, douze groupes d'animations, quatre arbres, la population vidée) ; `main` crée les bullies,
Davis (modèle 99, `1_03_Davis.act`, invulnérable, sprint infini) et Peter, puis lance la poursuite d'introduction :
`PedFollowPath(Davis, PATH._1_03_SCHOOLFLEE, 0, 3, cbSchoolFlee)` et `PedFollowPath(joueur, PATH._1_03_PURSUE, 0, 2,
cbIntroPursueDone)`, et attend `bIntroPursueDone`, que pose le rappel quand le joueur atteint le dernier nœud de son trajet
(`node == PathGetLastNode(path)`, lu dans `1_03.lur`). L'outil s'y arrêtait ; depuis la piste `FollowPath`, la poursuite démarre.

Ce qui est lu du suivi d'un trajet (le code est dans `src/peds/ScriptPedPool` et `lancer_lua`) :

- **`PedFollowPath`** (0x5cfbf0, réel dans `lancer_lua`) : un `FollowPathObjective` (type 12, `FUN_0046fc20`) ; le trajet +0x10, le
  départ +0x14 (sixième argument), le mode +0x24 (troisième), la vitesse +0x3c (l'allure du quatrième, `FUN_005c6180`), trois rappels
  Lua (+0x2c, cinquième ; +0x30, septième ; +0x34, huitième). Un objectif neuf arrête le contrôleur de l'IA et vide la file.
  **`PathGetLastNode`** (0x5bc600) : le nombre de nœuds moins 1, nil sans trajet.
- **La piste `FollowPath`** (0x93800c) : son lancement (`FUN_0062c290`) recopie l'objectif (`FUN_0062c1e0`) ; sa mise à jour
  (`FUN_0062eb70`) attend la fin de l'attente d'un nœud (+0x1c), puis compare la distance à plat au nœud visé au rayon du piéton
  (+0x51c, `PedPathNodeReachedDistance` 0x5cd930) ; atteint, le rappel +0x2c est appelé avec (piéton, trajet, nœud)
  (`FUN_0062e640`), puis les actions du nœud (`FUN_0062e780`), son attente, ou le nœud suivant (`FUN_0062e5f0` → `FUN_0061acc0`) ;
  sinon le pas (`FUN_0062ee20` : le but lissé `FUN_00621a30`, la navigation `FUN_004a4eb0`). Entre deux nœuds sans arrêt, le passage
  se voit au **plan** du nœud (`FUN_0061afa0` le pose : la direction vers la suite, ou dans un virage la perpendiculaire à la
  bissectrice ; `FUN_0062eaa0` le teste à chaque image et appelle les deux rappels, +0x2c puis +0x30). Les nœuds d'arrêt
  (`FUN_0061ad60`) : des actions, une attente, le dernier nœud des modes 0 et 3, les bouts de l'aller-retour. Le nœud suivant
  (`FUN_0061acc0`) : une fois (0), en boucle (1), aller-retour (2, le sens +0x34), sur place (3). La fin retire l'objectif
  (`FUN_00621c80`). `ScriptPedSuiviSuivant`, `ScriptPedSuiviArret`, `ScriptPedSuiviPoserPlan`, `ScriptPedSuiviPlanPasse`,
  `tests/test_scriptpeds`.
- **L'arrivée à un nœud** (`FUN_0062eb70`, lu à l'assembleur de 0x62ec10 à 0x62edd3) : la distance à plat sous le rayon (+0x51c),
  multiplié par 3 à vélo sauf au dernier nœud d'un trajet suivi une fois ; une attente positive (+0x1c du nœud) fait attendre la piste
  jusqu'à maintenant + attente × 1000 ms (`_DAT_0090d3d8` = -1000, soustrait) et tourner le piéton vers le lacet du nœud moins 90
  degrés (`FUN_0062c300`, plus 180 à vélo). `PedPathNodeReachedDistance` est réelle (`ScriptPedSuiviAtteint`, `ScriptPedSuiviEcheance`,
  `ScriptPedSuiviCap`). Dix scripts seulement l'appellent, contre 105 qui suivent un trajet.
- **Le rayon par défaut** : 1 m. Il n'est écrit ni par le constructeur de `CPed` (`FUN_0047ffe0`) ni par celui du composant de visée
  (`FUN_0048c1b0`), dont +0x51c est le +0x160, mais par sa remise (`FUN_0048c430`, `fld1` en 0x48c5c9 puis 0x48c5d1), que la remise
  du piéton (`FUN_00474f50`, 0x474f71) appelle depuis le constructeur.
- **La piste jouée par l'outil** (`AvancerSuivis`, comme un geste : le moteur la lance depuis l'arbre de l'IA,
  `GeneralObjectives/FollowPath`) : le lancement depuis l'objectif, l'attente, l'arrivée sous le rayon avec les plans en attente
  (`FUN_0062e640`), le but lissé à 3 × la vitesse de l'intention + 3 m devant la projection sur le segment (`FUN_00621a30` →
  `FUN_005e7560`, qui fait avancer le nœud visé quand le but dépasse un nœud sans arrêt), le passage des plans (`FUN_0062eaa0`),
  la fin qui retire l'objectif ; l'arrêt rend son état à l'objectif (`FUN_0062c230`). Le pas va droit vers le but, à la vitesse de
  l'allure de l'outil ; les actions des nœuds (`FUN_0062e780`) ne sont pas suivies. `DistanceBetweenCoords3d` (0x5bfac0) et
  `DistanceBetweenCoords2d` (0x5bfc10) sont réelles.
- **Dans `1_03`** : le joueur suit `_1_03_PURSUE` (deux nœuds, 2,28 et 3,37 s), le rappel pose `bIntroPursueDone`, le script rend la
  main (`PlayerSetControl(1)`, `TutorialStart("SPRINT1")`, l'objectif `1_03_INSTRUC02` à 3,40 s) ; Davis file sur `_1_03_SCHOOLFLEE`
  au sprint (nœuds 0 à 5 de 2,31 à 7,99 s). Le joueur de l'outil ne le poursuit pas : `1_03_D13` (8,81 s, il s'éloigne), puis
  `MissionFail("1_03_D11")` à 28,81 s. La suite est de jouer la poursuite (`--aller` sur le trajet de Davis). Dans `1_02`, le piéton 2
  suit le trajet 21 depuis 2,11 s, jusqu'à ce que le script le supprime (22,04 s) : rien d'autre ne change.

**La poursuite jouée (02.11.2026).** `--suivre PIETON[,d]@s` joue la manette du joueur : à partir de s secondes, il court au sprint
sur la trace du piéton (ses positions, une tous les 0,5 m) tant qu'il en est à plus de d mètres, sauf quand le script a repris le
contrôle (`PlayerSetControl(0)`) ou que le joueur suit lui-même un trajet. Lu dans `1_03.lur` : `ChaseOneLoop` échoue par
`F_CheckDistanceFailure` (`PlayerTooFarFromDavis` : plus de 35 m, `DistanceBetweenPeds3D` de la bibliothèque Lua sur `PedGetPosXYZ` ;
ou les déclencheurs `_1_03_FAILTRIGGER01`, `02`, `04`), et passe à `ChaseTwoSetup` quand `cbSchoolFlee` voit Davis au dernier nœud de
`_1_03_SCHOOLFLEE`. Avec `--suivre 6,3@3.4`, `1_03` n'échoue plus : Davis finit `_1_03_SCHOOLFLEE` à 18,51 s, `ChaseTwoSetup` le lance
sur `_1_03_FLEETOFOUNTAIN` (trajet 56, jusqu'au nœud 9 à 34,42 s) et crée `T_Chase2_MeetPeter`, `T_ChasePartTwo`, `T_RunFights`,
`L_MonitorTriggers`, `T_WaveMonitor`, `T_DoorMonitor`. `T_ChasePartTwo` attend le joueur dans `_1_03_DAVISREGISTERHIT`, une dalle
d'un mètre (z de 7,30 à 8,30) que la trace de Davis frôle 0,5 m plus bas : sans le sol du monde, l'outil y entre par
`--entrer _1_03_DAVISREGISTERHIT@29`, et le script pose Davis à `_1_03_DAVISBRAG` (34,45 s).

    build/outils/lancer_lua 120 --mission 1_03 --sans-cinematiques --suivre 6,3@3.4 --entrer _1_03_DAVISREGISTERHIT@29

La suite (02.11.2026) : `T_ChasePartTwo` attend que le joueur frappe Davis ou le bully 5 (`DavisGroupHitRegister` enregistre
`cbDavisGroupHit` sur eux deux à chaque image, `RegisterPedEventHandler`, l'événement 0 que la méthode du coup déclenche) ou
qu'il entre dans `_1_03_FIGHT1`. `--viser 5@35` (le bouton de verrouillage) et un `LightAttacks` à 38 s : le coup porte à 38,15 s,
le rappel pose `bDavisGroupHit` et `DavisBreakForGarage` lance Davis sur `_1_03_FLEEFROMFOUNTAIN` (trajet 55) et le bully 5 sur le
joueur (`PedClearObjectives`, `PedAttackPlayer`). Derrière Davis, le joueur entre dans `_1_03_DAVISRUN` (41,05 s) ; Davis file au
garage (`_1_03_FLEETOGARAGE`, trajet 54) et y attend, endormi, tourné vers le joueur (50,56 s). Les vagues de `WaveInit` attendent
le joueur dans `_1_03_FIGHT3` (derrière les tonneaux), `_1_03_FIGHT4` (le garage), `_1_03_FIGHT5` et `_1_03_LADDER_DROP`. Le bully 5
n'a pas d'arbre de l'IA (`1_03` ne lui en pose pas ; celui des `.idb`, `/Global/AI`, n'est pas posé par l'outil) : le geste réduit
de l'outil le fait rattraper et tuer le joueur à 62,67 s.

    build/outils/lancer_lua 120 --mission 1_03 --sans-cinematiques --suivre 6,3@3.4 --entrer _1_03_DAVISREGISTERHIT@29 \
      --suivre 5,0.8@35 --viser 5@35 --jouer /Global/Player/Attacks/Strikes/LightAttacks@38 --suivre 6,1@38.5

Le 03.11 (docs/suite.md) : les piétons reçoivent les arbres de leur modèle à la création (`FUN_00484d10` : `/Global/B_Striker_A` et
`/Global/AI` pour les bullies). Avec la même commande, Peter finit `FistShakeEnd` (27,59 s) ; le bully 5 attaque le joueur par la
banque `Combat` de `AI.act` (39,04 s) et tourne en `CombatLoco` ; le joueur reçoit les jetons de combat de ses agresseurs
(`FightToken`, `FUN_00486e40` : le bully 5 à 39,77 s) ; le bully 1, sorti de sa cachette par `T_RunFights` (45,24 s), puis le bully 5
prennent les droits d'attaque (62,20 et 66,83 s) et frappent par `/Global/B_Striker_A/Offense` (`LightAttacks`, `Windmill_L`,
`HeavyAttacks`, 6 et 8 points). Le joueur de l'outil ne riposte pas : il tombe à 0 point vers 98 s (`HitTree/KnockOuts`) et meurt par
la piste `Death` à 101,31 s. La suite est de le faire se battre (`--viser`, `--jouer`) pour entrer dans les vagues.

**La riposte (04.11.2026) : `1_03` se termine réussie à 148,90 s.** Lu dans `1_03.lur` : les vagues de `WaveInit` ne demandent
pas seulement d'entrer dans leurs déclencheurs. La vague des tonneaux (`barrelWave`, le bully 1, modèle 85, caché sur
`_1_03_BULLY3`) part quand le joueur, derrière Davis, entre dans `_1_03_FIGHT3` (45,24 s) ; les deux autres partent des portes du
garage. Chaque porte (`DoorsInit`) a un déclencheur, que `DoorsAssociateDavis` attache à Davis : quand Davis en sort
(`L_MonitorTriggers`, l'événement 3), `DavisExitDoorTrigger` la ferme et allume son corona ; le dessin du corona
(`PlayerIsInAreaXYZ(…, 0.75, 7)`) ne compte pas, c'est `cbDoorOpened`, qu'appelle la piste `Script` du nœud
`…/ParametricDoor/POpenUp/PedPropActions/#3be30de2/LiftOpen/success` quand le joueur soulève la porte, qui lance
`GarageFight` (porte 1 : le bully du garage, modèle 145, et Davis qui file sur `_1_03_FLEEGARAGE`) puis `FinalFight` (porte 2 :
les deux gardiens, 146 et 147, et Davis qui monte sur les tonneaux). Les vagues mortes (`F_PedIsDead`), Davis (200 points,
`cbDavisHit` sur l'événement 0) tombe au troisième coup du joueur : `CutsceneDavisBegs` (`FallOffBarrels/NewFall`, dont les
pistes `Script` appellent `cbBobbleDone` et `cbFallDone`, puis `PlayerPickupSlingshot`) et `MissionSucceed`. Dans le jeu, Davis
est frappé de loin (les briques, armes 311 et 315 que `T_BossBattleTutorial` attend) ; l'outil, sans projectiles, fait porter le
coup sur la cible verrouillée sans regarder la distance : le jab tient lieu du jet.

    R="--suivre 1,0.8@60 --viser 1@60 --jouer /Global/Player/Attacks/Strikes/LightAttacks@61,61.6,62.2,62.8,63.4,64,64.6,65.2,65.8,66.4,67,67.6"
    R="$R --suivre 5,0.8@68.5 --viser 5@68.5 --jouer /Global/Player/Attacks/Strikes/LightAttacks@69,69.6,70.2,70.8,71.4,72,72.6,73.2,73.8,74.4,75"
    P="/Global/1_03/ParametricDoor/POpenUp/PedPropActions/#3be30de2/LiftOpen/success:Act/Conv/1_03.act"
    R="$R --aller _1_03_DOOR1CORONA@80 --jouer $P@81"
    R="$R --suivre 4,0.8@82 --viser 4@82 --jouer /Global/Player/Attacks/Strikes/LightAttacks@83,83.6,84.2,84.8,85.4,86,86.6,87.2,87.8,88.4,89,89.6"
    R="$R --aller _1_03_DOOR2CORONA@96 --jouer $P@97"
    R="$R --suivre 2,0.8@100.5 --viser 2@100.5 --jouer /Global/Player/Attacks/Strikes/LightAttacks@101,101.6,102.2,102.8,103.4,104,104.6,105.2,105.8,106.4,107,107.6"
    R="$R --suivre 3,0.8@108.5 --viser 3@108.5 --jouer /Global/Player/Attacks/Strikes/LightAttacks@109,109.6,110.2,110.8,111.4,112,112.6,113.2,113.8,114.4,115,115.6"
    R="$R --suivre 2,0.8@116.5 --viser 2@116.5 --jouer /Global/Player/Attacks/Strikes/LightAttacks@117,117.6,118.2,118.8"
    R="$R --suivre 6,0.8@122 --viser 6@122 --jouer /Global/Player/Attacks/Strikes/LightAttacks@124,126,128"
    build/outils/lancer_lua 160 --mission 1_03 --sans-cinematiques --suivre 6,3@3.4 --entrer _1_03_DAVISREGISTERHIT@29 \
      --suivre 5,0.8@35 --viser 5@35 --jouer /Global/Player/Attacks/Strikes/LightAttacks@38 --suivre 6,1@38.5 $R

(sous zsh, `${=R}`.) Les bullies 1 et 5 meurent à 70,62 et 78,01 s (douze et onze jabs ; chacun tombe à 0 point, joue
`HitTree/KnockOuts` puis la piste `Death` environ 2,9 s plus tard ; un piéton à terre que l'outil frappe encore recommence son
`KnockOuts`, d'où l'arrêt des jabs à 0 point) ; la porte 1 s'ouvre à 81,02 s, le bully du garage (le piéton 4) meurt à 92,63 s ; la
porte 2 à 97,02 s, les gardiens (3 et 2) à 118,64 et 121,77 s, deux jabs sur le 2 manquant « dans son dos » ; `Chase2_HandleBarrelFight`
arme Davis (`PedSetTetherToPoint`, `PedSetWeapon(6, 303, 6)`, `PedCoverSet`) à 117,78 s ; les coups de 124, 126 et 128 s le mènent à
140, 80 puis 18 points, `CutsceneDavisBegs` part à 128,17 s (`PlayerSetControl(0)` : la manette de l'outil est coupée),
`Victory` finit à 138,01 s (le joueur a maintenant son arbre `/Global/Player`) et `MissionSucceed(true, false, false)` à 148,86 s.

**Les briques (05.11.2026) : Davis touché de loin.** `GetPointListSize` réelle, `CreateRespawnableBricks` (dans `ChaseOneSetup`) crée
les trois briques de `_1_03_PICKUPBRICKS` (le modèle 311, `PickupCreateXYZ(311, …, "BrickBute")`) à côté des quatre couvercles (315).
La riposte peut finir comme dans le jeu : après la dernière vague, au lieu des trois jabs sur Davis, le joueur va à chaque brique, la
ramasse, vise Davis et la lance (`--ramasser`, `--tirer`) :

    B="--aller 130.631,8.68252,6.16985@122 --ramasser @122.3 --viser 6@122.4 --tirer @122.6"
    B="$B --aller 127.722,16.6468,6.16985@124 --ramasser @124.3 --viser 6@124.4 --tirer @124.6"
    B="$B --aller 121.868,21.2301,6.09985@126 --ramasser @126.3 --viser 6@126.4 --tirer @126.6"
    (la commande de la riposte jusqu'à --jouer …/LightAttacks@117,117.6,118.2,118.8, puis $B à la place de la dernière ligne de $R)

Chaque brique (la fiche BRICK : 0,38 m par pas, 30 points) touche Davis sur ses tonneaux (123,72, 125,30 et 127,22 s : 140, 80 puis 18
points, `cbDavisHit`) ; `T_BossBattleTutorial` affiche `1_03_T02` quand le joueur tient la deuxième ; `CutsceneDavisBegs`, puis
`PlayerPickupSlingshot` met le lance-pierre en main (`PropSetInTransition`, `PropAttach`) : **`1_03` se termine réussie à 148,04 s**.

## `1_04` (« The Slingshot ») (05.11.2026)

`1_04` s'enchaîne après `1_03`. Lancée seule (`build/outils/lancer_lua 120 --mission 1_04 --sans-cinematiques`), `MissionSetup` finit à
3,23 s (la cinématique `1-04` sautée, le dortoir puis `MainMap`) ; `main` crée Gary (le piéton 4, modèle 130, `STAT_GS_GARY`), donne
au joueur le lance-pierre (`GiveWeaponToPlayer(303)`, `GiveAmmoToPlayer(303, 30)` : 1 dans l'inventaire, le maximum de sa fiche, et des
billes sans fin) et lance Gary vers `_1_04_NEMESISPARKING` (objectif `1_04_OBJ01`). Lu dans `1_04.lur` (`main`, la fonction 4, appelle à
chaque image l'étape en cours, son upvalue 7) :

1. `F_StageOneGetToBus` : le joueur dans `_1_04_PARKING`. La cinématique de Gary (la fonction 13) pose Gary à côté du joueur
   (`PedGetOffsetInWorldCoords`), qu'il ne faut pas laisser en bouchon (un seul 0 arrêtait `main`).
2. `F_StageTwoShootTheBottles` (objectif `1_04_BOT`, `TutorialStart("Shooting")`) : casser au lance-pierre au moins trois des cinq
   vitres du bus (les entrées spec `tschool_BusWindow`, `…01` à `…04` de `tschool.ipb`, le modèle BusWindow, 1 point de vie), que
   `OnObjectBrokenCallback` reçoit par `RegisterHashEventHandler(haché, 3, …)` ; `PAnimDestroyedByPed` doit rendre le joueur ;
   `ObjectNumProjectileImpacts(_1_04_BOTTLEZONE, 304)` compte les marques des billes.
3. `F_StageThreeGetToField` (objectif `1_04_OBJ02`) : le joueur dans `_1_04_FIELDTRIG` ; Gary montre l'arbre (`1_04_TREE`).
4. `F_LaunchFieldBullies` : le joueur dans `_1_04_TREE2_TRIG` et dans l'arbre (`PedIsPlaying(gPlayer, "/Global/TreeClimb/Actions/ON_BOT")`,
   l'arbre `pxTree.cat` de Stream/World.img) ; six jocks (22 points de vie) et Burton.
5. `F_TreeAssault` : les jocks font leurs tours puis leurs sauts sur place ; `F_CheckHit` (`RegisterGlobalEventHandler(7, …)`) compte
   les coups de bille (`PedGetLastHitWeapon` rend 304) ; les six à 0 point, `F_CheckBullyAttacked` mène à `MissionSucceed`.

Avec l'outil :

    F=/Global/SlingShot/SlingShot/Actions/controller/Upperbody/#3da01ef7/Charge/Release/Release
    V="--arme 303@15"
    V="$V --viser-objet tschool_BusWindow01@16 --jouer $F@17 --viser-objet tschool_BusWindow02@18 --jouer $F@19"
    V="$V --viser-objet tschool_BusWindow03@20 --jouer $F@21 --viser-objet -@22"
    A="--entrer _1_04_FIELDTRIG@26 --entrer _1_04_TREE2_TRIG@33 --jouer /Global/TreeClimb/Actions/ON_BOT:Act/Props/pxTree.act@34"
    T="--viser 13@73.28 --tirer @73.35 --viser 12@74.00 --tirer @74.07 --viser 13@74.14 --tirer @74.21 --viser 11@74.47 --tirer @74.54"
    T="$T --viser 12@74.80 --tirer @74.87 --viser 13@74.94 --tirer @75.01 --viser 10@75.08 --tirer @75.15 --viser 11@75.27 --tirer @75.34"
    T="$T --viser 9@75.41 --tirer @75.48 --viser 12@75.60 --tirer @75.67 --viser 10@75.74 --tirer @75.81 --viser 11@76.07 --tirer @76.14"
    T="$T --viser 9@76.21 --tirer @76.28 --viser 8@76.35 --tirer @76.42 --viser 10@76.49 --tirer @76.56 --viser 9@76.96 --tirer @77.03"
    T="$T --viser 8@77.10 --tirer @77.17 --tirer @77.83"
    build/outils/lancer_lua 90 --mission 1_04 --sans-cinematiques --entrer _1_04_PARKING@10 $V $A $T

(sous zsh, `${=V}`…) Le joueur prend le lance-pierre au HUD (`--arme`, 15,05 s : `Weapons/SelectActions`, `PropGetDesired`,
`PropAttach`, son contexte `/Global/SlingShot`), casse les trois vitres (17,29, 19,27 et 21,29 s : la bille part 1 m au-dessus de ses
pieds et touche la vitre visée 0,23 s plus tard), suit Gary au terrain, monte dans l'arbre et tire depuis lui, pendant leurs sauts
(73,23 à 79,13 s : trois secondes chacun), trois billes de 9 points sur chacun des six jocks ; le sixième tombe à 0 à 78,14 s, Burton
s'en prend au joueur (`F_BurtonPissed`), `MinigameSetCompletion("M_PASS")` et `MissionSucceed` à 81,02 s : **`1_04` se termine
réussie à 81,05 s**. Les jocks qui font leurs tours ne sont pas touchés : la bille part vers la position du piéton au moment du tir.

**La charge du lance-pierre** (06.11, séance 6) : la feuille `Charge` de SlingShotActions.cat (`SLNG_CHARGE_T`) pose la charge de l'objet
arme à 0 (`WeaponSetCharge`) et la fait monter à chaque image (`WeaponIncrementCharge` : la moitié du pas pour le lance-pierre) ;
`WeaponFire` la lit, bornée à [0, 1], et la remet à -100. La commande ci-dessus joue directement `…/Charge/Release/Release` : la charge
reste vide, chaque bille garde ses 9 points, et `1_04` finit toujours à 82,53 s.

## `1_05` (« Save Algie ») (06.11.2026)

`1_05` s'enchaîne après `1_04`. Lancée seule (`build/outils/lancer_lua 90 --mission 1_05 --sans-cinematiques`), elle bouclait
dans `F_SetupWorld` (« `while not AreaDisablePatrolPath(PATH._SGD_PREFECT2) do Wait(0) end` ») : les rondes des préfets sont
recréées (`src/peds/Patrouilles`). Lu dans `1_05.lur` (`main`, la fonction 2 : `F_Intro`, puis `F_Stage1`, la boucle des étapes) :

1. `F_Intro` (la fonction 5) : les modèles, `F_SetupWorld`, `F_Stage1_Setup` (le premier bully, le piéton 4, modèle 99, invulnérable ;
   la bande de droite, 5 et 6, sur `_1_05_SPAWNBULLIESRIGHT` ; celle de gauche, 7 à 9 ; Algie, le piéton 10, modèle 4,
   `PedSetMissionCritical`, `PedRecruitAlly(gPlayer, Algie)`), l'objectif `1_05_MOBJ_01`.
2. `F_Stage1Objectives` (la fonction 15, à chaque image) : le joueur dans `_1_05_TRIGGERJOCKSNEARLIBRARY` lance le premier bully sur
   `_1_05_ROUTEFIRSTBULLY` ; au nœud 2, `F_Stage1FirstBullyAttacking` : il attaque Algie (`1_05_MOBJ_00`). Sous la moitié de
   `bullyMaxHealth` (sa santé de départ), il fuit sur `_1_05_ROUTEBULLYFLEENEW` ; au nœud 1, `F_routeBullyFleeNew` lance la bande de
   droite sur le joueur (la bande de gauche part si le joueur entre dans `_1_05_TRIGGERJOCKPACK` ou la frappe). Le premier bully mort,
   et les bandes lancées mortes : `F_UnlockStage2` (`1_05_MOBJ_02`, la minuterie de 180 s : la manquer, c'est `F_AlgiePissSelf`).
3. `F_Stage2` quand le joueur est dans l'école (`T_MonitorPlayerLocation` : `AreaGetVisible() == 2`) et la bande battue :
   `F_SetupInteriorPeds`, `T_Stage2_Locates`, `T_BathroomAntics`. Algie dans `_1_05B_BATHROOMSECONDFLOOR` (ou dans les toilettes du
   premier étage, où deux jocks attendent) : `F_CutAlgieToStall`, `F_StopTimer`, `F_EncounterBathroomSecondFloor` ; Algie dans la
   cabine, `F_BathroomSequence` (Eunice et le piéton 11 sortent, deux bullies tendent une embuscade), la salle vide
   (`PedFindInAreaXYZ`, 12 m) et quinze secondes : la chasse d'eau, Algie au lavabo.
4. `F_Stage3` : Algie suit le joueur à son casier (`_1_05B_ALGIELOCKER`) ; `F_NIS_AlgieLocker` attend qu'Algie joue
   `/Global/NLockA/PedPropsActions/Interact/NPCGetItem/GetBooks/…/CloseDoor` (le casier, un objet animé du déclencheur `NLock02B06`,
   par `/Global/1_05/Anims/Rummage/Locker` : `TargetProp`, `PropInteract`), puis `MissionSucceed`.

Avec l'outil (sous zsh, `${=…}`) :

    LA=/Global/Player/Attacks/Strikes/LightAttacks
    J1=13.5,14.1,…,24.3 (dix-neuf jabs) ; J2=28,28.6,…,37 (seize) ; J3=39,39.6,…,48 (seize)
    build/outils/lancer_lua 200 --mission 1_05 --sans-cinematiques --courir 187.3,-132.2,6.1@5 \
      --suivre 4,0.8@12.5 --viser 4@12.5 --jouer $LA@$J1 --suivre 5,0.8@27 --viser 5@27 --jouer $LA@$J2 \
      --suivre 6,0.8@38 --viser 6@38 --jouer $LA@$J3 \
      --courir 209.2,-73.3,6@59 --porte DT_tschool_SchoolFrontDoorL@66 --courir -675.8,-325.4,5.9@70 \
      --suivre 12,0.8@105 --viser 12@105 --jouer $LA@106,106.6,…,115 --suivre 13,0.8@117 --viser 13@117 --jouer $LA@118,118.6,…,127 \
      --courir -657.2,-293.5,5.9@155

(`J1`, `J2`, `J3` et les deux séries de l'embuscade : seize ou dix-neuf instants de 0,6 s en 0,6 s.) Le joueur court à la bibliothèque,
Algie le suit (`TetherLoco`, 5,51 s) ; le premier bully attaque Algie à 12,08 s ; dix-neuf jabs (six manquent « dans son dos ») : il fuit
à 47 points (19,04 s), meurt à 27,32 s ; sa fuite lance les bullies 5 et 6 sur le joueur (25,11 s), morts à 40,00 et 50,98 s ;
`F_UnlockStage2` et `MissionTimerStart(180)` à 58,01 s. La porte de l'école à 66 s : Algie passe avec le joueur (`FUN_0048b970`), posé
dans le hall à 67,29 s ; `F_Stage2`. L'outil n'a pas de sol : le joueur court aux toilettes du second étage à 5,9 m de haut, pour
qu'Algie, qui s'arrête à 2,40 m de sa place et monte au même pas que lui, soit dans la hauteur de `_1_05B_BATHROOMSECONDFLOOR` (de 5,23 à
7,23 m) : `F_CutAlgieToStall` à 86,69 s, Algie dans la cabine à 91,64 s, la minuterie arrêtée ; l'embuscade (les bullies 12 et 13)
attaque à 104,54 s, morts à 117,97 et 129,99 s ; la chasse d'eau à 147,01 s, Algie au lavabo, puis `F_Stage3` : Algie suit de nouveau le
joueur, l'objectif `1_05_MOBJ_03` à 154,18 s. Le joueur court au casier (155 s) et entre dans `_1_05B_ALGIELOCKER` à 161,24 s :
`F_NIS_AlgieLocker` ; Algie suit `_1_05B_ROUTEALGIELOCKER`, et au nœud 1 (163,15 s) `F_routeAlgieLocker` pose `SaveAlgieBooks` au casier
(l'objet animé de `NLock02B06`, créé quand le joueur est entré à l'école, 67,29 s ; il offre alors `/Global/NLockA/PedPropsActions/Interact/NPCGetItem`
aux piétons) et `/Global/1_05/Anims/Rummage/Locker` à Algie : `TargetProp` lui fait viser le casier (1,09 m), la piste `PropInteract`
(163,25 s) lui fait jouer l'action du casier, `GetBooks`, la fouille et `CloseDoor` (166,12 s) ; il prend ses livres (`NerdBooks`), s'éloigne
sur `_1_05B_ALGIEFLEE`, et le joueur joue `/Global/Player/Gifts/GetMoney` (168,50 s) : la prise met le joueur dans `Give_Seek` et Algie dans
`ReceiveWait`, qui passe dix secondes plus tard à `Give/Receive` ; son `PlayOnTarget` fait jouer `Give/Give` au joueur (178,60 s ; l'outil
prend le partenaire de prise, faute du composant de visée des piétons de l'IA) ; l'écran `M_PASS` (182,62 s), les messages de respect,
le fondu et **`MissionSucceed` : `1_05` se termine réussie à 185,20 s**.

## `1_06_01` (« Get Help - Mission 1 ») (06.11.2026, séance 2)

`1_06_01` suit `1_05` dans la banque (`chap1.cat`) ; elle s'active, comme `1_05`, quand `1_04` est réussie (`MissionCond` sur le
haché de « 1_04 », 0x6a9d3c6 ; `TimeCond` de 8 h à 23 h, celle de `1_05` s'arrête à 19 h) et ne démarre pas seule (le champ 51 est nul) ; ses six missions (`1_06_01` et les
cinq `#3e2f5b3x` de la banque `1_06`) partagent le script `chap1\1_06_01.lua` et diffèrent par le nœud `Run` de leur entrée dans
`chap1.cat` (`Active/Run`), dont la piste `Script` appelle `F_SetStage(1)` à `F_SetStage(6)` au départ de `main` (`FUN_006abf20` →
`FUN_006ab510("Run")`). Lancée seule (`build/outils/lancer_lua 150 --mission 1_06_01 --sans-cinematiques`) :

1. `main` attend `bStageLoaded`, que `F_SetStage` pose (1,02 s) ; la cinématique `1-06` (au dortoir, zone 14) est sautée ; Gary (4) et
   Pete (5), recrutés et critiques, sont posés dans la cour (4,52 s).
2. `F_SetupRoutingToHobo` : l'objectif `1_06_OBJ01` (un `table.insert` du résultat de `MissionObjectiveAdd`, réel depuis cette séance),
   le marqueur de `_1_06BEGIN`, `T_GarySpeech` (quatre lignes de `M_1_06_01`) ; `F_RoutingToHobo` attend le joueur dans `_FIGHTINGAREA`.
3. La suite, lue dans `1_06_01.lur` : la cinématique `1-06B`, le clochard (`gHobo`) créé sur `_1_06BEGIN`, `F_SetupAchieveReceiver`
   (`1_06_OBJ02` : aller chercher le transistor par l'escalier `_STAIRCORONA`, `_LOWCORONA`, `_HIGHGROUND`, l'échelle
   `/Global/Ladder/Ladder_Actions`, les portes du bus `_BUSDOORS` par `PAnimIsOpen` et `PAnimOpenDoor`), `F_GoMeetWithHobo`
   (`/Global/1_06/1_06_Give/Give/GiveHobo_1_06`), puis l'entraînement au combat avec le clochard (`F_SetupMissionStageHoboFight`,
   `F_CorrectButtonPressed`).

Avec `--entrer _FIGHTINGAREA@14`, le joueur entre dans la zone du clochard à 14,03 s : la cinématique `1-06B` est sautée, le clochard
(le piéton 4, modèle 87, `STAT_HOBO`) est créé endormi devant le joueur (15,41 s), l'objectif `1_06_OBJ02` (le transistor) s'affiche à
15,97 s et `F_AchieveReceiver` attend l'objet 476 (`ItemGetCurrentNum`).

**Le transistor** (06.11, séance 3) est un collectible : la Radio 476 (`radio06` dans colctbls.dat, le type 1) posée par la seconde
section inst de `zone_school6.ipb` en (123,45, -9,06, 12,66), sur les hauteurs que l'échelle de `_LOWCORONA` mène à `_HIGHGROUND`.
gamemain rend les transistors indisponibles (`CollectiblesSetTypeAvailable(1, false)`, 0,17 s) ; `F_RoutingToHobo` les rend
disponibles (14,06 s) et la population change l'entrée en ramassage à 47,5 m du joueur ; pris au contact (1 m au plan, 2 m en hauteur,
l'octet +0x25 de la fiche Items/Radio), il compte un objet 476. Puis `F_GoMeetWithHobo` : à 2 m du clochard, `ItemSetCurrentNum(476, 0)`
et `GiveHobo_1_06` (la prise met le joueur dans `Give_Seek` et le clochard dans `ReceiveWait`), le fondu ; `F_SetupMissionStageHoboFight`
recrée le clochard dans `_RINGAREA` (`Hobo_Blocker`), et `F_SetupNextMove` pose la leçon : `ButtonHistoryAddSequence(6, true)` (le bouton
6 tenu, 750 ms par pas), trois répétitions. Dans le mode de l'arbre (`ButtonHistoryEnableActionTreeInput(true)` dans `MissionSetup`), la
piste `ActionRegisterButtonHistory` d'un coup chargé du joueur rapporte (6, 3) : la séquence réussit, `F_PassedCallback`, et `main`
compte la répétition ; sans coup, elle échoue toutes les 0,76 s et `F_FailedCallback` la fait reposer. Avec l'outil :

    C=/Global/Player/Attacks/Strikes/LightAttacks/#3f638a76/Release/HeavyAttacks/#775a9ffc:Act/Player.act
    build/outils/lancer_lua 90 --mission 1_06_01 --sans-cinematiques --entrer _FIGHTINGAREA@14 \
      --courir 123.9,-2.5,6.06@17 --courir 123.45,-9.06,12.66@25 --suivre 4,0.8@28 --jouer $C@46,47.5,49

Le joueur court à l'escalier, puis au transistor en ligne droite (l'outil n'a ni sol ni échelle), le prend à 26,86 s (le rappel
`CB_CollectibleCollected` de main.lur), rejoint le clochard (36,63 s), le don, la leçon à 44,85 s ; trois coups chargés (46,00, 47,52 et
49,01 s) font les trois répétitions : `1_06_MOVEUNLK01`, `F_FirstMissionEndCutscene` (le joueur suit `_ENDCUTSCENEPATH`), et
**`MissionSucceed` : `1_06_01` se termine réussie à 61,81 s**.

## `1_07` (« Defend Bucky ») (06.11.2026, séance 3)

`1_07` suit `1_06_01` dans la banque (`MissionCond`, `MissionCond`, `TimeCond`) et ne se lance pas seule après elle : l'outil la lance
(`build/outils/lancer_lua 120 --mission 1_07 --sans-cinematiques`). Elle bouclait sur `PAnimRequest(555)` (la caisse à outils `ToolBox`
10003 du déclencheur 555), un bouchon ; réelle, `main` crée ses objets animés par `PAnimCreate` (un bouchon : 555, la grille 544, les
portes 557 et 558, que `PAnimSetActionNode` met sur `/Global/1_07/ParametricDoor/…/DoorOnGround`), la planche `SC_SK8Board`
(`CreatePersistentEntity`, un bouchon), Bucky (le piéton 4, 300 points, sa barre de santé) et deux bullies (146 et 99, l'un à la batte),
pose le joueur et `1_07_command` (4,52 s) ; `T_ObjectiveMonitor` ôte 25 points à Bucky toutes les cinq secondes tant que le joueur n'est pas
près de lui (`PedFindInAreaObject(4, 2)`), et `1_07` échoue à 64,55 s (`1_07_BUCKYKO`).

**La défense de Bucky** (06.11, séance 4) : `PAnimCreate` est réelle (la caisse, la grille et les portes ont leur objet), comme
`PedFindInAreaObject` (`T_ObjectiveMonitor` trouve Bucky et le met dans `/Global/1_07/Idle`). L'objectif `objReachBucky` réussit quand le
joueur passe `_1_07_GATE_T` puis `_1_07_GATETRIG` ou `_1_07_FIGHTTRIGGER` (`F_ReachedBucky`) ; `objDefeatEnemies` attend
`F_MissionSpawnerFinished`. Avec l'outil :

    LA=/Global/Player/Attacks/Strikes/LightAttacks
    P=/Global/1_07/ParametricDoor/POpenUp/PedPropActions/#3be30de2/LiftOpen/success:Act/Conv/1_07.act
    J(a) = seize instants de 0,6 s en 0,6 s depuis a
    build/outils/lancer_lua 160 --mission 1_07 --sans-cinematiques --courir 157.9,6.66,6.4@5 --courir 150.15,6.6,6.4@25 \
      --suivre 5,0.8@40 --viser 5@40 --jouer $LA@J(41) --suivre 6,0.8@51 --viser 6@51 --jouer $LA@J(52) \
      --suivre 7,0.8@62 --viser 7@62 --jouer $LA@J(63) --suivre 8,0.8@73 --viser 8@73 --jouer $LA@J(74) \
      --courir 124.1,12.3,6.1@87 --jouer $P@94 --courir 157.6,6.48,6.35@97

Le joueur court au portail puis à la grille ; Bucky est rejoint à 39,07 s (`1_07_Objective1`, la grille joue `CloseGate/PropClosed`,
deux bullies de plus, 85 et 102, l'un à la batte) ; les greasers 5 et 6 attaquent Bucky et le joueur (41,58 s). Seize jabs chacun : le
piéton 5 meurt à 53,00 s, le 6 à 63,99 s (`T_MonitorGreasers` : Bucky recruté, `1_07_Objective2` et le marqueur de la caisse à outils
à 64,15 s), le 7 à 74,98 s, le 8 à 86,00 s (la porte 558 passe à `Closed`, `T_DoorMonitor`). Au corona du garage, `LiftOpen/success`
appelle `cbDoorOpened` (94,02 s) et la porte s'ouvre aux piétons ; `F_MissionSpawnerFinished` n'a pas encore sa dernière étape à 160 s.

**La fin de `1_07`** (06.11, séance 5). Lu dans `1_07.lur` : l'étape de la mission est l'upvalue r53 de `T_MonitorGreasers` (1 après la
première vague, 2 après la seconde paire, 3 après la porte, 4 après la seconde vague) ; après la porte, `PedFollowPath(Bucky,
_1_07_TOGATEWAIT)` attend le drapeau r54 que pose `cbDoorOpened`, puis la seconde vague est créée (le 145 à la batte sur
`_1_07_SPAWNPOINT04`, le 102 sur `_1_07_SPAWNPOINT05`), l'étape passe à 3, Bucky suit `_1_07_TOTOOLBOX` avec `CB_GrabWhatever` et
`F_DoBuckyGrabbingNIS` le pose près de la caisse : au dernier nœud de `_1_07_TOTOOLBOXF`, `CB_GrabWhatever` lui fait jouer
`/Global/WProps/Peds/ScriptedPropInteract`, dont la condition `PropTargetInteractive` (2 m) lui fait viser la caisse, que
`/Global/ToolBox/Useable` offre aux piétons ; il s'en approche (`SeekEntityPosition` du genre 4, 1 m), la piste `PropInteract` lui fait
jouer `/Global/ToolBox/PedPropsActions/Interact/StartUsing` et il prend le circuit ; l'objectif `1_07_Objective3`. La seconde vague
n'attaque que quand le joueur entre dans `_1_07_FIGHTTRIGGER` (`F_MissionSpawnerFinished` à l'étape 3 appelle `F_SpawnerCallback` pour
chacun : `PedMakeMissionChar`, `PedRestrictToTrigger(_1_07_NOAMBIENTPEDS)`, `PedAttack` sur le joueur et sur Bucky) ; battue, l'étape
passe à 4 (`1_07_Objective4`) et la mission attend le joueur dans le pavé de 0,8 m autour du point 1 de `_1_07_PLYRDOOR` (157,646, 6,477,
6,354 ; la hauteur comprise). `objDefeatEnemies` réussit et `F_CompleteMission` joue `F_Peanut_Confontation` (le joueur force la grille,
`/Global/1_07/FakeTry/startOpen`, le clochard Bum arrive sur `_1_07_BUM`), `F_BuckyThanksThePlayer` (le clochard ouvre la grille,
`HoboOpenGate`, et part sur `_1_07_TOBUSPATH`), puis le don de la planche (`/Global/Give1_07/Give_Attempt` : la prise, `Player_Part2`
par le `PlayOnTarget` de Bucky), `SK8_Examine`, l'écran `M_PASS` et `MissionSucceed`. Avec l'outil (sous zsh, `${=…}`) :

    LA=/Global/Player/Attacks/Strikes/LightAttacks
    P=/Global/1_07/ParametricDoor/POpenUp/PedPropActions/#3be30de2/LiftOpen/success:Act/Conv/1_07.act
    J(a) = seize instants de 0,6 s en 0,6 s depuis a
    build/outils/lancer_lua 200 --mission 1_07 --sans-cinematiques --courir 157.9,6.66,6.4@5 --courir 150.15,6.6,6.4@25 \
      --suivre 5,0.8@40 --viser 5@40 --jouer $LA@J(41) --suivre 6,0.8@51 --viser 6@51 --jouer $LA@J(52) \
      --suivre 7,0.8@62 --viser 7@62 --jouer $LA@J(63) --suivre 8,0.8@73 --viser 8@73 --jouer $LA@J(74) \
      --courir 124.1,12.3,6.1@87 --jouer $P@94 --courir 157.6,6.48,6.35@97 \
      --courir 135,27,6.1@103 --suivre 9,0.8@108 --viser 9@108 --jouer $LA@J(109) \
      --suivre 10,0.8@120 --viser 10@120 --jouer $LA@J(121) --courir 157.646,6.477,6.354@132

Après la porte (94,02 s), Bucky arrive à 1,00 m de la caisse (98,14 s), s'en sert (100,42 s) et prend le circuit (101,54 s) ; le joueur
reprend la main à 102,96 s, entre dans `_1_07_FIGHTTRIGGER` et la seconde vague l'attaque (105,77 s) ; seize jabs sur chacun : le piéton
9 meurt à 120,98 s, le 10 à 132,99 s, en laissant une santé (`PickupCreateFromPed`, 127,78 s) ; Bucky, renvoyé (`PedHasAlly`), se cache
(`/Global/1_07/Cower`, 121,01 s). Le joueur va au point de `_1_07_PLYRDOOR` : `F_CompleteMission` à 133,95 s, le clochard (134,51 s),
la grille ouverte (138,04 s), la prise du don (145,17 s), Bucky dit `THANKS_JIMMY`, `M_PASS` (148,73 s) et **`MissionSucceed` : `1_07`
se termine réussie à 151,47 s**.

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
