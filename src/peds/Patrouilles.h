// Les rondes : le gestionnaire des trajets de patrouille (les préfets qui font le tour des couloirs).
//
// Dans bully.exe, le gestionnaire est un bloc de 0x198 octets (DAT_00c2b034, alloué par FUN_00472ac0) : l'octet +0 dit si les
// rondes avancent (AreaEnableAllPatrolPaths, FUN_004721a0, le met à 1 ; AreaDisableAllPatrolPaths, FUN_00472710, à 0 ;
// FUN_00472aa0 et FUN_00472ac0 le remettent à 1 et l'octet +1 à 0), puis cent pointeurs d'entrées (+4 … +0x190) et leur nombre
// (+0x194). Chaque entrée fait 0x28 octets, prise dans le pool DAT_020c7c5c (FUN_0045c980) : le trajet en +0, le piéton posé
// sur la ronde en +4, -1 en +8, l'octet +0xc (FUN_00471d20 déjà joué, FUN_004725b0), +0xd, +0xe (recopié dans +0x1398 du
// piéton par FUN_00472320), +0xf (la ronde arrêtée : FUN_004725b0 ne fait rien d'une entrée dont il vaut 1), l'objet script
// propriétaire en +0x10, 2 en +0x14, 3 en +0x18, 0 en +0x1c et +0x20, 1 en +0x24 (le constructeur 0x471ce0, recopié en
// ligne par FUN_004720e0). Les entrées sont reconnues par le nom haché de leur trajet (+8 du trajet).
//
// DATLoad crée les rondes : FUN_006d3b20 → FUN_005ecf00 lit chaque bloc PATH (FUN_005ec480 : +4 AREACODE, +5 PATROLTYPE,
// 0xff tous deux par défaut ; FUN_005ecb20 les lit) et le passe à FUN_004720e0 avec l'objet script qui charge
// (FUN_005db800) ; seul un trajet dont PATROLTYPE n'est pas -1 devient une ronde (vingt sur 1 543 dans les .DAT,
// docs/trigger.md). Une entrée naît arrêtée (+0xf à 1) : AreaInitPatrolPath (0x5b6150 → FUN_00471fd0) ou
// AreaEnablePatrolPath (FUN_00471f80) la mettent en marche. DATUnload la retire (FUN_005ed010 → FUN_00472750),
// AreaClearPatrolPaths (0x5b60d0) retire celles d'un script (FUN_00472880), ou toutes sans script (FUN_004729a0).
//
// Arrêter une ronde (FUN_00472250, ou FUN_004729a0 en la retirant) rend son préfet à la ville : FUN_00471d20 vide sa file
// d'objectifs (FUN_00470110(0xc)) et lui donne un objectif d'errance (FUN_0046fb30), puis FUN_0047b690(1, 1, 1) et
// FUN_00465690 lâchent la référence (+4 à 0). Le préfet lui-même vient de la population (FUN_00472320) : pas suivi ici.
#pragma once
#include "../common.h"
#include <vector>

struct CEntreePatrouille {
	int trajet = -1;              // +0 : le trajet (son handle dans le pool des trajets, 0x20c7c50)
	uint32 hachage = 0;           // le +8 du trajet : la clé de toutes les recherches
	int pieton = -1;              // +4 : le préfet de la ronde, -1 sans lui
	int32 v8 = -1;                // +8
	bool errance = false;         // +0xc : FUN_004725b0 a déjà rendu le préfet à l'errance (FUN_00471d20)
	bool vd = true;               // +0xd
	bool discret = true;          // +0xe : recopié dans +0x1398 du préfet (FUN_00472320) ; le 5e argument d'AreaInitPatrolPath
	bool arretee = true;          // +0xf : la ronde n'avance pas
	int proprietaire = -1;        // +0x10 : l'objet script qui a chargé le trajet
	int32 v14 = 2, v18 = 3;       // +0x14, +0x18 : les 2e et 3e arguments d'AreaInitPatrolPath
	int32 v1c = 0, v20 = 0;       // +0x1c, +0x20
	int32 v24 = 1;                // +0x24 : le 4e argument d'AreaInitPatrolPath (jamais -1)
	int32 heureDebut = -1, heureFin = -1;   // AreaSetActivePatrolPathHours (0x5b6100 → FUN_00471df0) : non lu, gardé
};

class CPatrouilles
{
public:
	static const int kPlaces = 100;   // +4 … +0x190 ; FUN_004720e0 ne vérifie pas la place
	bool actives = true;              // +0
	bool b1 = false;                  // +1
	std::vector<CEntreePatrouille> entrees;   // +0x194 : leur nombre

	// FUN_004720e0 : une ronde neuve pour ce trajet, si son PATROLTYPE (+5) n'est pas -1 et s'il n'en a pas déjà une. Vrai si
	// elle est créée.
	bool Ajouter(int trajet, uint32 hachage, int32 typePatrouille, int proprietaire);
	// L'entrée de ce trajet, -1 sans elle (la boucle commune à FUN_00472250, FUN_00471f80, FUN_00471fd0, FUN_00472750).
	int Trouver(uint32 hachage) const;
	// FUN_00472250 (AreaDisablePatrolPath) : la ronde s'arrête, son préfet est lâché. Rend le préfet lâché, -1 sans lui.
	int Arreter(uint32 hachage);
	// FUN_00471f80 (AreaEnablePatrolPath) : l'octet +0xf à 0.
	void Reprendre(uint32 hachage);
	// FUN_00471fd0 (AreaInitPatrolPath) : rien si le 4e argument vaut -1 ; sinon +0x14, +0x18, +0x24, +0xe, et la ronde repart.
	void Initialiser(uint32 hachage, int32 a, int32 b, int32 c, bool discret);
	// FUN_004721a0 (AreaEnableAllPatrolPaths) : +0 à 1, et chaque entrée repart (la première de son haché, ici elle-même).
	void ReprendreToutes(void);
	// FUN_00472710 (AreaDisableAllPatrolPaths) : +0 à 0, puis FUN_00472250 sur chaque trajet.
	std::vector<int> ArreterToutes(void);
	// FUN_00472750 (DATUnload, par FUN_005ed010) : l'entrée de ce trajet lâche son préfet, rend sa place au pool, et la
	// dernière prend sa case.
	int Retirer(uint32 hachage);
	// FUN_00472880 (AreaClearPatrolPaths dans un script) : la même chose pour chaque entrée de ce propriétaire.
	std::vector<int> RetirerDe(int proprietaire);
	// FUN_004729a0 : toutes, puis le nombre à 0.
	std::vector<int> Vider(void);
private:
	int Lacher(CEntreePatrouille &e);   // FUN_00471d20, FUN_0047b690(1, 1, 1), FUN_00465690 : le préfet rendu, +4 à -1
	int Oter(int i);                    // la case i prend la dernière, le nombre baisse
};
