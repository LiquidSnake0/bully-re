// Le gestionnaire des collectibles (src/objects/Collectibles) : la lecture de colctbls.dat (FUN_004287a0, FUN_00426080), les types
// (FUN_005dad80), la disponibilité (FUN_00426340), les entrées prises (FUN_004288c0, FUN_004262f0, FUN_00428ba0). Aucun fichier du
// jeu : un texte de la même forme.
//   build/tests/test_collectibles
#include "../src/objects/Collectibles.h"
#include "../src/core/ActionTree.h"
#include <cstdio>
#include <cstring>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	VERIF(CCollectibles::Type("Gnome") == COLLECTIBLE_GNOME);
	VERIF(CCollectibles::Type("Radio") == COLLECTIBLE_RADIO);
	VERIF(CCollectibles::Type("Artifact") == COLLECTIBLE_ARTIFACT);
	VERIF(CCollectibles::Type("radio") == -1);   // strcmp : la casse compte
	const char *texte =
		"# this document sets up the collectibles items\r\n"
		"# blank lines and lines that start with # are ignored\r\n"
		"\r\n"
		"Radio\r\n"
		"radio01\r\n"
		"\tradio02\r\n"
		"\r\n"
		"radio06\r\n"
		"END\r\n"
		"\r\n"
		"RubberBand\r\n"
		"rubband01\r\n"
		"rubband02\r\n"
		"end\r\n"
		"Inconnu\r\n"
		"truc01\r\n"
		"end\r\n";
	static CCollectibles c;
	c.Initialiser(texte, strlen(texte));
	VERIF(c.nombre == 5);
	VERIF(c.NombreDuType(COLLECTIBLE_RADIO) == 3 && c.NombreDuType(COLLECTIBLE_RUBBERBAND) == 2 && c.NombreDuType(COLLECTIBLE_GNOME) == 0);
	VERIF(c.Indice(ActionHash("radio06")) == 2 && c.entrees[2].hache == 0x714e6af9u);   // le +116 de l'entrée inst de zone_school6.ipb
	VERIF(c.Indice(ActionHash("radio02")) == 1 && c.Indice(ActionHash("truc01")) == -1);
	VERIF(c.Disponible(0) && !c.Pris(0) && c.Ramassage(0) == -1);
	VERIF(!c.AuSol(0));
	c.PoserDisponible(COLLECTIBLE_RADIO, false);
	VERIF(!c.Disponible(0) && !c.Disponible(2) && c.Disponible(3));
	c.PoserDisponible(COLLECTIBLE_RADIO, true);
	VERIF(c.Disponible(2));
	c.PoserRamassage(2, 0x10004);
	CRappelCollectible r;
	VERIF(c.Marquer(2, true, &r));
	VERIF(r.type == COLLECTIBLE_RADIO && r.pris == 1 && r.nombre == 3 && r.hache == 0x714e6af9u && r.ramassage == 0x10004 && r.indice == 2);
	VERIF(c.Pris(2) && c.PrisDuType(COLLECTIBLE_RADIO) == 1);
	VERIF(!c.Marquer(2, false, &r) && !c.Pris(2));
	VERIF(!c.tousLesElastiques);
	CRappelCollectible rs[4];
	VERIF(c.ToutMarquer(COLLECTIBLE_RUBBERBAND, true, rs, 4) == 2);
	VERIF(c.tousLesElastiques && rs[0].pris == 1 && rs[1].pris == 2 && rs[1].indice == 4);
	VERIF(c.PrisDuType(COLLECTIBLE_RUBBERBAND) == 2);
	printf("%s\n", echecs ? "test_collectibles : ÉCHEC" : "test_collectibles : ok");
	return echecs ? 1 : 0;
}
