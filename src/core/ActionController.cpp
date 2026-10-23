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
	static const uint32 kCollision = ActionHash("Collision"), kExecute = ActionHash("Execute"), kDegats = ActionHash("DamageHandle"),
	                    kSante = ActionHash("SetHealth"), kPriseCible = ActionHash("SetTargetAsGrappleTarget"),
	                    kDrapeau = ActionHash("SetPedFlags");
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
		if(prof > 0){
			// Les pistes ajoutées comptent pour la fin du nœud joué comme les siennes (FUN_005f3e20 : une seule liste).
			float vie = Reel(p, 16, -1.0f), finAnim = FinAnimation(b, p);
			float bout = vie >= 0.0f ? a + vie : finAnim >= 0.0f ? entree + finAnim : -1.0f;
			if(bout > finExecutees) finExecutees = bout;
		}
		if(p.type == kCollision){
			const CActionAttribut *dr = p.Champ(44), *sur = p.Champ(104);
			Contact c{&n, entree + Reel(p, 24, 0.0f), entree + Reel(p, 28, -1.0f),
			          dr ? ActionHash(f->ChaineCitee(dr->decalage).c_str()) : 0u, Reel(p, 52, 0.0f),
			          sur ? f->ChaineCitee(sur->decalage) : std::string(), false};
			contacts.push_back(c);
		}else if(p.type == kDegats){
			{ const CActionAttribut *fx = p.Champ(28); degats.push_back({&n, a, Reel(p, 24, 0.0f), fx && fx->taille >= 1 && fx->valeur[0] != 0, false}); }
		}else if(p.type == kDrapeau){
			const CActionAttribut *v = p.Champ(24);
			float e = Reel(p, 12, 0.0f);
			drapeaux.push_back({e < 0.0f ? -1.0f : a, p.Mot(28), v && v->taille >= 1 ? (int32)v->valeur[0] : 0, false});
		}else if(p.type == kSante){
			const CActionAttribut *o0 = p.Champ(24), *o1 = p.Champ(25), *o2 = p.Champ(26);
			auto oct = [](const CActionAttribut *x){ return x && x->taille >= 1 && x->valeur[0] != 0; };
			int mode = oct(o0) ? 0 : oct(o1) ? 1 : oct(o2) ? 2 : -1;
			if(mode >= 0) santes.push_back({a, mode, Reel(p, 28, 0.0f), false});
		}else if(p.type == kPriseCible){
			const CActionAttribut *c1 = p.Champ(36), *c2 = p.Champ(52), *e1 = p.Champ(25), *e2 = p.Champ(26);
			auto oct = [](const CActionAttribut *x){ return x && x->taille >= 1 && x->valeur[0] != 0; };
			prisesCible.push_back({a, c1 ? f->ChaineCitee(c1->decalage) : std::string(), c2 ? f->ChaineCitee(c2->decalage) : std::string(),
			                       oct(e1), oct(e2), false});
			prisesCible.back().porteur = &n;
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
	for(Drapeau &d : drapeaux)    // FUN_005f3fb0 à l'arrêt du nœud : les entrées négatives se lancent
		if(!d.fait && d.entree < 0.0f){ d.fait = true; drapeauxPoses.push_back(d); }
	drapeaux.clear();
	joue = n;
	debut = t;
	occasions = n ? COpportunite::DuNoeud(b, *n) : std::vector<COpportunite>();
	suites.clear();
	surCible.clear();
	contacts.clear();
	degats.clear();
	suitesExecutees.clear();
	santes.clear();
	prisesCible.clear();
	finExecutees = -1.0f;
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
	if(finExecutees > d) d = finExecutees;
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
				// Un chemin qui remonte jusqu'à /Global (autant de « .. » que de niveaux sous /Global) vise un autre arbre
				// global en connaissance de cause (HumiliateFlee/…/RunStart → HitTree/KnockOuts/Rear/Rear) : il passe aussi.
				std::vector<const CActionNode *> lignee = b.Lignee(o.porteur);
				size_t montees = 0;
				for(size_t k = 0; o.chemin.compare(k, 2, "..") == 0; k += 3) montees++;
				bool versGlobal = lignee.size() > 2 && montees + 2 >= lignee.size();
				bool dedans = arbre == nil || Sous(*arbre, d) || versGlobal || (d && lignee.size() > 2 && Sous(*lignee[2], d));
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
	santesLancees.clear();
	prises.clear();
	for(Blessure &d : degats)     // FUN_0060fc70 : au lancement de la piste
		if(!d.fait && Temps(t) >= d.entree){ d.fait = true; blessures.push_back(d); }
	for(Drapeau &d : drapeaux)    // FUN_0060f8e0
		if(!d.fait && d.entree >= 0.0f && Temps(t) >= d.entree){ d.fait = true; drapeauxPoses.push_back(d); }
	for(Sante &d : santes)        // FUN_00627500
		if(!d.fait && Temps(t) >= d.entree){ d.fait = true; santesLancees.push_back(d); }
	for(PriseCible &d : prisesCible)   // FUN_0062a580
		if(!d.fait && Temps(t) >= d.entree){
			d.fait = true;
			const CActionNode *a = d.soi.empty() ? nil : b.Relatif(d.porteur, d.soi.c_str());
			const CActionNode *c = d.cible.empty() ? nil : b.Relatif(d.porteur, d.cible.c_str());
			if(a || c) prises.push_back({a, c, d.executerSoi, d.executerCible});
		}
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
		// FUN_005f7440 : à la fin du nœud, ses pistes s'arrêtent ; une Opportunity prise et pas encore arrêtée (bascule
		// -1 : elle attend la fin) dépose sa demande, la plus forte priorité l'emportant (FUN_005f3bc0). Supposé : à
		// priorité égale, les Sequence (priorité 0) passent avant.
		const CActionNode *fin = nil;
		int32 meilleure = 0;
		for(COpportunite &o : occasions)
			if(o.prise && !o.arretee){
				o.arretee = true;
				if(o.cible && (fin == nil ? o.priorite >= 0 : o.priorite > meilleure)){ fin = o.cible; meilleure = o.priorite; }
			}
		bool avantSuites = fin && meilleure > 0;
		if(avantSuites){ Jouer(b, fin, t, &juge); return true; }
		for(const std::string &chemin : suites){
			const CActionNode *d = b.Choisir(b.Relatif(joue, chemin.c_str()), juge);
			if(d){ Jouer(b, d, t, &juge); return true; }
		}
		for(const CActionNode *v : suitesExecutees){
			const CActionNode *d = b.Choisir(v, juge);
			if(d){ Jouer(b, d, t, &juge); return true; }
		}
		if(fin){ Jouer(b, fin, t, &juge); return true; }
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
