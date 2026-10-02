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
