# Les pools

`CPools::Initialise` (0x44d1d0) alloue 28 pools. Un pool est un objet de
0x1c octets : entrées, drapeaux (un octet par entrée, bit 7 = libre), taille,
taille d'entrée, pointeur d'allocation, deux booléens. Même disposition que
re3, avec la taille d'entrée stockée. La classe des éléments vient du
constructeur passé à `vector constructor iterator` (les entrées du pool de
piétons sont construites en `CPlayerPed`, les véhicules en `CAutomobile`).

| Globale | Nom | Entrées | Taille d'entrée | Classe des éléments |
|---|---|---|---|---|
| 0x00c0f5e8 | `ms_pPtrNodePool` | 15000 | 0x4 (4) | ? |
| 0x00c0f5ec | `ms_pEntryInfoNodePool` | 2000 | 0x14 (20) | ? |
| 0x00c0f5f4 | `ms_pVehiclePool` | 15 | 0x8c0 (2240) | CAutomobile |
| 0x00c0f5f0 | `ms_pPedPool` | 24 | 0x1f44 (8004) | CPlayerPed |
| 0x00c0f628 | `ms_pEffectProxyAttachPool` | 24 | 0x74 (116) | EffectProxyAttach |
| 0x00c0f62c | `ms_pPool62c` | 24 | 0x8 (8) | ? |
| 0x00c0f630 | `ms_pAttitudeSetPool` | 24 | 0x38 (56) | CAttitudeSet |
| 0x00c0f634 | `ms_pPool634` | 24 | 0x5c0 (1472) | ? |
| 0x00c0f638 | `ms_pPool638` | 24 | 0xe0 (224) | ? |
| 0x00c0f63c | `ms_pPool63c` | 24 | 0x20 (32) | ? |
| 0x00c0f640 | `ms_pIKBlendDriverGroupPool` | 24 | 0x200 (512) | IKBlendDriverGroup |
| 0x00c0f644 | `ms_pJointConstraintPool` | 48 | 0x80 (128) | JointConstraint |
| 0x00c0f648 | `ms_pFloorMotionDriverPool` | 24 | 0x40 (64) | FloorMotionDriver |
| 0x00c0f64c | `ms_pReachDriverPool` | 48 | 0x50 (80) | ReachDriver |
| 0x00c0f654 | `ms_pPool654` | 57 | 0x28 (40) | ? |
| 0x00c0f650 | `ms_pPool650` | 8 | 0x2b0c (11020) | ? |
| 0x00c0f600 | `ms_pDummyPool` | 300 | 0x128 (296) | CDummy |
| 0x00c0f608 | `ms_pPropAnimPool` | 220 | 0x3b8 (952) | CPropAnim |
| 0x00c0f5f8 | `ms_pBuildingPool` | 2250 | 0x120 (288) | CBuilding |
| 0x00c0f5fc | `ms_pTreadablePool` | 1 | 0x120 (288) | CTreadable |
| 0x00c0f60c | `ms_pAccessoryPool` | 48 | 0x2e8 (744) | CAccessory |
| 0x00c0f604 | `ms_pColModelPool` | 4150 | 0x40 (64) | ? |
| 0x00c0f610 | `ms_pPool610` | 87 | 0x2c (44) | ? |
| 0x00c0f614 | `ms_pObjectPool` | 275 | 0x22c (556) | CObject |
| 0x00c0f618 | `ms_pProjectilePool` | 35 | 0x24c (588) | CProjectile |
| 0x00c0f61c | `ms_pCutsceneObjectPool` | 30 | 0x33c (828) | CCutsceneObject |
| 0x00c0f620 | `ms_pSFXItemPool` | 48 | 0x74 (116) | SFXItem |
| 0x00c0f624 | `ms_pPool624` | 200 | 0x64 (100) | ? |

Tailles d'objets qui en découlent : `CEntity` / `CBuilding` 0x120 (288 octets),
`CDummy` 0x128, `CObject` 0x22c, `CProjectile` 0x24c, `CCutsceneObject` 0x33c,
`CPropAnim` 0x3b8, `CAutomobile` 0x8c0, `CPlayerPed` 0x1f44 (8004). Les pools
de pilotes d'animation (`IKBlendDriverGroup`, `JointConstraint`,
`FloorMotionDriver`, `ReachDriver`) sont propres à Bully. Le pool de 4150 ×
0x40 (0xc0f604) est vraisemblablement celui des modèles de collision (reVC :
4400). Les pools sans classe résolue construisent leurs entrées par un
constructeur qui n'écrit pas de table virtuelle.
