#include "ScriptPedPool.h"
#include <cmath>
#include <algorithm>

float
ScriptPedVitesse(int allure)
{
	switch(allure){
	case ALLURE_MARCHE: return 1.4f;
	case ALLURE_MARCHE_VITE: return 2.0f;
	case ALLURE_COURSE: return 3.5f;
	default: return 5.0f;
	}
}

float
ScriptPedIntentionVitesse(int allure, float defaut)
{
	switch(allure){
	case 0: return 0.3f;       // _DAT_00911e30
	case 1: return 0.7f;       // _DAT_00900e00
	case 2: return 1.1f;       // _DAT_0090cec8
	case 3: return 1.5f;       // _DAT_0090fb9c
	case 4: return -1.0f;      // _DAT_009009ac
	default: return defaut;
	}
}

CScriptPedPool::CScriptPedPool(void)
{
	// Le joueur existe toujours, à l'entrée 0.
	peds[SCRIPT_PED_JOUEUR].vivant = true;
	peds[SCRIPT_PED_JOUEUR].modele = 0;
	peds[SCRIPT_PED_JOUEUR].type = SCRIPT_PED_TYPE_JOUEUR;   // le modèle 0 (player) est PLAYER1
}

int
CScriptPedPool::Creer(int32 modele, const CVector &pos, float cap, int32 script)
{
	for(int i = 1; i < SCRIPT_PED_POOL_SIZE; i++){
		if(peds[i].vivant) continue;
		peds[i] = CScriptPed();
		peds[i].vivant = true;
		peds[i].modele = modele;
		peds[i].pos = pos;
		peds[i].cap = cap;
		peds[i].script = script;
		return i;
	}
	return -1;
}

// Un piéton qui disparaît quitte les zones de combat où il se tenait (supposé : le destructeur de CPed le fait).
static void
QuitterZones(CScriptPed *peds, int h)
{
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++)
		for(std::vector<int> &z : peds[i].combattants)
			for(size_t k = 0; k < z.size(); ) if(z[k] == h) z.erase(z.begin() + k); else k++;
}

// Un piéton qui disparaît : les références que les autres tiennent sur lui (FUN_00465680 les inscrit, le destructeur de
// l'entité les remet à nul) : le meneur +0x2f4, le suiveur +0x2f8 et le piéton de la laisse +0x35c.
static void
OublierReferences(CScriptPed *peds, int h)
{
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++){
		if(peds[i].meneur == h) peds[i].meneur = -1;
		if(peds[i].suiveur == h) peds[i].suiveur = -1;
		if(peds[i].laissePieton == h) peds[i].laissePieton = -1;
	}
}

bool
CScriptPedPool::Supprimer(int h)
{
	if(h == SCRIPT_PED_JOUEUR || !Valide(h)) return false;
	peds[h] = CScriptPed();
	QuitterZones(peds, h);
	OublierReferences(peds, h);
	return true;
}

bool
CScriptPedPool::Valide(int h) const
{
	return h >= 0 && h < SCRIPT_PED_POOL_SIZE && peds[h].vivant;
}

CScriptPed *
CScriptPedPool::Acceder(int h)
{
	return Valide(h) ? &peds[h] : nil;
}

const CScriptPed *
CScriptPedPool::Acceder(int h) const
{
	return Valide(h) ? &peds[h] : nil;
}

int
CScriptPedPool::SupprimerDuScript(int32 script)
{
	int n = 0;
	for(int i = 1; i < SCRIPT_PED_POOL_SIZE; i++)
		if(peds[i].vivant && peds[i].script == script){ peds[i] = CScriptPed(); QuitterZones(peds, i); OublierReferences(peds, i); n++; }
	return n;
}

int
CScriptPedPool::Vivants(void) const
{
	int n = 0;
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++) n += peds[i].vivant;
	return n;
}

bool
CScriptPedPool::Aller(int h, const CVector &cible, int allure)
{
	CScriptPed *p = Acceder(h);
	if(p == nil || p->mort) return false;
	p->enMarche = true;
	p->cible = cible;
	p->vitesse = ScriptPedVitesse(allure);
	p->attaque = -1;
	p->cap = atan2f(cible.y - p->pos.y, cible.x - p->pos.x) * 180.0f / 3.14159265f;
	{
		float dx = cible.x - p->pos.x, dy = cible.y - p->pos.y, n = sqrtf(dx * dx + dy * dy);
		if(n > 0) p->intention = CVector(dx / n, dy / n, 0);
	}
	p->intentionVitesse = ScriptPedIntentionVitesse(allure);
	return true;
}

bool
CScriptPedPool::Arreter(int h)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return false;
	p->enMarche = false;
	p->attaque = -1;
	p->intentionVitesse = 0;              // FUN_004a45c0
	return true;
}

bool
CScriptPedPool::Attaquer(int h, int cible, int32 style, bool devant)
{
	CScriptPed *p = Acceder(h);
	const CScriptPed *c = Acceder(cible);
	if(p == nil || c == nil || p->mort || h == cible) return false;
	p->attaque = cible;
	p->enMarche = false;
	p->prochainCoup = 0;
	CObjectifPieton o;
	o.type = OBJECTIF_ATTAQUER_PIETON; o.cible = cible; o.style = style;
	PousserObjectif(h, o, devant);
	return true;
}

// Deux objectifs qui reviennent au même pour la file : deux attaques (type 7) de la même cible (+0x14).
static bool
MemeObjectif(const CObjectifPieton &a, const CObjectifPieton &b)
{
	return a.type == OBJECTIF_ATTAQUER_PIETON && b.type == OBJECTIF_ATTAQUER_PIETON && a.cible == b.cible;
}

bool
CScriptPedPool::PousserObjectif(int h, const CObjectifPieton &o, bool devant)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return false;
	int &n = p->nombreObjectifs;
	if(n != 0 && MemeObjectif(o, p->objectifs[devant ? 0 : n - 1])) return true;
	if(n >= SCRIPT_PED_OBJECTIFS) return false;
	if(devant){
		for(int i = n; i > 0; i--) p->objectifs[i] = p->objectifs[i - 1];
		p->objectifs[0] = o;
	}else
		p->objectifs[n] = o;
	n++;
	return true;
}

bool
CScriptPedPool::JoindreZoneCombat(int h, int cible, int zone, bool essai)
{
	CScriptPed *p = Acceder(h), *c = Acceder(cible);
	if(p == nil || c == nil || h == cible) return false;
	if(zone >= 0 && zone <= 2 && !p->masqueZones[zone]) return false;
	if(!essai && p->verrou != cible) Verrouiller(h, cible, 0);
	if(zone < 0 || zone > 2) return false;
	std::vector<int> &z = c->combattants[zone];
	for(int x : z) if(x == h) return true;
	if((int)z.size() >= c->capaciteZones[zone]) return false;
	if(essai) return true;
	for(int k = 0; k < 3; k++)
		for(size_t i = 0; i < c->combattants[k].size(); )
			if(c->combattants[k][i] == h) c->combattants[k].erase(c->combattants[k].begin() + i); else i++;
	z.push_back(h);
	p->zoneCombat = zone;
	return true;
}

int
CScriptPedPool::RetirerObjectifs(int h, int32 type)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return 0;
	int retires = 0;
	for(int i = 0; i < p->nombreObjectifs; i++)
		if(p->objectifs[i].type == type){
			for(int k = i; k + 1 < p->nombreObjectifs; k++) p->objectifs[k] = p->objectifs[k + 1];   // FUN_0046fe40
			p->nombreObjectifs--;
			retires++;
		}
	return retires;
}

const CObjectifPieton *
CScriptPedPool::ObjectifCourant(int h) const
{
	const CScriptPed *p = Acceder(h);
	return p && p->nombreObjectifs > 0 ? &p->objectifs[0] : nil;
}

bool
CScriptPedPool::Verrouiller(int h, int cible, int mode)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return false;
	if(cible == -1){ LacherDroits(h); p->verrou = -1; p->verrouMode = 0; return true; }
	if(Acceder(cible) == nil) return false;
	if(cible != p->verrou) LacherDroits(h);   // FUN_00497d20 → FUN_00486c00, avant de changer de cible
	p->verrou = cible;
	p->verrouMode = mode;
	return true;
}

