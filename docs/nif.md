# Les modèles NIF (Gamebryo 2.3)

`Stream/World.img` contient 5 724 modèles `.nif`, tous « Gamebryo File Format,
Version 20.3.0.9 » (0x14030009), version utilisateur 0. 5 477 sont
petit-boutistes, 247 grand-boutistes (les cinématiques `CS_*`, exportées pour
console) : le boutisme s'applique à partir du nombre de types, le nombre de
blocs restant petit-boutiste. bully.exe vérifie la ligne d'en-tête en
0x75ab30 ; le chargeur est le NiStream de Gamebryo, compilé sans RTTI.

Lecteur : `src/gamebryo/NifFile.h/.cpp`, test `tests/test_nif.cpp` (FTEST11
contre des valeurs relevées à la main, puis les 5 724 fichiers : 587 264
blocs, 286 403 de types connus, tous décodés à l'octet près).

## En-tête

Ligne texte + `\n`, version u32, boutisme u8 (1 = petit), version
utilisateur u32, nombre de blocs u32, nombre de types u16, types (u32
longueur + texte), type de chaque bloc u16, taille de chaque bloc u32,
nombre de chaînes u32, longueur max u32, chaînes (u32 + texte), nombre de
groupes u32 (+ u32 par groupe), puis les blocs bout à bout. Les tailles
permettent de sauter les blocs qu'on ne décode pas.

## Types de blocs rencontrés (occurrences sur 5 476 fichiers petit-boutistes)

NiNode 85 160 · NiStringExtraData 72 603 · NiTransformController 63 915 ·
NiTransformInterpolator 63 915 · NiTransformData 53 213 · NiTriShape 43 450 ·
NiTriShapeData 43 450 · NiSourceTexture 41 705 · NiMaterialProperty 29 270 ·
NiTexturingProperty 16 346 · NiVertexColorProperty 10 937 · NiTriStrips 8 614 ·
NiTriStripsData 8 614 · NiZBufferProperty 5 476 · NiSpecularProperty 3 911 ·
NiBooleanExtraData 3 025 · NiSkinInstance 2 395 · NiSkinData 2 395 ·
NiSkinPartition 2 325 · NiAlphaProperty 2 262 · NiIntegerExtraData 1 180 ·
NiStencilProperty 1 122 · NiTextureEffect 986 · NiIntegersExtraData 512 ·
NiSourceCubeMap 452 · NiStringsExtraData 256 · NiFloatsExtraData 48 ·
NiFloatExtraData 18 · NiDitherProperty 1.

## Dispositions décodées (20.3.0.9, relevées sur les données)

- **NiAVObject** : nom (index de chaîne i32), extra data (u32 + refs),
  contrôleur i32, drapeaux u16, translation 3f, rotation 9f, échelle f,
  propriétés (u32 + refs), objet de collision i32.
- **NiNode** : + enfants (u32 + refs), effets (u32 + refs).
- **NiTriShape / NiTriStrips** : + données i32, peau i32, matériaux (u32,
  puis nom + extra par matériau), matériau actif i32, dirty u8.
- **NiGeometryData** : groupe i32, sommets u16, keep u8, compress u8, a des
  sommets u8 + 12 o chacun, drapeaux u16 (bits 0-5 : jeux d'UV, bit 12 :
  tangentes et binormales présentes après les normales), a des normales u8,
  centre 3f, rayon f, a des couleurs u8 + 16 o chacune, UV (8 o × jeux ×
  sommets), cohérence u16, données additionnelles i32.
- **NiTriShapeData** : + triangles u16, points u32, a des triangles u8 +
  6 o chacun, groupes d'appariement u16 (+ u16 compte + u16 × n).
- **NiTriStripsData** : + triangles u16, bandes u16, longueurs u16 × bandes,
  a des points u8 + u16 × total ; converties en liste de triangles.
- **NiSourceTexture** : nom, extra, contrôleur, externe u8, nom de fichier
  (index de chaîne, `.tga` dans les données, servi par les `.nft`), données
  de pixels i32, disposition u32, mipmaps u32, alpha u32, statique u8,
  rendu direct u8, persistant u8.
- **NiMaterialProperty** : nom, extra, contrôleur, ambiant, diffus,
  spéculaire, émissif (3f chacun), brillance f, alpha f.
- **NiTexturingProperty** : nom, extra, contrôleur, drapeaux u16, nombre
  d'emplacements u32 (9 : base, sombre, détail, brillance, lueur, relief,
  normale, parallaxe, décalque), par emplacement un u8 « présent » puis
  { source i32, drapeaux u16, transformation u8 (+ translation 2f, échelle
  2f, rotation f, méthode u32, centre 2f) }, le relief avec 6 flottants de
  plus, la parallaxe un flottant ; puis textures de shader u32.

