// La table des événements de parole (src/audio/SpeechEvents), sans fichier du jeu : les 493 noms de
// bully.exe (0x00ae74e8), la recherche sans la casse de SoundPlayScriptedSpeechEvent (FUN_005d37b0),
// et les bornes que le moteur du son vérifie (FUN_005a4980 refuse un index au-delà de 0x1ec = 492).
//   build/tests/test_speech
#include "../src/audio/SpeechEvents.h"
#include <cstdio>
#include <cstring>
#include <strings.h>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	VERIF(NUM_SPEECH_EVENTS == 493 && 0x1ec == NUM_SPEECH_EVENTS - 1);
	VERIF(strcmp(kSpeechEvents[0], "ALLY_ABOUT_TO_LEAVE") == 0 && strcmp(kSpeechEvents[492], "YEARBOOK") == 0);
	VERIF(SpeechEventIndex("ALLY_ABOUT_TO_LEAVE") == 0 && SpeechEventIndex("YEARBOOK") == 492);
	// Les missions parlent par M_<mission> : M_1_01 (1_01.lua : SoundPlayScriptedSpeechEvent(…, "M_1_01", 101)), M_1_02, M_1_02A.
	VERIF(SpeechEventIndex("M_1_01") == 184 && SpeechEventIndex("M_1_02") == 185 && SpeechEventIndex("M_1_02A") == 186);
	VERIF(SpeechEventIndex("m_1_01") == 184 && SpeechEventIndex("PRINCIPAL_LECTURING") == 399);
	VERIF(SpeechEventIndex("NUM_SPEECH_EVENTS") == -1 && SpeechEventIndex("") == -1 && SpeechEventIndex(nil) == -1 && SpeechEventIndex("M_9_99") == -1);
	// Aucun doublon, sans la casse : la recherche du moteur s'arrête au premier et ne se trompe donc jamais.
	int doublons = 0, missions = 0;
	for(int i = 0; i < NUM_SPEECH_EVENTS; i++){
		if(strncmp(kSpeechEvents[i], "M_", 2) == 0) missions++;
		for(int j = i + 1; j < NUM_SPEECH_EVENTS; j++) if(strcasecmp(kSpeechEvents[i], kSpeechEvents[j]) == 0) doublons++;
	}
	VERIF(doublons == 0 && missions == 98);
	printf("  %d événements, %d de missions (M_…), M_1_01 = %d, aucun doublon\n", NUM_SPEECH_EVENTS, missions, SpeechEventIndex("M_1_01"));
	printf("test_speech : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
