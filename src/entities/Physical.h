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
	virtual void Slot35(void);  virtual void Slot36(void);  virtual void Slot37(void);
	virtual void Slot38(void);  virtual void Slot39(void);  virtual void Slot40(void);
	virtual void Slot41(void);

	CEntryInfoList m_entryInfoList;             // +0x178
};
