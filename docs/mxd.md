# Points d'attache : MXDs.MGR

`Models/Peds/MXDs.MGR` (et ses frères `Objects`, `Vehicles`, `WProps`,
`Accssory`) dit, modèle par modèle, où s'accrochent les objets : la
cigarette à la bouche, le livre dans la main, l'adversaire d'une prise.

## Chargement

`CGame_InitRW` (`FUN_0042e820`), étape « LoadMGRFiles », appelle
`FUN_006c2cf0` sur les cinq fichiers. Chacun : un u32 (nombre d'entrées),
puis par entrée un nom de 64 octets (`MODELS\PEDS\PLAYER`), lu par
`FUN_006c2cf0`, et le reste par `FUN_006c2980` → `FUN_006b5710` :

| taille | contenu |
|---|---|
| 4 | version (f32) : 1,06 (constante 0x941fc0) partout |
| 4 + 4 | deux mots |
| 64 | dictionnaire d'animations (`ANIM\MAINPED`, `ANIM\RAT_PED`) |
| 4 | un mot |
| 4 | n, le nombre de points |
| 96 × n | les points (`FUN_006b5620`) |
| 4 + 12 × m | au-delà d'une version seuil : m, puis un vecteur par os |
| 4 | au-delà d'un second seuil : un dernier mot |

Un point (96 octets) : position (3 f32) et rotation (quaternion w, x, y, z,
la convention d'`AgrMatrice`) dans le repère de l'os ; le nom de l'os
(32 octets, celui du squelette du HXD : `Jaw`, `Left Hand`) ; son numéro ;
le nom du point (32 octets). Les m vecteurs de fin sont les translations de
repos des os du modèle, les mêmes que celles du NIF.

Le numéro d'os est l'indice dans la liste des os de `MAINPED.HXD`, qui est
aussi l'ordre des pistes `.agr` et des nœuds animés du modèle (en profondeur
sous `Dummy`, `outils/scene.h`) : 15 = `Jaw` (le nœud s'appelle
`Root Ponytail1` dans les NIF), 22 = `Left Hand`, 30 = `Right Hand`.

## Ce qu'on y trouve

Peds : 358 modèles (357 sur MAINPED, le rat sur RAT_PED), 9 à 11 points, à
part un modèle de 29. Toujours `HeadDir`, `LeftCig`, `LeftFoot`, `LeftHand`,
`Mouth`, `MouthCig`, `RightCig`, `RightFoot`, `RightHand`, presque toujours
`GrappleAnchor` (Spine2), parfois `Hat`, `Helmet`, `Glasses`. La cigarette
de gauche est tenue par la main (195 modèles) ou entre deux doigts
(`Left Finger2`, 161). `WProps` : 237 modèles, même format. `Objects`,
`Vehicles` et `Accssory` ont des entrées de forme différente, non lues.

## Les objets tenus

Les arbres d'actions désignent le point par son HashString (`PropAttachEx`,
docs/cat.md). L'objet est un modèle rigide de World.img (`Cigarette.nif` :
9,5 cm le long de son axe z, centré) placé à
`place du piéton ∘ monde(os) ∘ (rotation, position du point)`.

Pour les deux allumages (`SMK_WALL_LIGHT`, `SMK_STND_LIGHT`), les instants
de ces pistes sont en temps HXD : l'animation dure 3 s dans son `.agr` et
10 s dans MAINPED.HXD, et c'est sur 10 s que la main gauche rejoint la droite
(2,5), que la cigarette arrive à la bouche (3,33, la main gauche à 12 cm de
la bouche) et que la main droite la reprend (7,67, à 5 cm). La visite étire
donc ces deux étapes (vitesse 0,3). Ce n'est pas une règle générale
(docs/hxd.md) : ailleurs, l'animation garde la durée de son `.agr`.

Code : `src/anim/Mxd` (CMxdFile, `Point(hachage)`) ; test :
`tests/test_mxd` (les 358 + 237 entrées lues à l'octet près, les 3 590
points sur leur os du HXD) ; `outils/attache.h` (`RepereDuPoint`,
`Accrocher`) ; `visite --poi n` : les fumeurs prennent, allument, fument et
écrasent leur cigarette.
