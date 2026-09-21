// CVehicle : 60 méthodes virtuelles (42 de CPhysical + 18), reVC en ajoute 24.
#pragma once
#include "../entities/Physical.h"

class CVehicle : public CPhysical
{
public:
	void SetStatus(uint8 status) override;               // 0x4cb1a0, slot 6
	void SetModelIndex(int16 id, int32 flags) override;  // 0x4cb040

	virtual void Slot42(void);  virtual void Slot43(void);  virtual void Slot44(void);
	virtual void Slot45(void);  virtual void Slot46(void);  virtual void Slot47(void) {}
	virtual void Slot48(void);  virtual void Slot49(void);  virtual void Slot50(void);
	virtual void Slot51(void);  virtual void Slot52(void);  virtual void Slot53(void);
	virtual void Slot54(void);  virtual void Slot55(void);  virtual void Slot56(void);
	virtual void Slot57(void);  virtual void Slot58(void);  virtual void Slot59(void);

	void OnStatusThree(void);                            // 0x41bd80, nom provisoire

	uint8 m_nVehicleFlags33c;                            // +0x33c
};
