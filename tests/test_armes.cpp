// L'inventaire des armes (src/peds/Armes) : le pool commun de 57 entrées, GiveWeaponToPlayer (FUN_0045ccb0) et ses
// munitions, GiveAmmoToPlayer (FUN_0045cd80), les munitions bornées au maximum du type (FUN_0045c890), les armes aux
// munitions sans fin (FUN_0045cdf0), Changer (FUN_0045cee0) et Retirer (FUN_0045cf70). Des types écrits à la main, comme
// les fiches ObjectButes de Global_Weapons.cat (SLINGSHOT, marble, MarbBag, BRICK). Aucun fichier du jeu.
//   build/tests/test_armes
#include "../src/peds/Armes.h"
#include <cstdio>
#include <map>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

enum { LANCE_PIERRE = 303, BILLE = 304, BRIQUE = 311, SAC_DE_BILLES = 341, PETARD = 350 };

struct CMonde : CMondeArmes {
	std::map<int32, CTypeArme> types;
	CTypeArme defaut;
	const CTypeArme &Type(int32 a) const override { auto it = types.find(a); return it == types.end() ? defaut : it->second; }
};

int
main(void)
{
	CMonde m;
	m.defaut.maximum = 10;
	CTypeArme t;
	t.lu = true; t.maximum = 1; t.inventaire = 1; t.munition = BILLE; m.types[LANCE_PIERRE] = t;   // 52=1, 84='marble', 100=1
	t = CTypeArme(); t.lu = true; t.maximum = 1; m.types[BILLE] = t;                                 // 100=0, genre 0
	t = CTypeArme(); t.lu = true; t.maximum = 1; m.types[BRIQUE] = t;
	t = CTypeArme(); t.lu = true; t.maximum = 20; t.genre = 1; m.types[SAC_DE_BILLES] = t;          // genre 1 : rangé
	t = CTypeArme(); t.lu = true; t.maximum = 5; t.inventaire = 1; m.types[PETARD] = t;
	m.sansFin[0] = BILLE;

	CInventaireArmes inv;
	// GiveWeaponToPlayer(303) : le lance-pierre avec 1 ; ses munitions (« marble », sans +0x64, du genre 0) ne se rangent pas,
	// mais FUN_0045cdf0 en rend toujours 1.
	inv.Donner(m, 0, LANCE_PIERRE);
	VERIF(inv.Trouver(0, LANCE_PIERRE) >= 0 && inv.Munitions(m, 0, LANCE_PIERRE) == 1);
	VERIF(inv.Trouver(0, BILLE) == -1 && inv.Munitions(m, 0, BILLE) == 1);
	// GiveAmmoToPlayer(303, 30) : 1 + 30, borné au maximum de la fiche (1).
	inv.AjouterMunitions(m, 0, LANCE_PIERRE, 30);
	VERIF(inv.Munitions(m, 0, LANCE_PIERRE) == 1);
	// Une seconde fois : sans `force`, pas de seconde entrée.
	inv.Donner(m, 0, LANCE_PIERRE);
	int n = 0;
	for(int i = 0; i < CInventaireArmes::kTaille; i++) if(!inv.libre[i]) n++;
	VERIF(n == 1);
	// Une brique (+0x64 nul, genre 0) n'entre pas ; un sac de billes (genre 1) si, borné à 20.
	VERIF(!inv.Ajouter(m, 0, BRIQUE, 1) && inv.Munitions(m, 0, BRIQUE) == 0);
	inv.AjouterMunitions(m, 0, SAC_DE_BILLES, 25);
	VERIF(inv.Munitions(m, 0, SAC_DE_BILLES) == 20);
	// Les entrées d'un autre piéton sont à lui.
	inv.AjouterMunitions(m, 3, PETARD, 2);
	VERIF(inv.Munitions(m, 3, PETARD) == 2 && inv.Munitions(m, 0, PETARD) == 0);
	// Changer : - 2 vide et libère l'entrée ; une arme absente est ajoutée, même avec un compte négatif (bornée à 0).
	inv.Changer(m, 3, PETARD, -2);
	VERIF(inv.Trouver(3, PETARD) == -1);
	inv.Changer(m, 3, PETARD, -4);
	VERIF(inv.Trouver(3, PETARD) >= 0 && inv.Munitions(m, 3, PETARD) == 0);
	// Retirer : 1000 vide (PedClearWeapon) et libère.
	inv.Retirer(m, 0, SAC_DE_BILLES, 1000);
	VERIF(inv.Trouver(0, SAC_DE_BILLES) == -1);
	// Le pool est commun : 57 entrées au plus, puis plus rien.
	CInventaireArmes plein;
	int prises = 0;
	for(int p = 0; p < 60; p++) if(plein.Ajouter(m, p, PETARD, 1)) prises++;
	VERIF(prises == CInventaireArmes::kTaille);
	plein.Vider(5);
	VERIF(plein.Ajouter(m, 70, PETARD, 1) && plein.Trouver(70, PETARD) == 5);

	// ItemSetCurrentNum sur une arme (FUN_0045d0f0) : posée si présente, libérée sous 1, rien si elle manque.
	VERIF(plein.PoserMunitions(m, 70, PETARD, 3) == 5 && plein.Munitions(m, 70, PETARD) == 3);
	VERIF(plein.PoserMunitions(m, 70, PETARD, 0) == 5 && plein.Trouver(70, PETARD) == -1);
	VERIF(plein.PoserMunitions(m, 70, PETARD, 2) == -1 && plein.Trouver(70, PETARD) == -1);

	// Les objets du joueur (DAT_00cf0060) : le transistor (Radio, 476, un item de maximum 10), le BigGulp jamais compté.
	enum { PREMIER_ITEM = 460, RADIO = 476, BIG_GULP = 470 };
	t = CTypeArme(); t.lu = true; t.maximum = 10; t.genre = 4; m.types[RADIO] = t;
	CObjetsJoueur o;
	o.premier = PREMIER_ITEM; o.bigGulp = BIG_GULP;
	VERIF(o.Case(RADIO) == 16 && o.Case(PREMIER_ITEM) == 0 && o.Case(PREMIER_ITEM + 0x50) == 0 && o.Case(12) == 12 && o.Case(-1) == 0);
	VERIF(o.Nombre(RADIO) == 0);
	VERIF(o.Ajouter(m, RADIO, 1) && o.Nombre(RADIO) == 1);   // FUN_00541ae0(476, 1, 1, 0) : le transistor ramassé
	o.Ajouter(m, RADIO, 30);
	VERIF(o.Nombre(RADIO) == 10);                             // borné au maximum de la fiche
	VERIF(o.Poser(m, RADIO, -4) == 10 && o.Nombre(RADIO) == 0);   // FUN_00541c10 : borné à 0
	VERIF(!o.Ajouter(m, BIG_GULP, 1) && o.Nombre(BIG_GULP) == 0);

	// La charge (FUN_0065e810, FUN_00661f10) : le lance-pierre charge deux fois moins vite ; au tir, bornée à [0, 1], × le facteur.
	VERIF(CChargeArme::Augmenter(0.0f, 0.5f, true) == 0.25f && CChargeArme::Augmenter(0.0f, 0.5f, false) == 0.5f);
	VERIF(CChargeArme::AuTir(CChargeArme::kVide, 1.0f) == 0.0f && CChargeArme::AuTir(0.4f, 2.0f) == 0.8f && CChargeArme::AuTir(0.7f, 2.0f) == 1.0f);
	VERIF(CChargeArme::AuTir(0.3f, 0.0f) == 0.3f && CChargeArme::AuTir(1.5f, -1.0f) == 1.0f);
	printf(echecs ? "test_armes : %d échec(s)\n" : "test_armes : tout passe\n", echecs);
	return echecs != 0;
}
