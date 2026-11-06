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

