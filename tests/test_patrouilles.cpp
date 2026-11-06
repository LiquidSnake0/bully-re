// Les rondes (src/peds/Patrouilles) : le gestionnaire DAT_00c2b034 et ses entrées, comme DATLoad, AreaDisablePatrolPath,
// AreaEnablePatrolPath, AreaInitPatrolPath, AreaEnableAllPatrolPaths, AreaDisableAllPatrolPaths, AreaClearPatrolPaths et
// DATUnload les mènent. Aucun fichier du jeu.
//   build/tests/test_patrouilles
#include "../src/peds/Patrouilles.h"
#include <cstdio>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CPatrouilles p;
	VERIF(p.actives && p.entrees.empty());
	// FUN_004720e0 : PATROLTYPE -1 (l'octet +5 à 0xff) n'est pas une ronde ; 0 et 7 le sont, une fois chacun.
	VERIF(!p.Ajouter(3, 0x1111, -1, 0));
	VERIF(p.Ajouter(4, 0x2222, 0, 0));
	VERIF(!p.Ajouter(9, 0x2222, 0, 1));
	VERIF(p.Ajouter(5, 0x3333, 7, 1));
	VERIF(p.entrees.size() == 2);
	// Le constructeur 0x471ce0 : +8 -1, +0xc 0, +0xd +0xe +0xf 1, +0x14 2, +0x18 3, +0x24 1 ; la ronde naît arrêtée.
	const CEntreePatrouille &e = p.entrees[0];
	VERIF(e.trajet == 4 && e.pieton == -1 && e.v8 == -1 && !e.errance && e.vd && e.discret && e.arretee);
	VERIF(e.proprietaire == 0 && e.v14 == 2 && e.v18 == 3 && e.v1c == 0 && e.v20 == 0 && e.v24 == 1);
	// AreaEnablePatrolPath (FUN_00471f80) puis AreaDisablePatrolPath (FUN_00472250) : +0xf à 0 puis à 1, le préfet lâché.
	p.Reprendre(0x2222);
	VERIF(!p.entrees[0].arretee);
	p.entrees[0].pieton = 12;
	VERIF(p.Arreter(0x2222) == 12 && p.entrees[0].arretee && p.entrees[0].pieton == -1);
	VERIF(p.Arreter(0x9999) == -1);
	// AreaInitPatrolPath (FUN_00471fd0) : rien si le 4e argument vaut -1 ; sinon les réglages, et la ronde repart.
	p.Initialiser(0x3333, 5, 6, -1, false);
	VERIF(p.entrees[1].arretee && p.entrees[1].v14 == 2);
	p.Initialiser(0x3333, 5, 6, 8, false);
	VERIF(!p.entrees[1].arretee && p.entrees[1].v14 == 5 && p.entrees[1].v18 == 6 && p.entrees[1].v24 == 8 && !p.entrees[1].discret);
	// AreaDisableAllPatrolPaths (FUN_00472710) puis AreaEnableAllPatrolPaths (FUN_004721a0) : +0 et chaque +0xf.
	p.ArreterToutes();
	VERIF(!p.actives && p.entrees[0].arretee && p.entrees[1].arretee);
	p.ReprendreToutes();
	VERIF(p.actives && !p.entrees[0].arretee && !p.entrees[1].arretee);
	// AreaClearPatrolPaths dans un script (FUN_00472880) : ses rondes seulement ; la dernière prend la case ôtée.
	VERIF(p.Ajouter(6, 0x4444, 0, 0));
	p.RetirerDe(1);
	VERIF(p.entrees.size() == 2 && p.Trouver(0x3333) == -1 && p.Trouver(0x4444) == 1);
	// FUN_00472880 ne revoit pas la case qu'il vient de remplir : deux rondes de suite du même script, la seconde reste.
	CPatrouilles q;
	q.Ajouter(1, 0xa, 0, 2); q.Ajouter(2, 0xb, 0, 3); q.Ajouter(3, 0xc, 0, 2);
	q.RetirerDe(2);
	VERIF(q.entrees.size() == 2 && q.Trouver(0xb) == 1 && q.Trouver(0xc) == 0);
	// DATUnload (FUN_00472750) puis FUN_004729a0.
	VERIF(p.Retirer(0x2222) == -1 && p.entrees.size() == 1 && p.Trouver(0x4444) == 0);
	p.entrees[0].pieton = 3;
	VERIF(p.Vider().size() == 1 && p.entrees.empty());
	printf("test_patrouilles : %d échec(s)\n", echecs);
	return echecs ? 1 : 0;
}
