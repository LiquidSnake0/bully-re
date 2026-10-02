# Arbres d'actions compressés (`Act/Act.img`, `.cat`)

Les 479 fichiers `.cat` d'`Act/Act.img` décrivent le comportement des
personnages : une hiérarchie de nœuds (banques, nœuds jouables, feuilles,
références), des conditions qui gardent l'entrée dans chaque nœud, et des
pistes que joue un nœud (animations, sons, intentions…). Les 119 `.cat` de
`Stream/World.img` sont une autre famille (catalogues liés aux objets
animés, voir formats-restants.md).

## Chargement (bully.exe)

- `FUN_005f6f20` ouvre `Act\Act.IMG`, crée la banque racine « Global », puis
  charge la liste `Act\Globals.act`… (0xb0a4f0) par `FUN_005f6c50`, qui
  ajoute `.cat` au nom.
- `FUN_005fb3f0`, que le jeu nomme « CompressedActionTree::load() », lit
  les deux premiers mots (taille, fin de l'en-tête), charge l'en-tête dans
  un tampon de travail et le reste dans un bloc alloué, puis l'analyse :
- `FUN_005fb220` alloue les nœuds d'après les comptes de l'en-tête (banques
  0x20 octets, références 0x30, nœuds jouables 0x28, feuilles 0x18) ;
- `FUN_005fb1a0` lit une banque, `FUN_005fa9c0` un nœud jouable,
  `FUN_005fafb0` la liste des enfants, `FUN_005fa910` les conditions,
  `FUN_005fa8b0` les pistes.

Les noms passent par `HashString` (0x576d80) : la chaîne mise en
majuscules, `h = h * 0x83 + c`, gardé sur 31 bits.

## Format

En-tête, huit mots de 32 bits :

| | |
|---|---|
| +0 | taille du fichier |
| +4 | fin de l'en-tête = début des données |
| +8 | début des chaînes |
| +0xc | fin de l'arbre |
| +0x10 | nombre de banques (hors racine) |
| +0x14 | nombre de nœuds jouables, feuilles comprises |
| +0x18 | nombre de références |
| +0x1c | nombre de feuilles |

Puis deux tables de renvois, même forme : un compte (mot), et pour chaque
entrée une valeur (mot ; dans la première, un décalage dans les chaînes),
un compte k (16 bits) et k décalages des données qui la citent. Le moteur
s'en sert pour rebrancher les chaînes dans les enregistrements.

Puis l'arbre, octet par octet, à partir du caractère `b` de la racine :

- un nœud : son nom (mot ; bit de poids fort à 1 = hachage du nom, sinon
  décalage dans les chaînes), un octet n et n conditions (décalages dans
  les données), pour un nœud jouable un octet m et m pistes, puis ses
  enfants ;
- les enfants : un compte (16 bits), puis pour chacun un caractère, `b`
  banque, `n` nœud jouable, `l` feuille, `i` ou `r` référence (deux
  décalages dans les chaînes : le nom, le chemin du nœud visé), suivi du
  nœud.

Une condition, dans les données, commence par le hachage du nom de sa
classe, que le moteur cherche dans un registre de fabriques
(`FUN_0061a4f0`) : `Not`, `Or`, `ConditionGroup`, `HavePOIOfType`,
`IsSocializing`, `pedType`, `RangeToTarget`, `WeaponModelRequest`…

## Les pistes

Le nœud ne garde que des pointeurs ; la piste est créée à la demande
(`FUN_005f6100`, emplacement 17 de la table virtuelle) et remplie par
`FUN_005fa7e0` :

- un mot de base de 16 bits ; s'il n'est pas nul, la piste hérite de celle
  placée **base octets plus loin** dans le fichier (`ADD ECX, ESI` dans
  l'assembleur : le modèle est copié d'abord, puis écrasé) ;
- des attributs, chacun un mot w de 16 bits suivi de sa valeur : taille
  `1 << ((w >> 1) & 3)` octets (1, 2, 4 ou 8), position `w >> 3` dans
  l'objet, un autre attribut suit tant que `w & 1`.

La valeur en position 0 est le type : le hachage du nom de classe de la
piste, cherché dans le registre de fabriques (`FUN_0061a4b0`). Relevé :
**51 001 pistes, toutes typées** (par héritage pour 36 464), 398 types, tous
nommés : `Animation` (8 166), `Opportunity` (6 647), `Execute`, `SoundFX`,
`Sequence`, `SetPedFlags`, `Spawn`, `DialogLine`, `Target`, `JointDriver`,
`LocomotionAnimationNew`… Les chaînes (chemins de nœuds : `./SitDown`,
`../../../../AIActionOpps/SpectatorOpps`) se retrouvent par la table de
renvois : l'attribut placé au décalage cité.

Une piste `Animation` porte en position 24 **`HashString("GROUPE\NOM")`**
de l'animation (`C_PLAYER\PUNCH_SLOP_1` = 0x6158a6b0 : le groupe est le
fichier `.agr`), en 28 un masque d'os (`UpperBody_All`, `UBO_Arms_Hd_Sp`…),
puis des réglages (fondus, vitesse, boucle). Les noms « GROUPE\NOM » sont
écrits en clair dans `Anim/MAINPED.HXD` et les autres `.HXD` (2 889 dans
MAINPED, chacun suivi de son hachage). Le format des `.HXD` et le passage du nom à
l'animation du `.agr` sont dans docs/hxd.md ; `tools/cat.py --pistes`
affiche « [RAT_PED.agr n° 0] » à côté de chaque animation.