void
CScriptPedPool::ReleverProches(int h, bool refaire)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return;
	auto d2 = [&](int i){ const CScriptPed *q = Acceder(i); float dx = q->pos.x - p->pos.x, dy = q->pos.y - p->pos.y; return dx * dx + dy * dy; };
	if(refaire){
		std::vector<int> v;
		for(int i = SCRIPT_PED_POOL_SIZE - 1; i >= 0; i--){   // le pool parcouru du dernier au premier
			const CScriptPed *q = Acceder(i);
			if(q == nil || i == h || q->mort || v.size() >= 50) continue;
			if(d2(i) - 900.0f <= 0.0f) v.push_back(i);
		}
		std::stable_sort(v.begin(), v.end(), [&](int a, int b){ return d2(a) < d2(b); });   // FUN_004753e0
		if(v.size() > 16) v.resize(16);
		p->proches = v;
		return;
	}
	for(size_t k = 0; k < p->proches.size(); ){
		if(Acceder(p->proches[k]) == nil || d2(p->proches[k]) - 900.0f > 0.0f) p->proches.erase(p->proches.begin() + k);   // FUN_0047d0d0
		else k++;
	}
	for(size_t k = 0; k + 1 < p->proches.size(); k++)    // une passe, FUN_0047d130
		if(d2(p->proches[k + 1]) < d2(p->proches[k])) std::swap(p->proches[k], p->proches[k + 1]);
}

void
CScriptPedPool::PoserJetons(int h, const int32 nombres[4])
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return;
	for(int k = 0; k < 4; k++){
		p->nombreJetons[k] = nombres[k] < 0 ? 0 : nombres[k] > 6 ? 6 : nombres[k];
		for(int i = 0; i < 6; i++){ p->jetons[k][i] = CScriptPed::Jeton(); p->jetons[k][i].categorie = k; }
	}
	p->jetonCone = CScriptPed::Jeton();
}

// FUN_004867e0 : le piéton tient un jeton de la cible (une entrée ou la place du cône), coupé ou non.
static bool
TientUnJeton(const CScriptPed &c, int qui)
{
	for(int k = 0; k < 4; k++) for(int i = 0; i < c.nombreJetons[k]; i++) if(c.jetons[k][i].detenteur == qui) return true;
	return c.jetonCone.detenteur == qui;
}

// FUN_00486850 : son entrée, ou la place du cône, ou rien.
static CScriptPed::Jeton *
JetonDe(CScriptPed &c, int qui)
{
	for(int k = 0; k < 4; k++) for(int i = 0; i < c.nombreJetons[k]; i++) if(c.jetons[k][i].detenteur == qui) return &c.jetons[k][i];
	return c.jetonCone.detenteur == qui ? &c.jetonCone : nil;
}

// FUN_004868e0 : tout ce que le piéton tient, rendu (détenteur, précédent, filtre et instant à 0).
static void
RendreJetons(CScriptPed &c, int qui)
{
	for(int k = 0; k < 4; k++) for(int i = 0; i < c.nombreJetons[k]; i++){
		CScriptPed::Jeton &j = c.jetons[k][i];
		if(j.detenteur == qui){ j.detenteur = j.precedent = -1; j.filtre = 0; j.instant = 0; }
	}
	if(c.jetonCone.detenteur == qui){ c.jetonCone.detenteur = c.jetonCone.precedent = -1; c.jetonCone.filtre = 0; c.jetonCone.instant = 0; }
}

void
CScriptPedPool::DistribuerJetons(int h, uint32 maintenant, const ButesJetons &b, int (*tirer)(void *), void *ctx)
{
	CScriptPed *c = Acceder(h);
	if(c == nil) return;
	// 1. Le jeton que ce piéton tient sur sa propre cible : rendu si son dernier coup (+0x550), ou l'instant du jeton s'il est
	// plus tard, date de plus que l'échéance (AIButes +0x4c, +0x50 pour la catégorie 3 ; rien si elle n'est pas positive).
	if(CScriptPed *x = Acceder(c->verrou))
		if(CScriptPed::Jeton *j = JetonDe(*x, h)){
			uint32 depuis = c->dernierCoup > j->instant ? c->dernierCoup : j->instant;
			float e = j->categorie == 3 ? b.echeanceVelo : b.echeance;
			if(e > 0.0f && e * 1000.0f < (float)(int32)(maintenant - depuis)){ j->detenteur = j->precedent = -1; j->filtre = 0; j->instant = 0; }
		}
	// FUN_004865a0 : `q` a la cible `cible` dans son cône d'attaque (à moins d'AIButes +0x54, à moins de l'angle de +0x58 de
	// son cap ; le test de vue de la fin, FUN_0048d320, supposé vrai comme pour InAttackCone).
	auto dansCone = [&](const CScriptPed *q, const CScriptPed *cible){
		float dx = cible->pos.x - q->pos.x, dy = cible->pos.y - q->pos.y, n = sqrtf(dx * dx + dy * dy);
		if(n > b.porteeCone) return false;
		if(n <= 0.0f) return true;
		float a = q->cap * 3.14159265f / 180.0f;
		return (cosf(a) * dx + sinf(a) * dy) / n >= b.cosCone;
	};
	// 2. La place du cône : le premier proche qui vise ce piéton, l'a dans son cône, n'est pas armé (l'outil n'a pas d'armes)
	// et ne tient pas encore de jeton ; si un proche l'a dans son cône mais que la place reste vide, le plus proche de ceux
	// qui le visent rend ses jetons et la prend ; si aucun proche ne l'a dans son cône, elle est vidée.
	bool unDansCone = false;
	int plusProche = -1; float dMin = 0.0f;
	for(int q : c->proches){
		const CScriptPed *p = Acceder(q);
		if(p == nil) continue;
		if(dansCone(p, c)) unDansCone = true;
		if(p->verrou != h || !dansCone(p, c)) continue;
		float dx = p->pos.x - c->pos.x, dy = p->pos.y - c->pos.y, dz = p->pos.z - c->pos.z, d = sqrtf(dx * dx + dy * dy + dz * dz);   // FUN_0041a800
		if(plusProche < 0 || d < dMin){ plusProche = q; dMin = d; }
		if(!TientUnJeton(*c, q)){ c->jetonCone.detenteur = c->jetonCone.precedent = q; c->jetonCone.instant = maintenant; break; }
	}
	if(unDansCone){
		if(c->jetonCone.detenteur < 0 && plusProche >= 0){
			RendreJetons(*c, plusProche);
			c->jetonCone.detenteur = c->jetonCone.precedent = plusProche; c->jetonCone.instant = maintenant;
		}
	}else if(c->jetonCone.detenteur >= 0){ c->jetonCone.detenteur = c->jetonCone.precedent = -1; c->jetonCone.filtre = 0; c->jetonCone.instant = 0; }
	// 3 à 6. Pour chaque catégorie, la première entrée libre ; le départ dans la liste des proches est rand() / 0x7fff (une
	// division entière : 0, sauf quand rand() rend 0x7fff) ; un proche qui ne tient rien et vise ce piéton la reçoit s'il
	// remplit la condition de la catégorie ; le précédent détenteur passe son tour une fois (son +4 effacé) ; le filtre de
	// l'entrée revient à 0 après la recherche.
	auto dansZones = [&](int q, int n){ for(int z = 0; z < n; z++) for(int x : c->combattants[z]) if(x == q) return true; return false; };
	auto chercher = [&](CScriptPed::Jeton *e, int categorie){
		if(e == nil) return;
		int r = tirer(ctx) / 0x7fff;
		for(int k = r; k < (int)c->proches.size(); k++){
			int q = c->proches[k];
			const CScriptPed *p = Acceder(q);
			if(p == nil || TientUnJeton(*c, q) || p->verrou != h) continue;
			if(e->precedent == q){ e->precedent = -1; continue; }
			bool filtre = e->filtre == p->classeCombat || e->filtre == 0;
			bool donne = false;
			if(categorie == 2) donne = p->type == 0 || p->type == 8 || p->type == 7;   // les autorités (0x487106)
			else if(categorie == 3) donne = false;              // filtre et un véhicule (+0x1554) : l'outil n'en a pas
			else if(categorie == 0) donne = filtre && dansZones(q, 2);   // zone 0 ou 1, sans arme (0x4872d2)
			else donne = false;                                 // zones 0 à 2 et une arme à distance (0x487440)
			if(donne){ e->detenteur = e->precedent = q; e->instant = maintenant; break; }
		}
		e->filtre = 0;
	};
	auto libre = [&](int k) -> CScriptPed::Jeton * {
		for(int i = 0; i < c->nombreJetons[k]; i++) if(c->jetons[k][i].detenteur < 0) return &c->jetons[k][i];
		return nil;
	};
	chercher(libre(2), 2);
	chercher(libre(3), 3);
	CScriptPed::Jeton *e0 = libre(0);
	if(e0 == nil && c->jetonCone.detenteur < 0) e0 = &c->jetonCone;
	chercher(e0, 0);
	chercher(libre(1), 1);
}

