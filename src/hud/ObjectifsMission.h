// Les objectifs de la mission en cours (« 1_05_MOBJ_03 »…), l'objet 0x20c4420 que MissionObjectiveAdd, MissionObjectiveComplete
// et MissionObjectiveRemove mènent (lus dans l'index et à l'assembleur le 06.11.2026) : dix entrées de 0x54 octets chaînées par
// un octet (le suivant d'une entrée i est à +0x54 × (i + 1) de l'objet), la tête des objectifs en +0x34c, celle des entrées libres
// en +0x34d, leur nombre en +0x34e ; le texte du rappel (+0x34f), son délai (+0x368) et l'échéance du rappel (+0x36c).
#pragma once
#include "../common.h"
#include <string>

struct CObjectifsMission {
	struct Objectif { std::string texte; uint8 octet = 0; bool fait = false, pris = false; int8 suivant = -1; };   // +4, +0x1c, +0x1d, +0x1e
	Objectif objectifs[10];
	int8 tete = -1, libres = 0;      // +0x34c, +0x34d
	uint8 nombre = 0;                // +0x34e
	std::string rappel;              // +0x34f, 24 caractères
	float delaiRappel = -1.0f;       // +0x368 : 4 avec un texte (_DAT_00900b0c), -1 sans (_DAT_009009ac)
	uint32 echeance = 0;             // +0x36c : l'horloge du jeu (DAT_00c1a9b4) plus 180 000 ms
	// La liste libre au départ : 0 → 1 → … → 9 (supposé : le constructeur n'est pas lu).
	CObjectifsMission(void) { for(int i = 0; i < 10; i++) objectifs[i].suivant = (int8)(i < 9 ? i + 1 : -1); }
	// FUN_006ae110 (MissionObjectiveAdd) : rien (-1) quand l'écran de fin se montre (FUN_007053b0, sans couleurs données) ou que
	// dix objectifs sont pris ; sinon la première entrée libre reçoit le texte (24 caractères), l'octet, « fait » à 0 et « pris »
	// à 1, et devient la tête de la liste ; le rappel prend le texte ; le nombre monte ; rend l'indice de l'entrée.
	int Ajouter(const char *texte, uint8 octet, bool ecranDeFin, uint32 maintenant) {
		if(ecranDeFin || nombre >= 10 || libres < 0) return -1;
		int i = libres;
		Objectif &o = objectifs[i];
		o.texte = texte ? std::string(texte).substr(0, 24) : std::string();
		o.octet = octet; o.fait = false; o.pris = true;
		libres = o.suivant;
		o.suivant = tete;               // la tête d'avant passe derrière (-1 quand la liste était vide)
		tete = (int8)i;
		if(texte == nil){ delaiRappel = -1.0f; }
		else { delaiRappel = 4.0f; rappel = std::string(texte).substr(0, 24); }
		nombre++;
		echeance = maintenant + 180000;
		return i;
	}
	// MissionObjectiveComplete (FUN_006ae2f0) : l'octet +0x1d de l'entrée au second argument (1 depuis le Lua).
	void Terminer(int i, bool v, uint32 maintenant) {
		if(i < 0 || i >= 10) return;
		objectifs[i].fait = v;
		echeance = maintenant + 180000;
	}
	// MissionObjectiveRemove (FUN_006ae320) : l'entrée vidée, retirée de la liste et rendue aux libres ; le nombre baisse.
	void Retirer(int i, uint32 maintenant) {
		if(i < 0 || i >= 10) return;
		objectifs[i].texte.clear(); objectifs[i].octet = 0; objectifs[i].fait = false; objectifs[i].pris = false;
		echeance = maintenant + 180000;
		if(nombre == 0) return;
		if(tete == i){ tete = objectifs[i].suivant; objectifs[i].suivant = libres; libres = (int8)i; }
		else {
			int8 p = tete;
			while(p >= 0 && objectifs[p].suivant != i) p = objectifs[p].suivant;
			if(p >= 0){ objectifs[p].suivant = objectifs[i].suivant; objectifs[i].suivant = libres; libres = (int8)i; }
		}
		nombre--;
	}
};
