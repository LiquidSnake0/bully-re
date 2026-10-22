#include "Opportunite.h"
#include <cstring>

bool
TirageOpportunite(uint32 periode, uint16 graine, uint32 image)
{
	if((int32)periode < 2) return true;
	uint32 u = (uint32)graine + image;
	u = u % 0xffff + 1;
	return periode <= (u & (0u - u));
}

static float
Reel(const CActionTrack &p, uint16 position, float defaut)
{
	const CActionAttribut *a = p.Champ(position);
	if(a == nil || a->taille < 4) return defaut;
	float r;
	memcpy(&r, a->valeur, 4);
	return r;
}

bool
COpportunite::Lire(const CActionTreeFile &f, const CActionNode &porteur, const CActionTrack &p, COpportunite &o)
{
	static const uint32 kOpportunity = ActionHash("Opportunity");
	if(p.type != kOpportunity) return false;
	o = COpportunite();
	o.porteur = &porteur;
	if(const CActionAttribut *a = p.Champ(32)) o.chemin = f.ChaineCitee(a->decalage);
	o.entree = Reel(p, 12, 0.0f);
	o.vie = Reel(p, 16, -1.0f);
	o.fermeture = Reel(p, 40, -1.0f);
	o.bascule = Reel(p, 44, 0.0f);
	if(const CActionAttribut *a = p.Champ(48)) o.soiMeme = a->valeur[0] != 0;
	o.priorite = (int32)p.Mot(52);
	o.periode = p.Mot(56);
	o.conditions = p.Mot(60);
	return true;
}

std::vector<COpportunite>
COpportunite::DuNoeud(const CActionBank &b, const CActionNode &n)
{
	std::vector<COpportunite> v;
	const CActionTreeFile *f = b.Fichier(n);
	if(f == nil) return v;
	for(int32 dp : n.pistes){
		CActionTrack p;
		COpportunite o;
		if(f->Piste(dp, p) && Lire(*f, n, p, o)) v.push_back(o);
	}
	return v;
}
