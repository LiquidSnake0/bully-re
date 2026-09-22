// Définitions de modèles binaires « .idb » de Bully, compilées depuis les
// IDE texte de GTA et emballées dans Objects/ide.img. bully.exe :
// LoadImgIde 0x42caf0 lit l'archive entière, LoadIdeBinary 0x42c970 parcourt
// les sections. Format : dword = taille utile, puis des sections
// [tag 4 octets][données], le tag étant le nom de section GTA (« peds »,
// « objs », « cars », « weap », « cash », « scnd », « accs », « item »,
// « 2dfx », « panm », « clth », « path », « tobj ») stocké à l'envers.
// Après chaque section : deux dwords [premier id, dernier id], enregistrés
// par 0x52dc50 sauf si le premier vaut -1.
//
// Les chaînes sont sur des dwords : le lecteur consomme des dwords tant
// que l'octet de poids fort du dernier lu n'est pas nul.
#pragma once
#include "../common.h"
#include <cstring>

enum eIdeSection {
	IDE_OBJS = 0x6f626a73, IDE_TOBJ = 0x746f626a, IDE_WEAP = 0x77656170, IDE_CASH = 0x63617368,
	IDE_SCND = 0x73636e64, IDE_ACCS = 0x61636373, IDE_ITEM = 0x6974656d, IDE_CARS = 0x63617273,
	IDE_PEDS = 0x70656473, IDE_2DFX = 0x32646678, IDE_PANM = 0x70616e6d, IDE_CLTH = 0x636c7468,
	IDE_PATH = 0x70617468
};

class CIdeReader
{
public:
	const uint8 *m_p;
	const uint8 *m_end;

	CIdeReader(const uint8 *data, uint32 size) : m_p(data), m_end(data + size) {}
	bool AtEnd(void) const { return m_p >= m_end; }
	int32 Int(void) { int32 v; memcpy(&v, m_p, 4); m_p += 4; return v; }
	// 0x42bfd0 et consorts : dwords jusqu'à celui dont l'octet haut est nul
	void String(char *dest, int32 size)
	{
		int32 n = 0;
		for(;;){
			uint32 w; memcpy(&w, m_p, 4); m_p += 4;
			if(n + 4 < size){ memcpy(dest + n, &w, 4); n += 4; }
			if((w >> 24) == 0) break;
		}
		dest[size - 1] = '\0';
		if(n < size) dest[n] = '\0';
	}
};

// Une ligne « peds » de l'IDE (voir l'en-tête de Objects/default.ide) :
// MODEL TXD FEMALE SIZE TYPE STAT ANIMGROUP1..4 UNIQUE ACTIONTREE ROOT/FILE,
// puis la racine et le fichier de l'arbre d'IA et le nom du personnage.
struct CPedIdeEntry {
	int32 id;
	char model[32], txd[32];
	int32 female;
	char size[16], type[24], stat[32];
	char animGroup[4][24];
	int32 unique;
	char actionRoot[48], actionFile[48], aiRoot[48], aiFile[48];
	char name[32];
};

class CIdeBinary
{
public:
	static int32 (*ms_pedHandler)(const CPedIdeEntry &e);   // appelé par entrée lue
	static int32 ms_numPeds;

	// Parcourt les sections tant qu'elles sont connues ; retourne faux à la
	// première section non encore recréée (seule « peds » l'est).
	static bool Load(const uint8 *data, uint32 size);      // 0x42c970
	static void LoadPeds(CIdeReader &r);                    // 0x42bfd0
};

void RegisterModelRange(uint16 first, uint32 count);       // 0x52dc50
