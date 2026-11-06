// La minuterie de mission : le composant du HUD qui l'affiche (DAT_00bf4508, HUDMissionTimer, le composant 3 de la table du HUD) et ses
// champs : +0xc la durée en ms, +0x10 son départ, +0x14 le temps écoulé gardé, l'octet +0x19 en marche, +0x1a en pause ; l'horloge en
// ms est DAT_00c1a9b4. Lu à l'assembleur et dans l'index le 06.11.2026 : FUN_0043f600, FUN_0043f540, FUN_0043f560, FUN_0043f5a0, et
// les fonctions Lua MissionTimerStart (0x5c1fb0), MissionTimerStartFixed (0x5c1ff0), MissionTimerPause (0x5c2090), MissionTimerStop
// (0x5c20c0), MissionTimerHasFinished (0x5c20d0), MissionTimerGetTimeRemaining (0x5c2120).
#pragma once
#include "../common.h"

struct CMinuterieMission {
	uint32 duree = 0, depart = 0, ecoule = 0;
	bool enMarche = false, enPause = false;
	// FUN_0043f600(b) : à l'arrêt (b faux), une minuterie en marche et pas en pause garde le temps écoulé (+0x14) ; puis plus en
	// pause, et en marche selon b.
	void Marche(uint32 maintenant, bool b) {
		if(!b && !enPause && enMarche) ecoule = maintenant - depart;
		enPause = false; enMarche = b;
	}
	// FUN_0043f540(s) : la durée s × 1000, le départ maintenant.
	void Poser(uint32 maintenant, int32 secondes) { duree = (uint32)(secondes * 1000); depart = maintenant; }
	// FUN_0043f560(b) (Ghidra la nomme MissionTimerPause) : rien si l'état de pause est déjà b ou si la minuterie n'est pas en marche ;
	// sinon la pause prend b ; en reprenant, le départ recule du temps gardé ; en s'arrêtant, le temps écoulé est gardé (borné à la
	// durée).
	void Pause(uint32 maintenant, bool b) {
		if(enPause == b || !enMarche) return;
		enPause = b;
		if(!b) depart = maintenant - ecoule;
		else { ecoule = maintenant - depart; if(duree < ecoule) ecoule = duree; }
	}
	// FUN_0043f5a0 : en marche et pas en pause, la durée moins le temps écoulé (0 au-delà) ; sinon la durée moins le temps gardé ; en
	// secondes (× 0,001, _DAT_00900fa0).
	float Reste(uint32 maintenant) const {
		int64_t r;
		if(!enPause && enMarche){
			uint32 e = maintenant - depart;
			if(duree < e) return 0.0f;
			r = (int64_t)duree - e;
		}else r = (int64_t)duree - ecoule;
		return (float)r * 0.001f;
	}
	// MissionTimerStartFixed(h, m, s) (0x5c1ff0, lue à l'assembleur de 0x5c1ff0 à 0x5c208d) : l'heure visée en secondes du jour
	// ((h × 60 + m) × 60 + s, sur 16 bits) et l'heure du jeu ((DAT_00bceb4b × 60 + DAT_00bceb4a) × 60 + DAT_00bceb48 : heures, minutes,
	// secondes de l'horloge) ; si la visée est déjà passée, la durée vaut le double de la visée plus 0x5180 (86 400 sur 16 bits) moins
	// l'heure du jeu (lu tel quel : `lea eax, [esi + esi + 0x5180]`), sinon la visée moins l'heure du jeu, sur 16 bits ; puis
	// FUN_0043f600(1) et FUN_0043f540(durée). La durée est en secondes de jeu, posée comme des secondes réelles.
	static uint16 DureeFixe(int32 h, int32 m, int32 s, int32 heureJeu, int32 minuteJeu, int32 secondeJeu) {
		uint16 vise = (uint16)(s + 60 * (60 * h + m));
		uint16 jeu = (uint16)((heureJeu * 60 + minuteJeu) * 60 + secondeJeu);
		return vise < jeu ? (uint16)(2 * vise + 0x5180 - jeu) : (uint16)(vise - jeu);
	}
};
