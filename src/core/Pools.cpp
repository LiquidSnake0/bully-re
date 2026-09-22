// CPools::Initialise, 0x0044d1d0 : 28 pools, chacun un objet de 0x1c octets
// alloué par GameMalloc puis initialisé par un constructeur propre
// (taille d'entrée en dur, entrées construites par `vector constructor
// iterator` quand +0x19 est levé, drapeaux mis à 0x80 = libre).
#include "Pools.h"
#include <cstring>

extern void *GameMalloc(size_t size);   // 0x5ee6d0

CPool *CPools::ms_pPtrNodePool;
CPool *CPools::ms_pEntryInfoNodePool;
CPool *CPools::ms_pVehiclePool;
CPool *CPools::ms_pPedPool;
CPool *CPools::ms_pEffectProxyAttachPool;
CPool *CPools::ms_pPool62c;
CPool *CPools::ms_pAttitudeSetPool;
CPool *CPools::ms_pPool634;
CPool *CPools::ms_pPool638;
CPool *CPools::ms_pPool63c;
CPool *CPools::ms_pIKBlendDriverGroupPool;
CPool *CPools::ms_pJointConstraintPool;
CPool *CPools::ms_pFloorMotionDriverPool;
CPool *CPools::ms_pReachDriverPool;
CPool *CPools::ms_pPool654;
CPool *CPools::ms_pPool650;
CPool *CPools::ms_pDummyPool;
CPool *CPools::ms_pPropAnimPool;
CPool *CPools::ms_pBuildingPool;
CPool *CPools::ms_pTreadablePool;
CPool *CPools::ms_pAccessoryPool;
CPool *CPools::ms_pColModelPool;
CPool *CPools::ms_pPool610;
CPool *CPools::ms_pObjectPool;
CPool *CPools::ms_pProjectilePool;
CPool *CPools::ms_pCutsceneObjectPool;
CPool *CPools::ms_pSFXItemPool;
CPool *CPools::ms_pPool624;

const CPoolSpec CPools::ms_aSpecs[28] = {
	{ &ms_pPtrNodePool, 15000, 0x4, "?" },   // 0x00c0f5e8
	{ &ms_pEntryInfoNodePool, 2000, 0x14, "?" },   // 0x00c0f5ec
	{ &ms_pVehiclePool, 15, 0x8c0, "CAutomobile" },   // 0x00c0f5f4
	{ &ms_pPedPool, 24, 0x1f44, "CPlayerPed" },   // 0x00c0f5f0
	{ &ms_pEffectProxyAttachPool, 24, 0x74, "EffectProxyAttach" },   // 0x00c0f628
	{ &ms_pPool62c, 24, 0x8, "?" },   // 0x00c0f62c
	{ &ms_pAttitudeSetPool, 24, 0x38, "CAttitudeSet" },   // 0x00c0f630
	{ &ms_pPool634, 24, 0x5c0, "?" },   // 0x00c0f634
	{ &ms_pPool638, 24, 0xe0, "?" },   // 0x00c0f638
	{ &ms_pPool63c, 24, 0x20, "?" },   // 0x00c0f63c
	{ &ms_pIKBlendDriverGroupPool, 24, 0x200, "IKBlendDriverGroup" },   // 0x00c0f640
	{ &ms_pJointConstraintPool, 48, 0x80, "JointConstraint" },   // 0x00c0f644
	{ &ms_pFloorMotionDriverPool, 24, 0x40, "FloorMotionDriver" },   // 0x00c0f648
	{ &ms_pReachDriverPool, 48, 0x50, "ReachDriver" },   // 0x00c0f64c
	{ &ms_pPool654, 57, 0x28, "?" },   // 0x00c0f654
	{ &ms_pPool650, 8, 0x2b0c, "?" },   // 0x00c0f650
	{ &ms_pDummyPool, 300, 0x128, "CDummy" },   // 0x00c0f600
	{ &ms_pPropAnimPool, 220, 0x3b8, "CPropAnim" },   // 0x00c0f608
	{ &ms_pBuildingPool, 2250, 0x120, "CBuilding" },   // 0x00c0f5f8
	{ &ms_pTreadablePool, 1, 0x120, "CTreadable" },   // 0x00c0f5fc
	{ &ms_pAccessoryPool, 48, 0x2e8, "CAccessory" },   // 0x00c0f60c
	{ &ms_pColModelPool, 4150, 0x40, "?" },   // 0x00c0f604
	{ &ms_pPool610, 87, 0x2c, "?" },   // 0x00c0f610
	{ &ms_pObjectPool, 275, 0x22c, "CObject" },   // 0x00c0f614
	{ &ms_pProjectilePool, 35, 0x24c, "CProjectile" },   // 0x00c0f618
	{ &ms_pCutsceneObjectPool, 30, 0x33c, "CCutsceneObject" },   // 0x00c0f61c
	{ &ms_pSFXItemPool, 48, 0x74, "SFXItem" },   // 0x00c0f620
	{ &ms_pPool624, 200, 0x64, "?" },   // 0x00c0f624
};

static CPool *
CreerPool(int32 count, int32 entrySize)
{
	CPool *p = (CPool*)GameMalloc(sizeof(CPool));
	if(p == nil) return nil;
	p->m_field10 = 0;
	p->m_entries = nil;
	p->m_flags = nil;
	p->m_allocPtr = 0;
	p->m_bOwnsEntries = true;
	p->m_size = count;
	p->m_bConstructEntries = false;      // vrai pour les pools d'entités, qui construisent leurs éléments
	p->m_entrySize = entrySize;
	p->m_entries = (uint8*)GameMalloc(count * entrySize);
	p->m_flags = (uint8*)GameMalloc(count);
	for(int32 i = 0; i < count; i++) p->m_flags[i] = 0x80;
	return p;
}

void
CPools::Initialise(void)
{
	for(int i = 0; i < 28; i++)
		*ms_aSpecs[i].slot = CreerPool(ms_aSpecs[i].count, ms_aSpecs[i].entrySize);
}
