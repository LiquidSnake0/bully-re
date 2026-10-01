// Le cycle jour-nuit (docs/timecycle.md) : les quatre saisons se lisent en
// 6 météos × 24 heures, les valeurs tombent juste, l'interpolation est continue.
//   BULLY_DATA=<racine du jeu> build/tests/test_timecycle
#include "../src/render/TimeCycle.h"
#include "../src/core/FileMgr.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

static bool Charger(const char *chemin, CTimeCycle &c){
	std::vector<uint8> b(1 << 20);
	int32 n = CFileMgr::LoadFile(chemin, b.data(), (int32)b.size(), "rb");
	return n > 0 && c.Load((const char*)b.data(), (size_t)n);
}

int
main(void)
{
	const char *saisons[] = { "Config\\timecycF.dat", "Config\\timecycW.dat", "Config\\timecycS.dat", "Config\\sbtimecycS.dat" };
	for(const char *s : saisons){
		CTimeCycle c; bool ok = Charger(s, c);
		printf("  %s : %s, %zu météos\n", s, ok ? "lu" : "ILLISIBLE", c.meteos.size());
		VERIF(ok && c.meteos.size() == 6);
	}
	CTimeCycle f; VERIF(Charger("Config\\timecycF.dat", f));
	VERIF(f.meteos[0] == "Sunny");
	const CTimeCycleHeure &minuit = f.heures[0][0], &midi = f.heures[0][12], &h17 = f.heures[0][17];
	VERIF(minuit.ambMonde[0] == 163 && minuit.soleil[0] == 46 && minuit.cielHaut[0] == 10 && minuit.cielBas[2] == 89);
	VERIF(midi.soleil[1] == 160 && midi.cielHaut[2] == 190 && fabsf(midi.fogSt + 80) < 1e-3f && fabsf(midi.farClp - 400) < 1e-3f);
	VERIF(h17.cielBas[0] == 216 && h17.cielBas[2] == 68);             // l'horizon orange du soir
	CTimeCycleHeure x = f.Etat(0, 6.5f);
	VERIF(fabsf(x.cielBas[0] - (184 + 252) / 2.0f) < 1e-3f);
	CTimeCycleHeure a = f.Etat(0, 23.999f), b = f.Etat(0, 0.0f);
	VERIF(fabsf(a.cielBas[2] - b.cielBas[2]) < 0.1f);                  // minuit se recolle
	printf("  automne, beau temps : ciel bas à minuit %.0f %.0f %.0f, à 17 h %.0f %.0f %.0f\n",
	       minuit.cielBas[0], minuit.cielBas[1], minuit.cielBas[2], h17.cielBas[0], h17.cielBas[1], h17.cielBas[2]);
	printf("test_timecycle : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
