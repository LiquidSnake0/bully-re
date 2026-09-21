// CPlayerPed : 47 virtuelles (46 de CPed + 1).
#pragma once
#include "Ped.h"

class CPlayerPed : public CPed
{
public:
	~CPlayerPed(void) override;                                  // 0x4498f0
	void ProcessControl(void) override;                          // 0x49bae0, 928 octets
	void Render(void) override;                                  // 0x49a530
	bool IsRenderable(void) override;                            // 0x49a930
	void Slot42(uint8 a, void *b) override;                      // 0x49ac50
	void ProcessPunishmentDecay(void) override;                  // 0x49a6a0
	virtual void Slot46(void) {}                                 // 0x49a500, vide

	uint32 m_nMinPunishmentPoints; // +0x1ebc (Lua : PlayerSetMinPunishmentPoints)
	int32 m_field1ed4;            // +0x1ed4
};