- **NiAlphaProperty** : nom, extra, contrôleur, drapeaux u16, seuil u8.
- **NiStencilProperty** : nom, extra, contrôleur, drapeaux u16, référence
  u32, masque u32. Les faces dessinées sont les bits 10-11 des drapeaux.
  Relevé sur les 5 721 `.nif` de World.img : 1 129 blocs, tous décodés à
  l'octet près, tous avec les drapeaux 0x4d80, c'est-à-dire « les deux
  faces ». Le jeu ne s'en sert que pour ça : une forme qui en porte un est à
  double face, les autres ont leurs faces arrière éliminées.

Non décodés (sautés grâce aux tailles) : contrôleurs et interpolateurs de
transformation, extra data, peau (NiSkinInstance/Data/Partition), propriétés
spéculaire, Z-buffer, couleurs de sommets, effets.

## Le squelette (30.09.2026)

Trois blocs portent la peau d'un modèle animé ; relevé sur les 5 721 `.nif`
de World.img : 2 614 NiSkinInstance, 2 614 NiSkinData et 2 541
NiSkinPartition, **tous décodés à l'octet près**.

- **NiSkinInstance** : données i32, découpage i32, racine du squelette i32,
  nombre d'os u32 puis les os (références de NiNode).
- **NiSkinData** : transformation d'ensemble (matrice 9f par lignes,
  translation 3f, échelle f), nombre d'os u32, a des poids u8, puis par os :
  transformation de liaison (mêmes 13 flottants), sphère englobante (centre
  3f, rayon f), nombre de sommets u16 et, si les poids sont là, (index u16,
  poids f) par sommet. Les poids sont présents dans les 2 614 blocs.
- **NiSkinPartition** : nombre de lots u32, puis par lot : sommets,
  triangles, os, bandes, poids par sommet (u16 chacun) ; os u16 × n ; a une
  table de sommets u8 (+ u16 × sommets) ; a des poids u8 (+ f × sommets ×
  poids) ; longueurs des bandes u16 × bandes ; a des faces u8 (+ triangles
  u16 × 3, ou les bandes) ; a des index d'os u8 (+ u8 × sommets × poids).
  Au plus 57 os par peau et 4 poids par sommet.

**La formule.** Un sommet v devient Σ poids · (M_os ∘ S_os)(v), avec M_os
la transformation de l'os dans le modèle (celle de son nœud) et S_os sa
transformation de liaison, **sans transposition**. Établie comme le sens du
quaternion, en essayant les combinaisons (avec ou sans la transformation
d'ensemble, avant ou après, transposée ou non) : avec le squelette dans sa
pose du fichier, seule celle-ci redonne les sommets stockés, à 2,3 mm près
sur Jimmy ; les versions transposées s'écartent de 6 cm à 3,5 m. La
transformation d'ensemble est l'identité sur les piétons essayés. Sur Zoe
l'écart atteint 5,7 cm : son squelette est rangé dans une pose un peu
différente de la liaison, et c'est lui qui fait foi (un petit piéton qui
paraissait enfoncé dans le sol se retrouve debout).

