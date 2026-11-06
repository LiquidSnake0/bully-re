// La barre de santé secondaire du HUD (HUDSecondaryHealthBar, le composant 12 de la table DAT_00c674a0, DAT_00c674d0) : la santé
// d'un piéton de mission (Bucky dans 1_07, Algie dans 1_05) et d'un second, avec un ou deux textes. Lu à l'assembleur et dans l'index
// le 06.11.2026 : PedShowHealthBar (0x5ca5d0), PedHideHealthBar (0x5c6770), FUN_00553690, FUN_00551380, FUN_005513b0.
#pragma once
#include "../common.h"
#include <cstring>

struct CBarreSante {
	int pieton = -1;          // +0xac : une référence (FUN_00465680), -1 sans piéton
	int second = -1;          // +0xb0
	bool aTexte = false;      // +0xb8
	bool aTexte2 = false;     // +0xb9
	bool octetBA = false;     // +0xba : le quatrième argument de PedShowHealthBar
	bool octetBB = false;     // +0xbb : remis à 0 quand +0xba l'est
	char texte[0x41] = {};    // +0xbe, 64 caractères (_strncpy)
	char texte2[0x41] = {};   // +0xfe
	float part = 0.0f;        // +0x140 : la santé du piéton sur son maximum
	float part2 = 0.0f;       // +0x144 : celle du second
	float mots148[4] = {};    // +0x148 … +0x154, remis à 0

	// FUN_00553690(piéton, second) (lu dans l'index ; Ghidra le nomme PedHideHealthBar) : les références +0xac et +0xb0 changées
	// (FUN_00465690 relâche l'ancienne, FUN_00465680 prend la nouvelle), celle de +0xb4 relâchée et vidée, +0x148 … +0x154 à 0, puis,
	// pour chaque piéton donné, sa part de santé (+0x1cb8 / +0x1ca4) en +0x140 ou +0x144 ; sans piéton, la part garde sa valeur.
	void Poser(int p, float sante, float santeMax, int s, float sante2, float santeMax2) {
		pieton = p; second = s;
		for(float &m : mots148) m = 0.0f;
		if(p >= 0) part = sante / santeMax;
		if(s >= 0) part2 = sante2 / santeMax2;
	}
	// FUN_00551380(texte) : +0xb8 à 0, puis, avec un texte, à 1 et le texte copié en +0xbe (64 caractères au plus, _strncpy).
	void PoserTexte(const char *t) {
		aTexte = false;
		if(t){ aTexte = true; strncpy(texte, t, 0x40); texte[0x40] = 0; }
	}
	// FUN_005513b0(texte) : la même chose pour +0xb9 et +0xfe.
	void PoserTexte2(const char *t) {
		aTexte2 = false;
		if(t){ aTexte2 = true; strncpy(texte2, t, 0x40); texte2[0x40] = 0; }
	}
};