bool
CScriptPedPool::TientJeton(int h) const
{
	const CScriptPed *p = Acceder(h);
	const CScriptPed *c = p ? Acceder(p->verrou) : nil;
	if(c == nil) return false;
	for(int k = 0; k < 4; k++) for(int i = 0; i < c->nombreJetons[k]; i++)
		if(c->jetons[k][i].detenteur == h && !c->jetons[k][i].coupe) return true;
	return c->jetonCone.detenteur == h && !c->jetonCone.coupe;
}

bool
CScriptPedPool::PrendreDroits(int h, uint32 maintenant)
{
	const CScriptPed *p = Acceder(h);
	CScriptPed *c = p ? Acceder(p->verrou) : nil;
	if(c == nil) return false;
	c->droitsPris = true;               // FUN_00486c40 : +0x344
	c->detenteurDroits = h;             // +0x340
	c->echeanceDroits = maintenant + 15000;   // +0x348 : DAT_00c1a9b4 + 15 000
	return true;
}

bool
CScriptPedPool::RendreDroits(int h)
{
	const CScriptPed *p = Acceder(h);
	CScriptPed *c = p ? Acceder(p->verrou) : nil;
	if(c == nil || c->detenteurDroits != h || !c->droitsPris) return false;   // FUN_004861c0
	c->droitsPris = false;
	c->detenteurDroits = -1;
	return true;
}

void
CScriptPedPool::LacherDroits(int h)
{
	RendreDroits(h);   // FUN_00486c00 : le même test (détenteur et octet), sur la cible courante
}

void
CScriptPedPool::AvancerDroits(uint32 maintenant)
{
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++){
		CScriptPed &c = peds[i];
		if(c.vivant && c.droitsPris && c.echeanceDroits <= maintenant){ c.droitsPris = false; c.detenteurDroits = -1; }
	}
}

bool
CScriptPedPool::DroitsPrisSurCible(int h) const
{
	const CScriptPed *p = Acceder(h);
	const CScriptPed *c = p ? Acceder(p->verrou) : nil;
	return c && c->droitsPris;
}

bool
CScriptPedPool::Blesser(int cible, float degats, int attaquant)
{
	CScriptPed *c = Acceder(cible);
	if(c == nil || c->mort) return false;
	if(c->drapeaux[DRAPEAU_INTOUCHABLE] || c->drapeaux[DRAPEAU_INVULNERABLE]) return false;
	if(attaquant == SCRIPT_PED_JOUEUR && c->drapeaux[DRAPEAU_INVULNERABLE_JOUEUR]) return false;
	float s = c->sante - degats;
	if(s < 0) s = 0;
	if(s < c->santeMin) s = c->santeMin;
	Sante(cible, s);
	if(c->sante <= 0 && !c->mort && c->critique && c->rappelCritique){ c->rappelCritique = false; critiquesAZero.push_back(cible); }
	return true;
}

bool
CScriptPedPool::CoupRecevable(int cible, int attaquant) const
{
	const CScriptPed *c = Acceder(cible);
	if(c == nil) return false;
	if(c->drapeaux[DRAPEAU_INTOUCHABLE]) return false;
	if(c->drapeaux[DRAPEAU_INVULNERABLE] && c->type != SCRIPT_PED_TYPE_JOUEUR) return false;
	const CScriptPed *a = Acceder(attaquant);
	if(a && a->type == SCRIPT_PED_TYPE_JOUEUR && c->drapeaux[DRAPEAU_INVULNERABLE_JOUEUR]) return false;
	if(a && Attitude(cible, a->type) == 4) return false;   // FUN_0047b810 : PedGetPedToTypeAttitude == 4, sans arme
	return true;
}

int
CScriptPedPool::CibleDeLObjectif(int h, int32 &style) const
{
	const CObjectifPieton *o = ObjectifCourant(h);
	style = 3;
	if(o == nil) return -1;
	if(o->type == OBJECTIF_ATTAQUER_PIETON || o->type == OBJECTIF_ATTAQUER_16){ style = o->style; return o->cible; }
	if(o->type == OBJECTIF_FUIR) return o->cible;
	return -1;
}

static bool
Autorite(const CScriptPed &p)   // FUN_0041a900 : préfet, policier, professeur
{
	return p.type == 0 || p.type == 7 || p.type == 8;
}

bool
CScriptPedPool::AgresseurPossible(int h, int a, const ButesCibles &b) const
{
	const CScriptPed *p = Acceder(h), *q = Acceder(a);
	if(p == nil || q == nil || a == h || q->mort) return false;
	if(Attitude(h, q->type) > 0){
		if(!Autorite(*p)) return false;
		if(0 < b.punitionMin) return false;    // +0x1d40 de l'agresseur : 0 dans l'outil
	}
	return true;
}

bool
CScriptPedPool::CibleValable(int h, int cible, int32 style, bool aPortee, bool vivante, const ButesCibles &b, int (*tirer)(void *), void *ctx)
{
	CScriptPed *p = Acceder(h);
	const CScriptPed *c = Acceder(cible);
	if(p == nil || c == nil || cible == h) return false;
	if(vivante && c->mort) return false;
	if(c->type == SCRIPT_PED_TYPE_JOUEUR && !c->controlable && Autorite(*p)) return false;   // 0x4971c5 … 0x4971dc
	if(style != 0 && style != 2) return true;              // FUN_00496960
	if(aPortee){
		float dx = c->pos.x - p->pos.x, dy = c->pos.y - p->pos.y, dz = c->pos.z - p->pos.z;
		if(dx * dx + dy * dy + dz * dz - b.portee * b.portee > 0.0f) return false;   // FUN_00487d00
	}
	if(style == 2 || style == 3 || !aPortee) return true;   // FUN_00496980
	if(Voit(h, cible, tirer, ctx)) return true;
	return p->verrou == cible && p->tempsNonVu <= b.nonVuMax;
}

void
CScriptPedPool::ChoisirCible(int h, uint32 maintenantMs, const ButesCibles &b, int (*tirer)(void *), void *ctx)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return;
	auto verrouiller = [&](int cible, int32 style){   // FUN_00497d20
		if(p->verrou != cible) p->tempsNonVu = 0;
		Verrouiller(h, cible, style);
		p->instantVerrou = maintenantMs;
	};
	int32 style = 3;
	int c = CibleDeLObjectif(h, style);               // FUN_004988b0
	if(c >= 0 && CibleValable(h, c, style, false, true, b, tirer, ctx)){ verrouiller(c, style); return; }
	if(Acceder(p->verrou) && ((p->verrouMode != 0 && p->verrouMode != 2) || maintenantMs < p->instantVerrou)) return;
	if(p->dernierAgresseur >= 0 && p->tempsAgresseur < 5.0f && AgresseurPossible(h, p->dernierAgresseur, b)   // FUN_00498930
	   && CibleValable(h, p->dernierAgresseur, 0, false, true, b, tirer, ctx))
		verrouiller(p->dernierAgresseur, 0);
}

void
CScriptPedPool::SuivreCibles(float dt, uint32 maintenantMs, const ButesCibles &b, int (*tirer)(void *), void *ctx)
{
	for(int i = 1; i < SCRIPT_PED_POOL_SIZE; i++){
		CScriptPed &p = peds[i];
		// Supposé : un mort ne choisit plus (son contrôleur de l'IA est arrêté, FUN_00484490) ; la garde de FUN_004812d0 (+0x1e25)
		// n'est pas lue.
		if(!p.vivant || p.mort || p.type == SCRIPT_PED_TYPE_JOUEUR) continue;
		if(Acceder(p.verrou)){
			p.tempsNonVu = Voit(i, p.verrou, tirer, ctx) ? 0.0f : p.tempsNonVu + dt;
			if(!CibleValable(i, p.verrou, p.verrouMode, true, true, b, tirer, ctx)) Verrouiller(i, -1, 0);
		}
		ChoisirCible(i, maintenantMs, b, tirer, ctx);
	}
}

