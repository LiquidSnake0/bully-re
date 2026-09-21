# Table virtuelle de CPlaceable → CEntity → CPhysical → CVehicle → CBike

Lue dans `bully.exe` (vftable de CBike @ 0x912b24, 60 emplacements). Pour
chaque emplacement : la classe qui l'introduit, celle dont l'implémentation
est en table pour un CBike, et le nom retenu. Confiance : **sûr** (corps lu et
identique à re3/reVC), *probable* (corps lu, sémantique claire), ? (hypothèse).

| Slot | Adresse | Introduit | Implémenté | Nom | Confiance |
|---|---|---|---|---|---|
| 0 | 004bd6e0 | CPlaceable | CBike | `~CBike` (destructeur virtuel) | **sûr** |
| 1 | 004b9e30 | CEntity | CBike | `IsOfTypeId(int16)` : compare à deux identifiants de type alloués à la volée | *probable* |
| 2 | 00469090 | CEntity | CPhysical | `Add()` : insertion dans les secteurs du monde, listes d'entrées à +0x178 | **sûr** |
| 3 | 0046a620 | CEntity | CPhysical | `Remove()` : parcourt `m_entryInfoList` (+0x178) et détruit les nœuds | **sûr** |
| 4 | 007ffc70 | CEntity | CBike | retourne 1, prédicat de type (`IsBike` ?) | ? |
| 5 | 00828240 | CEntity | CEntity | fonction vide partagée (aussi slots 21, 47) | *probable* |
| 6 | 004cb1a0 | CEntity | CVehicle | `SetStatus(uint8)` : écrit les bits 3..7 de +0x108, réagit aux statuts 3 et 4 | *probable* |
| 7 | 004b95d0 | CEntity | CBike | écrit +0xb0 et +0x180 (`SetIsStatic` ?) | ? |
| 8 | 004c5ec0 | CEntity | CBike | `SetModelIndex(id, ?)` : appelle CVehicle::SetModelIndex puis attache `Handlebars`, `Front_Wheel`, `Rear_Wheel`, `Front_Mud` | **sûr** |
| 9 | 00465cd0 | CEntity | CEntity | `SetModelIndexNoCreate(int16)` : écrit `m_modelIndex` (+0x10e) et un drapeau à +0xbc | *probable* |
| 10 | 004661a0 | CEntity | CEntity | `CreateRwObject()` : `CModelInfo::ms_modelInfoPtrs[m_modelIndex]->CreateInstance()` dans `m_rwObject` (+0x18), cas piéton avec `RAT_PED\BASE` / `C_PLAYER\BASE` | **sûr** |
| 11 | 00468f90 | CEntity | CPhysical | `GetBoundRect()` : centre ± rayon de la sphère de collision | **sûr** |
| 12 | 004c61f0 | CEntity | CBike | `ProcessControl()` ? (793 octets) | ? |
| 13 | 004c6510 | CEntity | CBike | `ProcessCollision()` ? (2313 octets) | ? |
| 14 | 004c2080 | CEntity | CBike | `ProcessShift()` ? | ? |
| 15 | 0044be90 | CEntity | CEntity | `Teleport(CVector)` ? | ? |
| 16 | 00824d30 | CEntity | CEntity | ? | ? |
| 17 | 004c2180 | CEntity | CBike | `PreRender()` ? | ? |
| 18 | 004bfb70 | CEntity | CBike | `Render()` ? | ? |
| 19–33 | | CEntity | CEntity / CVehicle | à lire : éclairage, destruction différée, extensions Bully (CEntity a 34 virtuelles contre 17 dans reVC) | ? |
| 34 | 004b9ea0 | CPhysical | CBike | `ProcessEntityCollision` ? | ? |
| 35–40 | | CPhysical | CPhysical | extensions physiques propres à Bully | ? |
| 41 | 004c3ae0 | CPhysical | CBike | ? | ? |
| 42–59 | | CVehicle | CVehicle / CBike | `ProcessControlInputs`, portes, klaxon, `BlowUpCar`… à apparier avec reVC (18 emplacements contre 24) | ? |

## Champs identifiés sur CEntity / CPhysical / CBike

| Décalage | Type | Nom | Vu dans |
|---|---|---|---|
| +0x18 | `RwObject*` | `m_rwObject` | slot 10 |
| +0x108 | bits 0..2 / 3..7 | `m_type` / `m_status` | slots 2, 6, 10 |
| +0x10e | `int16` | `m_modelIndex` | slots 9, 10 |
| +0x178 | `CEntryInfoList` | `m_entryInfoList` | slots 2, 3 |
| +0x114 | `RpClump*` ou squelette | racine des nœuds nommés du modèle | slot 8 |
| +0x784..+0x790 | 4 × pointeur | nœuds `Handlebars`, `Front_Wheel`, `Rear_Wheel`, `Front_Mud` | slot 8 |

## Globales

| Adresse | Rôle |
|---|---|
| 0x00c1b17c | tableau des secteurs du monde (`CWorld::ms_aSectors`), pas de 0x24 × 5 mots par secteur en X, 0xb4 mots par ligne en Y |
| 0x00c67738 | `CModelInfo::ms_modelInfoPtrs` |
| 0x00bf3830 | compteur d'identifiants de type (slot 1) |
