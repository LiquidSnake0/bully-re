// Les ramassages (src/objects/Ramassages) : la création (FUN_00443af0), les poignées et leur génération, les drapeaux de +0x46
// (à la main pour PickupCreateXYZ, au contact pour un collectible), le pool plein ; le ramassage d'un collectible (FUN_00445640), la
// prise au contact (FUN_00445710) et celle d'un objet (FUN_00444ac0, les genres 2 à 4). Aucun fichier du jeu.
//   build/tests/test_ramassages
#include "../src/objects/Ramassages.h"
#include "../src/objects/Collectibles.h"
#include "../src/peds/Armes.h"
#include "../src/core/ActionTree.h"
#include <cstdio>
#include <cstring>
#include <map>

struct CMonde : CMondeArmes {
	std::map<int32, CTypeArme> types;
	CTypeArme defaut;
	const CTypeArme &Type(int32 a) const override { auto it = types.find(a); return it == types.end() ? defaut : it->second; }
};

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	static CRamassages p;
	// La brique de 1_03 (PickupCreateXYZ : FUN_00443e90, son cinquième argument pose le bit 2) : la première poignée vaut 0x10000.
	int32 h = p.Creer(CVector(130.631f, 8.68252f, 6.16985f), 311, "", false, false, 0, 0, false, 0.035f);
	VERIF(h == 0x10000);
	CRamassage *r = p.Entree(h);
	VERIF(r && r->modele == 311 && r->actif == 1 && r->collectible == -1);
	VERIF(r && r->pos.z > 6.2048f && r->pos.z < 6.2049f);   // z + le +0x44 de la fiche (0,035 pour la brique)
	VERIF(r && (r->drapeaux & RAMASSAGE_PRIS) && (r->drapeaux & RAMASSAGE_A_LA_MAIN) && !(r->drapeaux & RAMASSAGE_COLLECTIBLE));
	// Le transistor de zone_school6.ipb (FUN_00445640 : le sixième argument à 1, le haché de l'instance en +0x3c).
	int32 c = p.Creer(CVector(123.451f, -9.058f, 12.658f), 476, "", false, true, 0x714e6af9u, 0, false);
	VERIF(c == 0x10001);
	r = p.Entree(c);
	VERIF(r && (r->drapeaux & RAMASSAGE_COLLECTIBLE) && !(r->drapeaux & RAMASSAGE_A_LA_MAIN) && r->mot3c == 0x714e6af9u);
	// Retiré, la poignée ne vaut plus ; la place reprise monte d'une génération.
	p.Retirer(p.Indice(h));
	VERIF(p.Entree(h) == nil && p.Indice(h) == -1);
	int32 h2 = p.Creer(CVector(0, 0, 0), 311, "", false, false, 0, 0, false);
	VERIF(h2 == 0x20000 && p.Entree(h) == nil && p.Entree(h2) != nil);
	// La génération repasse de 0xfffe à 1.
	p.entrees[5].generation = 0xfffe;
	int n = 0;
	while(p.Creer(CVector(0, 0, 0), 311, "", false, false, 0, 0, false) >= 0) n++;
	VERIF(n == CRamassages::kTaille - 2);
	VERIF(p.entrees[5].generation == 1 && p.Poignee(5) == 0x10005);
	VERIF(p.Creer(CVector(0, 0, 0), 311, "", false, false, 0, 0, false) == -1);

	// Le transistor : un collectible de colctbls.dat, son ramassage (FUN_00445640), pris au contact par le joueur.
	static CCollectibles col;
	const char *texte = "Radio\nradio01\nradio06\nend\n";
	col.Initialiser(texte, strlen(texte));
	static CRamassages q;
	int16 idx = (int16)col.Indice(0x714e6af9u);
	VERIF(idx == 1);
	int32 t = q.CreerDuCollectible(CVector(123.451f, -9.058f, 12.658f), 476, idx, 0x714e6af9u, 12, 0.0f);
	col.PoserRamassage(idx, t);
	CRamassage *e = q.Entree(t);
	VERIF(e && e->collectible == 1 && e->octet45 == 12 && (e->drapeaux & RAMASSAGE_COLLECTIBLE) && col.Ramassage(1) == t);

	printf(echecs ? "test_ramassages : %d échec(s)\n" : "test_ramassages : tout passe\n", echecs);
	return echecs != 0;
}
