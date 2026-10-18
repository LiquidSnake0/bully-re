// La banque des missions lue comme le moteur : Missions.cat et les onze sous-arbres qu'il cite.
//   BULLY_DATA=<racine du jeu> build/tests/test_missions [--liste]
#include "../src/core/MissionBank.h"
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

	printf("test_missions : %zu missions dans %zu fichiers, %d lançables ; %s\n", b.missions.size(), b.banques.size(), jouables,
	       echecs == 0 ? "OK" : "ECHEC");
	return echecs != 0;
}
