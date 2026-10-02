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

Le premier flottant d'un enregistrement n'est pas toujours la durée du
`.agr` : sur les 3 365 animations de MAINPED dont le `.agr` se lit, 2 868 ont
la même durée à 2 % près, les autres de 0,05 à 2,9 fois. Ce n'est **pas** la
durée de jeu en général :

- 172 hachages ont plusieurs enregistrements (copies dans des groupes de
  mission), aux durées parfois différentes pour le même `.agr` :
  `C_PLAYER\GEN_GIVE` 4,0 s dans C_Player, 1,57 dans 2_S05_CooksCrush ;
- le champ 16 des pistes `Animation` (une coupure), quand il vaut une des
  deux durées, vaut celle du `.agr` (`C_PLAYER\IDLE` coupée à 2,00 : `.agr`
  2,00, HXD 1,80 ; `WEAPON\PICKUP_SLINGSHOT` à 2,98 : 3,00 contre 4,33),
  jamais celle du HXD ; `PLAYER_TALKING1` est coupée à 4,0 s, ce qui n'a de
  sens que sur ses 5,43 s de `.agr` (3,33 dans le HXD).

Exception : `NPC_GENERIC\SMK_WALL_LIGHT` et `SMK_STND_LIGHT` durent 3,0 s
dans POI_Smoking.agr et 10 s ici, et les instants de leurs pistes
`PropAttachEx` (2,5, 3,33, 7,67 s) ne tombent sur les gestes qu'en temps HXD
(docs/mxd.md) : la visite les étire, et elles seules.

Code : `src/anim/Hxd` (CHxdFile, `Indice` : hachage → groupe et indice) ; test : `tests/test_hxd` ; lecture :
`tools/hxd.py <fichier.HXD>`.
