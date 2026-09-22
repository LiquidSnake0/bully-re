// CPedStats, recréé depuis bully.exe (0x49a0a0, 0x49a180, 0x499d80).
#include "PedStats.h"
#include "../core/FileMgr.h"
#include "../core/ModelInfo.h"
#include <cstdio>
#include <cstring>
#include <strings.h>

extern void *GameMalloc(size_t size);          // 0x5ee6d0
extern uint32 HashString(const char *s);       // 0x576d80

CPedStat *CPedStats::ms_apPedStats = nil;
int32 CPedStats::ms_numPedStats = 0;

static bool
LigneUtile(const char *line)
{
	return line[0] != '\0' && line[0] != '#';
}

// 0x0049a0a0 : compte les lignes utiles, alloue, relit et analyse.
void
CPedStats::Initialise(void)
{
	CMemoryHeap::Push(0x1f);
	int32 fd = CFileMgr::OpenFile("Config\\Dat\\PEDSTATS.DAT", "r", 1);
	char *line;
	ms_numPedStats = 0;
	while((line = CFileLoader::LoadLine(fd)) != nil)
		if(LigneUtile(line)) ms_numPedStats++;
	ms_apPedStats = (CPedStat*)GameMalloc(sizeof(CPedStat) * ms_numPedStats);
	if(ms_apPedStats != nil)
		for(int32 i = 0; i < ms_numPedStats; i++)
			memset(&ms_apPedStats[i], 0, sizeof(CPedStat));      // constructeur 0x499d50
	CFileMgr::Seek(fd, 0, 0);
	int32 n = 0;
	while((line = CFileLoader::LoadLine(fd)) != nil)
		if(LigneUtile(line)) ParseLine(&ms_apPedStats[n++], line);
	CFileMgr::CloseFile(fd);
	CMemoryHeap::Pop();
}

// 0x0049a180 : relit dans les entrées existantes.
void
CPedStats::Reload(void)
{
	CMemoryHeap::Push(0x1f);
	int32 fd = CFileMgr::OpenFile("Config\\Dat\\PEDSTATS.DAT", "r", 1);
	char *line;
	int32 n = 0;
	while((line = CFileLoader::LoadLine(fd)) != nil)
		if(LigneUtile(line)) ParseLine(&ms_apPedStats[n++], line);
	CFileMgr::CloseFile(fd);
	CMemoryHeap::Pop();
}

// 0x00499d80. La ligne arrive de LoadLine avec tabulations et virgules
// déjà changées en espaces. Le nom, puis 64 champs : entiers par défaut,
// et quelques colonnes textuelles converties en identifiants.
void
CPedStats::ParseLine(CPedStat *stat, char *line)
{
	char word[32];
	char *p = line;
	sscanf(p, "%s", stat->m_name);
	stat->m_nameHash = HashString(stat->m_name);

	for(int32 i = 0; i < PEDSTAT_NUM_FIELDS; i++){
		// sauter le mot courant puis les espaces
		while(*p != '\0' && *p != ' ') p++;
		while(*p == ' ') p++;
		if(*p == '\0') break;      // le binaire continue avec sscanf sur une chaîne vide ; même résultat
		int32 *field = &stat->m_fields[i];
		switch(i){
		case PEDSTAT_PICKUP:
			sscanf(p, "%s", word);
			*field = strncmp(word, "none", 5) == 0 ? -1 : GetModelIndexByName(word);
			break;
		case PEDSTAT_CHARACTER_CLASS:
			sscanf(p, "%s", word);
			GetCharacterClassId(word);                // appelé deux fois dans le binaire
			*field = GetCharacterClassId(word);
			break;
		case PEDSTAT_NIGHT_WEAPON:
		case PEDSTAT_WEAPON1_TYPE: case PEDSTAT_WEAPON2_TYPE:
		case PEDSTAT_WEAPON3_TYPE: case PEDSTAT_WEAPON4_TYPE:
			sscanf(p, "%s", word);
			*field = GetWeaponIdByName(word);
			break;
		case PEDSTAT_BIKE_1: case PEDSTAT_BIKE_2: case PEDSTAT_BIKE_3: {
			sscanf(p, "%s", word);
			int32 id = GetModelIndexByName(word);
			*field = id == -1 ? 0 : id;
			break; }
		case PEDSTAT_WEAPON1_MISSION: case PEDSTAT_WEAPON2_MISSION:
		case PEDSTAT_WEAPON3_MISSION: case PEDSTAT_WEAPON4_MISSION:
			sscanf(p, "%s", word);
			*field = strcasecmp(word, "init") == 0 ? -1 : GetMissionIdByName(word);
			break;
		default:
			sscanf(p, "%d", field);
			break;
		}
	}
}
