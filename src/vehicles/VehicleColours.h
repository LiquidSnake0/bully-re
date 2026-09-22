// Couleurs des véhicules, héritées de GTA (reVC : CVehicleModelInfo).
// bully.exe : LoadVehicleColours 0x5355b0, table 0x00ce6d28 (256 × RGBA),
// dans le modelinfo : m_colours1 +0x1ac, m_colours2 +0x1b4, m_numColours +0x1bc.
#pragma once
#include "../common.h"

struct CRGBA { uint8 r, g, b, a; };

enum { NUM_VEHICLE_COLOURS = 256, MAX_VEHICLE_COLOUR_PAIRS = 8 };

class CVehicleModelInfo
{
public:
	static CRGBA ms_vehicleColourTable[NUM_VEHICLE_COLOURS];   // 0x00ce6d28
	static void LoadVehicleColours(void);                       // 0x5355b0

	uint8 m_colours1[MAX_VEHICLE_COLOUR_PAIRS];   // +0x1ac
	uint8 m_colours2[MAX_VEHICLE_COLOUR_PAIRS];   // +0x1b4
	uint8 m_numColours;                           // +0x1bc
};

// 0x51c0b0, avec deux globales (0xa136a8, 0xa136ac) : recherche d'un
// modelinfo de véhicule par nom. Recréée plus tard.
CVehicleModelInfo *FindVehicleModelInfoByName(const char *name);

// Objet fichier utilisé par ce chargeur (0x4264c0 ouvre, 0x42d4b0 lit une
// ligne dans un tampon de 1024 octets, l'objet est libéré à la fin).
class CFileStream
{
public:
	void *m_handle;
	bool Open(const char *path);
	bool ReadLine(char *buf, int32 size);
	void Close(void);
};