int
CScriptPedPool::PremierMeneur(int h) const
{
	const CScriptPed *p = Acceder(h);
	int m = p ? p->meneur : -1;
	for(int garde = 0; m >= 0 && garde < SCRIPT_PED_POOL_SIZE; garde++){
		const CScriptPed *q = Acceder(m);
		if(q == nil || q->meneur < 0) return m;
		m = q->meneur;
	}
	return m;
}

bool
CScriptPedPool::AUnAllie(int h) const
{
	const CScriptPed *p = Acceder(h);
	return p != nil && (p->meneur >= 0 || p->suiveur >= 0);
}

bool
CScriptPedPool::EstDuModele(int h, int32 modele) const
{
	const CScriptPed *p = Acceder(h);
	return p != nil && (float)(int16)p->modele == (float)modele;
}

CScriptPedPool::Cible
CScriptPedPool::CibleDeGenre(int h, int genre) const
{
	Cible c;
	const CScriptPed *p = Acceder(h);
	if(p == nil) return c;
	switch(genre){
	case 0: c.pieton = p->verrou; break;
	case 1: c.pieton = p->cibleDePrise; break;
	case 4: c.objet = p->objetVise; c.point = p->syncVise; break;
	case 0x10: c.pieton = p->suiveur >= 0 ? p->suiveur : p->meneur; break;
	default: break;
	}
	return c;
}

bool
CScriptPedPool::DansSesSuiveurs(int meneur, int autre) const
{
	const CScriptPed *p = Acceder(meneur);
	if(p == nil || autre < 0) return false;
	int s = p->suiveur;
	for(int garde = 0; s >= 0 && garde < SCRIPT_PED_POOL_SIZE; garde++){
		if(s == autre) return true;
		const CScriptPed *q = Acceder(s);
		s = q ? q->suiveur : -1;
	}
	return false;
}

bool
CScriptPedPool::DansSonGroupe(int h, int autre) const
{
	const CScriptPed *p = Acceder(h);
	if(p == nil || autre < 0) return false;
	if(autre == h) return true;
	int m = p->meneur;
	for(int garde = 0; m >= 0 && garde < SCRIPT_PED_POOL_SIZE; garde++){
		if(m == autre) return true;
		const CScriptPed *q = Acceder(m);
		m = q ? q->meneur : -1;
	}
	return DansSesSuiveurs(h, autre);
}

bool
CScriptPedPool::RenvoyerAllie(int meneur, int allie)
{
	CScriptPed *m = Acceder(meneur), *a = Acceder(allie);
	if(m == nil || a == nil) return false;
	if(a->suiveur >= 0 && a->suiveur != allie) RenvoyerAllie(allie, a->suiveur);   // ecx = l'allié + 0x2f0
	if(m->suiveur != allie) return false;
	a->meneur = -1;
	// FUN_00485820 : FUN_004852e0 (le marqueur et la barre de santé, non suivis), FUN_004851f0 (le rappel Lua +0x141c, non
	// suivi), +0x318, +0x13c8, +0x14d0 à 1, +0x14ec et +0x153c à 0, la laisse lâchée (FUN_004998f0) ; puis +0x2f8 du meneur.
	a->etatAllie = 1; a->sprintSansFin = true; a->groupe14ec = 0;
	if(a->laisse != 0){ a->laisse = 0; a->laissePieton = -1; }
	m->suiveur = -1;
	return true;
}

bool
CScriptPedPool::RecruterAllie(int meneur, int allie, bool forcer)
{
	CScriptPed *m = Acceder(meneur), *a = Acceder(allie);
	if(m == nil || a == nil) return false;
	if(!forcer && (a->type == 0 || a->type == 7 || a->type == 8 || Attitude(allie, m->type) != 4)) return false;
	if(DansSesSuiveurs(meneur, allie)) return true;
	if(m->suiveur >= 0) RenvoyerAllie(meneur, m->suiveur);
	m->suiveur = allie;
	a->groupeTenu = false; a->groupe300 = 0; a->groupe304 = true; a->groupe305 = true;
	a->etatAllie = 1; a->moralAllie = 0; a->groupe308 = 0; a->groupe30c = 0;
	a->meneur = meneur;
	int racine = PremierMeneur(allie);
	if(const CScriptPed *r = Acceder(racine)){   // FUN_00499960
		a->laisse = 2; a->laissePieton = racine; a->laisseRayon = 4.0f; a->laissePoint = r->pos;
		a->laisseDecalage = CVector(0, 0, 0);
	}
	a->laisse3b4 = true; a->laisse3b5 = true;   // FUN_00499960 pose 1 et 0, FUN_00485fa0 les deux à 1
	a->attitudes.Ecrire(m->type, 4);   // FUN_00473080
	a->sprintSansFin = true;
	m->groupeTenu = forcer;
	return true;
}

int32
CScriptPedPool::Attitude(int h, int type) const
{
	const CScriptPed *p = Acceder(h);
	if(p == nil) return 2;
	int32 v = p->attitudes.Lire(type);
	return v == 5 ? attitudesTypes.TypeVersType(p->type, type) : v;
}

bool
CScriptPedPool::ViderObjectifs(int h)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return false;
	p->nombreObjectifs = 0;
	p->attaque = -1;
	p->dernierAgresseur = -1; p->tempsAgresseur = 0;
	return true;
}

void
CGestionnairesPietons::Ajouter(int ped)
{
	for(const Paire &p : paires) if(p.ped == ped) return;
	paires.push_back({ped, SANS_RAPPEL});
}

int *
CGestionnairesPietons::Case(int ped)
{
	for(Paire &p : paires) if(p.ped == ped) return &p.rappel;
	return nil;
}

int
CGestionnairesPietons::Retirer(int ped)
{
	if(enAppel) return SANS_RAPPEL;
	for(size_t i = 0; i < paires.size(); i++){
		if(paires[i].ped != ped) continue;
		int r = paires[i].rappel;
		paires.erase(paires.begin() + i);
		return r;
	}
	return SANS_RAPPEL;
}

bool
CGestionnairesPietons::Pret(int ped) const
{
	for(const Paire &p : paires) if(p.ped == ped) return p.rappel != SANS_RAPPEL;
	return false;
}

void
CScriptPedPool::ViderEntrees(int h)
{
	if(CScriptPed *p = Acceder(h)) p->intentionVitesse = 0;
}

bool
CScriptPedPool::IAAvance(int h) const
{
	const CScriptPed *p = Acceder(h);
	return p && !p->mort && !p->endormi && !sommeilGeneral;
}

void
CScriptPedPool::Sante(int h, float sante)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return;
	p->sante = sante < p->santeMin ? p->santeMin : sante;
}

bool
CScriptPedPool::Mourir(int h, uint32 maintenantMs)
{
	CScriptPed *p = Acceder(h);
	if(p == nil || p->mort || p->immortel) return false;
	p->sante = -100.0f;                       // _DAT_00906520
	p->mort = true;
	p->vientDeMourir = true;
	p->enMarche = false;
	p->attaque = -1;
	p->intentionVitesse = 0;
	if(h != SCRIPT_PED_JOUEUR){
		p->instantMort = maintenantMs;        // +0x18b8 = DAT_00c1a9b4
		p->verrou = -1; p->verrouMode = 0;    // FUN_004989c0(0)
	}
	return true;
}

bool
CScriptPedPool::PoserStats(int h, const int32 colonnes[64])
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return false;
	for(int i = 0; i < 64; i++) p->stats[i] = colonnes[i];
	p->statsLues = true;
	p->santeMax = (float)colonnes[4];
	p->santeMin = 0;
	p->regeneration = (float)colonnes[5];
	p->sante = p->santeMax;
	p->visionAngle = (float)colonnes[2] * 3.14159265f / 180.0f;   // +0xa68 = la colonne D (+0x24) × π / 180
	p->visionPortee = (float)colonnes[3];   // +0xa6c = la colonne E (+0x28)
	p->sprint = (float)colonnes[16];        // FUN_00478e30 : +0x18d0 = la colonne R (+0x5c)
	p->classeCombat = colonnes[17];         // +0x714 = la colonne S (+0x60)
	return true;
}

