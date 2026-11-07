// L'inventaire des armes (src/peds/Armes) : le pool commun de 57 entrées, GiveWeaponToPlayer (FUN_0045ccb0) et ses
// munitions, GiveAmmoToPlayer (FUN_0045cd80), les munitions bornées au maximum du type (FUN_0045c890), les armes aux
// munitions sans fin (FUN_0045cdf0), Changer (FUN_0045cee0) et Retirer (FUN_0045cf70). Des types écrits à la main, comme
// les fiches ObjectButes de Global_Weapons.cat (SLINGSHOT, marble, MarbBag, BRICK). Aucun fichier du jeu.
//   build/tests/test_armes
#include "../src/peds/Armes.h"
#include "../src/objects/Projectiles.h"
#include <cmath>
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

	// Les drapeaux d'une entrée (FUN_00661d40, FUN_0065bb20) : posés et lus par l'arme, une arme absente n'est pas trouvée.
	{
		CInventaireArmes inv; CMonde mm; CTypeArme ta; ta.lu = true; ta.maximum = 10; ta.genre = 1; mm.types[400] = ta;
		inv.Ajouter(mm, 3, 400, 1);
		bool t = false;
		inv.PoserDrapeau(3, 400, 1, 1);
		VERIF(inv.LireDrapeau(3, 400, 1, t) && t && !inv.LireDrapeau(3, 400, 2, t) && t && !inv.LireDrapeau(3, 401, 1, t) && !t);
		inv.PoserDrapeau(3, 400, 9, 1);
		VERIF(!inv.LireDrapeau(3, 400, 9, t));
	}
	// La charge (FUN_0065e810, FUN_00661f10) : le lance-pierre charge deux fois moins vite ; au tir, bornée à [0, 1], × le facteur.
	VERIF(CChargeArme::Augmenter(0.0f, 0.5f, true) == 0.25f && CChargeArme::Augmenter(0.0f, 0.5f, false) == 0.5f);
	VERIF(CChargeArme::AuTir(CChargeArme::kVide, 1.0f) == 0.0f && CChargeArme::AuTir(0.4f, 2.0f) == 0.8f && CChargeArme::AuTir(0.7f, 2.0f) == 1.0f);
	VERIF(CChargeArme::AuTir(0.3f, 0.0f) == 0.3f && CChargeArme::AuTir(1.5f, -1.0f) == 1.0f);

	// HasRangedWeapon (FUN_00486ae0, FUN_00486130) : le lance-pierre (+0x68) et ses billes sans fin ; une arme de contact ne compte
	// pas ; une arme de l'inventaire avec l'octet +0x69 non plus ; une arme tenue sans munitions compte pour 1.
	{
		CMonde w;
		CTypeArme a; a.lu = true; a.maximum = 1; a.inventaire = 1; a.munition = BILLE; a.tir68 = true; w.types[LANCE_PIERRE] = a;
		a = CTypeArme(); a.lu = true; a.maximum = 1; w.types[BILLE] = a;
		a = CTypeArme(); a.lu = true; a.maximum = 5; a.inventaire = 1; a.tir68 = true; a.tir69 = true; w.types[BRIQUE] = a;
		a = CTypeArme(); a.lu = true; a.maximum = 5; a.inventaire = 1; a.tir68 = true; w.types[PETARD] = a;
		w.sansFin[0] = BILLE;
		CInventaireArmes v;
		VERIF(!v.ArmeADistance(w, 0, -1) && v.MunitionsPour(w, 0, -1, -1) == 0 && v.MunitionsPour(w, 0, 0, -1) == 0);
		v.Donner(w, 0, LANCE_PIERRE);
		VERIF(v.MunitionsPour(w, 0, LANCE_PIERRE, -1) == 1 && v.ArmeADistance(w, 0, -1) && v.ArmeADistance(w, 0, LANCE_PIERRE));
		VERIF(!v.ArmeADistance(w, 1, -1));                       // l'inventaire d'un autre piéton
		v.Vider(0);
		v.Ajouter(w, 0, BRIQUE, 3);                              // +0x69 : pas dans l'inventaire
		VERIF(!v.ArmeADistance(w, 0, -1) && v.ArmeADistance(w, 0, BRIQUE));   // tenue, elle compte (FUN_004d9560 ne lit que +0x68)
		v.Ajouter(w, 0, PETARD, 0);
		VERIF(v.MunitionsPour(w, 0, PETARD, -1) == 0 && !v.ArmeADistance(w, 0, -1));
		VERIF(v.MunitionsPour(w, 0, PETARD, PETARD) == 1 && v.ArmeADistance(w, 0, PETARD));   // tenue sans munitions : 1
	}

	// Les dégâts du tir (FUN_00664710) : la bille (9 sans charge, la fiche) ; la piste qui impose les siens ; l'échelle du tireur.
	VERIF(CChargeArme::Degats(true, 9.0f, 25.0f, 0.0f, -1.0f, 100) == 9.0f && CChargeArme::Degats(true, 9.0f, 25.0f, 0.5f, -1.0f, 100) == 17.0f);
	VERIF(CChargeArme::Degats(true, 9.0f, 25.0f, 0.5f, 40.0f, 100) == 20.0f && CChargeArme::Degats(true, 9.0f, 25.0f, 1.0f, -1.0f, 50) == 12.5f);
	VERIF(CChargeArme::Degats(false, 9.0f, 25.0f, 1.0f, -1.0f, 100) == 0.0f && CChargeArme::Degats(true, 9.0f, 25.0f, 1.0f, -1.0f, 0) == 0.0f);

	// Les réglages du tir (FUN_0065d3f0, FUN_0065d430, FUN_0065d350, FUN_0065d160) : le lance-pierre pose 0,2 puis 0 en hauteur, le
	// dernier l'emporte ; le décalage vise plus haut, devant et de côté dans le repère du tireur, le départ tourne avec lui.
	{
		CReglagesTir r;
		VERIF(r.munition == -1 && r.decalage[2] == 0.0f);
		r.PoserDecalage(0.0f, 0.0f, 0.2f);
		r.PoserDecalage(0.0f, 0.0f, 0.0f);
		CVector v = r.Viser(CVector(1.0f, 2.0f, 3.0f), 1.8f, CVector(1.0f, 0.0f, 0.0f));
		VERIF(v.x == 1.0f && v.y == 2.0f && v.z == 3.0f);
		r.PoserDecalage(0.5f, 2.0f, 0.5f);      // tireur tourné vers +y : sa droite est +x
		v = r.Viser(CVector(0.0f, 0.0f, 1.0f), 1.8f, CVector(0.0f, 1.0f, 0.0f));
		VERIF(fabsf(v.x - 0.5f) < 1e-6f && fabsf(v.y - 2.0f) < 1e-6f && fabsf(v.z - 1.9f) < 1e-6f);
		r.PoserDepart(1.0f, 2.0f, 0.5f);
		v = r.Partir(CVector(10.0f, 0.0f, 1.0f), CVector(1.0f, 0.0f, 0.0f), CVector(0.0f, 1.0f, 0.0f), CVector(0.0f, 0.0f, 1.0f));
		VERIF(v.x == 11.0f && v.y == 2.0f && v.z == 1.5f);
		r.PoserRotation(180.0f, 90.0f, 0.0f);
		VERIF(fabsf(r.rotation[0] - 3.1415927f) < 1e-6f && fabsf(r.rotation[1] - 1.5707964f) < 1e-6f && r.rotation[2] == 0.0f);
		r.munition = 304;
		r.Remettre();
		VERIF(r.munition == -1 && r.decalage[0] == 0.0f && r.depart[1] == 0.0f && r.rotation[0] == 0.0f);
	}
	// L'avance sur une cible qui bouge (FUN_0065ecb0) : la bille (0,65 m par pas sans charge), le facteur +0x28 à 1.
	{
		auto pres = [](float a, float b){ return fabsf(a - b) < 1e-3f; };
		CVector o(0, 0, 0), c(10, 0, 1);
		CVector r = CViseeTir::AvanceSurCible(o, c, CVector(0, 0, 0), 0.65f, 1.0f, false, 1.0f, 1.0f);
		VERIF(r.x == 10.0f && r.y == 0.0f && r.z == 1.0f);       // immobile : la cible
		r = CViseeTir::AvanceSurCible(o, c, CVector(0, 0.1f, 0), 0.65f, 1.0f, false, 1.0f, 1.0f);
		VERIF(pres(r.x, 10.0f) && pres(r.y, 10.0f / 0.65f * 0.1f) && pres(r.z, 1.0f));   // de côté : la vitesse × d / v
		r = CViseeTir::AvanceSurCible(o, c, CVector(0.1f, 0, 0), 0.65f, 1.0f, false, 1.0f, 1.0f);
		VERIF(pres(r.x, 10.0f + 0.1f * 10.0f / (0.65f - 0.1f)));   // qui s'éloigne : la durée refaite, d / (v − 0,1)
		r = CViseeTir::AvanceSurCible(o, c, CVector(0, 0.1f, 0.05f), 0.65f, 1.0f, false, 1.0f, 0.5f);
		VERIF(pres(r.y, 0.5f * 10.0f / 0.65f * 0.1f) && pres(r.z, 1.0f + 0.5f * 10.0f / 0.65f * 0.05f));   // × +0x84 de la fiche
		r = CViseeTir::AvanceSurCible(o, CVector(100, 0, 0), CVector(0.1f, 0, 0), 0.65f, 1.0f, true, 4.0f, 1.0f);
		// l'octet +0x124 : T = 153,8, T' = 89,4, T = √(64,4² / 4) + T' = 121,6 ; puis k = (100 / (0,65 − 0,1 × 121,6 / 121,6)) / 121,6
		float t = 100.0f / 0.65f, tp = 0.5f * t + 12.5f; t = sqrtf((t - tp) * (t - tp) / 4.0f) + tp;
		VERIF(pres(r.x, 100.0f + 0.1f * t * ((100.0f / (0.65f - 0.1f)) / t)));
		r = CViseeTir::AvanceSurCible(o, CVector(0, 0, 5), CVector(0.1f, 0, 0), 0.65f, 1.0f, false, 1.0f, 1.0f);
		VERIF(r.x == 0.0f && r.z == 5.0f);                        // à la verticale : la cible gardée
	}
	// La table des projectiles (src/objects/Projectiles.h : FUN_004d4590, FUN_004d3f60) : la première place libre ; pleine, la plus
	// vieille cède la sienne (la première à égalité).
	{
		CPlacesProjectiles t;
		int ev = 0;
		VERIF(t.Prendre(100, &ev) == 0 && ev == -1 && t.Prendre(100) == 1 && t.Prendre(150) == 2);
		t.Liberer(1);
		VERIF(t.Prendre(200) == 1 && t.creation[1] == 200);       // la place libérée d'abord
		for(int i = 3; i < 32; i++) VERIF(t.Prendre(300 + i) == i);
		VERIF(t.Prendre(1000, &ev) == 0 && ev == 0 && t.creation[0] == 1000);   // pleine : la 0 (100) cède sa place
		VERIF(t.Prendre(1001, &ev) == 2 && ev == 2);              // puis la 2 (150)
		t.Vider();
		VERIF(t.Prendre(5, &ev) == 0 && ev == -1);
	}
	// Les décalages de visée (FUN_00662760) : nuls, le point visé ; le côté × d, l'avant × v, la hauteur en z ; l'avant de la cible.
	{
		CVector o(0, 0, 0), c(10, 0, 1), av(0, 1, 0);
		CVector r = CViseeTir::DecalageVisee(o, c, av, 0, 0, 0, 0);
		VERIF(r.x == 10.0f && r.y == 0.0f && r.z == 1.0f);
		r = CViseeTir::DecalageVisee(o, c, av, 0.1f, 0, 0, 0);    // le côté de (10, 0) : (0, −1), × 0,1 × 10
		VERIF(fabsf(r.x - 10.0f) < 1e-5f && fabsf(r.y + 1.0f) < 1e-5f && r.z == 1.0f);
		r = CViseeTir::DecalageVisee(o, c, av, 0, 0.5f, 0.3f, 0);   // l'avant : 0,5 × (10, 0, 0) ; la hauteur : 0,3 en z seul
		VERIF(fabsf(r.x - 15.0f) < 1e-5f && r.y == 0.0f && fabsf(r.z - 1.3f) < 1e-5f);
		r = CViseeTir::DecalageVisee(o, c, av, 0, 0, 0, 2.0f);    // +0x24 × l'avant de la cible
		VERIF(r.x == 10.0f && r.y == 2.0f && r.z == 1.0f);
	}
	printf(echecs ? "test_armes : %d échec(s)\n" : "test_armes : tout passe\n", echecs);
	return echecs != 0;
}
