#include "ActionController.h"
#include "ConditionListe.h"
#include <cmath>
#include <cstring>

static bool
Octet(const CActionTrack &p, uint16 position)
{
	const CActionAttribut *a = p.Champ(position);
	return a && a->taille >= 1 && a->valeur[0] != 0;
}

static float
Reel(const CActionTrack &p, uint16 position, float defaut)
{
	if(p.Champ(position) == nil) return defaut;
	uint32 m = p.Mot(position);
	float r;
	memcpy(&r, &m, 4);
	return r;
}

// La clé du contexte (le haut de sa pile, +0x48 + 4 × +0x44) devient celle des parents (DAT_00b0a604) le temps d'un appel,
// comme FUN_005f3e20 et FUN_005f70f0 avant de résoudre les chemins des pistes.
struct CGardeCle {
	const CActionBank &b; uint32 avant;
	CGardeCle(const CActionBank &banque, uint32 k) : b(banque), avant(banque.cle) { if(k != 0xffffffffu) b.cle = k; }
	~CGardeCle(void){ b.cle = avant; }
};

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
	                    kDrapeau = ActionHash("SetPedFlags"), kParole = ActionHash("SpeechTrack"),
	                    kDepart = ActionHash("SpeechStartTrack"), kRegard = ActionHash("FaceTarget"),
	                    kPoursuite = ActionHash("SeekEntityPosition"), kSpawn = ActionHash("spawn"),
	                    kGestion = ActionHash("ManageTargets"), kCombatLoco = ActionHash("CombatLoco"),
	                    kDemandeNoeud = ActionHash("ActionNodeRequest"), kEffacer = ActionHash("ClearCurrentObjective"),
	                    kFuite = ActionHash("FleeObjective"), kRepos = ActionHash("IDLE"), kBouton = ActionHash("ActionRequest"),
	                    kInfoArme = ActionHash("SetWeaponInfo"), kMort = ActionHash("Death"),
	                    kDegatsCible = ActionHash("DamageTarget");
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
		if(Octet(p, 8)) retenu = true;   // FUN_005f4230 : une piste qui retient le nœud
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
			c.entree = a; c.vie = Reel(p, 16, -1.0f);
			c.debutBrut = Reel(p, 24, 0.0f); c.finBrut = Reel(p, 28, -1.0f);
			c.debutFenetre = entree + c.debutBrut; c.finFenetre = c.finBrut >= 0.0f ? entree + c.finBrut : -1.0f;
			c.agresseur = Octet(p, 532);
			contacts.push_back(c);
		}else if(p.type == kDegats){
			{ const CActionAttribut *fx = p.Champ(28); degats.push_back({&n, a, Reel(p, 24, 0.0f), fx && fx->taille >= 1 && fx->valeur[0] != 0, false}); }
		}else if(p.type == kDrapeau){
			const CActionAttribut *v = p.Champ(24);
			float e = Reel(p, 12, 0.0f);
			drapeaux.push_back({e < 0.0f ? -1.0f : a, p.Mot(28), v && v->taille >= 1 ? (int32)v->valeur[0] : 0, false});
		}else if(p.type == kParole){
			const CActionAttribut *l = p.Champ(28);
			float e = Reel(p, 12, 0.0f);
			paroles.push_back({e < 0.0f ? -1.0f : a, (int32)p.Mot(24), l && l->taille >= 1 && l->valeur[0] ? (int32)p.Mot(32) : -1,
			                   (int32)p.Mot(40), false});
		}else if(p.type == kDepart){
			departs.push_back({a, false});
		}else if(p.type == kRegard){
			regards.push_back({a, Reel(p, 16, -1.0f), (int32)p.Mot(32), Reel(p, 40, 0.0f)});
		}else if(p.type == kPoursuite){
			poursuites.push_back({a, Reel(p, 16, -1.0f), (int32)p.Mot(28), Reel(p, 40, 0.0f)});
		}else if(p.type == kEffacer){
			effacements.push_back({a, (int32)p.Mot(24), false});
		}else if(p.type == kFuite){
			fuites.push_back({a, Octet(p, 24), Octet(p, 25), false});
		}else if(p.type == kDegatsCible){
			float vie = Reel(p, 16, -1.0f);
			degatsCibles.push_back({a, vie < 0.0f ? -1.0f : vie - Reel(p, 12, 0.0f), Reel(p, 24, 0.0f), 0.0f, 0.0f, false});
		}else if(p.type == kMort){
			morts.push_back({Reel(p, 12, 0.0f) < 0.0f ? -1.0f : a, Octet(p, 24), false});
		}else if(p.type == kRepos){
			repos.push_back({a, false});
		}else if(p.type == kInfoArme){
			infosArme.push_back({a, Reel(p, 44, -1.0f), Reel(p, 64, 0.0f), false});
		}else if(sousControleurs && p.type == kBouton){
			demandesBouton.push_back({a, (int32)p.Mot(24), (int32)p.Mot(28), Octet(p, 32), Octet(p, 8), false, false});
		}else if(sousControleurs && (p.type == kGestion || p.type == kCombatLoco)){
			Gestion g{a, Reel(p, 16, -1.0f), Octet(p, 24), Octet(p, 8)};
			(p.type == kGestion ? gestions : combats).push_back(g);
		}else if(sousControleurs && p.type == kDemandeNoeud){
			demandesNoeud.push_back({a, p.Mot(24) & 0x7fffffff, Octet(p, 28), Octet(p, 29), Octet(p, 30), Octet(p, 8), false, true});
		}else if(sousControleurs && p.type == kSpawn){
			// FUN_005f8860 : le nœud visé (+0x6c), résolu d'avance depuis le nœud qui porte la piste (FUN_005f4e30).
			const CActionAttribut *at = p.Champ(32);
			const CActionNode *v = at ? b.Relatif(&n, f->ChaineCitee(at->decalage).c_str()) : nil;
			if(v) lances.push_back({a, Reel(p, 16, -1.0f), v, Octet(p, 41), false, false, nullptr});
		}else if(p.type == kSpawn){
			const CActionAttribut *at = p.Champ(32);
			if(at == nil) continue;
			const CActionNode *v = b.Relatif(&n, f->ChaineCitee(at->decalage).c_str());
			const CActionNode *d = v == nil ? nil : juge ? b.Choisir(v, *juge) : b.Choisir(v);
			if(d) PistesLancees(b, *d, a, prof + 1, juge);
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

// Un nœud qu'une piste spawn fait jouer à côté (lancement FUN_005f8860) : l'outil n'en garde que les pistes FaceTarget et
// SeekEntityPosition, qui vivent avec la piste spawn ; ce nœud n'a pas d'autre effet sur le nœud joué.
void
CActionController::PistesLancees(const CActionBank &b, const CActionNode &n, float entree, int prof, const CJugeAction *juge)
{
	static const uint32 kRegard = ActionHash("FaceTarget"), kPoursuite = ActionHash("SeekEntityPosition");
	const CActionTreeFile *f = b.Fichier(n);
	if(f == nil || prof > 4) return;
	for(int32 dp : n.pistes){
		CActionTrack p;
		if(!f->Piste(dp, p)) continue;
		float a = Reel(p, 12, 0.0f);
		if(a < entree) a = entree;
		if(p.type == kRegard) regards.push_back({a, Reel(p, 16, -1.0f), (int32)p.Mot(32), Reel(p, 40, 0.0f)});
		else if(p.type == kPoursuite) poursuites.push_back({a, Reel(p, 16, -1.0f), (int32)p.Mot(28), Reel(p, 40, 0.0f)});
	}
}

void
CActionController::Jouer(const CActionBank &b, const CActionNode *n, float t, const CJugeAction *juge)
{
	CGardeCle gardeCle(b, Cle());
	for(Drapeau &d : drapeaux)    // FUN_005f3fb0 à l'arrêt du nœud : les entrées négatives se lancent
		if(!d.fait && d.entree < 0.0f){ d.fait = true; drapeauxPoses.push_back(d); }
	drapeaux.clear();
	for(DegatsCible &d : degatsCibles)   // FUN_0060fbe0 : l'arrêt d'une piste lancée
		if(!d.finie && joue && Temps(t) >= d.entree && !(d.duree > 0.0f && Temps(t) - d.entree < d.duree)){
			coupsCible.push_back(d.total - d.fait); d.fait = d.total; d.finie = true;
		}
	degatsCibles.clear();
	for(Mort &d : morts)          // FUN_0060fdc0, à l'arrêt du nœud
		if(!d.fait && d.entree < 0.0f){ d.fait = true; mortsLancees.push_back(d); }
	morts.clear();
	for(Parole &d : paroles)
		if(!d.fait && d.entree < 0.0f){ d.fait = true; parolesLancees.push_back(d); }
	paroles.clear();
	departs.clear();
	regards.clear();
	poursuites.clear();
	ArreterCollisions();
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
	gestions.clear(); combats.clear(); demandesNoeud.clear();
	for(const DemandeBouton &d : demandesBouton)   // FUN_00626160 : l'arrêt d'une piste au mode 2 lâche son bouton
		if(!d.morte && d.lancee && d.mode == 2) laches.push_back(d.bouton);
	demandesBouton.clear();
	effacements.clear(); fuites.clear(); repos.clear(); infosArme.clear();
	lances.clear();                    // FUN_005f7390 : l'arrêt d'une piste spawn arrête son contrôleur
	finExecutees = -1.0f;
	retenu = false;
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
	// Un Execute ne retient pas son nœud : sa mise à jour (FUN_005f70f0) rend +0x31, posé seulement pour une durée de vie
	// positive ; seules les pistes du nœud exécuté le font (FUN_005f3e20). Un nœud qui n'a que des Execute et des
	// Sequence, et dont aucun Execute n'a ajouté de piste (le chemin ne se résout pas), finit donc tout de suite : la
	// charge d'une prise sans le drapeau 70 (GrappleStrikes/…/GIVE/#542875ee, « Not PedFlags ») passe à ../Give.
	if(n && d < 0.0f && finExecutees < 0.0f && !n->pistes.empty())
		if(const CActionTreeFile *f = b.Fichier(*n)){
			static const uint32 kExecute = ActionHash("Execute"), kSequence = ActionHash("Sequence");
			bool instantane = true;
			for(int32 dp : n->pistes){
				CActionTrack p;
				if(!f->Piste(dp, p) || (p.type != kSequence && !(p.type == kExecute && Reel(p, 16, -1.0f) <= 0.0f))){ instantane = false; break; }
			}
			if(instantane) d = 0.0f;
		}
	// FUN_005f4230 : un nœud qu'aucune de ses pistes (ni de celles que ses Execute ajoutent) ne retient (l'octet +8) finit à
	// sa première mise à jour. GV/END d'une humiliation (SetMotionIntention, PlayOnTarget, TargetGrappleRelease, Execute)
	// rend ainsi la main à l'arbre du joueur.
	// Exception de l'outil : un nœud que sa manette fait jouer (lancer_lua --jouer) tient lieu d'un bouton tenu, qui le
	// retient dans le jeu (0_BLOCK_0, la parade de 1_02, tant que le bouton est tenu) : il garde la règle d'avant.
	if(n && d < 0.0f && !retenu && !parLaManette) d = 0.0f;
	fin = d >= 0.0f ? t + d : -1.0f;
	finParRetenues = false;
	if(fin < 0.0f){
		for(const Gestion &g : gestions) finParRetenues |= g.retient;
		for(const Gestion &g : combats) finParRetenues |= g.retient;
		for(const DemandeNoeud &g : demandesNoeud) finParRetenues |= g.retient;
		for(const DemandeBouton &g : demandesBouton) finParRetenues |= g.retient;
		if(sousControleurs) for(const COpportunite &o : occasions) finParRetenues |= o.retient;
	}
}

void
CActionController::Racine(const CActionBank &b, const CActionNode *r)
{
	if(r == nil) return;                  // FUN_005f36d0 : sans racine, la pile ne change pas
	pile.clear();
	pile.push_back(b.CleDe(r));           // FUN_005f4e40 de la racine
}

void
CActionController::Empiler(uint32 k)
{
	if(pile.size() < 2) pile.push_back(k);   // FUN_005f3700 : deux places au plus
}

void
CActionController::Recueillir(std::vector<CActionController *> &tous)
{
	tous.push_back(this);
	for(Lance &l : lances) if(l.lancee && !l.morte && l.c) l.c->Recueillir(tous);
}

// Le contrôleur qui juge, posé dans le contexte le temps d'un appel (le +0x10 du contexte de FUN_005fb9b0).
struct CGardeControleur {
	const CJugeAction &juge; const CActionController *avant;
	CGardeControleur(const CJugeAction &j, const CActionController *c) : juge(j), avant(j.controleur) { j.controleur = c; }
	~CGardeControleur(void){ juge.controleur = avant; }
};

const CActionNode *
CActionController::Descendre(const CActionBank &b, const CActionNode *depuis, const CJugeAction &juge) const
{
	return depuis ? b.Choisir(depuis, juge) : nil;
}

bool
CActionController::Entrer(const CActionBank &b, const CActionNode *n, const CJugeAction &juge, float t)
{
	if(n == nil) return false;
	CGardeControleur garde(juge, this);
	CGardeCle gardeCle(b, Cle());
	const CActionNode *r = n->genre == 'b' ? b.Choisir(n, juge) : n;
	if(r == nil) return false;
	Jouer(b, r, t, &juge);
	return true;
}

bool
CActionController::Poser(const CActionBank &b, const CActionNode *a, const CJugeAction &juge, float t)
{
	arbre = a;
	Racine(b, a);
	CGardeControleur garde(juge, this);
	CGardeCle gardeCle(b, Cle());
	const CActionNode *n = Descendre(b, a, juge);
	if(n == nil) return false;
	Jouer(b, n, t, &juge);
	return true;
}


const CActionNode *
CActionController::Occasions(const CActionBank &b, const CJugeAction &juge, float t)
{
	demandeNoeud = nil;
	demandePriorite = -1;
	finDemandee = false;
	float temps = Temps(t);
	for(COpportunite &o : occasions){
		if(o.arretee || temps < o.entree) continue;
		bool arret = o.vie >= 0.0f && temps > o.entree + o.vie;   // la durée de vie, comme toute piste
		if(!arret && !o.prise && TirageOpportunite(o.periode, graine, image)){
			// Les conditions propres de la piste (+0x3c), jugées d'abord (FUN_005fb9b0) : faute de les passer, rien.
			// Sinon la piste n'est pas prise à cette image, et sa fermeture se juge comme d'habitude.
			const CActionTreeFile *fp = o.listeConditions.empty() ? nil : b.Fichier(*o.porteur);
			bool permise = fp == nil || b.JugerListe(*fp, o.listeConditions, juge) == 1;
			const CActionNode *visee = o.chemin.empty() ? nil : b.Relatif(o.porteur, o.chemin.c_str());
			if(!permise)
				;
			else if(visee == nil){
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
				bool dedans = arbre == nil || b.Sous(arbre, d) || versGlobal || (d && lignee.size() > 2 && b.Sous(lignee[2], d));
				if(d && dedans && !b.Sous(d, joue) && (d != joue || o.soiMeme)){ o.cible = d; o.prise = true; }
			}
		}
		if(!arret){
			if(!o.prise) arret = o.fermeture > 0.0f && o.fermeture < temps;
			else arret = o.bascule >= 0.0f && temps >= o.bascule;
		}
		if(!arret) continue;
		o.arretee = true;
		if(o.prise) finDemandee = true;                   // FUN_005f7440 : FUN_005f3bc0(nœud, priorité, 1)
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
	CGardeControleur garde(juge, this);
	CGardeCle gardeCle(b, Cle());
	intentions.clear();
	// FUN_00627420 (ManageTargets) et FUN_0062ea40 (CombatLoco), à chaque image, tant que le piéton a une cible.
	gestionsActives.clear(); combatsActifs.clear();
	for(int k = 0; k < 2; k++)
		for(Gestion &g : k ? combats : gestions){
			if(Temps(t) < g.entree || g.vie == -2.0f) continue;
			if((g.vie >= 0.0f && Temps(t) > g.entree + g.vie) || !juge.ACible()){ g.vie = -2.0f; continue; }   // morte
			(k ? combatsActifs : gestionsActives).push_back(g);
		}
	// FUN_006260d0 (ActionRequest) : le bouton appuyé à chaque image ; la piste meurt sans l'octet +0x20.
	appuis.clear();
	for(DemandeBouton &d : demandesBouton){
		if(d.morte || Temps(t) < d.entree) continue;
		appuis.push_back({d.bouton, d.mode});
		d.lancee = true;
		if(!d.vivante){ d.morte = true; if(d.mode == 2) laches.push_back(d.bouton); }   // morte : son arrêt la lâche
	}
	// FUN_006261f0 (ActionNodeRequest) : l'intention, puis la vie de la piste.
	for(DemandeNoeud &d : demandesNoeud){
		if(!d.vivante || Temps(t) < d.entree) continue;
		bool vivante;
		if(!d.vu){
			intentions.push_back(d.nom);
			d.vu = juge.JoueNom(d.nom) == 1;
			if(!d.vu){ d.vivante = d.tenir || d.suivre; continue; }
		}
		if(d.suivre){
			if(juge.JoueNom(d.nom) == 1) continue;
			d.vu = false;
		}
		vivante = d.rendre;
		d.vivante = vivante;
	}
	// FUN_005f8860 / FUN_005f72e0 : les contrôleurs des pistes spawn.
	for(Lance &l : lances){
		if(l.morte || Temps(t) < l.entree) continue;
		if(l.vie >= 0.0f && Temps(t) > l.entree + l.vie){ l.morte = true; l.c.reset(); continue; }
		if(!l.lancee){
			l.lancee = true;
			l.c = std::make_shared<CActionController>();
			l.c->sousControleurs = true;
			l.c->arbre = l.boucle ? l.vise : nil;   // +0x29 : redescendre depuis le nœud visé quand le nœud finit
			l.c->Racine(b, l.vise);                 // FUN_005f8860 : la racine du contexte de la piste (+0x40)
			l.c->image = image; l.c->graine = graine; l.c->hasard = hasard;
			if(!l.c->Entrer(b, l.vise, juge, t)){ if(!l.boucle){ l.morte = true; l.c.reset(); } }
			continue;
		}
		l.c->image = image; l.c->graine = graine;
		if(l.c->joue == nil){ if(l.boucle) l.c->Entrer(b, l.vise, juge, t); continue; }
		const CActionNode *avant = l.c->joue;
		l.c->Avancer(b, nil, juge, t);
		(void)avant;
	}
	pourCible.clear();
	touches.clear();
	blessures.clear();
	santesLancees.clear();
	prises.clear();
	for(Blessure &d : degats)     // FUN_0060fc70 : au lancement de la piste
		if(!d.fait && Temps(t) >= d.entree){ d.fait = true; blessures.push_back(d); }
	float pas = dernierAvancer >= 0.0f && t > dernierAvancer ? t - dernierAvancer : 0.033f;
	dernierAvancer = t;
	for(DegatsCible &d : degatsCibles){   // FUN_0060fad0, puis FUN_0060fbe0 à la fin de sa durée
		if(d.finie || Temps(t) < d.entree) continue;
		if(d.duree < 0.0f){ coupsCible.push_back(d.total - d.fait); d.fait = d.total; d.finie = true; continue; }
		if(Temps(t) - d.entree >= d.duree){ coupsCible.push_back(d.total - d.fait); d.fait = d.total; d.finie = true; continue; }
		float reste = d.duree - d.ecoule;
		if(reste < 0.0f && d.ecoule == 0.0f){ d.finie = true; continue; }
		float part = reste <= pas ? 0.0f : pas / reste, v = (d.total - d.fait) * part;
		if(v != 0.0f) coupsCible.push_back(v);
		d.fait += v; d.ecoule += pas;
	}
	for(Mort &d : morts)          // FUN_0060fdc0
		if(!d.fait && d.entree >= 0.0f && Temps(t) >= d.entree){ d.fait = true; mortsLancees.push_back(d); }
	for(Drapeau &d : drapeaux)    // FUN_0060f8e0
		if(!d.fait && d.entree >= 0.0f && Temps(t) >= d.entree){ d.fait = true; drapeauxPoses.push_back(d); }
	regardsActifs.clear();
	poursuitesActives.clear();
	for(const Regard &r : regards)          // FUN_00620dc0, tant que la piste vit
		if(Temps(t) >= r.entree && (r.vie < 0.0f || Temps(t) <= r.entree + r.vie)) regardsActifs.push_back(r);
	for(const Poursuite &r : poursuites)    // FUN_0062a940
		if(Temps(t) >= r.entree && (r.vie < 0.0f || Temps(t) <= r.entree + r.vie)) poursuitesActives.push_back(r);
	for(Parole &d : paroles)      // FUN_006195a0
		if(!d.fait && d.entree >= 0.0f && Temps(t) >= d.entree){ d.fait = true; parolesLancees.push_back(d); }
	for(Depart &d : departs)      // FUN_00619950
		if(!d.fait && Temps(t) >= d.entree){ d.fait = true; departsLances++; }
	effacementsLances.clear(); fuitesLancees.clear();
	for(Effacement &d : effacements)   // FUN_0062a180
		if(!d.fait && Temps(t) >= d.entree){ d.fait = true; effacementsLances.push_back(d); }
	for(Fuite &d : fuites)        // FUN_00628ce0
		if(!d.fait && Temps(t) >= d.entree){ d.fait = true; fuitesLancees.push_back(d); }
	infosArmeLancees.clear();
	for(InfoArme &d : infosArme)      // FUN_00660f50
		if(!d.fait && Temps(t) >= d.entree){ d.fait = true; infosArmeLancees.push_back(d); }
	reposLances = 0;
	for(Repos &d : repos)         // FUN_0061d8e0
		if(!d.fait && Temps(t) >= d.entree){ d.fait = true; reposLances++; }
	for(Sante &d : santes)        // FUN_00627500
		if(!d.fait && Temps(t) >= d.entree){ d.fait = true; santesLancees.push_back(d); }
	for(PriseCible &d : prisesCible)   // FUN_0062a580
		if(!d.fait && Temps(t) >= d.entree){
			d.fait = true;
			const CActionNode *a = d.soi.empty() ? nil : b.Relatif(d.porteur, d.soi.c_str());
			const CActionNode *c = d.cible.empty() ? nil : b.Relatif(d.porteur, d.cible.c_str());
			if(a || c) prises.push_back({a, c, d.executerSoi, d.executerCible});
		}
	PhasesCollision(t);
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
	if(joue && finParRetenues){   // FUN_005f4230 : plus aucune piste vivante qui retienne le nœud
		bool retenu = false;
		for(const Gestion &g : gestions) retenu |= g.retient && g.vie != -2.0f;
		for(const Gestion &g : combats) retenu |= g.retient && g.vie != -2.0f;
		for(const DemandeNoeud &g : demandesNoeud) retenu |= g.retient && g.vivante;
		for(const DemandeBouton &g : demandesBouton) retenu |= g.retient && !g.morte;
		if(sousControleurs) for(const COpportunite &o : occasions) retenu |= o.retient && !o.arretee;
		fini = !retenu;
	}
	// Une Opportunity prise sans nœud (un chemin vide) a demandé la fin du nœud (FUN_005f49a0 : l'octet +1, quelles que
	// soient les pistes qui le retiennent) : il finit comme les autres, ses Sequence comprises.
	if(joue && finDemandee) fini = true;
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

// Les millisecondes du jeu (DAT_00c1a9b4) plus `s` secondes, arrondies comme le fistp du moteur (le double 1000 de 0x900138).
static uint32
EcheanceMs(float t, float s)
{
	return (uint32)lrintf(t * 1000.0f + s * 1000.0f);
}

void
CActionController::PhasesCollision(float t)
{
	float temps = Temps(t);
	maintenantMs = EcheanceMs(t, 0.0f);
	for(Contact &c : contacts){
		if(c.arretee) continue;
		if(!c.lancee){
			if(temps < c.entree) continue;
			// FUN_005f9480 : la phase 1, l'octet +0x4e5 et l'échéance au début de la fenêtre (+0x18, relatif au nœud).
			c.lancee = true;
			phaseAttaque = 1;
			collisionDemarree = true;
			echeanceAttaque = EcheanceMs(t, c.debutBrut);
		}
		// La durée de vie (+0x10) passée, la piste meurt : son arrêt (FUN_005f8240).
		if(c.vie >= 0.0f && temps > c.entree + c.vie){ c.arretee = true; phaseAttaque = 0; minuterieAttaque = EcheanceMs(t, 0.0f); continue; }
		// FUN_005f9e70.
		if(temps < c.debutFenetre) continue;
		if(c.finFenetre >= 0.0f && temps > c.finFenetre){
			phaseAttaque = 3;
			float reste = c.vie > 0.0f ? c.vie : fin >= 0.0f && fin > t ? fin - t : 0.0f;
			echeanceAttaque = EcheanceMs(t, reste);
		}else{
			phaseAttaque = 2;
			echeanceAttaque = EcheanceMs(t, c.finBrut);
		}
	}
}

void
CActionController::ArreterCollisions(void)
{
	for(Contact &c : contacts)
		if(c.lancee && !c.arretee){ c.arretee = true; phaseAttaque = 0; minuterieAttaque = maintenantMs; }
}

bool
ConditionCollisionStart(const CActionController &c)
{
	return c.collisionDemarree;
}

bool
ConditionAttackPhase(const CActionController &c, int32 phase, float delai, uint32 maintenantMs)
{
	if(phase != c.phaseAttaque) return false;
	if(delai <= 0.0f) return true;
	if(c.echeanceAttaque == 0) return false;
	return (uint32)lrintf((float)maintenantMs + delai * 1000.0f) < c.echeanceAttaque;
}

bool
ConditionAttackTimer(CActionController &c, float delai, bool relancer, uint32 comparateur, uint32 maintenantMs)
{
	if(c.minuterieAttaque == 0 && maintenantMs < 2001) return false;
	bool vrai = ComparerCondition((float)maintenantMs, (float)c.minuterieAttaque + delai * 1000.0f, comparateur);
	if(vrai && relancer) c.minuterieAttaque = maintenantMs;
	return vrai;
}
