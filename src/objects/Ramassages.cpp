#include "Ramassages.h"

int32
CRamassages::Creer(const CVector &pos, int32 modele, const std::string &fiche, bool ficheBit, bool collectible, uint32 mot3c,
                   uint8 octet45, bool neuf, float decalageZ)
{
	int i = 0;
	while(i < kTaille && entrees[i].Pris()) i++;
	if(i >= kTaille) return -1;
	CRamassage &r = entrees[i];
	uint16 g = r.generation;
	r = CRamassage();
	r.generation = g;
	r.fiche = fiche;
	r.modele = modele;
	r.actif = 1;
	r.collectible = -1;
	// +0x46 = (((neuf << 1 | collectible) << 2 | ordinaire | 2) << 2) | (ancien & 0xcb), le bit 0 venant de la fiche.
	bool aLaMain = !neuf && !collectible;
	r.drapeaux = (uint8)((ficheBit ? RAMASSAGE_FICHE : 0) | (aLaMain ? RAMASSAGE_A_LA_MAIN : 0) | RAMASSAGE_PRIS |
	                     (collectible ? RAMASSAGE_COLLECTIBLE : 0) | (neuf ? RAMASSAGE_NEUF : 0));
	r.pos = CVector(pos.x, pos.y, pos.z + decalageZ);
	r.mot3c = mot3c;
	r.octet45 = octet45;
	r.generation = r.generation < 0xfffe ? (uint16)(r.generation + 1) : (uint16)1;
	return Poignee(i);
}

int
CRamassages::Indice(int32 h) const
{
	if(h < 0) return -1;
	int i = h & 0xffff;
	if(i >= kTaille || !entrees[i].Pris() || entrees[i].generation != (uint16)((uint32)h >> 16)) return -1;
	return i;
}

CRamassage *
CRamassages::Entree(int32 h)
{
	int i = Indice(h);
	return i < 0 ? nil : &entrees[i];
}

