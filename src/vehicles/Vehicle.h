// CVehicle : 60 méthodes virtuelles (42 de CPhysical + 18), reVC en ajoute 24.
#pragma once
#include "../entities/Physical.h"

class CVehicle : public CPhysical
{
public:
	void SetStatus(uint8 status) override;               // 0x4cb1a0, slot 6
	void SetModelIndex(int16 id, int32 flags) override;  // 0x4cb040

	virtual void ProcessControlInputs(uint8 pad) {}                          // slot 42, vide (0x49a500)
	virtual void GetComponentWorldPosition(int32 component, CVector &pos) {}  // slot 43, vide (0x4358b0)
	virtual bool IsComponentPresent(int32 component) { return false; }       // slot 44 (0x6094f0)
	virtual void SetComponentRotation(int32 component, float x, float y, float z) {}   // slot 45, vide
	virtual void Slot46(void) {}                                             // slot 46, vide (OpenDoor ?)
	virtual void Slot47(void) {}                                             // slot 47, vide (ProcessOpenDoor ?)
	virtual void RemoveRefsToVehicle(CEntity *ent) = 0;                      // slot 48, pure ; CBike : 0x4b9b60
	virtual bool Slot49(void) { return false; }                              // slot 49 (0x6094f0) ; CBike : 0x4c4270, 1107 octets, colmodel
	virtual void Slot50(void) {}                                             // slot 50, vide
	virtual void Slot51(float f);                                            // slot 51, 0x4cc8c0, boîte du colmodel
	virtual void Slot52(void);                                               // slot 52, 0x44bf30 → 0x4ce4f0
	virtual void Slot53(void) = 0;                                           // slot 53, pure ; CBike : 0x4c0200, 1355 octets
	virtual float GetHeightAboveRoad(void);                                  // slot 54, 0x4cca60, ?
	virtual void PlayCarHorn(void) {}                                        // slot 55, vide ; CBike : 0x4b9960
	virtual void SetField2a0(uint32 v) { m_field2a0 = v; }                   // slot 56, 0x44bf40
	virtual float Slot57(void);                                              // slot 57, constante 0x912fd0
	virtual void Slot58(int32 a, int32 b);                                   // slot 58, 0x4cb2b0
	virtual void Slot59(float a, float b) = 0;                               // slot 59, pure ; CBike : 0x4bd300

	void OnStatusThree(void);                            // 0x41bd80, nom provisoire

	uint8 m_nVehicleFlags33c;                            // +0x33c
	uint32 m_field2a0;                                   // +0x2a0
	int32 m_nCarHornTimer;                               // +0x368, comme reVC
	uint8 m_nCarHornDelay;                               // +0x36e, comme reVC
	CEntity *m_pRef6e4;                                  // +0x6e4, conducteur ou cible (RemoveRefsToVehicle)
	CEntity *m_pRef6e8;                                  // +0x6e8
};