Les autres champs d'une piste `Animation`, lus dans `AnimationTrack`
(table virtuelle : lancement `FUN_00612360`, mise à jour `FUN_0060ae00`,
arrêt `FUN_0060af80`) ; la position d'un attribut est son décalage dans
l'objet :

| Position | Sens |
|---|---|
| 12 | (toutes les pistes) instant d'entrée en jeu dans le nœud (s) |
| 16 | (toutes les pistes) durée de vie dans le nœud (s) ; −1 : sans limite. Une boucle à 16 = 2,27 finit le nœud à 2,27 s |
| 24 | HashString de l'animation |
| 28 | masque d'os (`FUN_00607340`) |
| 32 | mode : 2 = boucle ; sinon la piste finit avec l'animation (1 : appelle aussi `FUN_006c0c60` à la fin) |
| 36 | couche du lecteur (0 à 6) |
| 40 | instant de départ (s) |
| 44 | instant de fin (s) ; −1 ou au-delà : la durée du HXD |
| 48 | vitesse ; multipliée par la stat 0x14 du piéton / 100 |
| 52 | fondu (s) ; 0 : sans fondu ; −1 : celui du HXD |
| 56 | passé à `FUN_006bd320` à l'arrêt |
| 60 | octet : pose un drapeau du piéton (+0xe4) le temps de la piste |
| 61 | octet : la vitesse suit le déplacement réel (synchro de la marche) |

## Un exécuteur simplifié

`outils/arbres.h` déroule un arbre pour un piéton de la visite, au lieu des
programmes écrits à la main. C'est une approximation, avec ces règles :

- chemins `.`, `..` et noms hachés ; une cible hors du sous-arbre est ignorée ;
- piste Animation : champs 16 (durée de vie dans le nœud, même en boucle) à 52 ;
  mode 1 : la dernière pose reste figée ;
