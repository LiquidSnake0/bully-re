// Recréation partielle de CBike à partir de bully.exe.
#include "Bike.h"

// 0x004c5ec0, slot 8. Appelle CVehicle::SetModelIndex (0x4cb040), remet à
// zéro un bloc de huit mots à +0x580, y installe un rappel (0x4c46d0) sur le
// squelette (+0x114), puis retient les nœuds nommés du modèle.
void
CBike::SetModelIndex(int16 id, int32 flags)
{
	CVehicle::SetModelIndex(id, flags);
	for(int i = 0; i < 8; i++) m_aBikeState[i] = 0;         // +0x580..+0x59c
	CVisibilityPlugins::SetupBikeCallback(m_rwObject, m_aBikeState);   // 0x5109f0
	m_pSkeleton->callback = CBike::SkeletonCallback;         // +0x114 → +4 = 0x4c46d0
	m_pSkeleton->owner = this;
	m_pSkeleton->extraA = &m_bikeExtraA;                     // +0x5b8
	m_pSkeleton->extraB = &m_bikeExtraB;                     // +0x5c4
	m_pHandlebars  = m_pSkeleton->FindNode("Handlebars");    // 0x68e380
	m_pFrontWheel  = m_pSkeleton->FindNode("Front_Wheel");
	m_pRearWheel   = m_pSkeleton->FindNode("Rear_Wheel");
	m_pFrontMud    = m_pSkeleton->FindNode("Front_Mud");
}

// 0x004b95d0, slot 7. Sens exact à confirmer par les appelants.
void
CBike::SetIsStatic(bool isStatic)
{
	m_nStaticFlag = isStatic;              // +0xb0
	m_nStaticCounter = isStatic ? 10 : 0;  // +0x180
}

// 0x004cb1a0, slot 6, implémenté par CVehicle et hérité par CBike.
// m_status occupe les bits 3..7 de l'octet +0x108, m_type les bits 0..2.
void
CVehicle::SetStatus(uint8 status)
{
	if(status == 4){
		if(m_status == 3 || m_status == 0) m_nVehicleFlags33c |= 8;
		else if(m_status == 2)             m_nVehicleFlags33c &= ~8;
	}else if(status == 3)
		OnStatusThree();                   // 0x41bd80
	m_status = status;
}
