#include "ActionController.h"
#include <cstring>

static float
Reel(const CActionTrack &p, uint16 position, float defaut)
{
	if(p.Champ(position) == nil) return defaut;
	uint32 m = p.Mot(position);
	float r;
	memcpy(&r, &m, 4);
	return r;
}

float
CActionController::Duree(const CActionBank &b, const CActionNode &n)
{
	const CActionTreeFile *f = b.Fichier(n);
	if(f == nil) return -1.0f;
	float d = -1.0f;
	for(int32 dp : n.pistes){
		CActionTrack p;
		if(!f->Piste(dp, p)) continue;
		float vie = Reel(p, 16, -1.0f);
		if(vie < 0.0f) continue;
		float bout = Reel(p, 12, 0.0f) + vie;
		if(bout > d) d = bout;
	}
	return d;
}

void
CActionController::Jouer(const CActionBank &b, const CActionNode *n, float t)
{
	joue = n;
	debut = t;
	float d = n ? Duree(b, *n) : -1.0f;
	fin = d >= 0.0f ? t + d : -1.0f;
}

const CActionNode *
CActionController::Descendre(const CActionBank &b, const CActionNode *depuis, const CJugeAction &juge) const
{
	return depuis ? b.Choisir(depuis, juge) : nil;
}

bool
CActionController::Poser(const CActionBank &b, const CActionNode *a, const CJugeAction &juge, float t)
{
	arbre = a;
	const CActionNode *n = Descendre(b, a, juge);
	if(n == nil) return false;
	Jouer(b, n, t);
	return true;
}

bool
CActionController::Avancer(const CActionBank &b, const CActionNode *depuisRacine, const CJugeAction &juge, float t, bool demande)
{
	(void)depuisRacine;
	bool fini = joue && fin >= 0.0f && t >= fin;
	if(!fini && !demande) return false;
	if(joue){
		for(const CActionNode &e : joue->enfants){
			const CActionNode *n = Descendre(b, &e, juge);
			if(n && n != joue){ Jouer(b, n, t); return true; }
		}
	}
	if(arbre){
		const CActionNode *n = Descendre(b, arbre, juge);
		if(n && (n != joue || fini)){ Jouer(b, n, t); return true; }
	}
	return false;
}
