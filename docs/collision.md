# Collisions

Les modèles de collision vivent dans `Stream/World.img` : 488 fichiers
`.col`, 3 863 modèles, presque tous au format « COL3 » (version 4, drapeaux
1, 2 ou 3), quelques « COLL » (vélos, objets anciens) et « COL2 ».
`CStreaming` les nomme par les identifiants 0x56b8..0x58ac (500 slots de
collision, voir `docs/streaming.md`) et les charge par
`CFileLoader::LoadCollisionFile` (0x42c790 la première fois, 0x42c570
ensuite) qui appelle `LoadCollisionModel` (0x42bb60) pour chaque modèle.

Le format est décrit en tête de `src/collision/ColModel.h`. Points propres
à Bully par rapport à Vice City :

- en-tête de 36 octets pour COL2/COL3 (fourcc, taille, version u16,
  drapeaux u16, nom[20], id i32) ; le nom vide signifie « l'id désigne le
  modèle » ;
- la boîte englobante et les boîtes de collision ont un dword de bourrage
  après chaque vecteur (alignement 16) ;
- les sommets COLL sont quatre flottants ;
- les triangles COL3 font 8 octets (12 si version et drapeaux valent 3),
  COL2 cinq dwords, COLL quatre ou cinq ;
- deux blocs facultatifs suivent : 0xef5dcb33 (arbre de partition : 40
  octets d'en-tête, 12 par nœud, compte au 10e dword ; 766 modèles) et
  « LIMK » (compte i16 → +0x32 du modèle, puis 3 dwords par triangle ;
  72 modèles) ;
- `CColModel` fait 0x40 octets et pointe vers des données de collision
  (0x56d980) dont la disposition n'est pas retrouvée ; 0x42a8e0 est appelé
  après le chargement (plans des triangles, à lire).

`Coll/ColAreas.cfg` (lu par 0x56f420) regroupe les fichiers de collision
par zone : un nom de zone en tête de ligne, puis les fichiers indentés.
`test_col` vérifie `70wagon.col`, `bike.col` et l'exactitude de la taille
consommée sur les 488 fichiers.
