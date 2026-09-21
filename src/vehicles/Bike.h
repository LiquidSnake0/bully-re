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
	static void SetupBikeCallback(RwObject *obj, void **nodes);   // 0x5109f0
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
	void GetComponentWorldPosition(int32 component, CVector &pos) override;   // 0x4b9ed0
	bool IsComponentPresent(int32 component) override;   // 0x4b9450
	void SetComponentRotation(int32 component, float x, float y, float z) override;   // 0x4bda90
	void RemoveRefsToVehicle(CEntity *ent) override;     // 0x4b9b60
	void PlayCarHorn(void) override;                     // 0x4b9960
	void Slot53(void) override {}                        // 0x4c0200, à recréer
	void Slot59(float a, float b) override {}            // 0x4bd300, à recréer

	static void SkeletonCallback(void);                  // 0x4c46d0

	uint32 m_nStaticFlag;         // +0xb0
	uint8 m_nStaticCounter;       // +0x180
	CVehicleSkeleton *m_pSkeleton; // +0x114
	void *m_aBikeNodes[8];        // +0x580 : nœuds Gamebryo des composants (GetComponentWorldPosition)
	uint8 m_bikeExtraA[12];       // +0x5b8
	uint8 m_bikeExtraB[12];       // +0x5c4
	void *m_pHandlebars;          // +0x784
	void *m_pFrontWheel;          // +0x788
	void *m_pRearWheel;           // +0x78c
	void *m_pFrontMud;            // +0x790
};
