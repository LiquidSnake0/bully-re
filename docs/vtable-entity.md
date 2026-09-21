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
| 19 | 00467d60 | CEntity | CEntity | `SetupLighting()` : 726 octets, n'agit que si `m_rwObject` est un clump (type 2) ; même position que reVC | *probable* |
| 20 | 00468040 | CEntity | CEntity | `RemoveLighting()` : 700 octets, symétrique du 19 | *probable* |
| 21 | 00828240 | CEntity | CEntity | `FlagToDestroyWhenNextProcessed()` : vide, même position que reVC | *probable* |
| 22 | 004655f0 | CEntity | CEntity | `IsRenderable()` ? : faux si +0xa4 non nul et +0xe4 nul | ? |
| 23 | 0085b353 | CEntity | CVehicle | méthode virtuelle pure (`__purecall`), CVehicle l'implémente en 0x43a2d0 | **sûr** (pure) |
| 24 | 0049a500 | CEntity | CEntity | vide | *probable* |
| 25 | 00467ab0 | CEntity | CEntity | `GetBoundingBoxTransformed(out, matrice)` ? : boîte du modèle (modelinfo +0xc) transformée | ? |
| 26 | 00466060 | CEntity | CEntity | appelle le slot 25 puis 0x41a800 | ? |
| 27, 28 | 00466090 | CEntity | CEntity | `GetBoundDiameter()` ? : 2 × rayon du colmodel | ? |
| 29 | 00465cc0 | CEntity | CEntity | `GetBoundRadius()` : rayon du colmodel (+0xc), utilisé par `GetBoundRect` | **sûr** |
| 30 | 0044bea0 | CEntity | CEntity | `+0xcc != 0` | ? |
| 31 | 00512610 | CEntity | CEntity | parcourt les effets 2D du modèle (0x50ec20), vrai si l'un a +0x38 non nul | ? |
| 32 | 0044bec0 | CEntity | CEntity | retourne une constante flottante (0x8ff34c), distance de dessin par défaut ? | ? |
| 33 | 00467ce0 | CEntity | CEntity | vrai si type != 5 et drapeau 0x10000 du modelinfo (+0x28) | ? |
| 34 | 004b9ea0 | CPhysical | CBike | `ProcessEntityCollision` ? | ? |
| 35–40 | | CPhysical | CPhysical | extensions physiques propres à Bully | ? |
| 41 | 004c3ae0 | CPhysical | CBike | ? | ? |
| 42 | 0049a500 | CVehicle | CBike | `ProcessControlInputs(uint8)` : vide dans CVehicle, même position que reVC | *probable* |
| 43 | 004358b0 | CVehicle | CBike | `GetComponentWorldPosition(int, CVector&)` : vide dans CVehicle | *probable* |
| 44 | 006094f0 | CVehicle | CBike | `IsComponentPresent(int)` : retourne faux dans CVehicle | *probable* |
| 45 | 0044bf10 | CVehicle | CBike | `SetComponentRotation(int, CVector)` : vide | *probable* |
| 46 | 0044bf20 | CVehicle | CVehicle | `OpenDoor(...)` : vide | ? |
| 47 | 00828240 | CVehicle | CVehicle | `ProcessOpenDoor(...)` : vide | ? |
| 48 | 0085b353 | CVehicle | CBike | virtuelle pure dans CVehicle, porte ou état de porte | ? |
| 49 | 006094f0 | CVehicle | CBike | retourne faux dans CVehicle (`IsDoor...`) | ? |
| 50 | 004358b0 | CVehicle | CBike | vide dans CVehicle | ? |
| 51 | 004cc8c0 | CVehicle | CVehicle | `SetUpWheelColModel(CColModel*)` ? : 405 octets, lit la boîte englobante du modelinfo (+0xc) et prend un flottant | ? |
| 52 | 0044bf30 | CVehicle | CVehicle | appelle 0x4ce4f0 (`PlayCarHorn` ou `BlowUpCar` ?) | ? |
| 53 | 0085b353 | CVehicle | CBike | virtuelle pure dans CVehicle | ? |
| 54 | 004cca60 | CVehicle | CBike | `GetHeightAboveRoad()` ? : hauteur de la boîte du colmodel (+0x18) × constante 0x900550, comme reVC | *probable* |
| 55 | 0049a500 | CVehicle | CBike | vide dans CVehicle | ? |
| 56 | 0044bf40 | CVehicle | CBike | accesseur : écrit +0x2a0 | ? |
| 57 | 004cb090 | CVehicle | CBike | retourne une constante flottante (0x912fd0), CBike la redéfinit | ? |
| 58 | 004cb2b0 | CVehicle | CVehicle | (int a, int b) : si a et b valides, relie le squelette (+0x114) à celui d'une autre entité via 0x6c1f10 ; attache d'un passager ? | ? |
| 59 | 0085b353 | CVehicle | CBike | virtuelle pure dans CVehicle | ? |

Vue d'ensemble : les 17 virtuelles de reVC sont toutes là, dans le même ordre, aux slots 0, 2, 3, 8 à 21. Bully en intercale 5 avant `Add` (1, 4 à 7) et en ajoute 12 après (22 à 33).

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
