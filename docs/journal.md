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
