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
