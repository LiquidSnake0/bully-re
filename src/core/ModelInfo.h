// Informations de modèle, héritées de GTA (re3 : modelinfo/).
// Dans bully.exe : CModelInfo::ms_modelInfoPtrs à 0x00c67738, indexé par
// l'int16 m_modelIndex ; CreateInstance est le slot 4 (+0x10) de la table
// virtuelle de CBaseModelInfo.
#pragma once
#include "../common.h"

class CBaseModelInfo
{
public:
	virtual ~CBaseModelInfo(void) {}
	virtual void Slot1(void) = 0;
	virtual void Slot2(void) = 0;
	virtual void Slot3(void) = 0;
	virtual RwObject *CreateInstance(void) = 0;   // +0x10
	// +0x48 (piVar7[0x12]) : données de squelette / animation, nul pour un modèle simple
	void *m_pSkeletonData;
};

class CModelInfo
{
public:
	static CBaseModelInfo **ms_modelInfoPtrs;     // 0x00c67738
	static CBaseModelInfo *GetModelInfo(int16 id) { return ms_modelInfoPtrs[id]; }
	static bool IsSpecialModel(int16 id);         // 0x43f250
};

// Tas mémoire avec identifiants de zone, comme PUSH_MEMID dans reVC.
class CMemoryHeap
{
public:
	static void Push(int id);   // 0x5eef40
	static void Pop(void);      // 0x5eefa0
};
