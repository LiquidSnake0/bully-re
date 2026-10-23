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

// AnimationTrack, mise à jour FUN_0060ae00 : en mode 0 (position 32), la piste finit quand le temps joué dépasse la
// fin : la position 44 si elle est positive et ne dépasse pas la durée de l'animation, sinon cette durée (HXD +0xc).
// Le temps part de la position 40 et avance à la vitesse 48 (la stat 0x14 du piéton n'est pas suivie). Les modes 1
// et 2 (boucle) ne finissent pas d'eux-mêmes. Rend l'instant de fin dans le nœud, ou -1.
// HitAnimation (HitAnimationTrack, table 0x934cb8, une AnimationTrack) : sa mise à jour FUN_00617040 passe d'abord par
// FUN_0060ae00, la même fin ; ses fichiers ne donnent pas la vitesse (48), qui vaut alors 1.
static float
FinAnimation(const CActionBank &b, const CActionTrack &p)
{
	static const uint32 kAnimation = ActionHash("Animation"), kCoup = ActionHash("HitAnimation");
	if((p.type != kAnimation && p.type != kCoup) || p.Mot(32) != 0) return -1.0f;
	float duree = b.DureeAnimation(p.Mot(24));
	if(duree <= 0.0f) return -1.0f;
	float fin = Reel(p, 44, -1.0f), debut = Reel(p, 40, 0.0f), vitesse = Reel(p, 48, 1.0f);
	if(fin < 0.0f || duree < fin) fin = duree;
	if(vitesse <= 0.0f) vitesse = 1.0f;
	float reste = (fin - debut) / vitesse;
	return Reel(p, 12, 0.0f) + (reste > 0.0f ? reste : 0.0f);
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
		float bout = vie >= 0.0f ? Reel(p, 12, 0.0f) + vie : FinAnimation(b, p);
		if(bout > d) d = bout;
	}
	return d;
}

// Les pistes d'un nœud que le contrôleur suit (Collision, DamageHandle et, pour un nœud exécuté, Opportunity), puis
// celles des nœuds que ses pistes Execute font jouer (FUN_005f7070 : la descente +0x4c du nœud visé, FUN_005f3e20 :
// ses pistes rejoignent la liste du contrôleur). entree : l'instant où le nœud a rejoint le nœud joué ; une piste
// ajoutée se lance quand le temps du nœud atteint son entrée (+12, FUN_005f3fb0), donc au plus tôt à `entree`.
void
CActionController::Pistes(const CActionBank &b, const CActionNode &n, float entree, int prof, const CJugeAction *juge)
{
	static const uint32 kCollision = ActionHash("Collision"), kExecute = ActionHash("Execute"), kDegats = ActionHash("DamageHandle");
	const CActionTreeFile *f = b.Fichier(n);
	if(f == nil || prof > 4) return;
	static const uint32 kSequence = ActionHash("Sequence");
	if(prof > 0)
		for(COpportunite &o : COpportunite::DuNoeud(b, n)){
			if(o.entree < entree) o.entree = entree;
			occasions.push_back(o);
		}
	for(int32 dp : n.pistes){
		CActionTrack p;
		if(!f->Piste(dp, p)) continue;
		float a = Reel(p, 12, 0.0f);
		if(a < entree) a = entree;
		if(p.type == kCollision){
			const CActionAttribut *dr = p.Champ(44), *sur = p.Champ(104);
			Contact c{&n, entree + Reel(p, 24, 0.0f), entree + Reel(p, 28, -1.0f),
			          dr ? ActionHash(f->ChaineCitee(dr->decalage).c_str()) : 0u, Reel(p, 52, 0.0f),
			          sur ? f->ChaineCitee(sur->decalage) : std::string(), false};
			contacts.push_back(c);
		}else if(p.type == kDegats){
			{ const CActionAttribut *fx = p.Champ(28); degats.push_back({&n, a, Reel(p, 24, 0.0f), fx && fx->taille >= 1 && fx->valeur[0] != 0, false}); }
		}else if(p.type == kSequence && prof > 0){
			// La Sequence d'un nœud exécuté (HitBaseStanding : ../../PostHit/Standing) s'arrête avec le nœud joué
			// (FUN_005f7030) : son chemin, relatif au nœud exécuté, devient absolu pour Avancer.
			const CActionAttribut *at = p.Champ(32);
			const CActionNode *v = at ? b.Relatif(&n, f->ChaineCitee(at->decalage).c_str()) : nil;
			if(v) suitesExecutees.push_back(v);
		}else if(p.type == kExecute){
			const CActionAttribut *at = p.Champ(32);
			if(at == nil) continue;
			const CActionNode *v = b.Relatif(&n, f->ChaineCitee(at->decalage).c_str());
			const CActionNode *d = v == nil ? nil : juge ? b.Choisir(v, *juge) : b.Choisir(v);
			if(d) Pistes(b, *d, a, prof + 1, juge);
		}
	}
}

