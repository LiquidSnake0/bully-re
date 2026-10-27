#include "Clothing.h"

CClothingManager::CClothingManager(void)
{
	portee = -1;   // FUN_006cbb40
}

int
CClothingManager::Definir(uint32 nom, uint32 second, const uint32 pieces[GARDE_ROBE_PIECES], bool dernier)
{
	if(!ouverte) return -1;
	int i = 0;
	while(tenues[i].drapeaux & 1)
		if(++i >= GARDE_ROBE_TENUES) return -1;
	CTenue &t = tenues[i];
	t.nom = nom;
	t.second = second;
	t.drapeaux = (uint8)((dernier ? 2 : 0) | (t.drapeaux & 0xf1) | 0x11);
	for(int k = 0; k < GARDE_ROBE_PIECES; k++) t.pieces[k] = pieces[k];
	return i;
}

bool
CClothingManager::Mettre(uint32 nom, bool pieceAPiece)
{
	for(int i = 0; i < GARDE_ROBE_TENUES; i++)
		if(tenues[i].nom == nom){
			portee = pieceAPiece ? -1 : i;
			return true;
		}
	return false;
}

bool
CClothingManager::Porte(uint32 nom) const
{
	return portee != -1 && tenues[portee].nom == nom;
}
