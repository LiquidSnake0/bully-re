// L'inventaire des armes (src/peds/Armes) : le pool commun de 57 entrées, GiveWeaponToPlayer (FUN_0045ccb0) et ses
// munitions, GiveAmmoToPlayer (FUN_0045cd80), les munitions bornées au maximum du type (FUN_0045c890), les armes aux
// munitions sans fin (FUN_0045cdf0), Changer (FUN_0045cee0) et Retirer (FUN_0045cf70). Des types écrits à la main, comme
// les fiches ObjectButes de Global_Weapons.cat (SLINGSHOT, marble, MarbBag, BRICK). Aucun fichier du jeu.
//   build/tests/test_armes
#include "../src/peds/Armes.h"
#include "../src/objects/Projectiles.h"
#include "../src/core/Manette.h"
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
	// Les destinations forcées du tir (PedSetProjDest FUN_0048bd40, FUN_0048bd90, FUN_0048bdb0, FUN_0048c430) : trois places, la
	// première vide reçoit ; on prend la première, les autres avancent et seul le x de la troisième redevient 1e11.
	{
		CDestinationsTir d;
		VERIF(!d.EnAttente());
		VERIF(d.Poser(1, 2, 3) && d.Poser(4, 5, 6) && d.Poser(7, 8, 9) && !d.Poser(10, 11, 12));
		CVector r = d.Prendre();
		VERIF(r.x == 1.0f && r.y == 2.0f && r.z == 3.0f && d.EnAttente());
		VERIF(d.place[2].x == CDestinationsTir::kVide && d.place[2].y == 8.0f && d.place[2].z == 9.0f);
		VERIF(d.Poser(13, 14, 15) && d.place[2].x == 13.0f);   // la troisième, vide de nouveau
		d.ViderPremiere();                                      // FUN_0048c430 : seul le x de la première
		VERIF(!d.EnAttente() && d.place[1].x == 7.0f && d.place[2].x == 13.0f);
		VERIF(d.Poser(16, 17, 18) && d.place[0].x == 16.0f);    // la première vide reçoit, la deuxième garde 7
		r = d.Prendre();
		VERIF(r.x == 16.0f && d.EnAttente() && d.place[0].x == 7.0f);
		d.Vider();
		VERIF(!d.EnAttente() && d.place[0].y == 0.0f);
	}
	// D'où vient la cible du tir (FUN_00662a40) et le repli du composant de visée (FUN_004d7630).
	{
		typedef CViseeTir V;
		VERIF(V::SourceCible(true, 1, true, false, 0, 0, true, true) == V::SOURCE_OBJET_VISE);   // l'octet +0x31 d'abord
		VERIF(V::SourceCible(true, 1, false, false, 0, 0, true, true) == V::SOURCE_AUCUNE);      // le joueur sans rien : devant lui
		VERIF(V::SourceCible(false, 1, false, false, 0, 0, false, true) == V::SOURCE_VISEE);     // un autre piéton cherche toujours
		VERIF(V::SourceCible(true, 1, false, true, 0, 0, false, false) == V::SOURCE_REPLI);      // la visée automatique
		VERIF(V::SourceCible(true, 1, false, false, 0.5f, 0, true, true) == V::SOURCE_54C);      // le bouton 10, +0x54c d'abord
		VERIF(V::SourceCible(true, 1, false, false, 0, 2, false, true) == V::SOURCE_VISEE);      // le mode 2 du composant de visée
		VERIF(V::SourceCible(true, 0xd, false, false, 0, 0, false, true) == V::SOURCE_VISEE);    // la caméra de visée 0xd
		VERIF(V::SourceCible(true, 2, false, false, 0, 0, false, false) == V::SOURCE_REPLI);     // ou 2
		VERIF(!V::CameraDeVisee(false, 2) && V::CameraDeVisee(true, 2) && !V::CameraDeVisee(true, 1));
		int gardee = -1;
		VERIF(V::CibleOuRepli(4, gardee, 5, 6, 7) == 4 && gardee == -1);   // +0xa0 : rien n'est gardé
		VERIF(V::CibleOuRepli(-1, gardee, 5, 6, 7) == 5 && gardee == 5);   // +0x9c, gardée en +0xa8
		VERIF(V::CibleOuRepli(-1, gardee, -1, 6, 7) == 5);                 // la gardée tant qu'elle vit
		gardee = -1;
		VERIF(V::CibleOuRepli(-1, gardee, -1, 6, 7) == 6 && gardee == 6);  // le partenaire de prise (+0x155c)
		gardee = -1;
		VERIF(V::CibleOuRepli(-1, gardee, -1, -1, 7) == 7 && gardee == 7); // FUN_004d6dc0
		gardee = -1;
		VERIF(V::CibleOuRepli(-1, gardee, -1, -1, -1) == -1 && gardee == -1);
	}
	// La manette (FUN_0050c730, FUN_007386f0) : numérique, le bit du bouton dans +0x1e8 ; sinon la valeur lue de la touche ; un bouton
	// hors des valables rend sa valeur brute.
	{
		CManette m;
		m.numerique = true;
		VERIF(m.Valeur(10) == 0.0f);
		m.bits = 0x400;                                            // le bouton 10
		VERIF(m.Valeur(10) == 1.0f && m.Valeur(12) == 0.0f);
		m.bits = 0x2000;                                           // le bouton 8 (le tir chargé du lance-pierre)
		VERIF(m.Valeur(8) == 1.0f && m.Valeur(10) == 0.0f);
		m.valeurs[0x18] = 0.25f;
		VERIF(m.Valeur(0x18) == 0.25f && m.Valeur(-1) == 0.0f);   // numérique : 24 n'est pas valable
		m.numerique = false;
		VERIF(m.Valeur(0x18, 0.5f) == 1.0f && m.Valeur(0x18, 0.0f) == 0.0f && m.Valeur(3, -1.0f) == 0.0f);
		m.valeurs[0x10] = 0.75f;
		VERIF(m.Valeur(0x10, 1.0f) == 0.75f);                      // 16 : hors des valables, la valeur brute
		VERIF(CManette::Index(0, 2, 3) == 2 && CManette::Index(1, 2, 3) == 3 && CManette::Index(5, 2, 3) == 5);
	}
	// Le point devant le tireur sans cible (FUN_0065ea50) : au-delà de 0,9 m, à la hauteur de ses pieds ; le véhicule du joueur.
	{
		CVector d(1, 2, 3), av(0, 1, 0), v(10, 0, 0);
		CVector r = CViseeTir::PointDevant(d, av, 0.5f, 2.0f);
		VERIF(r.x == 1.0f && r.y == 2.5f && r.z == 3.0f);
		r = CViseeTir::PointDevant(d, av, 0.9f, 2.0f);              // 0,9f (0,89999998) ne dépasse pas le double 0,9
		VERIF(r.z == 3.0f);
		r = CViseeTir::PointDevant(d, av, 0.91f, 2.0f);             // au-delà : les pieds
		VERIF(r.z == 2.0f);
		r = CViseeTir::PointDevant(d, av, 20.0f, 2.0f, &v, 0.5f);
		VERIF(r.x == 6.0f && r.y == 22.0f && r.z == 2.0f);
	}
	// La vitesse de départ (FUN_0065f050) : la durée du vol à plat, la parabole, les bornes de la fiche et la dispersion.
	{
		typedef CViseeTir V;
		int n = 0; float T = 0.0f; int tires = 0;
		auto zero = [&]{ tires++; return 0.5f; };
		CVector d(0, 0, 0), c(8, 6, 3);
		CVector v = V::VitesseDepart(d, c, 0.0f, nullptr, false, 0.0f, zero, &n, &T);   // sans fiche : 0,4 m par pas, G = −0,008, un essai
		VERIF(n == 1 && T == 25.0f && tires == 0);                                       // 10 m à plat (le z n'y entre pas) / 0,4
		VERIF(fabsf(v.x - 0.32f) < 1e-6f && fabsf(v.y - 0.24f) < 1e-6f && fabsf(v.z - (3.0f + 0.004f * 625.0f) / 25.0f) < 1e-6f);
		V::FicheVitesse f; f.allureMin = 0.5f; f.allureMax = 1.0f; f.gravite = 0.5f;
		v = V::VitesseDepart(d, c, 1.0f, &f, false, 0.0f, zero, &n, &T);                 // la charge pleine : 1 m par pas, T = 10
		VERIF(n == 1 && T == 10.0f && fabsf(v.x - 0.8f) < 1e-6f && fabsf(v.z - (3.0f + 0.002f * 100.0f) / 10.0f) < 1e-6f && tires == 0);
		f.allureMin = f.allureMax = 0.001f;                                              // sous 0,01 : 0,01
		v = V::VitesseDepart(d, c, 0.0f, &f, false, 0.0f, zero, &n, &T);
		VERIF(n == 1 && fabsf(T - 1000.0f) < 0.01f);
		v = V::VitesseDepart(d, CVector(0, 0, 5), 0.0f, nullptr, false, 0.0f, zero, &n, &T);   // à la verticale : T = 0,01 au moins
		VERIF(T == 0.01f && v.x == 0.0f && v.z > 499.0f);
		// l'angle d'arrivée : à 1 m par pas sur 10 m, w.z = 0,32 − 0,04 = 0,28 (le tir monte encore) ; avec 10° demandés, T s'allonge
		// de (T + 1) × 1,06 jusqu'à ce que la plongée y soit
		f = V::FicheVitesse(); f.allureMin = f.allureMax = 1.0f; f.gravite = 0.5f; f.angle = 10.0f;
		v = V::VitesseDepart(d, c, 0.0f, &f, false, 0.0f, zero, &n, &T);
		VERIF(n > 1 && n < 20 && T > 10.0f);
		{
			float wz = v.z + T * -0.004f, plat = sqrtf(v.x * v.x + v.y * v.y);
			VERIF(atanf(-wz / plat) * 180.0f / 3.1415927f >= 10.0f);
			float T0 = 10.0f; for(int k = 1; k < n; k++){ T0 = (float)((double)T0 + 1.0); T0 = (float)((double)T0 * 1.059999942779541); }
			VERIF(T == T0);
		}
		f.angle = 89.99f; f.arriveeMin = 5.0f;                                           // jamais bon : le vingtième essai est gardé
		v = V::VitesseDepart(d, c, 0.0f, &f, false, 0.0f, zero, &n, &T);
		VERIF(n == 20);
		f = V::FicheVitesse(); f.allureMin = f.allureMax = 1.0f; f.monteeMax = 0.3f;     // v.z = 0,3 + 0,02 = 0,32 dépasse +0x6c
		v = V::VitesseDepart(d, c, 0.0f, &f, false, 0.0f, zero, &n, &T);
		VERIF(n > 1 && v.z <= 0.3f);
		f = V::FicheVitesse(); f.allureMin = f.allureMax = 1.0f; f.monteeMin = 0.1f;     // v.z sous +0x68 : le vol s'allonge
		v = V::VitesseDepart(d, CVector(8, 6, 0), 0.0f, &f, false, 0.0f, zero, &n, &T);   // à plat : v.z = 0,004 T, 0,04 à T = 10
		VERIF(n == 9 && v.z >= 0.1f && T > 25.0f && tires == 0);
		// la dispersion : trois tirages, le premier en z, le troisième en x ; le joueur × (1 − la précision)
		f = V::FicheVitesse(); f.allureMin = f.allureMax = 1.0f; f.gravite = 0.0f; f.dispersion = 0.1f;
		float suite[3] = {1.0f, -1.0f, 0.5f}; int k = 0;
		auto tir = [&]{ return suite[k++]; };
		v = V::VitesseDepart(d, c, 0.0f, &f, false, 0.0f, tir, &n, &T);
		VERIF(k == 3 && fabsf(v.x - 0.85f) < 1e-6f && fabsf(v.y - 0.5f) < 1e-6f && fabsf(v.z - 0.4f) < 1e-6f);
		k = 0;
		v = V::VitesseDepart(d, c, 0.0f, &f, true, 0.75f, tir, &n, &T);
		VERIF(k == 3 && fabsf(v.x - 0.8125f) < 1e-6f && fabsf(v.y - 0.575f) < 1e-6f && fabsf(v.z - 0.325f) < 1e-6f);
	}
	// L'imprécision (FUN_0065eb40) et la précision du verrou (FUN_004d5cd0).
	{
		typedef CViseeTir V;
		VERIF(V::Tirage(0) == -1.0f && V::Tirage(32767) == 1.0f);
		CVector t(0, 0, 0), q(30, 40, 9);                          // d = 50 à plat (le z ne compte pas)
		CVector e = V::Imprecision(t, q, 95, true, 0.5f, false, 0.0f, 1.0f, -1.0f, 0.5f);   // s = 50 / 20 × 5 / 50 = 0,25
		VERIF(fabsf(e.x - 0.125f) < 1e-6f && fabsf(e.y + 0.25f) < 1e-6f && fabsf(e.z - 0.25f) < 1e-6f);
		e = V::Imprecision(t, q, 95, false, 0.5f, true, 0.5f, 1.0f, 0.0f, 0.0f);            // hors de l'écran × 0,5, joueur × 0,5
		VERIF(fabsf(e.z - 0.0625f) < 1e-6f && e.x == 0.0f && e.y == 0.0f);
		e = V::Imprecision(t, q, 100, true, 1.0f, false, 0.0f, 1.0f, 1.0f, 1.0f);            // la statistique à 100 : aucun écart
		VERIF(e.x == 0.0f && e.y == 0.0f && e.z == 0.0f);
		VERIF(V::PrecisionVerrou(500.0f, 2.0f, false, 0.0f) == 0.25f);    // 500 ms sur 2 s
		VERIF(V::PrecisionVerrou(5000.0f, 2.0f, false, 0.0f) == 1.0f);    // borné au plein
		VERIF(V::PrecisionVerrou(500.0f, 2.0f, true, 0.5f) == 0.75f);     // le +0x1ec4 du joueur
		VERIF(V::PrecisionVerrou(1500.0f, 2.0f, true, 0.5f) == 1.0f);     // 1 au plus
	}
	// La vie d'un projectile à sa création (FUN_004d4c40) et ce que le tir lui passe (FUN_00664710).
	{
		typedef CPlacesProjectiles P;
		VERIF(P::Vie(0, 3000, 0) == 3000 && P::Vie(1500, 3000, 0) == 1500);   // la vie du tireur d'abord
		VERIF(P::Vie(0, 0, 500) == 0);                                        // sans vie : sans fin, la mèche n'y fait rien
		VERIF(P::Vie(0, 3000, 1000) == 2000 && P::Vie(0, 3000, 3000) == 1);   // la mèche ; une vie nulle devient 1
		VERIF(P::Vie(0, 3000, 3500) == -500);                                 // la mèche plus longue : négative
		VERIF(P::EcouleMeche(5000, 0) == 0 && P::EcouleMeche(5000, 4200) == 800);
		VERIF(P::VieDuTireur(0.0f) == 0 && P::VieDuTireur(2.5f) == 2500 && P::VieDuTireur(0.0019f) == 1);   // tronquée
	}
	// Le test d'eau (FUN_006d48d0) et le fondu de la fin de vie (FUN_004d45e0).
	{
		typedef CPlacesProjectiles P;
		float n = 5.0f; int32 g = 5;
		VERIF(!P::DansLaMer(0.5f, true, &n, &g) && n == -1000000.0f && g == -1);
		VERIF(P::DansLaMer(0.0f, true, &n, &g) && n == 0.0f && g == 1);    // 0 n'est pas au-dessus
		VERIF(P::DansLaMer(-3.0f, true) && !P::DansLaMer(-3.0f, false));   // dedans, pas de mer
		VERIF(P::Alpha(1000, 3000, 2000, 0) == 255 && P::Alpha(1000, 0, 2000, 500) == 255);
		VERIF(P::Alpha(1000, 3000, 2000, 500) == 255);                     // reste 2000
		VERIF(P::Alpha(1000, 3000, 3750, 500) == 127);                     // reste 250 : 250 × 255 / 500
		VERIF(P::Alpha(1000, 3000, 4100, 500) == 0);                       // passé : 0 au moins
	}
	// Le choc d'un projectile (FUN_004d3860) et son explosion (FUN_004d37d0).
	{
		typedef CChocProjectile C;
		VERIF(!C::Explose(false, false, false, 1.0f, 3, true));                 // aucun des trois octets
		VERIF(C::Explose(true, false, false, -1.0f, 1, false));                 // +0x8c : partout
		VERIF(C::Explose(false, true, false, 0.5f, 1, false) && !C::Explose(false, true, false, 0.49f, 1, false));   // +0x8d : au sol
		VERIF(!C::Explose(false, true, false, 1.0f, 3, false));                 // +0x8d : pas sur un piéton
		VERIF(C::Explose(false, false, true, 0.0f, 3, false) && C::Explose(false, false, true, 0.0f, 2, true));      // +0x8e
		VERIF(!C::Explose(false, false, true, 1.0f, 4, false) && !C::Explose(false, true, true, 1.0f, 1, false));    // +0x8e efface +0x8d
		VERIF(C::StimulusDeLObjet(true, 9.0f, 4, 0, false) && C::StimulusDeLObjet(true, 9.0f, 1, 0xf, false));
		VERIF(!C::StimulusDeLObjet(true, 0.0f, 4, 0, false) && !C::StimulusDeLObjet(false, 9.0f, 4, 0, false));
		VERIF(!C::StimulusDeLObjet(true, 9.0f, 3, 0, false) && !C::StimulusDeLObjet(true, 9.0f, 4, 0, true));
		VERIF(C::Compter(0) == 1 && C::Compter(4) == 5 && C::Compter(5) == 5 && C::Compter(0xf8) == 0xf9 && C::Compter(0xfd) == 0xfd);
		bool c = true;
		VERIF(C::Effet("Slingshot_Hit", false, false, true, 4, 0, &c) == nil && !c);
		VERIF(C::Effet("Slingshot_Hit", true, false, false, 0, 0, &c) != nil && c);
		VERIF(C::Effet(nil, true, false, true, 4, 8, &c) == nil && !c);          // sans nom, pas la balle de baseball
		VERIF(std::string(C::Effet(nil, false, true, true, 4, 8, &c)) == "BsBallMiss" && c);
		VERIF(std::string(C::Effet(nil, false, true, true, 4, 0xd, &c)) == "DunkBallMiss" && c);
		VERIF(C::Effet(nil, true, true, true, 4, 3, &c) == nil && c);            // le compteur monte, pas d'effet
		VERIF(C::Effet(nil, true, true, true, 6, 8, &c) == nil && !c && C::Effet(nil, true, true, false, 0, 8, &c) == nil && !c);
		VERIF(C::PremierSol(false, 0.51f) && !C::PremierSol(false, 0.5f) && !C::PremierSol(true, 1.0f));
		CPlacesProjectiles t;
		int i = t.Prendre(1000);
		VERIF(i == 0 && t.Exploser(i) && !t.Exploser(i) && t.explose[i]);        // une seule fois
		t.auSol[i] = true; t.Liberer(i);
		VERIF(t.Prendre(2000) == 0 && !t.explose[0] && !t.auSol[0] && t.Exploser(0) && !t.Exploser(-1) && !t.Exploser(32));
		// Le contact d'un projectile en vol (FUN_0046ad90).
		typedef CContactProjectile K;
		VERIF(K::Coup(3, 0, false) && K::Coup(2, 0, false) && !K::Coup(3, 1, false) && K::Coup(3, 5, true) && !K::Coup(4, 0, false));
		VERIF(K::TireurEpargne(true, 1199, 1000) && !K::TireurEpargne(true, 1200, 1000) && !K::TireurEpargne(false, 1000, 1000));
		VERIF(K::Fort(3, 0, false, 0.5f) && !K::Fort(3, 1, false, 0.5f) && K::Fort(3, 1, true, 0.5f));   // le coup sauté
		VERIF(K::Fort(4, 3, false, 0.03f) && !K::Fort(4, 0, false, 0.029f) && !K::Fort(3, 0, false, 0.01f));
		VERIF(K::AuRepos(0.5e-4f, 0.81f, false) && !K::AuRepos(1e-4f, 0.81f, false) && !K::AuRepos(0.5e-4f, 0.8f, false));
		VERIF(!K::AuRepos(0.5e-4f, 0.9f, true) && K::AuRepos(0.5e-10f, 0.9f, true));
		// Les explosions (FUN_004d2b40, FUN_004d2aa0, FUN_004d2690).
		typedef CPlacesExplosions X;
		X x; int ev = 0; bool vite = false;
		VERIF(x.Prendre(1000, 0.001f, 0, 0, 0.0f, false, &ev, &vite) == 0 && vite && ev == -1 && x.depart[0] == 0 && x.fin[0] == 1000);
		VERIF(x.Avancer(0, 1000, 1.0f) == X::kFin && !x.actif[0] && x.Avancer(0, 1001, 1.0f) == X::kRien);   // une bille : finie à l'image suivante
		VERIF(x.Prendre(2000, 3.0f, 500, 7000, 0.5f, true, &ev, &vite) == 0 && !vite && x.depart[0] == 2500 && x.fin[0] == 9500);
		VERIF(x.Avancer(0, 2500, 1.0f) == X::kRien && x.Avancer(0, 2501, 1.0f) == X::kDepart && x.depart[0] == 0 && x.rayon[0] == 3.0f);
		VERIF(x.Avancer(0, 2534, 2.0f) == X::kDure && x.rayon[0] == 4.0f);
		x.Toucher(0, 3000, false, 100);
		VERIF(x.fin[0] == 9500);
		x.Toucher(0, 3000, true, 100);
		VERIF(x.fin[0] == 3100 && x.actif[0] && x.Avancer(0, 3099, 0.0f) == X::kDure);
		x.Toucher(0, 3099, true, 9000);
		VERIF(x.fin[0] == 3100 && x.Avancer(0, 3100, 0.0f) == X::kFin);
		VERIF(x.Prendre(4000, 1.0f, 0, 5000, 0.0f, false) == 0);
		x.Toucher(0, 4100, true, 0);
		VERIF(!x.actif[0]);
		X y;   // la table pleine : la plus vieille des évinçables, ou rien
		for(int k = 0; k < 15; k++) VERIF(y.Prendre(100 + k, 1.0f, 0, 9000, 0.0f, false) == k);
		VERIF(y.Prendre(500, 1.0f, 0, 9000, 0.0f, true, &ev) == -1);
		y.evincable[7] = true; y.evincable[3] = true; y.heure[3] = y.heure[7];
		VERIF(y.Prendre(500, 2.0f, 0, 9000, 0.0f, false, &ev) == 3 && ev == 3 && y.rayon[3] == 2.0f && !y.evincable[3]);
		VERIF(y.Prendre(501, 2.0f, 0, 9000, 0.0f, false, &ev) == 7 && ev == 7 && y.Prendre(502, 2.0f, 0, 9000, 0.0f, false) == -1);
		VERIF(X::AutreFiche(true, 0.95f) && X::AutreFiche(true, -0.5f) && !X::AutreFiche(true, 0.97f) && !X::AutreFiche(false, 0.0f));
		// Le vol (FUN_004494d0, FUN_004d1510, FUN_00469760) : la gravité d'un facteur 1 est 0,008 par pas ; un pas de 1,65.
		typedef CVolProjectile V;
		VERIF(V::Gravite(1.0f) == 0.008f && V::Gravite(0.0f) == 0.0f && fabsf(V::Gravite(2.5f) - 0.02f) < 1e-7f);
		VERIF(V::Peser(0.5f, false, 0.008f, 1.65f) == 0.5f && fabsf(V::Peser(0.5f, true, 0.008f, 1.65f) - (0.5f - 0.0132f)) < 1e-6f);
		VERIF(fabsf(V::Peser(0.5f, true, 0.008f, 1.65f, 0.5f) - (0.5f - 0.0066f)) < 1e-6f);
		float vp[3] = {10.0f, 20.0f, 30.0f}, vv[3] = {1.0f, -2.0f, 0.5f};
		VERIF(!V::Deplacer(vp, vv, true, true, 0.008f, 1.65f) && vv[0] == 0.0f && vv[1] == 0.0f && vv[2] == 0.0f && vp[0] == 10.0f && vp[2] == 30.0f);
		vv[0] = 1.0f; vv[1] = -2.0f; vv[2] = 0.5f;
		VERIF(V::Deplacer(vp, vv, false, false, 0.008f, 1.65f) && fabsf(vp[0] - 11.65f) < 1e-5f && fabsf(vp[1] - 16.7f) < 1e-5f && fabsf(vp[2] - 30.825f) < 1e-5f && vv[2] == 0.5f);
		// Pesant : la vitesse perd 0,0132 avant de déplacer, et la position perd encore ½ × 0,008 × 1,65² = 0,01089.
		vp[0] = 0.0f; vp[1] = 0.0f; vp[2] = 0.0f;
		VERIF(V::Deplacer(vp, vv, false, true, 0.008f, 1.65f) && fabsf(vv[2] - 0.4868f) < 1e-6f && fabsf(vp[2] - (0.4868f * 1.65f - 0.01089f)) < 1e-5f);
		// L'air (FUN_004698c0) : au-dessus de 0,1, le facteur est la résistance puissance le pas, sur les deux vitesses.
		float av[3] = {1.0f, -2.0f, 4.0f}, aw[3] = {0.5f, 0.0f, -1.0f};
		V::Air(av, aw, 0.5f, 2.0f);
		VERIF(av[0] == 0.25f && av[1] == -0.5f && av[2] == 1.0f && aw[0] == 0.125f && aw[1] == 0.0f && aw[2] == -0.25f);
		av[0] = 1.0f; av[1] = 0.0f; av[2] = 0.0f;
		V::Air(av, aw, V::kAir, 1.65f);
		VERIF(av[0] < 1.0f && fabsf(av[0] - 0.9999835f) < 1e-6f);
		// Sous 0,1 : la traînée 1 / |1 + r × 0,5 × |v|²| puissance le pas, et 0,99 sur la vitesse angulaire.
		av[0] = 2.0f; av[1] = 0.0f; av[2] = 0.0f; aw[0] = 1.0f; aw[1] = 2.0f; aw[2] = -1.0f;
		V::Air(av, aw, 0.05f, 1.0f);
		VERIF(fabsf(av[0] - 2.0f / 1.1f) < 1e-6f && fabsf(aw[0] - 0.99f) < 1e-7f && fabsf(aw[1] - 1.98f) < 1e-6f && fabsf(aw[2] + 0.99f) < 1e-7f);
		av[0] = 0.0f; av[1] = 3.0f; av[2] = 4.0f;
		V::Air(av, aw, 0.08f, 2.0f);
		VERIF(fabsf(av[1] - 3.0f / 4.0f) < 1e-6f && fabsf(av[2] - 1.0f) < 1e-6f);
		// Le repos (FUN_00469a20) : le seuil est 0,003 × le pas (0,00495 pour 1,65), sur la moyenne glissante des vitesses.
		float rv[3] = {1.0f, 0.0f, 0.0f}, rw[3] = {0.0f, 0.0f, 0.0f}, rmv[3] = {0.0f, 0.0f, 0.0f}, rmw[3] = {0.0f, 0.0f, 0.0f};
		unsigned char rc = 7;
		// En vol : la moyenne prend la moitié de la vitesse, le compteur retombe à 0, l'air passe.
		VERIF(!V::Controler(rv, rw, rmv, rmw, &rc, true, false, 0.5f, 1.0f) && rc == 0 && rmv[0] == 0.5f && rv[0] == 0.5f);
		// Sans mesure (un +0x34, ou un autre genre) : ni moyenne ni compteur, l'air seul.
		rc = 7;
		VERIF(!V::Controler(rv, rw, rmv, rmw, &rc, false, false, 0.5f, 1.0f) && rc == 7 && rmv[0] == 0.5f && rv[0] == 0.25f);
		// Immobile : la moyenne se divise par deux à chaque image (0,5 → sous 0,00495 à la septième), puis le compteur monte ;
		// à onze, les vitesses tombent à 0 et la fonction rend vrai, le compteur gardé à 10.
		rv[0] = 0.0f; rw[1] = 0.001f; rc = 0;
		int images = 0, sousLeSeuil = 0;
		bool pose = false;
		while(!pose && images < 100){ pose = V::Controler(rv, rw, rmv, rmw, &rc, true, false, V::kAir, 1.65f); images++; if(!pose && rc == 1 && sousLeSeuil == 0) sousLeSeuil = images; }
		VERIF(pose && sousLeSeuil == 7 && images == 17 && rc == 10 && rw[1] == 0.0f && rv[0] == 0.0f);
		// Tenu (+0xb0) : le compteur ne monte pas. Une vitesse angulaire au-dessus du seuil le remet à 0. Le seuil juste aussi.
		rc = 3; rmv[0] = 0.0f; rmw[1] = 0.0f; rw[1] = 0.0f;
		VERIF(!V::Controler(rv, rw, rmv, rmw, &rc, true, true, V::kAir, 1.65f) && rc == 3);
		rw[2] = 1.0f;
		VERIF(!V::Controler(rv, rw, rmv, rmw, &rc, true, false, V::kAir, 1.65f) && rc == 0);
		rw[2] = 0.0f; rmw[2] = 0.0f; rc = 3; rv[0] = 0.006f; rmv[0] = 0.0f;
		VERIF(!V::Controler(rv, rw, rmv, rmw, &rc, true, false, V::kAir, 1.0f) && rc == 0 && rmv[0] == 0.003f);
		// Le rebond (FUN_0046ca40) : le recul au contact, puis la vitesse renversée le long de la normale.
		VERIF(fabsf(V::Recul(0.5f) - 0.495f) < 1e-7f && V::Recul(0.0f) < 0.0f);
		// Le début du déplacement : l'entité immobile, le partage de la chute ; le balayage d'un secteur (FUN_0046c970) et des
		// secteurs ; la sortie : la pose après la chute et la distance parcourue.
		{
			float nul[3] = {0, 0, 0}, vz[3] = {0, 0, -0.01f};
			VERIF(V::Immobile(nul, nul, 0x08) && !V::Immobile(nul, nul, 0x03) && !V::Immobile(vz, nul, 0x08) && !V::Immobile(nul, vz, 0xf8));
			float z = 4.0f;
			VERIF(V::PartagerChute(4.0f, &z, 0) == 0.0f && z == 4.0f);                    // à plat : pas de partage
			z = 4.5f; VERIF(V::PartagerChute(4.0f, &z, 0) == 0.0f && z == 4.5f);          // en montée non plus
			z = 3.0f; VERIF(V::PartagerChute(4.0f, &z, 1) == 0.0f && z == 3.0f);          // ni pour un projectile (+0xc4)
			float chute = V::PartagerChute(4.0f, &z, 0);
			VERIF(fabsf(chute + 1.05f) < 1e-6f && fabsf(z - 4.05f) < 1e-6f);
			VERIF(V::Recul(1.0f) == (float)(1.0 - (double)0.005f));
			V::ContactBalaye m;
			VERIF(m.fraction == 3.4028234663852886e+38f && sizeof(float) * 3 == sizeof(m.point));
			float fr[4] = {0.8f, 0.3f, 0.5f, 0.1f}; int vus = 0;
			auto liste = [&](int i, V::ContactBalaye *c){ vus |= 1 << i; if(i == 0) return false; c->fraction = fr[i]; c->point[0] = (float)i; return true; };
			VERIF(V::BalayerSecteur(&m, false, liste) && vus == 7 && m.fraction == 0.3f && m.point[0] == 1.0f);   // sans la quatrième liste
			vus = 0;
			VERIF(V::BalayerSecteur(&m, true, liste) && vus == 15 && m.fraction == 0.1f && m.point[0] == 3.0f);   // avec elle
			VERIF(!V::BalayerSecteur(&m, true, liste) && m.fraction == 0.1f);            // l'égalité ne remplace pas
			V::ContactBalaye m2; float fs[3] = {0.6f, 0.2f, 0.2f};
			auto sect = [&](int i, V::ContactBalaye *c){ if(i == 0) return false; if(fs[i] < c->fraction){ c->fraction = fs[i]; c->point[1] = (float)i; return true; } return false; };
			VERIF(V::BalayerSecteurs(3, &m2, sect) && m2.fraction == 0.2f && m2.point[1] == 1.0f);
			V::ContactBalaye m3;
			VERIF(!V::BalayerSecteurs(1, &m3, sect) && m3.fraction == 3.4028234663852886e+38f);
			{
				V::ContactBalaye a, b; int e1 = 1, e2 = 2; std::string liens;
				auto del = [&](const void *e){ liens += e == &e1 ? "-1" : "-2"; };
				auto lie = [&](const void *e){ liens += e == &e1 ? "+1" : "+2"; };
				b.entite = &e1; b.point[2] = 3.0f; b.normale[1] = -1.0f; b.o1c = 7; b.o20 = 8; b.o22 = 9; b.fraction = 0.25f;
				VERIF(&a.Copier(b, del, lie) == &a && liens == "+1" && a.entite == &e1 && a.point[2] == 3.0f && a.normale[1] == -1.0f);
				VERIF(a.o1c == 7 && a.o20 == 8 && a.o22 == 9 && a.fraction == 0.25f);
				a.Copier(b, del, lie); VERIF(liens == "+1");                               // la même référence : ni déliée ni liée
				b.entite = &e2; a.Copier(b, del, lie); VERIF(liens == "+1-1+2");           // l'ancienne déliée, puis la nouvelle liée
				b.entite = nullptr; a.Copier(b, del, lie); VERIF(liens == "+1-1+2-2" && a.entite == nullptr);
			}
			{
				// la sphère englobante d'une entité contre celle d'un balayage (FUN_00465bc0)
				float c1[3] = {0.0f, 0.0f, 0.0f}, c2[3] = {3.0f, 4.0f, 0.0f}, v0[3] = {0.0f, 0.0f, 0.0f}, v1[3] = {-0.25f, 0.5f, -0.25f};
				VERIF(!V::SpheresSeTouchent(c1, 2.0f, false, v0, c2, 3.0f));                // 5 m d'écart, 5 m de rayons : l'égalité rend faux
				VERIF(V::SpheresSeTouchent(c1, 2.0f, false, v0, c2, 3.01f));
				VERIF(!V::SpheresSeTouchent(c1, 2.0f, false, v1, c2, 3.0f));                // la vitesse d'une entité immobile ne compte pas
				VERIF(V::SpheresSeTouchent(c1, 2.0f, true, v1, c2, 3.0f));                  // mobile : le rayon grandit de 1 (0,5 + 0,25 + 0,25)
				VERIF(!V::SpheresSeTouchent(c1, 1.0f, true, v1, c2, 3.0f));                 // 1 + 3 + 1 = 5 : l'égalité encore
				float c3[3] = {0.0f, 0.0f, 12.0f};
				VERIF(V::SpheresSeTouchent(c3, 6.0f, false, v0, c2, 7.01f) && !V::SpheresSeTouchent(c3, 6.0f, false, v0, c2, 6.99f));   // 13 m, avec le z
			}
			bool remis = false;
			VERIF(V::CodeDeBalayage(7, &remis) == 8 && !remis && V::CodeDeBalayage(0xfffe) == 0xffff && V::CodeDeBalayage(0xffff, &remis) == 1 && remis);
			float avantChute[3] = {1.0f, 2.0f, 4.05f}, pos[3] = {1.0f, 2.0f, 3.0f};
			VERIF(!V::PoserApresChute(avantChute, pos, false, 0.5f) && pos[2] == 3.0f);
			VERIF(V::PoserApresChute(avantChute, pos, true, 0.5f) && fabsf(pos[2] - (4.05f - 1.05f * 0.495f)) < 1e-6f && pos[0] == 1.0f);
			float debut[3] = {0, 0, 0}, fin[3] = {2.0f, 3.0f, 6.0f};
			VERIF(V::Distance(debut, fin) == 7.0f && V::Distance(fin, fin) == 0.0f);
		}
		// Sur un sol (normale en z), élasticité 0,5 : la chute de 2 devient une montée de 1, le glissement de 4 tombe à 2.
		float bv[3] = {4.0f, 0.0f, -2.0f}, bn[3] = {0.0f, 0.0f, 3.0f};
		V::Rebondir(bv, bn, false, false, 0.5f);
		VERIF(bv[0] == 2.0f && bv[1] == 0.0f && bv[2] == 1.0f);
		// Contre un piéton : 0,75 et 0,75, quelle que soit l'élasticité. Un mur en x.
		bv[0] = -4.0f; bv[1] = 2.0f; bv[2] = 0.0f; bn[0] = 1.0f; bn[1] = 0.0f; bn[2] = 0.0f;
		V::Rebondir(bv, bn, true, false, 0.1f);
		VERIF(bv[0] == 3.0f && bv[1] == 1.5f && bv[2] == 0.0f);
		// L'entité à plat (+0xf0) : le z positif de la normale est effacé avant de normaliser ; un z négatif reste.
		bv[0] = -1.0f; bv[1] = 0.0f; bv[2] = -1.0f; bn[0] = 3.0f; bn[1] = 0.0f; bn[2] = 4.0f;
		V::Rebondir(bv, bn, false, true, 1.0f);
		VERIF(bv[0] == 1.0f && bv[1] == 0.0f && bv[2] == -1.0f);
		bv[0] = 0.0f; bv[1] = 0.0f; bv[2] = 2.0f; bn[0] = 0.0f; bn[1] = 0.0f; bn[2] = -1.0f;
		V::Rebondir(bv, bn, false, true, 1.0f);
		VERIF(bv[0] == 0.0f && bv[2] == -2.0f);
		// Une élasticité de 1 garde la norme ; de 0,75 (le constructeur), une vitesse de face revient aux trois quarts.
		bv[0] = 0.0f; bv[1] = -0.4f; bv[2] = 0.0f; bn[0] = 0.0f; bn[1] = 1.0f; bn[2] = 0.0f;
		V::Rebondir(bv, bn, false, false, V::kElasticite);
		VERIF(fabsf(bv[1] - 0.3f) < 1e-7f);
		// La paire du contact (FUN_0046b720) : pas de contact entre un projectile bas et lent et un piéton.
		typedef CContactProjectile K;
		float lent[3] = {0.05f, 0.0f, 0.05f}, vif[3] = {0.2f, 0.0f, 0.0f}, juste[3] = {0.0f, 0.099f, 0.0f};
		VERIF(K::SansContact(true, 0, false, 10.5f, 10.0f, lent) && !K::SansContact(false, 0, false, 10.5f, 10.0f, lent));
		VERIF(!K::SansContact(true, 0, false, 10.5f, 10.0f, vif) && K::SansContact(true, 0, false, 10.5f, 10.0f, juste));
		// La hauteur : 0,5 + 0,1847944 au-dessus du piéton, strictement.
		VERIF(K::SansContact(true, 0, false, 10.68f, 10.0f, lent) && !K::SansContact(true, 0, false, 10.69f, 10.0f, lent) && K::SansContact(true, 0, false, 5.0f, 10.0f, lent));
		// Le tireur dans le jeu 0xd n'est jamais touché par son projectile, à toute vitesse et à toute hauteur.
		VERIF(K::SansContact(true, 0xd, true, 12.0f, 10.0f, vif) && !K::SansContact(true, 0xd, false, 12.0f, 10.0f, vif) && !K::SansContact(true, 8, true, 12.0f, 10.0f, vif));
		// Le contrôle d'un projectile (FUN_004d2290) : tenu, l'autre vol, la poussée, la chute refaite, sinon les déclencheurs.
		VERIF(V::Controle(true, true, 5, true) == V::CONTROLE_AUCUN && V::Controle(false, true, 5, true) == V::CONTROLE_AUTRE_VOL);
		VERIF(V::Controle(false, false, 5, true) == V::CONTROLE_POUSSEE && V::Controle(false, false, 0, true) == V::CONTROLE_CHUTE && V::Controle(false, false, 0, false) == V::CONTROLE_DECLENCHEURS);
		// Près du joueur : 2 m autour d'un point 1,5 m au-dessus de sa position, le bord compris.
		float pj[3] = {10.0f, 20.0f, 5.0f}, pq1[3] = {10.0f, 20.0f, 8.5f}, pq2[3] = {10.0f, 20.0f, 8.6f}, pq3[3] = {12.0f, 20.0f, 6.5f}, pq4[3] = {11.5f, 21.5f, 6.5f};
		VERIF(V::PresDuJoueur(pj, pq1) && !V::PresDuJoueur(pj, pq2) && V::PresDuJoueur(pj, pq3) && !V::PresDuJoueur(pj, pq4) && V::PresDuJoueur(pj, pj));
		// L'échelle de la chute (FUN_004d2090) : le +0x128 de la fiche sous -0,1 m par pas, strictement ; 1 sinon.
		VERIF(V::EchelleDeChute(-0.2f, 0.5f) == 0.5f && V::EchelleDeChute(-0.1f, 0.5f) == 1.0f && V::EchelleDeChute(0.3f, 0.5f) == 1.0f && V::EchelleDeChute(-0.1000001f, 0.5f) == 0.5f);
		// La poussée (FUN_004d1ec0) : pleine jusqu'au délai, puis éteinte en ligne droite jusqu'à la fin de la durée.
		float pv[3] = {1.0f, 0.0f, 2.0f};
		VERIF(V::Pousser(pv, 1000, 1000, 2000, 0.5f, 0.25f, 0.5f, 2.0f) && pv[0] == 1.5f && pv[1] == 0.0f && pv[2] == 4.0f);
		pv[0] = 1.0f; pv[1] = 0.0f; pv[2] = 2.0f;
		VERIF(V::Pousser(pv, 2000, 1000, 2000, 0.5f, 0.25f, 0.5f, 2.0f) && pv[0] == 1.5f && pv[2] == 4.0f);   // au délai compris, f = 0
		pv[0] = 1.0f; pv[1] = 0.0f; pv[2] = 2.0f;
		VERIF(V::Pousser(pv, 2500, 1000, 2000, 0.5f, 0.25f, 0.5f, 2.0f) && pv[0] == 1.25f && pv[2] == 3.0f);  // f = 0,5
		pv[0] = 1.0f; pv[1] = 0.0f; pv[2] = 2.0f;
		VERIF(!V::Pousser(pv, 3000, 1000, 2000, 0.5f, 0.25f, 0.5f, 2.0f) && !V::Pousser(pv, 4000, 1000, 2000, 0.5f, 0.25f, 0.5f, 2.0f) && pv[0] == 1.0f && pv[2] == 2.0f);
		VERIF(V::Pousser(pv, 2999, 1000, 2000, 0.5f, 0.25f, 0.5f, 2.0f) && pv[0] > 1.0f && pv[0] < 1.001f);
		// Le freinage d'un objet (FUN_004cf350) : au-dessus du seuil haut un facteur, dessous l'autre, et l'arrêt sous le seuil bas.
		float fv[3] = {3.0f, 0.0f, 4.0f}, fw[3] = {1.0f, 2.0f, 0.0f};
		VERIF(V::Norme(fv) == 5.0f);
		V::Freiner(fv, fw, 1.0f, 5.0f, 0.5f, 0.25f);       // la norme atteint le seuil haut : le facteur des vitesses hautes
		VERIF(fv[0] == 1.5f && fv[2] == 2.0f && fw[0] == 0.5f && fw[1] == 1.0f);
		V::Freiner(fv, fw, 1.0f, 5.0f, 0.5f, 0.25f);       // 2,5 : sous le seuil haut, au-dessus du seuil bas
		VERIF(fv[0] == 0.375f && fv[2] == 0.5f && fw[0] == 0.125f && fw[1] == 0.25f);
		V::Freiner(fv, fw, 1.0f, 5.0f, 0.5f, 0.25f);       // 0,625 : sous le seuil bas, tout s'arrête
		VERIF(fv[0] == 0.0f && fv[1] == 0.0f && fv[2] == 0.0f && fw[0] == 0.0f && fw[1] == 0.0f && fw[2] == 0.0f);
		fv[0] = 1.0f; fv[1] = 0.0f; fv[2] = 0.0f; fw[0] = 1.0f;
		V::Freiner(fv, fw, 1.0f, 5.0f, 0.5f, 0.25f);       // au seuil bas compris : pas d'arrêt
		VERIF(fv[0] == 0.25f && fw[0] == 0.25f);
		// La mise au repos d'un objet (FUN_004cf190) : +0x28, ou +0xb0 pour l'octet +0x20a à 7 ; le compteur à 10 ou à 0.
		uint32 r28 = 5, rb0 = 5; unsigned char rcpt = 3;
		V::Reposer(1, false, &r28, &rb0, &rcpt);
		VERIF(r28 == 1 && rb0 == 5 && rcpt == 10);
		V::Reposer(0, true, &r28, &rb0, &rcpt);
		VERIF(r28 == 1 && rb0 == 0 && rcpt == 0);
		// La fléchette se plante : la vitesse à 0, la position avancée de 0,1 le long de la direction du vol.
		float dp[3] = {1.0f, 2.0f, 3.0f}, dv[3] = {0.0f, 3.0f, 4.0f};
		V::Planter(dp, dv);
		VERIF(dv[0] == 0.0f && dv[1] == 0.0f && dv[2] == 0.0f && dp[0] == 1.0f && fabsf(dp[1] - 2.06f) < 1e-6f && fabsf(dp[2] - 3.08f) < 1e-6f);
		// Le frisbee dévié : contre un mur droit, 0,03 le long de la normale et 0,03 vers le bas ; rien sur un sol ni au-dessus de 0,7.
		float fn[3] = {2.0f, 0.0f, 0.0f}, fz[3] = {0.0f, 0.0f, 1.0f}, fp[3] = {0.6f, 0.0f, 0.8f}, fq[3] = {0.8f, 0.0f, 0.6f}, fr[3] = {0.0f, 0.0f, 0.0f};
		VERIF(V::Devier(fr, fn, false) && fabsf(fr[0] - 0.03f) < 1e-8f && fr[1] == 0.0f && fabsf(fr[2] + 0.03f) < 1e-8f);
		fr[0] = fr[1] = fr[2] = 0.0f;
		VERIF(!V::Devier(fr, fz, false) && !V::Devier(fr, fp, false) && fr[0] == 0.0f && fr[2] == 0.0f);
		VERIF(V::Devier(fr, fq, false) && fabsf(fr[0] - 0.8f * 0.03f / 7.0f) < 1e-7f && fabsf(fr[2] - (0.6f * 0.03f - 0.03f) / 7.0f) < 1e-7f);
		// Un sol sous une entité à plat (+0xf0) devient une normale nulle : la poussée vers le bas seule, entière.
		fr[0] = fr[1] = fr[2] = 0.0f;
		VERIF(V::Devier(fr, fz, true) && fr[0] == 0.0f && fabsf(fr[2] + 0.03f) < 1e-8f);
		// La fin d'un projectile (FUN_004d45e0, FUN_004d3dc0).
		CPlacesProjectiles f;
		VERIF(f.Prendre(1000) == 0 && f.Vit(0, 1000, 0) && f.Vit(0, 999999, 0) && f.Vit(0, 1500, 500) && !f.Vit(0, 1501, 500) && !f.Vit(1, 1000, 0));
		VERIF(f.Exploser(0) && !f.Vit(0, 1000, 0) && !f.Detruire(0, true) && !f.actif[0] && !f.Detruire(0, true));
		VERIF(f.Prendre(2000) == 0 && f.Vit(0, 2000, 0) && f.Detruire(0, true) && !f.actif[0] && f.explose[0]);
		VERIF(f.Prendre(3000) == 0 && !f.Detruire(0, false) && !f.explose[0] && !f.Detruire(5, true));
		VERIF(CPlacesProjectiles::ExploseDansLEau(true, false) && CPlacesProjectiles::ExploseDansLEau(false, true) && !CPlacesProjectiles::ExploseDansLEau(false, false));
		// Les marques d'impact (FUN_00560f70, FUN_00560f10) et celle du contact avec un bâtiment (FUN_0046ad90).
		CPlacesMarques m;
		const float pt[3] = {1.0f, 2.0f, 3.0f}, nz[3] = {0.0f, 0.0f, 1.0f};
		auto partout = [](const float *) { return true; };
		auto haut = [](const float *p) { return p[2] > 3.005f; };
		m.Poser(7, pt, nz);
		VERIF(m.indice == 1 && m.genre[0] == 7 && m.pos[0][2] > 3.0099f && m.pos[0][2] < 3.0101f && m.pos[0][0] == 1.0f);
		VERIF(m.Compter(7, partout) == 1 && m.Compter(7, haut) == 1 && m.Compter(8, partout) == 0);
		for(int k = 0; k < 31; k++) m.Poser(8, pt, nz);
		VERIF(m.indice == 0 && m.Compter(8, partout) == 31 && m.Compter(7, partout) == 1);
		m.Poser(9, pt, nz);   // l'anneau : la place 0 cède
		VERIF(m.indice == 1 && m.Compter(7, partout) == 0 && m.Compter(9, partout) == 1 && m.Compter(8, partout) == 31);
		VERIF(K::MarqueDuBatiment(1.0f) == 0 && K::MarqueDuBatiment(-0.3f) == 0 && K::MarqueDuBatiment(0.29f) == 1 && K::MarqueDuBatiment(0.0f) == 1);
	}
	printf(echecs ? "test_armes : %d échec(s)\n" : "test_armes : tout passe\n", echecs);
	return echecs != 0;
}
