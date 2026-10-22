# Journal

## 21 septembre 2026

Départ. `bully.exe` extrait de l'ISO retail (le jeu est possédé sur Steam),
analysé sous Ghidra headless : 20 406 fonctions, 615 nommées par les imports
et le RTTI. 1 522 classes reconstruites avec leur hiérarchie et la taille de
leur table virtuelle. Découverte du soir : l'arbre `CPlaceable → CEntity →
CPhysical → CVehicle → CBike` est celui de GTA III / Vice City, ce qui donne
les noms de la moitié des méthodes virtuelles de base. Première classe
travaillée : `CBike` (60 virtuelles).

Suite de soirée. En-têtes écrits (`common.h`, `core/Lists.h`, `core/World.h`,
`core/ModelInfo.h`, `entities/*.h`, `vehicles/*.h`) : les trois sources
recréées compilent (`./construire.sh`, g++ seul, pas de make sur la machine).
Les 34 virtuelles de `CEntity` sont lues : les 17 de reVC sont toutes là dans
le même ordre (slots 0, 2, 3, 8 à 21), Bully en intercale 5 avant `Add` et en
ajoute 12 après. Slot 23 est une virtuelle pure. Les 18 de `CVehicle` (42 à
59) suivent aussi l'ordre reVC pour ce qui est reconnaissable
(`ProcessControlInputs`, composants, portes, `SetUpWheelColModel`,
`GetHeightAboveRoad`), avec trois virtuelles pures. Le monde fait 36 secteurs
de large, cinq listes par secteur.

Fin de soirée. `CBike` : cinq méthodes de plus recréées et sûres, parce
qu'elles sont la copie de reVC : `GetComponentWorldPosition`,
`IsComponentPresent`, `SetComponentRotation`, `RemoveRefsToVehicle`, et
`PlayCarHorn`, identique ligne pour ligne à `CAutomobile::PlayCarHorn` de
Vice City, même délai aléatoire, même compteur à 45. Le tableau `+0x580`
n'est pas un état mais les huit nœuds Gamebryo des composants du vélo.
`CPhysical` ajoute `ApplyMoveSpeed` / `ApplyTurnSpeed` en virtuelles, avec
`CTimer::ms_fTimeStep` en 0xc1a9a4. Bilan du jour : 11 fonctions recréées,
60 slots documentés, 1 522 classes cartographiées, ça compile.

Piétons. `CPed` (46 virtuelles) et `CPlayerPed` (47) lus, doc dans
`docs/vtable-ped.md`. Surprise : le slot 15 de `CEntity` n'est pas `Teleport`
(3765 octets, 19 paramètres chez le piéton), l'ordre reVC ne tient donc pas
partout. Une jauge de piéton découverte : +0x1d40 monte par un appel avec
montant et son pour le joueur, redescend avec le temps, avec un plancher
propre au joueur ; nommée `m_nTrouble` en attendant mieux. Le pool de piétons
(0xc0f5f0) a la disposition de re3 avec la taille d'entrée en quatrième mot.
Recréés : `CPed::AddTrouble`, `CPed::ProcessTroubleDecay`,
`CPlayerPed::ProcessTroubleDecay`. Quatre objets compilent.

La passerelle Lua. Le binaire garde les 914 noms de l'API Lua
(`docs/api-lua.txt`) dans des tables `{ nom, fonction C }`. Un script Ghidra
remonte du nom à la fonction C, puis à ce qu'elle appelle. Résultat immédiat :
la jauge +0x1d40 est `m_nPunishmentPoints`, le slot 45 `IncPunishmentPoints`,
+0x1ebc `m_nMinPunishmentPoints`, 0x4773d0 `SetPunishmentPoints`, le joueur
courant `0x00c1aea8`. C'est la méthode qui nommera le plus vite : chaque
fonction Lua désigne une fonction du moteur.

Carte Lua complète : 661 des 914 noms ont une table de liaison (les 253
autres sont des noms de classes ou de pistes d'action, pas des fonctions
Lua). 231 liaisons n'appellent qu'une seule fonction du moteur ; après
dédoublonnage, 150 fonctions reçoivent un nom sans ambiguïté et 32 ont
plusieurs candidats (`docs/fonctions-nommees.tsv`), plus les 19 lues à la
main ce soir : 169 fonctions renommées dans le projet Ghidra. `ApplyNames.java` reporte ces noms dans le projet
Ghidra, les décompilations suivantes seront lisibles. Fonctions les plus
appelées par l'API : 0x73ad50 (50 fois), 0x5ee6d0 (35), 0x471240 (29), à
identifier en priorité, ce sont des points d'entrée du moteur.

Points d'entrée (`docs/points-entree.md`) : l'allocateur du jeu (0x5ee6d0),
`CPed::PushObjective` (0x471240, pile de 20 objectifs à +0x58), la
traduction handle Lua → entité (0x5c2770), le pool de points, le hachage
des noms. Règle de nommage automatique resserrée : deux liaisons Lua au plus
par fonction du moteur, sinon le nom d'une seule appelante est trompeur ;
8 noms retirés, 142 gardés. Le `cp` interactif du shell a encore bloqué une
commande : toujours `\cp -f`.

Chargeurs de données. Le binaire contient l'assertion
`CFileLoader::LoadLevel('Config/Dat/default.dat')` et les noms de fichiers de
GTA : `SURFACE.DAT`, `PEDSTATS.DAT`, `CARCOLS.DAT`, `PED.DAT`, `OBJECT.DAT`.
La séquence de démarrage (0x42ec80, `docs/demarrage.md`) est jalonnée de
marqueurs de profilage vides qui nomment chaque étape. `CSurfaceTable::Initialise`
(0x45b250) est reVC à l'identique : recréé dans `src/core/SurfaceTable.cpp`,
avec un premier programme de test hôte (`tests/test_surface`) qui lira le
vrai `SURFACE.DAT` dès que les données du jeu seront extraites du MSI.

**Premier test contre le vrai jeu.** Les données ont été sorties des trois
cabs (clés MSI replacées avec les tables File / Component / Directory,
3 805 fichiers). `Config/dat/surface.dat` est le fichier de GTA III, signé
Richard Jobling, 14/02/00. Le `CSurfaceTable::Initialise` recréé le charge
et rend la matrice 6×6 attendue, symétrique, valeurs identiques au fichier
(rubber/rubber 6.0, wet/wet 0.5). Première fonction vérifiée sur données
réelles.

## 22 septembre 2026

Index. `ExportTout.java` décompile les 20 819 fonctions en JSONL (26 Mo,
35 minutes), `tools/indexer.py` en fait une base SQLite (64 Mo, deux
secondes) avec appels, appelants, chaînes, offsets lus ou écrits, globales,
et recherche plein texte. `tools/chercher.py offset 0x1d40` répond en une
seconde : sept fonctions écrivent les points de punition, cinq les lisent.

`pedstats.dat` : parseur lu dans l'index, recréé, testé sur le vrai fichier.
Deuxième chargeur vérifié. `CGame::Initialise` (0x42ee90) porte les noms de
GTA en clair dans ses marqueurs (`CWorld::Initialise()`, `CPickups::Init()`,
`CStreaming::Init()`), et le moteur audio s'appelle Screamer.

`carcols.dat` : `CVehicleModelInfo::LoadVehicleColours` (0x5355b0), même
logique que Vice City avec une table de 256 couleurs et seize entiers par
ligne de véhicule ; lecture par un objet fichier (0x4264c0, 0x42d4b0) et
non par `work_buff`. Recréé, vérifié : 100 couleurs, 16 véhicules, huit
paires pour `bike`. Troisième chargeur vérifié. Les chargeurs suivants sont
repérés par leur chaîne dans l'index : `HANDLING.CFG` (0x4c9d30),
`PED.DAT` (0x499ff0), `colours.dat`, `PedPop`, `VehPop`, `Cloths`, `OBJECT.DAT`
(0x4d0c90). Sept objets et trois tests compilent.

`handling.cfg` : le fichier de GTA III (Bill Henderson, 10/12/1999), 81
véhicules, 15 lignes vélo, 10 lignes hydravion lues puis ignorées, 11 lignes
bateau. `cHandlingDataMgr::LoadHandlingData` (0x4c9d30) est reVC avec une
disposition d'entrée propre (0xdc octets, 34 colonnes dont une colonne de
dégâts aux piétons), un vélo à 23 champs, et la table des 120 identifiants
de Vice City en 0xa357c8 où les motos sont remplacées par les vélos de
Bully. Recréé avec la table, vérifié sur COMET, BIKE et PREDATOR. Quatrième
chargeur vérifié. Reste `ConvertDataToGameUnits` (0x4c9ad0), lisible mais
qui dépend de constantes globales à identifier.

`object.dat` : rien à voir avec celui de GTA, 22 colonnes propres à Bully
(points de vie, effets de destruction, sons, butes de ramassage), 80 lignes
d'alias. Nouveau découpeur de ligne `CTokenizer` (0x61a310), propre à Bully.
Recréés et vérifiés : cinquième chargeur. Erreur corrigée : 0x85c6f0 n'est
pas `CWorld::GetSectorIndex` mais `_ftol`, appelé par 412 fonctions.
Dix objets et cinq tests compilent.

Le monde sans image, premier pas. `CGame::Initialise` donne les adresses :
`CWorld::Initialise` 0x45d430 (drapeaux à zéro, comme reVC), `InitModelIndices`
0x43e910, `CPickups::Init` 0x444900, `CdStreamAddImage` 0x73a740,
`CFileLoader::LoadLevel` 0x42cd30. `CPools::Initialise` (0x44d1d0) crée 28
pools ; la classe de chaque pool sort du constructeur passé à
`vector constructor iterator` : 24 piétons construits en `CPlayerPed`
(8004 octets), 15 véhicules en `CAutomobile` (0x8c0), 2250 `CBuilding`
(0x120 = la taille de `CEntity`), 300 `CDummy`, 275 `CObject`, plus les
pilotes d'animation propres à Bully. `docs/pools.md`, `src/core/Pools.cpp`,
`src/core/World.cpp`. Douze objets compilent.

Les archives. `CFileLoader::LoadLevel` (0x42cd30) est petit : IMAGEPATH,
TEXDICTION, IMGIDE, SKY_DOME, EXIT ; COLFILE / MODELFILE / HIERFILE reconnus
mais ignorés. Les définitions de modèles ne sont plus des IDE texte mais des
`.idb` binaires emballés dans `Objects/ide.img` (77 fichiers, `default.idb`
en premier) et lus par `LoadImgIde` (0x42caf0) puis `LoadIdeBinary`
(0x42c970) ; les IDE texte sont encore dans `Objects/` comme source.
`Stream/World.img` (1,9 Go) contient 5 724 modèles `.nif`, 4 469 textures
`.nft`, 550 groupes d'animations, 488 collisions, 52 scripts Lua compilés.
`CdStream` recréé (répertoire .dir de 32 octets par entrée), vérifié :
11 980 entrées, `Algie1.lur` en tête. Sixième test vert, quatorze objets.

Les `.idb`. Format déchiffré (`docs/idb.md`) : sections taguées par leur
nom GTA à l'envers, chaînes sur dwords, plage d'ids après chaque section.
`CIdeBinary` recréé avec la section `peds`, vérifié sur `default.idb`
extrait de `ide.img` : 259 piétons, `player` et `DOgirl_Zoe_EG` identiques
au IDE texte, plage de modèles 0 à 258. Septième test vert, quinze objets.
Reste onze sections à recréer, `objs` en premier.

## 2026-09-22 (suite) — section objs des .idb

- `CIdeBinary::LoadObjs` (0x42aa20) recréée, avec `ConvertFlags` (0x429d30),
  `IsSpecialObjectId` (0x429e70) et le test `nog_` / `walkable_`.
- Disposition validée par script sur les 77 fichiers de `Objects/ide.img` :
  3 297 entrées objs, toutes de type 0, chaque section retombant sur un tag
  connu ou la fin du fichier.
- Ordre des sections de `default.idb` relevé ; il faut cars, weap, cash,
  item et scnd avant d'atteindre objs en lecture séquentielle, le test entre
  donc directement sur le tag pour l'instant.
- `test_ide` : 6 objs de default.idb conformes au texte, 122 de ifunhous.idb.
- Suite : cars, weap, item, cash, scnd, clth, puis tobj, accs, panm, 2dfx.
  Les 77 `.idb` se lisent en entier ; `default.idb` en séquence (le dword
  de tête compte les octets qui le suivent, corrigé dans le test).
- Chaque `.idb` a son IDE texte (`Interior/`, `Prop/`, `Terrain/`), ce qui
  a permis de vérifier objs/tobj/2dfx sur `iboxing`, panm sur `props`, accs
  sur `access`.
- Handling : `CTransmission` (0x5c octets à +0x34, sans vitesse de croisière),
  `InitGearRatios` (0x4ca5f0, passage à 0.95 de l'écart au lieu de 0.6667),
  `ConvertDataToGameUnits` (0x4c9ad0) et `ConvertBikeDataToGameUnits`
  (0x4c9ca0) recréés ; les deux facteurs d'échelle sont des globales bss
  dont l'écriture n'a pas été retrouvée, valeurs de Vice City prises.
- `Stream/World.img` contient 5 724 `.nif`, 4 469 `.nft`, 550 `.agr`, 493
  `.lip`, 488 `.col`, 119 `.cat`, 85 `.ipb`, 52 `.lur`.
- Collisions : `src/collision/ColModel.h/.cpp`, format COL3/COL2/COLL de
  Bully déchiffré (en-tête de 36 octets, bourrages, blocs KD et LIMK) ;
  `test_col` lit les 488 fichiers de World.img à l'octet près (3 863 modèles).
- Placements binaires « Ipl$ » : `src/core/IplFile.h/.cpp`, sections inst,
  spec, proj, occl, prop, rail, perm, pont ; `test_ipl` sur ftest.ipb et les
  85 fichiers (reste pois, quatre fichiers). Table des identifiants de
  streaming dans `docs/streaming.md`.
- IPL : sections pois (4 chaînes + 21 dwords par point), perm (compte au
  4e dword), trig (19 dwords), pthx (9 dwords par point, déduit des données)
  ajoutées ; les 85 `.ipb` se lisent jusqu'à la queue de zéros.

## 2026-09-23 — lecteur NIF

- `src/gamebryo/NifFile` : en-tête, table des blocs avec tailles, chaînes,
  et décodage de NiNode, NiTriShape/NiTriStrips, NiTriShapeData/
  NiTriStripsData, NiSourceTexture, NiMaterialProperty, NiTexturingProperty.
  Dispositions établies sur les octets (docs/nif.md), y compris les
  tangentes (bit 12 des drapeaux) et la transformation de texture (32 o).
- 247 fichiers `CS_*` sont grand-boutistes : boutisme appliqué à partir du
  nombre de types. Les 5 724 fichiers se lisent, 286 403 blocs connus décodés
  à l'octet près.
- Prochaine étape : les textures `.nft`, puis un premier rendu d'une
  géométrie NIF.

## 2026-09-23 — CWorld, et la surprise des listes

Reprise du chantier `CWorld` laissé en plan (le travail était mis de côté et
ne compilait pas). Les champs de `CEntity` qu'il utilisait n'étaient pas
déclarés ; leurs décalages sont maintenant vérifiés un par un dans
`CWorld::Add` (0x45d560), qui les lit sur un `int*` : `bIsStatic` +0x28,
`bIsBIGBuilding` +0x58, `bIsStaticWaitingForCollision` +0xac, et un quatrième
champ +0xf0 dont le sens n'est pas retrouvé mais qui remplace le test
`IsPhysical()` de reVC. `m_scanCode` est en +0x10a, juste avant
`m_modelIndex`.

**Les listes de Bully ne sont pas celles de re3.** Un nœud tient sur un seul
mot de 32 bits : 4 bits de pool, 14 bits d'index dans ce pool, 14 bits
d'index du nœud suivant (0x3fff = fin). Un secteur ne stocke donc aucun
pointeur d'entité, seulement des poignées, et le chaînage lui-même est un
index relatif à la base `0xc0f788`. C'est ce qui fait qu'un secteur tient en
20 octets pour cinq listes.

`CPools::GetEntity` (0x44a290) résout une poignée sur dix pools, et
`GetEntityPoolAndIndex` (0x44c7e0) choisit le pool à partir de `m_type`. Ça
relie enfin les 28 pools de `docs/pools.md` aux types d'entités : seuls dix
sont référençables depuis une liste du monde, et le type 4 se répartit entre
objets, projectiles et objets de cinématique selon +0xc4 et +0xec. Deux pools
pour le type 1, séparés par le slot 34 (`GetIsATreadable` dans re3).

Détail à ne pas perdre : `RemoveFromMovingList` (0x4696f0) fait avancer le
curseur global `ms_pMovingListCursor` avant de décrocher le nœud, sinon la
boucle qui parcourt la liste des mobiles perdrait le fil en retirant l'entité
en cours de traitement. Le test le vérifie.

Tout est écrit dans `docs/world.md`. La recréation garde des nœuds à
pointeurs plutôt que des mots compressés : la sémantique est la même et la
disposition mémoire n'est de toute façon pas reproduite ailleurs. L'écart est
documenté.

`tests/test_world.cpp` ne lit aucun fichier du jeu, il vérifie
l'arithmétique de la grille contre des valeurs calculées à la main, l'ordre
des cinq listes, les deux listes globales et les gardes de la liste des
mobiles. 19 objets compilent, tous les tests passent.

- Prochaine étape, inchangée : les textures `.nft`, puis un premier rendu
  d'une géométrie NIF.

## 2026-09-24 — les textures `.nft`

Surprise du chantier : **un `.nft` est un fichier NIF**. Même ligne d'en-tête,
même table de blocs, seul le contenu change puisqu'il n'y a que des textures.
Le lecteur `src/gamebryo/NifFile` s'y applique donc tel quel, il ne lui
manquait que quatre types de blocs.

- `NiPixelData` : la disposition est établie et sa taille se calcule
  exactement, 79 + 12 × mipmaps + numPixels × numFaces. Quatre canaux toujours
  présents, les inutilisés portant le type 19 et la convention 5. Les tailles
  confirment du DXT1, une 8 × 8 tenant en 32 octets.
- `NiPalette`, `NiStringExtraData`, `NiIntegerExtraData` ajoutés aussi, et
  `NiSourceCubeMap` qui se lit comme un `NiSourceTexture`.
- Piège corrigé au passage : `NiSourceTexture` garde **deux mots après
  `useExternal`** quel que soit le cas. Pour une texture externe, le second
  vaut -1 au lieu de pointer vers les pixels. J'avais d'abord cru à un champ
  en moins, ce qui cassait les 466 `NiSourceCubeMap` des `.nif`.
- Effet de bord bienvenu : le test des modèles décode maintenant **363 085
  blocs** contre 286 403 avant.
- Le boutisme se comporte comme pour les `.nif` : 131 fichiers `CS_*` sont
  grand-boutistes, et le nombre de blocs se lit toujours en petit-boutiste.

`tests/test_nft` lit les 4 469 fichiers et compare chaque bloc à la taille
annoncée : 4 465 passent sans un octet d'écart, 142 157 blocs, 258 535 niveaux
de mipmap, 1,25 Go de pixels. Les quatre qui résistent (BBonusB,
Barr01_Switch, BeerKeg, BirdBath) portent un octet parasite dont je n'ai pas
trouvé l'origine ; ils sont décrits dans `docs/nft.md` et le test les attend
en échec plutôt que de les masquer.

- Prochaine étape : dessiner une première géométrie NIF, maintenant que les
  pixels sont lisibles.

## 2026-09-25 — décoder les textures, sortir un premier modèle

- `src/gamebryo/TextureDecode` : cinq formats réellement présents, DXT1 pour
  presque tout, DXT5 pour l'alpha, et quelques RGB, RGBA et palettes. Les
  35 635 textures se décodent.
- Hypothèse réfutée : les blocs DXT des fichiers grand-boutistes **ne sont pas**
  inversés. Mesuré sur 1,7 million de blocs, les lire tels quels donne la même
  statistique que les fichiers petit-boutistes, les inverser donne le hasard.
- `outils/nif2obj` : un modèle, son dictionnaire de textures lu dans les `.idb`
  (le lien TXD de GTA), l'arbre de nœuds composé, les textures décodées en TGA.
  Premier modèle vu à l'écran : le break `70wagon`, assemblé correctement.
- Prochaine étape : un vrai rendu temps réel, et éprouver la convention de
  rotation sur un modèle à pièces tournées.

## 2026-09-25 — le sens des rotations, tranché ; un rasteriseur en dépôt

- La convention `v' = R·v` que `nif2obj` supposait est maintenant **vérifiée
  contre les collisions** : sur les 60 modèles à pièces tournées où les deux
  lectures diffèrent, douze correspondent presque exactement à la boîte de
  collision avec `R·v`, aucun avec la transposée. `catwalk` fait 0,0001 contre
  42. C'est `tests/test_transform`, et la composition vit dans
  `src/gamebryo/NifTransform`, partagée par l'export et le rendu.
- Leçon de méthode : comparer les positions absolues des boîtes ne donnait que
  du bruit, beaucoup d'intérieurs étant modélisés en coordonnées monde. Comparer
  les dimensions a fait apparaître le signal.
- `src/render/SoftRaster` + `outils/rendu` : premier rendu en dépôt, sans
  dépendance, avec tampon de profondeur et texture par pixel. Le script Python
  jetable est à la poubelle.
- Prochaine étape : le rendu depuis les placements `Ipl$` (une scène, pas un
  modèle), puis la question du moteur temps réel.

## 2026-09-25 — une scène entière, et le sens des placements

- `outils/scene` : tous les modèles d'un `.ipb` placés et dessinés ensemble.
  La salle de boxe (`iboxing`) sort comme un bâtiment complet, le dortoir
  (`isc_dorm`) vu de dessus sans son toit montre chaque chambre meublée. Le
  chargement partagé par les trois outils est dans `outils/commun.h`, la
  scène dans `outils/scene.h`.
- Deux pièges sur la route. Les grandes pièces d'intérieur sont modélisées en
  coordonnées monde dans leur propre nœud, et étaient placées une seconde fois
  : d'où la règle de l'**espace entité**, le nœud fille de « Scene Root » qui
  porte le nom du modèle est ignoré, c'est l'entité qui le remplace. Vérifié :
  dans cet espace, 3 155 boîtes sur 3 842 tombent **au même endroit** que la
  collision, et `DormGxref82` (une forme directement sous la racine) suit la
  même règle. Puis les volumes `nog_`/`walkable_`/`_nd` qui ne se dessinent
  jamais et laissent des triangles à z ≈ −320.
- Le sens du quaternion des placements a résisté une soirée : les deux
  lectures donnent une scène cohérente, et compter les objets dans les murs
  ne sépare rien (87,18 % contre 86,98 %). Ce sont les modèles qui ont parlé :
  sur les nœuds de modèle restés tournés à la position de leur placement, 7
  collent au **conjugué** à 10⁻⁶, 0 au quaternion brut. La conversion est
  `NifFromPlacement`, le test `tests/test_placement`, l'histoire dans
  `docs/ipl.md`.
- Prochaine étape : le moteur temps réel, avec le rendu logiciel comme
  référence de ce que chaque image doit contenir.

## 2026-09-29 — première visite en temps réel

- `outils/visite` : on marche dans la salle de boxe et dans le dortoir en
  temps réel, caméra à la première personne. Rendu entièrement logiciel en
  400 × 240 (l'écran du haut de la New 3DS), 3,7 à 4,5 ms par image sur PC ;
  SDL2 seulement pour la fenêtre. Voir `docs/rendu-temps-reel.md`.
- `src/render/Camera` (repère, projection, découpage au plan proche) et
  `RasterTrianglePersp` (perspective correcte en u/w, fonctions d'arête pas
  à pas). `tests/test_camera` ne lit aucun fichier du jeu et vérifie la
  projection, le découpage et la perspective : la moitié d'une texture sur un
  sol incliné tombe à y = 16, pas 32.
- `NiAlphaProperty` décodée : 2 305 blocs à l'octet près, 365 390 blocs
  connus au lieu de 363 085. Quatre modes de rendu ; les halos de lumière de
  la salle de boxe, qui sortaient en carrés noirs, sont des ajouts.
- Le chargement d'une scène `.ipb` est passé dans `outils/scene.h`
  (`ChargerPlacements`), partagé par `scene` et `visite`.
- Prochaine étape : marcher sur les collisions au lieu de voler, puis
  enchaîner les scènes voisines.
- Suite, même jour : **la marche**. F passe du vol à la marche ; les
  collisions du jeu (488 `.col`, 3 862 modèles) sont posées avec la
  transformation des modèles, `src/collision/Marche` fait le sol, les marches
  (45 cm), les murs et la gravité, `tests/test_marche` le vérifie sans fichier
  du jeu. Dans la salle de boxe, le corps s'arrête au bord du ring à un rayon
  près et reste au sol. Leçon : `NOGO_` est une zone interdite, un volume
  fermé ; l'exclure faisait sortir de la zone jouable et tomber dans le vide.


## 30.09.2026 — les scènes s'enchaînent

- `outils/carte` : les 85 fichiers de placements et leur emprise. Le monde
  extérieur = les quartiers `t*`, les tuiles `zone_*` (mobilier urbain) et
  `tGlobal` (relief, sur toute la carte) ; les intérieurs `i*` sont à part.
- La visite charge ce qui est à moins de 60 m et libère ce qui passe au-delà
  de 120 m, pendant qu'on avance : de la cour de l'école au quartier
  d'affaires sans rechargement. `--survol n` le vérifie sans écran.
- Rejet par modèle (sphère contre cône de vue et 250 m) : même image au
  pixel près, école 65,6 → 47,3 ms, boxe 4,3 → 3,1 ms.
- Détails : `docs/rendu-temps-reel.md`.
- Suite, même soir : **le rendu extérieur allégé**. La mesure montre que la
  rasterisation prend les deux tiers du temps. `NiStencilProperty` décodée
  (1 129 blocs, tous « deux faces ») : les autres formes perdent leurs faces
  arrière. Modèles triés du plus proche au plus lointain (la transparence à
  l'envers), boucle par pixel allégée. École 39,7 → 29,9 ms, quartier
  d'affaires 42,1 → 28,2 ms, boxe 3,1 → 2,3 ms.
- Suite : **les premiers piétons**. `--pietons n` pose des piétons de la
  section « peds » (modèles et textures du jeu, pose de repos) en cercle dans
  la visite. Formes cachées (APP_CULLED) respectées partout, textures cherchées
  dans le dictionnaire du modèle quand celui de l'IDE est une variante
  d'hiver, lacet au signe du conjugué du quaternion. 11 piétons sur 12 dans la
  cour de l'école. Suite : le squelette, puis les animations.
- Suite : **le squelette**. NiSkinInstance, NiSkinData et NiSkinPartition
  décodés (7 769 blocs, tous à l'octet près). Formule du skinning établie en
  essayant les combinaisons : os ∘ liaison, sans transposition, 2,3 mm
  d'écart sur Jimmy. `src/gamebryo/NifSkin` (transformations des nœuds avec
  une pose, déformation des sommets), `tests/test_skin`. Les piétons sont
  déformés par leur squelette ; `--bras` leur baisse les bras (pose de
  démonstration). Suite : les animations des `.agr`.
- Suite : **les animations**. Format `.agr` déchiffré pour le type 1002 (2 772
  animations sur 3 443) : le décompresseur d'images clés trouvé dans
  `bully.exe` (0x6b1710, quaternion sur 44 bits, instant sur 9, lien vers la
  clé précédente sur 11), l'ordre des 36 pistes établi sur les os immobiles de
  Jimmy. `src/anim/Agr`, `tests/test_agr`, `docs/agr.md`. Les piétons de la
  visite jouent l'attente de leur catégorie, en temps réel.
- Suite : **les piétons marchent**. Positions des `.agr` rangées par os (clé 1
  = bassin, clé 35 = flèche ARROW, dont le trajet est le déplacement). Les
  vraies marches sont dans les groupes F_* de l'IDE et avancent vers +y ; les
  S*_S qui avancent vers −y sont des reculs (vérifié à l'image). Marche sur le
  sol et contre les murs, demi-tour quand bloqué. Collisions en grille de 4 m :
  2,8 s → 11 ms pour 5 s de foule, résultat identique.

## 18.10.2026 — l'octet +0xc, les zones, les cinématiques

- **L'octet +0xc de l'entrée de liste est l'AREACODE.** `FUN_006d59a0`, le lecteur
  des blocs `POINT`, l'écrit juste après avoir lu le mot-clé ; `FUN_005d2310`
  (`PlayerSetPosPoint`) le relit, remplace −1 par `DAT_00bd1008`, et le passe à
  `FUN_00415730` comme zone de destination. Le même lecteur donne tous les autres
  champs d'un point (étiquette, objet à ramasser, son, horaires, masque météo de
  `FUN_00593f20`, mission) : `CTriggerPointPoint` les lit tous, `tests/test_trigger`
  compte 49 listes `SV_`, 114 `DT_` (les trois lettres telles quelles), 408 objets à
  ramasser, 82 points de mission sur les 6 783. Tableau dans docs/trigger.md.
- Suite : **AREACODE ≠ zone visible, tranché.** La zone visible est `DAT_00bd1008`, écrite
  par la seule `FUN_00415750`, à l'état 9 de la machine de changement de zone
  (`FUN_00416e30`, l'objet `PTR_DAT_009e4f4c`) ; l'AREACODE d'un point n'est que la
  destination que `PlayerSetPosPoint` demande. Les points du bureau du principal disent 5
  (l'ancien intérieur `PrincipalOffice`, dont la porte est `Disabled_` dans AreaTran.dat),
  le bureau est dans l'intérieur de l'école (2). `src/core/AreaManager` recrée la table
  des scripts de zone, la demande et la machine (fondu, scripts de zone retirés puis
  chargés, joueur posé à l'arrivée) ; `tests/test_zones` suit ses quatre chemins ;
  `lancer_lua` charge `MainMap.lua` puis `SchoolHallways.lua` pour de vrai, et leur
  `main` signale la zone prête (`F_PreDATInit` → `AreaSignalAreaTransitionReadyToLoad`).
  `AreaTransitionXYZ` n'est pas du moteur : SGlFunc.lua la définit. docs/zones.md.
- Suite : **les cinématiques, lues.** Aucune mission ne parle au moteur : les 80 scripts
  appellent `PlayCutsceneWithLoad` de `SCutscenes.lua`, qui enchaîne `LoadCutscene`,
  `IsCutsceneLoaded`, `StartCutscene`, la boucle sur `GetCutsceneRunning` /
  `GetCutsceneTime` contre la durée de `cutsceneTable`, `StopCutscene`. Treize fonctions
  du moteur relevées avec leurs adresses ; la fin vient de l'arbre d'actions
  `/Global/CutScenes` (`FUN_006c3a70`), pas d'un compteur. Dans `Cuts/Cuts.img` : `.INF`
  (objets, jeux faciaux `f_`), `.SUB` (30 images/s, `{début}{fin}CLÉ`), `.DAT` (la piste
  caméra de `FUN_004f86d0` : champ de vision, position, cible, en clés à 6 et 12 colonnes),
  et les `.agr` / `.hxd` / `.mxd` à côté. `src/cutscene/CutsceneFiles` les lit tous,
  `tests/test_cutscene` passe les 863 entrées et vérifie `1-1-1` (61,67 s). docs/cinematiques.md.
- Suite : **`lancer_lua` joue les cinématiques.** `LoadCutscene` lit les fichiers de
  `Cuts.img`, `IsCutsceneLoaded` ne répond vrai que s'il y a des objets (comme
  `DAT_020c5be2`), `StartCutscene` lance le temps, `GetCutsceneRunning` /
  `GetCutsceneTime` le rendent, les sous-titres passent au journal, la fin est la dernière
  clé de la piste caméra ; `1-1-1` tourne 61,7 s dans `MissionSetup` de `1_01`, qui se
  termine réussie à 75,7 s ; `--sans-cinematiques` les saute.
- Suite : **`POI`, la cinquième énumération.** 0x5db170 retire le premier caractère et
  `FUN_00448e30` parcourt les groupes de points d'intérêt du dernier au premier : le nom
  d'un groupe rend son premier point, sinon le premier point de ce nom dans le groupe ; le
  handle est l'index dans le pool des `CStaticPOI` (0x20c7c84, nom haché à +0x84).
  `CDatPools::Poi` le refait, `tests/test_datpools` le vérifie (`1_04_NemSitSpot`,
  `F_ClassSmokers` dans « Smoking »), `lancer_lua` ne répond plus −1.
- Suite : **le chemin d'un nœud d'action.** `/Global/1_01/Talking` : la banque `Global`
  sous une racine sans nom (`FUN_005f6f20`), chaque `.act` greffé sous `Global` par sa
  banque de tête (`FUN_005f6c50`, l'extension changée en `.cat`), et `FUN_005f59b0` qui
  descend segment par segment par les noms hachés, `..` compris. `CActionBank` le refait,
  `tests/test_actionbank` suit les chemins de `1_01.lua` et le chargement à la demande
  (`FUN_004849a0`) ; `PedSetActionNode` répond vrai ou faux pour de vrai dans `lancer_lua`.
- Suite : **la parole des scripts.** `SoundPlayScriptedSpeechEvent(ped, "M_1_01", 101)`
  cherche le nom, sans la casse, dans une table de 493 événements de `bully.exe`
  (0x00ae74e8, `ALLY_ABOUT_TO_LEAVE` … `YEARBOOK`, 98 missions `M_…`), et passe l'index
  (9 bits) et la ligne (11 bits) au moteur du son ; `SoundSpeechPlaying` compare locuteur,
  événement et ligne. `src/audio/SpeechEvents` reprend la table, `tests/test_speech` la
  vérifie, `lancer_lua` les joue (2,5 s par ligne, durée réelle non lue) : Jimmy parle à la
  secrétaire et `1_01` finit à 12,1 s. docs/son.md.
- Suite : **le corona n'a pas de mode, il a une taille.** `FUN_006a8650`, la mise à jour de
  la piste `MissionButtonTrigger`, lit l'objet par les décalages des attributs : +0x1c
  (champ 28) est le demi-côté du pavé testé par `FUN_005c2910` autour du point — 2 m pour
  `1_01`, 1 m pour les autres —, +0x20 le marqueur, +0x28 (champ 40) sa taille, +0x30 la
  clé de l'invite `BUT_LAUNCH`, puis la manette est lue et `FUN_006acfa0` lance. Le
  « mode 2 » du 09.10 était ce demi-côté. `CMissionData` lit ces champs, `test_missions`
  les vérifie, le gestionnaire de `lancer_lua` teste le pavé au lieu d'une sphère de 2 m.
- Suite : **ce que fait `DATInit`.** 0x5bf410 → `FUN_006d2200` → `FUN_00416000` : la table
  des portes du gestionnaire de zones, refaite depuis les pools : chaque liste `DT_` avec tous
  les déclencheurs de son nom (marque 1 : la porte passe par son objet, +0x394), chaque liste
  `SV_` avec le premier (marque 0 : en entrant dans le déclencheur, vers le premier point,
  dans la zone de son AREACODE) ; puis `FUN_004154e0` crée les objets à ramasser des listes
  dont l'AREACODE est la zone visible — les deux usages du champ. `AreaTran.dat` : 7 paires
  `SV_` ; les `*_doors.dat` : 38 `DT_`, dont la porte d'entrée de l'école (cour → hall).
  `CAreaManager::ReconstruirePortes` et `PasserUnePorte`, `tests/test_portes`, `DATInit` réel.
- Suite : **le pavé fermé.** `FUN_005c2910` délègue à `FUN_0046d960` (trois axes) ou
  `FUN_0046d880` (deux) : bornes remises dans l'ordre, puis min ≤ p ≤ max, les bornes
  comprises. `PaveContient` le refait pour `PlayerIsInAreaXYZ`, `PedIsInAreaXYZ` (réelle
  désormais) et le corona ; `tests/test_zones` vérifie les bornes et le corona de `1_01`.
- **Bilan du jour** : dix commits. Établi : +0xc = AREACODE et tous les champs des blocs
  POINT ; la zone visible et la machine de changement de zone (`CAreaManager`, scripts de
  zone chargés pour de vrai) ; les cinématiques (treize fonctions, les fichiers de
  `Cuts.img`, jouées dans `lancer_lua`) ; `POI` ; le chemin d'un nœud d'action
  (`CActionBank`) ; la table des 493 événements de parole ; le corona est un pavé, pas un
  mode ; `DATInit` refait les portes et crée les objets à ramasser ; le pavé fermé. `1_01`
  se termine réussie avec sa cinématique (78,1 s) ou sans (12,1 s), à la nouvelle partie
  comme par `--mission`. Ouvert : les conditions des nœuds pour enchaîner `1_02`, les portes
  `DT_`, la durée de la parole, jouer les nœuds d'action, les constantes des fondus.

## 21.10.2026 — 1_02 s'enchaîne

- **Les conditions des nœuds, construites sur place.** `FUN_005fa910` passe chaque
  enregistrement à la fabrique `FUN_0061a4f0`, qui cherche la classe dans le registre de
  `FUN_0064b090` (419 classes : nom, création, construction) et construit l'objet **à +4 de
  l'enregistrement** : les champs sont les mots du fichier. `MissionCond` (`FUN_006a7db0`)
  est vraie quand le mot +2 de l'entrée de la mission nommée n'est pas nul, et ce mot est le
  compte des réussites (`FUN_006ac910` l'incrémente, `FUN_006a9ff0` les compte) ; `TimeCond`
  (`FUN_00425cf0`) compare l'heure du jeu à une plage en minutes qui peut passer minuit ;
  `ConditionFalse` rend 0. `CMissionBank::Evaluer` / `Activable`, `tests/test_missions` (la
  chaîne `1_01` → `1_02` → `1_02A`, `1_03` entre 8 h et 19 h, une plage de nuit). Dans
  `lancer_lua --nouvelle-partie`, **`1_02` s'active et démarre seule à la réussite de `1_01`**
  (15,15 s), sans `--mission`. 195 des 333 missions à conditions s'évaluent déjà ;
  `ConditionGroup` est la suivante par le nombre. docs/missions.md.
- Suite : **le préchargement de la parole.** `1_02` attendait `SoundIsSpeechPreloaded`. `SoundPreloadSpeech`
  (0x5d4a20) remet à zéro le bloc `DAT_00cfb3b4` (en attente, échéance à 30 s, jouer, préchargée,
  locuteur) et le confie au moteur du son avec le rappel `FUN_005d49d0`, qui pose « préchargée »
  quand la ligne est prête, la retient tant que `SoundPlayPreloadedSpeech` (0x5d39d0) n'a pas posé
  « jouer », et l'annule passé l'échéance. `CSpeechPreload`, `tests/test_speech` ; dans `lancer_lua`,
  la narration de `1_02` (quatre lignes) et la secrétaire passent, et la mission donne au joueur
  l'objectif du dortoir à 32,3 s. docs/son.md.
- Suite : **la vraie durée de la parole.** `SpeechDB.bin` donne pour chaque événement ses éléments
  (voix = indice du modèle du locuteur, ligne sur un octet, numéro d'échantillon), et le numéro est
  l'indice dans la table triée de `Speech.bin` (« Hash », 40 400 entrées, clé = hachage `FUN_005a7c10`
  du nom de fichier, vérifié sur les 40 400 noms de `Speech.lst`). Chaque échantillon est une petite
  banque XACT (`SDBK`, puis `WBND` à +0x800) dont l'onde porte sa durée. `CSpeechBank`,
  `tests/test_speechbank` ; dans `lancer_lua`, les lignes durent leur échantillon : `1_01` finit à
  10,5 s (la ligne de Jimmy dure 0,80 s, pas 2,5), la narration de `1_02` prend 18 s. docs/son.md.
- Suite : **les portes `DT_`.** L'état 0 de `FUN_00416e30` les passe quand le mot +0x394 du gestionnaire,
  le déclencheur de la porte que le joueur pousse (`FUN_00415160`), est le leur ; neuf zones
  (`FUN_0042e570`) gardent le retour (`FUN_00416750`, +0x86f), que la porte suivante rejoue.
  `CAreaManager::PousserPorte`, `tests/test_portes` (37 portes `DT_` hors `tschool_doors`, 6 sorties du
  hall, l'aller-retour par la salle de classe) ; `lancer_lua --porte`. docs/zones.md.
- Suite : **NOT, OR et la liste.** `FUN_005fb9b0` parcourt la liste de conditions d'un nœud : NOT et OR
  sont des conditions vraies de genre 1 et 2 (emplacement 2 de la table virtuelle), le résultat suit
  la dernière condition (inversée après un NOT), un faux arrête la liste sauf devant un OR, et un vrai
  avant un OR saute la suite jusqu'au prochain opérateur. `FUN_006ad130` ne réactive une mission
  réussie que si son champ 44 (+0x2c) le dit (171 missions). `CMissionBank::Activable`,
  `tests/test_missions` (sept listes faites à la main). docs/missions.md.
- Suite : **`tests/test_transform` repasse.** Il échouait depuis le 25.09 (`exactT == 0` : un modèle,
  `jphoto`, collait à la lecture transposée). Le parcours « Rᵀ·v » du test gardait les nœuds et les
  formes cachés (drapeau 1, `APP_CULLED`) que `NifWalkShapes` saute comme Gamebryo : `jphoto` porte
  une forme cachée qui gonflait une boîte et pas l'autre. Les deux parcours suivent maintenant la même
  règle ; il ne reste que la différence de convention : 47 modèles départagent, 13 collent à `R·v` à
  moins de 1 %, aucun à `Rᵀ·v`, et `catwalk` reste à 0,0000 contre 42.
- Suite : **`1_02` jusqu'au combat de Russell.** Avec `--porte DT_ischool_FrontDoorL` puis `--entrer
  _GARYTRIGGER`, la mission joue la scène de Gary et lance l'entraînement au combat. Les coups suivent
  maintenant `FUN_0047b6b0` / `FUN_004782a0` : invulnérabilité (+0x138c), invulnérabilité au joueur
  (+0x1364, type 13), santé minimale (+0x1ca8) ; `PedSetInvulnerable`, `PedSetInvulnerableToPlayer`,
  `PlayerSetInvulnerable`, `PedSetMinHealth` réels ; `CScriptPedPool::Blesser`, `tests/test_scriptpeds`.
  Le joueur ne meurt plus sous les coups de Russell (il mourait à 66,5 s). Reste : `PedIsPlaying` du
  joueur sur `/Global/TrainingPlayer/Default`, que l'entraînement attend.
- Suite : **`PedIsPlaying`.** `FUN_005f3bf0` compare le nœud du chemin au nœud joué (+0x10 du contrôleur)
  et, avec le troisième argument, `FUN_005f4f80` remonte les parents du nœud joué. `CActionBank::Joue`,
  `tests/test_actionbank` ; réel dans `lancer_lua` sur le nœud que `PedSetActionNode` pose. L'entraînement
  de `1_02` attend `/Global/TrainingPlayer/Default`, que seul le contrôleur, en jouant l'arbre posé par
  `PedSetActionTree`, atteint : c'est le chantier suivant. docs/cat.md.
- Suite : **`CurrentMission` et `CurrentClassMission`.** L'assembleur de `FUN_006a7db0` range les deux noms
  dans deux globales : `CurrentMission` vaut la mission en cours (`FUN_006aa630`, un des deux
  emplacements du gestionnaire), `CurrentClassMission` le cours actif (`FUN_006a9ed0`) ; le premier
  relevé les avait inversés. `CEtatMissions::courante` / `coursCourant`, `tests/test_missions`.
- Suite : **`DayCond`.** `FUN_006a8b30` compare son champ au jour du calendrier (`DAT_020c3640`, ce que
  `GetCurrentDay` rend), modulo le +0x18 de `TimeCycleButes` (5 dans `Globals.cat`) quand l'octet +8 le
  demande, ce que font les 59 `DayCond` des missions. `GetCurrentDay` réel dans `lancer_lua`.
- **Bilan du jour** : dix commits. `1_02` s'enchaîne seule après `1_01` (conditions `MissionCond`,
  `TimeCond`, `DayCond`, `ConditionFalse`, `NOT` / `OR` et la liste de `FUN_005fb9b0`, `CurrentMission`,
  le champ 44 « rejouable ») ; la parole se précharge (`SoundPreloadSpeech`) et dure ses vrais
  échantillons (`SpeechDB.bin`, la table de `Speech.bin`, banques XACT) ; les portes `DT_` passent
  (+0x394, retour gardé) ; les coups respectent invulnérabilité et santé minimale ; `PedIsPlaying` est
  réel ; `tests/test_transform` repasse (formes cachées). Dans `lancer_lua --nouvelle-partie`, `1_02`
  joue sa narration, la secrétaire, la sortie de l'école et va jusqu'au combat de Russell. Ouvert : le
  contrôleur d'actions qui descend dans l'arbre de `PedSetActionTree` (l'entraînement attend
  `/Global/TrainingPlayer/Default`), `ConditionGroup` (la référence vers le groupe nommé), les constantes
  des fondus, le recul de `FUN_00416750`, l'animation de la porte qui pose +0x394.

## 22.10.2026

- **Le contrôleur d'actions.** `PedSetActionTree` (0x5cac20) pose l'arbre au contrôleur (`FUN_005dc040`), qui y
  descend : une banque teste ses conditions puis rend le premier enfant qui aboutit (`FUN_005f5490`), un nœud
  jouable ou une feuille se rend lui-même (`FUN_005f5500`). `CActionBank::Choisir` ; dans `1_02`, le joueur joue
  `/Global/TrainingPlayer/Default/Default` et l'entraînement passe à la parade.
- **Les fondus.** `AreaTransitionButes` (`DAT_020c8910`), trouvée dans `Globals.cat` par `FUN_0072ad00` : 0,65 s
  au noir (+0x18), 0,5 s au retour (+0x1c), les défauts de `CameraFade(-1)` et du changement de zone (au lieu
  d'une seconde). `TimeCycleButes` est relue de la même façon (5 jours).
- **Le recul des portes.** `FUN_00416750` sans objet de porte : 2,5 m en arrière le long du regard
  (`DAT_00900ab0`), cap retourné de π (`DAT_00900160`).
- **Les arbres globaux.** `FUN_005f6f20` charge 68 fichiers au démarrage (liste 0xb0a4f0) : `HitTree`, que la
  leçon de parade attend, en fait partie.
- **`lancer_lua --jouer`** joue un nœud pour le joueur (la manette) ; **`PedMePlaying`** compare le nom au nœud joué
  et à ses parents (`FUN_005f46a0`). Avec `--jouer /Global/HitTree/Standing/BlockHits@42 --jouer
  /Global/GlobalLoco/Locomotion/Combat/0_BLOCK_0:Act/Globals/GlobalLoco.act@45`, la leçon passe et
  l'entraînement revient à `/Global/TrainingPlayer/Default`.
- **`PlayerIsInAreaObject`** (0x5d14f0) et **`PedLockTarget`** (0x5cb060) réels, appelés à chaque image par `1_02`.
- `tests/test_surface` passe (37 tests sur 37) : le plantage du 21.10 venait de la façon de le lancer, pas du chargeur.
- **Bilan du jour** : dix commits ; 403 fonctions recréées (2,7 % du code), contre 389. Ouvert : juger les
  conditions des nœuds d'action (garder les données des .cat), la résolution de `ConditionGroup` (le +4 n'est pas
  donné par la seconde table de renvois telle qu'on la lit), redescendre quand le nœud joué finit.

## 22.10.2026, seconde séance

- **`ConditionGroup`.** La seconde table de renvois ne cite pas des chaînes : sa valeur est un décalage après la fin
  de l'arbre, où le fichier range des listes de conditions (un octet n, n décalages). Le +4 d'une `ConditionGroup`
  (mot +8 de l'enregistrement) y renvoie : c'est son groupe. Les 3 220 listes des 479 fichiers se lisent ainsi.
- **Les conditions des nœuds sont jugées.** `CActionBank` garde les données des .cat ; la liste de `FUN_005fb9b0`
  sort de `CMissionBank::Activable` dans `ConditionListe.h` et sert aux nœuds et aux groupes ; `NOT`, `OR`, `False`,
  `ConditionGroup` par la banque, le reste par un juge : `WeaponEquiped` (le nom de l'arme cité, « unarmed » vrai
  dans l'outil), `ActionRequest` (n° de l'action), `ActionNodePlayTime` (temps du nœud, comparateurs 2 et 5 lus
  `<` et `≥`), `TargetPlaying` (le nœud de la cible verrouillée), `LuaCond` (la fonction du script de mission).
- **Le contrôleur d'actions** (`CActionController`) : chaque piéton de `lancer_lua` en a un ; la fin d'un nœud vient
  de ses pistes (+12 + +16) ; quand il finit, ou qu'une demande attend, un enfant qui aboutit le remplace, sinon
  il redescend depuis l'arbre posé. `PedSetActionNode` et `--jouer` entrent dans le nœud (une banque descend).
- **Les inclusions** `i` sont remplacées par l'arbre de leur fichier : `/Global/Actions/Grapples/…` existe.
- **L'historique des boutons** : les rappels « réussie » et « ratée » de la séquence sont gardés ; `--sequence @s`
  fait réussir le joueur.
- Dans `1_02`, la leçon de combat avance jusqu'à la prise et sa charge :
  `lancer_lua 130 --mission 1_02 --sans-cinematiques --porte DT_ischool_FrontDoorL@3 --entrer _GARYTRIGGER@6
  --jouer /Global/HitTree/Standing/BlockHits@42 --jouer /Global/GlobalLoco/Locomotion/Combat/0_BLOCK_0:Act/Globals/GlobalLoco.act@45
  --demander 0@46 --sequence @48 --sequence @51 --sequence @53 --jouer /Global/Actions/Grapples/Front/Grapples/GrappleAttempt/GrappleSuccess/GIVE@54
  --sequence @56 --jouer /Global/Actions/Grapples/Front/Grapples/GrappleMoves/GrappleStrikes/HitC/Charge@57` :
  la parade relâchée ramène le joueur à `/Global/TrainingPlayer/Default`, la séquence X passe, la prise (`GIVE`)
  ouvre la leçon de coup, la charge descend dans son coup (0,57 s) et le joueur revient à `Default`.
- **Bilan de la séance** : quinze commits ; 403 fonctions recréées (2,7 % du code), comme ce matin : la séance a
  porté sur les données (groupes, champs des conditions) et l'outil, sans nouvelle fonction du moteur citée.
  Ouvert : les pistes `Opportunity`, les comparateurs d'`ActionNodePlayTime` et le +8 d'`ActionRequest` à
  confirmer dans les classes (`FUN_0064b090`), la fin de la leçon de prise, les coups au sol, le combat de Russell.


## 22.10.2026, troisième séance

- **Les passages viennent des pistes.** `Opportunity` (`FUN_005f7d70`) : tirage, nœud visé relatif, descente, prise ;
  fermeture +0x28, bascule +0x2c ; à l'arrêt (`FUN_005f7440`) la demande de plus forte priorité (`FUN_005f3bc0`, +0x34)
  fait passer. `OpportunityRandomLatch` tire sa bascule entre +0x4c et +0x50 (`rand()` du CRT). `Sequence` dépose une
  demande de priorité 0 à la fin du nœud (`FUN_005f7030`). `PlayOnTarget` fait jouer un nœud à la cible
  (`FUN_006615f0`). La règle de l'outil reste en secours. Relevé : 3 080 `Opportunity` au démarrage, aucune avec des
  conditions propres.
- **Les deux suppositions levées** : `ActionNodePlayTime` compare par `FUN_005fb750` (0 ≈, 1 ≉, 2 <, 3 >, 4 ≤, 5 ≥, à
  1e-5 près) ; `ActionRequest` est `ConditionActionRequestButton` (`FUN_005fd650`) : +8 le mode du bouton (3 lâché, 2
  bit 0 ou 1, sinon bit 0), +0xc / +0x10 / +0x14 la durée tenue.
- **`1_02`** : avec `--jouer GIVE@60 --sequence @62 --jouer …/DirectionalPush/PushFwd@64`, le bully joue `PushFwd/RCV`,
  la leçon de prise se termine et pose la séquence des coups au sol (réussie à 67 s). Reste : les pistes `Collision`
  pour que le coup au sol touche le bully, puis le combat de Russell.
- **Garde-fous de l'outil, dits** : pas de pile de nœuds joués ni de références montées ; une `Opportunity` qui vise
  un nœud contenant déjà le nœud joué, ou hors de l'arbre posé, ne fait pas passer.
- **Bilan** : 417 fonctions recréées (2,8 % du code), contre 403 ce matin.

## 22.10.2026, quatrième séance

- **Une `Sequence` sort de l'arbre posé** (`FUN_005f7030` / `FUN_005f4e30` ne le limitent pas) : après la poussée,
  `PushFwd/RCV` finit et le bully passe par `HitTree/Standing/PostHit/BellyUp/OnGroundBounce` puis `BellyUp/On_Ground` ;
  la charge de la prise enchaîne `Charge_Pt2`.
- **La fin des animations** (`AnimationTrack`, `FUN_0060ae00`) : en mode 0, la piste finit quand le temps (parti de +40,
  vitesse +48) dépasse +44 ou la durée de l'animation ; 3 169 durées lues dans `MAINPED.HXD`.
- **Les coups** (`CollisionTrack`, `FUN_005f9e70`) : fenêtre +0x18 / +0x1c, drapeau +0x2c, dégâts +0x34, nœud de
  l'attaquant +0x68 ; les `Collision` d'un nœud joué par un `Execute` sont relevées. Le coup touche la cible verrouillée
  (l'outil n'a pas les os) ; la cible descend dans `HitTree` (à terre : `GroundAndWallHits/On_Ground`, supposé) ; jugées :
  `ConditionFlag`, `MePlaying`, `HitLocation` (supposée vraie pendant un coup). Le coup reçu est le slot 15 de `CPed`
  (`FUN_0047b810`).
- **`1_02`** : avec `--sequence @63` (la prise maintenue, `1_02_GrapplePunch`) puis la poussée à 64 s, la leçon pose
  `1_02_GroundKicks` à 65 s ; le coup de pied lourd au sol (`--jouer …/GroundAttacks/GroundAttacks/Strikes/HeavyAttacks@67`)
  porte, le bully joue `GroundAndWallHits/On_Ground/BellyUp/GroundHitHeavy/GroundHitHeavy`, et la mission affiche
  `1_02_FightMsg` (67,2 s). `LANCER_CONTROLEURS=1` journalise tous les contrôleurs.
- **Supposé, dit** : la route directe vers `GroundAndWallHits` (le moteur passe par un `Execute` de `HitTreeBase`) ; le
  corps projeté ne touche pas celui qui le pousse ; le drapeau du coup tient une seconde ; `HitLocation`.
- **Bilan** : 423 fonctions recréées (2,9 % du code), contre 417 ce matin.
