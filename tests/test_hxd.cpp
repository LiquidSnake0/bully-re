// Lit les dictionnaires d'animations : Anim/*.HXD et les entrées d'Anim/hxds.dat.
//   BULLY_DATA=<racine du jeu> build/tests/test_hxd
#include "../src/core/FileMgr.h"
#include "../src/anim/Hxd.h"
#include "../src/core/ActionTree.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

static uint8 *
Lire(const char *nom, uint32 *n)
{
	const int32 MAX = 1 << 20;                 // le plus gros (MAINPED.HXD) fait 451 Ko
	uint8 *b = (uint8*)malloc(MAX);
	int32 k = CFileMgr::LoadFile(nom, b, MAX, "rb");
	if(k <= 0){ free(b); return nil; }
	*n = (uint32)k;
	return b;
}

int
main(void)
{
	static const char *const fichiers[] = { "ANIBBALL", "ANIFOOTY", "BANANA", "BATON", "BBGUN", "BIKE", "BOLTCUTT", "BROCKETL", "COPBIKE",
	                                        "MAINPED", "MOT_CTRL", "RAT_PED", "SCOOTER", "SIAMESE", "SK8BOARD", "SLINGSH", "SPRAYCAN", "SPUDG", "UMBRELLA", "WBALLOON" };
	int lus = 0, exacts = 0, anims = 0, hachagesJustes = 0;
	CHxdFile mainped;
	for(const char *f : fichiers){
		char chemin[64]; snprintf(chemin, sizeof chemin, "Anim\\%s.HXD", f);
		uint32 n; uint8 *b = Lire(chemin, &n);
		VERIF(b != nil);
		if(!b) continue;
		CHxdFile h;
		if(h.Load(b, n)){
			lus++; exacts += h.lus == n; anims += (int)h.anims.size();
			for(const CHxdAnim &a : h.anims) hachagesJustes += a.hachage == ActionHash(a.nom.c_str());
			if(strcmp(f, "MAINPED") == 0) mainped = h;
		}else printf("  illisible : %s\n", chemin);
		free(b);
	}
	printf("%d .HXD lus, %d exactement jusqu'au dernier octet ; %d animations, %d hachages justes\n", lus, exacts, anims, hachagesJustes);
	VERIF(lus == 20 && exacts == 20 && anims == 3913 && hachagesJustes == anims);
	// MAINPED : 46 masques, 36 os, 3 365 animations, 425 groupes.
	VERIF(mainped.masques.size() == 46 && mainped.os.size() == 36 && mainped.anims.size() == 3365 && mainped.groupes.size() == 425);
	const CHxdAnim *p = mainped.Chercher(0x6158a6b0);
	VERIF(p && p->nom == "C_PLAYER\\PUNCH_SLOP_1");
	// NPC_LOVE\KISS_HARD_B : 4,433 s, 5 044 octets dans NPC_Love.agr (n° 8), 4 de plus ici.
	const CHxdAnim *k = mainped.Chercher(ActionHash("NPC_LOVE\\KISS_HARD_B"));
	VERIF(k && fabsf(k->duree - 4.433f) < 0.01f && k->taille == 5044 + 4);
	// Les groupes : Grap y compte 182 104 octets, le fichier 181 868, soit 4 de plus par animation (59).
	bool grap = false;
	for(const CHxdGroupe &g : mainped.groupes) if(g.nom == "Grap") grap = g.taille == 181868 + 4 * 59;
	VERIF(grap);

	// hxds.dat : un compte, puis des entrées « ANIM », taille, HXD.
	uint32 n; uint8 *b = Lire("Anim\\hxds.dat", &n);
	VERIF(b != nil);
	int entrees = 0, entreesExactes = 0, animsProps = 0;
	if(b){
		uint32 q = 4;
		while(q + 8 <= n){
			uint32 t; memcpy(&t, b + q + 4, 4);
			CHxdFile h;
			if(memcmp(b + q, "ANIM", 4) == 0 && q + 8 + t <= n && h.Load(b + q + 8, t)){ entreesExactes += h.lus == t; animsProps += (int)h.anims.size(); }
			entrees++; q += 8 + t;
		}
		VERIF(q == n);
		free(b);
	}
	printf("hxds.dat : %d entrées, %d lues exactement, %d animations\n", entrees, entreesExactes, animsProps);
	VERIF(entrees == 130 && entreesExactes == 130 && animsProps == 402);
	printf("test_hxd : %s\n", echecs ? "ECHEC" : "ok");
	return echecs != 0;
}
