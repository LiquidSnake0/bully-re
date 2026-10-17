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

CScriptPedPool::CScriptPedPool(void)
{
	// Le joueur existe toujours, à l'entrée 0.
	peds[SCRIPT_PED_JOUEUR].vivant = true;
	peds[SCRIPT_PED_JOUEUR].modele = 0;
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

bool
CScriptPedPool::Supprimer(int h)
{
	if(h == SCRIPT_PED_JOUEUR || !Valide(h)) return false;
	peds[h] = CScriptPed();
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
		if(peds[i].vivant && peds[i].script == script){ peds[i] = CScriptPed(); n++; }
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
	return true;
}

bool
CScriptPedPool::Arreter(int h)
{
	CScriptPed *p = Acceder(h);
	if(p == nil) return false;
	p->enMarche = false;
	p->attaque = -1;
	return true;
}

bool
CScriptPedPool::Attaquer(int h, int cible)
{
	CScriptPed *p = Acceder(h);
	const CScriptPed *c = Acceder(cible);
	if(p == nil || c == nil || p->mort || h == cible) return false;
	p->attaque = cible;
	p->enMarche = false;
	p->prochainCoup = 0;
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
		if(p.attaque >= 0){
			CScriptPed *c = Acceder(p.attaque);
			if(c == nil || c->mort){ p.attaque = -1; continue; }
			// La poursuite se fait à la course ; à portée, un coup toutes les SCRIPT_PED_CADENCE s.
			if(!Pas(p, c->pos, ScriptPedVitesse(ALLURE_COURSE) * dt, SCRIPT_PED_PORTEE)) continue;
			p.prochainCoup -= dt;
			if(p.prochainCoup > 0) continue;
			p.prochainCoup = SCRIPT_PED_CADENCE;
			p.aFrappe = true;
			Sante(p.attaque, c->sante - SCRIPT_PED_DEGATS);
			if(c->mort) p.attaque = -1;
		}else if(p.enMarche){
			if(Pas(p, p.cible, p.vitesse * dt, 0)){ p.enMarche = false; p.arrive = true; }
		}
	}
}
