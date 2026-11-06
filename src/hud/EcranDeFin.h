// L'écran de fin d'une mission ou d'un mini-jeu (« M_PASS », l'argent, les messages de respect) : les globales autour de
// DAT_020c8708, que MinigameSetCompletion, MinigameAddCompletionMsg et MinigameIsShowingCompletion mènent, et que l'affichage
// (FUN_007073c0, à chaque image) compte à rebours. Lu à l'assembleur et dans l'index le 06.11.2026.
#pragma once
#include "../common.h"
#include <string>

struct CEcranDeFin {
	// FUN_00705990 (MinigameSetCompletion → FUN_00707390(texte, réussi, argent, sous-texte, 0, 0)) : le texte (DAT_020c8738, 32
	// caractères), un second texte vide (DAT_020c8758), l'octet « réussi » (DAT_020c8709), l'octet des couleurs données
	// (DAT_020c870a : à 0 depuis le Lua, les couleurs de la réussite FUN_00510c90(0xa6, 0x93, 0x3f, 0xd2) ou de l'échec), le
	// signal de fin (DAT_020c870b) et +0x20c87a8 / +0x20c87a9 à 0, l'argent (_DAT_020c8734, donné au joueur par FUN_00475150 s'il
	// n'est pas nul : non suivi), le sous-texte (DAT_020c8778), les messages vidés (DAT_020c8798 à 0, leur minuterie
	// _DAT_020c879c à 0), et la minuterie de l'écran (_DAT_020c87a0) à 2 s (_DAT_00948e78). Sans couleurs données, FUN_006adec0 et
	// FUN_0055afe0(1, 0) (non suivis).
	std::string texte, sousTexte;
	bool reussi = false, couleurs = false, signal = false;
	float argent = 0.0f;
	float minuterie = 0.0f;             // _DAT_020c87a0
	float minuterieMessages = 0.0f;     // _DAT_020c879c
	struct Message { std::string texte; int32 nombre; };
	Message messages[5];                // DAT_020c8658 + 0x20 × i, le nombre en DAT_020c8640 + 4 × i
	int nombreMessages = 0;             // DAT_020c8798
	void Poser(const char *t, bool r, float a, const char *s) {
		texte = t ? std::string(t).substr(0, 32) : std::string();
		sousTexte = s ? std::string(s).substr(0, 32) : std::string();
		reussi = r; couleurs = false; signal = false; argent = a;
		minuterieMessages = 0.0f; nombreMessages = 0;
		minuterie = 2.0f;
	}
	// FUN_00705360 (MinigameAddCompletionMsg) : un texte non vide, cinq au plus ; la minuterie des messages repart à 2 s.
	void Ajouter(const char *t, int32 n) {
		if(t == nil || *t == 0 || nombreMessages >= 5) return;
		messages[nombreMessages].texte = std::string(t).substr(0, 32);
		messages[nombreMessages].nombre = n;
		minuterieMessages = 2.0f;
		nombreMessages++;
	}
	// MinigameIsShowingCompletion (0x708f90, lu à l'assembleur) : FUN_007053b0 (la minuterie positive) et pas de couleurs données.
	bool Montre(void) const { return minuterie > 0.0f && !couleurs; }
	// FUN_007073c0, à chaque image : la minuterie perd 0,02 (DAT_00bc6ecc) fois le pas (ms_fTimeStep, DAT_00c1a9a4). La tenue de
	// l'écran (une réussite reste à 0,5 s, 0x9055a0, jusqu'au signal DAT_020c870b ou jusqu'à la fin des messages, et le délai de
	// 10 s, 0x2710, d'un état 7 de 0xcf42d4) n'est pas suivie : l'écran s'efface au bout des deux secondes.
	void Avancer(float pas) {
		if(minuterie <= 0.0f) return;
		minuterie -= 0.02f * pas;
		if(minuterie < 0.0f) minuterie = 0.0f;
		if(minuterieMessages > 0.0f){ minuterieMessages -= 0.02f * pas; if(minuterieMessages < 0.0f) minuterieMessages = 0.0f; }
	}
};
