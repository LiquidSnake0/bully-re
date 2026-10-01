// Sonde jetable : les trajets « pthx » d'un ou de tous les .ipb.
//   BULLY_DATA=<jeu> build/outils/sonde_trajets [fichier.ipb] [nombre de points montrés]
#include "commun.h"
#include "../src/core/IplFile.h"

static int g_montres = 4;
static int32 Montrer(const CIplPthx &t){
	printf("  « %s » a=%d b=%d (0x%x 0x%x), %d points\n", t.name, t.a, t.b, t.a, t.b, t.numPoints);
	for(int i = 0; i < t.numPoints && i < g_montres; i++){
		const CIplPthxPoint &q = t.points[i];
		printf("    %3d :", i);
		for(int k = 0; k < 3; k++) printf("  (%.3f %.3f %.3f)", q.v[k][0], q.v[k][1], q.v[k][2]);
		printf("\n");
	}
	return 0;
}

int
main(int argc, char **argv)
{
	outil::Archives a;
	if(!outil::Ouvrir(a)) return 1;
	if(argc > 2) g_montres = atoi(argv[2]);
	CIplFile::ms_pthxHandler = Montrer;
	const CdImage &im = CdStream::ms_images[a.monde];
	for(int32 e = 0; e < im.m_numEntries; e++){
		const char *n = im.m_entries[e].name; size_t L = strlen(n);
		if(L < 4 || strcasecmp(n + L - 4, ".ipb") != 0) continue;
		if(argc > 1 && strcasecmp(n, argv[1]) != 0) continue;
		uint32 nb; uint8 *b = outil::LireMonde(a, n, &nb);
		int32 avant = CIplFile::ms_numPthx;
		printf("%s\n", n);
		if(b){ CIplFile::Load(b, nb); free(b); }
		if(CIplFile::ms_numPthx == avant) printf("  (aucun trajet)\n");
	}
	return 0;
}
