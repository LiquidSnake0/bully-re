// CPed : 46 virtuelles (42 de CPhysical + 4). Voir docs/vtable-ped.md.
#pragma once
#include "../entities/Physical.h"

enum ePedState {
	PED_STATE_PLAYER_CONTROLLED = 0xd     // +0x1310, testé avec FindPlayerPed
};

class CPed : public CPhysical
{
public:
	void Remove(void) override;                                  // 0x4731e0
	void SetModelIndex(int16 id, int32 flags) override;          // 0x4788d0
	void ProcessCollision(void) override;                        // 0x4839c0
	void ProcessShift(void) override;                            // 0x47b570
	void PreRender(void) override;                               // 0x47b210
	void Render(void) override;                                  // 0x474820 ?
	bool SetupLighting(void) override;                           // 0x4741b0
	void RemoveLighting(bool reset) override;                    // 0x4771c0
	void FlagToDestroyWhenNextProcessed(void) override;          // 0x474820
	void PureVirtual23(void) override;                           // 0x43a2d0

	virtual void Slot42(uint8 a, void *b);                       // 0x479110, sons et animations
	virtual uint32 GetField1d2c(void) { return m_field1d2c; }    // 0x46de90
	virtual void ProcessPunishmentDecay(void);                   // 0x477530
	virtual void IncPunishmentPoints(int32 amount);              // 0x477480 (Lua : PlayerIncPunishmentPoints)

	int32 m_nPedState;            // +0x1310
	uint32 m_field1d2c;           // +0x1d2c
	int32 m_nPunishmentPoints;    // +0x1d40 (Lua : PedSetPunishmentPoints, PlayerGetPunishmentPoints)
	uint32 m_nPunishmentLastTick; // +0x1d48
	uint8 m_field1624;            // +0x1624
	uint32 m_field148c;           // +0x148c
	uint32 m_field137c;           // +0x137c
	uint8 m_field1d44;            // +0x1d44
	int32 m_field14a8;            // +0x14a8

	void OnSmallPunishment(void);               // 0x474480, nom ?
	void SetPunishmentPoints(int32 points);     // 0x4773d0 (Lua : PedSetPunishmentPoints → 0x5ccdf0 → 0x4773d0)

	static CPed *FindPlayerPed(int32 index);    // 0x4ce410
	static int16 ms_reservedPedModel[5];        // 0xbf42d4, 0xbf42d0, 0xbf42cc, 0xbf42dc, 0xbf42d8
};
