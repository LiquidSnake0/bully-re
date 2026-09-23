// CWorld : grille de secteurs et listes globales. Voir docs/world.md.
#include "World.h"
#include "../entities/Physical.h"

CSector CWorld::ms_aSectors[NUMSECTORS_Y][NUMSECTORS_X];   // 0xc1b178 .. 0xc216b8
CPtrList CWorld::ms_bigBuildingsList;                       // 0xc216b8
CPtrList CWorld::ms_listMovingEntityPtrs;                   // 0xc1aea4
CPtrNode *CWorld::ms_pMovingListCursor;                     // 0xc1ae84
int32 CWorld::ms_nCurrentScanCode = -1;                     // 0xc1ae88
CEntity *CWorld::ms_apIgnoreEntities[7];                    // 0xc1aa64
int32 CWorld::ms_nNumIgnoreEntities;                        // 0xc1ae68

uint8 CWorld::ms_flags6c[5];    // 0xc1ae6c .. 0xc1ae70
uint8 CWorld::ms_flags74[3];    // 0xc1ae74 .. 0xc1ae76
uint8 CWorld::ms_flags80[3];    // 0xc1ae80 .. 0xc1ae82

extern void WorldSubsystemInit(void);   // 0x4397a0 (thunk)
extern void AreaRemoveExtraScene(void); // 0x4297f0, aussi liaison Lua

// 0x0045d430 : remise à zéro des drapeaux du monde, comme dans reVC
// (pIgnoreEntity, bDoingCarCollisions, bSecondShift…), puis initialisation
// d'un sous-système en 0x4397a0 (tas 0x1e, crée un pool de 0x1c octets en
// 0xbd5c4c) et `AreaRemoveExtraScene`. Appelée par 0x42ee90 sous
// « CWorld::Initialise() », tas 2.
void
CWorld::Initialise(void)
{
	ms_flags74[2] = ms_flags74[1] = ms_flags74[0] = 0;
	ms_flags6c[3] = ms_flags6c[2] = ms_flags6c[1] = ms_flags6c[0] = 0;
	ms_flags80[0] = ms_flags80[1] = ms_flags80[2] = 0;
	ms_flags6c[4] = 0;
	ms_nNumIgnoreEntities = 0;
	WorldSubsystemInit();
	AreaRemoveExtraScene();
}

// 0x0045d480
void
CWorld::ClearIgnoreEntities(void)
{
	ms_nNumIgnoreEntities = 0;
}

// 0x0045d490 : au plus sept entrées.
void
CWorld::AddIgnoreEntity(CEntity *ent)
{
	if(ms_nNumIgnoreEntities < 7)
		ms_apIgnoreEntities[ms_nNumIgnoreEntities++] = ent;
}

// Switch de CEntity::Add (0x465d60) et CEntity::Remove (0x4677a0).
// CPhysical::Add (0x469090) n'a que les cas 2, 3, 4/6/7 : un CPhysical de
// type bâtiment ou dummy n'est inscrit nulle part.
int32
CWorld::GetSectorListForType(int32 type)
{
	switch(type){
	case ENTITY_TYPE_BUILDING: return ENTITYLIST_BUILDINGS;
	case ENTITY_TYPE_VEHICLE:  return ENTITYLIST_VEHICLES;
	case ENTITY_TYPE_PED:      return ENTITYLIST_PEDS;
	case ENTITY_TYPE_OBJECT:
	case ENTITY_TYPE_6:
	case ENTITY_TYPE_7:        return ENTITYLIST_OBJECTS;
	case ENTITY_TYPE_DUMMY:    return ENTITYLIST_DUMMIES;
	default:                   return -1;
	}
}

// Le rectangle est lu dans l'ordre du binaire : xstart = left, ystart = top
// (y min), xend = right, yend = bottom (y max).
void
CWorld::AddToSectorLists(const CRect &bounds, int32 listId, void *item, CEntryInfoList *entries)
{
	if(listId < 0) return;
	int32 xstart = GetSectorIndexX(bounds.left);
	int32 ystart = GetSectorIndexY(bounds.top);
	int32 xend   = GetSectorIndexX(bounds.right);
	int32 yend   = GetSectorIndexY(bounds.bottom);

	for(int32 y = ystart; y <= yend; y++)
		for(int32 x = xstart; x <= xend; x++){
			CSector *s = GetSector(x, y);
			CPtrList *list = &s->m_lists[listId];
			CPtrNode *node = list->InsertItem(item);   // 0x44dee0, 0x44c7e0, chaînage en tête
			if(entries){
				CEntryInfoNode *entry = entries->InsertItem();   // 0x4296b0
				entry->list = list;
				entry->listnode = node;
				entry->sector = s;
			}
		}
}

