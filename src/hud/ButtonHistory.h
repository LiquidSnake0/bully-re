// L'historique des boutons (HUDButtonHistory, le composant 21 du HUD ; vftable 0x919140) : les leçons de combat de 1_02 et de
// 1_06, les séquences de boutons que les scripts demandent au joueur (ButtonHistoryAddSequence…) et les rappels qu'ils posent.
//
// L'objet est DAT_00c674f4. Ses champs (lus à l'assembleur des fonctions Lua de 0x5bd720 à 0x5bdaad et dans l'index : FUN_0053abb0,
// FUN_0053ad50, FUN_0053ac70, FUN_0053b2e0, FUN_0053ade0, FUN_0053acc0, FUN_0053b340, FUN_0053b370, FUN_0053c250) :
//   +0x34  le nombre de pas au plus (9, posé par FUN_0053b080)
//   +0x6c  la manette ignorée (ButtonHistoryIgnoreController) ; +0x70, +0x74, +0x78 les rappels « réussie », « ratée », « bon bouton »
//   +0x7c  le pas courant ; +0x80 le nombre de pas tenus ; +0x84 les pas (0x18 octets) ; +0x88 leur nombre
//   +0x8c, +0x90  l'historique de la manette (le mode sans l'arbre : non suivi) ; +0x94, +0x98 les boutons ignorés
//   +0x9c  l'heure du dernier événement ; +0xa0 le délai du pas ; +0xa4 ; +0xa8, +0xac le bouton et l'état que l'arbre rapporte
//   +0xb0  l'entrée par l'arbre d'actions (ButtonHistoryEnableActionTreeInput)
//   +0xb4  la durée de la séquence (-1 sans) ; +0xb8 son temps (jamais avancé dans le mode de l'arbre) ; +0xbc son départ
//   +0xc0  le bouton du dernier échec ; +0xc4 le masque des boutons des pas ; +0xc8 échouée ; +0xca un événement à cette image ;
//   +0xcb l'échec déjà annoncé ; +0xcc le texte (ButtonHistoryAddSequenceLocalText, 31 caractères)
// Un pas : +0 le bouton, +8 « tenir », +0xa fait, +0xc son délai (750 ms pour ButtonHistoryAddSequence, 0x2ee), +0x10 et +0x14 deux
// durées (ButtonHistoryAddSequenceTimeIntervalCharge ; -1 sinon).
#pragma once
#include "../common.h"
#include <string>

struct CPasBouton {
	int32 bouton = -1;          // +0
	int32 mot4 = 0;             // +4
	bool tenir = false;         // +8
	bool fait = false;          // +0xa
	int32 delai = 0;            // +0xc (ms)
	int32 charge1 = -1, charge2 = -1;   // +0x10, +0x14
};

class CButtonHistory
{
public:
	static const int kPas = 9;  // +0x34
	CPasBouton pas[kPas + 1];   // la case après le dernier reçoit la copie d'un dernier pas tenu (FUN_0053b2e0)
	int32 nombre = 0, tenus = 0, courant = 0;   // +0x88, +0x80, +0x7c
	int32 ignores[kPas] = {};   // +0x94
	int32 nombreIgnores = 0;    // +0x98
	bool manetteIgnoree = false, parArbre = false;   // +0x6c, +0xb0
	uint32 dernier = 0;         // +0x9c
	int32 delaiPas = 0;         // +0xa0
	bool octetA4 = false;       // +0xa4
	int32 boutonRapporte = 0, etatRapporte = 0;      // +0xa8, +0xac
	int32 duree = -1;           // +0xb4
	uint32 temps = 0, depart = 0;                    // +0xb8, +0xbc
	int32 boutonEchoue = 0;     // +0xc0
	uint32 masque = 0;          // +0xc4
	bool echouee = false, octetC9 = false, evenement = false, echecAnnonce = false;   // +0xc8 … +0xcb
	std::string texte;          // +0xcc

	// FUN_0053abb0 (ButtonHistoryClearSequence) : les pas vidés (bouton -1, délai au +0xa0, durées -1), l'historique et les
	// ignorés aussi ; les comptes, le pas courant, le masque, +0xa4, +0xc9, +0xca, +0xb8 et le texte à 0, +0x3c à 100, +0x40 à 1,
	// +0xcb à 1. Les rappels restent.
	void Effacer(void);
	// FUN_0053ad50(bouton, tenir, délai, durée1, durée2) : un pas de plus s'il reste de la place, son bit au masque, les tenus comptés.
	void Ajouter(int32 bouton, bool tenir, int32 delai, int32 charge1 = -1, int32 charge2 = -1);
	// FUN_0053ac70 (ButtonHistorySetSequenceTime) : +0xb8 à 0 ; la durée, -1 si elle n'est pas positive.
	void PoserDuree(int32 ms) { temps = 0; duree = ms > 0 ? ms : -1; }
	// FUN_0053b2e0 (ButtonHistoryIgnoreController) : +0x6c, +0x9c et +0xbc à l'heure ; un dernier pas tenu est recopié derrière.
	void IgnorerManette(bool b, uint32 maintenant);
	// FUN_0053ade0 (ButtonHistoryIgnoreSequence, un appel par bouton) : un bouton ignoré de plus, s'il reste de la place.
	void Ignorer(int32 bouton) { if(nombreIgnores < kPas) ignores[nombreIgnores++] = bouton; }
	// FUN_0053b340, depuis la piste ActionRegisterButtonHistory (son lancement FUN_0061bab0) : dans le mode de l'arbre, le bouton
	// (+0x20 de la piste) et l'état (+0x24 : 0 à la pression, 1 relâché, 3 tenu) en +0xa8 et +0xac, +0x9c à l'heure.
	void Rapporter(int32 bouton, int32 etat, uint32 maintenant);
	// FUN_0053acc0 : les pas défaits ; le pas courant, +0xb8, l'historique, +0xa4, +0xc9, +0xcb, +0xa8 et +0xac à 0.
	void Remettre(void);

	// Ce qu'une image décide (FUN_0053c250, lue dans l'index) : le rappel « réussie » (sans argument ; la manette est alors ignorée),
	// « bon bouton » (le bouton +0xc0) et « ratée » (le bouton +0xc0 et 1 si la durée de la séquence est écoulée).
	struct Evenements { bool reussie = false, bonBouton = false, ratee = false, expiree = false; int32 bouton = 0; };
	// FUN_0053c250 hors du dessin, avec FUN_0053b370 dans le mode de l'arbre : rien sans pas (le jeu dessine alors l'historique de
	// la manette) ni avec la manette ignorée. `rappels` dit lesquels des trois rappels sont posés (FUN_005d7f80).
	Evenements Avancer(uint32 maintenant, bool rappelReussie, bool rappelRatee, bool rappelBonBouton);
};
