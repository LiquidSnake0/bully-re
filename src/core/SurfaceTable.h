// Table des adhérences entre surfaces, héritée de GTA (re3 : core/SurfaceTable).
// Dans bully.exe : CSurfaceTable::Initialise en 0x45b250, appelée depuis le
// chargement initial (0x42ec80) avec "Config\Dat\SURFACE.DAT" ; la table
// est en 0x00c1a8f8, six groupes comme dans Vice City.
#pragma once
#include "../common.h"

enum eAdhesionGroup {
	ADHESIVE_RUBBER,
	ADHESIVE_HARD,
	ADHESIVE_ROAD,
	ADHESIVE_LOOSE,
	ADHESIVE_SAND,
	ADHESIVE_WET,
	NUMADHESIVEGROUPS
};

class CSurfaceTable
{
public:
	static float ms_aAdhesiveLimitTable[NUMADHESIVEGROUPS][NUMADHESIVEGROUPS];   // 0x00c1a8f8

	static void Initialise(const char *filename);   // 0x45b250
	static float GetAdhesiveLimit(int groupA, int groupB) { return ms_aAdhesiveLimitTable[groupB][groupA]; }
};
