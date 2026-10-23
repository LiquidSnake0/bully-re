// MODELENUM : les noms de modèles de tous les .idb de Objects/ide.img, résolus en indice sans la
// casse, -1 pour un nom inconnu (FUN_0051c1e0).
//   BULLY_DATA=<racine du jeu> build/tests/test_modelenum [--liste]
#include "../src/core/ModelEnum.h"
#include <cstdio>
#include <cstring>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(int argc, char **argv)
{
	CModelEnum m;
	VERIF(m.Charger());
	if(argc > 1 && strcmp(argv[1], "--liste") == 0)
		for(auto &p : m.parNom) printf("%s\t%d\n", p.first.c_str(), p.second);
	printf("%zu noms, %d doublons\n", m.Nombre(), m.doublons);
	VERIF(m.Nombre() > 4000);
	// Des noms que les scripts demandent : MODELENUM._Taxicab (TestCarAI), _GN_BULLY01 (3_XM : le
	// modèle 102, l'élève de 1_01), _TULIPS (3_01D) ; un véhicule est dans la plage 272..298.
	int32 taxi = m.Index("Taxicab");
	VERIF(taxi >= 272 && taxi <= 298);
	VERIF(m.Index("TAXICAB") == taxi && m.Index("taxicab") == taxi);
	VERIF(m.Index("GN_Bully01") == 102 && m.Index("Tulips") == 9642);
	// _WBRUSH (GraffitiCleanup) et _STEMROSE (3_01D) ne sont dans aucun .idb : leurs noms sont dans
	// bully.exe, Config/dat/object.dat et World.img, pas dans la table des CModelInfo lue ici (à lire).
	VERIF(m.Index("WBRUSH") == -1 && m.Index("STEMROSE") == -1);
	VERIF(m.Index("n_existe_pas_du_tout") == -1 && m.Index("") == -1);
	// La colonne STAT des entrées peds : le bully de 1_02 (146) et GN_Bully01 (102).
	VERIF(m.statDuPieton.count(146) && m.statDuPieton.at(146) == "STAT_B_STRIKER_A");
	VERIF(m.statDuPieton.count(102) && m.statDuPieton.at(102) == "STAT_B_STRIKER_B");
	// La colonne SIZE : son rang dans Small, Medium, Large, Fat, Huge (le bully de 1_02 : Medium).
	VERIF(m.tailleDuPieton.count(146) && m.tailleDuPieton.at(146) == 1);
	printf("test_modelenum : %s\n", echecs == 0 ? "OK" : "ECHEC");
	return echecs != 0;
}
