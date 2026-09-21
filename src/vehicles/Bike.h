// CBike : même nombre de virtuelles que CVehicle (60), en redéfinit 27.
#pragma once
#include "Vehicle.h"

class CBike;

// Squelette Gamebryo attaché au véhicule (+0x114). Champs vus dans
// CBike::SetModelIndex ; le reste sera lu dans le code du squelette.
struct CVehicleSkeleton {
	void *unk0;
	void (*callback)(void);   // +4
	CBike *owner;             // +8
	uint8 pad[0x98];
	void *extraA;             // +0xa4
	void *extraB;             // +0xa8
	void *FindNode(const char *name);   // 0x68e380
};

class CVisibilityPlugins
{
public:
	static void SetupBikeCallback(RwObject *obj, uint32 *state);   // 0x5109f0
};

class CBike : public CVehicle
{
public:
	~CBike(void) override;                               // 0x4bd6e0
	bool IsOfTypeId(int16 typeId) override;              // 0x4b9e30
	bool Slot4(void) override { return true; }           // 0x7ffc70
	void SetIsStatic(bool isStatic) override;            // 0x4b95d0
	void SetModelIndex(int16 id, int32 flags) override;  // 0x4c5ec0
	void ProcessControl(void) override;                  // 0x4c61f0, ?
	void ProcessCollision(void) override;                // 0x4c6510, ?

	static void SkeletonCallback(void);                  // 0x4c46d0

	uint32 m_nStaticFlag;         // +0xb0
	uint8 m_nStaticCounter;       // +0x180
	CVehicleSkeleton *m_pSkeleton; // +0x114
	uint32 m_aBikeState[8];       // +0x580
	uint8 m_bikeExtraA[12];       // +0x5b8
	uint8 m_bikeExtraB[12];       // +0x5c4
	void *m_pHandlebars;          // +0x784
	void *m_pFrontWheel;          // +0x788
	void *m_pRearWheel;           // +0x78c
	void *m_pFrontMud;            // +0x790
};
