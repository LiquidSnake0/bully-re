#include "ScriptPedPool.h"
#include <cmath>

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

bool
CScriptPedPool::Supprimer(int h)
{
	if(h == SCRIPT_PED_JOUEUR || !Valide(h)) return false;
	peds[h] = CScriptPed();
	QuitterZones(peds, h);
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
		if(peds[i].vivant && peds[i].script == script){ peds[i] = CScriptPed(); QuitterZones(peds, i); n++; }
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
	if(cible == -1){ p->verrou = -1; p->verrouMode = 0; return true; }
	if(Acceder(cible) == nil) return false;
	p->verrou = cible;
	p->verrouMode = mode;
	return true;
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
	return true;
}

void
CScriptPedPool::Sante(int h, float sante)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return;
	p->sante = sante;
	if(sante <= 0 && !p->mort){ p->mort = true; p->vientDeMourir = true; p->enMarche = false; p->attaque = -1; }
}

bool
CScriptPedPool::PoserStats(int h, const int32 colonnes[64])
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return false;
	for(int i = 0; i < 64; i++) p->stats[i] = colonnes[i];
	p->santeMax = (float)colonnes[4];
	p->santeMin = 0;
	p->regeneration = (float)colonnes[5];
	p->sante = p->santeMax;
	p->sprint = (float)colonnes[16];        // FUN_00478e30 : +0x18d0 = la colonne R (+0x5c)
	return true;
}

int32
CScriptPedPool::Stat(int h, int n) const
{
	const CScriptPed *p = Acceder(h);
	return p && n >= 0 && n < 64 ? p->stats[n] : 0;
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
