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

## 23.10.2026

- **Un `Execute` n'a pas de contrôleur à part** : son lancement (`FUN_005f7070`) fait descendre le nœud visé, jugé, et
  `FUN_005f3e20` ajoute ses pistes à la liste du contrôleur ; l'outil en garde les `Opportunity`, `Collision`,
  `DamageHandle`, `SetHealth`, `SetPedFlags`, la `Sequence` et la fin. La supposition du 22.10 (un contrôleur
  secondaire) est levée.
- **Les coups blessent** : le relevé du coup (+0x408 : attaquant +0x40c, temps +0x434 / +0x438, dégâts +0x440,
  drapeau +0x46c) et les dégâts de `FUN_0047b810` (piste +0x34, `PlayerButes` +0xac pour le joueur, arrondis aux
  facteurs +0x1cd4 / +0x1cfc) ; la `DamageHandle` d'un nœud de coup (`Base/HitBase`, facteur 1) les porte par
  `FUN_0047b6b0` → `FUN_004782a0`. `SetHealth`, `SetPedFlags` (les drapeaux de +0x1314 ; les invulnérabilités en sont),
  `SetTargetAsGrappleTarget`, `HitAnimation`, l'arrêt d'une `Opportunity` à la fin du nœud.
- **pedstats.dat dans les piétons** (`FUN_00478e30`) : santé maximale (F), régénération (G, `FUN_00472ff0`), les 64
  colonnes ; `GameGetPedStat`, `GameSetPedStat`, `PedGetMaxHealth`, `PedSetMaxHealth`, `PedSetFightingSystem` réels ;
  les colonnes STAT et SIZE des entrées peds. Le bully de l'entraînement est `GN_Bully05` (146, `STAT_B_STRIKER_A`) :
  95 points, seuil d'humiliation 15 %.
