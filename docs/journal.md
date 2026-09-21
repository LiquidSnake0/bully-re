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
