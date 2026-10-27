// La garde-robe du joueur (CClothingManager, 0x20c5f70) : FUN_006cbf70, FUN_006cb5c0, FUN_006ca890. Sans fichier du jeu.
#include "../src/peds/Clothing.h"
#include <cstdio>

static int echecs = 0;
#define VERIF(c) do{ if(!(c)){ printf("échec ligne %d : %s\n", __LINE__, #c); echecs++; } }while(0)

int
main(void)
{
	CClothingManager g;
	const uint32 pieces[GARDE_ROBE_PIECES] = {1, 2, 3, 4, 5, 6};
	VERIF(g.Portee() == -1 && !g.PorteUneTenue());
	VERIF(g.Definir(100, 200, pieces, false) == -1);                  // fermée : rien
	g.Ouvrir(true);
	VERIF(g.Definir(100, 200, pieces, false) == 0 && g.Definir(101, 201, pieces, true) == 1);
	VERIF(g.Tenue(0).drapeaux == 0x11 && g.Tenue(1).drapeaux == 0x13 && g.Tenue(1).pieces[5] == 6);
	g.Ouvrir(false);
	VERIF(g.Mettre(101, true) && g.Portee() == -1 && !g.Porte(101));   // ClothingSetPlayerOutfit("Starting", true)
	VERIF(g.Mettre(101, false) && g.Porte(101) && !g.Porte(100) && g.PorteUneTenue());
	VERIF(!g.Mettre(999, false) && g.Porte(101));                       // inconnue : rien ne change
	g.Ouvrir(true);
	for(int i = 2; i < GARDE_ROBE_TENUES; i++) VERIF(g.Definir(100 + i, 0, pieces, false) == i);
	VERIF(g.Definir(500, 0, pieces, false) == -1);                       // 35 places
	printf("test_clothing : %s\n", echecs == 0 ? "OK" : "ECHEC");
	return echecs != 0;
}