- **Conditions jugées** : `HitTime`, `stat` (0x5ffff0, hors de l'index), `IsPlayer`, `PedFightingSystem`, `Health`,
  `PercentageHealth`, `DamagePending`, `IsHealthAtHumiliationLevel`, `IsMissionSpecific`, `CharacterSize`. Garde-fous
  desserrés : une `Opportunity` reste dans l'arbre global de son porteur, ou remonte jusqu'à `/Global`.
- **`1_02`** : le coup de pied au sol de 67 s ôte 8 points, `1_02_FightMsg` (67,2 s) ; le bully se relève (67,9 s) ;
  onze jabs (`--jouer …/LightAttacks@72,73,…,82`, la nouvelle forme à plusieurs instants) le mènent de 87 à 5 points :
  **`1_02_HumiliateHim` à 82,1 s**, `TutorialStart("HUMILIATEX")` à 83,2 s. À 84 s, l'humiliation commence
  (`SetTargetAsGrappleTarget` : `WaitForVictim` pour le joueur, `Humiliate_Me` pour le bully), mais le bully tombe dans
  `Humiliate_Me/Fail`.
- **Ce qui bloque, dit** : `HumiliationGroupIs` (+0xd44, aucune écriture directe dans le binaire), les conditions de
  `RCV` (`AnimationGroupLoaded`, `IsSpeechPreloaded`, `TargetRelativePosition`) ; `PedFlags` non jugée (un chemin à
  cinq `..` vers `ExecuteNodes/StanceTransition/Give` ne se résout pas sans les références montées).
- **Bilan** : 451 fonctions recréées (3,2 % du code), contre 423 (2,9 %) ce matin.

## 23.10.2026 (soir)

- **`HumiliationGroupIs` élucidée** : +0xd44 n'est pas un champ écrit directement, c'est le +0x200 du composant social du
  piéton, en +0xb44 (`ecx = piéton + 0xb44` à l'assembleur). Construit à -1 (`FUN_0048e790`), posé par `FUN_0048f5d0`
  (la table des 22 groupes `HUMIL_` en 0x9113d0), choisi par `FUN_004925d0` selon la taille de la victime (le groupe déjà
  pris par un piéton de même taille, `FUN_00490ef0`, sinon un tirage), donné par `FUN_00492950` quand la santé passe
  sous la moitié, au lancement d'une piste `ManageTargets` (`FUN_00627420`) qui vise le joueur. Dans `1_02`, le bully
  (Medium) reçoit le groupe 0 à 76,16 s : `5-8_NoogieSpit`.
- **Les conditions propres des `Opportunity`** : le champ 60 vaut 0 dans le fichier, mais la seconde table de renvois le
  cite (951 des 3 080 du démarrage). Sans elles, l'attente du chargement de l'humiliation abandonnait (`ABORT`) dès la
  première image ; avec, l'abandon attend 6 s. `ActionNodePlayTimePast` lue (vraie : seuil -1).
- **Les conditions de `RCV`** : `AnimationGroupLoaded` (`CAnimGroups`, les 550 `.agr`, `LoadAnimationGroup` et
  `HasAnimGroupLoaded` réels), `IsSpeechPreloaded` (la parole retenue des `SpeechTrack`, lâchée par `SpeechStartTrack`),
  `TargetRelativePosition` ; et pour s'y trouver, `FaceTarget`, `SeekEntityPosition` (y compris sous un `spawn`) et les
  partenaires de prise (+0x155c).
- **`PedFlags` jugée** : la régression d'hier venait d'un nœud qui ne finissait pas. Un `Execute` ne retient pas son nœud
  (`FUN_005f70f0`, +0x31) ; la transition de posture de la charge vise un chemin qui n'existe pas
  (`GrappleMoves/ExecuteNodes`, alors qu'`ExecuteNodes` est sous `GrappleStrikes`) et passe aussitôt à `../Give`.
- **`1_02` se termine réussie à 119,13 s** (`lancer_lua 125`, la commande de docs/missions.md) : à 84,08 s les
  conditions de `RCV` passent, le joueur joue `GV`, la ligne `HUMILIATION_ANIM` 3 part, `HumiliateFlee` ôte les 5 derniers
  points, `KnockOuts` à 93,56 s, la rencontre avec Russell (`F_SetupRussellFight`), `MissionSucceed` à 119,10 s.
- **Lu sans le recréer** : l'objet de porte (+0x394) est posé par `ActivateDoor`, `PropAttemptDoorTransition` et une
  `PropAnimation` `OPEN` de l'objet que le joueur touche (docs/zones.md) ; la clé des références montées (`DAT_00b0a604`,
  `FUN_005f6290`).
- **Ce qui bloque encore** : les arbres de l'IA (`AI_FightTutorial`, son `CombatActions` lancé par `spawn` et une dizaine
  de conditions non lues) ; la locomotion des arbres (le bully ne fuit pas, les `FRIENDLYHIT` de sa chute touchent le
  joueur, 0 dégât).
- **Bilan** : 475 fonctions recréées (3,4 % du code), contre 451 (3,2 %) cet après-midi ; 18 commits.

## 23.10.2026 (nuit)

- **L'arbre de l'IA est joué** : `PedSetAITree` (0x5caac0 → `FUN_00484290`) pose l'arbre au contrôleur de l'IA du
  composant d'actions (+0x2e0 : le contrôleur +0x290, son contexte +0x358), que `FUN_00484490` fait avancer à chaque image
  et redescendre quand il ne joue rien. `PedAttack` met un `AttackPedObjective` (type 7) dans la file des objectifs
  (+0x5f0). Dix-huit conditions lues, par la table d'enregistrement `FUN_0064b090` et l'emplacement 1 des tables
  virtuelles (`Objective`, `AcquireTarget`, `JoinCombatZone`, `InCombatZone`, `Interval`, `WeightedRandom`,
  `ActionNodeRequest` = l'intention +0x254, `OpeningBranchChildHasChild`, `TargetConditionGroup`…). `LuaCond` lit le
  résultat du script par `lua_tonumber` : `0.0` est faux (l'outil le croyait vrai).
- **Les pistes de l'IA** : `ManageTargets` (sa mise à jour `FUN_00627420`, pas son lancement, donne le groupe
  d'humiliation : la supposition d'hier tombe), `CombatLoco` (la zone de combat, `PedJoinCombatZone`, les rayons
  d'`AIButes`), `ActionNodeRequest`, `spawn` (un contrôleur propre, qui reboucle), `ClearCurrentObjective`,
  `FleeObjective`. **L'octet +8 d'une piste retient son nœud** (`FUN_005f4230`) : un nœud qu'aucune piste ne retient finit
  tout de suite ; c'est ce qui rend la main au joueur après `GV/END`.
- **Dans `1_02`** : le bully entre dans `CombatLoco` à 30,06 s, se tient à 1,5 m, son `CombatActions` boucle
  (`ControlNode`, `PostHit/OnGround`, `PostHit/Hit`) ; le groupe 0 lui vient de `ManageTargets` à 76,20 s ;
  `HumiliateFlee` lui retire son objectif d'attaque et le fait fuir ; retourné, il tombe sans toucher le joueur (les coups
  ne portent que de face, supposé). Le succès d'hier reposait en partie sur ce coup parasite, qui sortait le joueur de
  `GV/END` : c'est maintenant la vraie règle de fin. **`1_02` se termine réussie à 119,03 s** ; `1_01` est inchangée.
- **Ce qui bloque encore** : l'intention `Short` ne fait pas jouer `Offense/Short` à l'arbre d'actions (`Fight_Tutorial`
  passe par les `ActionOpps` des références montées : la pile du contexte, `FUN_005f4e40`, `FUN_005f6290`, dont la clé
  reste à comprendre) ; la course de `RunStart` (l'intention de mouvement que la piste `Locomotion` exécute, que rien ne
  pose pour l'objectif 22 dans `AI_FightTutorial`).
- **Bilan** : 513 fonctions recréées (3,5 % du code), contre 475 (3,4 %) ce soir ; 1 265 comprises (7,7 %) ; 14 commits.

## 26.10.2026 (nuit)

- **Les références montées** : `DAT_01b649dc` est la banque `Global` elle-même, donc `FUN_005f4e40` rend le nom de l'arbre de
  tête (`Fight_Tutorial`), pas `Global` : c'est la clé. Une référence 'r' (`FUN_005f6bb0` → `FUN_005f6960`, vérifié à
  l'assembleur en 0x5f6a7e et 0x5f6b16) lit son fichier une fois sous `/Global/<nom>`, dans une
  `ActionNodeMultiParentReference` (`FUN_005f66b0`) dont le parent dépend de la clé (`FUN_005f6800`, `FUN_005f6290`) ; une
  référence vaut sa cible (`FUN_005f5da0`, `FUN_005f5340`, `FUN_005f5360`, `FUN_005f5470`). `Locomotion.act` (42 fichiers)
  n'existe qu'une fois, avec 28 parents.
- **La pile du contexte** (`FUN_005f36d0`, `FUN_005f3700`) : sa clé devient `DAT_00b0a604` le temps des chemins, comme
  `FUN_005f3e20` et `FUN_005f70f0`.
- **`LuaCond` dans l'espace de noms** (`FUN_006a74d0` → `FUN_005d80a0` → `FUN_005d8000` : `NSCall(fichier, fonction,
  false)`) : `F_CanBullyAttack` valait nil dans `_G`. Dans `1_02`, de 31,12 à 42,60 s, le bully attaque le joueur pendant la
  leçon de parade (`Offense/Short/Strikes`, par l'`Opportunity` `../../../Offense` des `ActionOpps` montées) : le blocage
  « l'intention `Short` ne mène pas à `Offense` » est levé.
- **Les piétons** : le type (+0x1310, la table 0x90e7d0, `FUN_004889c0`, `FUN_0047ffe0`), le sexe (+0x50 du modelinfo),
  `PedSetFaction`, `PedGetFaction`, `PedSetIsStealthMissionPed`, la réserve de sprint (+0x18d0), l'intention de mouvement
  (+0 la direction, +0x1c la vitesse ; les allures de `FUN_005c6180` : 0,3 / 0,7 / 1,1 / 1,5) ; le chapitre
  (`DAT_00a147e0`, `ChapterSet`, `ChapterGet`) et la saison (`DAT_00a147ec`).
- **Seize conditions** par le registre `FUN_0064b090` : `IsFemale`, `IsAuthority`, `PedType`, `PedModelID`, `IsElderly`,
  `HasGrappleTarget`, `IsStealthMissionChar`, `MotionIntentionSpeed`, `MotionIntention`, `Season`,
  `TargetRelativeOrientation`, `Sprint`, `IsPlayerTired`, `WeaponDesired`, `WeaponIsInventory`, et `IsPlayer` par le type ;
  `lancer_lua` dresse le bilan des conditions encore jugées -1.
- **`1_02` reste réussie à 119,03 s**, `1_01` à 10,46 s, à chaque commit.
- **Ce qui bloque encore** : `CollisionStart` (+0x4e5, posé par `FUN_005f9480`, sa remise à zéro introuvable) et
  `AttackPhase`, donc la parade du bully ; la course de `RunStart` (pas avancé) ; `HudComponentVisible`, les conditions
  sociales. Séance close avant l'objectif de 40 commits, à la demande : 12 commits de code et un de docs.
- **Bilan** : 549 fonctions recréées (3,6 % du code), contre 513 (3,5 %) ; 1 319 comprises (7,8 %).

## 27.10.2026 (nuit)

- **`CollisionStart` et `AttackPhase`** : la piste `Collision` tient la phase de l'attaque du piéton (+0x71c) et son échéance
  (+0x720) : 1 au lancement (`FUN_005f9480`), 2 dans la fenêtre et 3 après (`FUN_005f9e70`), 0 à l'arrêt (`FUN_005f8240`). L'octet
  +0x4e5 n'est jamais remis à 0 : à l'assembleur, seuls 0x5f94aa et 0x661694 (`WeaponCollisionTrack`) l'écrivent, à 1 ; la
  troisième écriture (0x6f2d68) est dans un autre objet. `CollisionStart` est donc vraie pour tout piéton qui a déjà frappé.
  L'arrêt pose aussi la minuterie du dernier coup (+0x550, `FUN_0048c040(0)`), que lit `AttackTimer`.
- **Le HUD** : 48 composants (`DAT_00c674a0`, la fabrique `FUN_0053cee0`), cachés à la naissance (`FUN_0053cc70`) ;
  `ToggleHUDComponentVisibility` et ses voisines sont réelles, `HudComponentVisible` lit l'octet +8.
- **Les caméras** : `DAT_00c3ccf8` est le mode courant (+0x90) du gestionnaire en 0xc3cc68 ; `CameraSetActive` et
  `CameraReturnToPlayer` ne rangent qu'une demande (+0x270), que seules deux pistes appliquent (`FUN_004f1740`).
- **Les manettes** : `DAT_00c2b120` (le joueur, posé par le constructeur de `CPlayerPed`) et `DAT_00c2b124` ; `Controller` passe par
  un octet du .data qui vaut 1 et qu'aucune instruction n'écrit.
- **La vitesse de `CombatLoco`** : le pas (`FUN_00625970`) donne à la navigation une vitesse selon le mode (2 et 4 : 0, 7 et 9,
  12 : deux formules, sinon 1,5) ; l'approche est le mode 11 : le 0,7 supposé tombe, le bully gagne sa zone en `Sprint`.
- **La course de `RunStart`** : pas résolue. La piste `Locomotion` lit l'intention ; aucune piste du nœud ne l'écrit ; les 35
  écritures de la vitesse relevées à l'assembleur sont dans la navigation et les pistes de locomotion de l'IA.
- **Seize conditions** : `HudComponentVisible`, `CollisionStart`, `AttackPhase`, `Controller`, `WeaponDesiredIsEquiped`,
  `WeaponAmmo`, `IsWeaponFiredWhenDropped`, `CollisionWeaponType` (le +0x10 du contexte est le relevé du coup reçu : +0x5e l'arme,
  +0x60 le genre), `IsSocialStateWait`, `IsSocializing` (+0xbb0 / +0xbb4, le composant social), `CameraActive`,
  `CameraInWidescreen`, `AttackTimer`, `Attacked`, `FightClass` (la colonne S de pedstats.dat), `RangeToFocus`. Au bilan de
  `1_02` ne restent que sept classes, dont `WeaponNeedInit` (762 jugements).
- **`1_02` reste réussie à 119,03 s**, `1_01` à 10,46 s, à chaque commit. Le joueur passe par `GrappleOpps` et
  `CombatIdle_Out`, le bully par `Sprint`, sans changer le chrono.
- **Ce qui bloque encore** : la vitesse de `RunStart` ; le reste de `CombatLoco` (le point tiré, les sous-états) ; la parade du
  bully (`BLOCK` attend la fin du `ControlNode`) ; `WeaponNeedInit`, `OnMission`, `ConversationActive`.
- **Bilan** : 614 fonctions recréées (4,1 % du code), contre 549 (3,6 %) ; 1 388 comprises (8,4 %) ; 10 commits.

## 27.10.2026 (soir)

- **La course de `RunStart`, établie** : il n'y en a pas. Trois angles : les vidages des entrées (`FUN_005f38e0`, l'écriture de +0x1c
  qui manquait au relevé des 35 : la piste `IDLE`, `FUN_0061d8e0`, et la garde de l'IA `FUN_00484490`, mort +0x1c98, endormi +0x1358,
  `DAT_00c2b108`), la vitesse de la navigation (`FUN_005e4c60` → `FUN_005e4a60` : 0 arrivé ou immobile, × cos de l'écart de cap près du
  point, au moins 0,1) et `FleeObjective` (aucune mise à jour, pas de branche 22). L'IA du bully reste dans `CombatLoco`, le joueur est
  sous le petit rayon : le pas est le mode 10, sur place, et la vitesse reste 0.
- **`CombatLoco` complet pour le combat à mains nues** : l'arrêt au petit rayon (`FUN_0062c740`), et les quatre sous-états
  (`FUN_0062c540`, `FUN_0062c740`, `FUN_0061b700`, `FUN_00625740`, le retour en 0x6257c0) : le point tiré entre les deux rayons, l'attente,
  le retour par le mode 12. Le bully tourne autour du joueur en lui faisant face.
- **La parade** : `ActionRequest` est `FUN_005fd570` (le bouton brut ; l'attribution à `FUN_005fd650`, `ActionRequestButton`, était
  inversée) ; la piste `ActionRequest` de l'IA appuie le bouton 12 (`FUN_006260d0`, `FUN_005f3930`), qui mène l'arbre d'actions dans
  `Defence/Block` ; avec `AttackerConditionGroup` (`FUN_00601e00`), un coup reçu là va dans `HitTree/Standing/BlockHits`. Une `Opportunity`
  prise fait finir son nœud même sans nœud visé (`FUN_005f7440` → `FUN_005f3bc0(…, 1)`, `FUN_005f49a0`) : l'IA sort de `Block`. Un premier
  commit concluait « le bully ne pare pas » ; le neuvième le corrige.
- **Huit conditions** : `WeaponNeedInit` (le +0x120 du `CWeapon`, posé par `SetWeaponInfo`, `FUN_00660f50`), `ConversationActive`
  (fausse avec un piéton, lue à l'assembleur), `BlockTimer`, `OnMission` (les états de `FUN_006ad3e0` relevés), `MissionCond` dans les
  arbres d'actions (la charge de prise se lâche tant que `C_Wrestling_1` n'est pas réussi), `PedIsInOutfit` et la garde-robe du joueur
  (`src/peds/Clothing`, `tests/test_clothing`), `AttackerConditionGroup`. Au bilan de `1_02` ne restent que `PropTargetInteractive`,
  `ParametricTime` et #3bb83b8f (8 jugements en tout).
- **Chronos** : `1_01` à 10,46 s à chaque commit ; `1_02` à 119,03 s, puis 118,93 s (le petit rayon : l'humiliation commence 0,1 s plus
  tôt), puis **121,94 s** (le bully pare quatre jabs : la commande de docs/missions.md en tape quinze et humilie à 87 s).
- **Ce qui bloque encore** : la mort par la piste `Death` (l'outil tient un piéton pour mort à santé nulle) ; dans `CombatLoco`, la cible
  vue (`FUN_0048d320`), le point valable (`FUN_006254c0`), la recherche de chemin ; `ParametricTime`, `PropTargetInteractive`.
  `tests/test_surface` plante déjà sur `main` avec `surface.dat` (hors CI, non touché).
- **Bilan** : 667 fonctions recréées (4,3 % du code), contre 614 (4,1 %) ; 1 446 comprises (8,6 %) ; 10 commits.

## 27.10.2026 (nuit, 2)

- **La mort d'un piéton** : seule la piste `Death` tue (`FUN_0060fdc0` → l'emplacement 42 de `CPed`, `FUN_00479110`, lu à l'assembleur) :
  santé à -100 (`_DAT_00906520`), +0x1c98 à 1, rien pour un immortel (+0x13fc, `PedSetImmortalFlag`). Une santé nulle ne fait qu'appeler
  un rappel Lua (`FUN_00478190`, `FUN_004782a0`). Dans `1_02`, le bully à 0 point reste vivant, son IA passe dans `AMBIENT/Idle` par
  l'`Opportunity` `../../../` de `CombatLoco`, et il meurt à 97,61 s par `BellyDown/Dead/KOReactions` → `Base/DeadCleanup`.
- **Plus aucune condition jugée -1 dans `1_02`** : `ParametricTime` (`FUN_005fbed0`, le +0x157c du piéton par le +0x38 du contexte),
  `PropTargetInteractive` (`FUN_00655110`, les objets proches, faux sans monde) et #3bb83b8f = `MissionUnlocked` (`FUN_006a72a0` : une
  réussite ou le bit 8 de `MissionForceCompleted`), retrouvés par le registre `FUN_0064b090` (419 noms relus et hachés). En la jugeant,
  un écart de l'outil est apparu et a été corrigé : sans coup reçu, `HitTime` vaut -1 (`FUN_005f3ab0`), il ne court pas depuis la
  création ; faute de quoi les renversements toujours ouverts de `Grapples.cat` (`not MissionUnlocked[GrapTestUnlock]`) faisaient rompre
  la prise au bully à 57,75 s.
- **La vue** (`FUN_0048d320`) : le dernier agresseur à moins de 5 s et 5 m (+0x1dbc, posé quand l'octet +0x214 de la piste `Collision`
  est mis, relu à l'appel de la méthode du coup), un cache de 16 piétons, le cône et la portée des colonnes D et E de `pedstats.dat`
  (100 degrés, 12 m pour le bully). `CombatLoco` ne suppose plus la cible vue.
- **La piste `DamageTarget`** (`FUN_0060fad0`, `FUN_0060fbe0`, `FUN_0060fc60`) : les coups portés pendant une prise ; sans effet dans
  `1_02`, où la commande force `GIVE` sans partenaire de prise.
- **`1_03` démarre** : `PedFollowPath`, `PathGetLastNode`, `PedPathNodeReachedDistance`, `DistanceBetweenCoords3d/2d` réels, et la piste
  `FollowPath` jouée comme un geste (le nœud suivant `FUN_0061acc0`, les arrêts `FUN_0061ad60`, le plan de passage `FUN_0061afa0` /
  `FUN_0062eaa0`, l'arrivée `FUN_0062eb70`, le but lissé `FUN_005e7560`, les rappels `FUN_0062e640` / `FUN_0062df60`). Le rayon d'arrivée
  par défaut, introuvable au premier relevé, est 1 m (`FUN_0048c430`, la remise du composant de visée, 0x48c5c9). Le joueur finit sa
  poursuite d'introduction, le script lui rend la main ; Davis file, et sans poursuite `1_03` échoue à 28,81 s (`1_03_D11`).
- **`tests/test_surface`** ne plante plus : `CheminHote` (tests/hote) préfixait `BULLY_DATA` à un chemin absolu.
- **Chronos** : `1_01` à 10,46 s et `1_02` à 121,94 s à chaque commit, déroulé de `1_02` inchangé sauf le bully (vivant jusqu'à
  97,61 s, dans `AMBIENT/Idle` après l'humiliation) et le piéton 2 (qui suit le trajet 21 jusqu'à sa suppression).
- **Ce qui bloque encore** : le point valable de `CombatLoco` (`FUN_006254c0` : la liste des piétons proches +0x3c0 du composant de
  visée, le maillage de navigation) ; le choix d'une cible (`FUN_004992a0`, appelé à chaque image par `FUN_00499340`, et sa validité
  `FUN_00497080`) ; les sphères des os (`FUN_005f8e20` teste six sphères du corps de la cible, qu'il faudrait tirer du squelette animé) ;
  les `SpeechTrack` sans tenue (`FUN_006195a0` → `FUN_005a4980` : une parole par piéton avec sa priorité, que la parole unique de
  l'outil ne sait pas tenir) ; la hauteur relative (`FUN_00603170`, les boîtes de collision des modèles) ; dans `1_03`, la poursuite.
- **Bilan** : 710 fonctions recréées (5,0 % du code), contre 667 (4,3 %) ; 1 499 comprises (9,0 %) ; 10 commits. La mesure compte les
  fonctions citées dans `src/` : une part du saut vient de fonctions seulement citées en commentaire (la méthode du coup `FUN_0047b810`,
  `CPed` `FUN_004812d0`), pas recréées.

## 02.11.2026

- **La poursuite de `1_03`** : `--suivre PIETON[,d]@s` joue la manette du joueur, au sprint sur la trace du piéton poursuivi, sauf
  quand le script a repris le contrôle. `1_03` n'échoue plus à 28,81 s : Davis finit `_1_03_SCHOOLFLEE` (18,51 s), `ChaseTwoSetup`
  le lance sur `_1_03_FLEETOFOUNTAIN` et crée six threads. `_1_03_DAVISREGISTERHIT` est une dalle d'un mètre de haut que la trace de
  Davis frôle 0,5 m plus bas : l'origine d'un piéton est bien à ses pieds (`CTempColModels::Initialise`, `FUN_005712f0` : trois
  sphères de 0,35 m à 0,35, 0,95 et 1,55 m, boîte de z = 0 à 1,9), c'est le sol du monde qui manque ; l'outil y entre par `--entrer`.
- **`PedSetPosPoint`, `PedFaceObject`, `PedFaceObjectNow`** : la pose par `FUN_005c74a0` (le sol sous -20 m, la distance du centre à
  la base du modèle, `FUN_00465fc0`, nulle pour un piéton), le cap par `CPed::SetHeading` (`FUN_004755e0` : +0x1700, +0x10 et la
  direction de l'intention), le virage immédiat vers une entité (`FUN_005c73c0`, `FUN_00470b40`).
- **Le coup reçu prévient les scripts** : `RegisterPedEventHandler` (0x5bbdf0, lu à l'assembleur) et sa table de paires (piéton,
  rappel) (`FUN_005b4600`, `FUN_005b4030`, `FUN_005b42d0`, `FUN_005b40b0`) ; la méthode du coup appelle le rappel de l'événement 0
  (`FUN_005b2190`, 0x47c5c9) après ses sorties anticipées (drapeau 12, invulnérable, attitude 4). `--viser PIETON@s` (le bouton de
  verrouillage) : dans `1_03`, le joueur frappe le bully 5 à 38,15 s, `cbDavisGroupHit` passe, `DavisBreakForGarage` lance Davis et
  le bully 5 attaque le joueur.
- **`PedClearObjectives`, `PedAttackPlayer`, `PedMakeTargetable`** réelles : Davis s'arrête à la pause (sa piste coupée avec sa
  file), file au garage par `_1_03_DAVISRUN` et y attend, tourné vers le joueur (50,56 s).
- **Les actions des nœuds de trajet** (`FUN_0062e780`) : virage (type 0) ou arrêt (type 1), leur attente, le rappel +0x34 de
  l'objectif (le huitième argument de `PedFollowPath`, lu en 0x62e076) ; les 62 actions de `Trigger.img` sont de type 0, aucune
  dans `1_01` à `1_03` (test unitaire sur `hallspatrol_1A`).
- **`PedOverrideStat`, `PedSetFlag`, `PedGetFlag`** réelles : 0x89 drapeaux par piéton, le 12 interdit aux scripts.
- **Les attitudes** : `CAttitudeSet`, `CVariableAttitudeSet` (une attitude jamais posée se lit 4), le gestionnaire (`FUN_0046f1f0`),
  l'ensemble propre du piéton (+0x2e8), les cinq fonctions Lua, `SetFactionRespect` et `GetFactionRespect`.
- **Le choix d'une cible par l'IA** (`FUN_004992a0`, à chaque image comme `FUN_00499340`) : la cible de l'objectif (`FUN_004988b0`,
  `FUN_00496b60`), la cible déjà prise, l'agresseur récent (`FUN_00498930`, `FUN_00496e60`), la validité (`FUN_00497080` : style,
  portée de 30 m, vue, 6 s sans la voir) ; `AcquireTarget` ne suppose plus la cible de l'objectif.
- **Chronos** : `1_01` à 10,46 s et `1_02` à 121,94 s à chaque commit, journaux identiques (hors le piéton 2 de `1_02`, posé à son
  point par `PedSetPosPoint`, qui atteint un nœud de plus, et les lignes de changement de cible). `1_03`, avec
  `--suivre 6,3@3.4 --entrer _1_03_DAVISREGISTERHIT@29 --suivre 5,0.8@35 --viser 5@35 --jouer …/LightAttacks@38 --suivre 6,1@38.5`,
  va jusqu'au garage (50,56 s) ; le bully 5 y tue le joueur à 62,67 s.
- **Ce qui bloque encore** : le bully 5 attaque par le geste réduit de l'outil (10 points toutes les 0,8 s) faute d'arbre de l'IA :
  le modèle 146 a `/Global/AI` (`Act/AI/AI.act`) dans les `.idb`, que le constructeur de `CPed` doit poser, l'outil ne le fait pas ;
  les vagues de `1_03` (`_1_03_FIGHT3`, `FIGHT4`, `FIGHT5`, `_1_03_LADDER_DROP`) demandent ces combats. Le sol du monde (les
  déclencheurs plus hauts que les nœuds). Dans le choix d'une cible : la riposte (`FUN_00496ee0`), les piétons proches
  (`FUN_00498fc0`, la liste +0x3c0), la conversation, les zones interdites (`FUN_0046ee60`). Restent aussi le point valable de
  `CombatLoco` (`FUN_006254c0` : le maillage de navigation), les sphères des os et les `SpeechTrack` sans tenue.
- **Bilan** : 750 fonctions recréées (5,2 % du code), contre 710 (5,0 %) ; 1 542 comprises (9,1 %) ; 10 commits.

## 03.11.2026

- **Les arbres du modèle posés à la création** : l'initialisation du composant d'actions (`FUN_00484d10`, appelée par
  `CPed::SetModelIndex` `FUN_004788d0` depuis le constructeur `FUN_0047ffe0`) prend l'arbre de l'IA et l'arbre d'actions dans
  les noms que `PedSetAITree` / `PedSetActionTree` ont gardés, sinon dans le modelinfo (+0x6c / +0x70 et +0x64 / +0x68, rangés par
  `FUN_00522680` et `FUN_00522580` depuis `LoadPeds`) ; faute de nœud joué, chaque contrôleur redescend depuis sa racine
  (`FUN_00484040`, `FUN_00483ec0`). Tous les piétons des scripts ont `/Global/AI` ; Peter finit `FistShakeEnd` dans `1_03`.
- **26 classes de conditions de `AI.act`**, retrouvées par leur hachage dans le RTTI et lues dans l'index et à l'assembleur : les
  véhicules (`OnBike`, `InCar`, `TargetVehicleCar`, `TargetVehicleBike`), le lieu et le rôle (`IsHangingOut`, `HavePOI`,
  `HaveDynamicPOI`, `IsScriptedAmbient`, `IsScenarioPed`, `NearbyDockLocation`, `IsGuarding`, `IsTethered`, `IsPatrolPed`,
  `NeedsToDive`), l'état global (`VisibleArea`, `IsNISorCutsceneRunning` avec `EnterNIS` / `ExitNIS` réelles, les deux modes
  rumble et `PedSetGlobalAttitude_Rumble`, `PlayerIsInStealthProp`, `TimeCond` dans un arbre), les dernières (`DetectProjectile`,
  `DetectGrenade`, `LiquidLevel`, les alliés, `IsTandemHelper`, `Stimulus`, `IsStimulusPriority`, `WeaponEquipedType`,
  `PedIsTrigger`, `CheckActionTimer` et la piste `SetActionTimer`, `IsSpeechBusy`, `SocialAnimIs`, `IsWantingToSocialize`).
- **Les droits d'attaque** (`TargetAttackRightsAcquire` / `Release`, `FUN_00486c40`, `FUN_004861c0`, `FUN_00486c00`, l'échéance de
  15 s dans `FUN_00487780`) et `IsTargetAttackRightsTaken` ; une piste à l'octet +8 qui meurt à son lancement ne retient plus son
  nœud pour toujours.
- **`stat` et son tirage** (0x5ffff0, lu à l'assembleur) : la colonne de pedstats.dat devient une chance en pour cent. Le bully de
  `1_02` pare désormais selon sa colonne : la commande passe à dix-huit jabs (72 à 89 s) et l'humiliation à 90 s.
- **Les tirages des arbres** : le tirage +0x24 et les quatre dés +0x28 … +0x34 de chaque contexte (`FUN_00484ab0`), `WeightedRandom`
  qui ne renouvelle son tirage que quand elle passe, `Random` sur ses dés ou le tirage de l'image du piéton (+0x1748, refait par
  `FUN_004812d0` en 0x4818f8), la piste `RandomDiceRoll`.
- **Les jetons de combat** (`FightToken`, `FUN_00601f70`, `FUN_00486760`, la distribution `FUN_00486e40` lue en entier : quatre
  catégories et la place du cône, l'échéance de 10 s) et **les piétons proches** (`CPed::BuildPedLists`, `FUN_0047d1d0` : 30 m à
  plat, 16 au plus) : dans `1_03`, le bully 1 (62,20 s) puis le bully 5 (66,83 s) prennent les droits et frappent le joueur par
  `/Global/B_Striker_A/Offense`.
- **`LANCER_JUGES=PIETON@DEBUT-FIN`** dans `lancer_lua` : chaque jugement d'un piéton, la classe hachée, ses mots, la réponse.
- **REA amont** : 412 commits en attente (375 au 02.11), le dernier du 04.11. Touchent ce que bully-re utilise : le mode « projet
  Ghidra existant » (`REA_GHIDRA_PROJECT_PATH`, #7922, #8222), qui répond aux 6 min 30 d'analyse refaites à chaque ouverture (point 3
  de docs/suite.md) ; le décompilé par lots rendu en résumé gardé (#8631, #8629) ; la comparaison des preuves de fonctions Ghidra
  (#8672), la poignée de main refusée avant la première requête (#8630), les symboles de relocation (#8531), les adresses
  d'instructions non listées (#7823, rupture) ; côté PE, les noms des certificats (#8494, #8011, #7817), les entrées non MZ refusées
  (#7709), l'imphash borné (#7830). Rien fusionné. La séance n'a pas ouvert REA : l'index `export/index.sqlite` et le désassemblage
  `objdump` de `bully.exe` ont suffi.
- **Chronos** : `1_01` à 10,46 s à chaque commit, journaux identiques hors le bilan des conditions non lues (et le bully 5 qui
  attaque le 4 par l'arbre au lieu du geste). `1_02` à 121,94 s jusqu'au tirage de `stat`, puis 124,97 s avec la nouvelle commande
  (docs/missions.md). `1_03`, avec la commande du 02.11 : le bully 5 ne tue plus le joueur à 62,67 s par le geste réduit de l'outil ;
  il l'attaque par son arbre, le joueur de l'outil ne riposte pas et meurt par la piste `Death` à 101,31 s.
- **Ce qui bloque encore** : la riposte du joueur dans `1_03` (la commande ne le fait pas se battre : `T_RunFights` attend toujours
  les vagues `_1_03_FIGHT3` à `FIGHT5`) ; les classes suivantes de `AI.act` (`IsSocializingWithPlayer`, `TargetedByPlayer`,
  `TargetFriendsAttacker`, `AcquirePunishableTarget`, `FactionScan`, `HasRangedWeapon`, `InTriggerVolume`, `TargetProp`, `HitByPed`,
  `HasNodeCooldown`, `PedSphereHit`, `HitFlagClear`, `#53408a0c`, `AcquirePOI`) ; les arbres du joueur (`/Global/Player`,
  `/Global/PlayerAI`), que l'outil ne lui pose pas ; les pistes `Enable` / `DisableFightTokens` ; puis ce qui restait (la riposte de
  l'IA `FUN_00496ee0`, le sol du monde, le point valable, les sphères des os, les `SpeechTrack` sans tenue).
- **Bilan** : 781 fonctions recréées (5,3 % du code), contre 750 (5,2 %) ; 1 642 comprises (9,7 %) ; 10 commits.

## 04.11.2026

