// Le monde en secteurs, hérité de GTA (reVC : core/World.h). Voir docs/world.md.
//
// Grille : 36 × 36 secteurs de 50 m, soit un monde de [-900, 900[ en x et
// en y. Les constantes sont en .rdata : 50.0 (double, 0x900aa0), 18.0
// (double, 0x900a98) et 35.0 (float, 0x900a94). La conversion est en ligne
// dans chaque fonction qui touche la grille (CEntity::Add 0x465d60,
// CPhysical::Add 0x469090, CEntity::Remove 0x4677a0, 0x45dd10…) :
//   indice = _ftol(coord / 50.0 + 18.0), borné à [0, 35]
// 0x85c6f0 est `_ftol` du CRT, pas une fonction du monde.
//
// Un CSector fait cinq CPtrList (20 octets), pas de listes « overlap » :
// une entité qui chevauche plusieurs secteurs est inscrite dans chacun.
// Le tableau commence à 0xc1b178 (Ghidra nomme 0xc1b17c, la deuxième liste
// du premier secteur) et finit à 0xc216b8 (36 × 36 × 20 = 0x6540), où
// commence ms_bigBuildingsList.
#pragma once
#include "../common.h"
#include "Lists.h"

class CEntity;

#define SECTOR_SIZE_X (50.0f)      // 0x900aa0
#define SECTOR_SIZE_Y (50.0f)

#define NUMSECTORS_X (36)          // 0x24, pas d'une rangée dans CPhysical::Add
#define NUMSECTORS_Y (36)          // 0x452930 parcourt 0xc1b178 .. 0xc216b8

#define WORLD_SIZE_X (NUMSECTORS_X * SECTOR_SIZE_X)
#define WORLD_SIZE_Y (NUMSECTORS_Y * SECTOR_SIZE_Y)

#define WORLD_MIN_X (-900.0f)      // -18 secteurs (0x900a98)
#define WORLD_MIN_Y (-900.0f)

#define WORLD_MAX_X (WORLD_MIN_X + WORLD_SIZE_X)
#define WORLD_MAX_Y (WORLD_MIN_Y + WORLD_SIZE_Y)

// Ordre des listes d'un secteur, d'après le switch sur m_type de
// CEntity::Add (0x465d60) : type 1 → 0, types 4/6/7 → 1, 2 → 2, 3 → 3, 5 → 4.
enum {
	ENTITYLIST_BUILDINGS,
	ENTITYLIST_OBJECTS,
	ENTITYLIST_VEHICLES,
	ENTITYLIST_PEDS,
	ENTITYLIST_DUMMIES,

	NUMSECTORENTITYLISTS
};

class CSector
{
public:
	CPtrList m_lists[NUMSECTORENTITYLISTS];
};

class CWorld
{
public:
	static CSector ms_aSectors[NUMSECTORS_Y][NUMSECTORS_X];   // 0x00c1b178
	static CPtrList ms_bigBuildingsList;                       // 0x00c216b8 : entités hors grille (bIsBIGBuilding)
	static CPtrList ms_listMovingEntityPtrs;                   // 0x00c1aea4
	static CPtrNode *ms_pMovingListCursor;                     // 0x00c1ae84 : nœud en cours de parcours, avancé par RemoveFromMovingList
	static int32 ms_nCurrentScanCode;                          // 0x00c1ae88 : -1 = à remettre à zéro (ClearScanCodes)

	// Entités ignorées par les tests du monde (reVC : pIgnoreEntity, une seule).
	static CEntity *ms_apIgnoreEntities[7];                    // 0x00c1aa64
	static int32 ms_nNumIgnoreEntities;                        // 0x00c1ae68

	// Drapeaux remis à zéro par Initialise, à nommer (reVC : bNoMoreCollisionTorque,
	// bDoingCarCollisions, bSecondShift, bForceProcessControl…).
	static uint8 ms_flags6c[5], ms_flags74[3], ms_flags80[3];  // 0xc1ae6c.., 0xc1ae74.., 0xc1ae80..

	static float GetSectorX(float f) { return (float)((double)f / 50.0 + 18.0); }
	static float GetSectorY(float f) { return (float)((double)f / 50.0 + 18.0); }
	// En ligne dans le binaire : _ftol puis bornage à [0, 35].
	static int32 GetSectorIndexX(float f) {
		int32 i = (int32)((double)f / 50.0 + 18.0);
		if(i < 0) i = 0;
		if(i > NUMSECTORS_X-1) i = NUMSECTORS_X-1;
		return i;
	}
	static int32 GetSectorIndexY(float f) {
		int32 i = (int32)((double)f / 50.0 + 18.0);
		if(i < 0) i = 0;
		if(i > NUMSECTORS_Y-1) i = NUMSECTORS_Y-1;
		return i;
	}
	static float GetWorldX(int32 x) { return x*SECTOR_SIZE_X + WORLD_MIN_X; }
	static float GetWorldY(int32 y) { return y*SECTOR_SIZE_Y + WORLD_MIN_Y; }
	// &ms_aSectors[0][0] + (x + y*0x24)*5 mots, comme dans CPhysical::Add.
	static CSector *GetSector(int32 x, int32 y) { return &ms_aSectors[y][x]; }
	static CPtrList &GetBigBuildingList(void) { return ms_bigBuildingsList; }
	static CPtrList &GetMovingEntityList(void) { return ms_listMovingEntityPtrs; }

	static void Initialise(void);                  // 0x45d430
	static void Add(CEntity *ent);                 // 0x45d560
	static void Remove(CEntity *ent);              // 0x45dc20
	static void ClearScanCodes(void);              // 0x45d6f0
	static void AdvanceCurrentScanCode(void);      // en ligne dans 0x45dd10 : -1 → ClearScanCodes et 1, sinon +1
	static void ClearIgnoreEntities(void);         // 0x45d480
	static void AddIgnoreEntity(CEntity *ent);     // 0x45d490
	static void SortBIGBuildings(void);            // 0x452930 (CRenderer::SortBIGBuildings dans re3), « Find big buildings »
	static void SortBIGBuildingsForSectorList(CPtrList *list);   // 0x451460

	// Liste du secteur pour un type d'entité (switch de 0x465d60), -1 sinon.
	static int32 GetSectorListForType(int32 type);
	// Corps commun, en ligne dans le binaire, de CEntity::Add (0x465d60),
	// CDummy::Add (0x465050) et CPhysical::Add (0x469090) : un nœud dans la
	// liste `listId` de chaque secteur couvert par `bounds` ; si `entries`
	// est donné, une entrée CEntryInfoNode par secteur.
	static void AddToSectorLists(const CRect &bounds, int32 listId, void *item, CEntryInfoList *entries);
	// CEntity::Remove (0x4677a0) : retire `item` de la liste `listId` de
	// chaque secteur couvert, par parcours.
	static void RemoveFromSectorLists(const CRect &bounds, int32 listId, void *item);
	// CPhysical::Remove (0x46a620) : retire chaque entrée puis la détruit.
	static void RemoveEntriesFromSectorLists(CEntryInfoList &entries);
};
