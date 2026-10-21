// Les événements de parole des scripts (docs/son.md) : la table de bully.exe en 0x00ae74e8, 493 pointeurs
// de chaînes finis par « NUM_SPEECH_EVENTS ». SoundPlayScriptedSpeechEvent (0x5d37b0) et
// SoundSpeechPlaying (0x5d3a50) cherchent le nom donné par le script dans cette table, sans la casse
// (stricmp), et passent son index au moteur du son (FUN_005a4980 pour jouer, FUN_005a27b0 pour savoir si
// ça parle) ; un nom absent fait sortir sans rien jouer.
#pragma once
#include "../common.h"

enum { NUM_SPEECH_EVENTS = 493 };
extern const char *const kSpeechEvents[NUM_SPEECH_EVENTS];

// L'index d'un événement de parole dans la table, sans la casse ; -1 s'il n'y est pas.
int SpeechEventIndex(const char *nom);

// Le préchargement d'une ligne (SoundPreloadSpeech, 0x5d4a20) : une seule à la fois, dans le bloc
// DAT_00cfb3b4 que le moteur du son reçoit avec le rappel FUN_005d49d0.
//   +0 en attente (posé à 1), +4 échéance (l'heure du jeu DAT_00c1a9b4 + 30 000 ms), +8 jouer
//   (SoundPlayPreloadedSpeech, 0x5d39d0), +9 préchargée (ce que SoundIsSpeechPreloaded, 0x5d39e0, rend),
//   +0xc le locuteur.
struct CSpeechPreload {
	bool attente = false, jouer = false, prechargee = false;
	uint32 echeance = 0;
	int ped = -1, evenement = -1, ligne = -1;

	// SoundPreloadSpeech après la recherche du nom : tout est remis à zéro, l'échéance à 30 s.
	void Precharger(uint32 maintenant, int locuteur, int index, int numero);
	// FUN_005d49d0(prete, bloc), le rappel du moteur du son tant que la ligne attend : prête, elle devient
	// « préchargée » ; si on n'a pas encore demandé de la jouer, elle attend (faux), et l'échéance passée
	// l'annule (FUN_005a5880(locuteur, 3, 0x19)) ; sinon elle sort de l'attente et se joue (vrai).
	bool Rappel(bool prete, uint32 maintenant, bool *annulee = nil);
};