- fin d'une animation : `sequence` ; sinon un nœud jouable qui offre des
  occasions se rejoue (état d'attente), un autre revient au nœud jouable ancêtre
  le plus proche, à défaut au départ (un banc choisit un enfant une fois, il ne
  rejoue pas ses frères) ; un nœud qui lâche l'objet ressort au-dessus de celui
  qui l'avait pris ;
- `Opportunity` : ouverte de max(12, 44) à 16 (44 : l'instant au plus tôt ; la
  reprise « ./ » de `Sit_Smoke_Idle` a 44 = 3,33, la fin de son animation),
  prise au hasard (8 % par seconde) si les conditions du nœud visé passent ;
  champ 8 (un octet) à 1 : prise dès qu'elle s'ouvre ; elle peut viser le nœud
  lui-même (reprise) ; `OpportunityRandomLatch` : à coup
  sûr, à un instant tiré entre ses champs 76 et 80 ;
- `Execute` dans un nœud sans animation : un saut ;
- conditions : `WeightedRandom` et `Random` tirent au sort ; `IsFemale` suit le
  piéton ; `ActionRequest`,
  `IsScriptedAmbient`, `false`, `IsPlayer`, `IsAuthority`, `PedModelID`,
  `Health`, `DamagePending`, `HitTime`, `PropTargetInteractive`,
  `TargetRelativeOrientation`, `OBJECTIVE` sont fausses ; `Not` inverse la suivante ;
  `OR` : une au moins ; les autres passent ;
- `PlayOnTarget` envoie le partenaire ; le nœud visé est passif : on n'y entre
  que sur ordre, on n'en sort que par sa `sequence`. Le rôle passe avec l'ordre :
  qui l'envoie mène, qui le reçoit suit ; un suiveur fige sa pose à la fin de
  son animation et, envoyé vers un nœud sans animation, en applique les effets
  sans quitter le sien. Dans un nœud animé, un `Execute` fait jouer au
  partenaire le `PlayOnTarget` de la feuille visée (`./TargetOrientation`) ;
- `TargetSync` (placement relatif des deux piétons) n'est pas géré : les paires
  partagent leur origine, les décalages de bassin des animations font le reste
  (voir ci-dessous).

### TargetSync

Champs relevés : 12 et 16 (fenêtre), 28 distance (m), 36 angle (rad) ; prise
GRAP_INIT et couple `Hold` : 28 = 0,9, 36 = π ; vol d'arme `Steal_Easy` :
28 = 0,7, 36 = π. Les méthodes virtuelles de `SyncBaseTrack` sont vides (le
travail est fait ailleurs dans le moteur, non suivi). Ce que disent les
animations, à t = 0 :

| Animation | Flèche (ARROW) | Bassin |
|---|---|---|
| Grap n° 23 `GRAP_INIT_GV` | (−0,03 ; 0,22) | 0,22 devant |
| Grap n° 22 `GRAP_INIT_RCV` | (0,05 ; 1,18) | 1,17 devant |
| C_Player n° 366 `GRB_STEAL_ATT` | aucune | à l'origine |
| C_Player n° 365 `GRB_STEAL_VIC` | (0 ; 0) | à l'origine |

Les paires Grap portent leur écart dans la flèche (0,96 m, presque les 0,9 de
TargetSync) : poser les deux piétons à la même origine revient au même que
poser chacun sur sa flèche et la cible à 0,9 m, face au meneur. Le vol d'arme
n'a pas d'écart : seul TargetSync sépare les deux piétons. Modèle probable du
moteur : chaque piéton suit sa flèche, TargetSync place la cible à la
distance 28, tournée de 36, par rapport au meneur.


Pilotés par un arbre :

| Point | Arbre | Sous-arbre, départ |
|---|---|---|
| fumeur au mur | Ambient.cat | `Wall_Smoke`, `Wall_Start` |
| fumeur debout | 5_02.cat | `StandingSmoke`, `light` |
| couple | NPC_Ambient.cat | banc de `Hold` ; le garçon part de `Hold`, la fille suit |
| bagarre de filles | Grapples.cat | banc de `GirlFight_Init` : Init → Loop (5 à 10 s) → Out |
| bagarre au sol | Grapples.cat | `mount`, `MountIdle/Give` : `MountOpps` choisit le coup selon la clique (`FacePunch`, `KneeDrop`, `Headbutt`, `Dismount`…) |
| place assise | Ambient.cat | `Sitting_Down/SitHigh` : s'asseoir (`Sit_Start`), attendre, fumer ou discuter assis, se relever ; variante des filles (`IsFemale`) |
| prise debout | Ambient.cat | `LockerStuff/StuffGrap`, `GrappleSuccess/Pull_In_heavy/Give` (empoignade `GRAP_INIT`), puis `Hold_Idle` (`GRAP_IDLE`) |

Tout piéton dont l'arbre accroche un objet reçoit la cigarette (qui n'apparaît
qu'aux instants de l'arbre). Restent sur des tables : les spectateurs (leur
vrai déroulement passe par AI_POI, `SpectatorOpps`), les groupes qui discutent
et le harcèlement (animations `Hang_Talking` / `AggroTaunt` jouées par des
arbres de mission et le système de dialogue, pas par un arbre unique).