int32
CScriptPedPool::Stat(int h, int n) const
{
	const CScriptPed *p = Acceder(h);
	return p && n >= 0 && n < 64 ? p->stats[n] : 0;
}

void
CScriptPedPool::RemettreArme(int h)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return;
	p->armeInit = -1.0f;
	p->armePortee = -1.0f;
	if(p->armeTenue != -1) p->armeTenue = -1;   // FUN_004da790(0)
}

void
CScriptPedPool::PrendreArme(int h, int32 modele)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return;
	RemettreArme(h);
	p->armeCharge = 0.0f;
	p->armeMunition = modele;
	p->armeInit = -1.0f;
	p->armePortee = -1.0f;
	for(int32 &d : p->drapeauxArme) d = 0;
	p->armeTenue = modele;
}

int32
CScriptPedPool::StatLue(int h, int n, int32 sansEntree) const
{
	const CScriptPed *p = Acceder(h);
	if(p == nil || n < 0 || n >= 64) return 0;
	return p->statsLues ? p->stats[n] : sansEntree;
}

float
CScriptPedPool::PasDuControleur(int h, float pasTimer) const
{
	const CScriptPed *p = Acceder(h);
	if(p == nil) return 0.0f;
	return PasDuControleur(pasTimer, p->statsLues ? p->stats[0x14] : 100);
}

bool
CScriptPedPool::PoserStat(int h, int n, int32 v)
{
	CScriptPed *p = Acceder(h);
	if(p == nil || n < 0 || n >= 64) return false;
	p->stats[n] = v;
	if(n == 4){
		p->santeMax = (float)v;
		if(p->santeMax < p->sante) p->sante = p->santeMax;
	}else if(n == 5) p->regeneration = (float)v;
	return true;
}

bool
CScriptPedPool::RendreMission(int h, int32 script)
{
	CScriptPed *p = Acceder(h);
	if(p == nil || p->script != -1) return false;
	p->script = script;
	return true;
}

std::vector<int>
CScriptPedPool::AmbiantsAVider(void) const
{
	std::vector<int> v;
	for(int h = SCRIPT_PED_POOL_SIZE - 1; h >= 0; h--){
		const CScriptPed *p = Acceder(h);
		if(p == nil || p->script != -1 || h == SCRIPT_PED_JOUEUR || p->type == SCRIPT_PED_TYPE_JOUEUR) continue;
		v.push_back(h);
	}
	return v;
}

bool
CScriptPedPool::ArreterSocial(int h)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return false;
	p->partenaireSocial = -1;
	p->drapeauxSociaux &= 0x60;   // & 0xe0 puis & 0x7f
	p->etatSocial = 0;
	p->drapeauxSociaux &= 0xbf;   // le bit 6 : le +0x1e28 du piéton (non suivi) vaut 0, sous 1
	return true;
}

bool
CScriptPedPool::RendreAmbiant(int h)
{
	CScriptPed *p = Acceder(h);
	if(p == nil || h == SCRIPT_PED_JOUEUR) return false;
	p->script = -1;
	return true;
}

// Un pas vers `vers`, sans dépasser ; vrai si le point est atteint.
static bool
Pas(CScriptPed &p, const CVector &vers, float distanceMax, float garde)
{
	float dx = vers.x - p.pos.x, dy = vers.y - p.pos.y, dz = vers.z - p.pos.z;
	float d = sqrtf(dx * dx + dy * dy + dz * dz);
	if(d <= garde) return true;
	if(d - garde <= distanceMax){
		float k = (d - garde) / d;
		p.pos.x += dx * k; p.pos.y += dy * k; p.pos.z += dz * k;
		return true;
	}
	p.pos.x += dx / d * distanceMax; p.pos.y += dy / d * distanceMax; p.pos.z += dz / d * distanceMax;
	p.cap = atan2f(dy, dx) * 180.0f / 3.14159265f;
	return false;
}

void
CScriptPedPool::Avancer(float dt)
{
	// Les drapeaux de l'image précédente tombent d'abord pour tous : un coup porté par une entrée
	// à une entrée plus haute dans le pool doit rester visible jusqu'à la fin de l'image.
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++) peds[i].arrive = peds[i].aFrappe = peds[i].vientDeMourir = false;
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++){
		CScriptPed &p = peds[i];
		if(!p.vivant || p.mort) continue;
		if(p.sante > 0 && p.sante < p.santeMax){   // FUN_00472ff0
			p.sante += p.regeneration / 100.0f * dt;
			if(p.sante > p.santeMax) p.sante = p.santeMax;
		}
		if(p.attaque >= 0 && !p.arbreIA){
			CScriptPed *c = Acceder(p.attaque);
			if(c == nil || c->mort){ p.attaque = -1; continue; }
			// La poursuite se fait à la course ; à portée, un coup toutes les SCRIPT_PED_CADENCE s.
			if(!Pas(p, c->pos, ScriptPedVitesse(ALLURE_COURSE) * dt, SCRIPT_PED_PORTEE)) continue;
			p.prochainCoup -= dt;
			if(p.prochainCoup > 0) continue;
			p.prochainCoup = SCRIPT_PED_CADENCE;
			p.aFrappe = true;
			p.dernierFrappe = p.attaque;
			Blesser(p.attaque, SCRIPT_PED_DEGATS, (int)(&p - peds));
			// Le geste de l'outil n'a pas d'arbre d'actions : la cible à santé nulle meurt sur le coup, à la place de
			// HitTree/…/Dead, qui exécute Base/DeadCleanup et sa piste Death (supposé pour ce geste seulement).
			if(c->sante <= 0) Mourir(p.attaque);
			if(c->mort) p.attaque = -1;
		}else if(p.enMarche){
			if(Pas(p, p.cible, p.vitesse * dt, 0)){ p.enMarche = false; p.arrive = true; p.intentionVitesse = 0; }
		}
	}
}

// 0x9113d0 : les noms des groupes, 32 octets chacun.
static const char *const kGroupesHumiliation[SCRIPT_PED_GROUPES_HUMILIATION] = {
	"HUMIL_5-8_A", "HUMIL_5-8_B", "HUMIL_5-8_C", "HUMIL_5-8F_A", "HUMIL_5-8F_B", "HUMIL_6-1_A", "HUMIL_6-1_B",
	"HUMIL_6-1_C", "HUMIL_4-10_B", "HUMIL_4-10_C", "HUMIL_6-5_A", "HUMIL_6-5_B", "HUMIL_6-5_C", "HUMIL_6-5V4-10",
	"HUMIL_6-5VPLY", "HUMIL_6-5V6-1", "HUMIL_5-8V4-10", "HUMIL_5-8VPLY", "HUMIL_5-8V6-1", "HUMIL_6-1V4-10",
	"HUMIL_6-1VPLY", "HUMIL_6-1V6-1",
};

const char *
ScriptPedGroupeHumiliation(int groupe)
{
	return groupe >= 0 && groupe < SCRIPT_PED_GROUPES_HUMILIATION ? kGroupesHumiliation[groupe] : nil;
}

int
CScriptPedPool::GroupeHumiliationDans(int de, int a) const
{
	for(int i = SCRIPT_PED_POOL_SIZE - 1; i >= 0; i--){
		if(!peds[i].vivant) continue;
		int32 g = peds[i].groupeHumiliation;
		if(de <= g && g <= a) return g;
	}
	return -1;
}

int
CScriptPedPool::LibererGroupeHumiliation(int h)
{
	CScriptPed *p = Acceder(h);
	if(p == nil || p->groupeHumiliation == -1) return -1;
	int ancien = p->groupeHumiliation;
	p->groupeHumiliation = -1;
	return ancien;
}

int
CScriptPedPool::PoserGroupeHumiliation(int h, int groupe)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return -1;
	int libere = -1;
	if(p->groupeHumiliation != -1 && p->groupeHumiliation != groupe) libere = LibererGroupeHumiliation(h);
	p->groupeHumiliation = groupe;
	return libere;
}

// Les seuils du tirage : 0,5 (0x9055a0) pour deux groupes, 0,33 (0x9118b0) et 0,66 (0x9118ac) pour trois.
static int
TirerGroupe(int premier, int nombre, float tirage)
{
	if(nombre == 2) return tirage < 0.5f ? premier : premier + 1;
	if(tirage < 0.33f) return premier;
	if(tirage >= 0.66f) return premier + 2;
	return premier + 1;
}

