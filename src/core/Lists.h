// Listes chaînées du monde, héritées de GTA (re3 : core/Lists.h).
// Dans bully.exe : un CPtrNode fait 4 mots (0x44dee0 alloue 4 ; le premier
// mot encode l'objet et l'index de liste), un CEntryInfoNode fait 5 mots
// (0x4296b0 alloue 0x14) : list, listnode, sector, prev, next.
#pragma once
#include "../common.h"

class CEntity;
class CSector;

class CPtrNode
{
public:
	void *item;
	CPtrNode *prev;
	CPtrNode *next;
};

class CPtrList
{
public:
	CPtrNode *first;

	CPtrList(void) : first(nil) {}
	CPtrNode *InsertItem(void *item);      // 0x44dee0 + chaînage
	void DeleteNode(CPtrNode *node);       // 0x44dde0 / 0x44def0
};

class CEntryInfoNode
{
public:
	CPtrList *list;
	CPtrNode *listnode;
	CSector *sector;
	CEntryInfoNode *prev;
	CEntryInfoNode *next;
};

class CEntryInfoList
{
public:
	CEntryInfoNode *first;

	CEntryInfoList(void) : first(nil) {}
	CEntryInfoNode *InsertItem(void);      // 0x4296b0, inséré en tête
	void DeleteNode(CEntryInfoNode *node); // 0x4296c0
};
