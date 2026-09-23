# bully-re

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
- la numérotation du streaming ([docs/streaming.md](docs/streaming.md)) et
  les pools ([docs/pools.md](docs/pools.md)).

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