int
CScriptPedPool::ChoisirGroupeHumiliation(int victime, int taille, int humiliateur, int tailleHumiliateur, bool pourLeJoueur, float tirage)
{
	if(Acceder(victime) == nil || Acceder(humiliateur) == nil) return -1;
	int g = -1;
	if(humiliateur == SCRIPT_PED_JOUEUR){
		if(pourLeJoueur){
			// Par taille : le premier groupe de la tranche et leur nombre.
			static const int premier[5] = { 8, 0, 5, 3, 10 }, nombre[5] = { 2, 3, 3, 2, 3 };
			if(taille < 0 || taille > 4) return -1;
			g = GroupeHumiliationDans(premier[taille], premier[taille] + nombre[taille] - 1);
			if(g == -1) g = TirerGroupe(premier[taille], nombre[taille], tirage);
		}else
			g = taille == 1 ? 17 : taille == 2 ? 20 : taille == 4 ? 14 : -1;
	}else if(!pourLeJoueur){
		// La victime (Medium, Large, Huge) contre la taille de l'autre : Small, Medium ou Fat, Large.
		int base = taille == 1 ? 16 : taille == 2 ? 19 : taille == 4 ? 13 : -1;
		if(base < 0) return -1;
		switch(tailleHumiliateur){
		case 0: g = base; break;
		case 1: case 3: g = base + 1; break;
		case 2: g = base + 2; break;
		default: return -1;
		}
	}
	if(g == -1) return -1;
	PoserGroupeHumiliation(victime, g);
	return g;
}

int
CScriptPedPool::SurveillerHumiliation(int victime, int taille, int tailleJoueur, float tirage)
{
	const CScriptPed *p = Acceder(victime);
	if(p == nil || (p->groupeHumiliation != -1 && p->groupeHumiliation <= 12)) return -1;
	if(!(p->santeMax > 0 && (double)(p->sante / p->santeMax) < 0.5)) return -1;
	return ChoisirGroupeHumiliation(victime, taille, SCRIPT_PED_JOUEUR, tailleJoueur, true, tirage);
}

bool
CScriptPedPool::PoserManette(int handle, int manette)
{
	if(!Valide(handle) || (manette != 0 && manette != 1)) return false;
	manettes[manette] = handle;
	return true;
}

bool
CScriptPedPool::MeneParLaManette(int handle, bool manette1, bool lesDeux) const
{
	if(!Valide(handle)) return false;
	if(lesDeux) return manettes[0] == handle || manettes[1] == handle;
	return manettes[manette1 ? 1 : 0] == handle;
}

bool
CScriptPedPool::AttenteSociale(int h) const
{
	if(!Valide(h)) return false;
	const CScriptPed &p = peds[h];
	return p.etatSocial != 0 && p.partenaireSocial >= 0 && (p.etatSocial == 2 || p.etatSocial == 4 || p.etatSocial == 6);
}

bool
CScriptPedPool::Socialise(int h, bool pourLeJoueur) const
{
	if(!Valide(h)) return false;
	const CScriptPed &p = peds[h];
	if(!pourLeJoueur || p.type != SCRIPT_PED_TYPE_JOUEUR) return p.etatSocial != 0 && p.partenaireSocial >= 0;
	for(int i = SCRIPT_PED_POOL_SIZE - 1; i >= 0; i--){
		const CScriptPed &o = peds[i];
		if(i == h || !o.vivant) continue;
		if(o.etatSocial != 0 && o.partenaireSocial == h && (o.drapeauxSociaux & 1) == 0) return true;
	}
	return false;
}

float
ScriptPedVitesseDuPas(int mode, float d2Cible, float d2Point, float rayonButes2c)
{
	if(mode == PAS_SUR_PLACE || mode == PAS_ATTENDRE) return 0.0f;
	if(mode == PAS_RALENTI){   // FUN_0061b820
		float t = (d2Cible - rayonButes2c) * 0.04f;
		if(!(t >= 0.0f)) t = 0.0f; else if(t > 1.0f) t = 1.0f;
		return (1.0f - powf(1.0f - t, 8.0f)) + 0.5f;
	}
	if(mode == PAS_APPROCHE_7 || mode == PAS_APPROCHE_9){   // FUN_0061b8e0
		float t = d2Point * 0.015625f;
		if(!(t >= 0.0f)) t = 0.0f; else if(t > 1.0f) t = 1.0f;
		return t * 1.3f + 0.2f;
	}
	return 1.5f;
}

float
ScriptPedVitesseDuTrajet(float vitesse, float d2Point, float ecartCap, float rayonArrivee, bool immobile)
{
	if(immobile || !((double)vitesse >= 0.1)) return 0.0f;         // FUN_005e09c0, FUN_005e4a60
	if(d2Point <= rayonArrivee * rayonArrivee) return 0.0f;         // FUN_005e2fb0 : arrivé
	if(!(d2Point < 9.0f)) return vitesse;
	float ecart = fabsf(ecartCap);
	if(!((double)ecart <= 3.14159265358979)) return 0.0f;
	float v = cosf(ecart) * vitesse;
	if(d2Point < 0.09f) v = (float)(sqrtf(d2Point) / 0.3) * v;
	return v > 0.1f ? v : 0.1f;                                      // FUN_00414340
}

static float
Plat2(const CVector &a, const CVector &b)
{
	float dx = a.x - b.x, dy = a.y - b.y;
	return dx * dx + dy * dy;
}

// FUN_0061b430 : le point visé, son décalage depuis la cible et le mode.
static void
PoserPoint(CCombatLoco &c, const CVector &point, const CVector &cible, int32 mode)
{
	c.point = point;
	c.decalage = CVector(point.x - cible.x, point.y - cible.y, point.z - cible.z);
	c.mode = mode;
}

void
ScriptPedCombatLocoChoisir(CCombatLoco &c, const CVector &soi, const CVector &cible, float petit, float grand,
                           int32 pasMs, bool cibleVue, int (*tirer)(void *), void *ctx)
{
	const float kPi = 3.14159265f, kQuart = kPi / 4.0f, kHuitieme = kPi / 8.0f;   // DAT_00b0c0a0, DAT_00b0c0a4
	auto retour = [&](void){   // FUN_0061b6a0
		c.sousEtat = 3; c.tirage = true; c.forcer = true;
		c.angle = (float)tirer(ctx) / 32767.0f * (kQuart + kQuart) - kQuart;
	};
	// FUN_0062c540
	if(c.sousEtat != 3){
		float r = grand + 2.0f;
		if(!(Plat2(cible, soi) - r * r < 0.0f)){ retour(); return; }
		c.minuterieVue = cibleVue ? 0 : c.minuterieVue + pasMs;
		if(c.minuterieVue > 1000){ c.minuterieVue = 0; retour(); return; }
	}
	switch(c.sousEtat){
	case 0:   // FUN_0062c740
		if(c.tirage){
			float d = (float)tirer(ctx) / 32767.0f * (grand - petit) + petit;
			float cap = atan2f(soi.y - cible.y, soi.x - cible.x);
			float a = (float)tirer(ctx) / 32767.0f * (kQuart + kQuart) - kQuart;
			cap = a < 0.0f ? cap - (kHuitieme - a) : a + kHuitieme + cap;
			PoserPoint(c, CVector(cible.x + cosf(cap) * d, cible.y + sinf(cap) * d, cible.z), cible, 8);   // FUN_0062c4f0
			c.minuterieAller = 0; c.sousEtat = 1; c.forcer = true;
			return;
		}
		if(Plat2(cible, soi) - petit * petit < 0.0f) PoserPoint(c, soi, cible, 10);
		else PoserPoint(c, cible, cible, 11);
		return;
	case 1:   // FUN_0061b700
		c.minuterieAller += pasMs;
		if(Plat2(soi, c.point) - 0.09f < 0.0f){ c.minuterieAttente = 0; c.sousEtat = 2; c.forcer = true; }
		if(c.minuterieAller >= 10000){ c.forcer = true; c.sousEtat = 0; }
		return;
	case 2:   // FUN_00625740
		c.minuterieAttente += pasMs;
		if(c.minuterieAttente > 300){
			if(tirer(ctx) & 1){ c.sousEtat = 0; c.forcer = true; return; }
			c.minuterieAttente = 0;
		}
		return;
	case 3:   // 0x6257c0
		if(Plat2(cible, soi) - grand * grand < 0.0f && cibleVue){ c.sousEtat = 0; c.forcer = true; return; }
		PoserPoint(c, cible, cible, 12);
		return;
	}
}

