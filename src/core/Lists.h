// Listes chaînées du monde, héritées de GTA (re3 : core/Lists.h).
//
// Dans bully.exe un CPtrNode tient sur UN mot de 32 bits (pool
// ms_pPtrNodePool, 0xc0f5e8, entrées de 4 octets, base des entrées cachée
// dans la globale 0xc0f788) :
//   bits 0..3   : pool de l'entité (eEntityPool ci-dessous), 0xf = aucun
//   bits 4..17  : index de l'entité dans ce pool (14 bits)
//   bits 18..31 : index du nœud suivant dans ms_pPtrNodePool, 0x3fff = fin
// L'entité est retrouvée par CPools::GetEntity(pool, index) (0x44a290) et
// encodée par CPools::GetEntityPoolAndIndex (0x44c7e0) via CPool::GetIndex
// (0x44a5e0) ; CPtrNode::SetItem est 0x45d520. La liste est donc simplement
// chaînée : CPtrList::RemoveNode (0x44dde0) repart de la tête pour trouver
// le prédécesseur. Ici on garde des pointeurs, la sémantique est la même.
//
// Un CEntryInfoNode fait 5 mots (pool ms_pEntryInfoNodePool, 0xc0f5ec,
// 0x14 octets, alloué par 0x4296b0 → 0x5ec5d0, libéré par 0x4296c0) :
// list, listnode, sector, prev, next ; inséré en tête (CPhysical::Add).
#pragma once
#include "../common.h"

class CEntity;
class CSector;

// Pools de CPools::GetEntity (0x44a290), dans l'ordre du switch.
enum eEntityPool {
	POOL_PED = 0,              // 0xc0f5f0
	POOL_VEHICLE = 1,          // 0xc0f5f4
	POOL_OBJECT = 2,           // 0xc0f614
	POOL_PROJECTILE = 3,       // 0xc0f618 : type 4 avec +0xc4 non nul
	POOL_CUTSCENE_OBJECT = 4,  // 0xc0f61c : type 4 avec +0xec non nul
	POOL_DUMMY = 5,            // 0xc0f600
	POOL_PROP_ANIM = 6,        // 0xc0f608 : type 6
	POOL_BUILDING = 7,         // 0xc0f5f8 : type 1
	POOL_TREADABLE = 8,        // 0xc0f5fc : type 1 dont le slot 34 répond vrai
	POOL_ACCESSORY = 9,        // 0xc0f60c : type 7
	POOL_NONE = 0xf
};

class CPtrNode
{
public:
	void *item;
	CPtrNode *next;
};

class CPtrList
{
public:
	CPtrNode *first;

	CPtrList(void) : first(nil) {}
	CPtrNode *FindItem(void *item) const {
		for(CPtrNode *node = first; node; node = node->next)
			if(node->item == item)
				return node;
		return nil;
	}
	int32 CountItems(void) const {
		int32 n = 0;
		for(CPtrNode *node = first; node; node = node->next) n++;
		return n;
	}
	// Insertion en tête : c'est ce que font CEntity::Add, CPhysical::Add et
	// CRenderer::SortBIGBuildingsForSectorList (0x451460) en ligne.
	CPtrNode *InsertNode(CPtrNode *node) {
		node->next = first;
		first = node;
		return node;
	}
	CPtrNode *InsertItem(void *item);      // 0x44dee0 (nœud neuf) puis chaînage en tête
	void RemoveNode(CPtrNode *node);       // 0x44dde0 : décroche sans libérer
	void DeleteNode(CPtrNode *node);       // 0x44dde0 puis 0x44def0
	void RemoveItem(void *item);           // parcours en ligne dans CEntity::Remove (0x4677a0)
	void Flush(void);
};

// Où un CPhysical (ou un CDummy) est inscrit : une entrée par secteur.
class CEntryInfoNode
{
public:
	CPtrList *list;         // +0x0 : la liste du secteur
	CPtrNode *listnode;     // +0x4 : le nœud dans cette liste
	CSector *sector;        // +0x8
	CEntryInfoNode *prev;   // +0xc
	CEntryInfoNode *next;   // +0x10
};

class CEntryInfoList
{
public:
	CEntryInfoNode *first;

	CEntryInfoList(void) : first(nil) {}
	CEntryInfoNode *InsertNode(CEntryInfoNode *node) {
		node->prev = nil;
		node->next = first;
		if(first) first->prev = node;
		first = node;
		return node;
	}
	CEntryInfoNode *InsertItem(void);      // 0x4296b0, inséré en tête
	void RemoveNode(CEntryInfoNode *node) {
		if(first == node) first = node->next;
		if(node->prev) node->prev->next = node->next;
		if(node->next) node->next->prev = node->prev;
	}
	void DeleteNode(CEntryInfoNode *node); // décrochage en ligne puis 0x4296c0
	void Flush(void);
};
