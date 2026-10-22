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
	occasions = n ? COpportunite::DuNoeud(b, *n) : std::vector<COpportunite>();
	suites.clear();
	surCible.clear();
	if(n){
		static const uint32 kSequence = ActionHash("Sequence"), kSurCible = ActionHash("PlayOnTarget");
		if(const CActionTreeFile *f = b.Fichier(*n))
			for(int32 dp : n->pistes){
				CActionTrack p;
				if(!f->Piste(dp, p)) continue;
				const CActionAttribut *a = p.Champ(32);
				if(a == nil) continue;
				if(p.type == kSequence) suites.push_back(f->ChaineCitee(a->decalage));
				else if(p.type == kSurCible) surCible.push_back({Reel(p, 12, 0.0f), f->ChaineCitee(a->decalage), false});
			}
	}
	for(COpportunite &o : occasions)   // FUN_005f7510 : la bascule d'un loquet tirée au lancement
		if(o.loquet) o.bascule = (float)RandCrt(hasard) / 32767.0f * (o.loquetMax - o.loquetMin) + o.loquetMin;
	float d = n ? Duree(b, *n) : -1.0f;
	fin = d >= 0.0f ? t + d : -1.0f;
}

const CActionNode *
CActionController::Descendre(const CActionBank &b, const CActionNode *depuis, const CJugeAction &juge) const
{
	return depuis ? b.Choisir(depuis, juge) : nil;
}

bool
CActionController::Entrer(const CActionBank &b, const CActionNode *n, const CJugeAction &juge, float t)
{
	if(n == nil) return false;
	const CActionNode *r = n->genre == 'b' ? b.Choisir(n, juge) : n;
	if(r == nil) return false;
	Jouer(b, r, t);
	return true;
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

// n est `a` ou un de ses descendants.
static bool
Sous(const CActionNode &a, const CActionNode *n)
{
	if(&a == n) return true;
	for(const CActionNode &e : a.enfants) if(Sous(e, n)) return true;
	return false;
}

const CActionNode *
CActionController::Occasions(const CActionBank &b, const CJugeAction &juge, float t)
{
	demandeNoeud = nil;
	demandePriorite = -1;
	float temps = Temps(t);
	for(COpportunite &o : occasions){
		if(o.arretee || temps < o.entree) continue;
		bool arret = o.vie >= 0.0f && temps > o.entree + o.vie;   // la durée de vie, comme toute piste
		if(!arret && !o.prise && TirageOpportunite(o.periode, graine, image)){
			// Les conditions propres de la piste (+0x3c) : nulles dans les fichiers relevés, non jugées.
			const CActionNode *visee = o.chemin.empty() ? nil : b.Relatif(o.porteur, o.chemin.c_str());
			if(visee == nil){
				if(o.chemin.empty()) o.prise = true;           // sans chemin : prise, sans nœud
			}else{
				const CActionNode *d = b.Choisir(visee, juge);  // l'emplacement +0x4c du nœud visé
				// Limites de l'outil : le moteur garde la pile des nœuds joués (+0x44, +0x48) et monte les références
				// ('r', GlobalLoco sous TrainingPlayer) ; l'outil ne les a pas. Un nœud visé qui contient déjà le
				// nœud joué (une remontée dans la pile) ne fait pas passer, ni un nœud hors de l'arbre posé.
				bool dedans = arbre == nil || Sous(*arbre, d);
				if(d && dedans && !Sous(*d, joue) && (d != joue || o.soiMeme)){ o.cible = d; o.prise = true; }
			}
		}
		if(!arret){
			if(!o.prise) arret = o.fermeture > 0.0f && o.fermeture < temps;
			else arret = o.bascule >= 0.0f && temps >= o.bascule;
		}
		if(!arret) continue;
		o.arretee = true;
		if(o.prise && o.priorite > demandePriorite){       // FUN_005f3bc0
			demandePriorite = o.priorite;
			demandeNoeud = o.cible;
		}
	}
	return demandeNoeud;
}

bool
CActionController::Avancer(const CActionBank &b, const CActionNode *depuisRacine, const CJugeAction &juge, float t, bool demande)
{
	(void)depuisRacine;
	pourCible.clear();
	for(SurCible &c : surCible)   // FUN_006615f0
		if(!c.fait && Temps(t) >= c.entree){
			c.fait = true;
			if(const CActionNode *n = b.Relatif(joue, c.chemin.c_str())) pourCible.push_back(n);
		}
	if(joue){
		const CActionNode *n = Occasions(b, juge, t);
		if(n){ Jouer(b, n, t); return true; }
	}
	bool fini = joue && fin >= 0.0f && t >= fin;
	// FUN_005f7030 : les Sequence à la fin du nœud. Le nœud visé (FUN_005f4e30) peut être hors de l'arbre posé : la
	// poussée de la prise (PushFwd/RCV) envoie la cible dans HitTree/Standing/PostHit/BellyUp/OnGroundBounce.
	if(fini)
		for(const std::string &chemin : suites){
			const CActionNode *d = b.Choisir(b.Relatif(joue, chemin.c_str()), juge);
			if(d){ Jouer(b, d, t); return true; }
		}
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
