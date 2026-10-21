// La parole enregistrée (src/audio/SpeechBank) sur les vrais fichiers : SpeechDB.bin et la table de Speech.bin.
//   BULLY_DATA=<racine du jeu> build/tests/test_speechbank
#include "../src/audio/SpeechBank.h"
#include "../src/audio/SpeechEvents.h"
#include "../src/core/FileMgr.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC ligne %d : %s\n", __LINE__, #cond); echecs++; } }while(0)

int
main(void)
{
	CSpeechBank b;
	VERIF(b.Charger());
	VERIF(b.evenements.size() == NUM_SPEECH_EVENTS && b.echantillons.size() == 40400);
	size_t elements = 0; bool dansLaTable = true;
	for(const auto &e : b.evenements) for(const CSpeechItem &it : e){ elements++; if(it.echantillon >= b.echantillons.size()) dansLaTable = false; }
	VERIF(elements == 40882 && dansLaTable);
	// La table est triée par clé, pour le bsearch de FUN_005a7c80.
	bool triee = true;
	for(size_t k = 1; k < b.echantillons.size(); k++) if(b.echantillons[k - 1].cle >= b.echantillons[k].cle) triee = false;
	VERIF(triee);
	// Les clés sont le hachage FUN_005a7c10 des noms de fichiers : audio\PLAYLIST\Speech.lst (que le moteur ne
	// lit pas) les donne dans l'ordre de la table, et les 40 400 tombent juste.
	static uint8 lst[0xe0000];
	int32 n = CFileMgr::LoadFile("audio\\PLAYLIST\\Speech.lst", lst, sizeof(lst), "rb");
	std::vector<std::string> noms;
	for(char *l = strtok((char *)lst, "\r\n"); l; l = strtok(nil, "\r\n")) noms.push_back(l);
	int justes = 0;
	for(size_t k = 0; k < noms.size() && k < b.echantillons.size(); k++) justes += CSpeechBank::Cle(noms[k].c_str()) == b.echantillons[k].cle;
	printf("  %zu événements, %zu éléments, %zu échantillons ; %d clés sur %zu refaites depuis Speech.lst (%d octets)\n",
	       b.evenements.size(), elements, b.echantillons.size(), justes, noms.size(), n);
	VERIF(noms.size() == 40400 && justes == 40400);
	VERIF(CSpeechBank::Cle("JIMMY_NARR1_01_100.RSM") == CSpeechBank::Cle("jimmy_narr1_01_100.rsm"));

	// La narration de 1_02 : Jimmy (modèle 0), NARRATION lignes 101 à 104 = NARR1_01_100 … NARR1_02_101.
	const char *attendus[] = { "Jimmy_NARR1_01_100.rsm", "Jimmy_NARR1_01_101.rsm", "Jimmy_NARR1_02_100.rsm", "Jimmy_NARR1_02_101.rsm" };
	int narration = SpeechEventIndex("NARRATION");
	for(int l = 101; l <= 104; l++){
		std::vector<int> c = b.Candidats(narration, 0, l);
		VERIF(c.size() == 1);
		if(c.size() != 1) continue;
		int ms = b.DureeMs(c[0]);
		printf("  NARRATION %d : %s, %.2f s\n", l, noms[c[0]].c_str(), ms / 1000.0);
		VERIF(noms[c[0]] == attendus[l - 101] && ms > 1000 && ms < 20000);
	}
	// La secrétaire de 1_02 (modèle 59, Mlle Danvers) : M_1_02 lignes 101 et 123.
	int m102 = SpeechEventIndex("M_1_02");
	for(int l : {101, 123}){
		std::vector<int> c = b.Candidats(m102, 59, l);
		VERIF(c.size() == 1);
		if(c.size() == 1) printf("  M_1_02 %d (modèle 59) : %s, %.2f s\n", l, noms[c[0]].c_str(), b.DureeMs(c[0]) / 1000.0);
	}
	// Une ligne au-delà de 255 ne trouve rien : FUN_0059ee10 compare les 11 bits demandés à un octet.
	VERIF(b.Candidats(narration, 0, 101 + 256).empty());
	// Plusieurs prises d'une même ligne : Bucky, M_1_02 ligne 113, v1 à v3.
	VERIF(b.Candidats(m102, 8, 113).size() == 3);

	printf("test_speechbank : %s\n", echecs ? "ECHEC" : "ok");
	return echecs != 0;
}
