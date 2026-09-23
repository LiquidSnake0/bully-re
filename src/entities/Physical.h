// CPhysical : 42 méthodes virtuelles (34 de CEntity + 8), reVC en a 18.
#pragma once
#include "Entity.h"
#include "../core/Lists.h"

class CPhysical : public CEntity
{
public:
	void Add(void) override;                    // 0x469090
	void Remove(void) override;                 // 0x46a620
	CRect GetBoundRect(void) override;          // 0x468f90

	virtual void Slot34(void);                  // ProcessEntityCollision ? (CBike : 0x4b9ea0)
	virtual float Slot35(void);                 // constante 0x905a6c
	virtual void Slot36(void);                  // 0x46aa90, 760 octets, cas véhicule
	virtual void ApplyMoveSpeed(void);          // slot 37, 0x469760 : position += vitesse × CTimer::ms_fTimeStep (0xc1a9a4) ; ?
	virtual void ApplyTurnSpeed(void);          // slot 38, 0x469c70 : produit vectoriel vitesse angulaire × matrice ; ?
	virtual void Slot39(void) {}                // vide (0x8a1f10)
	virtual void Slot40(void);                  // 0x449f00 : matrice (0x8b5f90) → 0x4128c0
	virtual void Slot41(void);                  // CBike : 0x4c3ae0, 1931 octets, 4 paramètres

	// 0x469680 : n'inscrit que si m_pMovingListNode est nul et que
	// bIsStaticWaitingForCollision (+0xac) est nul ; insertion en tête de
	// CWorld::ms_listMovingEntityPtrs (0xc1aea4).
	void AddToMovingList(void);                 // 0x469680
	// 0x4696f0 : avance d'abord ms_pMovingListCursor (0xc1ae84) si c'est le
	// nœud en cours de parcours, puis décroche, libère et remet le champ à 0.
	void RemoveFromMovingList(void);            // 0x4696f0

	CEntryInfoList m_entryInfoList;             // +0x178
	CPtrNode *m_pMovingListNode;                // +0x17c
};
