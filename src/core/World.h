// Le monde en secteurs, hérité de GTA. Dans bully.exe (CPhysical::Add,
// 0x469090) : la grille commence à 0x00c1b17c, un secteur fait cinq listes
// (cinq pointeurs, 20 octets), une rangée fait 0x24 = 36 secteurs.
// Le nombre de rangées et les bornes du monde restent à lire dans
// GetSectorIndexX/Y (0x85c6f0).
#pragma once
#include "../common.h"
#include "Lists.h"

enum {
	NUMSECTORS_X = 36,
	NUMSECTORS_Y = 36,     // hypothèse, à confirmer
	NUMSECTORLISTS = 5
};

class CSector
{
public:
	CPtrList m_lists[NUMSECTORLISTS];   // [0] objets (types 4, 6, 7), [1] véhicules, [2] piétons
};

class CWorld
{
public:
	static CSector ms_aSectors[NUMSECTORS_Y][NUMSECTORS_X];   // 0x00c1b17c

	static int GetSectorIndexX(float x);   // 0x85c6f0 (même fonction pour X et Y dans le binaire ?)
	static int GetSectorIndexY(float y);
	static CSector *GetSector(int x, int y) { return &ms_aSectors[y][x]; }
};
