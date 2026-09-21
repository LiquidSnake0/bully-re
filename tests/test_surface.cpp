// Premier test contre le vrai jeu : charge Config/Dat/SURFACE.DAT avec le
// CSurfaceTable::Initialise recréé et affiche la table 6×6.
// Construction : tests/construire_tests.sh, puis
//   build/tests/test_surface <chemin>/Config/Dat/SURFACE.DAT
#include "../src/core/SurfaceTable.h"
#include <cstdio>

int
main(int argc, char **argv)
{
	if(argc < 2){ fprintf(stderr, "usage : test_surface SURFACE.DAT\n"); return 2; }
	CSurfaceTable::Initialise(argv[1]);
	const char *noms[NUMADHESIVEGROUPS] = { "rubber", "hard", "road", "loose", "sand", "wet" };
	printf("%8s", "");
	for(int j = 0; j < NUMADHESIVEGROUPS; j++) printf("%8s", noms[j]);
	printf("\n");
	for(int i = 0; i < NUMADHESIVEGROUPS; i++){
		printf("%8s", noms[i]);
		for(int j = 0; j < NUMADHESIVEGROUPS; j++)
			printf("%8.3f", CSurfaceTable::ms_aAdhesiveLimitTable[i][j]);
		printf("\n");
	}
	return 0;
}
