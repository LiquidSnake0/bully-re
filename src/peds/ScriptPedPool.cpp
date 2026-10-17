#include "ScriptPedPool.h"

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
