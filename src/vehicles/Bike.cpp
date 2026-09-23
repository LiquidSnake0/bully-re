// Recréation partielle de CBike à partir de bully.exe.
#include "Bike.h"
#include <cstdlib>

// Gamebryo, à recréer : accès aux matrices d'un NiNode et pile de matrices.
extern const float *NiNodeGetWorldMatrix(void *node);   // 0x6c9f40
extern void *NiNodeGetLocalMatrix(void *node);          // 0x6c9930
extern void MatrixBegin(void *m);                       // 0x412920
extern void MatrixRotateX(float a);                     // 0x412b50
extern void MatrixRotateY(float a);                     // 0x412b70
extern void MatrixRotateZ(float a);                     // 0x412b90
extern void MatrixEnd(void);                            // 0x413920

// 0x004c5ec0, slot 8. Appelle CVehicle::SetModelIndex (0x4cb040), remet à
// zéro un bloc de huit mots à +0x580, y installe un rappel (0x4c46d0) sur le
// squelette (+0x114), puis retient les nœuds nommés du modèle.
void
CBike::SetModelIndex(int16 id, int32 flags)
{
	CVehicle::SetModelIndex(id, flags);
	for(int i = 0; i < 8; i++) m_aBikeNodes[i] = nil;       // +0x580..+0x59c
	CVisibilityPlugins::SetupBikeCallback(m_rwObject, m_aBikeNodes);   // 0x5109f0 : remplit les nœuds
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

// 0x004b9ed0, slot 43. Même rôle que CAutomobile::GetComponentWorldPosition
// dans reVC : position monde du nœud du composant (translation à +0x24 de
// la matrice monde, obtenue par 0x6c9f40).
void
CBike::GetComponentWorldPosition(int32 component, CVector &pos)
{
	void *node = m_aBikeNodes[component];
	if(node == nil) return;
	const float *world = NiNodeGetWorldMatrix(node);   // 0x6c9f40
	pos.x = world[9]; pos.y = world[10]; pos.z = world[11];   // +0x24, +0x28, +0x2c
}

// 0x004b9450, slot 44.
bool
CBike::IsComponentPresent(int32 component)
{
	return m_aBikeNodes[component] != nil;
}

// 0x004bda90, slot 45. Angles reçus en degrés, convertis en radians
// (constantes 0x900160 / 0x900158 = pi / 180).
void
CBike::SetComponentRotation(int32 component, float x, float y, float z)
{
	void *m = NiNodeGetLocalMatrix(m_aBikeNodes[component]);   // 0x6c9930
	MatrixBegin(m);                                             // 0x412920
	MatrixRotateX(x * PI / 180.0f);                             // 0x412b50
	MatrixRotateY(y * PI / 180.0f);                             // 0x412b70
	MatrixRotateZ(z * PI / 180.0f);                             // 0x412b90
	MatrixEnd();                                                // 0x413920
}

// 0x004b9b60, slot 48. Comme reVC : oublie une entité qui va disparaître.
void
CBike::RemoveRefsToVehicle(CEntity *ent)
{
	if(m_pRef6e4 == ent) m_pRef6e4 = nil;
	if(m_pRef6e8 == ent) m_pRef6e8 = nil;
}

// 0x004b9960, slot 55. Copie presque littérale de CAutomobile::PlayCarHorn
// de reVC : même délai aléatoire, même compteur à 45.
void
CBike::PlayCarHorn(void)
{
	if(m_nCarHornTimer != 0) return;
	if(m_nCarHornDelay != 0) m_nCarHornDelay--;
	int r = rand();
	m_nCarHornDelay = (r & 0x7f) + 150;
	if((m_nCarHornDelay & 7) < 4)          // le binaire teste < 2 puis < 4
		m_nCarHornTimer = 45;
}
