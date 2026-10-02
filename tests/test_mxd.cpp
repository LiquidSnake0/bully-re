// Lit les points d'attache : Models/Peds/MXDs.MGR et Models/WProps/MXDs.MGR.
//   BULLY_DATA=<racine du jeu> build/tests/test_mxd
#include "../src/core/FileMgr.h"
#include "../src/anim/Mxd.h"
#include "../src/anim/Hxd.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

static uint8 *
Lire(const char *nom, uint32 *n)
{
	const int32 MAX = 1 << 20;
	uint8 *b = (uint8*)malloc(MAX);
	int32 k = CFileMgr::LoadFile(nom, b, MAX, "rb");
	if(k <= 0){ free(b); return nil; }
	*n = (uint32)k;
	return b;
}

int
main(void)
{
	uint32 n; uint8 *b = Lire("Models\\Peds\\MXDs.MGR", &n);
	VERIF(b != nil);
	if(!b){ printf("test_mxd : ECHEC\n"); return 1; }
	CMxdFile f;
	VERIF(f.Load(b, n));
	printf("peds : %zu modèles, %u octets lus sur %u\n", f.modeles.size(), f.lus, n);
	VERIF(f.modeles.size() == 358 && f.lus == n);
	free(b);

	// Les os des points sont ceux de MAINPED.HXD, par leur numéro.
	CHxdFile mainped; uint8 *h = Lire("Anim\\MAINPED.HXD", &n);
	VERIF(h && mainped.Load(h, n)); free(h);
	int points = 0, osJustes = 0, mainpeds = 0, cigs = 0;
	for(const CMxdModele &m : f.modeles){
		VERIF(fabsf(m.version - 1.06f) < 1e-4f);
		if(m.anims != "ANIM\\MAINPED") continue;
		mainpeds++;
		for(const CMxdPoint &p : m.points){
			points++;
			osJustes += p.numeroOs >= 0 && p.numeroOs < (int32)mainped.os.size() && mainped.os[p.numeroOs] == p.os;
		}
		const CMxdPoint *g = m.Point(0x47c19c6a), *b2 = m.Point(CMxdFile::Hachage("MouthCig")), *d = m.Point(0x2654fed);
		// La cigarette de gauche est tenue par la main ou entre deux doigts (Left Finger2).
		cigs += g && b2 && d && (g->os == "Left Hand" || g->os == "Left Finger2") && b2->os == "Jaw" && d->os == "Right Hand";
	}
	printf("%d modèles MAINPED, %d points, %d sur leur os du HXD, %d avec les trois points de cigarette\n", mainpeds, points, osJustes, cigs);
	VERIF(mainpeds == 357 && osJustes == points && cigs == 355);
	// Les hachages des pistes PropAttachEx d'Ambient.cat (docs/cat.md).
	VERIF(CMxdFile::Hachage("LeftCig") == 0x47c19c6a && CMxdFile::Hachage("RightCig") == 0x2654fed);
	const CMxdModele *joueur = f.Chercher("player");
	VERIF(joueur && joueur->points.size() == 10 && joueur->nOs == 36);
	if(joueur){ const CMxdPoint *p = joueur->Point(CMxdFile::Hachage("MouthCig")); VERIF(p && p->numeroOs == 15 && fabsf(p->pos[0] - 0.14f) < 1e-3f); }

	b = Lire("Models\\WProps\\MXDs.MGR", &n);
	CMxdFile w;
	VERIF(b && w.Load(b, n));
	printf("wprops : %zu modèles, %u octets lus sur %u\n", w.modeles.size(), w.lus, n);
	VERIF(w.modeles.size() == 237 && w.lus == n);
	free(b);
	printf("test_mxd : %s\n", echecs ? "ECHEC" : "ok");
	return echecs != 0;
}
