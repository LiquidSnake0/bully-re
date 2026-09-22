// CPedStats, hérité de GTA mais avec un fichier bien plus riche : 65
// colonnes, nombre d'entrées libre. Voir docs/pedstats.md.
// bully.exe : Initialise 0x49a0a0, Reload 0x49a180, ParseLine 0x499d80.
#pragma once
#include "../common.h"

enum {
	PEDSTAT_NAME_LEN = 24,
	PEDSTAT_NUM_FIELDS = 64        // colonnes B à BM
};

// Indices des colonnes textuelles dans m_fields (colonne B = 0).
enum ePedStatField {
	PEDSTAT_PICKUP = 0,            // B  : nom de pickup, "none" → -1, sinon index de modèle
	PEDSTAT_CHARACTER_CLASS = 0x11,// S  : classe ("Generic", "Ranged"…) → id
	PEDSTAT_NIGHT_WEAPON = 0x28,   // AP : nom d'arme → id
	PEDSTAT_BIKE_1 = 0x29,         // AQ, AR, AS : modèle de vélo → index, -1 → 0
	PEDSTAT_BIKE_2 = 0x2a,
	PEDSTAT_BIKE_3 = 0x2b,
	PEDSTAT_WEAPON1_TYPE = 0x2d,   // AU, AY, BC, BG : nom d'arme → id
	PEDSTAT_WEAPON1_MISSION = 0x30,// AX, BB, BF, BJ : "init" → -1, sinon id de mission
	PEDSTAT_WEAPON2_TYPE = 0x31,
	PEDSTAT_WEAPON2_MISSION = 0x34,
	PEDSTAT_WEAPON3_TYPE = 0x35,
	PEDSTAT_WEAPON3_MISSION = 0x38,
	PEDSTAT_WEAPON4_TYPE = 0x39,
	PEDSTAT_WEAPON4_MISSION = 0x3c
};

struct CPedStat {
	char m_name[PEDSTAT_NAME_LEN];       // +0x00
	uint32 m_nameHash;                   // +0x18
	int32 m_fields[PEDSTAT_NUM_FIELDS];  // +0x1c, 0x11c octets au total
};

class CPedStats
{
public:
	static CPedStat *ms_apPedStats;      // tableau alloué par GameMalloc
	static int32 ms_numPedStats;

	static void Initialise(void);        // 0x49a0a0
	static void Reload(void);            // 0x49a180
	static void ParseLine(CPedStat *stat, char *line);   // 0x499d80
};

// Tables de correspondance nom → identifiant utilisées par ParseLine.
// Recréées plus tard ; les tests hôtes fournissent une version de secours.
int32 GetModelIndexByName(const char *name);      // 0x51c1e0, -1 si inconnu
int32 GetCharacterClassId(const char *name);      // 0x488a00
int32 GetWeaponIdByName(const char *name);        // 0x51ad50
int32 GetMissionIdByName(const char *name);       // 0x5fa7b0 puis 0x6a9e70
