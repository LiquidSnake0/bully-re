// Les objets animés des déclencheurs (src/objects/ObjetsAnimes) : leur création pour la zone visible (FUN_00416540,
// FUN_0044cd20), leur retrait hors de la zone (FUN_00416490), les drapeaux (FUN_004ea300) et la fiche PropButes
// (FUN_00729b70, FUN_00520460). Aucun fichier du jeu.
//   build/tests/test_objetsanimes
#include "../src/objects/ObjetsAnimes.h"
#include "../src/core/ActionTree.h"
#include <cstdio>
#include <cstring>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

static CObjetsAnimes::Declencheur
D(int h, int32 modele, int zone, int emplacement = 0, bool mission = false)
{
	CObjetsAnimes::Declencheur d;
	d.handle = h; d.modele = modele; d.zone = zone; d.emplacement = emplacement; d.mission = mission;
	d.pos = CVector((float)h, 0.0f, 5.5f); d.lacet = 180.0f;
	return d;
}

int
main(void)
{
	CObjetsAnimes p;
	CFicheObjetAnime hiver;   // le constructeur FUN_00729b70 : toutes les saisons, puis l'hiver seul
	VERIF(hiver.saisons[0] && hiver.saisons[1] && hiver.saisons[2] && hiver.saisons[3] && hiver.distance == 2.0f);
	VERIF(!hiver.pnj && hiver.garcons && hiver.filles);
	hiver.saisons[0] = hiver.saisons[2] = hiver.saisons[3] = false;
	std::vector<CObjetsAnimes::Declencheur> ds = {
		D(0, 10028, 2),            // la case 0 : jamais prise
		D(1, 10028, 2),            // un casier de l'école
		D(2, -1, 2),               // pas de PLACEMENT
		D(3, 10028, 0),            // une autre zone
		D(4, 10028, 2, 5),         // l'emplacement 5 (portes, tags)
		D(5, 10028, 2, 0, true),   // propre à une mission
		D(6, 10027, 2),
		D(7, 10027, 2),
	};
	ds[6].porte = true;
	ds[7].fiche = &hiver;
	// FUN_00416540 pour la zone 2, la saison 0 : les déclencheurs 1 et 6 ; le 7 n'existe qu'en saison 1 (+0x75).
	std::vector<int> c = p.CreerPourZone(2, ds, 0);
	VERIF(c.size() == 2 && p.ObjetDe(1) == c[0] && p.ObjetDe(6) == c[1]);
	VERIF(p.ObjetDe(0) == -1 && p.ObjetDe(2) == -1 && p.ObjetDe(3) == -1 && p.ObjetDe(4) == -1 && p.ObjetDe(5) == -1 && p.ObjetDe(7) == -1);
	const CObjetAnime *o = p.Objet(c[0]);
	VERIF(o && o->modele == 10028 && o->declencheur == 1 && o->zone == 2 && o->octet3a8 == 1 && o->pos.x == 1.0f && o->utilisateur == -1);
	// FUN_0044cd20 : le drapeau 13 vaut le bit 3 de +0x3f (le nom en DT_).
	VERIF(!p.Drapeau(c[0], 13) && p.Drapeau(c[1], 13));
	// Un déclencheur qui a déjà son objet n'en reçoit pas un second.
	VERIF(p.CreerPourZone(2, ds, 0).empty());
	VERIF(p.CreerPourZone(2, ds, 1).size() == 1 && p.ObjetDe(7) >= 0);
	// FUN_004ea300 et PAnimGetPropFlag (le mot +0x160 + 4n).
	p.PoserDrapeau(c[0], 11, true); p.PoserDrapeau(c[0], 19, true); p.PoserDrapeau(c[0], 40, true);
	VERIF(p.Drapeau(c[0], 11) && p.Drapeau(c[0], 19) && !p.Drapeau(c[0], 12) && !p.Drapeau(c[0], 40));
	p.PoserDrapeau(c[0], 11, false);
	VERIF(!p.Drapeau(c[0], 11));
	// FUN_00416490 vers la zone 0 : les objets de la zone 2 partent, leur déclencheur perd son +0x1c.
	std::vector<int> partis = p.RetirerHorsZone(0, ds);
	VERIF(partis.size() == 3 && p.ObjetDe(1) == -1 && p.ObjetDe(6) == -1 && p.ObjetDe(7) == -1 && p.Objet(c[0]) == nil);
	// Les cases libérées resservent ; la génération monte.
	std::vector<int> d = p.CreerPourZone(0, ds, 0);
	VERIF(d.size() == 1 && p.ObjetDe(3) == d[0] && p.Objet(d[0])->generation == 2);
	// Un objet propre à une mission ou de l'emplacement 5 resterait ; DATUnload emporte celui de son déclencheur.
	p.RetirerDuDeclencheur(3);
	VERIF(p.ObjetDe(3) == -1);
	// La fiche lue (FUN_00520460) : l'arbre, la saison, la portée, qui s'en sert, le genre.
	CActionTrack t;
	auto ajouter = [&](uint16 pos, uint32 v, uint8 n){ CActionAttribut a{}; a.position = pos; a.taille = n; memcpy(a.valeur, &v, 4); t.attributs.push_back(a); };
	float un = 1.0f; uint32 m; memcpy(&m, &un, 4);
	ajouter(116, 1, 1); ajouter(117, 0, 1); ajouter(120, m, 4); ajouter(124, 1, 1); ajouter(126, 0, 1); ajouter(128, 0x8000beefu, 4);
	std::map<uint16, std::string> ch = {{104, "/Global/NLockA"}, {108, "/Act/Props/NLockA.act"}, {112, "/Global/NLockA/RandomLockState"}};
	CFicheObjetAnime f = CFicheObjetAnime::Lire(t, ch);
	VERIF(f.lue && f.racine == "/Global/NLockA" && f.fichier == "/Act/Props/NLockA.act" && f.depart == "/Global/NLockA/RandomLockState");
	VERIF(f.saisons[0] && !f.saisons[1] && f.saisons[2] && f.distance == 1.0f && f.pnj && f.garcons && !f.filles && f.genre == 0xbeef);
	// Les portes (FUN_004eebc0, FUN_004eed70) : ouverte, les drapeaux 0, 15 et 9 ; fermée, remis à 0 ; une porte déjà ouverte ne
	// se rouvre pas, une porte fermée ne se referme pas ; sans nœud, rien ne change.
	CObjetsAnimes q;
	std::vector<CObjetsAnimes::Declencheur> ps = { D(1, 999, 2) };
	int k = q.CreerPourZone(2, ps, 0)[0];
	VERIF(!q.FermerPorte(k, true, false));
	VERIF(q.OuvrirPorte(k, 0, false, false) && !q.Drapeau(k, 9));
	VERIF(q.OuvrirPorte(k, 1, true, false) && q.Drapeau(k, 9) && q.Drapeau(k, 0) && q.Drapeau(k, 15));
	VERIF(!q.OuvrirPorte(k, 0, true, false));
	VERIF(q.FermerPorte(k, true, false) && !q.Drapeau(k, 9) && !q.Drapeau(k, 0) && !q.Drapeau(k, 15));
	// PAnimCreate (CreerParScript) : un déclencheur propre à une mission, d'une autre zone, a son objet ; déjà là, il le garde ; sans
	// modèle, rien. CreatePersistentEntity (CreerPersistant) : sans déclencheur, l'octet +0x3a8 à 0, gardé hors de sa zone.
	CObjetsAnimes s2;
	CObjetsAnimes::Declencheur md = D(9, 10015, 0, 2, true);
	int a = s2.CreerParScript(md, 1);
	VERIF(a >= 0 && s2.ObjetDe(9) == a && s2.Objet(a)->octet3a8 == 1 && s2.Objet(a)->modele == 10015);
	VERIF(s2.CreerParScript(md, 0) == a);
	VERIF(s2.CreerParScript(D(10, -1, 0), 1) == -1);
	int b = s2.CreerPersistant(10003, CVector(1, 2, 3), 90.0f, 0);
	VERIF(b >= 0 && b != a && s2.Objet(b)->declencheur == -1 && s2.Objet(b)->octet3a8 == 0);
	std::vector<CObjetsAnimes::Declencheur> ds2 = { md };
	VERIF(s2.RetirerHorsZone(2, ds2).empty() && s2.Objet(a) && s2.Objet(b));   // la mission et la case 0 restent
	s2.Liberer(b);
	VERIF(s2.Objet(b) == nil);
	printf("test_objetsanimes : %d échec(s)\n", echecs);
	return echecs ? 1 : 0;
}
