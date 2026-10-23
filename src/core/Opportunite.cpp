#include "Opportunite.h"
#include <cstring>

int
RandCrt(uint32 &graine)
{
	graine = graine * 214013u + 2531011u;
	return (int)((graine >> 16) & 0x7fff);
}

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
	static const uint32 kOpportunity = ActionHash("Opportunity"), kLoquet = ActionHash("OpportunityRandomLatch");
	if(p.type != kOpportunity && p.type != kLoquet) return false;
	o = COpportunite();
	o.loquet = p.type == kLoquet;
	o.loquetMin = Reel(p, 76, 0.0f);
	o.loquetMax = Reel(p, 80, 0.0f);
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
	if(const CActionAttribut *a = p.Champ(60)){
		int32 v = f.Renvoi2(a->decalage);
		if(v >= 0) f.Groupe((uint32)v, o.listeConditions);
	}
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
