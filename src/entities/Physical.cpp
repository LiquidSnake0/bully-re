// Recréation de CPhysical::Add, CPhysical::Remove et CPhysical::GetBoundRect
// à partir de bully.exe (0x469090, 0x46a620, 0x468f90). Le code suit la
// structure de reVC, dont Bully hérite ; les décalages sont ceux du binaire.
#include "Physical.h"
#include "../core/World.h"
#include "../core/Lists.h"

// 0x00469090, slot 2 de la table virtuelle.
// Insère l'entité dans chaque secteur couvert par son rectangle englobant.
// Différence avec reVC : la liste cible dépend de m_type via un switch
// (2 → liste +1, 3 → liste +2, 4/6/7 → liste 0), et chaque secteur garde
// cinq listes de 0x24 octets.
void
CPhysical::Add(void)
{
	CRect bounds = GetBoundRect();
	int xstart = CWorld::GetSectorIndexX(bounds.left);
	int xend   = CWorld::GetSectorIndexX(bounds.right);
	int ystart = CWorld::GetSectorIndexY(bounds.top);
	int yend   = CWorld::GetSectorIndexY(bounds.bottom);

	for(int y = ystart; y <= yend; y++)
		for(int x = xstart; x <= xend; x++){
			CSector *s = CWorld::GetSector(x, y);
			CPtrList *list;
			switch(m_type){
			case ENTITY_TYPE_VEHICLE: list = &s->m_lists[1]; break;   // 2
			case ENTITY_TYPE_PED:     list = &s->m_lists[2]; break;   // 3
			case ENTITY_TYPE_OBJECT:                                  // 4
			case 6:
			case 7:                   list = &s->m_lists[0]; break;
			default: continue;                                        // 0, 1, 5 : jamais insérés
			}
			CPtrNode *node = list->InsertItem(this);          // 0x44dee0 puis encodage
			CEntryInfoNode *entry = m_entryInfoList.InsertItem();  // 0x4296b0, 0x14 octets
			entry->list = list;
			entry->listnode = node;
			entry->sector = s;
		}
}

// 0x0046a620, slot 3. Identique à reVC.
void
CPhysical::Remove(void)
{
	CEntryInfoNode *node, *next;
	for(node = m_entryInfoList.first; node; node = next){
		next = node->next;
		node->list->DeleteNode(node->listnode);      // 0x44dde0 / 0x44def0
		m_entryInfoList.DeleteNode(node);            // 0x4296c0
	}
}

// 0x00468f90, slot 11. Centre de la sphère de collision ± rayon.
CRect
CPhysical::GetBoundRect(void)
{
	CVector center = GetBoundCentre();               // 0x466b20
	float radius = GetBoundRadius();                 // 0x465cc0
	return CRect(center.x - radius, center.y - radius,
	             center.x + radius, center.y + radius);
}

// 0x00469680. L'entité rejoint CWorld::ms_listMovingEntityPtrs (0xc1aea4),
// en tête, et garde son nœud en +0x17c. Deux gardes dans le binaire : rien
// à faire si elle y est déjà, ni si elle attend encore sa collision.
void
CPhysical::AddToMovingList(void)
{
	if(m_pMovingListNode != nil || bIsStaticWaitingForCollision != 0)
		return;
	m_pMovingListNode = CWorld::GetMovingEntityList().InsertItem(this);
}

// 0x004696f0. Si le nœud retiré est celui que le parcours global tient
// (ms_pMovingListCursor, 0xc1ae84), le curseur avance d'abord sur le
// suivant : sans ça, retirer l'entité en cours de traitement couperait
// la boucle. Puis décrochage, libération, champ remis à zéro.
void
CPhysical::RemoveFromMovingList(void)
{
	if(m_pMovingListNode == nil)
		return;
	if(m_pMovingListNode == CWorld::ms_pMovingListCursor)
		CWorld::ms_pMovingListCursor = m_pMovingListNode->next;
	CWorld::GetMovingEntityList().DeleteNode(m_pMovingListNode);
	m_pMovingListNode = nil;
}