**Les os** suivent le nommage Biped de 3ds Max, préfixé par « Root » :
`Root Pelvis`, `Root L Thigh`, `Root L Calf`, `Root L Foot`, `Root Spine`,
`Root Spine1`, `Root Spine2`, `Root L Clavicle`, `Root L UpperArm`,
`Root L Forearm`, `Root L Hand`, les doigts, `Root Neck`, `Root Head`,
`Root Brow`, `Root EyeLids`, `Root Eyes`, `Root Ponytail1`… Le haut du bras
se baisse autour de son axe y local (+ à gauche, − à droite).

Code : `src/gamebryo/NifSkin` ; test : `tests/test_skin` (sur PLAYER.nif).

## Sens des rotations

Un `NiAVObject` porte une translation, une matrice 3 × 3 et une échelle. La
matrice est stockée ligne par ligne, mais rien dans le fichier ne dit si un
point local se transforme par `R·v` ou par `Rᵀ·v`. Les deux lectures donnent
des modèles plausibles tant que les pièces ne sont pas tournées les unes par
rapport aux autres, ce qui est le cas de la plupart des props.

Les collisions tranchent. Chaque modèle de collision (`docs/collision.md`)
porte une boîte englobante dans l'espace du modèle, calculée par l'outil
d'export de Rockstar, donc avec la bonne convention. `tests/test_transform`
relie 3 862 collisions à leur modèle par l'identifiant des définitions `.idb`,
compose l'arbre NIF sous les deux conventions, et compare les **dimensions**
des boîtes (les positions absolues ne servent à rien : beaucoup d'intérieurs
sont modélisés à leurs coordonnées monde alors que leur collision est locale).

| | Modèles |
|---|---|
| appariés à une collision | 3 842 |
| dont les deux conventions donnent la même boîte à 5 % près | 3 782 |
| discriminants | 60 |
| quasi exacts (écart < 1 %) avec `R·v` seulement | 12 |
| quasi exacts avec `Rᵀ·v` seulement | 0 |

Le cas le plus net est `catwalk`, une passerelle à poteaux et rambardes : écart
de 0,0001 avec `R·v`, de 42 avec la transposée. Les 48 autres discriminants
sont des modèles dont la collision ne suit pas la géométrie visible, ils ne
départagent rien de fiable.

La convention est donc `v' = R·(s·v) + t`, avec R telle qu'elle est lue, et
`monde(enfant) = monde(parent) ∘ local(enfant)`. Elle est implémentée dans
`src/gamebryo/NifTransform`, utilisée par `nif2obj`, par `outils/rendu` et par
le futur moteur.

## Voir un modèle sans Blender : outils/rendu

```sh
BULLY_DATA=/chemin/vers/Bully build/outils/rendu 70wagon sortie.ppm 35 20 800
```

`src/render/SoftRaster` est un rasteriseur logiciel minimal : projection
orthographique, tampon de profondeur, placage de texture par pixel, éclairage
par face. Il n'a aucune dépendance et il est écrit pour rester portable vers
une cible sans GPU exploitable, ce qui sera d'abord le cas sur New 3DS. Ce
n'est pas le moteur de rendu du jeu, c'est l'outil qui permet de regarder ce
que les chargeurs produisent.

## L'espace de l'entité : le nœud du modèle

Sous « Scene Root », chaque modèle a un nœud qui porte son nom, et c'est la
transformation de **ce** nœud que le jeu remplace par la matrice de l'entité.
Les grandes pièces d'intérieur (`BX_loungeFL`, `iboxing`, `SC1b_bldgLib`…) y
gardent leur position dans le monde ; `DPI_Teacup` y garde un décalage
d'artiste de −38,7 en x ; `pxFireEx` y a zéro. Dans les trois cas, le
placement `Ipl$` fournit la position réelle et le nœud du modèle est ignoré.

`NifWalkShapes` compose donc, par défaut, la racine et ses enfants directs
comme l'identité. Le test des collisions le confirme : dans cet espace, la
boîte des sommets tombe **au même endroit** que la boîte de collision, et pas
seulement aux mêmes dimensions. C'est aussi l'espace où `scene` applique les
placements (`docs/ipl.md`).
