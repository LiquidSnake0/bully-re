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
disent quand la mission devient lançable.

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
- **La première mission** : dans `chap1`, `1_01` est la seule mission sans aucune
  condition (`tools/missions.py | awk -F'\t' '$1=="chap1" && $4=="-"'`). Ce qui la
  démarre au début d'une nouvelle partie n'est pas encore lu dans le moteur.
- `SMissPass.F_LaunchingMission` prépare l'état du monde selon la mission lancée
  (`MissionGetIndex`, `1_01`, `1_02A`…) : à lire quand on branchera le lancement.

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
handle de `_1_01_CHEERPOINTS` et non plus un bouchon.

Les piétons des scripts sont un pool de 24 (`src/peds/ScriptPedPool`, le joueur en 0
comme `PlayerGetPedIndex` le dit, `tests/test_scriptpeds`) : `PedCreatePoint`,
`PedCreateXYZ`, `PedDelete`, `PedIsValid`, `PedIsPlayer`, `PedGetPosXYZ`, `PedSetPosXYZ`,
`PedGetHeading`, `PedFaceXYZ`, `PedIsDead`, `PedGetHealth`, `PedSetHealth`,
`PlayerGetHealth` répondent dessus ; un piéton qu'une mission laisse derrière elle est
libéré à la fin de la mission (le moteur le note à l'objet script, `FUN_005d94d0`).

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

    build/outils/lancer_lua 40 --mission 1_01 --entrer _1_01_BULLYTRIG@4 --aller _1_01_ENDPOINT@8 --zone 2@8.1

1. Objectif `1_01_StandGround` ; le joueur entre dans `_1_01_BULLYTRIG` → le premier
   élève redevient ambiant, un bully (modèle 69) est créé sur `_1_01_FIGHTBULLIES` et
   `PedAttack(5, 4, 3)` lance la bagarre (que l'outil ne joue pas).
2. Le joueur au point 1 de `_1_01_ENDPOINT` **et** `AreaGetVisible() == 2` (le hall de
   l'école ; l'AREACODE du point dit 5, ce n'est donc pas la zone visible) → la
   secrétaire (59) est créée sur `_1_01_SECRETARYINOFFICE`, la population revient.
3. Puis deux élèves (69 et 102) sur `_1_01_KIDRUN`, `PedMoveToPoint`, `PedAttack`, et
   le joueur attendu dans `_1_01_CHECKOBJS1`… la suite se lit dans le listage.

## Suite

1. Continuer `1_01` avec `--entrer` / `--aller` / `--zone` jusqu'à `MissionSucceed`, en
   rendant réelles les fonctions qui bloquent (`PedMoveToPoint`, `PedAttack` et la mort
   d'un piéton, les cinématiques `PlayCutsceneWithLoad`).
2. Ce que lit `FUN_005d2310` à +0xc de l'entrée de liste (l'AREACODE ?), et la vraie
   zone d'un point.
3. Lire dans le moteur qui démarre `1_01` à la nouvelle partie.
