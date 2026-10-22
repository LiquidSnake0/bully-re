// La banque des missions lue comme le moteur : Missions.cat et les onze sous-arbres qu'il cite.
//   BULLY_DATA=<racine du jeu> build/tests/test_missions [--liste]
#include "../src/core/MissionBank.h"
#include "../src/core/ActionTree.h"
#include "../src/core/ConditionListe.h"
#include <vector>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(int argc, char **argv)
{
	CMissionBank b;
	VERIF(b.Charger());
	if(argc > 1 && strcmp(argv[1], "--liste") == 0)
		for(const CMissionData &m : b.missions)
			printf("%s\t%s\t%d\t%s\t%s\t%s\n", m.banque.c_str(), m.nom.empty() ? "#" : m.nom.c_str(), m.type,
			       m.titre.c_str(), m.script.c_str(), m.cle.c_str());

	// Missions.cat puis ses onze références, dans l'ordre du fichier, Doors compris.
	static const char *attendues[] = { "Missions.cat", "chap1.cat", "chap2.cat", "chap3.cat", "chap4.cat", "chap5.cat",
	                                   "classes.cat", "Minigames.cat", "Punish.cat", "Tutorials.cat", "Doors.cat", "Debug.cat" };
	VERIF(b.banques.size() == 12);
	for(size_t i = 0; i < b.banques.size() && i < 12; i++) VERIF(b.banques[i] == attendues[i]);

	// Une entrée par piste MissionData : 499, dont 360 ont un script à lancer.
	VERIF(b.missions.size() == 499);
	std::map<std::string, int> parBanque; std::map<int, int> parType; int jouables = 0;
	for(const CMissionData &m : b.missions){ parBanque[m.banque]++; parType[m.type]++; jouables += m.Jouable(); }
	VERIF(parBanque["chap1.cat"] == 33 && parBanque["Tutorials.cat"] == 116 && parBanque["Doors.cat"] == 17 && parBanque["Debug.cat"] == 103);
	VERIF(jouables == 360);
	// Les types (champ 40) : 0 histoire, 1 cours, 3 boulots et courses, 4 tutoriels, 5 mini-jeux, et rien d'autre.
	VERIF(parType.size() == 5 && parType.count(0) && parType.count(1) && parType.count(3) && parType.count(4) && parType.count(5));
	VERIF(parType[0] == 148 && parType[1] == 64 && parType[3] == 101 && parType[4] == 114 && parType[5] == 72);

	// Qui démarre à la nouvelle partie : 1_01 n'a aucune condition et ne se lance pas seule (champ 51) ;
	// son corona 1_01_Corona est un pavé de 2 m de demi-côté (champ 28), les autres font 1 m ; 1_02 a une
	// condition et se lance seule.
	int sansCondition = 0, larges = 0, coronas = 0;
	for(const CMissionData &x : b.missions){ sansCondition += x.conditions == 0 && x.Jouable(); coronas += !x.corona.empty(); larges += x.coronaDemiCote == 2; }
	printf("%d missions jouables sans condition, %d coronas dont %d de 2 m de demi-côté\n", sansCondition, coronas, larges);
	VERIF(larges == 19);
	const CMissionData *d2 = b.Trouver("1_02");
	VERIF(d2 != nil && d2->conditions == 1 && d2->lancerSeul && d2->corona.empty());
	const CMissionData *d3 = b.Trouver("1_02B");
	VERIF(d3 != nil && d3->conditions == 1 && d3->corona == "1_02B_Corona" && d3->coronaDemiCote == 1 && d3->coronaMarqueur == 1 && d3->coronaMarqueurVisible && d3->coronaTaille == 25 && d3->coronaInvite == "BUT_LAUNCH");

	// Les conditions, construites sur l'enregistrement comme la fabrique FUN_0061a4f0 le fait : la chaîne
	// du chapitre 1. 1_02 attend la réussite de 1_01, 1_02A celle de 1_02, 1_02C celle de 1_02B ; 1_03
	// attend 1_02C et 1_02B, entre 8 h et 19 h (TimeCond : +4 = 8, +8 = 19, minutes à 0).
	{
		CEtatMissions etat; etat.reussites.assign(b.missions.size(), 0);
		int i01 = b.Index("1_01"), i02 = b.Index("1_02"), i02a = b.Index("1_02A"), i02b = b.Index("1_02B"),
		    i02c = b.Index("1_02C"), i03 = b.Index("1_03");
		VERIF(i01 >= 0 && i02 >= 0 && i02a >= 0 && i02b >= 0 && i02c >= 0 && i03 >= 0);
		VERIF(d2 && d2->conds.size() == 1 && d2->conds[0].mots[0] == (ActionHash("1_01") & 0x7fffffff));
		VERIF(b.Activable(i01, etat) == 1);                  // aucune condition
		VERIF(b.Activable(i02, etat) == 0 && b.Activable(i02a, etat) == 0);
		etat.reussites[i01] = 1;
		VERIF(b.Activable(i02, etat) == 1 && b.Activable(i02a, etat) == 0);
		etat.reussites[i02] = 1;
		VERIF(b.Activable(i02a, etat) == 1);
		const CMissionData &m3 = b.missions[i03];
		VERIF(m3.conds.size() == 3);
		int temps = 0;
		for(const CMissionCondition &c : m3.conds)
			if(c.type == (ActionHash("TimeCond") & 0x7fffffff)){ temps++; VERIF(c.mots[0] == 8 && c.mots[1] == 19 && c.mots[2] == 0 && c.mots[3] == 0); }
		VERIF(temps == 1);
		etat.reussites[i02b] = 1; etat.reussites[i02c] = 1;
		etat.heure = 7; etat.minute = 59; VERIF(b.Activable(i03, etat) == 0);
		etat.heure = 8; etat.minute = 0; VERIF(b.Activable(i03, etat) == 1);
		etat.heure = 18; etat.minute = 59; VERIF(b.Activable(i03, etat) == 1);
		etat.heure = 19; VERIF(b.Activable(i03, etat) == 0);
		// Une plage qui passe minuit (FUN_00425cf0) : 22 h – 6 h.
		CMissionCondition nuit; nuit.type = ActionHash("TimeCond") & 0x7fffffff; nuit.mots[0] = 22; nuit.mots[1] = 6;
		etat.heure = 23; VERIF(b.Evaluer(nuit, etat) == 1);
		etat.heure = 3; VERIF(b.Evaluer(nuit, etat) == 1);
		etat.heure = 12; VERIF(b.Evaluer(nuit, etat) == 0);
		// Le relevé : sur les 360 missions jouables, combien de conditions chaque classe porte, et combien de
		// missions ne s'évaluent qu'avec les trois classes lues.
		std::map<std::string, int> classes; int lisibles = 0, avecConditions = 0;
		for(size_t i = 0; i < b.missions.size(); i++){
			if(!b.missions[i].Jouable() || b.missions[i].conds.empty()) continue;
			avecConditions++;
			bool toutes = true;
			for(const CMissionCondition &c : b.missions[i].conds){
				const char *cl = nil; int v = b.Evaluer(c, etat, &cl);
				classes[cl ? cl : "autre"]++;
				if(v < 0) toutes = false;
			}
			lisibles += toutes;
		}
		printf("conditions des missions jouables :");
		for(auto &k : classes) printf(" %s %d", k.first.c_str(), k.second);
		printf(" ; %d missions sur %d s'évaluent entièrement\n", lisibles, avecConditions);
		VERIF(classes["MissionCond"] > 0 && classes["TimeCond"] > 0);
		// La liste de FUN_005fb9b0, sur des nœuds faits à la main : A NOT B, A OR B, A B OR C.
		const uint32 kM = ActionHash("MissionCond") & 0x7fffffff, kN = ActionHash("NOT") & 0x7fffffff, kO = ActionHash("OR") & 0x7fffffff;
		auto cond = [&](const char *mission){ CMissionCondition c; c.type = kM; c.mots[0] = ActionHash(mission) & 0x7fffffff; return c; };
		CMissionCondition non; non.type = kN; CMissionCondition ou; ou.type = kO;
		VERIF(non.Genre() == 1 && ou.Genre() == 2 && cond("1_01").Genre() == 0);
		CMissionBank t = b;
		auto essai = [&](std::vector<CMissionCondition> l){
			CMissionData &x = t.missions[0]; x.conds = l; x.conditions = (int)l.size(); return t.Activable(0, etat); };
		etat.reussites.assign(t.missions.size(), 0);
		etat.reussites[i01] = 1;                                     // 1_01 réussie, 1_02 non
		VERIF(essai({cond("1_01"), non, cond("1_02")}) == 1);        // 1_01 et pas 1_02
		VERIF(essai({cond("1_01"), non, cond("1_01")}) == 0);
		VERIF(essai({cond("1_02"), ou, cond("1_01")}) == 1);         // faux, puis OR : on continue
		VERIF(essai({cond("1_01"), ou, cond("1_02")}) == 1);         // vrai avant le OR : la suite est sautée
		VERIF(essai({cond("1_02"), ou, cond("1_02A")}) == 0);
		VERIF(essai({cond("1_01"), cond("1_02"), ou, cond("1_01")}) == 1);   // (1_01 et 1_02) ou 1_01
		VERIF(essai({cond("1_02"), cond("1_01"), ou, cond("1_01")}) == 0);   // un faux suivi d'autre chose qu'un OR arrête tout
		VERIF(essai({}) == 1);
		// CurrentMission : la mission du moment, réussie au moins une fois.
		{
			CMissionCondition cur; cur.type = kM; cur.mots[0] = ActionHash("CurrentMission") & 0x7fffffff;
			CMissionCondition cls = cur; cls.mots[0] = ActionHash("CurrentClassMission") & 0x7fffffff;
			etat.courante = -1; VERIF(t.Evaluer(cur, etat) == 0);
			etat.courante = i01; VERIF(t.Evaluer(cur, etat) == 1);
			etat.courante = i02; VERIF(t.Evaluer(cur, etat) == 0);
			etat.coursCourant = -1; VERIF(t.Evaluer(cls, etat) == 0);
			etat.coursCourant = i01; VERIF(t.Evaluer(cls, etat) == 1);
			etat.courante = etat.coursCourant = -1;
		}
		// DayCond : le jour du calendrier, tel quel (l'octet +8 demande un modulo dont le diviseur n'est pas lu).
		{
			int simples = 0, modulo = 0, egaux = 0;
			for(const CMissionData &x : b.missions) for(const CMissionCondition &c : x.conds)
				if(c.type == (ActionHash("DayCond") & 0x7fffffff)){ (c.mots[1] & 0xff) ? modulo++ : simples++; }
			CMissionCondition jour; jour.type = ActionHash("DayCond") & 0x7fffffff; jour.mots[0] = 2;
			etat.jour = 2; egaux += t.Evaluer(jour, etat) == 1; etat.jour = 3; egaux += t.Evaluer(jour, etat) == 0;
			jour.mots[1] = 1; etat.jour = 7; VERIF(t.Evaluer(jour, etat) == 1);   // 7 modulo 5
			etat.jour = 8; VERIF(t.Evaluer(jour, etat) == 0);
			etat.jour = 0;
			printf("DayCond : %d sans modulo, %d avec\n", simples, modulo);
			VERIF(egaux == 2 && simples + modulo > 0);
		}
		// Après une réussite, FUN_006ad130 ne réactive une mission que si son champ 44 (+0x2c) le permet.
		int rejouables = 0; for(const CMissionData &x : b.missions) rejouables += x.rejouable;
		printf("%d missions rejouables (champ 44)\n", rejouables);
		VERIF(!b.missions[i01].rejouable && !b.missions[i02].rejouable);
	}

	// 1_01 : la première mission, type 0, titre affiché, script chap1\1_01.lua.
	const CMissionData *m = b.Trouver("1_01");
	VERIF(m != nil);
	VERIF(m && m->conditions == 0 && !m->lancerSeul && m->corona == "1_01_Corona" && m->coronaDemiCote == 2 && m->coronaMarqueur == 1 && !m->coronaMarqueurVisible && m->coronaTaille == 3 && m->coronaInvite == "BUT_LAUNCH");
	if(m){
		VERIF(m->banque == "chap1.cat" && m->type == 0 && m->afficherTitre);
		VERIF(m->script == "chap1\\1_01.lua" && m->cle == "1_01_TITLE");
		VERIF(b.Trouver("1_01") == b.Trouver("1_01") && b.Index("1_01") == b.Index("1_01"));
	}
	// Le nom se cherche sans la casse, et par hachage pour les nœuds sans nom en clair.
	VERIF(b.Trouver("1_01") == b.Trouver("1_01"));
	VERIF(b.Trouver("ArcadeRace3D") != nil && b.Trouver("arcaderace3d") == b.Trouver("ArcadeRace3D"));
	VERIF(b.Trouver("pas_une_mission") == nil);
	// Doors : des « missions » de verrouillage de portes, script none.lua, non lançables.
	bool doorsSansScript = true;
	for(const CMissionData &x : b.missions) if(x.banque == "Doors.cat" && x.Jouable()) doorsSansScript = false;
	VERIF(doorsSansScript);
	// Les tutoriels n'ont aucun script : ils ne se lancent pas par ForceStartMission.
	int tutorielsSansScript = 0;
	for(const CMissionData &x : b.missions) if(x.banque == "Tutorials.cat" && !x.Jouable()) tutorielsSansScript++;
	VERIF(tutorielsSansScript == 116);

	// EvaluerListe, les règles de FUN_005fb9b0 sur des listes écrites à la main : 'v' vrai, 'f' faux, '?' inconnu,
	// 'N' NOT, 'O' OR.
	{
		auto liste = [](const char *l){
			int n = (int)strlen(l);
			return EvaluerListe(n, [&](int i){ return l[i] == 'v' || l[i] == 'N' || l[i] == 'O' ? 1 : l[i] == 'f' ? 0 : -1; },
			                    [&](int i){ return l[i] == 'N' ? 1 : l[i] == 'O' ? 2 : 0; });
		};
		VERIF(liste("") == 1 && liste("v") == 1 && liste("f") == 0 && liste("vv") == 1 && liste("vf") == 0);
		VERIF(liste("Nf") == 1 && liste("Nv") == 0);                     // NOT : le contraire de la suivante
		VERIF(liste("fOv") == 1 && liste("fOf") == 0 && liste("vOf") == 1);   // OR : la suivante décide après un faux
		VERIF(liste("vO?") == -1 && liste("?") == -1 && liste("fO?") == -1);  // la suivante d'un OR est jugée même après un vrai
		VERIF(liste("vOf") == 1 && liste("vOfv") == 1);                       // … mais sa valeur ne compte pas
		VERIF(liste("vOfv") == 1 && liste("fOvf") == 0);
	}
	printf("test_missions : %zu missions dans %zu fichiers, %d lançables ; %s\n", b.missions.size(), b.banques.size(), jouables,
	       echecs == 0 ? "OK" : "ECHEC");
	return echecs != 0;
}