- **Les événements des déclencheurs** : `RegisterTriggerEventHandler` (0x5bbfa0, lu à l'assembleur) et
  `SetNumberOfHandledTriggerEventObjects` (`FUN_005bbeb0` → `FUN_005b5e50`) remplissent deux tables de 0x1c octets (le
  déclencheur, six cases de rappel ; la table 0 en `DAT_00cfb068`, la table 1 en `DAT_00cfb18c`), vidées à la fin de leur script
  (`FUN_005b5ae0`) ; `AreaSetTriggerMonitoringRules` (0x5b7f60) pose le bit « joueur seul » (+0x3f & 4). `FUN_006d6f60`, appelée par
  la mise à jour de CPed pour chaque piéton (0x481c45), suit les paires (déclencheur, piéton) et appelle l'entrée (0 pour un piéton,
  1 pour le joueur), la sortie (3, 4) et l'événement 5 (`FUN_005b26b0`, `FUN_005b2770`, `FUN_005b2140`, `FUN_005b1ec0`). Dans `1_03`,
  Davis qui sort d'une porte du garage la fait fermer (`DavisExitDoorTrigger`, LibTriggerNew).
- **La piste `Script`** (ScriptTrack 0x935ad4, lancement `FUN_006851a0`) : la fonction d'un espace de noms (« mission »), sans
  argument ou avec la poignée du piéton et un nombre. Elle ouvre les portes du garage (`cbDoorOpened`, depuis
  `ParametricDoor/POpenUp/PedPropActions/…/LiftOpen/success`) et fait tomber Davis de ses tonneaux (`cbBobbleDone`, `cbFallDone`).
  `--jouer` prend aussi un segment haché (« #3be30de2 »).
- **Les arbres du joueur** (`/Global/Player`, `/Global/PlayerAI`, posés au démarrage par `FUN_00484d10` comme pour les autres) :
  un nœud que le joueur finit rend la main à son arbre (`Victory` de `PlayerPickupSlingshot`, dont `CutsceneDavisBegs` attendait la fin).
- **`PlayerSetControl`** (0x5d12f0) : la manette coupée (les gestes de l'outil sont perdus), l'octet +0x1e90 (une autorité ne prend
  pas pour cible le joueur que le script tient, 0x4971c5), les entrées vidées, le drapeau 23 remis à 0 au retour.
- **La laisse** (`PedSetTetherToPoint` 0x5cc370, `PedSetTetherToXYZ` `FUN_00499910` sur +0x358, `PedClearTether`) et `IsTethered` ;
  **les stimuli ignorés** (`PedIgnoreStimuli` 0x5ca370, `SetAmbientPedsIgnoreStimuli` 0x5bb2b0 : le mot +0x1370) ; **l'état des
  portes** nommées (l'objet 0xc3b808, 400 paires : verrouillée, verrouillée aux piétons, ouverte ; `AreaSetDoorLocked`,
  `AreaSetDoorLockedToPeds`, `AreaSetDoorOpen`, `AreaIsDoorLockedToPeds`, `AreaIsDoorOpen`).
- **Deux classes de `AI.act`** : `IsSocializingWithPlayer` (`FUN_00603a70`) et `TargetedByPlayer` (`FUN_00601be0`), les deux plus
  jugées du bilan ; entre `#51223f18`.
- **`1_03` réussie** (docs/missions.md, « La riposte ») : l'outil fait riposter le joueur (`--suivre`, `--viser`, des
  `LightAttacks`), ouvrir les deux portes du garage (`--aller` au corona et le nœud `success`), battre les trois vagues et frapper
  trois fois Davis ; `CutsceneDavisBegs` puis `MissionSucceed` à 148,86 s. Réserve : Davis est touché par des jabs portés sans
  distance, là où le jeu fait lancer des briques (l'outil n'a ni armes ni projectiles).
- **REA amont** : 417 commits en attente (412 au 03.11), le dernier du 04.11. Touchent ce que bully-re utilise : les références
  croisées vides de Ghidra marquées non résolues (#8831), la recherche d'octets des littéraux encodés (#8817), l'identité d'un
  exécutable inconnu gardée par objdump (#8824, rupture) ; le mode « projet Ghidra existant » (#7922, #8222) toujours en attente.
  Rien fusionné. La séance n'a pas ouvert REA : l'index `export/index.sqlite` et le désassemblage `objdump` de `bully.exe` (déjà
  dans le dossier de travail de la session, pas regénéré) ont suffi.
- **Chronos** : `1_01` à 10,46 s et `1_02` à 124,97 s à chaque commit (journaux identiques hors les événements du déclencheur
  PSecretary, les arbres du joueur, les cibles lâchées par les autorités pendant que la mission tient le joueur, et le bilan).
  `1_03` : avec la commande du 03.11, le joueur meurt toujours à 101,31 s ; avec la riposte, réussie à 148,90 s dès les arbres du
  joueur (83d005d), inchangée ensuite.
- **Ce qui bloque encore** : `1_04` n'est pas lancée ; les armes et les projectiles ; les objets animés (`PAnimCreate`) ; les classes
  suivantes de `AI.act` (`#51223f18`, `FactionScan`, `TargetFriendsAttacker`, `SpecialPoints`…) ; le composant de visée du joueur.
- **Bilan** : 817 fonctions recréées (5,4 % du code), contre 781 (5,3 %) ; 1 691 comprises (9,9 %) ; 10 commits.

## 05.11.2026

- **`1_04` lancée seule** : elle s'arrêtait dans la cinématique du parking, où `PedGetOffsetInWorldCoords` (0x5ce500), un bouchon,
  rendait un seul 0 ; réelle, avec `PedFaceHeading` (0x5c76b0) et `PedSetStationary` (0x5c7950), `1_04` attend les vitres du bus.
- **Les armes** : le type d'une arme est une fiche ObjectButes (Weapons/Info/<nom> de `Global_Weapons.cat`, que Globals.cat monte ;
  `FUN_0050eaa0`, `FUN_0072ad50`, `WeaponGetType` 0x51b300 ; le modèle des munitions +0x84 tiré de +0x54 par `FUN_00728150`) ;
  l'inventaire est un pool commun de 57 entrées (`ms_pPool654`, `src/peds/Armes`, `tests/test_armes`) ; marble, supermarble et RubBand ont
  des munitions sans fin. L'arme en main passe par l'arbre : `PedSetWeapon` pose l'arme voulue, `Weapons/SelectActions` la prend
  (`PropGetDesired`, `PropAttach`, qui fait de l'arbre de l'arme, `/Global/SlingShot`, le contexte du piéton) ; onze conditions d'armes
  jugées, le HUD des armes (`--arme`).
- **Les projectiles** : `WeaponFire` (`FUN_00661f10`, `FUN_00664710`) tire la munition de l'objet arme (+0x12c de `SetWeaponInfo`) ;
  la vitesse balistique de `FUN_0065f050` (la fiche ProjectileButes : la bille 0,65 m par pas, 9 points ; la brique 0,38, 30 points) ;
  un point qui vole (l'outil ne joue pas la physique des `CProjectile`), qui touche les trois sphères des piétons (la méthode du coup,
  `/Global/HitTree`, l'événement 0) et les objets ; les marques et `ObjectNumProjectileImpacts`.
- **Les vitres du bus** : les entrées spec des `.ipb` (128 octets, le nom haché de l'instance en +116), des CObject quand leur modèle a
  une ligne de object.dat (supposé) ; `ObjectNameToHashID` rend un pointeur léger, `RegisterHashEventHandler` (0x5bbd80) ; détruit,
  l'objet prévient son haché (l'événement 3, `FUN_005b3530`) ; `PAnimDestroyedByPed`, `ObjectToggleVulnerability` ; `--viser-objet`.
- **L'arbre et les jocks** : les 119 arbres des objets animés sont dans Stream/World.img (`pxTree.cat` : `/Global/TreeClimb`) ;
  `RegisterGlobalEventHandler` (0x5bbcf0, deux tables ; l'événement 7 à chaque coup reçu, l'événement 1 à chaque objet détruit) ;
  `PedGetLastHitWeapon` ; `--tirer` (le haut du corps). Depuis l'arbre, dix-huit billes sur les six jocks pendant leurs sauts :
  **`1_04` se termine réussie à 81,05 s**.
- **Les briques de `1_03`** : `GetPointListSize` (0x5bc740), qui rendait 0, et les ramassages (`PickupCreateXYZ` 0x5d0b90, 300 entrées,
  `--ramasser`) ; `PropSetInTransition` met le lance-pierre en main dans `PlayerPickupSlingshot`. Trois briques lancées : Davis touché de
  loin, **`1_03` réussie à 148,04 s** (avec les jabs, toujours 148,90 s).
- **`1_02` par son vrai chemin** : `PedGetWhoHitMeLast` (0x5c8c60) réelle ; le bouchon rendait le joueur, et `T_MonitorBullies` lançait
  la cinématique des bullies sur un coup qui n'avait pas eu lieu. La commande entre dans `_1_02_RUSSELTEASE` (22,3 s), le combat tire
  ses parades autrement, vingt-cinq jabs et l'humiliation à 98 s : **`1_02` réussie à 132,92 s**.
- **`PedCanSeeObject`** (0x5cafe0, le cache de la vue) et **la minuterie de mission** (`MissionTimerStart`, `Stop`, `HasFinished`,
  `GetTimeRemaining`).
- **Le bilan nomme trois classes** par leur chaîne (le HashString de chaque chaîne de bully.exe) : `#51223f18` est `CheckWeather`,
  `#53408a0c` `PedVelocity`, `#74b4308b` `WeaponFlags` ; `IsPlayerFatigued`, `WeaponEquipedSocket` et `TargetProjectile` entrent au
  bilan avec l'arbre du lance-pierre.
- **REA amont** : 508 commits en attente (417 au 04.11), le dernier du 05.11. Touchent ce que bully-re utilise : la recherche de Ghidra
  dans les blocs de mémoire voisins (#8840, rupture), le snapshot qui garde les pseudo-codes d'une requête (#8089), les projets Ghidra
  existants (#8222, #8273, #8605) ; rien fusionné. La séance n'a pas ouvert REA : l'index `export/index.sqlite` et le désassemblage
  `objdump` de `bully.exe` (déjà dans le dossier de travail de la session, pas regénéré) ont suffi.
- **Chronos** : `1_01` à 10,46 s à chaque commit (journal identique hors les lignes de chargement) ; `1_02` à 124,97 s jusqu'au
  septième commit, puis 132,92 s par son vrai chemin ; `1_03` avec la riposte à 148,90 s à chaque commit (journaux identiques hors les
  briques et la prise du lance-pierre), aux briques à 148,04 s ; `1_04` réussie à 81,05 s dès le sixième commit.
- **Ce qui bloque encore** : `1_05` n'est pas lancée ; la charge et la visée des tirs (les jocks qui courent ne sont pas touchés), le
  haut du corps en contrôleur à part, la physique des projectiles ; les CObject et les objets animés pour de vrai ; les ramassages par
  la banque `Weapons/PickUpActions`.
- **Bilan** : 854 fonctions recréées (5,6 % du code), contre 817 (5,4 %) ; 1 785 comprises (10,4 %) ; 10 commits.

## 06.11.2026, séance 1 (nuit du 05 au 06, de 22 h 12 à 2 h 05)

- **`1_05` lancée seule** : elle bouclait dans `F_SetupWorld` sur `AreaDisablePatrolPath(PATH._SGD_PREFECT2)`, un bouchon qui ne
  rendait rien. Les rondes des préfets sont recréées (`src/peds/Patrouilles`, `tests/test_patrouilles`) : le gestionnaire
  `DAT_00c2b034` et ses entrées, que `DATLoad` crée pour chaque trajet dont `PATROLTYPE` n'est pas -1 (`FUN_004720e0`), arrêtées à la
  naissance ; `AreaDisablePatrolPath`, `AreaEnablePatrolPath`, `AreaInitPatrolPath`, `AreaEnableAllPatrolPaths`,
  `AreaDisableAllPatrolPaths`, `AreaClearPatrolPaths`, `DATUnload`. Le préfet posé sur une ronde (la population) n'est pas suivi.
- **`PedIsHit`** (0x5c8a40 : le temps depuis le dernier coup du relevé, au plus le troisième argument en millisecondes).
- **Le groupe des alliés** (`src/peds/ScriptPedPool`) : le meneur +0x2f4 et le suiveur +0x2f8 de CPed, `FUN_00485fa0` (le recrutement :
  la laisse de 4 m au premier meneur, l'attitude 4), `FUN_00485880` et `FUN_00485820` (le renvoi, récursif par l'allié lui-même) ;
  `PedRecruitAlly`, `PedDismissAlly` ; sept conditions (`IsAllyFollower` et `IsAllyLeader`, jusque-là fausses, `IsTargetAlly`,
  `HasAllyState`, `HasAllyMorale`, `AllyLeaderConditionGroup`, `AllyFollowerConditionGroup`), puis `AllyIsInCombat`. L'IA d'Algie
  entre dans `/Global/AI/Ally/allyloco`.
- **La piste `TetherLoco`** (`FUN_00627f10`, `FUN_00627fc0`) : le point de la laisse que `FUN_00499840` refait depuis le piéton tenu,
  ou la place à côté du premier meneur (`FUN_00485490` : à 1,35 m sur le côté, 1 m devant si l'allié est sorti de sa laisse), la
  vitesse de la distance au-delà de l'arrivée sur le rayon, moyennée sur trente images ; `DAT_00c1a9a4` est `CTimer::ms_fTimeStep`.
  Algie suit le joueur (`--courir x,y,z@s`, la manette vers un point fixe).
- **La première étape** : le premier bully attaque Algie, fuit sous la moitié de sa santé, meurt ; sa fuite lance les deux bullies de
  droite sur le joueur ; morts tous trois, `F_UnlockStage2` et la minuterie de 180 s (58,01 s).
- **Les alliés passent les portes** (`FUN_004162f0` → `FUN_0048b970` → `FUN_0048af70`) : Algie entre à l'école avec le joueur
  (`DT_tschool_SchoolFrontDoorL`, 67,29 s) ; `F_Stage2` démarre (`T_Stage2_Locates`, `T_BathroomAntics`). `PedSetMissionCritical`
  (0x5cca10) et le rappel du piéton critique tombé à 0 (`FUN_004782a0`) ; `PedFindInAreaXYZ` (0x5d0960) ; la hauteur de l'allié suit
  celle du point visé (l'outil n'a pas de sol) : Algie monte aux toilettes du second étage derrière le joueur.
- **L'embuscade et la cabine** : `SoundStopCurrentSpeechEvent`, `PedRestrictToTrigger`, `PedSetInfiniteSprint` (+0x13c8, le champ que
  le recrutement posait : le sprint sans fin), `PedCanTeleportOnAreaTransition` ; Algie entre dans la cabine (86,69 s), l'embuscade est
  battue (117,97 et 129,99 s), la chasse d'eau (147,01 s), le lavabo, `F_Stage3` à 154,18 s.
- **Ce qui bloque encore** : `F_NIS_AlgieLocker` attend qu'Algie joue le `CloseDoor` de l'arbre du casier (`NLockA`), par
  `Rummage/Locker` (`TargetProp`, `PropInteract`) : il faut les objets animés des déclencheurs (le `PLACEMENT` des périmètres,
  `FUN_004efe60`, `PAnimExists`…) ; le sol (la commande fait courir le joueur à 5,9 m de haut aux toilettes).
- **La machine partagée** : un autre agent tenait le verrou commun vingt à trente minutes par mesure ; la séance a compilé et vérifié
  dans un instantané des sources (`build/instantane`) pour ne pas geler l'arbre de travail, et lancé ses variantes de `1_05` à côté
  des quatre missions. Six commits de moteur au lieu de huit ou neuf : chaque commit demandait
  un passage complet sous le verrou (compilation, tests, quatre missions, variantes).
- **REA amont** : 570 commits en attente (508 au 05.11). Touchent ce que bully-re utilise : les pseudo-codes gardés en résumé et
  paginés (#5046, fb453f293), le JDK inspecté de Ghidra pour la vérification (#8440), les snapshots Ghidra historiques et leurs
  profils (#8381, #8536) ; rien fusionné. La séance n'a pas ouvert REA : l'index `export/index.sqlite` et le désassemblage `objdump`
  de `bully.exe` (déjà dans le dossier de travail de la session, pas regénéré) ont suffi.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 81,05 s à chaque commit ;
  journaux identiques hors les lignes des rondes, et dans `1_02` une ligne de plus depuis `SoundStopCurrentSpeechEvent` (la ligne 101 de
  Russell coupée à 22,87 s).
- **Bilan** : 917 fonctions recréées (5,8 % du code), contre 854 (5,6 %) ; 1 859 comprises (10,7 %) ; 7 commits.

## 06.11.2026, séance 2 (de 2 h 07 à 4 h 20)

- **Les objets animés des déclencheurs** (`src/objects/ObjetsAnimes`, `tests/test_objetsanimes`) : un déclencheur dont le périmètre a
  un `PLACEMENT` reçoit un objet de 0x3b8 octets ; l'outil suit le chemin de la scène, `FUN_00416ae0` à l'arrivée d'un changement de
  zone (`FUN_00416490` retire ceux des autres zones, `CStreaming::LoadScene` → `FUN_00416c70` → `FUN_00416540` crée ceux de la zone
  visible : pas de mission, un modèle, la zone, hors de l'emplacement 5, la saison de la fiche). Les trois autres chemins sont lus :
  la section prop des `.ipb` (`FUN_00438d90`), les déclencheurs des `.ipb` (`FUN_00436e00`), et `AreaRegisterAnimProps`, dont la table
  n'a jamais d'entrée (vérifié à l'assembleur). La fiche PropButes d'un modèle (le +0x70 du modelinfo) vient de Props/Info de
  `PropGlbl.cat`, cherchée en profondeur (`FUN_0072bd60`) : l'arbre, la saison, la portée, qui s'en sert, le genre (LOCKER, door).
  `PAnimExists` réelle : à l'école, `cbLockerCreated` voit le casier d'Algie (`NLock02B06`).
- **Le contrôleur de l'objet** : `PAnimSetActionNode` (`FUN_004ef1f0` : le même fichier que l'arbre de l'objet, le nœud joué tel quel
  par `FUN_004edec0` ; sinon une descente dans le contexte de l'objet, qui n'a pas de piéton), `PAnimSetPropFlag`, `PAnimGetPropFlag`
  (les mots +0x160 + 4n, `FUN_004ea300`), les pistes `PropFlags` et `PropSetPedAction` (ce que l'objet offre aux piétons, +0x230),
  quatre conditions des objets ; le contrôleur est figé (son nœud ne finit pas : non suivi, la règle de secours des piétons le ferait
  changer de nœud).
- **`TargetProp`** (`FUN_0065c250`) et le relevé des objets proches d'un piéton (`FUN_004991e0`, `FUN_004989f0`, `FUN_00497e90`,
  `FUN_004979b0`) : les cinq points de synchronisation (FeetSync…), le drapeau 11 qui écarte les piétons d'ambiance, NotUseable, le
  genre LOCKER ; les objets du piéton (+0x438, +0x444, +0x548). Algie vise le casier à 1,09 m.
- **Les pistes `Sync` et `PropInteract`** : Algie joue l'action du casier (`NLockA/PedPropsActions/Interact/NPCGetItem`), fouille,
  prend ses livres et referme la porte (`CloseDoor`, 166,12 s).
- **L'écran de fin** (`src/hud/EcranDeFin.h` : `MinigameSetCompletion`, `MinigameAddCompletionMsg`, `MinigameIsShowingCompletion`, la
  minuterie de 2 s que l'affichage compte à rebours) et **les objectifs de mission** (`src/hud/ObjectifsMission.h` : la liste de dix
  entrées de l'objet 0x20c4420, `MissionObjectiveAdd` rend l'indice). Le bouchon de `MinigameIsShowingCompletion` rendait faux :
  `1_04` attend maintenant l'écran avant `MissionSucceed` et finit à 82,53 s.
- **`PlayOnTarget` attend sa cible** (`FUN_006615f0` réessaie à chaque image tant que le piéton n'a pas de cible) ; faute du composant
  de visée des piétons de l'IA, l'outil prend le partenaire de prise (supposé) : dans `Gifts/GetMoney`, Algie fait jouer `Give/Give`
  au joueur, l'écran `M_PASS`, **`1_05` se termine réussie à 185,20 s**.
- **`1_06_01` lancée seule** : le nœud `Run` de la mission au départ de `main` (`FUN_006abf20` → `FUN_006ab510`) et les pistes
  `Script` à un nombre (`FUN_006851a0`) : `F_SetStage(1)` pose `bStageLoaded` ; puis `MissionObjectiveAdd` réelle : Gary et Pete
  parlent, `F_RoutingToHobo` attend le joueur dans `_FIGHTINGAREA`.
- **Les portes animées** : `PAnimOpenDoor`, `PAnimOpenDoorImmediate`, `PAnimCloseDoor`, `PAnimDoorStayOpen`, `PAnimIsOpen`
  (`FUN_004eebc0`, `FUN_004eed70` : les drapeaux 0, 9, 15, les nœuds Opening et Closing de `Door.act`) ; les portes nommées
  (`AreaSetDoorLocked` et ses voisines) passent par l'objet de leur déclencheur quand il en a un (`FUN_006d6420` … `FUN_006d64d0`).
- **Ce qui reste supposé** (docs/suite.md) : le composant de visée des piétons de l'IA, les points de synchronisation des modèles,
  l'arbre propre d'un objet animé à sa création, la vue du relevé, la tenue de l'écran de fin, le sol.
- **La machine partagée** : l'autre agent a tenu le verrou par séries (batch1 à batch8) ; la séance a gardé quatre instantanés des
  sources (`build/instantane`, `instantane2` à `instantane4`) pour vérifier deux commits à la suite sous un seul verrou et écrire le
  suivant pendant l'attente ; une leçon : ne jamais réécrire un script qu'un `sh` est en train de lire (la vérification du quatrième
  commit a rejoué ses missions deux fois).
- **REA amont** : 571 commits en attente (570 à la séance 1) ; rien de neuf qui touche bully-re (le dernier, #8979, concerne Windows) ;
  rien fusionné. La séance n'a pas ouvert REA : l'index `export/index.sqlite` et `objdump` sur `bully.exe` ont suffi.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s) à chaque commit ; `1_04` à 81,05 s
  jusqu'au quatrième commit, 82,53 s depuis l'écran de fin ; journaux identiques hors les lignes des objets animés, la ligne de l'écran
  de fin et les indices des objectifs. `1_05` jusqu'à `F_Stage3` (154,18 s), puis le casier (166,12 s), puis réussie à 185,20 s.
- **Bilan** : 1 002 fonctions recréées (6,4 % du code), contre 917 (5,8 %) ; 1 945 comprises (11,2 %) ; 10 commits.

## 06.11.2026, séance 3 (de 4 h 24 à 6 h 29)

- **Les collectibles** (`src/objects/Collectibles`, `tests/test_collectibles`) : CCollectiblesMgr est l'objet 0xbceb50, initialisé par
  CGame::Initialise (`FUN_00428bf0`) depuis config\dat\colctbls.dat (`FUN_004287a0` : sept types, 192 entrées, le HashString de
  chaque nom) ; les octets « pris » et « disponible », le rappel des scripts à six nombres (`CB_CollectibleCollected` de main.lur) ; les
  cinq fonctions Lua `Collectibles*` réelles.
- **Les objets du joueur** : 80 compteurs en DAT_00cf0060 hors de l'inventaire des armes (`FUN_00541cb0`, `FUN_00541c10`,
  `FUN_00541ae0`, le BigGulp jamais compté) ; `ItemGetCurrentNum` et `ItemSetCurrentNum` réelles.
- **Les ramassages dans src** (`src/objects/Ramassages`) : le pool est à 0xc0a100 (et non 0xc0a0f8) ; `FUN_00443af0` et les bits de
  +0x46 (le bit 2 « à la main » des ramassages de `PickupCreateXYZ`, le bit 4 des collectibles) ; le décalage +0x44 des fiches (les
  briques de `1_03` 3,5 cm plus haut).
- **Les collectibles des placements** : la seconde section inst des `.ipb` porte les élastiques, les cartes et les transistors ; leur
  +0x74 est le haché de l'instance, leur +0x44 la zone. `FUN_00438790` en fait des objets fictifs, la population (`FUN_004a0130`, 80 m)
  des ramassages (`FUN_0049f2f0`, `FUN_00445640`) ; l'outil juge les 146 entrées à chaque image (sans diffusion des placements, supposé).
- **La prise au contact** (`FUN_00445970`, `FUN_00445710` : 1 m au plan, 2 m en hauteur ; `FUN_00444ac0` pour les objets) : le
  transistor est pris à 26,86 s, `F_AchieveReceiver` passe ; le don au clochard (`GiveHobo_1_06`), puis `ObjectRemovePickupsInTrigger`
  réelle (`FUN_004448c0`).
- **L'historique des boutons** (`src/hud/ButtonHistory`) : HUDButtonHistory (DAT_00c674f4), ses douze fonctions Lua, la mise à jour
  `FUN_0053c250` et `FUN_0053b370` dans le mode de l'arbre ; la piste `ActionRegisterButtonHistory` (`FUN_0061bab0` → `FUN_0053b340`)
  rapporte les boutons des nœuds du joueur. Trois coups chargés : **`1_06_01` se termine réussie à 61,81 s**. Dans `1_02`, les nœuds
  joués rapportent aussi les leurs ; deux séquences de plus réussissent par eux, sans rien changer : 132,92 s.
- **Un collectible changé en ramassage n'est plus reposé** (l'objet fictif est détruit par `FUN_0049f2f0`).
- **`1_07` lancée seule** : elle bouclait sur `PAnimRequest` (0x5c4590, réelle : vrai, faute de diffusion) ; puis `PAnimCreate` et
  `CreatePersistentEntity` (des bouchons), et Bucky perd 25 points toutes les cinq secondes faute de défense : échec à 64,55 s.
- **La machine partagée** : l'autre agent a tenu le verrou par lots (batch12 à batch16, vingt à trente minutes chacun) ; la séance a
  vérifié deux commits par passage du verrou dans des instantanés (`build/c1` à `build/c9`) découpés d'un même état courant
  (`decouper.py`). Une vérification a été arrêtée en cours (un instantané fautif : `Ramasser` ne compilait pas, puis un test aux bornes
  trop serrées) et relancée ; arrêter un `deux2.sh` ne suffit pas, ses enfants gardent le verrou : il faut les arrêter aussi.
- **REA amont** : 581 commits en attente (571 à la séance 2) ; le plus proche de bully-re est #9046 (la découverte des outils M2 024 en
  deux temps, à activer) ; rien qui touche l'index ou Ghidra ; rien fusionné. La séance n'a pas ouvert REA : l'index
  `export/index.sqlite` et le désassemblage `objdump` de `bully.exe` (déjà dans le dossier de la session) ont suffi.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 82,53 s et `1_05` à 185,20 s à
  chaque commit ; journaux identiques hors les lignes de chargement, la hauteur des briques ramassées (6,20 au lieu de 6,17), les
  collectibles posés (de six à dix-sept lignes par mission), les poignées des ramassages créés après eux, et dans `1_02` les boutons
  rapportés à l'historique. `1_06_01` : le transistor à 26,86 s, la leçon à 44,85 s, réussie à 61,81 s.
- **Bilan** : 1 061 fonctions recréées (6,9 % du code), contre 1 002 (6,4 %) ; 2 028 comprises (11,6 %) ; 10 commits.

## 06.11.2026, séance 4 (de 6 h 30 à 7 h 51)

- **`PAnimCreate`** (0x5c4680, hors de l'index, lue à l'assembleur) : l'objet animé d'un déclencheur, même propre à une mission
  (`CObjetsAnimes::CreerParScript`) ; dans `1_07`, la caisse à outils, la grille et les portes du garage ; dans `1_01`, la grille de
  l'école s'ouvre ; dans `1_03`, les portes du garage et le tas de tonneaux de Davis jouent leurs arbres.
- **`PedFindInAreaObject`** (0x5d0a30) : la recherche de `PedFindInAreaXYZ` autour d'un piéton ; `T_ObjectiveMonitor` de `1_07` cesse
  de la redemander à chaque image.
- **`CreatePersistentEntity` et `DeletePersistentEntity`** (0x5bd090, 0x5bca50, lues dans l'index) : les modèles panm deviennent des
  objets animés sans déclencheur, les autres des CObject (rangés à part des objets cassables de l'outil) ; main.lua en pose trente et
  une dès 1,12 s ; la poignée et le genre rendus (0 pour un CObject, 5 pour un objet animé).
- **Les marqueurs du radar** (`src/hud/Marqueurs`) : les 300 traces de CRadar (DAT_00c674cc + 0x210), leurs poignées (la génération <<
  12 | l'indice) ; `AddBlipForChar`, `BlipAddPoint`, `BlipAddXYZ` et `BlipRemove` réelles, les marqueurs retirés avec la mission.
- **`1_07` jusqu'à la porte du garage** : le joueur rejoint Bucky (39,07 s), bat les quatre bullies (53,00 à 86,00 s) et ouvre la
  porte (`cbDoorOpened`, 94,02 s) ; la dernière étape de `F_MissionSpawnerFinished` reste à trouver.
- **La machine partagée** : le verrou a été pris par lots (batch16 et suivants) ; deux vérifications ont échoué à la compilation (un
  nom en double dans un test, les marqueurs déclarés après leur usage) et ont été relancées ; un essai de `1_07` a tourné entre deux
  vérifications.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 82,53 s, `1_05` à 185,20 s et
  `1_06_01` à 61,81 s à chaque commit ; journaux identiques hors les objets animés créés par `PAnimCreate` (trois lignes dans `1_01`,
  vingt-sept dans `1_03`), les entités persistantes (une trentaine de lignes par mission) et les arguments de `BlipRemove`.
- **Bilan** : 1 077 fonctions recréées (7,0 % du code), contre 1 061 (6,9 %) ; 2 041 comprises (11,7 %) ; 6 commits (cinq de moteur et la
  documentation : la séance s'arrête là, le contexte de l'agent étant chargé).

## 06.11.2026, séance 5 (de 19 h 00 à 21 h 05)

- **`PropTargetInteractive`** (`FUN_00655110`, lue à l'assembleur ; le créateur 0x645470, `FUN_00654e30`) : le relevé des objets proches
  et le choix de `TargetProp` mis en commun (`ReleverObjetsProches`, `ViserObjetChoisi`) ; le juge rendait toujours faux. Dans `1_07`,
  Bucky vise la caisse à outils, joue `StartUsing` et prend le circuit : `F_DoBuckyGrabbingNIS` se termine, l'objectif 3 (103,22 s).
- **`PickupCreateFromPed`** (0x5d0cc0 ; `FUN_00443d90`, `CRamassages::CreerDuPieton`) : la santé que laisse le second piéton de la
  seconde vague, 0,45 m au-dessus de lui, prise au contact.
- **`PedHasAlly`** (0x5cd730) et **`PedIsModel`** (0x5c92a0) : Bucky est renvoyé quand il part se cacher ; avec la commande étendue
  (le joueur entre dans `_1_07_FIGHTTRIGGER`, bat la seconde vague et va à `_1_07_PLYRDOOR`), **`1_07` se termine réussie à 151,47 s**.
  Ce que la séance du matin avait lu se vérifie dans `1_07.lur` (l'étape r53, le drapeau r54, la vague du 145 et du 102, l'étape 4 et les
  0,8 m de `_1_07_PLYRDOOR`), à deux corrections près : ce qui suit l'étape 4 n'est pas une troisième vague mais `F_CompleteMission`
  (le clochard et ses trajets, `PedFollowPath` sans rappel), et `PAnimFollowPath` n'est pas dans `1_07` (seulement dans `1_01` et
  `1_02`, docs/suite.md).
- **`PlayerUnequip`** (`FUN_0055c780`), **`PlayerSetPosSimple`** (0x5d2bd0), **`PlayerFaceHeadingNow`** (0x5c7360) : le joueur de
  `F_CompleteMission`.
- **`PAnimDelete`** (0x5c4930) : la tourelle de l'école, la grille de `1_01`, les portes de `1_03`, la grille de `1_07` ; la zone
  visible recrée les objets qui ne sont pas propres à une mission, et les indices des objets créés ensuite changent dans les journaux.
- **`FUN_00477ab0`** (`CScriptPedPool::CibleDeGenre`) : l'entité visée selon un genre, pour `FaceTarget` et `SeekEntityPosition` ; Bucky
  s'approche de la caisse à 1 m.
- **La barre de santé secondaire** (`src/hud/BarreSante.h`, le composant 12 du HUD) : `PedShowHealthBar`, `PedHideHealthBar`,
  `FUN_00553690`, `FUN_00551380`, `FUN_005513b0`.
- **`PedMakeMissionChar`** (0x5ca890, `FUN_004770a0`) : un piéton de la population redevient de mission. **`AreaClearAllPeds`**
  (0x5b7e70) est recréée (`CScriptPedPool::AmbiantsAVider`) mais pas branchée : branchée, elle détruisait les trois joueurs de foot de
  `F_FootballFieldThread` rendus à la population, les piétons suivants prenaient leurs cases, et les commandes, qui visent les piétons par
  leur poignée, ne visaient plus les bons : `1_04` n'allait plus au bout, `1_05` échouait à 57,88 s. La vérification a été refaite sans
  elle (docs/suite.md : refaire ces commandes).
- **`PedStopSocializing`** (0x5cdbf0, `FUN_00490f50`) : l'état social, le partenaire et les bits de l'octet +0x224 remis à zéro.
- **La machine partagée** : l'autre agent a tenu le verrou (dotnet test) au premier passage ; une vérification a échoué deux fois à la
  compilation (un nom de variable en double dans `tests/test_ramassages.cpp`) et a été relancée ; deux instantanés sont vérifiés par
  passage du verrou (`build/k1` à `build/k8`). Les commits sont faits depuis les instantanés vérifiés (`git --work-tree=build/kN add`) :
  le crochet pre-commit compte l'avancement sur l'arbre de travail, un commit fait pendant que les correctifs suivants y étaient a eu un
  compte faux, refait avant d'être poussé. La vérification du huitième commit a été refaite (`AreaClearAllPeds` débranchée).
- **REA amont** : 651 commits en attente (581 à la séance 3) ; les plus proches de bully-re sont #9151 (les vues gardées des catalogues
  progressifs du serveur MCP) et #8991 (le décodeur des commandes rizin d'un coup) ; rien qui touche l'index ou Ghidra ; rien fusionné.
  La séance n'a pas ouvert REA : l'index `export/index.sqlite` et `objdump` sur `bully.exe` ont suffi.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 82,53 s, `1_05` à 185,20 s et
  `1_06_01` à 61,81 s à chaque commit ; journaux identiques hors les destructions de `PAnimDelete` et les indices des objets (de trois à
  vingt-huit lignes par mission). `1_07` : la caisse à 102,96 s au premier commit, puis réussie à 151,47 s dès le troisième.
- **Bilan** : 1 105 fonctions recréées (7,1 % du code), contre 1 077 (7,0 %) ; 2 073 comprises (11,8 %) ; 10 commits (neuf de moteur et la documentation).

## 06.11.2026, séance 6 (de 21 h 07 à 21 h 55)

- **La charge de l'arme** (`CChargeArme`, `src/peds/Armes.h`) : `WeaponSetCharge` (`FUN_0065e7c0`), `WeaponIncrementCharge`
  (`FUN_0065e810`, la moitié pour le lance-pierre, DAT_00bf444c « slingshot »), la charge de `WeaponFire` (`FUN_00661f10` : × +0x34,
  bornée à [0, 1]) et sa remise à -100 (`FUN_00664710`) ; la commande de `1_04` ne joue pas la feuille `Charge` : rien ne change.
- **Les drapeaux des armes** : `SetWeaponFlags` (`FUN_00661d40`), `WeaponFlags` (`FUN_0065bb20`), `PedSetWeaponFlag` (0x5cf090) ; huit
  mots par entrée d'inventaire et par objet arme.
- **La minuterie de mission dans src** (`src/hud/MinuterieMission.h`) : `FUN_0043f600`, `FUN_0043f540`, `FUN_0043f560`, `FUN_0043f5a0` ;
  `MissionTimerStartFixed` (0x5c1ff0 : l'heure visée de l'horloge du jeu ; quand elle est passée, le double de la visée plus 0x5180 moins
  l'heure du jeu, lu tel quel) et `MissionTimerPause` (0x5c2090) réelles.
- **`FaceTarget` des contrôleurs de l'IA** par `FUN_00477ab0` (le genre 0 reste la cible de l'IA) : les pistes du genre 4 et 0x10 de
  l'IA visent l'objet et l'allié.
- **Lu sans être recréé** (docs/suite.md) : `WeaponFireOffset` (`FUN_0065d3f0`, `FUN_00662380`, `FUN_0065d160`), l'avancée des
  PathReference (`FUN_005ed750`) et la mise à jour des entités qui suivent un trajet (`FUN_005ec0a0`), les conditions `CheckWeather`
  (`FUN_005fcfa0` : DAT_00ce756c, le temps qu'il fait, égal au mot +4) ; le juge de `PedVelocity` est repéré (`FUN_00604c50`) ; l'IA
  les juge des milliers de fois par mission : les brancher change ses choix, à mesurer seules.
- **La machine partagée** : l'autre agent a tenu le verrou au premier passage (de 21 h 14 à 21 h 16) ; deux instantanés par passage
  (`build/k10` à `build/k13`), sans échec.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 82,53 s, `1_05` à 185,20 s,
  `1_06_01` à 61,81 s et `1_07` à 151,47 s à chaque commit, journaux identiques.
- **Bilan** : 1 121 fonctions recréées (7,1 % du code), contre 1 105 (7,1 %) ; 2 086 comprises (11,8 %) ; 5 commits (quatre de moteur et la
  documentation : la séance s'arrête là, le contexte de l'agent étant chargé).

## 07.11.2026, séance 1 (de 21 h 34 à 22 h 12)

- **La branche de la veille au soir** (`seance-2026-11-06f`, six commits de 22 h 49 à 23 h 42 le 06.11, vérifiés et poussés sans
  documentation ni PR : ca12e67, fa5abcb, 228e02d, 83e971c, 4542548, f6d6093) : le pas du contrôleur d'actions (`FUN_004812d0` →
  `FUN_004850b0` : 0,02 × ms_fTimeStep × la colonne V de pedstats.dat / 100, 0,033 s par image, que reçoit `WeaponIncrementCharge`) ;
  la manette qui tient un bouton (`--tenir N,durée@s` : les octets 3, 2, 8 puis 0 de `FUN_005f3930` et `FUN_005f3720`) et la feuille
  `Charge` du lance-pierre jouée pour de vrai dans `1_04` (une charge de 0,37 : 0,87 m par pas et 28 points par bille) ; les réglages
  du tir (`WeaponFireOffset` `FUN_0065d3f0`, `WeaponFireStartOffset` `FUN_0065d430`, `WeaponFireRotation` `FUN_0065d350`, remis par
  `FUN_0065d160`) ; les dégâts du tir (`FUN_00664710` : le +0x2c de la piste et la colonne Damage_Scale du tireur) ; l'objet arme
  (le constructeur `FUN_004da610`, la remise `FUN_004d92c0`, le changement de modèle `FUN_004da6f0`) ; `AttackTimerReset`
  (`FUN_006121d0`, `FUN_0048c040`).
- **`HasRangedWeapon`** (`CInventaireArmes::ArmeADistance` et `MunitionsPour`) : la table 0x92e874 et son juge `FUN_00605a80` (lu à
  l'assembleur), `FUN_00486ae0` et `FUN_00486130` ; jugée -1 jusqu'ici (36 fois dans `1_01`, 451 dans `1_03`, 254 dans `1_05`, 49 dans
  `1_06_01`, 211 dans `1_07`), elle rend faux : aucun piéton de l'IA de ces missions n'a d'arme à distance.
- **`DetectProjectile` et `DetectGrenade`** sur les projectiles de l'outil (`CScriptPedPool::VoitPoint`, `DetecterProjectile`,
  `DetecterGrenade`, `GrenadeAPortee`) : la table 0xc3a068, `FUN_0048a300` (la trajectoire à plat à moins de 3 m), `FUN_0048a130` ; le
  juge de `DetectGrenade` (`FUN_00604380`, lu à l'assembleur) refait un test de portée, le rayon plus 1 m à plat, que le travail mis de
  côté oubliait. La bille et la brique (relues dans Global_Weapons.cat) n'ont pas l'octet +0xe4 : rien ne change.
- **Le haut du corps du joueur** en contrôleurs à part (`CActionController::arbreDesLances`, `AppliquerLances`) : les spawn de l'arbre
  de l'arme, et la référence montée descendue à l'entrée (`FUN_005f5470`). Le bouton 8 tenu seul fait partir la première bille de
  `1_04` (17,36 s, la vitre à 17,52 s) ; les suivantes non, le contrôleur d'`UpperBody` s'arrêtant après `AimOut`.
- **Le travail mis de côté** (`git stash` sur main, de la séance d'hier soir) : comparé à f6d6093, repris en trois commits vérifiés (le
  juge de `DetectGrenade` corrigé, deux commentaires précisés), puis supprimé ; les instantanés `build/k7`, `build/k8`, `build/l1` et
  `build/l2` aussi.
- **REA amont** : 714 commits en attente (653 la veille), des corrections de l'outil Ghidra et des tests (#8811, #8382, #8376…) ;
  rien qui touche bully-re, rien fusionné.
- **La machine** : seul sur le portable ; une vérification par commit (construction, tests, sept missions) en 8 minutes sous le verrou,
  dans le worktree `build/wt`.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 82,53 s, `1_05` à 185,20 s,
  `1_06_01` à 61,81 s et `1_07` à 151,47 s à chaque commit.
- **Bilan** : 1 157 fonctions recréées (7,3 % du code), contre 1 121 (7,1 %) au départ de la branche ; 2 107 comprises (11,9 %) ;
  quatre commits ce soir (trois de moteur et la documentation), dix sur la branche.

## 07.11.2026, séance 2 (de 22 h 16 à 23 h 35)

- **`IsPlayerFatigued`** (`FUN_005fc3a0`, lu à l'assembleur : l'état physique du joueur, `DAT_00c1af4c`, vaut 1) : jugée -1 depuis les
  contrôleurs du lance-pierre (257 fois dans `1_03`, 2 153 dans `1_07`), elle rend faux.
- **L'avance sur une cible qui bouge** (`CViseeTir::AvanceSurCible`, `FUN_0065ecb0` lu à l'assembleur : la durée du vol à plat, la
  vitesse de la cible et sa part le long du tir, × +0x84 de la fiche) : les cibles des sept missions sont immobiles au tir, rien ne
  change.
- **La table des projectiles** (`CPlacesProjectiles`, `src/objects/Projectiles.h` : 32 places, `FUN_004d4590`, la plus vieille cédée
  par `FUN_004d3f60`), puis **leur vie** (`FUN_004d45e0` : une vie nulle est sans fin, le tireur disparu oublié) et
  `AreaClearAllProjectiles` par modèle (`FUN_004d48a0`).
- **Les entités qui suivent un trajet** (`src/entities/SuiviTrajet.h`) : le PathReference (`CReferenceTrajet` : `FUN_005ed750`,
  `FUN_005ed620`, `FUN_005ed6f0`, `FUN_005ed400`, `FUN_005ed470`), le gestionnaire 0xd13730 (`CSuiveursTrajet` : `FUN_005eb880`,
  `FUN_005ebfd0`, `FUN_005ebd60`, `FUN_005ebee0`, `FUN_005ec3a0`), et dans l'outil `PAnimFollowPath`, `PAnimStopFollowPath`,
  `PAnimSetPathFollowSpeed`, `PAnimGetPathFollowSpeed`, `PAnimFollowPathReset` (lues à l'assembleur, hors de l'index pour trois
  d'entre elles) et le rappel des nœuds atteints.
- **La grille du magasin** : dans `1_01` (7,36 s), `1_02` (1,95 s) et `1_05` (67,35 s), la grille fermée du magasin
  (`SCStoreGte_Closed`) est posée au départ de son trajet, en (-611,65, -314,69, 1,93), à 0,79 m de sa création ; sa vitesse de -1,2
  ne la fait pas bouger (une distance nulle ne recule pas).
- **La machine** : une vérification par commit, sous le verrou, dans le worktree `build/wt` ; les commits suivants préparés en
  patchs pendant ce temps (une vérification lancée par erreur avec `&` au lieu de l'arrière-plan de l'outil, attendue jusqu'au
  bout).
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 82,53 s, `1_05` à 185,20 s,
  `1_06_01` à 61,81 s et `1_07` à 151,47 s à chaque commit.
- **Bilan** : 1 194 fonctions recréées (7,6 % du code), contre 1 157 (7,3 %) ; 2 133 comprises (12,1 %) ; dix commits (neuf de
  moteur et la documentation).

## 07.11.2026, séance 3 (de 23 h 39 à 0 h 27)

- **`AreaClearAllPeds` branchée** (`CScriptPedPool::AmbiantsAVider`, lue à l'assembleur de 0x5b7e70 à 0x5b7ecb) : toutes les missions
  l'appellent ; dans `1_04` (10,06 s), `1_05` (3,43 s), `1_06_01` (40,79 s) et `1_07` (133,95 s), elle détruit les trois joueurs de
  foot rendus à la population, et les poignées des piétons créés ensuite baissent de 3 (le script `remap.py` du scratchpad aligne les
  créations des deux journaux). Les anciennes commandes échouent (`1_04` ne va plus au bout, `1_05` s'arrête à 57,88 s) ; refaites
  (`1_04` : les jocks 5 à 10 ; `1_05` : les bullies 1, 2, 3, puis 9 et 10), elles réussissent. `1_05` finit à 194,30 s : l'objectif
  de la fin passe à 133,95 s au lieu de 154,18 s, et le joueur, que la commande fait courir à 155 s, arrive au bout plus tard.
- **`PedClearHitRecord`** (0x5cfde0, lue à l'assembleur) : le drapeau et l'attaquant du relevé du dernier coup oubliés ; six appels
  sur Bucky dans `1_07`, sans effet visible.
- **`PedSetMinEngage`** (0x5cb790 : le +0x42c du piéton) et la condition **`EngageDistance`** (`FUN_00602070` : la distance
  d'engagement par la zone de combat et AIButes, ou le +0x42c et la portée de l'arme pour un piéton armé à distance) ; aucun arbre des
  sept missions ne la cite.
- **La visée automatique du joueur** (`WeaponPlayerFireAutoTarget`, `FUN_00661ee0` : l'octet +0x1ec0, que `FUN_00662a40` lit avec le
  bouton 10 de la manette) : posée par le nœud `Controller` du lance-pierre ; sans autre cible que le verrou de `--viser`, l'outil
  tire comme avant.
- **Les décalages de visée de `WeaponFire`** (`CViseeTir::DecalageVisee`, `FUN_00662760` lu à l'assembleur) : le côté × la distance,
  l'avant × l'écart, la hauteur en z, et l'avant de la cible ; nuls pour la bille.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 82,53 s, `1_05` à 194,30 s
  (185,20 s avant `AreaClearAllPeds`), `1_06_01` à 61,81 s et `1_07` à 151,47 s.
- **Bilan** : 1 199 fonctions recréées (7,6 % du code), contre 1 194 (7,6 %) ; 2 139 comprises (12,1 %) ; six commits (cinq de moteur et la documentation). Sur la nuit : vingt commits nouveaux en trois séances
  (PR #24, #25 et celle-ci), plus les six de la veille au soir documentés.

## 09.11.2026, séance 1 (de 19 h 21 à 21 h 06)

- **Les destinations forcées du tir** (`PedSetProjDest`, 0x5cd470 → `FUN_0048bd40` ; `FUN_0048bd90`, `FUN_0048bdb0`, lus à
  l'assembleur ; `CDestinationsTir`) : trois places en +0x48, +0x54, +0x60 du composant +0x3bc, vides à x = 1e11 ; la visée les prend
  avant tout point visé, sans décalage ni avance. Seul 4_B1.lur s'en sert.
- **La source de la cible du tir** (`FUN_00662a40` lu à l'assembleur de 0x662a40 à 0x662c90, `CViseeTir::SourceCible`) : l'octet +0x31
  de la piste (l'objet visé +0x438 et son point +0x43c) ; pour le joueur, la visée automatique, le bouton 10, le mode 2 du composant de
  visée ou les caméras de visée 2 et 0xd (DAT_00c3ccf8 est le mode courant du gestionnaire de caméras) ; +0x54c pris une fois, la cible
  du composant de visée (`FUN_004d5980`), puis son repli (`FUN_004d7630` : +0xa8 gardée, +0x9c, le partenaire de prise,
  `FUN_004d6dc0`). Le genre de la cible : seule CAutomobile a une méthode +0x14 qui ne rend pas faux (`FUN_007ffc70`).
- **Le bouton 10** (`FUN_007386f0`, `FUN_0050c730`, lus à l'assembleur ; `src/core/Manette.h`) : les masques de DAT_00a14810 sont les
  bits de XInput, plus 0x400 (le bouton 10, le verrou de `FUN_004d5d50`) et 0x800 (le bouton 12).
- **Le point devant le tireur sans cible** (`FUN_0065ea50`, lu à l'assembleur) : au-delà de 0,9 m, à la hauteur de ses pieds.
- **L'imprécision** (`FUN_006626a0`, `FUN_0065eb40` lus à l'assembleur, `FUN_004d5cd0`) : d / 20 × (100 − la statistique 10) / 50,
  hors de l'écran et la précision du verrou, trois tirages de rand ; l'outil prend leur moyenne (0) : l'écart est nul.
- **La vie du projectile** (`FUN_004d4c40` de 0x4d4d05 à 0x4d4d38, `FUN_00664710`) : la vie du tireur (+0x1cc0, `PedSetProjectileLifetime`)
  ou celle de la fiche, moins la mèche de l'objet arme (+0x140), 1 pour 0.
- **La piste spawn au contrôleur vide** (`FUN_005f72e0`) : elle meurt sans l'octet +0x29 et ne retient plus son nœud. L'essai de `1_04`
  au seul bouton 8 (`e104b.sh`) montre le vrai blocage du haut du corps : `AimOut/default` est retenue par `C_PLAYER\SLNG_2IDLE_T`
  (`W_Slingshot.agr` n° 2), dont l'outil n'a pas la durée (ni MAINPED.HXD ni SLINGSH.HXD) ; seule la première vitre casse.
- **Le test d'eau des projectiles** (`FUN_006d48d0` lu à l'assembleur, `FUN_004d45e0`) : z ≤ 0 dans une zone dehors ; le fondu de la fin
  de vie (+0x88). Aucun projectile des sept missions ne descend jusque-là.
- **`PedTargetPAnim`** (0x5cfe50, lu à l'assembleur ; `FUN_004711b0`) : l'objet visé, son point haché, et le verrou sur l'objet animé
  (`CScriptPedPool::VerrouillerObjet`).
- **Les comptes des README** : le crochet de commit (`tools/avancement.py`) calcule depuis l'arbre de travail ; les quatre premiers
  commits, faits depuis des instantanés de `build/` pendant que l'arbre avait déjà les suivants, portent des comptes d'avance (1 223 et 1 228,
  au lieu de 1 203, 1 211, 1 215 et 1 215) ;
  les cinq suivants sont faits avec `--work-tree` sur l'instantané, leurs comptes sont justes. La réécriture de ces quatre commits
  (un `push --force`) a été refusée : ils restent tels quels.
- **REA amont** : 875 commits en attente (714 le 08.11), dont des corrections de l'outil Ghidra (#9611, #8651, #8608, #8777) ; rien ne
  touche bully-re, qui interroge l'index et objdump.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 82,53 s, `1_05` à 194,30 s,
  `1_06_01` à 61,81 s et `1_07` à 151,47 s, inchangés, journaux identiques à chaque commit.
- **Bilan** : 1 228 fonctions recréées (7,7 % du code), contre 1 199 (7,6 %) ; 2 163 comprises (12,3 %) ; dix commits (neuf de moteur et
  la documentation).

## 11.11.2026, séance 1 (de 21 h 41 à 23 h 00)

- **Le contrôleur d'actions qui se vide** (`FUN_005f49a0`, lue à l'assembleur de 0x5f49a0 à 0x5f4a46 ; `CActionController::Avancer`) :
  quand `FUN_005f4230` ne rend plus vrai, les pistes s'arrêtent, le temps +8 revient à 0, le nœud joué +0x10 passe à 0 et seule une
  demande déposée (+0x14) relance le contrôleur. L'outil gardait le nœud fini quand rien ne donnait de suite : un contrôleur sans arbre
  (celui d'une piste spawn) se vide maintenant. C'était le point qui bloquait le haut du corps du lance-pierre, et il est passé devant
  les durées des groupes d'armes : le diagnostic du 09.11 était faux, `C_PLAYER\SLNG_2IDLE_T` est bien dans MAINPED.HXD
  (l'enregistrement 715, 0,3333 s, groupe 39 `W_Slingshot`) et l'outil lisait déjà sa durée ; `hxds.dat` et `PropHXDs.dat`
  (`FUN_0042e640`, `FUN_006c1ab0`) ne chargent que les dictionnaires des accessoires. Essai `e104b.sh` (`1_04`, les vitres au seul
  bouton 8, sans `--jouer` du nœud `Charge`) : trois vitres sur trois, à 17,56 s, 19,70 s et 21,85 s, contre une seule avant.
- **La durée d'un nœud par ses pistes retenantes** (`FUN_005f4230`, relue dans l'index ; `CActionController::Duree`, la fin des pistes
  des nœuds exécutés de `FUN_005f3e20`) : seule une piste vivante à l'octet +8 retient le nœud ; les pistes sans l'octet ne comptent
  plus pour sa fin. La feuille `AimOut/DEFAULT` finit au champ 44 de `C_PLAYER\SLNG_2IDLE_T` (0,0333 s) au lieu de la durée de
  `C_PLAYER\BLOCK` (1,1667 s, sans l'octet). L'essai `e104b.sh` casse les trois vitres à **17,56 s, 19,54 s et 21,55 s** (les billes
  partent à 17,36 s, 19,34 s et 21,35 s), `AimOut` commence à 17,79 s, l'attente #0bb98aa5 revient à 17,89 s, et la mission finit
  réussie à 82,53 s. Les nœuds de coups reçus finissent avec leur animation retenante et non avec leur piste la plus longue : `1_05`
  passe de 194,30 s à 180,31 s (la première piste `Death` à 26,63 s au lieu de 27,32 s) et `1_07` de 151,47 s à 150,88 s (52,30 s au
  lieu de 53,00 s).
- **Le choc d'un projectile** (`FUN_004d3860`, lue à l'assembleur de 0x4d3860 à 0x4d3c14 ; `FUN_004d37d0`, lue dans l'index ;
  `CChocProjectile`) : l'explosion par les octets +0x8c, +0x8d et +0x8e de la fiche (partout ; au sol, sur une normale dont le z n'est
  pas sous 0,5 et hors d'un piéton ; sur un piéton ou un véhicule occupé), du contact relevé de 0,05 ; le stimulus 2 de l'objet touché
  ; le compteur d'impacts (les trois bits bas de +0x23c, sous 5) et l'effet +0xc0 de la fiche pour un choc fort ; le premier sol
  (l'octet +0x18 de l'entrée) ; l'explosion une seule fois par place (l'octet +0x16 de l'entrée, 0xc3a04e), puis le stimulus +0x44 du
  tireur avec l'octet +0x40.
- **Le choc dans l'outil** (`ChoquerProjectile`) : la fiche d'un projectile lit ses champs 64, 68, 142, 192 et 196, et chaque contact
  suit l'ordre de `FUN_0046ad90` (sur un objet le choc puis le coup de l'objet, sur un piéton le coup puis le choc). L'outil n'a ni
  stimuli ni effets : chaque étape est dite au journal. Dans `1_04` une bille dans une vitre dit l'explosion de rayon 0,001 m, le
  stimulus 2 du tireur et l'effet `Slingshot_Hit` ; dans `1_03` aux briques, `Brick_Hit`.
- **Le contact d'un projectile** (`FUN_0046ad90`, lue dans l'index ; `CContactProjectile`) : le coup au premier contact seulement
  (l'octet +0x22c nul, ou à chaque contact pour RBandBall) ; le tireur épargné pendant les 200 ms qui suivent la création ; l'octet «
  fort » (nul contre un piéton ou un véhicule qui ne reçoit pas le coup, puis sous une vitesse de 0,03, `FUN_0085c4d0`) ; le repos (une
  vitesse au carré sous 1e-4 et une normale au z au-dessus de 0,8). Dans l'outil, le tireur n'est plus écarté pour toute la vie du
  projectile mais par `TireurEpargne`.
- **Les explosions** (`FUN_004d2b40`, `FUN_004d2690`, `FUN_004d92b0`, lues dans l'index ; `FUN_004d2aa0`, lue à l'assembleur de
  0x4d2aa0 à 0x4d2b34 ; `CPlacesExplosions`) : la table de 15 entrées de 0x3c octets à 0xc39b60 ; la pose (la première place libre,
  sinon la plus vieille des évinçables, l'octet +0xdc de la fiche), le départ retardé (+0xcc), la durée (+0xc8), le rayon qui grandit
  de +0xd0 par pas, l'arrêt quand le balayage a touché (+0xdd, +0xe0) ; les explosions avancent avant les projectiles. Dans `1_04`
  l'explosion d'une bille est posée à 17,52 s et finie à l'image suivante, 17,56 s. Le balayage lui-même (la fonction en 0x463b70, puis
  `004629d0`, 2 427 octets) n'est pas lu.
- **La fin d'un projectile** (`FUN_004d45e0`, lue à l'assembleur de 0x4d4601 à 0x4d4632, de 0x4d4684 à 0x4d4739 et à 0x4d4868 ;
  `FUN_004d3dc0`, lue dans l'index ; `CPlacesProjectiles::Vit`, `Detruire`, `ExploseDansLEau`) : un projectile qui a explosé est
  détruit à l'image suivante ; les autres vivent jusqu'à la fin de leur vie (sans fin pour une vie nulle), où `FUN_004d3dc0(i, 1)` fait
  l'explosion de la fin de vie au point de l'objet relevé de 0,05 ; dans l'eau l'explosion se fait d'abord avec l'octet +0x8c ou +0x8d,
  puis la destruction sans explosion. L'octet « +0x12 » d'un commentaire précédent était l'octet +0x16 de l'entrée. Dans l'outil, le
  choc, la chute dans l'eau et la fin de vie passent par une seule fonction (`ExploserProjectile`).
- **Les marques d'impact** (`FUN_00560f70`, `FUN_00560f10`, lues dans l'index ; `FUN_0046ad90`, lue à l'assembleur de 0x46b468 à
  0x46b691 ; `CPlacesMarques`, `CContactProjectile::MarqueDuBatiment`) : la table à 0xcf1f60, son indice d'écriture en tête et 32
  places de 16 octets ; la pose écrit le point avancé de 0,01 fois la normale et revient à 0 après la place 31 ; le compte est celui
  des places d'un genre dans un déclencheur. Sur un bâtiment, la fiche du projectile donne deux marques que `FUN_007282e0` résout à la
  première demande : +0x150 (le nom +0x54, `FUN_00468bb0`) quand le z de la normale n'est pas sous 0,3 en valeur absolue, +0x152 (le
  nom +0x58, `FUN_00468bd0`) sinon, un mur. `ObjectNumProjectileImpacts` compte les deux et les ajoute ; l'outil lit le champ 88 de la
  fiche et numérote les noms des marques (il n'a pas la normale du contact : la marque est posée au point même, supposé). Aucune des
  sept missions n'appelle `ObjectNumProjectileImpacts`.
- **Les comptes des README** : chaque commit est parti d'un arbre qui ne contenait que lui, et le compte a été vérifié par
  `tools/avancement.py` avant chaque envoi. Deux fois des adresses de fonctions non recréées s'étaient glissées dans les commentaires
  de `src/objects/Projectiles.h` (cinq aux explosions, deux aux marques) : elles sont nommées dans l'outil ou sans adresse. Une
  fonction non lue se nomme ici sans préfixe (`004629d0`, la forme de l'argument de `tools/chercher.py`), pour ne pas compter comme
  comprise.
- **Neuf commits et non dix** : huit de moteur et la documentation. Le neuvième commit de moteur aurait été le balayage des explosions
  (`004629d0`, 2 427 octets, non lue) ou le rebond d'un projectile, trop gros pour la fin de la séance ; pas de remplissage.
- **REA amont** : 1 295 commits en attente (875 le 09.11) ; rien n'est fusionné, bully-re interroge l'index et objdump.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 82,53 s et `1_06_01` à 61,81 s,
  inchangés ; `1_05` à 180,31 s (194,30 s avant) et `1_07` à 150,88 s (151,47 s avant) depuis le deuxième commit, inchangés ensuite.
- **Bilan** : 1 243 fonctions recréées (7,9 % du code), contre 1 228 (7,7 %) ; 2 181 comprises (12,5 %), contre 2 163 (12,3 %) ; neuf
  commits (huit de moteur et la documentation).

## 12.11.2026, séance 1 (de 22 h 19 à 23 h 41)

- **Le vol d'un projectile** (`FUN_004494d0`, lue à l'assembleur de 0x4494d0 à 0x4494e4 ; `FUN_004d1510`, de 0x4d1510 à 0x4d1555 ;
  `FUN_00469760`, de 0x469760 à 0x4698bb ; `FUN_004d2160`, de 0x4d2160 à 0x4d228c ; `CVolProjectile::Gravite`, `Peser`, `Deplacer`) :
  la vtable de `CProjectile` (0x9059c4) garde le déplacement de `CPhysical` (+0x94) et change la gravité par pas (+0x8c : le facteur
  +0x230 fois 0,008, le double 0x905a70) et le poids (+0x90 : vz perd cette gravité fois le pas 0xc1a9a4 fois l'échelle +0x154 quand
  le bit 0x2 de +0x1bc est levé). Le déplacement avance x et y de la vitesse fois d (d = +0x154 fois le pas) et z de vz × d moins la
  moitié de la gravité fois d² : **l'unité de +0x124 est bien le mètre par pas**. Le constructeur pose la masse et le facteur de
  gravité à 1, l'air +0x160 à 0,99999, l'élasticité +0x164 à 0,75, +0x168 à 0,0008, +0xc4 à 1. L'outil remplace son pas d'Euler par
  `Deplacer` : par image le projectile descend de 1,5 × g × pas² de plus (0,0327 m pour un facteur 1), et les chocs baissent : dans
  `1_04` à 17,52 s, z 7,68 → 7,58 m (42 lignes changées) ; dans `1_03` aux briques à 123,72 s, z 10,68 → 9,57 m (6 lignes).
  **Rectification** : le message du commit d37cc93 range ce choc de 123,72 s dans `1_04` ; il est dans `1_03` aux briques (`1_04`
  finit à 82,53 s).
- **La résistance de l'air** (`FUN_004698c0`, lue de 0x4698c0 à 0x469a1c ; `CVolProjectile::Air`) : la fin du contrôle d'un
  `CPhysical`. Au-dessus de 0,1 (le double 0x900128), la vitesse et la vitesse angulaire prennent +0x160 puissance le pas (la
  fonction 0085cb40, supposée être pow) ; sinon une traînée, (1 / |1 + +0x160 × 0,5 × |v|²|) puissance le pas, et 0,99 pour la
  vitesse angulaire. Pour un projectile : 0,99999 puissance 1,65 par image de 33 ms, 0,05 % de vitesse perdue par seconde. Dans
  `1_04` les chocs des vols les plus longs bougent d'un centimètre (75,04, 75,87 et 76,66 s : y -92,15 → -92,16 m) ; dans `1_03` aux
  briques, 123,72 s et 125,30 s.
- **Le repos** (`FUN_00469a20`, lue de 0x469a20 à 0x469c5c ; `CVolProjectile::Controler`) : le contrôle d'un `CPhysical` à chaque
  image. Les moyennes +0x13c et +0x148 prennent la moitié de leur somme avec la vitesse et la vitesse angulaire du moment ; sous
  0,003 fois le pas (le double 0x90c700 ; 0,00495 m par pas pour 33 ms, soit 0,25 m/s) pour les deux, le compteur +0x180 monte, et à
  la onzième image de suite la méthode +0x1c(1) met l'entité au repos et les vitesses tombent à 0 ; sinon la fonction finit par un
  saut vers l'air. L'outil garde la moyenne et le compteur de chaque projectile et écrit « le projectile N est au repos en (…) » ;
  **faute de sol dans l'outil des missions, la ligne ne paraît dans aucune des sept** (tests seulement).
- **Le rebond** (la part du projectile dans la fonction `0046ca40`, le déplacement d'un `CPhysical` dans le monde, 2 884 octets,
  non recréée en entier et non comptée ; lue de 0x46cd98 à 0x46ce47 et de 0x46d0d7 à 0x46d22e ; `CVolProjectile::Recul`,
  `Rebondir`) : après le contact, l'entité revient à la fraction du trajet moins 0,005 (le double 0x90c740) ; +0xc4 non nul (cmp en
  0x46ce40, jne 0x46cf6f), c'est un projectile et il rebondit, les autres glissent. La normale (son z positif effacé quand
  l'entité touchée porte +0xf0) est normalisée ; la vitesse devient tangentiel × (v − n p) − normal × n p : 0,75 et 0,75
  (_DAT_00a24040, _DAT_00a2403c) contre un piéton, l'élasticité +0x164 (le +0x5c de la fiche) sinon. **L'outil ne retire plus un
  projectile à son premier contact** : il recule, rebondit et reste dans le monde. Supposé : le contact au point où le trajet
  entre dans la sphère de l'outil et la normale du centre à ce point (essayée d'abord et écartée : la normale au point le plus
  proche du centre, perpendiculaire au vol). Dans `1_04`, vingt et un rebonds : les trois vitres à 17,52, 19,50 et 21,52 s
  (coefficient 0,3, l'élasticité de la bille), dix-huit sur les piétons 5 à 10 de 74,08 à 78,11 s (0,75) ; dans `1_03` aux briques,
  trois sur Davis à 123,72, 125,30 et 127,22 s. Tous explosent à leur contact et sont détruits à l'image suivante : aucun vol
  après rebond dans les sept missions.
- **La paire d'un contact** (`FUN_0046b720`, 342 octets, lue de 0x46b720 à 0x46b873 ; `CContactProjectile::SansContact`) : elle
  remet à 0 le +0xb0 de l'entité touchée et fait les deux contacts (`FUN_0046ad90` dans les deux sens), sauf entre un projectile
  et un piéton dans deux cas : le piéton a un +0x1310 de 0xd et il est le tireur ; ou le projectile est bas (son z moins celui du
  piéton, moins 0,1847944, strictement sous 0,5) et lent (strictement sous 0,1 m par pas). **Une bille qui roule aux pieds d'un
  piéton ne le touche pas.** L'outil passe ce juge avant ses sphères ; le +0x1310 n'est pas suivi.
- **Le contrôle d'un projectile** (`FUN_004d2290`, sa méthode +0x30, lue de 0x4d2290 à 0x4d24e9 ; `FUN_004d2090`, de 0x4d2090 à
  0x4d20ce ; `CVolProjectile::Controle`, `PresDuJoueur`, `EchelleDeChute`) : tenu (+0xb0), rien ; sinon le son du modèle (non
  suivi), l'effet +0xa8 de la fiche une seule fois (le bit 0x1 de +0x22d) quand le projectile passe à 2 m au plus d'un point 1,5 m
  au-dessus du joueur, puis le choix : l'octet +0x15 de l'entrée → un autre vol (`004d1960`, non lu) ; un +0x10 de l'entrée (le
  +0xf0 de la fiche) → le contrôle de l'objet puis la poussée ; l'octet +0x124 de la fiche → le contrôle de l'objet puis
  l'échelle de la chute (+0x154 = le +0x128 de la fiche tant que vz est strictement sous -0,1 m par pas, 1 sinon) ; sinon les
  déclencheurs (`006d7220`, non recréée) puis le contrôle de l'objet. L'outil lit l'effet (le champ 168), garde le bit et
  l'échelle, et écrit « passe près du joueur » et « échelle de temps ».
- **La poussée** (`FUN_004d1ec0`, lue de 0x4d1ec0 à 0x4d208a ; `CVolProjectile::Pousser`) : tant que le temps n'a pas atteint la
  création (+8 de l'entrée) plus la durée (+0x10, le +0xf0 de la fiche), la vitesse reçoit ((v × le pas) × le +0xf8 de la fiche)
  × (1 − f) et vz reçoit en plus (le +0xfc × le pas) × (1 − f) ; f vaut 0 jusqu'au délai (la durée fois le +0x100 de la fiche,
  rendue en entier par 0085c6f0, supposée la conversion), puis monte en ligne droite jusqu'à 1 à la fin de la durée. L'outil lit
  les champs 240, 248, 252 et 256 et pousse après le contrôle.
- **Le freinage et la mise au repos** (`FUN_004cf350`, le contrôle d'un `CObject`, lue de 0x4cf350 à 0x4cf4cd ; `FUN_0040df90`,
  la norme ; `FUN_004cf190`, lue de 0x4cf190 à 0x4cf1c6 ; `CVolProjectile::Norme`, `Freiner`, `Reposer`) : pour un objet dont
  l'octet +0x20a vaut 7 (la création le pose à 7 avec l'octet +0x7c de la fiche, 6 sinon), la fiche du freinage (le nom +0x12c de
  la fiche du projectile dans la table 0x1b65378, sinon DAT_020c888c) donne deux seuils et deux facteurs : au-dessus de +0x1c, la
  vitesse et la vitesse angulaire prennent +0x20 ; sinon +0x24, et sous +0x18 elles tombent d'abord à DAT_00bcc564 (bss, supposé
  nul) ; puis le contrôle du `CPhysical`. La mise au repos : +0xb0 pour le genre 7, +0x28 sinon, et le compteur +0x180 à 10 ou à 0.
  `FUN_004d1560` (lue de 0x4d1560 à 0x4d1666, la méthode +0x1c d'un `CProjectile`) redresse la matrice avant, non recréé.
  L'outil lit le champ 124 et pose par `Reposer` ; il ne lit pas la fiche du freinage (`Freiner` dans les tests seulement).
- **La fléchette et le frisbee** (les deux modèles à part du rebond dans `0046ca40`, lus de 0x46cf6f à 0x46d0d2 et de 0x46d231 à
  0x46d2f6 ; `FUN_004150b0`, lue de 0x4150b0 à 0x4150cf ; `CVolProjectile::Planter`, `Devier`, `NormaleDuContact`) : la table des
  noms de modèles (0xa136e8, `FUN_0043f1e0`, `FUN_0043f220`) donne DAT_00bf44dc = « dart », DAT_00bf439c = « Wfrisbee »,
  DAT_00bf4394 = « WFtBomb ». La fléchette ne rebondit pas : vitesse à 0, 0,1 de plus le long de son vol, le bit 0x2 de +0x1bc
  baissé, +0x28 à 1 : plantée. Le frisbee reçoit après son rebond, sur une normale au z sous 0,7 en valeur absolue, la normale
  fois 0,03 et (0, 0, 1) fois -0,03, à l'échelle (0,7 − |nz|) / 0,7. Aucune des sept missions ne tire ces deux modèles : tests
  seulement.
- **Le sol manque à l'outil des missions** : `outils/lancer_lua.cpp` n'a pas la collision du monde (`CMondeCollision` n'est branché
  que dans `outils/scene.h` et `outils/visite.cpp`). Ses projectiles ne touchent que des sphères de piétons et d'objets, et tous
  ceux des sept missions explosent à leur contact : le repos, le freinage, la poussée, l'échelle de la chute, la paire du contact
  et les deux modèles à part ne se voient que dans `tests/test_armes.cpp`. C'est le premier point de `docs/suite.md`.
- **Les comptes des README** : chaque commit est parti d'un arbre qui ne contenait que lui, le compte vérifié par
  `tools/avancement.py` avant chaque envoi, et les fonctions ajoutées au compte listées avant chaque vérification (un script de
  la séance compare les adresses citées dans `src` à celles de HEAD). Écrites sans préfixe dans `src` pour ne pas être comptées :
  `0046ca40` (lue et recréée pour la seule part du projectile), `0085cb40` (supposée pow), `004d3360`, `006d7220`,
  `00598030`, `0059a710`, `00414cc0`.
- **REA amont** : 1 320 commits en attente (1 295 le 11.11), dont 35 sur PE, x86, objdump ou l'index (`fix(pe): honor import
  directory bounds (#10044)`, `fix(objdump): reflect observed launch failure in discovery availability (#9051)`) ; rien n'est
  fusionné, bully-re interroge l'index et objdump.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 148,04 s), `1_04` à 82,53 s, `1_05` à 180,31 s,
  `1_06_01` à 61,81 s et `1_07` à 150,88 s, inchangés à chaque commit. Les journaux ne changent qu'aux trois premiers commits et
  au rebond (les coordonnées des chocs, puis vingt et une lignes de rebond dans `1_04` et trois dans `1_03` aux briques).
- **Bilan** : 1 256 fonctions recréées (8,0 % du code), contre 1 243 (7,9 %) ; 2 196 comprises (12,6 %), contre 2 181 (12,5 %) ; dix
  commits (neuf de moteur et la documentation).

## 12.11.2026, séance 2 (de 23 h 47 à 01 h 16)

- **Le sol sous les projectiles de l'outil des missions** (`CMondeCollision::Trajet`, `src/collision/Marche.h` et `Marche.cpp` ;
  `AvancerProjectiles` dans `outils/lancer_lua.cpp`) : `Trajet(a, b, &fraction, &normale, &volume)` rend le premier volume que
  croise le segment d'une image (triangles des deux côtés, la normale tournée contre le trajet ; sphères à leur entrée). Ce n'est
  pas le balayage du moteur, c'est le sol qui manquait. L'outil charge les 488 .col de `Stream/World.img` (3 862 modèles de
  collision) et pose les volumes des placements « inst » de la zone visible : zone 0, 3 243 placements posés (164 sans volume),
  97 905 triangles, 863 sphères, grille de 4 m (`LANCER_SANS_MONDE=1` la coupe). Un projectile qui croise le monde avant une
  sphère écrit « le projectile N touche le monde (placement) en (…), normale (…), à … m par pas », puis le choc de sa fiche, la
  marque, le repos par le contact ou le recul et le rebond avec la vraie normale. **Essayé et écarté** : un `AddImage` de plus
  pour l'image du monde (la table de `CdStream` n'a que huit places : à 34,02 s de `1_04` l'arbre `Act/Props/pxTree.act` ne se
  chargeait plus) → l'image déjà ouverte est reprise dans la table ; compter le monde dès la première image du vol (le tir de
  73,36 s de `1_04`, depuis l'arbre, touchait à 27 cm de son départ le volume « treeclimbbranch » où se tient le joueur, et la
  mission ne finissait plus) → **supposé** : la première image du vol ne compte pas le monde (le moteur part de la douille
  « Muzzle » ou de l'os de la main, que l'outil n'a pas). Une option de plus : `--donner ARME@s`. L'essai hors mission
  (`lancer_lua 40 --mission 1_04 --sans-cinematiques --entrer _1_04_PARKING@10 --donner 302@15 --tirer @16.6`) montre enfin le
  repos : la balle part à 16,63 s, touche un mur à 17,06 s, le sol à 17,39 s (« premier contact au sol ») et s'arrête à 18,02 s.
  Dans les missions : deux lignes de plus (`1_04` à 17,39 s, `1_03` aux briques à 122,66 s), temps inchangés.
- **La vitesse de départ d'un tir** (`FUN_0065f050`, lue de 0x65f050 à 0x65f4c4 ; `CViseeTir::VitesseDepart`, `FicheVitesse`
  dans `src/peds/Armes.h`) : la gravité est le +0x80 de la fiche × −0,008, l'allure +0x60 + charge × (+0x64 − +0x60), la durée du
  vol **la distance à plat** divisée par l'allure (le z n'y entre pas) ; jusqu'à vingt essais, la durée passant à (T + 1) × 1,06
  tant que la vitesse de départ, la vitesse d'arrivée et l'angle de plongée ne tiennent pas dans les bornes +0x68 à +0x78 de la
  fiche (une borne à −1 est sans effet) ; puis la dispersion +0x120. **C'est la réponse au troisième point du chantier** : la
  parabole continue de la visée de l'outil est bien celle du moteur ; ce qui différait, c'est la durée du vol (l'outil la tirait de
  la distance dans l'espace) et les bornes, que l'outil ne lisait pas (champs 104 à 120 et 288 : toutes à −1 sauf un angle de 35°
  sur une fiche ; dispersion 0 sauf 0,07 sur la fiche « bRocket »). `1_03` aux briques finit à **147,97 s** au lieu de 148,04 s
  (la deuxième brique touche Davis à 125,27 s au lieu de 125,30 s) ; dans `1_04` les rebonds des vitres passent de 0,840, 0,857
  et 0,860 à 0,847, 0,864 et 0,867 m par pas (12 lignes). `FUN_0065e880` (la douille « Muzzle ») reste non recréée : l'outil n'a
  ni os ni douilles.
- **Le freinage dans l'outil** (`FUN_004cf350` déjà recréée ; `CFicheFreinage`, `Freinage(arme)` dans `outils/lancer_lua.cpp`) :
  la fiche « DodgeballPhysicsButes » est lue dans `Globals.cat` (le chemin « DodgeballPhysics » + le nom +0x12c de la fiche du
  projectile, le champ 300 ; à défaut la piste du nœud Globals ; noms lus à l'assembleur de 0x618052 à 0x61807a) et
  `CVolProjectile::Freiner` passe avant `Controler` pour un projectile de genre 7. Par défaut : arrêt sous 0,0015 m par pas,
  × 0,998 dès 0,05 m par pas, × 0,985 en dessous ; 17 fiches de genre 7, 12 fiches nommées. Dans `1_04`, 192 lignes changent (les
  vitres à 0,837, 0,854 et 0,857 m par pas ; le tir sur le piéton 10 à 74,12 s au lieu de 74,08 s), temps inchangés ; l'essai hors
  mission s'arrête à 17,98 s en (175,48, 7,95, 5,48) au lieu de 18,02 s.
- **L'entourage du balayage** (`FUN_0046c970`, lue de 0x46c970 à 0x46ca3f ; `FUN_0046a920`, de 0x46a920 à 0x46a99b ; le début de
  `0046ca40`, de son premier octet à 0x46cd98, et sa sortie ; `CVolProjectile::Immobile`, `PartagerChute`, `ContactBalaye`,
  `BalayerSecteur`, `BalayerSecteurs`, `CodeDeBalayage`, `PoserApresChute`, `Distance`, `Recul` à la constante exacte
  0.004999999888241291) : le balayage d'un secteur garde le contact de plus petite fraction parmi ses listes, l'affectation d'un
  contact délie l'ancienne entité et lie la nouvelle. Tests seulement, 0 ligne changée dans les missions.
- **Le test des sphères** (`FUN_00465bc0`, lue de 0x465bc0 à 0x465cb9 ; `CVolProjectile::SpheresSeTouchent`) : avant le vrai
  test de collision, le balayage d'une liste écarte l'entité dont la sphère englobante ne touche pas celle du balayage ; le rayon
  grandit de |vx| + |vy| + |vz| pour une entité au +0xf0 non nul ; l'égalité rend faux. Tests seulement.
- **Les trois filtres de piéton** (`FUN_004760b0`, lue de 0x4760b0 à 0x476152 ; `FUN_00476160`, de 0x476160 à 0x476198 ;
  `FUN_00476270`, de 0x476270 à 0x4762ba ; `CVolProjectile::EntiteFiltree`, `IgnorePieton`, `IgnoreSonGenre2`,
  `IgnoreGenre2Lie`) : ce que le balayage d'une liste écarte quand c'est un piéton qui balaie. Les champs gardent le nom de leur
  offset, leur sens n'est pas lu. Tests seulement.
- **La référence d'un contact et deux projectiles de même modèle** (`FUN_0045f0a0`, lue de 0x45f0a0 à 0x45f0d2 ;
  `ContactBalaye::LierEntite` ; une part de `0046c4b0`, de 0x46c71b à 0x46c7b4 ; `CVolProjectile::MemeSens`) : deux projectiles
  du même modèle (un des trois de DAT_00bf435c, DAT_00bf4358, DAT_00bf4354, noms non lus) qui volent à moins de 30° l'un de
  l'autre (cosinus strictement au-dessus du double 0x90c730, 0,866) se traversent. Tests seulement.
- **Le balayage d'une liste est lu, pas recréé** : la fonction `0046c4b0` (de 0x46c4b0 à 0x46c877) est lue en entier à
  l'assembleur ; son test de collision est la méthode +0xa4 de la vtable de l'entité qui balaie (`0046a340` pour un `CProjectile`, le mot
  lu dans le binaire), non lue. La lecture complète est
  dans `docs/suite.md` : c'est le premier point de la suite. La glissade des autres `CPhysical` (de 0x46ce71 à 0x46cf6f) et le
  balayage d'une explosion (`004629d0`) ne sont pas commencés.
- **Moins de commits que demandé** : sept commits de moteur au lieu de huit ou neuf. La première heure est partie dans le premier
  commit (l'`AddImage` de trop et la règle de la première image, trouvés aux missions qui ne finissaient plus), et chaque
  vérification complète (build, tests, huit exécutions) prend six minutes.
- **Les comptes des README** : chaque commit est parti d'un arbre qui ne contenait que lui, les adresses ajoutées au compte
  listées avant chaque commit. **Appris** : le compte prend aussi, dans `src`, la forme `0x…` du premier octet d'une fonction, pas
  seulement `FUN_…` (deux commentaires reformulés avant l'envoi). Écrites sans préfixe pour ne pas être comptées : `0046ca40`,
  `0046c4b0`, `0051ae00`, `00465690`, `00465680`, `0065e880`.
- **REA amont** : déjà relevé cette nuit (1 320 commits en attente), pas refait.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques **147,97 s**, 148,04 s avant : la durée du vol
  tirée de la distance à plat), `1_04` à 82,53 s, `1_05` à 180,31 s, `1_06_01` à 61,81 s et `1_07` à 150,88 s. Les journaux ne
  changent qu'aux trois premiers commits.
- **Bilan** : 1 264 fonctions recréées (8,0 % du code), contre 1 256 (8,0 %) ; 2 203 comprises (12,7 %), contre 2 196 (12,6 %) ;
  huit commits (sept de moteur et la documentation).

## 13.11.2026, séance 1 (de 01 h 20 à 02 h 17)

- **Le balayage d'une liste de secteur est recréé** (`FUN_0046c4b0`, 967 octets, lue de 0x46c4b0 à 0x46c877 ;
  `CVolProjectile::BalayerListe`, `EntiteBalayee`, `VolumeBalaye` dans `src/objects/Projectiles.h`) : c'était le premier point de
  la suite, le vrai test de collision du déplacement d'un `CPhysical`. Un gabarit sur ce qu'il appelle, dans l'ordre du binaire :
  le rayon (celui de l'entité, plus la norme de sa vitesse fois le pas quand son +0x28 est nul) et le centre ; les nœuds de la
  liste (la piscine, l'indice, le nœud suivant, 0x3fff à la fin) ; le code de balayage +0x10a (une entité n'est testée qu'une
  fois) ; ce qu'un piéton et une entité de genre 2 ne testent jamais ; l'entité elle-même ; le filtre d'entrée ; le volume du
  modèle de collision ; les sphères englobantes ; les trois jugements d'un projectile ; puis la méthode +0xa4, le contact local
  lié à l'entité et copié dans le meilleur quand sa fraction est strictement plus petite. 39 vérifications dans `test_armes` sur
  un monde d'essai.
- **Ce qu'il appelle, lu et recréé avant lui** : les deux filtres d'un projectile, `FUN_004761a0` (202 octets : le piéton écarté,
  dont **le tireur, toujours**) et `FUN_00469f10` (84 octets : l'entité de genre 2), avec le ballon prisonnier (`FUN_006838d0`,
  `FUN_0067b0f0`, `FUN_00683a10`, `FUN_00680b60`) et l'entrée de la table des projectiles (`FUN_004d3360`) : `ProjectileIgnore`,
  `VehiculeIgnore`, `BallonActif`, `JoueurDeBallon`, `CPlacesProjectiles::AdresseEntree` ; **la méthode +0xa4 d'un `CProjectile`,
  `FUN_0046a340`** (228 octets, `TesterCollision`) avec `FUN_00447290` (`EstUnChien`) : elle choisit entre le test fin contre un
  piéton (`00572680` puis `00469f70`, non lues) et le test général (`0056c770`, non lue) ; le centre du balayage (`FUN_00466b50`,
  `FUN_004660b0`, `FUN_0046d700` : `DansLeMonde`, `PlacerSansMatrice`), le modèle de collision d'une entité (`FUN_0051ae00`,
  `ModeleDeCollision`), son vecteur « avant » (`FUN_004ea210`, `Avant`), le contact neuf et sa fin (`FUN_00415e50`,
  `FUN_004153b0`, `ContactBalaye::Defaire`). Les trois modèles de la règle des deux projectiles sont nommés, lus dans la table
  0xa136e8 : « brocket », « potato », « ammo_scatter ».
- **La glissade** (`0046ca40`, de 0x46ce71 à 0x46cf6a, avec le recul de 0x46cd98 à 0x46ce6b ; `Reculer`, `Glisser`,
  `TourDeGlissade`, et `FUN_0041c390`, 83 octets, `PoserPosition`) : une entité qui n'est pas un projectile recule au contact
  (l'écart fois la fraction moins 0,005), puis son reste de déplacement est ramené dans le plan du contact (le produit scalaire
  de la normale et du reste changé de signe, plus 0,001, le double 0x900fa0) et la boucle des contacts repart ; au troisième
  contact d'une image elle reste au recul. **`0046ca40` est maintenant lue en entier** (2 884 octets), recréée par morceaux et
  pas d'une pièce : elle n'est pas au compte. Tests seulement.
- **Le balayage branché dans l'outil des missions** (`outils/lancer_lua.cpp` : `CBalayageOutil`, `AvancerProjectiles`) : les
  projectiles de l'outil passent par `BalayerListe` au lieu de ses trois boucles. À chaque image et pour chaque projectile,
  l'outil fait une liste aux nœuds du binaire (les piétons vivants, les objets cassables qui restent, puis le monde de la zone,
  une seule entité, absente à la première image du vol) ; le code recréé juge le code de balayage, le filtre d'entrée, le volume
  et les trois jugements d'un projectile, dont `FUN_004761a0` : **un projectile ne touche plus jamais son tireur** (l'outil
  l'épargnait 200 ms seulement, la règle du contact `FUN_0046ad90`). Le test lui-même reste celui de l'outil, **supposé** (le
  segment de l'image contre trois sphères de 0,35 m pour un piéton, une de 0,5 m pour un objet, les triangles du monde) : les
  deux tests fins ne sont pas lus. Aucune ligne ne change dans les huit
  exécutions ni dans l'essai hors mission (la balle 302 au repos à 17,98 s) : `1_04` (110 lignes de projectile, 21 rebonds),
  `1_03` aux briques (23 lignes) et l'essai (37 lignes) sortent les mêmes par le code recréé ; la ligne « traverse son tireur »
  ne sort dans aucune (aucun trajet ne recroise le tireur après les 200 ms).
- **Les références vers une entité** (`FUN_00450730`, 66 octets ; `FUN_00450780`, 56 octets ; `FUN_00465680` et `FUN_00465690`,
  15 octets chacune ; `src/entities/References.h` : `CReferenceEntite`, `CReferencesEntite`) : la liste +0x118 d'une entité, des
  maillons de 8 octets (le suivant, l'adresse du champ qui la désigne) pris à une réserve (le mot 0xc11d40) ; ce sont les deux
  appels que le contact d'un balayage fait avec son champ +0xc. **Supposé** : la liste sert à remettre ces champs à nul quand
  l'entité disparaît (le parcours n'est pas lu). Tests seulement.
- **Les joueurs du ballon prisonnier** (`FUN_0067b0b0`, 51 octets ; `FUN_0067b100`, 64 octets ; `PlaceDeBallon`,
  `ValeurDeBallon`) : l'objet du mot DAT_01b89150 tient neuf emplacements de 0x28c octets à partir de +0x28 (le piéton en +0, un
  réel en +8) ; c'est le réel que `FUN_00680b60` compare à 0,5 : le bout de la chaîne de `FUN_004761a0`. Un piéton absent rend
  0,0. Le sens du réel n'est pas lu. Tests seulement.
- **Non fait, écrit en tête de la suite** : les deux tests fins que choisit `FUN_0046a340` (`00469f70` après `00572680`, et
  `0056c770`, 4 610 octets), non lus : tant qu'ils ne le sont pas, le test lui-même reste celui de l'outil, supposé ; composer
  `0046ca40` d'une pièce (il manque l'angle passé à `FUN_0046b720` pour un projectile contre un piéton, un vecteur pointé par
  ebp non identifié) ; le départ du tir `0065e880`, toujours bloqué (ni os ni douilles) ; le balayage d'une explosion
  (`004629d0`), pas commencé.
- **Une erreur de test, au quatrième commit** : six attentes de `test_armes` sur l'ordre des liens et des déliens étaient
  fausses (le second lien du meilleur contact, sur l'entité qu'il vient de copier, ne fait rien : `FUN_0045f0a0` sort quand
  c'est déjà elle). Corrigées dans le test, les tests rejoués seuls ; le code recréé n'a pas changé.
- **Moins de commits que demandé** : sept commits de moteur au lieu de huit ou neuf. Les références et les joueurs du ballon
  prisonnier sont partis dans un seul commit (une vérification complète prend six minutes), pour que la séance soit fusionnée
  avant 02 h 30, l'heure où la limite d'usage pouvait couper l'agent.
- **Les comptes des README** : chaque commit est parti d'un arbre qui ne contenait que lui, les adresses ajoutées au compte
  listées avant chaque commit (un script du dossier temporaire compare le diff de `src` au compte d'avant). Écrites sans préfixe
  pour ne pas être comptées : `0046ca40`, `00469f70`, `0056c770`, `00572680`, `0065e880`.
  **Une erreur dans un message** : celui du septième commit (f54307f) dit « six fonctions de plus au compte » ; c'est quatre
  (`FUN_00450730`, `FUN_00450780`, `FUN_0067b0b0`, `FUN_0067b100`) : `FUN_00465680` et `FUN_00465690` étaient déjà citées dans
  `src/peds` (`Armes.h`, `Patrouilles.h`, `ScriptPedPool.cpp`) sans être recréées ; elles le sont maintenant. Le compte des README,
  lui, est juste (1 286). Le script du dossier temporaire ne voyait pas un fichier neuf non suivi (`References.h`).
- **REA amont** : déjà relevé cette nuit, pas refait.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 147,97 s), `1_04` à 82,53 s, `1_05` à
  180,31 s, `1_06_01` à 61,81 s et `1_07` à 150,88 s, réussies à chaque commit, **inchangées de bout en bout**. Aucune ligne de
  journal ne change dans les huit exécutions, à aucun commit, branchement du balayage dans l'outil compris.
- **Bilan** : 1 286 fonctions recréées (8,1 % du code), contre 1 264 (8,0 %) ; 2 223 comprises (12,7 %), contre 2 203 (12,7 %) ;
  huit commits (sept de moteur et la documentation).

## 13.11.2026, séance 2 (de 03 h 05 à 04 h 18)

- **Le test général de collision est recréé d'une pièce** (`FUN_0056c770`, 4 610 octets, lu à l'assembleur de 0x56c770 à
  0x56d970 ; `CCollisionBalayee::TestGeneral`, `src/collision/Balayes.h`, nouveau) : c'était le premier point de la suite, celui
  des deux tests fins que choisit la méthode +0xa4 d'un `CProjectile` (`FUN_0046a340`). Deux modèles, le premier qui se déplace
  de m1 à m2, le second de m4 à m5 ; pour chaque sphère du premier, les sphères du second (bit 1), ses boîtes (bit 2) et ses
  triangles (bit 8), dans l'espace du second ; la plus petite fraction gagne, strictement. Les données d'un modèle (+0x38) sont
  maintenant connues : les sphères en +0x0 (0x14 octets), les boîtes en +0x8 (0x24), les sommets en +0xc, les triangles en +0x10
  (8 octets), l'arbre en +0x14, les comptes en +0x1c (16 bits), +0x1e et +0x1f (8 bits) ; `src/collision/ColModel.h` les disait
  « non retrouvées ». 13 vérifications dans `test_marche`.
- **Tout ce qu'il appelle, recréé avant lui** : les matrices (`FUN_0045d3b0`, `FUN_00414560`, `FUN_00413620`, `FUN_00413780`),
  les sommets compressés (`FUN_0056e530`), deux sphères qui se déplacent (`FUN_00576a00`, 894 octets) ; une sphère contre un
  triangle (`FUN_00575510`, 2 205 octets) avec ses sept pièces et ce qu'elles appellent (`FUN_00414ad0`, `FUN_00414cc0`, `FUN_00574fe0`, `FUN_00575100`,
  `FUN_00575140`, `FUN_005751a0`, `FUN_00575260`, `FUN_00575310`, `FUN_00574dc0`) ; une sphère contre une boîte (`FUN_00576660`,
  900 octets), sa face à quatre coins (`FUN_00575db0`, 2 214 octets, lue en regard de `00575510`) et le point dans un polygone
  (`FUN_00574f90`) ; le point dans une boîte (`FUN_00565ad0`). La fraction que rend `00575510` est une distance le long du
  déplacement normalisé, que le test général divise par sa longueur. **La normale d'un triangle est (v2 − v0) × (v1 − v0)**
  (`FUN_004145e0` est a × b) et **un triangle n'est touché que de face** : un projectile qui arrive par l'arrière le traverse.
- **Le test fin contre un piéton** (`FUN_00469f70`, 966 octets ; `TestPieton`) : les sphères du projectile contre celles du
  modèle du piéton ; la sphère 0 touchée donne la fraction, puis le déplacement est allongé cent fois et seule la sphère 3 donne
  le point, la normale et le rang. Avec lui les deux tests fins de `FUN_0046a340` sont recréés ; le modèle du piéton, que prépare
  `00572680` sur six os du squelette, ne l'est pas (l'outil n'a pas d'os).
- **Le monde de l'outil passe par le test général** (`outils/lancer_lua.cpp` : `CVueMonde`, `VueDuMonde`, `TesterMonde`) au lieu
  de `CMondeCollision::Trajet` : la collision de la zone est vue comme un seul modèle posé à l'identité ; la grille de l'outil
  remplace l'arbre de partition ; les boîtes du monde, déjà en triangles, tournent dans le sens du binaire. Les mêmes contacts
  aux mêmes instants dans l'essai hors mission : seules 16 lignes changent, par le signe d'une composante nulle de la normale.
- **La méthode +0xa4 branchée dans l'outil** : `CBalayageOutil::Tester` passe par `FUN_0046a340` (`TesterCollision`) ; la
  collision de la zone note la surface de chaque triangle (`CMondeCollision::surface`). **Le mur « nogo_tschoolnop » porte la
  surface 0x41** : dans l'essai hors mission, la balle 302 le traverse à 17,06 s, touche le sol « fgrd_nd_sc1op » à 17,19 s en
  (175,48, 12,08, 6,07) et est au repos à 17,85 s en (175,49, 12,66, 6,07), au lieu de rebondir sur le mur et de s'arrêter à
  17,98 s en (175,48, 7,95, 5,48) ; 17 rebonds, 40 lignes de projectile au lieu de 37.
- **Le projectile reçoit son modèle de collision** (`SpheresDuProjectile`) : les .col de `Stream/World.img` contiennent ceux
  des projectiles (baseball.col, marble.col, BRICK.col…) ; la balle 302 a une sphère de 0,055 m, la bille 304 une de 0,025 m,
  la brique 311 une de 0,129 m, au centre du modèle. Dans l'essai hors mission, le centre de la balle s'arrête 5,5 cm au-dessus
  du sol (au repos à 17,85 s en (175,49, 12,52, 6,13)) ; dans `1_03` aux briques, la brique de 0,129 m touche une surface 0x41
  à 127,08 s et la traverse.
- **Dans les sept missions, aucun projectile ne touche le monde** : les rebonds de `1_04` et de `1_03` aux briques sont sur des
  objets et des piétons. Aucune ligne de journal ne change à aucun commit, sauf au huitième : la ligne du modèle de la bille dans `1_04`, celles du modèle de la brique et de sa traversée
  dans `1_03` aux briques (des lignes ajoutées ; aucune ligne d'avant ne change).
- **Une erreur dans un message** : celui du septième commit (b554023) dit que la balle « rebondit treize fois sur place » après
  avoir touché le sol ; c'est seize fois (dix-sept rebonds en tout, 40 lignes de projectile).
  Celui du huitième commit (58f2b05) dit que 16 lignes de projectile de l'essai changent par leur point : c'est 18 (les dix-sept
  contacts et la ligne du repos), et une ligne s'ajoute (le modèle de la balle).
- **Non fait, écrit en tête de la suite** : le modèle de collision des objets cassables (il manque leur matrice dans l'outil),
  celui d'un piéton (`00572680`, bloqué sans os), l'arbre de partition d'un modèle (`005616a0`, `00561750`), puis ce qui restait
  du 13.11, séance 1 (composer `0046ca40`, le départ du tir, le contrôle, les références d'entité) et le balayage d'une explosion.
- **Les comptes des README** : chaque commit est parti d'un arbre qui ne contenait que lui, les adresses ajoutées au compte
  listées avant chaque commit (le script du dossier temporaire voit maintenant un fichier neuf, ajouté par `git add -N`). Écrites
  sans préfixe pour ne pas être comptées : `00572680`, `005616a0`, `00561750`, `00561080`, `005615a0` (et `00576660` jusqu'à sa
  recréation, au quatrième commit).
- **REA amont** : déjà relevé cette nuit, pas refait.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 147,97 s), `1_04` à 82,53 s, `1_05` à
  180,31 s, `1_06_01` à 61,81 s et `1_07` à 150,88 s, réussies à chaque commit, **inchangées de bout en bout**.
- **Bilan** : 1 307 fonctions recréées (6,3 % des fonctions, 8,4 % du code), contre 1 286 (6,2 %, 8,1 %) ; 2 240 comprises
  (13,0 % du code), contre 2 223 (12,7 %) ; neuf commits (huit de moteur et la documentation).

## 13.11.2026, séance 3 (de 04 h 24 à 05 h 08)

- **La règle supposée de la première image du vol est retirée** (`q.sorti` dans `AvancerProjectiles`) : le monde est compté dès
  le premier pas. Depuis que le monde passe par le test général, un triangle n'est touché que de face (`FUN_00575510`) : un
  projectile qui part de l'intérieur d'un volume en voit les faces de dos. Le tir de 73,36 s de `1_04`, qui touchait à 27 cm le
  volume « treeclimbbranch » le 12.11, ne touche plus rien et la mission finit au même instant ; seule la lecture du monde avance
  de 3 centièmes (au premier pas du premier tir).
- **Les objets cassables passent par le test général** (`DonneesObjet`, `CVueObjet`, `TesterObjet`) avec leur modèle de
  collision (le .col de même nom) et la matrice de leur placement (`CObjetOutil` garde le quaternion et l'échelle de l'entrée
  inst) ; leurs boîtes par `FUN_00576660`. Le contact garde la fraction, la normale et la surface du test ; le rebond les prend
  comme pour le monde. Un objet sans modèle garde la sphère de 0,5 m. L'outil n'essaie que les objets dont la sphère englobante,
  posée et grossie du projectile, croise le trajet (sinon les 60 modèles d'objets de la carte étaient essayés à chaque pas).
  Dans `1_04`, la bille touche les trois vitres du bus (« buswindow », 4 triangles) aux mêmes instants, 17,52, 19,50 et
  21,52 s, sur leurs triangles : le rebond suit la normale de la vitre, (-0,185, 0,837, 0,086) → (-0,056, -0,243, 0,067) m par
  pas au lieu de (0,052, -0,234, -0,097), et le monde, qui n'est plus caché par la sphère de 0,5 m, a derrière la vitre une
  surface 0x41 que la bille traverse.
- **L'arbre de partition d'un modèle et son parcours, recréés** (`src/collision/Arbre.h`, `CArbreCollision`) : `FUN_005616a0`
  (le premier candidat), `FUN_00561750` (le suivant), `FUN_005615a0` (une case de la pile), `FUN_00564a00` (une sphère contre une
  boîte), la pose au chargement `FUN_00561790` et le constructeur `FUN_00561690` ; la descente `00561080` pour les modes 1 et 3
  (le mode 2, `00564d40`, 2 605 octets, posé par `00561700`, ne l'est pas). **Les nœuds du fichier ne sont pas dans l'ordre de
  la mémoire** : `FUN_00561790` les remet (les drapeaux de +0 à +3, les comptes de +1 à +7, les coupes de +2..+7 à +0..+2 et
  +4..+6) ; lus dans l'ordre du fichier, un nœud renvoyait à la racine et le parcours bouclait sans fin. La pile du parcours a
  32 cases. Une coupe vaut min + octet × (max − min) / 255.
- **Le carré de la distance en x et y** (`FUN_005641c0`, 1 001 octets, chaque pas arrondi à float) et le mode 3 de la descente
  (la sphère vue en x et y, celui que demande `FUN_005616a0` quand son troisième argument n'est pas nul), au quatrième commit.
- **Où cela se voit** : `test_col` essaie les 772 modèles qui ont un arbre (sur 3 863), une sphère de 5 cm au centre de chacun de
  leurs 131 022 triangles : le parcours les rend tous sauf 5, en mode 1 comme en mode 3 ; aucun sommet hors de la boîte de la
  racine ; les 5 et 4 rangs au-delà des triangles sont dans deux modèles dont l'arbre ne couvre pas les triangles (l'un, de 12
  triangles et 7 nœuds, rend les rangs 2 573 et 2 574) : leurs données, gardées telles quelles dans la vérification. Dans
  l'outil, un objet dont le modèle a un arbre le donne au test général : les vitres de `1_04` ont un arbre d'un nœud, mêmes
  contacts.
- **Non fait, écrit en tête de la suite** : le modèle d'un piéton (`00572680`, bloqué sans os : ce qu'il faudrait est écrit), le
  mode 2 de l'arbre, le monde par placements avec leurs arbres au lieu de la grille de l'outil.
- **Une erreur dans un message** : celui du quatrième commit (9fa2ffd) dit « 8,4 % du code » ; le compte est passé à 8,5 % à ce
  commit (1 314 fonctions, 379 387 octets sur 4 480 993).
- **Quatre commits de moteur, pas huit** : le chantier de la suite (les points 1, 2 et 4) est fait ; le point 3 est bloqué (pas
  d'os) ; ce qui reste de l'arbre (le mode 2, `00564d40`, 2 605 octets) n'a aucun appelant dans l'outil, et le monde par
  placements ne changerait rien de visible : plutôt que des commits sans effet, la séance s'est arrêtée avant l'heure.
- **Les comptes des README** : chaque commit est parti d'un arbre qui ne contenait que lui ; écrites sans préfixe pour ne pas
  être comptées : `00561080` (le mode 2 manque), `00564d40`, `00561700`, `00572680`.
- **REA amont** : déjà relevé cette nuit, pas refait.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 147,97 s), `1_04` à 82,53 s, `1_05` à
  180,31 s, `1_06_01` à 61,81 s et `1_07` à 150,88 s, réussies à chaque commit, **inchangées de bout en bout**.
- **Bilan** : 1 314 fonctions recréées (6,3 % des fonctions, 8,5 % du code), contre 1 307 (6,3 %, 8,4 %) ; 2 247 comprises
  (13,0 % du code), contre 2 240 (13,0 %). Quatre commits de moteur et celui-ci.

## 13.11.2026, séance 4 (de 05 h 13 à 06 h 09)

- **Le modèle de collision d'un piéton est recréé et branché** (0cb74b1) : `FUN_00572680` (lue à l'assembleur de 0x572680 à
  0x572c78, 1 529 octets ; `CModelePieton::Construire`, `src/collision/ModelePieton.h`) refait pour un piéton le modèle unique
  du mot 0xcf3138 : six sphères sur six os du squelette, les rangs et les rayons dans la fiche FightCollisionButes du modèle
  (Globals.cat), ramenées au repère du piéton, avec une sphère englobante et une boîte. Avec elle `FUN_00412670`,
  `FUN_004130d0`, `FUN_00571280`, `FUN_00570e40`, `FUN_0056ea30` et `FUN_005646e0`. Le blocage « l'outil n'a pas d'os » est
  levé : l'outil lit le squelette du .nif du modèle (36 nœuds sous « Dummy »), à la pose du fichier (**supposé** : le binaire
  lit la pose de l'image), et `TesterPieton` remplace les trois sphères de 0,35 m. Ce que cela corrige : la sphère 0 n'englobe
  pas le piéton, elle est sur le bassin ; la 1 sur le haut de la colonne, la 2 sur la tête, la 3 de nouveau sur le bassin
  (0,1 m), les 4 et 5 sur les mollets.
- **Ce qui se voit** : dans `1_04`, le modèle de Gary à 17,36 s, ceux de dix piétons à 73,36 s (quatre avec leur fiche, six par
  défaut) et de Jimmy à 73,59 s ; les 18 rebonds sur un piéton restent (piéton 10 à la sphère 5, piétons 5 et 6 à la sphère 3,
  piétons 7, 8 et 9 à la sphère 0), trois coups sur le piéton 8 un pas plus tôt (75,04, 75,87, 76,66 s au lieu de 75,08, 75,90,
  76,69 s). Dans `1_03` aux briques, les modèles des piétons 6 et 7 à 122,63 s et de Jimmy à 122,86 s ; trois briques sur le
  piéton 6 à 123,72, 125,27 et 127,18 s (sphères 5, 0, 0), aux mêmes secondes, le point du choc et le rebond changés.
- **Un os par son nom** (3983d49) : `FUN_0068e380` (cdecl (squelette, nom), de 0x68e380 à 0x68e3d7, 88 octets) est
  `CHxdFile::OsParNom` (`src/anim/Hxd.h`) ; « Jaw » a le rang 15 et « Eyes » le rang 18 dans MAINPED.HXD. L'outil nomme les
  six os de chaque modèle, et sur les quinze modèles des missions le nœud du .nif à chaque rang est l'os de même nom : l'ordre
  des os, qui était supposé, est vérifié.
- **Les deux autres parcours qui refont le modèle d'un piéton** (a24dc5a, fa14a47, dfb291e) : `FUN_00563aa0`
  (`CVolProjectile::LancerSurListe`, de 0x563aa0 à 0x563d22 : le parcours d'un « lancer », qui garde le contact le plus
  proche) et `FUN_0045f520` (`VueSurListe`, de 0x45f520 à 0x45f7e6, 711 octets : une ligne, qui rend l'entité la plus proche)
  avec le filtre d'entrée `FUN_0041c3f0` (`Entre`, 69 octets). Avec `FUN_0046c4b0`, les trois appelants de `FUN_00572680`
  sont recréés. Avec eux `FUN_00460f60` (`VueSurSecteur`, 327 octets), la seule appelante de `FUN_0045f520` : la ligne sur
  les cinq listes d'un secteur, choisies par les cinq bits bas des drapeaux ; la liste +0xc est celle des piétons (la seule
  qui reçoive les six sphères et les deux octets du monde qui laissent passer un piéton mort ou monté). Ils ne sont pas
  branchés (l'outil n'a ni lancer ni ligne de vue) : ils se voient dans `test_armes`, sur un monde d'essai. Leurs tests
  (`00569f10`, `0056aee0`) et leurs appelantes (`00563f30`, `00461d50`) ne sont pas lus.
- **Le départ du tir, lu et écarté** : `0065e880` (461 octets) est lue en entier, avec `006c1030` et `006bf410` (la
  transformation d'un os) : la douille « Muzzle » de l'arme tenue, à défaut la matrice du piéton. Elle dépend de la pose
  animée (la main levée) et des douilles de l'arme : pas recréée, le détail est dans `docs/suite.md` (point 5).
- **La pose par l'animation, écartée** : l'outil ne sait pas quelle animation joue un piéton ; les pièces existent
  (`src/anim/Agr.h`, `AgrHxd.h`, `outils/scene.h`) mais ne sont pas liées à lancer_lua. C'est le premier point de la suite.
- **Les comptes des README** : chaque commit est parti d'un arbre qui ne contenait que lui. Le compte n'a pas bougé au
  commit 3983d49 : l'adresse 0x68e380 était déjà citée par `src/vehicles/Bike.h` (une déclaration sans corps, `FindNode`, qui
  la dit rendre un pointeur : à corriger). Écrites sans préfixe pour ne pas être comptées : `00569f10`, `0056aee0`,
  `0051ae00`, `00563f30`, `00461d50`, `00465a50`.
- **REA amont** : déjà relevé cette nuit, pas refait.
- **Chronos** : `1_01` à 10,46 s, `1_02` à 132,92 s, `1_03` à 148,90 s (aux briques 147,97 s), `1_04` à 82,53 s, `1_05` à
  180,31 s, `1_06_01` à 61,81 s et `1_07` à 150,88 s, réussies à chaque commit, **inchangées de bout en bout**.
- **Bilan** : 1 325 fonctions recréées (6,4 % des fonctions, 8,6 % du code), contre 1 314 (6,3 %, 8,5 %) ; 2 256 comprises
  (13,1 % du code), contre 2 247 (13,0 %). Cinq commits de moteur et celui-ci.
