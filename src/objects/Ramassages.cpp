#include "Ramassages.h"
#include "Collectibles.h"
#include "../peds/Armes.h"
#include <cmath>

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

int32
CRamassages::CreerDuCollectible(const CVector &pos, int32 modele, int16 indice, uint32 hache, uint8 octet110, float decalageZ)
{
	int32 h = Creer(pos, modele, "", false, true, hache, octet110, false, decalageZ);
	if(CRamassage *r = Entree(h)) r->collectible = indice;
	return h;
}

bool
CRamassages::AuContact(int i, const CVector &joueur, float santeJoueur, bool typeAuContact) const
{
	if(i < 0 || i >= kTaille) return false;
	const CRamassage &r = entrees[i];
	if(!r.Pris() || r.actif != 1) return false;
	float dx = joueur.x - r.pos.x, dy = joueur.y - r.pos.y;
	if(dx * dx + dy * dy - 40.0f * 40.0f > 0.0f) return false;
	if((r.drapeaux & RAMASSAGE_A_LA_MAIN) || !typeAuContact || !(0.0f < santeJoueur)) return false;
	if(fabsf(joueur.z - r.pos.z) > 2.0f) return false;
	return dx * dx + dy * dy - 1.0f <= 0.0f;
}

int
CRamassages::PrendreObjet(int i, int genre, const CMondeArmes &m, CObjetsJoueur &objets, CCollectibles &c, CRappelCollectible *rappel,
                          bool *aRappel)
{
	if(aRappel) *aRappel = false;
	if(i < 0 || i >= kTaille || genre < 2 || genre > 4) return 0;
	CRamassage &r = entrees[i];
	if(objets.Nombre(r.modele) >= m.Type(r.modele).maximum) return 0;   // FUN_00444150 : le joueur n'en porte pas plus
	if(r.drapeaux & RAMASSAGE_COLLECTIBLE){
		bool a = c.Marquer(r.collectible, true, rappel);
		if(aRappel) *aRappel = a;
	}
	objets.Ajouter(m, r.modele, 1);
	Retirer(i);
	return 1;
}
