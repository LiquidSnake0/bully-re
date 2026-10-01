// L'heure du jeu et les moments de la journée (docs/horloge.md).
//
// Établi depuis les scripts du jeu (Scripts.img, désassemblés par tools/lur.py) :
//  - départ à 8 h 00 : SInitGl.lur, gGameStartHour = 8, gGameStartMinute = 0 ;
//  - une minute de jeu par seconde réelle : main.lur, ClockSetTickRate(60, 30)
//    (60 secondes de jeu par seconde ; le second argument n'est pas encore lu) ;
//  - couvre-feu de 23 h à 7 h : STimeCycle.lur, F_CurfewDefaultRules
//    (heure >= 23 ou heure < 7) ;
//  - classes de 9 h à 11 h 30 et de 13 h à 15 h 30 : bascules horaires de
//    Bdorm.lur (8 h 40 départ, 9 h 15, 11 h 30, 12 h 45, 13 h 15, 15 h 30) et
//    périodes d'apparition de MainMap.lur (11 h 30, 15 h 30).
// Déduit, à confirmer : la nuit de 19 h à 23 h (le jour s'arrête après 18 h dans
// Bdorm.lur ; la vraie borne est la période PER_EVEN de Act/Globals.cat, non décodée).
#pragma once
#include "../common.h"
#include "TriggerFile.h"        // MOMENT_JOUR…

class CHorloge
{
public:
	float minutes = 8 * 60;                 // depuis minuit, dans [0, 1440[
	float minutesParSeconde = 1.0f;         // ClockSetTickRate(60, …) : 60 s de jeu par seconde réelle

	void Regler(int32 heure, int32 minute){ minutes = (float)((heure % 24) * 60 + minute % 60); }
	void Avancer(float secondesReelles){
		minutes += secondesReelles * minutesParSeconde;
		while(minutes >= 1440) minutes -= 1440;
		while(minutes < 0) minutes += 1440;
	}
	int32 Heure(void) const { return (int32)minutes / 60; }
	int32 Minute(void) const { return (int32)minutes % 60; }

	// Le tableau de population à utiliser à cette heure (MOMENT_JOUR, _CLASSE, _NUIT, _COUVREFEU).
	static int32 MomentA(int32 heure, int32 minute){
		int32 m = heure * 60 + minute;
		if(heure >= 23 || heure < 7) return MOMENT_COUVREFEU;
		if((m >= 9 * 60 && m < 11 * 60 + 30) || (m >= 13 * 60 && m < 15 * 60 + 30)) return MOMENT_CLASSE;
		if(heure >= 19) return MOMENT_NUIT;
		return MOMENT_JOUR;
	}
	int32 Moment(void) const { return MomentA(Heure(), Minute()); }
};
