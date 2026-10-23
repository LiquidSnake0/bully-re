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
	float sante = 100;           // +0x1cb8 ; le constructeur de CPed (FUN_0047ffe0) la pose à 100, comme +0x1cbc
	float santeMax = 100;        // +0x1ca4, PedSetMaxHealth (0x5c8de0), PedGetMaxHealth (0x5c8d80)
	float santeMin = 0;          // +0x1ca8, PedSetMinHealth (0x5c8ef0)
	float regeneration = 0;      // +0x1cac : la colonne G de pedstats.dat (taux de régénération, n/100)
	int32 stats[64] = {};        // +0x1900 : les 64 colonnes B à BM de son entrée de pedstats.dat (GameGetPedStat)
	bool invulnerable = false;   // +0x138c, PedSetInvulnerable (0x5c9160), PlayerSetInvulnerable (0x5d1810)
	bool invulnerableAuJoueur = false;   // +0x1364, PedSetInvulnerableToPlayer (0x5c9200)
	bool mort = false;
	// Le geste en cours.
	bool enMarche = false;       // va vers `cible` à `vitesse`
	CVector cible;
	float vitesse = 0;
	int attaque = -1;            // handle de la cible de PedAttack, -1 sinon
	int verrou = -1;             // PedLockTarget : la cible verrouillée (FUN_00497d20), -1 sinon
	int verrouMode = 0;          // son troisième argument (le genre de verrouillage)
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
	// lua_PedLockTarget (0x5cb060) : rien si le piéton n'existe pas ; cible -1 lève le verrou (FUN_004989c0(0)) ;
	// sinon, si la cible existe, FUN_00497d20 la verrouille avec le mode donné. Vrai si quelque chose a changé.
	bool Verrouiller(int handle, int cible, int mode);
	// lua_PedSetHealth : la santé ; à zéro, le piéton meurt et lâche son geste.
	void Sante(int handle, float sante);
	// FUN_00478e30 (lua_PedSetStatsType, et la création avec l'entrée du modèle, modelinfo +0x54) : les 64 colonnes de
	// l'entrée de pedstats.dat copiées en +0x1900, la santé maximale = colonne F (entrée +0x2c), la santé = le maximum,
	// la santé minimale à 0, la régénération = colonne G (+0x30). Le reste (vision, armes, vitesse) n'est pas suivi.
	bool PoserStats(int handle, const int32 colonnes[64]);
	// GameGetPedStat (FUN_00476f10) : la colonne n ; le cas des modèles uniques de la colonne 6 n'est pas suivi.
	int32 Stat(int handle, int n) const;
	// GameSetPedStat (FUN_00476fa0) : la colonne n ; 4 (santé maximale) ramène la santé sous le nouveau maximum,
	// 5 pose la régénération. Les autres cas (2, 3, 0x10, 0x16, 0x17 : vision, vitesse) ne sont pas suivis.
	bool PoserStat(int handle, int n, int32 valeur);
	// FUN_0047b6b0 puis FUN_004782a0 : un coup de `degats` porté par `attaquant` (-1 : personne). Rien si la
	// cible est invulnérable (+0x138c), ou si l'attaquant est le joueur (type 13) et la cible invulnérable
	// au joueur (+0x1364) ; sinon la santé baisse, jamais sous 0 ni sous la santé minimale (+0x1ca8).
	// Renvoie vrai si le coup a porté.
	bool Blesser(int cible, float degats, int attaquant);
	// lua_PedMakeAmbient (0x5ca850) : rendu à la population, plus ramassé par son script.
	bool RendreAmbiant(int handle);
	// Une image de dt secondes : marches, poursuites et coups ; pose les drapeaux d'événements. La régénération
	// (FUN_00472ff0, à chaque image depuis FUN_004812d0) : entre 0 et le maximum, la santé gagne
	// régénération / 100 × 0,02 × CTimer::ms_fTimeStep, le pas valant 1 pour 20 ms (FUN_0045b810) : régénération / 100
	// points par seconde, sans dépasser le maximum.
	void Avancer(float dt);
private:
	CScriptPed peds[SCRIPT_PED_POOL_SIZE];
};
