// L'horloge du jeu (docs/horloge.md) : départ à 8 h, une minute par seconde,
// et les moments de la journée aux bornes établies par les scripts.
#include "../src/core/Horloge.h"
#include <cstdio>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CHorloge h;
	VERIF(h.Heure() == 8 && h.Minute() == 0 && h.Moment() == MOMENT_JOUR);
	h.Avancer(60);                                   // une minute réelle = une heure de jeu
	VERIF(h.Heure() == 9 && h.Moment() == MOMENT_CLASSE);
	h.Regler(23, 59); h.Avancer(2);                  // passe minuit
	VERIF(h.Heure() == 0 && h.Minute() == 1);
	struct { int hh, mm, attendu; } cas[] = {
		{6, 59, MOMENT_COUVREFEU}, {7, 0, MOMENT_JOUR}, {8, 59, MOMENT_JOUR}, {9, 0, MOMENT_CLASSE},
		{11, 29, MOMENT_CLASSE}, {11, 30, MOMENT_JOUR}, {12, 59, MOMENT_JOUR}, {13, 0, MOMENT_CLASSE},
		{15, 29, MOMENT_CLASSE}, {15, 30, MOMENT_JOUR}, {18, 59, MOMENT_JOUR}, {19, 0, MOMENT_NUIT},
		{22, 59, MOMENT_NUIT}, {23, 0, MOMENT_COUVREFEU}, {2, 0, MOMENT_COUVREFEU},
	};
	for(auto &c : cas){
		int m = CHorloge::MomentA(c.hh, c.mm);
		if(m != c.attendu) printf("  %02d:%02d → %s, attendu %s\n", c.hh, c.mm, kMoment[m], kMoment[c.attendu]);
		VERIF(m == c.attendu);
	}
	printf("test_horloge : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
