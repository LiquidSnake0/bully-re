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

## Avancement

**3,4 % du code de `bully.exe` recréé** (475 fonctions sur 20 819), 7,2 % compris
(code, documentation, noms). Le détail par sous-système est dans
[docs/avancement.md](docs/avancement.md), régénéré à chaque commit par
`tools/avancement.py`.

Les **scripts Lua du jeu tournent** dans `outils/lancer_lua`, sur la VM Lua 5.0.2
d'origine : `main.lua` et ses threads image par image, la banque des missions lue
dans les arbres d'actions, et la première mission, `1_01`, jouée jusqu'à
`MissionSucceed` — sa cinématique `1-1-1` lue dans `Cuts.img` (61,7 s), les
changements de zone par la machine à états du moteur (fondu, scripts de zone
retirés puis chargés, portes appariées par `DATInit`), la parole scriptée, les
piétons qu'elle crée et fait marcher. Depuis le 21.10, à la nouvelle partie, **`1_02` s'enchaîne
seule** après `1_01` (les conditions des nœuds de mission lues : `MissionCond`, `TimeCond`, `DayCond`,
`NOT`, `OR`), sa narration se précharge et dure le temps de ses vrais échantillons (`Speech.bin`), le
joueur sort de l'école par une porte `DT_` et la mission va jusqu'au combat d'entraînement contre Russell.
Depuis le 22.10, le contrôleur d'actions du joueur descend dans l'arbre que pose `PedSetActionTree` (les
68 arbres globaux sont chargés au démarrage, comme le moteur) : l'entraînement démarre, et avec `--jouer`
(la parade, puis `0_BLOCK_0`) la leçon de parade passe. Depuis la seconde séance du 22.10, les conditions des nœuds sont jugées (`ConditionGroup`, `WeaponEquiped`, `ActionRequest`, `ActionNodePlayTime`, `TargetPlaying`, `LuaCond`) et chaque piéton a un contrôleur d'actions qui redescend dans son arbre quand le nœud joué finit : la séquence de boutons, la prise et sa charge passent (`--demander`, `--sequence`). Depuis la troisième séance du 22.10, les passages viennent des pistes elles-mêmes, comme dans le moteur : `Opportunity` (et son loquet aléatoire), `Sequence` et `PlayOnTarget`, avec leur règle de priorité ; le bully poussé joue `PushFwd/RCV` et la leçon de prise se termine. Depuis la quatrième séance du 22.10, les coups portent : les pistes `Collision` (le coup touche la cible verrouillée, l'outil n'ayant pas les os), la réaction de la cible par l'arbre des coups (`ConditionFlag`, `MePlaying`), une `Sequence` qui sort de l'arbre posé, et la fin des animations lue dans `MAINPED.HXD` ; poussé, le bully tombe, et le coup de pied lourd au sol lui fait jouer `GroundHitHeavy` : la leçon des coups au sol passe et `1_02` affiche son objectif de combat. Depuis la séance du 23.10, les coups blessent : un `Execute` ajoute les pistes du nœud visé à celles du contrôleur, comme dans le moteur, et la `DamageHandle` de `HitTree/…/Base/HitBase` ôte les dégâts du coup reçu ; les piétons reçoivent leur entrée de `pedstats.dat` (santé, régénération, seuil d'humiliation), les pistes `SetHealth`, `SetPedFlags` et `SetTargetAsGrappleTarget` sont jouées et onze conditions des coups sont jugées (`HitTime`, `Health`, `IsHealthAtHumiliationLevel`…) ; dans `1_02`, le bully se relève, onze jabs le mènent à 5 points de vie et la mission demande de l'humilier (`1_02_HumiliateHim`).
L'outil joue le joueur (`--entrer`, `--aller`, `--zone`, `--porte`, `--jouer`, `--demander`, `--sequence`) ; tout le reste répond comme le
moteur, ou dit qu'il est un bouchon. Voir `docs/lua.md`, `docs/missions.md`, `docs/zones.md`,
`docs/cinematiques.md`.

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
- la synchronisation labiale `.lip`, 493 fichiers et 40 882 répliques,
  2 bits par image lus comme dans `bully.exe` ([docs/lip.md](docs/lip.md)) ;
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

## Ce qui vient après

Voir [docs/suite.md](docs/suite.md) : le premier point ouvert est le prochain chantier.

## Journal

Voir [docs/journal.md](docs/journal.md).