bool
CScriptPedPool::PrendreAttaquant(int h, int attaquant)
{
	if(!Valide(h) || !Valide(attaquant) || peds[h].type == SCRIPT_PED_TYPE_JOUEUR) return false;
	CScriptPed &p = peds[h];
	if(attaquant != h && p.verrou != attaquant){
		QuitterZones(peds, h);
		p.zoneCombat = -1;
		p.verrou = attaquant;
	}
	p.cibleObjectif = attaquant;
	p.agresseur = attaquant;
	return true;
}

bool
CScriptPedPool::Voit(int h, int cible, int (*tirer)(void *), void *ctx)
{
	CScriptPed *p = Acceder(h);
	const CScriptPed *c = Acceder(cible);
	if(p == nil || c == nil) return false;
	if(h == cible) return true;
	float dx = c->pos.x - p->pos.x, dy = c->pos.y - p->pos.y, dz = c->pos.z - p->pos.z;
	if(p->dernierAgresseur == cible && p->tempsAgresseur < 5.0f && dx * dx + dy * dy + dz * dz - 25.0f < 0.0f)
		return true;
	for(const CScriptPed::Vu &v : p->vus)
		if(v.ped == cible) return v.vu;
	// FUN_0048cf50 : à portée, à plat (z mis à 0) ; puis dans le cône du devant.
	bool vu = VoitPoint(h, c->pos);
	// FUN_0048d0e0 : la ligne de vue (FUN_00461d50) n'a rien à traverser dans l'outil.
	for(CScriptPed::Vu &v : p->vus)
		if(v.ped < 0){ v.ped = cible; v.vu = vu; v.images = tirer(ctx) * 5 / 0x7fff + 4; break; }
	return vu;
}

bool
CScriptPedPool::VoitPoint(int h, const CVector &point) const
{
	const CScriptPed *p = Acceder(h);
	if(p == nil) return false;
	float dx = point.x - p->pos.x, dy = point.y - p->pos.y;
	if(dx * dx + dy * dy - p->visionPortee * p->visionPortee > 0.0f) return false;
	float n = sqrtf(dx * dx + dy * dy), a = p->cap * 3.14159265f / 180.0f;
	float produit = n > 0.0f ? (cosf(a) * dx + sinf(a) * dy) / n : 1.0f;
	return produit > cosf(p->visionAngle * 0.5f);
}

bool
CScriptPedPool::DetecterProjectile(int h, const std::vector<ProjectileVu> &vus) const
{
	const CScriptPed *p = Acceder(h);
	if(p == nil || p->drapeaux[19] != 0) return false;
	float meilleur = 1000.0f;
	bool trouve = false;
	for(const ProjectileVu &q : vus){
		if(q.tireur == h || (q.fiche && !q.menace)) continue;
		const CVector &v = q.vitesse;
		if(!(0.0f <= v.z * v.z + v.x * v.x + v.y * v.y - (float)0.00039999998211860677)) continue;
		if(!VoitPoint(h, q.pos)) continue;
		float ex = q.pos.x - p->pos.x, ey = q.pos.y - p->pos.y, ez = q.pos.z - p->pos.z;
		float d2 = ez * ez + ex * ex + ey * ey;   // FUN_00472bb0
		if(!(d2 <= meilleur)) continue;
		float dx = p->pos.x - q.pos.x, dy = p->pos.y - q.pos.y, dz = p->pos.z - q.pos.z;
		if(!(dz * v.z + dx * v.x + dy * v.y > 0.0f)) continue;   // FUN_004144f0 : il vient vers le piéton
		float n = sqrtf(v.x * v.x + v.y * v.y);                   // FUN_0041a3d0 : la vitesse à plat, normée (FUN_00414cc0)
		if(n <= 0.0f) continue;
		float ux = v.x / n, uy = v.y / n;
		float t = dz * 0.0f + dx * ux + dy * uy;
		float px = q.pos.x + ux * t, py = q.pos.y + uy * t;      // FUN_00412200, FUN_00412850
		float fx = p->pos.x - px, fy = p->pos.y - py;
		if(fy * fy + fx * fx + 0.0f - 3.0f * 3.0f <= 0.0f){ trouve = true; meilleur = d2; }   // FUN_0041a380(…, 3)
	}
	return trouve;
}

bool
CScriptPedPool::DetecterGrenade(int h, const std::vector<ProjectileVu> &vus, float meilleur, CVector *pos, float *rayon) const
{
	const CScriptPed *p = Acceder(h);
	if(p == nil || p->drapeaux[19] != 0) return false;
	bool trouve = false;
	for(const ProjectileVu &q : vus){
		if(!q.fiche) continue;
		float ex = q.pos.x - p->pos.x, ey = q.pos.y - p->pos.y, ez = q.pos.z - p->pos.z;
		float d2 = ez * ez + ex * ex + ey * ey;
		if(q.tireur == h && 0.0f < q.rayon){
			float r = q.rayon + (float)0.5;
			if(r * r < d2) return true;   // sans rien rendre
		}
		if(!q.menace || !(0.0f < q.rayon)) continue;
		if(0.0f < q.a0 && (q.o8c || q.o8d || 0 < q.vie) && d2 <= meilleur){
			trouve = true; meilleur = d2;
			if(pos) *pos = q.pos;
			if(rayon) *rayon = q.rayon;
		}
	}
	return trouve;
}

bool
CScriptPedPool::GrenadeAPortee(int h, const std::vector<ProjectileVu> &vus) const
{
	const CScriptPed *p = Acceder(h);
	if(p == nil) return false;
	CVector g(0.0f, 0.0f, 0.0f);
	float r = 0.0f;
	if(!DetecterGrenade(h, vus, 100.0f, &g, &r)) return false;   // 0x900490
	float d = r + (float)1.0;                                     // le double 0x8ff1d8
	float dx = g.x - p->pos.x, dy = g.y - p->pos.y;
	return dy * dy + dx * dx + 0.0f - d * d <= 0.0f;              // FUN_0041a380
}

void
CScriptPedPool::AvancerVue(float dt)
{
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++){
		CScriptPed &p = peds[i];
		for(CScriptPed::Vu &v : p.vus)   // FUN_0048ccf0
			if(v.ped >= 0 && --v.images < 1){ v.ped = -1; v.vu = false; v.images = 0; }
		p.tempsAgresseur = p.dernierAgresseur >= 0 ? p.tempsAgresseur + dt : 0.0f;   // FUN_00499340
	}
}

void
CScriptPedPool::Agresse(int h, int attaquant)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return;
	p->dernierAgresseur = attaquant;
	p->tempsAgresseur = 0;
}

bool
ScriptPedSuiviSuivant(CSuiviTrajet &s, int nombre, int32 &noeud)
{
	if(nombre == 0) return false;
	switch(s.mode){
	case 0:
		noeud++;
		return noeud < nombre;
	case 1:
		noeud++;
		if(noeud >= nombre) noeud = 0;
		return true;
	case 2:
		noeud += s.sens;
		if(noeud >= nombre){ s.sens = -1; noeud = nombre - 2; return true; }
		if(noeud < 0){ noeud = 1; s.sens = 1; }
		return true;
	case 3:
		return true;
	default:
		return false;
	}
}

bool
ScriptPedSuiviArret(const CSuiviTrajet &s, const std::vector<CNoeudTrajet> &noeuds, int32 noeud)
{
	int nombre = (int)noeuds.size();
	if(noeud < 0 || noeud >= nombre) return true;
	const CNoeudTrajet &n = noeuds[noeud];
	if(n.actions || n.attente > 0.0f) return true;
	if(s.mode == 2) return noeud == nombre - 1 || noeud == 0;
	if(s.mode == 0 || s.mode == 3) return noeud == nombre - 1;
	return false;
}

static CVector
Unitaire(const CVector &v)
{
	float n = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	return n > 0.0f ? CVector(v.x / n, v.y / n, v.z / n) : CVector(0, 0, 0);
}

