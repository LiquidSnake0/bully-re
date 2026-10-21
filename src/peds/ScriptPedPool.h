// Les piétons vus par les scripts : un pool de 24 (ms_pPedPool, docs/pools.md), l'entrée 0 étant
// le joueur (lua_PlayerGetPedIndex, 0x5d5260, renvoie 0). Un handle est l'index dans le pool ;
// -1 quand le pool est plein (FUN_005cf6b0 rend -1, que PedCreatePoint transmet au script).
// Chaque piéton créé par un script est noté à son objet script (FUN_005d94d0 ; PedDelete le
// retire, FUN_005d9520) : c'est ce qui permet de ramasser ce qu'une mission a laissé.
// Ici, le piéton n'a qu'un modèle, une position, un cap, une santé, et un geste à la fois :
// aller quelque part (PedMoveToPoint / PedMoveToXYZ) ou attaquer quelqu'un (PedAttack) ; pas de
// monde, pas d'animation, pas de collision : c'est ce qu'il faut à lancer_lua pour que les
// scripts avancent et voient les piétons bouger, arriver et mourir.
#pragma once
#include "../common.h"

enum { SCRIPT_PED_POOL_SIZE = 24, SCRIPT_PED_JOUEUR = 0 };

// Le deuxième argument de PedMoveToPoint / PedMoveToXYZ dans les scripts : 0 et 1 dans LibPed
// et les missions pour marcher, 2 pour courir, 3 pour sprinter (1_01 : `PedMoveToPoint(k, 3, …)`
// pour les deux élèves qui courent). Les vitesses sont des ordres de grandeur, à mesurer sur
// le jeu (docs/missions.md).
enum eScriptPedAllure { ALLURE_MARCHE = 0, ALLURE_MARCHE_VITE = 1, ALLURE_COURSE = 2, ALLURE_SPRINT = 3 };
float ScriptPedVitesse(int allure);   // m/s

// La bagarre, réduite : l'attaquant rejoint sa cible à la course, puis frappe à portée.
enum { SCRIPT_PED_PORTEE_CM = 120, SCRIPT_PED_COUP_PV = 10 };
static const float SCRIPT_PED_PORTEE = 1.2f;        // m
static const float SCRIPT_PED_CADENCE = 0.8f;       // s entre deux coups
static const float SCRIPT_PED_DEGATS = 10.0f;       // points de vie par coup

struct CScriptPed {
	bool vivant = false;         // l'entrée est prise
	int32 modele = -1;
	CVector pos;
	float cap = 0;               // degrés, 0 vers +x comme les lacets des .dat
	int32 script = -1;           // l'objet script qui l'a créé, -1 pour le joueur ou un piéton rendu ambiant
	float sante = 100;           // +0x1cb8
	float santeMin = 0;          // +0x1ca8, PedSetMinHealth (0x5c8ef0)
	bool invulnerable = false;   // +0x138c, PedSetInvulnerable (0x5c9160), PlayerSetInvulnerable (0x5d1810)
	bool invulnerableAuJoueur = false;   // +0x1364, PedSetInvulnerableToPlayer (0x5c9200)
	bool mort = false;
	// Le geste en cours.
	bool enMarche = false;       // va vers `cible` à `vitesse`
	CVector cible;
	float vitesse = 0;
	int attaque = -1;            // handle de la cible de PedAttack, -1 sinon
	float prochainCoup = 0;      // s avant le prochain coup
	// Ce qui s'est passé pendant la dernière image (pour le journal de lancer_lua).
	bool arrive = false, aFrappe = false, vientDeMourir = false;
	int dernierFrappe = -1;      // la cible du dernier coup (pour le journal)
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
	// lua_PedMoveToPoint / lua_PedMoveToXYZ : le piéton part vers un point à une allure.
	bool Aller(int handle, const CVector &cible, int allure);
	// lua_PedStop (0x5c7630) : plus de geste en cours.
	bool Arreter(int handle);
	// lua_PedAttack (0x5c9ca0) : l'attaquant prend la cible ; faux si l'un des deux n'existe pas.
	bool Attaquer(int handle, int cible);
	// lua_PedSetHealth : la santé ; à zéro, le piéton meurt et lâche son geste.
	void Sante(int handle, float sante);
	// FUN_0047b6b0 puis FUN_004782a0 : un coup de `degats` porté par `attaquant` (-1 : personne). Rien si la
	// cible est invulnérable (+0x138c), ou si l'attaquant est le joueur (type 13) et la cible invulnérable
	// au joueur (+0x1364) ; sinon la santé baisse, jamais sous 0 ni sous la santé minimale (+0x1ca8).
	// Renvoie vrai si le coup a porté.
	bool Blesser(int cible, float degats, int attaquant);
	// lua_PedMakeAmbient (0x5ca850) : rendu à la population, plus ramassé par son script.
	bool RendreAmbiant(int handle);
	// Une image de dt secondes : marches, poursuites et coups ; pose les drapeaux d'événements.
	void Avancer(float dt);
private:
	CScriptPed peds[SCRIPT_PED_POOL_SIZE];
};
