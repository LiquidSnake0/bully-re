// Recréation partielle de CEntity à partir de bully.exe.
#include "Entity.h"
#include "../core/ModelInfo.h"

// 0x00465cd0, slot 9. Reçoit un int16 (l'index de modèle est un short ici,
// pas un uint32 comme dans reVC). Le drapeau à +0xbc est mis à 1 quand
// l'entité a déjà un objet lié (+0xc4), quand le modèle est un modèle
// « spécial » (0x43f250), ou quand l'index vaut l'un des deux modèles
// réservés (globales 0xbf44ec / 0xbf44e8) ou que le bit 0 de +0x209 est levé.
void
CEntity::SetModelIndexNoCreate(int16 id)
{
	m_modelIndex = id;
	if(m_pAttachedObject != nil){ bHasPreRenderEffects = true; return; }
	if(CModelInfo::IsSpecialModel(id)){ bHasPreRenderEffects = true; return; }
	if(id != ms_reservedModelA && id != ms_reservedModelB && !bFlag209_0){
		bHasPreRenderEffects = false;
		return;
	}
	bHasPreRenderEffects = true;
}

// 0x004661a0, slot 10 (1009 octets, recréé en partie : le chemin principal).
// Crée l'instance graphique du modèle. Pour un piéton (m_type == 3) charge
// d'abord le bloc d'animations « RAT_PED\BASE » ou « C_PLAYER\BASE ».
void
CEntity::CreateRwObject(void)
{
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(m_modelIndex);
	CMemoryHeap::Push(0x27);                          // 0x5eef40
	// TODO : branche « modèle avec données de squelette » (piVar7[0x12] != 0)
	m_rwObject = mi->CreateInstance();                // vtable +0x10
	CMemoryHeap::Pop();                               // 0x5eefa0
	if(m_rwObject == nil) return;
	if(m_type == ENTITY_TYPE_BUILDING) ms_numBuildingObjects++;   // 0xc2a8ec
	// TODO : cas rpCLUMP (0x02) : mise à jour de la matrice et enregistrement
}
