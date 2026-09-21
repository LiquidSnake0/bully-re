// CEntity, 34 méthodes virtuelles dans bully.exe (17 dans reVC).
// Les noms marqués « ? » sont des hypothèses, voir docs/vtable-entity.md.
// La disposition mémoire n'est pas encore reproduite : les champs portent
// leur décalage d'origine en commentaire.
#pragma once
#include "Placeable.h"

enum eEntityType {
	ENTITY_TYPE_NOTHING = 0,
	ENTITY_TYPE_BUILDING = 1,
	ENTITY_TYPE_VEHICLE = 2,
	ENTITY_TYPE_PED = 3,
	ENTITY_TYPE_OBJECT = 4,
	ENTITY_TYPE_DUMMY = 5,
	ENTITY_TYPE_6 = 6,       // inséré dans la liste des objets (CPhysical::Add)
	ENTITY_TYPE_7 = 7
};

class CEntity : public CPlaceable
{
public:
	// slot 0 : ~CEntity (CPlaceable)
	virtual bool IsOfTypeId(int16 typeId);                // slot 1, ?
	virtual void Add(void);                               // slot 2
	virtual void Remove(void);                            // slot 3
	virtual bool Slot4(void) { return false; }            // slot 4, prédicat de type ?
	virtual void Slot5(void) {}                           // slot 5, vide
	virtual void SetStatus(uint8 status);                 // slot 6 (implémenté par CVehicle)
	virtual void SetIsStatic(bool isStatic);              // slot 7, ?
	virtual void SetModelIndex(int16 id, int32 flags);    // slot 8
	virtual void SetModelIndexNoCreate(int16 id);         // slot 9
	virtual void CreateRwObject(void);                    // slot 10
	virtual CRect GetBoundRect(void);                     // slot 11
	virtual void ProcessControl(void) {}                  // slot 12, vide dans CEntity (0x49a500)
	virtual void ProcessCollision(void) {}                // slot 13, vide (0x8a1f10)
	virtual void ProcessShift(void) {}                    // slot 14, vide (0x44bf20)
	virtual bool Slot15(void) { return false; }           // slot 15, retourne 0 (0x44be90) ; Teleport dans reVC
	virtual void *Slot16(void) { return nil; }            // slot 16, retourne 0 (0x824d30)
	virtual void PreRender(void);                         // slot 17, 0x466b70
	virtual void Render(void);                            // slot 18, 0x466bf0, 1643 octets
	virtual bool SetupLighting(void);                     // slot 19, 0x467d60, ordre reVC
	virtual void RemoveLighting(bool reset);              // slot 20, 0x468040, ordre reVC
	virtual void FlagToDestroyWhenNextProcessed(void) {}  // slot 21, vide, ordre reVC
	virtual bool IsRenderable(void);                      // slot 22, 0x4655f0 : faux si +0xa4 != 0 et +0xe4 == 0 ; nom ?
	virtual void PureVirtual23(void) = 0;                 // slot 23, __purecall dans CEntity, implémenté par CVehicle (0x43a2d0)
	virtual void Slot24(void) {}                          // slot 24, vide (0x49a500)
	virtual void GetBoundingBoxTransformed(float *out, void *matrix);   // slot 25, 0x467ab0, boîte du colmodel × matrice ; nom ?
	virtual void Slot26(void *arg);                       // slot 26, 0x466060 : appelle slot 25 puis 0x41a800
	virtual float GetBoundDiameter(void);                 // slot 27, 0x466090 : 2 × rayon du colmodel ; nom ?
	virtual float GetBoundDiameter2(void);                // slot 28, même adresse que 27
	virtual float GetBoundRadius(void);                   // slot 29, 0x465cc0 : rayon du colmodel (+0xc)
	virtual bool HasField0xcc(void);                      // slot 30, 0x44bea0 : +0xcc != 0 ; nom ?
	virtual bool Slot31(void);                            // slot 31, 0x512610 : parcourt les effets (0x50ec20), vrai si l'un a +0x38 != 0
	virtual float GetDefaultDrawDistance(void);           // slot 32, 0x44bec0 : constante 0x8ff34c ; nom ?
	virtual bool HasModelFlag10000(void);                 // slot 33, 0x467ce0 : type != 5 et flags du modelinfo (+0x28) & 0x10000 ; nom ?

	CVector GetBoundCentre(void);                         // 0x466b20, non virtuelle

	RwObject *m_rwObject;          // +0x18
	uint8 m_type : 3;              // +0x108, bits 0..2
	uint8 m_status : 5;            // +0x108, bits 3..7
	int16 m_modelIndex;            // +0x10e
	bool bHasPreRenderEffects;     // +0xbc (mot entier dans le binaire)
	void *m_pAttachedObject;       // +0xc4, sens à confirmer
	bool bFlag209_0;               // +0x209, bit 0

	static int16 ms_reservedModelA;   // 0x00bf44ec
	static int16 ms_reservedModelB;   // 0x00bf44e8
	static int32 ms_numBuildingObjects; // 0x00c2a8ec
};
