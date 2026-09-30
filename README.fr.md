# bully-re

*Version française. [English version](README.md).*

Recréation du moteur de **Bully: Scholarship Edition** (PC, 2008) à partir de
l'exécutable, à la manière de re3 et reVC pour GTA. Projet de longue haleine,
commencé le 21 septembre 2026.

Le jeu n'est pas inclus et ne le sera jamais : ce dépôt ne contient que du code
réécrit, des outils d'analyse et de la documentation. Pour l'utiliser il faut
posséder le jeu (Steam ou disque).

## Ce qu'on sait déjà

- `bully.exe` v1.0 : 8 Mo, non protégé, RTTI intact, Gamebryo 2.3, Lua 5.
- 20 406 fonctions, 4,4 Mo de code, 1 522 classes reconstruites depuis le RTTI
  ([docs/classes.md](docs/classes.md)), 66 racines d'héritage.
- Le modèle d'objets est celui de GTA : `CPlaceable → CEntity → CPhysical →
  CVehicle → CBike`, `CPed`, `CPlayerPed`. Rockstar Vancouver a repris la base
  de Rockstar North. Les noms et l'ordre des méthodes virtuelles de re3 / reVC
  servent donc de point de départ.
- Au-dessus, un système d'arbres d'actions propre à Bully (`ActionNode`,
  `Condition*`, `*Track`, `*Objective`) qui pilote les personnages et les
  activités.

## Ce qui est recréé et vérifié sur les vrais fichiers

Chaque chargeur de données a un test hôte (`tests/construire_tests.sh`, puis
`BULLY_DATA=<racine du jeu> build/tests/test_…`) qui le compare aux fichiers
du jeu :

- `Config/dat/*.dat`, `handling.cfg` complet avec la boîte de vitesses et les
  conversions en unités du jeu ([src/vehicles](src/vehicles)) ;
- les définitions de modèles binaires `.idb` de `Objects/ide.img`, treize
  sections, 77 fichiers ([docs/idb.md](docs/idb.md)) ;
- les archives `.img` / `.dir` ([src/core/CdStream.h](src/core/CdStream.h)) ;
- les collisions COL3 / COL2 / COLL de `Stream/World.img`, 488 fichiers et
  3 863 modèles à l'octet près ([docs/collision.md](docs/collision.md)) ;
- les placements binaires « Ipl$ », 85 fichiers, onze sections
  ([src/core/IplFile.h](src/core/IplFile.h)) ;
- les modèles NIF de Gamebryo 2.3, 5 724 fichiers, géométrie, matériaux et
  textures décodés ([docs/nif.md](docs/nif.md)) ;
- les textures `.nft`, 4 469 fichiers, les 35 635 images décodées en DXT1,
  DXT5, RGB, RGBA et palette ([docs/nft.md](docs/nft.md)) ;
- un premier export visible : `outils/nif2obj` sort n'importe quel modèle en
  OBJ avec ses textures, et `outils/rendu` le dessine avec un rasteriseur
  logiciel sans dépendance ;
- le sens des rotations des nœuds NIF, tranché contre 3 842 boîtes de
  collision ([docs/nif.md](docs/nif.md)) ;
- une scène entière depuis les placements `Ipl$` : `outils/scene` rend une
  zone du jeu (bâtiment, dortoir) avec le sens du quaternion établi par les
  modèles eux-mêmes ([docs/ipl.md](docs/ipl.md)) ;
- une première visite en temps réel : `outils/visite` promène une caméra à la
  première personne dans une zone, rendu logiciel en 400 × 240 (l'écran du
  haut de la New 3DS), perspective correcte, découpage au plan proche et
  modes de transparence du jeu ; SDL2 n'ouvre que la fenêtre
  ([docs/rendu-temps-reel.md](docs/rendu-temps-reel.md)) ; F passe en marche,
  sur les volumes de collision du jeu (sol, marches, murs) ; dehors, les
  scènes voisines se chargent et se libèrent pendant qu'on avance
  (`outils/carte` dresse la carte des 85 fichiers de placements) : on passe
  de l'école au quartier d'affaires ; `--pietons n` pose des piétons du jeu
  déformés par leur squelette et **animés** par les animations du jeu (le
  format des `.agr` est déchiffré, [docs/agr.md](docs/agr.md)) : ils
  **marchent** dans le décor, sur le sol et contre les murs ;
- la numérotation du streaming ([docs/streaming.md](docs/streaming.md)) et
  les pools ([docs/pools.md](docs/pools.md)) ;
- la grille du monde, ses listes et ses dix pools d'entités
  ([docs/world.md](docs/world.md)).

Le plus gros écart avec GTA est là : un nœud de liste tient sur **un seul mot
de 32 bits** (4 bits de pool, 14 bits d'index, 14 bits pour le nœud suivant),
là où re3 en utilise trois pointeurs. Un secteur stocke donc des poignées et
non des pointeurs, et loge ses cinq listes dans 20 octets.

## Méthode

1. Cartographier : RTTI, tables virtuelles, formats de fichiers, appels de scripts Lua.
2. Recréer par sous-système, en commençant par ce qui se teste sans le rendu :
   chargeurs de données, `CPlaceable` / `CEntity`, véhicules.
3. Vérifier chaque fonction recréée contre l'original (mêmes entrées, mêmes sorties).
4. Gamebryo est recréé au fur et à mesure, uniquement ce que le jeu utilise.

## Outils

- `tools/ghidra/` : scripts headless (inventaire, RTTI, décompilation par table virtuelle).
- `tools/generer_squelettes.py` : produit `docs/classes.md` et `src/squelettes/`.
- Analyse sous Ghidra en conteneur (`blacktop/ghidra`), projet local hors dépôt.

## Journal

Voir [docs/journal.md](docs/journal.md).
