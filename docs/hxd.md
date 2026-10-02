# Dictionnaires d'animations (`.HXD`)

Les `.agr` ne nomment pas leurs animations (docs/agr.md). Les noms sont
dans les `.HXD` du dossier `Anim` (MAINPED.HXD pour les piétons, BIKE,
SK8BOARD, SLINGSH… pour les véhicules et accessoires) et dans
`Anim/hxds.dat`, qui en emballe 130 (balise `ANIM`, taille, puis le HXD).

## Chargement (bully.exe)

`FUN_006c1ab0` charge un HXD par son nom et le lit par `FUN_006b2d90` ;
`FUN_0042e640` ouvre `Anim\hxds.dat` et y lit un HXD par ligne de
`Config\Dat\PropHXDs.dat`. La version (f32, 1,12 au plus) décide de
quelques champs : au-delà de 1,09 (0x941f74) et de 1,11 (0x941f70), des mots
de plus.

## Format

Dans l'ordre :

- version (f32), un mot ;
- les masques d'os : nombre, puis pour chacun un nom de 32 octets
  (`default`, `UpperBody_ALL`, `JUST_RightArm`…), n, un mot, n poids (f32) ;
- les os : nombre, puis des noms de 32 octets, rangés par leur HashString ;
- les animations, chacune : durée (f32), un flottant (0,3 le plus souvent),
  le nom **« GROUPE\NOM »** sur 64 octets, son HashString, des drapeaux, la
  **taille de ses données dans le .agr, plus 4**, un mot, n événements (16
  bits ; trois mots chacun, plus 40 octets si le troisième vaut 1), quatre
  mots de 16 bits, un mot (deux de plus en 1,12), un vecteur (3 × f32) ;
- les groupes : nombre, puis un nom de 32 octets, une taille (celle du `.agr`
  plus 4 par animation : `Grap` 182 104 = 181 868 + 4 × 59), et en 1,11 et
  plus 32 octets et un mot ;
- 12 octets par os, n blocs de 0x30 octets, un mot final.

Relevé : les 20 `.HXD` et les 130 entrées de `hxds.dat` se lisent
exactement jusqu'au dernier octet ; 3 913 + 402 animations, et pour toutes
le hachage stocké est bien `HashString(nom)` : ce que citent les pistes
`Animation` des arbres d'actions (docs/cat.md). MAINPED.HXD : 46 masques,
36 os, 3 365 animations, 425 groupes.

## Du nom à l'animation du `.agr`

Quand le streaming a chargé un `.agr` (identifiants 0x58ac et suivants :
`FUN_0052d910` donne à chaque HXD un bloc d'identifiants, un par groupe de
sa table, MAINPED en premier), `FUN_00531b60` retrouve le HXD et l'indice
du groupe (`FUN_0052da00`) et appelle `FUN_006bf3a0`, qui aiguille vers
`FUN_006bf0e0` (ou `FUN_006bee40` au-delà de 50 000 octets) :

- il parcourt **tous les enregistrements d'animation du HXD, dans l'ordre** ;
- il garde ceux dont le champ placé après la taille (+0x20 dans l'objet)
  **vaut l'indice du groupe** ;
- il lit le `.agr` d'un trait, une animation par enregistrement gardé
  (`FUN_006be480`, qui reconnaît l'en-tête 0x100), et range le résultat à
  la suite dans un bloc de la taille du groupe.

C'est ce champ, pas le préfixe du nom, qui désigne le groupe : les
enregistrements d'un même groupe ne sont pas consécutifs dans le HXD. Vérifié
sur les 423 groupes dont le `.agr` existe : en avançant de `taille − 4`
octets par enregistrement, chaque pas tombe sur un en-tête et le dernier sur
la fin du fichier ; 3 913 animations rattachées, toutes. L'animation n° k
d'un `.agr` (celle que lit `AgrLireGroupe`) est donc le k-ième
enregistrement de son groupe : `NPC_LOVE\KISS_HARD_B` est la n° 8 de
NPC_Love.agr, `RAT_PED\RAT_SCURRY` la n° 0 de RAT_PED.agr.

**La durée de l'enregistrement HXD est la durée de jeu.** Sur les 3 365
animations de MAINPED dont le `.agr` se lit, 2 868 ont la même durée que
l'en-tête de leur `.agr` à 2 % près ; les autres en diffèrent de 0,05 à 2,9
fois, et c'est celle du HXD qui compte. Lu dans l'exe :

- en mémoire, chaque animation occupe 0x40 octets (`FUN_006b2d90`) : durée
  à +0xc, fondu à +0x10, hachage +0x14, drapeaux +0x18, taille +0x1c, groupe
  +0x20, événements +0x24 / +0x28, indice +0x32 ; `FUN_006b1c00` y cherche
  une animation par hachage (ou par nom, haché à la volée) ;
- le lancement : `FUN_006c0be0` → `FUN_006bcad0` → `FUN_006b6570` ; un fondu
  négatif prend `+0x10` (0,3 s pour 2 855 des 3 365 animations ; puis 0,1,
  0,167, 0,067, 0, 0,667) ; une vitesse négative part de `+0xc` ;
- **l'évaluation d'un os (`FUN_006be250`) borne le temps à `+0xc` puis le
  divise par `+0xc`** : la position entre 0 et 1 qui en sort donne la clé. Les
  instants des clés du `.agr` sont stockés normalisés, la durée de l'en-tête
  du `.agr` ne sert pas à les lire ;
- la piste `Animation` se termine quand son temps dépasse `+0xc` (ou son
  champ 44, s'il est plus court ; docs/cat.md).

`HxdEtirer` (`src/anim/AgrHxd`) applique la règle au chargement d'un groupe :
chaque animation est étirée sur la durée de son enregistrement.
`NPC_GENERIC\SMK_WALL_LIGHT` passe de 3 à 10 s, et les instants de ses pistes
`PropAttachEx` (2,5, 3,33, 7,67 s) tombent alors sur les gestes (docs/mxd.md).

Pièges rencontrés en route : 172 hachages ont plusieurs enregistrements (copies
dans des groupes de mission, `C_PLAYER\GEN_GIVE` : 4,0 s dans C_Player, 1,57
dans 2_S05_CooksCrush, même `.agr`) ; chaque copie vaut pour son groupe. Le
champ 16 des pistes `Animation` n'est pas une fin d'animation.

Code : `src/anim/Hxd` (CHxdFile, `Indice` : hachage → groupe et indice) ; test : `tests/test_hxd` ; lecture :
`tools/hxd.py <fichier.HXD>`.
