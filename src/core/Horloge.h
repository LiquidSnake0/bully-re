// L'heure du jeu et les moments de la journée (docs/horloge.md).
//
// Les périodes viennent de Config/timeCycl.dat, en clair : MORNING 7-9,
// FIRST_CLASS 9-12, LUNCH_TIME 12-13, SECOND_CLASS 13-16, AFTERNOON 16-21,
// EVENING 21-23, BED_TIME 23-7 (heures pleines ; les couleurs qui suivent sont
// celles de l'horloge à l'écran). Elles donnent le tableau de population :
// classes → CLASS, soirée → NIGHT, coucher → CURFEW, le reste → DAY.
// Le reste vient des scripts (Scripts.img, désassemblés par tools/lur.py) :
//  - départ à 8 h 00 : SInitGl.lur, gGameStartHour = 8 ;
//  - une minute de jeu par seconde réelle : main.lur, ClockSetTickRate(60, 30) ;
//  - le couvre-feu 23 h - 7 h de STimeCycle.lur (F_CurfewDefaultRules) concorde
//    avec BED_TIME.
#pragma once
#include "../common.h"
#include "TriggerFile.h"        // MOMENT_JOUR…
#include <string>
#include <cstring>
#include <cstdlib>

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

	// Une période de timeCycl.dat ; une période qui finit avant de commencer passe minuit.
	struct Periode { char nom[24]; int32 debut, fin; };
	struct TablePeriodes {
		Periode p[16];
		int32 n;
	};
	// Les valeurs du fichier, tant que ChargerPeriodes ne les a pas relues.
	static TablePeriodes &Periodes(void){
		static TablePeriodes t = { { {"MORNING", 7, 9}, {"FIRST_CLASS", 9, 12}, {"LUNCH_TIME", 12, 13}, {"SECOND_CLASS", 13, 16},
		                             {"AFTERNOON", 16, 21}, {"EVENING", 21, 23}, {"BED_TIME", 23, 7} }, 7 };
		return t;
	}
	// Lit le texte de timeCycl.dat : « NOM début fin couleurs… », commentaires « ## ».
	static bool ChargerPeriodes(const char *texte, size_t taille){
		TablePeriodes t; t.n = 0;
		const char *c = texte, *fin = texte + taille;
		while(c < fin && t.n < 16){
			const char *d = c; while(c < fin && *c != '\n') c++;
			std::string l(d, c); if(c < fin) c++;
			size_t a = l.find_first_not_of(" \t\r");
			if(a == std::string::npos || l[a] == '#') continue;
			size_t e = l.find_first_of(" \t", a); if(e == std::string::npos) continue;
			Periode &q = t.p[t.n];
			std::string nom = l.substr(a, e - a);
			strncpy(q.nom, nom.c_str(), sizeof(q.nom) - 1); q.nom[sizeof(q.nom) - 1] = 0;
			char *f; q.debut = (int32)strtol(l.c_str() + e, &f, 10); q.fin = (int32)strtol(f, nil, 10);
			t.n++;
		}
		if(t.n == 0) return false;
		Periodes() = t;
		return true;
	}
	static const char *PeriodeA(int32 heure){
		const TablePeriodes &t = Periodes();
		for(int32 i = 0; i < t.n; i++){
			const Periode &p = t.p[i];
			bool dedans = p.debut <= p.fin ? (heure >= p.debut && heure < p.fin) : (heure >= p.debut || heure < p.fin);
			if(dedans) return p.nom;
		}
		return "";
	}
	// Le tableau de population à utiliser à cette heure (MOMENT_JOUR, _CLASSE, _NUIT, _COUVREFEU).
	static int32 MomentA(int32 heure, int32 minute){
		(void)minute;                                 // les périodes sont en heures pleines
		std::string p = PeriodeA(heure);
		if(p == "FIRST_CLASS" || p == "SECOND_CLASS") return MOMENT_CLASSE;
		if(p == "EVENING") return MOMENT_NUIT;
		if(p == "BED_TIME") return MOMENT_COUVREFEU;
		return MOMENT_JOUR;
	}
	int32 Moment(void) const { return MomentA(Heure(), Minute()); }
};