void
ScriptPedSuiviPoserPlan(CSuiviTrajet &s, const std::vector<CNoeudTrajet> &noeuds, int32 precedent, int32 noeud)
{
	int32 suivant = noeud;
	if(ScriptPedSuiviArret(s, noeuds, noeud) || !ScriptPedSuiviSuivant(s, (int)noeuds.size(), suivant) ||
	   suivant < 0 || suivant >= (int)noeuds.size()){
		s.plan = -1;
		return;
	}
	const CVector &p = noeuds[noeud].pos, &q = noeuds[suivant].pos;
	CVector vers = Unitaire(CVector(q.x - p.x, q.y - p.y, q.z - p.z));
	s.planPoint = p;
	s.plan = noeud;
	s.planNormale = vers;
	if(precedent < 0 || precedent >= (int)noeuds.size()) return;
	const CVector &r = noeuds[precedent].pos;
	CVector depuis = Unitaire(CVector(r.x - p.x, r.y - p.y, r.z - p.z));
	CVector somme(depuis.x + vers.x, depuis.y + vers.y, depuis.z + vers.z);
	if(somme.x * somme.x + somme.y * somme.y + somme.z * somme.z < 0.05f) return;   // _DAT_00900c18 (0,05) : presque droit
	CVector n = Unitaire(CVector(-somme.y, somme.x, 0.0f));
	if(n.x * vers.x + n.y * vers.y + n.z * vers.z < 0.0f) n = CVector(-n.x, -n.y, -n.z);
	s.planNormale = n;
}

bool
ScriptPedSuiviPlanPasse(const CSuiviTrajet &s, const CVector &pos)
{
	if(s.plan < 0) return false;
	CVector v(s.planPoint.x - pos.x, s.planPoint.y - pos.y, s.planPoint.z - pos.z);
	return s.planNormale.x * v.x + s.planNormale.y * v.y + s.planNormale.z * v.z <= 0.0f;
}

bool
ScriptPedSuiviAtteint(const CSuiviTrajet &s, int nombre, const CVector &pos, const CVector &noeud, float rayon, bool velo)
{
	float dx = noeud.x - pos.x, dy = noeud.y - pos.y, d = sqrtf(fabsf(dx * dx + dy * dy));
	float facteur = velo && !(s.noeud >= nombre - 1 && s.mode == 0) ? 3.0f : 1.0f;
	return d < rayon * facteur;
}

uint32
ScriptPedSuiviEcheance(float attente, uint32 maintenantMs)
{
	if(!(attente > 0.0f)) return 0;
	return maintenantMs - (uint32)(int32)(attente * -1000.0);   // fistp, arrondi par troncature (le mot de contrôle | 0xc00)
}

bool
ScriptPedSuiviAction(const CNoeudTrajet &n, int32 &index, uint32 maintenantMs, uint32 &echeance, CActionNoeud &action)
{
	if(index < 0 || index >= (int32)n.listeActions.size()){ index = 0; echeance = 0; return false; }
	action = n.listeActions[index++];
	echeance = maintenantMs - (uint32)(int32)(action.attente * -1000.0);
	return true;
}

float
ScriptPedSuiviCap(float lacetNoeud, bool velo)
{
	return velo ? lacetNoeud + 180.0f : lacetNoeud - 90.0f;   // _DAT_00900158, _DAT_00900ab8
}

CVector
ScriptPedSuiviBut(const CVector &a, const CVector &b, const CVector &pos, float avance, float &t)
{
	float sx = b.x - a.x, sy = b.y - a.y, sz = b.z - a.z, l2 = sx * sx + sy * sy + sz * sz;
	if(l2 <= 0.0f){ t = 2.0f; return b; }
	float l = sqrtf(l2);
	float u = ((pos.x - a.x) * sx + (pos.y - a.y) * sy + (pos.z - a.z) * sz) / l2;   // FUN_005e7340
	CVector proj(a.x + sx * u, a.y + sy * u, a.z + sz * u);
	// L'écart à plat, perpendiculaire au segment.
	float nx = -sy, ny = sx, n = sqrtf(nx * nx + ny * ny);
	float ecart = n > 0.0f ? fabsf((a.x - pos.x) * nx / n + (a.y - pos.y) * ny / n) : 0.0f;
	t = u;
	if(u <= 1.0f && ecart > avance) return proj;
	t = u + avance / l;
	if(t > 1.0f) return b;
	if(t < 0.0f) return a;
	return CVector(a.x + sx * t, a.y + sy * t, a.z + sz * t);
}

void
CGestionnairesDeclencheurs::Dimensionner(int table, int n, int script)
{
	if(table < 0 || table > 1) return;
	Table &t = tables[table];
	if(!t.entrees.empty()){
		if(script == t.proprietaire){ t.entrees.clear(); t.enPlus = 0; }   // FUN_005b5a60
		if(!t.entrees.empty()) return;
	}
	if(n > EN_PLACE && n - EN_PLACE > t.enPlus) t.enPlus = n - EN_PLACE;  // FUN_005b55b0 : le tas ne rapetisse pas
	t.proprietaire = script;
}

bool
CGestionnairesDeclencheurs::Ajouter(int table, int declencheur)
{
	if(table < 0 || table > 1) return false;
	Table &t = tables[table];
	for(const Entree &e : t.entrees) if(e.declencheur == declencheur) return true;
	if((int)t.entrees.size() == t.enPlus + EN_PLACE) return false;
	Entree e;
	e.declencheur = declencheur;
	t.entrees.push_back(e);
	return true;
}

int *
CGestionnairesDeclencheurs::Case(int table, int declencheur, int evenement)
{
	if(table < 0 || table > 1 || evenement < 0 || evenement >= EVENEMENTS) return nil;
	for(Entree &e : tables[table].entrees) if(e.declencheur == declencheur) return &e.rappels[evenement];
	return nil;
}

bool
CGestionnairesDeclencheurs::Pret(int table, int declencheur, int evenement) const
{
	if(table < 0 || table > 1 || evenement < 0 || evenement >= EVENEMENTS) return false;
	for(const Entree &e : tables[table].entrees) if(e.declencheur == declencheur) return e.rappels[evenement] != SANS_RAPPEL;
	return false;
}

int
CGestionnairesDeclencheurs::Retirer(int table, int declencheur, int evenement)
{
	if(enAppel || table < 0 || table > 1 || evenement < 0 || evenement >= EVENEMENTS) return SANS_RAPPEL;
	std::vector<Entree> &v = tables[table].entrees;
	for(size_t i = 0; i < v.size(); i++){
		if(v[i].declencheur != declencheur) continue;
		int r = v[i].rappels[evenement];
		v[i].rappels[evenement] = SANS_RAPPEL;
		v[i].scripts[evenement] = -1;
		bool vide = true;
		for(int k = 0; k < EVENEMENTS; k++) if(v[i].rappels[k] != SANS_RAPPEL) vide = false;
		if(vide) v.erase(v.begin() + i);
		return r;
	}
	return SANS_RAPPEL;
}

bool
CGestionnairesDeclencheurs::Suivie(int table, int i) const
{
	if(table < 0 || table > 1 || i < 0 || i >= (int)tables[table].entrees.size()) return false;
	const Entree &e = tables[table].entrees[i];
	return e.rappels[ENTREE_PIETON] != SANS_RAPPEL || e.rappels[ENTREE_JOUEUR] != SANS_RAPPEL ||
	       e.rappels[SORTIE_PIETON] != SANS_RAPPEL || e.rappels[SORTIE_JOUEUR] != SANS_RAPPEL || e.rappels[DEDANS] != SANS_RAPPEL;
}

std::vector<int>
CGestionnairesDeclencheurs::OublierScript(int script)
{
	std::vector<int> laches;
	for(Table &t : tables){
		if(script == t.proprietaire){
			for(const Entree &e : t.entrees) for(int r : e.rappels) if(r != SANS_RAPPEL) laches.push_back(r);
			t.entrees.clear(); t.enPlus = 0; t.proprietaire = -1;
			continue;
		}
		for(size_t i = 0; i < t.entrees.size(); ){
			Entree &e = t.entrees[i];
			bool vide = true;
			for(int k = 0; k < EVENEMENTS; k++){
				if(e.rappels[k] != SANS_RAPPEL && e.scripts[k] == script){ laches.push_back(e.rappels[k]); e.rappels[k] = SANS_RAPPEL; e.scripts[k] = -1; }
				if(e.rappels[k] != SANS_RAPPEL) vide = false;
			}
			if(vide) t.entrees.erase(t.entrees.begin() + i); else i++;
		}
	}
	return laches;
}
