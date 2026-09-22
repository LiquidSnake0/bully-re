// CObjectData, hérité de GTA par le nom seulement : le fichier de Bully
// (Richard Jobling, 3/2/2000, 22 colonnes) décrit les points de vie, les
// effets de destruction, les sons et les butes de ramassage d'un modèle.
// bully.exe : Initialise 0x4d0c90, table 0x00c35200 (entrées de 0x38),
// table d'alias 0x00c34f58 (paires de shorts), compteurs 0xc34f54 / 0xc34f50.
#pragma once
#include "../common.h"

enum {
	NUM_OBJECT_DATA = 336,       // (0xc39b70 - 0xc35200) / 0x38
	NUM_OBJECT_ALIASES = 170,    // (0xc35200 - 0xc34f58) / 4
	OBJECT_NAME_BUF = 0x100
};

// Tables de volume (0x4d0a60) : small 1, medium 2, large 3, speech 4,
// extralarge 5, jumbo 8, supersize 9, generic 0, autre 1.
enum eVolumeTable { VOL_GENERIC = 0, VOL_SMALL = 1, VOL_MEDIUM = 2, VOL_LARGE = 3, VOL_SPEECH = 4, VOL_EXTRALARGE = 5, VOL_JUMBO = 8, VOL_SUPERSIZE = 9 };

struct CObjectInfo {
	uint16 hitPointsAndLock;       // +0x00 : points de vie sur 15 bits, ManualLockTargetable en bit 15
	uint8 destroyByWeaponOnly;     // +0x02 : bit 0
	uint8 pad03;
	uint16 destroyedEffect1;       // +0x04 : nom d'effet → id (0x667a00), None → 0xffff
	uint16 destroyedEffect2;       // +0x06
	int16 effectZOffset1;          // +0x08 : flottant tronqué
	int16 effectZOffset2;          // +0x0a
	uint16 hitEffect;              // +0x0c
	uint16 soundBank;              // +0x0e : nom de banque → id (0x597e70)
	int32 soundLoadRangeSq;        // +0x10 : portée au carré
	uint32 idleSound;              // +0x14 : son (0x595a30) ou flux .rsm (0x5a7c10)
	uint32 breakSound;             // +0x18
	uint32 hitSound;               // +0x1c
	uint8 volumeTables;            // +0x20 : table idle sur 4 bits, table dégâts << 4
	uint8 idleVolumeAndStream;     // +0x21 : bit 0 = flux .rsm, bits 1.. = volume × 100 arrondi
	uint8 attachToEmitter;         // +0x22 : bit 0
	uint8 pad23;
	uint32 damageableClass;        // +0x24 : HashString du nom, défaut 0x1b654d4
	int32 modelIndex;              // +0x28 : modèle (0x51c040) ou nombre en mode préhaché
	uint32 pickupButes;            // +0x2c : 0x72ad50, défaut 0
	int16 pickupZOffset;           // +0x30 : défaut 0x33
	uint8 pad32[2];
	int32 weaponModelIndex;        // +0x34 : None → -1
};

struct CObjectAlias {
	int16 modelIndex;              // le modèle de la ligne courte
	int16 target;                  // d'abord l'index de modèle visé, puis l'index d'entrée après résolution
};

class CObjectData
{
public:
	static CObjectInfo ms_aObjectInfo[NUM_OBJECT_DATA];    // 0x00c35200
	static CObjectAlias ms_aAliases[NUM_OBJECT_ALIASES];   // 0x00c34f58
	static int32 ms_numObjectInfos;                        // 0x00c34f54
	static int32 ms_numAliases;                            // 0x00c34f50
	static bool ms_bPrehashedNames;                        // 0x00bf380b : les noms sont déjà des nombres
	static bool ms_bSkipPostInit;                          // 0x00bf3813
	static uint32 ms_defaultDamageableClass;               // 0x01b654d4

	static void Initialise(const char *filename);          // 0x4d0c90
	static uint8 ParseVolumeTable(const char *word);       // 0x4d0a60
};

// Tables de correspondance appelées par Initialise ; recréées plus tard,
// fournies par les tests hôtes en attendant.
int32 FindModelIndexByName(const char *name);              // 0x51c040 (via HashString et 0xc67738)
uint16 GetEffectIdByName(const char *name);                // 0x667a00
uint16 GetSoundBankIdByName(const char *name);             // 0x597e70
uint32 GetSoundIdByName(const char *name);                 // 0x595a30
uint32 GetStreamIdByName(const char *name, int32 flag);    // 0x5a7c10
uint32 GetPickupButesByName(const char *name);             // 0x5fa7b0 … 0x72ad50
void ObjectDataPostInit(void);                             // 0x4d0bc0
uint32 HashString(const char *s);                          // 0x576d80
