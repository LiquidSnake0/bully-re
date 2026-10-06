// La synchronisation labiale (docs/lip.md) : les 493 lipFile_*.lip de World.img.
//   BULLY_DATA=<racine du jeu> build/tests/test_lip
#include "../src/core/FileMgr.h"
#include "../src/core/CdStream.h"
#include "../src/anim/Lip.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <strings.h>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

static uint8 *
Lire(const char *nom, uint32 *bytes)
{
	const CDirectoryEntry *d = CdStream::ms_images[0].Find(nom);
	if(d == nil) return nil;
	*bytes = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(*bytes);
	int32 fd = CFileMgr::OpenFile("Stream\\World.img", "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, *bytes);
	CFileMgr::CloseFile(fd);
	if(!ok){ free(buf); return nil; }
	return buf;
}

int
main(void)
{
	VERIF(CdStream::AddImage("Stream\\World.img") == 0);
	// Le premier fichier, valeurs relevées à l'octet.
	uint32 n; uint8 *b = Lire("lipFile_000.lip", &n);
	VERIF(b != nil);
	if(!b){ printf("test_lip : ECHEC\n"); return 1; }
	CLipFile f;
	VERIF(f.Load(b, n));
	VERIF(f.repliques.size() == 120);
	VERIF(f.donnees.size() == 2191);
	const CLipReplique &r = f.repliques[0];
	VERIF(r.id == 11783 && r.images == 55 && r.depart == 0);
	VERIF(fabsf(r.duree - 1.6448f) < 1e-3f);
	VERIF(r.sonDecalage == 0xc50800 && r.sonTaille == 0x5800);
	VERIF(f.repliques[1].depart == 14 && f.repliques[2].depart == 36);   // ceil(55 / 4) = 14, puis + ceil(88 / 4)
	// Poids forts d'abord : l'image 0 est dans les bits 7-6 du premier octet.
	VERIF(f.Niveau(r, 0) == (f.donnees[0] >> 6 & 3));
	VERIF(f.Niveau(r, 3) == (f.donnees[0] & 3));
	// L'ouverture : aux images exactes, niveau / 3 ; entre deux, l'interpolation.
	VERIF(fabsf(f.Ouverture(r, 0) - f.Niveau(r, 0) / 3.0f) < 1e-6f);
	VERIF(fabsf(f.Ouverture(r, 1) - f.Niveau(r, 54) / 3.0f) < 1e-6f);
	float mi = f.Ouverture(r, 0.5f / 54);
	VERIF(fabsf(mi - 0.5f * (f.Niveau(r, 0) + f.Niveau(r, 1)) / 3.0f) < 1e-5f);
	free(b);

	// Tous les fichiers : en-tête cohérent, répliques contiguës, rien après les données.
	int fichiers = 0, vides = 0, repliques = 0, contigus = 0, propres = 0; long niveaux[4] = {0};
	const CdImage &im = CdStream::ms_images[0];
	for(int32 e = 0; e < im.m_numEntries; e++){
		const char *nom = im.m_entries[e].name; size_t l = strlen(nom);
		if(l < 4 || strcasecmp(nom + l - 4, ".lip") != 0) continue;
		uint32 tn; uint8 *tb = Lire(nom, &tn);
		CLipFile g;
		if(!tb || !g.Load(tb, tn)){ printf("  %s illisible\n", nom); free(tb); continue; }
		fichiers++;
		if(g.repliques.empty()) vides++;
		uint32 s = 0; bool ok = true;
		for(const CLipReplique &x : g.repliques){
			if(x.depart != s) ok = false;
			s += (x.images + 3u) / 4;
			for(int32 k = 0; k < x.images; k++) niveaux[g.Niveau(x, k)]++;
			repliques++;
		}
		contigus += ok && s == g.donnees.size();
		uint32 fin = 4 + 24 * (uint32)g.repliques.size() + (uint32)g.donnees.size();
		bool zero = true; for(uint32 i = fin; i < tn; i++) if(tb[i]){ zero = false; break; }
		propres += zero;
		free(tb);
	}
	printf("  %d fichiers (%d vides), %d répliques, %d contigus, %d sans octet après les données\n", fichiers, vides, repliques, contigus, propres);
	printf("  niveaux : 0 = %ld, 1 = %ld, 2 = %ld, 3 = %ld\n", niveaux[0], niveaux[1], niveaux[2], niveaux[3]);
	VERIF(fichiers == 493 && vides == 25 && repliques == 40882);
	VERIF(contigus == 493 && propres == 493);
	VERIF(niveaux[0] == 2014903 && niveaux[1] == 302509 && niveaux[2] == 538355 && niveaux[3] == 445987);

	printf(echecs ? "test_lip : ECHEC (%d)\n" : "test_lip : OK\n", echecs);
	return echecs ? 1 : 0;
}