void
CWorld::RemoveFromSectorLists(const CRect &bounds, int32 listId, void *item)
{
	if(listId < 0) return;
	int32 xstart = GetSectorIndexX(bounds.left);
	int32 ystart = GetSectorIndexY(bounds.top);
	int32 xend   = GetSectorIndexX(bounds.right);
	int32 yend   = GetSectorIndexY(bounds.bottom);

	for(int32 y = ystart; y <= yend; y++)
		for(int32 x = xstart; x <= xend; x++){
			CPtrList *list = &GetSector(x, y)->m_lists[listId];
			CPtrNode *node = list->FindItem(item);
			if(node)
				list->DeleteNode(node);              // 0x44dde0 puis 0x44def0
		}
}

// 0x0046a620 (CPhysical::Remove), aussi la fin de CPhysical::RemoveAndAdd.
void
CWorld::RemoveEntriesFromSectorLists(CEntryInfoList &entries)
{
	CEntryInfoNode *node, *next;
	for(node = entries.first; node; node = next){
		next = node->next;
		node->list->DeleteNode(node->listnode);      // 0x44dde0 / 0x44def0
		entries.DeleteNode(node);                    // décrochage puis 0x4296c0
	}
}

// 0x0045d560. Comme reVC : une grosse entité (+0x58, bIsBIGBuilding) va
// dans ms_bigBuildingsList au lieu de la grille, sinon slot 2 (Add). Puis,
// si l'entité n'est pas statique (+0x28 == 0), n'attend pas sa collision
// (+0xac == 0) et a +0xf0 non nul, elle rejoint la liste des mobiles.
void
CWorld::Add(CEntity *ent)
{
	if(ent == nil) return;
	if(ent->bIsBIGBuilding)
		ms_bigBuildingsList.InsertItem(ent);
	else
		ent->Add();
	if(!ent->bIsStatic && !ent->bIsStaticWaitingForCollision && ent->m_field0xf0 != 0)
		((CPhysical*)ent)->AddToMovingList();        // 0x469680
}

// 0x0045dc20. Symétrique : retrait de la grosse liste par parcours, sinon
// slot 3 (Remove) ; puis retrait de la liste des mobiles si +0xf0 ; enfin,
// si le slot 33 (drapeau 0x10000 du modelinfo) répond vrai, 0x5e6830(ent).
void
CWorld::Remove(CEntity *ent)
{
	if(ent == nil) return;
	if(ent->bIsBIGBuilding)
		ms_bigBuildingsList.RemoveItem(ent);
	else
		ent->Remove();
	if(ent->m_field0xf0 != 0)
		((CPhysical*)ent)->RemoveFromMovingList();   // 0x4696f0
	if(ent->HasModelFlag10000()){
		// TODO : 0x5e6830(ent), désinscription d'un gestionnaire (effets ? sons ?)
	}
}

// 0x0045d6f0 : m_scanCode (+0x10a) = 0 pour chaque entité des cinq listes
// de chaque secteur. Ordre du binaire : bâtiments, véhicules, piétons,
// objets, dummies.
void
CWorld::ClearScanCodes(void)
{
	static const int32 order[NUMSECTORENTITYLISTS] = {
		ENTITYLIST_BUILDINGS, ENTITYLIST_VEHICLES, ENTITYLIST_PEDS, ENTITYLIST_OBJECTS, ENTITYLIST_DUMMIES
	};
	for(int32 y = 0; y < NUMSECTORS_Y; y++)
		for(int32 x = 0; x < NUMSECTORS_X; x++){
			CSector *s = GetSector(x, y);
			for(int32 l = 0; l < NUMSECTORENTITYLISTS; l++)
				for(CPtrNode *node = s->m_lists[order[l]].first; node; node = node->next)
					((CEntity*)node->item)->m_scanCode = 0;
		}
}

// En ligne dans 0x45dd10 (recherche d'entité par modèle autour d'un point).
void
CWorld::AdvanceCurrentScanCode(void)
{
	if(ms_nCurrentScanCode == -1){
		ClearScanCodes();
		ms_nCurrentScanCode = 1;
	}else
		ms_nCurrentScanCode++;
}

// 0x00451460 (re3 : CRenderer::SortBIGBuildingsForSectorList) : chaque
// entité marquée bIsBIGBuilding (+0x58) est décrochée puis remise en tête.
void
CWorld::SortBIGBuildingsForSectorList(CPtrList *list)
{
	CPtrNode *node, *next;
	for(node = list->first; node; node = next){
		next = node->next;
		if(((CEntity*)node->item)->bIsBIGBuilding){
			list->RemoveNode(node);      // 0x44dde0
			list->InsertNode(node);
		}
	}
}

// 0x00452930, via 0x4529b0 (« Find big buildings » dans 0x42ee90, après
// un 0x5f2da0(0x28)). Parcourt les 36 × 36 secteurs, les cinq listes.
void
CWorld::SortBIGBuildings(void)
{
	for(int32 y = 0; y < NUMSECTORS_Y; y++)
		for(int32 x = 0; x < NUMSECTORS_X; x++)
			for(int32 l = 0; l < NUMSECTORENTITYLISTS; l++)
				SortBIGBuildingsForSectorList(&GetSector(x, y)->m_lists[l]);
}
