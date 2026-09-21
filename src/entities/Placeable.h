// CPlaceable : une matrice et un destructeur virtuel, comme dans GTA.
// Dans bully.exe la table virtuelle de CPlaceable ne compte qu'un slot.
#pragma once
#include "../common.h"

class CPlaceable
{
public:
	virtual ~CPlaceable(void) {}                 // slot 0

	// TODO : CMatrix m_matrix (décalage à lire dans le constructeur)
	CVector GetPosition(void);
};