void
CActionController::Jouer(const CActionBank &b, const CActionNode *n, float t, const CJugeAction *juge)
{
	joue = n;
	debut = t;
	occasions = n ? COpportunite::DuNoeud(b, *n) : std::vector<COpportunite>();
	suites.clear();
	surCible.clear();
	contacts.clear();
	degats.clear();
	suitesExecutees.clear();
	if(n) Pistes(b, *n, 0.0f, 0, juge);
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
	Jouer(b, r, t, &juge);
	return true;
}

bool
CActionController::Poser(const CActionBank &b, const CActionNode *a, const CJugeAction &juge, float t)
{
	arbre = a;
	const CActionNode *n = Descendre(b, a, juge);
	if(n == nil) return false;
	Jouer(b, n, t, &juge);
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
				// nœud joué (une remontée dans la pile) ne fait pas passer, ni un nœud hors de l'arbre posé, sauf s'il
				// est dans le même arbre global que le nœud qui porte la piste : un piéton entré dans /Global/HitTree
				// (un coup reçu, une Sequence) y suit ses Opportunity (BellyUp/On_Ground → ../BellyUpGetUp).
				std::vector<const CActionNode *> lignee = b.Lignee(o.porteur);
				bool dedans = arbre == nil || Sous(*arbre, d) || (d && lignee.size() > 2 && Sous(*lignee[2], d));
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
	touches.clear();
	blessures.clear();
	for(Blessure &d : degats)     // FUN_0060fc70 : au lancement de la piste
		if(!d.fait && Temps(t) >= d.entree){ d.fait = true; blessures.push_back(d); }
	for(Contact &c : contacts)    // FUN_005f9e70 : la fenêtre du coup
		if(!c.fait && Temps(t) >= c.debut && (c.fin < 0.0f || Temps(t) <= c.fin)){ c.fait = true; touches.push_back(c); }
	for(SurCible &c : surCible)   // FUN_006615f0
		if(!c.fait && Temps(t) >= c.entree){
			c.fait = true;
			if(const CActionNode *n = b.Relatif(joue, c.chemin.c_str())) pourCible.push_back(n);
		}
	if(joue){
		const CActionNode *n = Occasions(b, juge, t);
		if(n){ Jouer(b, n, t, &juge); return true; }
	}
	bool fini = joue && fin >= 0.0f && t >= fin;
	// FUN_005f7030 : les Sequence à la fin du nœud. Le nœud visé (FUN_005f4e30) peut être hors de l'arbre posé : la
	// poussée de la prise (PushFwd/RCV) envoie la cible dans HitTree/Standing/PostHit/BellyUp/OnGroundBounce.
	if(fini){
		for(const std::string &chemin : suites){
			const CActionNode *d = b.Choisir(b.Relatif(joue, chemin.c_str()), juge);
			if(d){ Jouer(b, d, t, &juge); return true; }
		}
		for(const CActionNode *v : suitesExecutees){
			const CActionNode *d = b.Choisir(v, juge);
			if(d){ Jouer(b, d, t, &juge); return true; }
		}
	}
	if(!fini && !demande) return false;
	if(joue){
		for(const CActionNode &e : joue->enfants){
			const CActionNode *n = Descendre(b, &e, juge);
			if(n && n != joue){ Jouer(b, n, t, &juge); return true; }
		}
	}
	if(arbre){
		const CActionNode *n = Descendre(b, arbre, juge);
		if(n && (n != joue || fini)){ Jouer(b, n, t, &juge); return true; }
	}
	return false;
}
