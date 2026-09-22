# Config/dat/object.dat

Le nom vient de GTA, le contenu est propre à Bully : 22 colonnes (en-tête
signé Richard Jobling, 3/2/2000), 372 lignes utiles dont 80 « alias » de six
champs qui renvoient un modèle vers l'entrée d'un autre (`PGGarbagecn01 …
DrmGarbagecn`). `CObjectData::Initialise` (0x4d0c90) remplit une table de 336
entrées de 0x38 octets en 0x00c35200 et une table de 170 alias en 0x00c34f58,
puis résout chaque alias vers l'index d'entrée du modèle visé.

| Colonne | Champ | Conversion |
|---|---|---|
| ModelName | +0x28 | `FindModelIndexByName` (0x51c040, haché puis recherche dans `ms_modelInfoPtrs`) |
| WeaponModelName | +0x34 | idem, `None` → -1 |
| HitPoints, ManualLockTargetable | +0x00 | 15 bits + bit 15 |
| DestroyByWeaponOnly | +0x02 | bit 0 |
| DestroyedEffect(1), (2), HitEffect | +0x04, +0x06, +0x0c | `GetEffectIdByName` (0x667a00), `None` → 0xffff |
| EffectZ-Offset(1), (2) | +0x08, +0x0a | flottant tronqué en short |
| SoundBankName | +0x0e | `GetSoundBankIdByName` (0x597e70) |
| SoundLoadRange | +0x10 | stockée au carré |
| IdleSoundName | +0x14 | `None` → 0 ; `x.rsm` → flux (0x5a7c10) et bit 0 de +0x21 ; `AttachToEmitter` → bit 0 de +0x22 ; sinon `GetSoundIdByName` (0x595a30) |
| IdleVolume | +0x21 | borné à 0..1, × 100 arrondi, décalé d'un bit |
| BreakSoundName, HitSoundName | +0x18, +0x1c | `GetSoundIdByName` |
| IdleSoundVolumeTable, DamageSoundVolumeTable | +0x20 | `ParseVolumeTable` (0x4d0a60) : small 1, medium 2, large 3, speech 4, extralarge 5, jumbo 8, supersize 9, generic 0 ; 4 bits chacune |
| [DamageableClassName] | +0x24 | `HashString`, défaut 0x1b654d4 |
| [PickupButesName] | +0x2c | recherche de butes (0x5fa7b0 … 0x72ad50) |
| [PickupZoffset] | +0x30 | flottant tronqué, défaut 0x33 |

La colonne « Vandalizable? » de l'en-tête n'est pas lue. Un drapeau global
(0xbf380b) fait lire des nombres à la place des noms de modèles. Le découpage
passe par `CTokenizer` (0x61a310…), propre à Bully, quatre délimiteurs.
Recréé dans `src/objects/ObjectData.cpp` et `src/core/Tokenizer.cpp`, vérifié
par `tests/test_objectdata` : 292 entrées, 80 alias, `DrmGarbagecn`,
`PGGarbagecn01 → 0`, `Aud_Speaker`.
