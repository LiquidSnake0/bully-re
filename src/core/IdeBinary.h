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
	float Float(void) { float v; memcpy(&v, m_p, 4); m_p += 4; return v; }
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

// Une ligne « objs » (id, dff, txd, nombre d'objets, distances, flags, puis
// des colonnes propres à Bully). Dans le binaire, un « type » 0..5 précède
// l'id : type/2 + 1 = nombre de distances de dessin ; les types pairs
// portent trois dwords de queue (octets écrits à +0x2d, +0x2e, +0x2f du
// modelinfo), les impairs non. Les 3 297 entrées de Objects/ide.img sont
// toutes de type 0. Le chargeur (0x42aa20) ne garde que la première
// distance (modelinfo +0x24), les flags (convertis par SetFlags) et les
// octets de queue ; les trois dwords après les flags ne sont pas lus.
struct CObjIdeEntry {
	int32 type;
	int32 id;
	char model[32], txd[32];
	int32 numObjs;
	float drawDist[3];
	uint32 flags;                   // flags IDE, avant conversion
	float unk1;                     // non lu par le jeu ; 0 sauf ~90 entrées
	float unk2;                     // non lu ; toujours 1.0
	int32 unk3;                     // non lu ; toujours 0
	int32 byte0b;                   // → modelinfo +0xb (255 partout)
	int32 byte2d, byte2e, byte2f;   // types pairs seulement ; → +0x2d, +0x2e, +0x2f
};

// Flags du modelinfo simple (+0x28), après conversion des flags IDE par
// 0x429d30 ; les noms viennent de reVC (eSimpleModelInfoFlags) quand le
// bit correspond, sinon de l'offset.
enum eSimpleModelInfoFlags {
	SMI_WET_ROAD_REFLECTION = 0x4,       // IDE 0x1
	SMI_NO_FADE             = 0x20,      // IDE 0x2
	SMI_DRAW_LAST           = 0x40,      // IDE 0x4 ou 0x8
	SMI_ADDITIVE            = 0x80,      // IDE 0x8
	SMI_SPECIAL             = 0x100,     // ids « spéciaux » (0x429e70)
	SMI_FLAG_IDE_40         = 0x400,
	SMI_FLAG_IDE_80         = 0x800,
	SMI_FLAG_IDE_100        = 0x1000,
	SMI_FLAG_IDE_200        = 0x2000,
	SMI_FLAG_IDE_400        = 0x4000,
	SMI_FLAG_IDE_1000       = 0x10000,
	SMI_FLAG_IDE_2000       = 0x20000,
	SMI_FLAG_IDE_10         = 0x40000,
	SMI_FLAG_IDE_4000       = 0x80000,
	SMI_FLAG_IDE_10000      = 0x200000,
	SMI_NOG_WALKABLE        = 0x1000000, // nom en nog_ / walkable_
	SMI_FLAG_IDE_20000      = 0x2000000
};

class CIdeBinary
{
public:
	static int32 (*ms_pedHandler)(const CPedIdeEntry &e);   // appelé par entrée lue
	static int32 (*ms_objHandler)(const CObjIdeEntry &e);
	static int32 ms_numPeds;
	static int32 ms_numObjs;

	// Parcourt les sections tant qu'elles sont connues ; retourne faux à la
	// première section non encore recréée (« peds » et « objs » le sont).
	static bool Load(const uint8 *data, uint32 size);      // 0x42c970
	static void LoadPeds(CIdeReader &r);                    // 0x42bfd0
	static void LoadObjs(CIdeReader &r);                    // 0x42aa20

	static uint32 ConvertFlags(uint32 ideFlags);            // 0x429d30 (bits posés dans +0x28)
	static bool IsSpecialObjectId(int32 id);                // 0x429e70
	static bool IsNogOrWalkable(const char *name);          // test en ligne dans 0x42aa20
};

void RegisterModelRange(uint16 first, uint32 count);       // 0x52dc50
