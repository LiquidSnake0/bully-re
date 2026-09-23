// Listes du monde. Dans bully.exe les nœuds viennent des pools
// ms_pPtrNodePool (0xc0f5e8, mot compressé, voir Lists.h) et
// ms_pEntryInfoNodePool (0xc0f5ec) ; ici on alloue simplement.
#include "Lists.h"

CPtrNode *
CPtrList::InsertItem(void *item)
{
	CPtrNode *node = new CPtrNode;   // 0x44dee0 → CPool::New 0x44de70
	node->item = item;               // 0x45d520 : CPtrNode::SetItem
	return InsertNode(node);
}

// 0x0044dde0. Liste simplement chaînée : si le nœud n'est pas en tête on
// cherche son prédécesseur depuis la tête, puis on recopie son « suivant ».
void
CPtrList::RemoveNode(CPtrNode *node)
{
	if(first == node){
		first = node->next;
		return;
	}
	for(CPtrNode *prev = first; prev; prev = prev->next)
		if(prev->next == node){
			prev->next = node->next;
			return;
		}
}

void
CPtrList::DeleteNode(CPtrNode *node)
{
	RemoveNode(node);
	delete node;                     // 0x44def0 : remise dans ms_pPtrNodePool
}

// Corps en ligne dans CEntity::Remove (0x4677a0) : on parcourt la liste
// jusqu'à l'entité, puis RemoveNode + libération.
void
CPtrList::RemoveItem(void *item)
{
	CPtrNode *node, *next;
	for(node = first; node; node = next){
		next = node->next;
		if(node->item == item)
			DeleteNode(node);
	}
}

void
CPtrList::Flush(void)
{
	CPtrNode *node, *next;
	for(node = first; node; node = next){
		next = node->next;
		delete node;
	}
	first = nil;
}

CEntryInfoNode *
CEntryInfoList::InsertItem(void)
{
	CEntryInfoNode *node = new CEntryInfoNode;   // 0x4296b0
	node->list = nil; node->listnode = nil; node->sector = nil;
	return InsertNode(node);
}

void
CEntryInfoList::DeleteNode(CEntryInfoNode *node)
{
	RemoveNode(node);
	delete node;                     // 0x4296c0 : remise dans ms_pEntryInfoNodePool
}

void
CEntryInfoList::Flush(void)
{
	CEntryInfoNode *node, *next;
	for(node = first; node; node = next){
		next = node->next;
		delete node;
	}
	first = nil;
}
