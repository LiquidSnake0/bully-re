// L'horloge du jeu (docs/horloge.md) : départ à 8 h, une minute par seconde,
// les périodes de Config/timeCycl.dat relues dans le fichier, et les moments
// de population à leurs bornes.
//   BULLY_DATA=<racine du jeu> build/tests/test_horloge
#include "../src/core/Horloge.h"
#include "../src/core/FileMgr.h"
#include <cstdio>
#include <vector>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	std::vector<uint8> b(1 << 16);
	int32 n = CFileMgr::LoadFile("Config\\timeCycl.dat", b.data(), (int32)b.size(), "rb");
	VERIF(n > 0 && CHorloge::ChargerPeriodes((const char*)b.data(), (size_t)n));
	const CHorloge::TablePeriodes &t = CHorloge::Periodes();
	VERIF(t.n == 7);
	printf("  timeCycl.dat :");
	for(int32 i = 0; i < t.n; i++) printf(" %s %d-%d", t.p[i].nom, t.p[i].debut, t.p[i].fin);
	printf("\n");
	VERIF(strcmp(t.p[5].nom, "EVENING") == 0 && t.p[5].debut == 21 && t.p[5].fin == 23);
	VERIF(strcmp(t.p[6].nom, "BED_TIME") == 0 && t.p[6].debut == 23 && t.p[6].fin == 7);

	CHorloge h;
	VERIF(h.Heure() == 8 && h.Minute() == 0 && h.Moment() == MOMENT_JOUR);
	h.Avancer(60);                                   // une minute réelle = une heure de jeu
	VERIF(h.Heure() == 9 && h.Moment() == MOMENT_CLASSE);
	h.Regler(23, 59); h.Avancer(2);                  // passe minuit
	VERIF(h.Heure() == 0 && h.Minute() == 1);
	struct { int hh, mm, attendu; } cas[] = {
		{6, 59, MOMENT_COUVREFEU}, {7, 0, MOMENT_JOUR}, {8, 59, MOMENT_JOUR}, {9, 0, MOMENT_CLASSE},
		{11, 59, MOMENT_CLASSE}, {12, 0, MOMENT_JOUR}, {12, 59, MOMENT_JOUR}, {13, 0, MOMENT_CLASSE},
		{15, 59, MOMENT_CLASSE}, {16, 0, MOMENT_JOUR}, {20, 59, MOMENT_JOUR}, {21, 0, MOMENT_NUIT},
		{22, 59, MOMENT_NUIT}, {23, 0, MOMENT_COUVREFEU}, {2, 0, MOMENT_COUVREFEU},
	};
	for(auto &c : cas){
		int m = CHorloge::MomentA(c.hh, c.mm);
		if(m != c.attendu) printf("  %02d:%02d → %s, attendu %s\n", c.hh, c.mm, kMoment[m], kMoment[c.attendu]);
		VERIF(m == c.attendu);
	}
	VERIF(strcmp(CHorloge::PeriodeA(12), "LUNCH_TIME") == 0 && strcmp(CHorloge::PeriodeA(3), "BED_TIME") == 0);
	printf("test_horloge : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
