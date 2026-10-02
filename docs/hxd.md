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

## Du nom à l'animation du `.agr` (ouvert)

La taille `e` d'une animation vaut celle de ses données dans le `.agr`, plus
4 : `NPC_LOVE\KISS_HARD_B` (4,433 s, 5 048) est la n° 8 de NPC_Love.agr
(4,433 s, 5 044 octets), et les 14 noms NPC_LOVE de MAINPED tombent, dans
l'ordre, sur les n° 8 à 21. En prenant les animations d'un groupe dans
l'ordre de MAINPED et en avançant de `e − 4`, 47 groupes sur 82 se
retrouvent en entier ; pour les autres (C_PLAYER, F_JOCKS…) la suite casse,
et un appariement par taille et durée sur tous les HXD ne fait pas mieux.
Le jeu rattache l'animation à son enregistrement quand le groupe est chargé
(l'enregistrement reçoit en +0 un pointeur vers les données, que
`FUN_006b2ca0` décode) : c'est ce code-là qu'il faut lire pour conclure.

Code : `src/anim/Hxd` (CHxdFile) ; test : `tests/test_hxd` ; lecture :
`tools/hxd.py <fichier.HXD>`.