Le vol d'arme (`GrappleOpps/Scripted/WeaponSteal`) est écarté : il suppose une
arme sur la cible, et ses animations (C_Player) se placent par `TargetSync`. `BULLY_TRACE=1` affiche les
derniers nœuds traversés par chaque piéton.

La visite (`outils/visite.cpp`) applique 40, 44 et 48 aux étapes de ses
programmes : au premier point d'intérêt, elle lit les pistes `Animation` des
479 arbres d'`Act.img` et garde, pour chaque animation, le réglage le plus
fréquent. Relevé sur 2 660 animations : `NPC_LOVE` boucle des baisers à 1,6,
fins à 2 ; `SMK_STND_SMKB` à 0,6 ; les cigarettes au mur à 1. La stat 0x14 du
piéton qui multiplie la vitesse n'est pas lue (100 supposé).

Les objets tenus : une piste `PropAttachEx` porte en 12 l'instant (en
secondes de la durée **du HXD**, pas du `.agr`), en 24 le modèle
(`cigarette`), en 28 le **HashString du point d'attache** de
`Models/Peds/MXDs.MGR` (`LeftCig` = 0x47c19c6a, `MouthCig`,
`RightCig` = 0x2654fed) et en 32 l'emplacement (`LeftHand`, `Mouth`,
`RightHand`) ; `PropDetachEx` porte en 24 le point à libérer. Pour
`Wall_Smoke` (Ambient.cat) : main gauche à 2,5 s, bouche à 3,33, main droite
à 7,67 pendant `SMK_WALL_LIGHT`, lâchée à 1,6 s de `SMK_WALL_STUB` ; pour
`StandingSmoke` (5_02.cat) : 2,67, 3,2 et 7,67 avec des `PropAttach`. Les
points et l'échelle de temps : docs/mxd.md.

Relevé : les 479 fichiers se lisent en entier, l'arbre finit exactement à
l'en-tête +0xc et les quatre comptes concordent partout (7 552 banques,
4 546 nœuds jouables, 259 références, 13 435 feuilles). En hachant les
chaînes de bully.exe, des scripts, des noms d'archives et des tables de
chaînes, `tools/cat.py` résout 51 % des noms de nœuds et tous les types de
conditions (20 846).

## Exemple : AI_POI

La racine `POIPoint` (si le piéton n'a ni objectif, ni vélo, ni arme…)
contient `Scenario` (`ScenarioSeek`, `ScenarioDialog`, `getGift`…), `Hangout`,
`sitting` (si `HavePOIOfType`, `Not IsSocializing` ; feuilles `SitDown`,
`ClearPOI`) et `spectator` : c'est là que le jeu choisit ce que fait un
piéton arrivé à un point d'intérêt (docs/trigger.md). Avec `--pistes` :
le nœud jouable sous `sitting` enchaîne la séquence `./SitDown`, `SitDown`
joue `AIActionOpps/SitOpps` avec des opportunités de réaction
(`Reactions/HitReact`, `Social_System`) ; `spectator` joue
`AIActionOpps/SpectatorOpps` et peut basculer vers `./StopSpectacleEarly` ou
`FleeObjective`.

Code : `src/core/ActionTree` (CActionTreeFile, ActionHash) ; test :
`tests/test_actiontree` (les 479 fichiers, les 51 001 pistes) ; lecture :
`tools/cat.py <nom.cat> [--pistes]` (arbre aux noms résolus) et
`tools/cat.py --stats`. Scripts Ghidra :
`bully-test/scripts/ExportArbre.java` (une fonction et ses appelées),
`ExportAsm.java` (les instructions d'une fonction).
