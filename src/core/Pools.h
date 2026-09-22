// Pools d'objets, hérités de GTA (re3 : core/Pools.h). Dans bully.exe le
// pool n'est pas un gabarit mais un objet de 0x1c octets qui connaît la
// taille de ses entrées ; CPools::Initialise (0x44d1d0) en crée 28.
#pragma once
#include "../common.h"

class CPool
{
public:
	uint8 *m_entries;        // +0x00 : count × entrySize (précédé du count si les entrées sont construites)
	uint8 *m_flags;          // +0x04 : un octet par entrée, bit 7 = libre, bits 0..6 = référence
	int32 m_size;            // +0x08
	int32 m_entrySize;       // +0x0c
	int32 m_field10;         // +0x10
	int32 m_allocPtr;        // +0x14
	bool m_bOwnsEntries;     // +0x18
	bool m_bConstructEntries;// +0x19 : construire chaque entrée à la création

	bool IsFree(int32 i) const { return (m_flags[i] & 0x80) != 0; }
	void *GetAt(int32 i) const { return IsFree(i) ? nil : m_entries + i * m_entrySize; }
	int32 GetSize(void) const { return m_size; }
};

struct CPoolSpec {
	CPool **slot;
	int32 count;
	int32 entrySize;
	const char *elementClass;
};

class CPools
{
public:
	static CPool *ms_pPtrNodePool;
	static CPool *ms_pEntryInfoNodePool;
	static CPool *ms_pVehiclePool;
	static CPool *ms_pPedPool;
	static CPool *ms_pEffectProxyAttachPool;
	static CPool *ms_pPool62c;
	static CPool *ms_pAttitudeSetPool;
	static CPool *ms_pPool634;
	static CPool *ms_pPool638;
	static CPool *ms_pPool63c;
	static CPool *ms_pIKBlendDriverGroupPool;
	static CPool *ms_pJointConstraintPool;
	static CPool *ms_pFloorMotionDriverPool;
	static CPool *ms_pReachDriverPool;
	static CPool *ms_pPool654;
	static CPool *ms_pPool650;
	static CPool *ms_pDummyPool;
	static CPool *ms_pPropAnimPool;
	static CPool *ms_pBuildingPool;
	static CPool *ms_pTreadablePool;
	static CPool *ms_pAccessoryPool;
	static CPool *ms_pColModelPool;
	static CPool *ms_pPool610;
	static CPool *ms_pObjectPool;
	static CPool *ms_pProjectilePool;
	static CPool *ms_pCutsceneObjectPool;
	static CPool *ms_pSFXItemPool;
	static CPool *ms_pPool624;

	static const CPoolSpec ms_aSpecs[28];
	static void Initialise(void);   // 0x44d1d0
};
