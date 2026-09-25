# Les placements « Ipl$ » (.ipb) et le rendu d'une scène

`Stream/World.img` contient 85 fichiers `.ipb`, un par zone (`iboxing`,
`isc_dorm`, `ftest`…). Chacun est une suite de sections nommées ; celle qui
place les objets est `inst`, lue par `CIplFile` en entrées `CIplInst`
(`src/core/IplFile.h`) :

| champ | taille | sens |
|---|---|---|
| modelId | 4 | identifiant du modèle dans les `.idb` |
| name | 64 | nom du modèle ; vide sur certaines entrées, alors l'identifiant sert |
| unk68 | 4 | 22.0 partout dans ftest.ipb, rôle inconnu |
| pos | 12 | position monde |
| scale | 12 | échelle **par axe** |
| rot | 16 | quaternion x, y, z, w |
| unk112, unk116 | 8 | rôle inconnu |

Le modèle nommé se lit dans `World.img` en `<nom>.nif`, son dictionnaire de
textures vient de la colonne TXD des `.idb` (`docs/idb.md`).

## Modèles jamais dessinés

Certains placements ne sont que des volumes utilitaires : `nog_`, `nogo_` et
`walkable_` (le binaire les marque du drapeau 0x1000000, `IsNogOrWalkable`),
et les modèles « no draw » en `_nd` ou `_ND_`. Leur géométrie est parfois
aberrante (triangles à z ≈ −320 sous `FGRD_ND`). `outils/scene` les saute.

## Le sens du quaternion

Le quaternion d'un placement, converti par la formule usuelle, donne une
matrice `R` telle que `v' = R·v` tourne un point. Mais c'est la **transposée**
qui est la bonne, autrement dit le conjugué du quaternion tourne les sommets.
Visuellement les deux lectures donnent chacune une scène cohérente (l'une est
l'image miroir tournée de l'autre) et mesurer le contact entre objets et murs
ne les sépare pas : 87,18 % contre 86,98 % de sommets dans la boîte des pièces
fixes, sur 3 285 objets tournés de plus de 23°.

Ce sont les modèles eux-mêmes qui tranchent. Pour une partie des placements,
le nœud du modèle (`docs/nif.md`, « L'espace de l'entité ») est resté **à la
position du placement, avec une rotation** : l'artiste a exporté l'objet
déjà posé, et le placement encode la même pose. Sur les 11 nœuds de modèle
tournés à la position exacte de leur placement, 7 sont égaux à 10⁻⁶ près à la
matrice du **conjugué** (`matchstick_1p`, `matchbox_1p`, `0iobserv06`,
`CH_Sludge01`, `boilerhanglamp09N03`…), aucun à celle du quaternion tel quel,
et 4 (les écrans `LibrScreen01`, `DL_iAsylumScreen`) ne collent à aucune des
deux. C'est `tests/test_placement`, qui échoue si la formule usuelle se met à
coller quelque part.

La conversion vit dans `NifFromPlacement` (`src/gamebryo/NifTransform`) :
matrice transposée, échelle par axe appliquée dans les colonnes de `R` (avant
la rotation), translation = `pos`. Un point du modèle passe dans le monde par
`place ∘ (composition de l'arbre NIF en espace entité)`.

## outils/scene

    BULLY_DATA=<racine> build/outils/scene iboxing.ipb sortie.ppm [azimut] [élévation] [taille] [--coupe f]

Place tous les modèles d'un `.ipb`, charge chaque dictionnaire de textures une
fois, et dessine le tout avec `SoftRaster` en caméra orthographique en orbite.
`--coupe 0.6` ne dessine pas les triangles au-dessus de 60 % de la hauteur de
la scène : on enlève le toit pour regarder dans une pièce. `iboxing.ipb` sort
comme un bâtiment complet, 56 modèles, 20 738 triangles, 145 textures ;
`isc_dorm.ipb` vu de dessus avec `--coupe 0.6` montre les chambres du dortoir
et leur mobilier à sa place.
