// Les piétons vus par les scripts : un pool de 24 (ms_pPedPool, docs/pools.md), l'entrée 0 étant
// le joueur (lua_PlayerGetPedIndex, 0x5d5260, renvoie 0). Un handle est l'index dans le pool ;
// -1 quand le pool est plein (FUN_005cf6b0 rend -1, que PedCreatePoint transmet au script).
// Chaque piéton créé par un script est noté à son objet script (FUN_005d94d0 ; PedDelete le
// retire, FUN_005d9520) : c'est ce qui permet de ramasser ce qu'une mission a laissé.
// Ici, le piéton n'a qu'un modèle, une position, un cap et une santé : pas de monde, pas
// d'animation ; c'est ce qu'il faut à lancer_lua pour que les scripts avancent.
#pragma once
#include "../common.h"

enum { SCRIPT_PED_POOL_SIZE = 24, SCRIPT_PED_JOUEUR = 0 };

struct CScriptPed {
	bool vivant = false;         // l'entrée est prise
	int32 modele = -1;
	CVector pos;
	float cap = 0;               // degrés, 0 vers +x comme les lacets des .dat
	int32 script = -1;           // l'objet script qui l'a créé, -1 pour le joueur
	float sante = 100;
	bool mort = false;
};

class CScriptPedPool
{
public:
	CScriptPedPool(void);
	// FUN_005cf6b0 : la première entrée libre, ou -1 si les 24 sont prises.
	int Creer(int32 modele, const CVector &pos, float cap, int32 script);
	// lua_PedDelete : libère l'entrée ; le joueur ne se supprime pas.
	bool Supprimer(int handle);
	// lua_PedIsValid : un handle qui désigne une entrée prise.
	bool Valide(int handle) const;
	CScriptPed *Acceder(int handle);
	const CScriptPed *Acceder(int handle) const;
	// Libère tout ce qu'un objet script a créé ; renvoie le nombre libéré.
	int SupprimerDuScript(int32 script);
	int Vivants(void) const;
private:
	CScriptPed peds[SCRIPT_PED_POOL_SIZE];
};
