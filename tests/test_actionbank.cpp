// L'arbre d'actions global et le chemin d'un nœud (src/core/ActionBank) : 1_01.cat greffé sous /Global,
// les chemins que 1_01.lua donne à PedSetActionNode, « .. », la casse, le chargement à la demande
// de Player.act (FUN_004849a0), deux fichiers sous la même racine.
//   BULLY_DATA=<racine du jeu> build/tests/test_actionbank
#include "../src/core/ActionBank.h"
#include <cstdio>
#include <cstring>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	VERIF(CActionBank::NomCat("Act/Conv/1_01.act") == "1_01.cat" && CActionBank::NomCat("/Act/Player.act") == "Player.cat" && CActionBank::NomCat("Act\\AI\\AI.act") == "AI.cat");
	CActionBank b;
	VERIF(b.racine.enfants.size() == 1 && b.Global()->nom == "Global" && b.Global()->hachage == ActionHash("GLOBAL") && b.Global()->enfants.empty());
	VERIF(b.Trouver("/") == &b.racine && b.Trouver("/Global") == b.Global() && b.Trouver("/Global/..") == &b.racine && b.Trouver("/Global/1_01") == nil && b.Trouver("Global") == nil);
	VERIF(!b.EstCharge("Act/Conv/1_01.act"));
	VERIF(!b.Charger("Act/Conv/pas_un_fichier.act"));
	VERIF(b.Charger("Act/Conv/1_01.act"));
	if(b.Global()->enfants.empty()){ printf("test_actionbank : ECHEC (Act\\Act.img ?)\n"); return 1; }
	VERIF(b.EstCharge("Act/Conv/1_01.act") && b.EstCharge("1_01.act") && b.fichiers.size() == 1 && b.Global()->enfants.size() == 1);
	// Les nœuds que 1_01.lua demande : /Global/1_01/Talking (une banque), ShortIdle, PunchHands, WaveToSec
	// (des feuilles à une piste), Gates/BarUpHold.
	const CActionNode *talking = b.Trouver("/Global/1_01/Talking");
	VERIF(talking != nil && talking->genre == 'b' && talking->hachage == ActionHash("Talking") && talking->nom.empty() && talking->enfants.size() == 1);
	const CActionNode *idle = b.Trouver("/Global/1_01/ShortIdle");
	VERIF(idle != nil && idle->genre == 'l' && idle->pistes.size() == 1);
	VERIF(b.Trouver("/Global/1_01/PunchHands") != nil && b.Trouver("/Global/1_01/WaveToSec") != nil);
	const CActionNode *barre = b.Trouver("/Global/1_01/Gates/BarUpHold");
	VERIF(barre != nil && barre->genre == 'l' && b.Trouver("/Global/1_01/Gates/OpenHold") != nil);
	// « .. », les deux séparateurs, la casse (HashString met en majuscules), un segment vide.
	VERIF(b.Trouver("/Global/1_01/Talking/Talk/..") == talking && b.Trouver("\\Global\\1_01\\Gates\\..\\ShortIdle") == idle);
	VERIF(b.Trouver("/global/1_01/TALKING") == talking && b.Trouver("/Global//1_01/ShortIdle") == idle);
	VERIF(b.Trouver("/Global/1_01/Nope") == nil && b.Trouver("/Global/1_02/Talking") == nil && b.Trouver("/Autre/1_01") == nil);
	// FUN_004849a0 : trouver, sinon charger le fichier donné.
	VERIF(!b.EstCharge("Act/Player.act"));
	VERIF(b.TrouverOuCharger("/Global/Player", "Act/Player.act"));
	VERIF(b.EstCharge("Act/Player.act") && b.fichiers.size() == 2 && b.Global()->enfants.size() == 2);
	VERIF(b.Trouver("/Global/Player/Default_KEY/Locomotion") != nil);
	VERIF(!b.TrouverOuCharger("/Global/1_02/Talking/Talk", nil) && !b.TrouverOuCharger("/Global/Nope", "Act/Player.act"));
	VERIF(b.TrouverOuCharger("/Global/1_02/Talking/Talk", "Act/Conv/1_02.act") && b.fichiers.size() == 3);
	// 1_01 est encore là après les autres.
	VERIF(b.Trouver("/Global/1_01/Talking") == talking);
	printf("  %zu fichiers sous /Global (%zu banques), /Global/1_01/Talking, Gates/BarUpHold, Player et 1_02 trouvés\n", b.fichiers.size(), b.Global()->enfants.size());
	printf("test_actionbank : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
