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
