// Implémentation hôte minimale pour les tests qui manipulent des entités.
// Les méthodes recréées à partir du binaire vivent dans src/ ; ici on ne
// fournit que les corps manquants, pour que les tables virtuelles soient
// complètes à l'édition de liens. Aucune de ces méthodes n'est testée.
#include "../../src/entities/Physical.h"
#include "../../src/core/ModelInfo.h"

CVector CPlaceable::GetPosition(void) { return CVector(0.0f, 0.0f, 0.0f); }

bool CEntity::IsOfTypeId(int16) { return false; }
void CEntity::Add(void) {}
void CEntity::Remove(void) {}
void CEntity::SetStatus(uint8 status) { m_status = status; }
void CEntity::SetIsStatic(bool isStatic) { bIsStatic = isStatic ? 1 : 0; }
void CEntity::SetModelIndex(int16 id, int32) { m_modelIndex = id; }
CRect CEntity::GetBoundRect(void) { return CRect(); }
void CEntity::PreRender(void) {}
void CEntity::Render(void) {}
bool CEntity::SetupLighting(void) { return false; }
void CEntity::RemoveLighting(bool) {}
bool CEntity::IsRenderable(void) { return true; }
void CEntity::GetBoundingBoxTransformed(float*, void*) {}
void CEntity::Slot26(void*) {}
float CEntity::GetBoundDiameter(void) { return 0.0f; }
float CEntity::GetBoundDiameter2(void) { return 0.0f; }
float CEntity::GetBoundRadius(void) { return 0.0f; }
bool CEntity::HasField0xcc(void) { return false; }
bool CEntity::Slot31(void) { return false; }
float CEntity::GetDefaultDrawDistance(void) { return 0.0f; }
bool CEntity::HasModelFlag10000(void) { return false; }
CVector CEntity::GetBoundCentre(void) { return CVector(0.0f, 0.0f, 0.0f); }

int16 CEntity::ms_reservedModelA = -1;
int16 CEntity::ms_reservedModelB = -1;
int32 CEntity::ms_numBuildingObjects = 0;

// Appelés par CWorld::Initialise (0x4397a0 et 0x4297f0), hors sujet ici.
void WorldSubsystemInit(void) {}
void AreaRemoveExtraScene(void) {}

// Slots propres à CPhysical, déclarés d'après la table virtuelle mais pas
// encore recréés : il faut un corps pour que la vtable s'édite.
void CPhysical::Slot34(void) {}
float CPhysical::Slot35(void) { return 0.0f; }
void CPhysical::Slot36(void) {}
void CPhysical::ApplyMoveSpeed(void) {}
void CPhysical::ApplyTurnSpeed(void) {}
void CPhysical::Slot40(void) {}
void CPhysical::Slot41(void) {}

// Modèles et tas : hors sujet pour CWorld, mais référencés par
// CEntity::SetModelIndexNoCreate et CEntity::CreateRwObject.
CBaseModelInfo **CModelInfo::ms_modelInfoPtrs = nil;
bool CModelInfo::IsSpecialModel(int16) { return false; }
void CMemoryHeap::Push(int) {}
void CMemoryHeap::Pop(void) {}
