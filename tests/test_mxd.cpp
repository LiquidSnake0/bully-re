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
	// Les vecteurs de fin (+0xa8 du squelette) et le réel de référence des groupes du HXD (+0x28 du descripteur) : l'échelle
	// du bassin de FUN_006b6e50 est la norme du vecteur de l'os 1 du modèle, divisée par le réel du groupe de l'animation.
	{
		int tous = 0, memes = 0; float mini = 1e9f, maxi = 0; std::string nMini, nMaxi;
		for(const CMxdModele &m : f.modeles){
			if(m.anims != "ANIM\\MAINPED") continue;
			tous += (int32)m.vecteurs.size() == 3 * m.nOs && m.nOs == (int32)mainped.os.size();
			float n1 = m.NormeOs(1);
			if(n1 > 0.0f && n1 < mini){ mini = n1; nMini = m.nom; }
			if(n1 > maxi){ maxi = n1; nMaxi = m.nom; }
		}
		int refs = 0, sansRef = 0, joueurs = 0;
		for(const CHxdGroupe &g : mainped.groupes){
			if(g.reference == 0.0f){ sansRef++; continue; }
			refs++;
			const CMxdModele *sq = g.squelette.size() > 4 ? f.Chercher(g.squelette.substr(0, g.squelette.size() - 4).c_str()) : nil;
			bool meme = sq && fabsf(sq->NormeOs(1) - g.reference) < 1e-4f;
			memes += meme;
			if(!meme) printf("  le groupe %s : %.4f, écrit sur %s (%s%.4f)\n", g.nom.c_str(), g.reference, g.squelette.c_str(), sq ? "os 1 à " : "absent de MXDs.MGR, ", sq ? sq->NormeOs(1) : 0.0f);
			joueurs += strcasecmp(g.squelette.c_str(), "player.mxd") == 0;
		}
		const CHxdGroupe *gj = nil;
		for(const CHxdGroupe &g : mainped.groupes) if(g.nom == "C_Player") gj = &g;
		printf("vecteurs des os : %d modèles MAINPED en ont 36 ; l'os 1 du joueur (%.4f ; %.4f ; %.4f), norme %.4f ; de %.4f (%s) à %.4f (%s)\n", tous,
		       joueur ? joueur->vecteurs[3] : 0, joueur ? joueur->vecteurs[4] : 0, joueur ? joueur->vecteurs[5] : 0, joueur ? joueur->NormeOs(1) : 0, mini, nMini.c_str(), maxi, nMaxi.c_str());
		printf("réels de référence des groupes de MAINPED.HXD : %d groupes en ont un, %d n'en ont pas, %d égaux à la norme de l'os 1 de leur squelette, %d sur player.mxd ; C_Player %.4f (%s)\n",
		       refs, sansRef, memes, joueurs, gj ? gj->reference : 0, gj ? gj->squelette.c_str() : "?");
		VERIF(tous == 357 && joueur && fabsf(joueur->NormeOs(1) - 0.8462f) < 1e-4f && joueur->NormeOs(99) == 0.0f);
		VERIF(refs + sansRef == 425 && sansRef == 154 && memes == 268 && joueurs == 165);
		VERIF(gj && fabsf(gj->reference - joueur->NormeOs(1)) < 1e-5f && mainped.Groupe(425) == nil && mainped.Groupe(0) == &mainped.groupes[0]);
		VERIF(mainped.vecteursOs.size() == 108 && fabsf(mainped.vecteursOs[5] - 0.8462f) < 1e-4f);
		// L'échelle : un modèle sur une animation de C_Player.
		const CMxdModele *grand = f.Chercher("TO_RichW2_W");
		if(grand && gj) printf("l'échelle du bassin de %s sur C_Player : %.4f\n", grand->nom.c_str(), grand->NormeOs(1) / gj->reference);
	}
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
