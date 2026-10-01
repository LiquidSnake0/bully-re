// Sonde jetable : la section « pois » d'un ou de tous les .ipb, point par point.
//   BULLY_DATA=<jeu> build/outils/sonde_pois [fichier.ipb] [nombre de points montrés]
#include "commun.h"
#include "../src/core/IplFile.h"

static int g_montres = 3, g_vus = 0;
static const char *g_fichier = "";
static int32 Montrer(const CIplPois &g, const CIplPoiPoint &p){
	if(g_vus++ >= g_montres) return 0;
	printf("%s « %s » zone %d, %d points : [%s] [%s] [%s] [%s] max %d (%.3f %.3f %.3f) lacet %.1f, rayon %d/%.2f, ignore %d, limite %d ;",
	       g_fichier, g.name, g.zone, g.numPoints, p.genre, p.nom, p.type, p.clique, p.max, p.pos.x, p.pos.y, p.pos.z, p.lacetTangageRoulis[0],
	       (int)p.utiliseRayon, p.rayon, (int)p.ignorePopulation, p.limite);
	for(int k = 0; k < IPL_POI_NUM_PERIODES; k++) if(p.periodes[k]) printf(" %s", kIplPoiPeriode[k]);
	printf("\n");
	return 0;
}

int
main(int argc, char **argv)
{
	outil::Archives a;
	if(!outil::Ouvrir(a)) return 1;
	if(argc > 2) g_montres = atoi(argv[2]);
	CIplFile::ms_poisHandler = Montrer;
	const CdImage &im = CdStream::ms_images[a.monde];
	for(int32 e = 0; e < im.m_numEntries; e++){
		const char *n = im.m_entries[e].name; size_t L = strlen(n);
		if(L < 4 || strcasecmp(n + L - 4, ".ipb") != 0) continue;
		if(argc > 1 && strcmp(argv[1], "-") != 0 && strcasecmp(n, argv[1]) != 0) continue;
		uint32 nb; uint8 *b = outil::LireMonde(a, n, &nb);
		g_fichier = n;
		if(b){ CIplFile::Load(b, nb); free(b); }
	}
	printf("%d points en tout\n", CIplFile::ms_numPois);
	return 0;
}
