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
	// Au contact : à moins de 1 m au plan, au plus 2 m d'écart en hauteur, le type pris au contact, le joueur vivant.
	VERIF(q.AuContact(q.Indice(t), CVector(123.9f, -9.3f, 11.0f), 100.0f, true));
	VERIF(!q.AuContact(q.Indice(t), CVector(124.6f, -9.058f, 12.658f), 100.0f, true));   // 1,15 m au plan
	VERIF(!q.AuContact(q.Indice(t), CVector(123.451f, -9.058f, 10.5f), 100.0f, true));   // 2,16 m plus bas
	VERIF(!q.AuContact(q.Indice(t), CVector(123.451f, -9.058f, 12.0f), 100.0f, false));  // l'octet +0x25 du type
	VERIF(!q.AuContact(q.Indice(t), CVector(123.451f, -9.058f, 12.0f), 0.0f, true));     // la santé
	VERIF(!q.AuContact(q.Indice(h2), CVector(0, 0, 0), 100.0f, true));                   // une brique : à la main
	// Pris (le genre 4, un item de maximum 10) : marqué, le rappel des scripts, compté, retiré.
	CMonde m;
	CTypeArme radio; radio.lu = true; radio.genre = 4; radio.maximum = 10; radio.auContact = true; m.types[476] = radio;
	CObjetsJoueur o; o.premier = 460;
	CRappelCollectible r1; bool a = false;
	VERIF(q.PrendreObjet(q.Indice(t), 4, m, o, col, &r1, &a) == 1);
	VERIF(a && r1.type == COLLECTIBLE_RADIO && r1.pris == 1 && r1.nombre == 2 && r1.ramassage == t && r1.indice == 1);
	VERIF(col.Pris(1) && o.Nombre(476) == 1 && q.Entree(t) == nil);
	// Plein : rien n'est pris ; un autre genre non plus.
	o.Poser(m, 476, 10);
	int32 t2 = q.CreerDuCollectible(CVector(0, 0, 0), 476, 0, 0, 0, 0.0f);
	VERIF(q.PrendreObjet(q.Indice(t2), 4, m, o, col, &r1, &a) == 0 && !a && q.Entree(t2) != nil && !col.Pris(0));
	VERIF(q.PrendreObjet(q.Indice(t2), 0, m, o, col, &r1, &a) == 0);

	// Laissé par un piéton (FUN_00443d90) : 0,45 m au-dessus de lui, plus le décalage du type, et pris au contact (bits 0 et 2
	// effacés, même quand la fiche porte son octet +0x18).
	static CRamassages pd;
	int32 s = pd.CreerDuPieton(CVector(10, 20, 6), 502, "HealthBute", 0.1f);
	const CRamassage *sr = pd.Entree(s);
	VERIF(sr && sr->modele == 502 && sr->fiche == "HealthBute" && sr->pos.z > 6.5499f && sr->pos.z < 6.5501f && sr->pos.x == 10 && sr->pos.y == 20);
	VERIF(sr && (sr->drapeaux & (RAMASSAGE_FICHE | RAMASSAGE_A_LA_MAIN)) == 0 && sr->Pris() && !(sr->drapeaux & RAMASSAGE_COLLECTIBLE));
	VERIF(pd.AuContact(pd.Indice(s), CVector(10.5f, 20, 6), 100.0f, true));

	// ObjectRemovePickupsInTrigger : les entrées prises de ce modèle (ou toutes pour -1) dans le déclencheur.
	static CRamassages z;
	z.Creer(CVector(1, 1, 0), 500, "", false, true, 0, 0, false);
	z.Creer(CVector(2, 2, 0), 474, "", false, true, 0, 0, false);
	z.Creer(CVector(50, 50, 0), 500, "", false, true, 0, 0, false);
	auto dedans = [](const CVector &p){ return p.x < 10.0f && p.y < 10.0f; };
	VERIF(z.RetirerSi(474, dedans) == 1 && !z.entrees[1].Pris() && z.entrees[0].Pris());
	VERIF(z.RetirerSi(-1, dedans) == 1 && !z.entrees[0].Pris() && z.entrees[2].Pris());

	printf(echecs ? "test_ramassages : %d échec(s)\n" : "test_ramassages : tout passe\n", echecs);
	return echecs != 0;
}
