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
	// PedIsPlaying (FUN_005f3bf0 / FUN_005f4f80) : le nœud lui-même, ou, avec « sous », un de ses descendants.
	{
		const CActionNode *talking = b.Trouver("/Global/1_01/Talking");
		VERIF(talking != nil);
		VERIF(b.Joue(talking, "/Global/1_01/Talking", false) && b.Joue(talking, "/Global/1_01", true));
		VERIF(!b.Joue(talking, "/Global/1_01", false) && !b.Joue(nil, "/Global/1_01", true) && !b.Joue(talking, "/Global/Absent", true));
		VERIF(!b.Joue(b.Trouver("/Global/1_01"), "/Global/1_01/Talking", true));   // un parent n'est pas « sous » son enfant
	}
	// La descente du contrôleur (FUN_005f5490 / FUN_005f5500) : dans /Global/TrainingPlayer, la banque DEFAULT vient
	// en premier, sans condition, et sa première feuille est DEFAULT : c'est le nœud que l'entraînement de 1_02 attend
	// sous /Global/TrainingPlayer/Default. Une branche à conditions n'est pas prise sans réponse.
	{
		VERIF(b.TrouverOuCharger("/Global/TrainingPlayer", "Act/TrainingPlayer.act"));
		const CActionNode *arbre = b.Trouver("/Global/TrainingPlayer");
		const CActionNode *pris = b.Choisir(arbre);
		VERIF(pris != nil && pris->genre == 'l' && pris == b.Trouver("/Global/TrainingPlayer/Default/Default"));
		VERIF(b.Joue(pris, "/Global/TrainingPlayer/Default", true) && !b.Joue(pris, "/Global/TrainingPlayer/Default", false));
		const CActionNode *attaques = b.Trouver("/Global/TrainingPlayer/Attacks");
		VERIF(attaques != nil && !attaques->conditions.empty() && b.Choisir(attaques) == nil);
		VERIF(b.Choisir(attaques, [](const CActionNode &, void *){ return true; }) != nil);
		VERIF(b.Choisir(nil) == nil);
		// Les données du fichier restent chargées : la ConditionGroup de .../Locomotion/animations se lit encore.
		const CActionNode *anim = b.Trouver("/Global/TrainingPlayer/Default/Locomotion/animations");
		const CActionTreeFile *f = anim ? b.Fichier(*anim) : nil;
		std::vector<int32> groupe;
		VERIF(f != nil && anim->conditions.size() == 1 && f->TypeCondition(anim->conditions[0]) == ActionHash("ConditionGroup")
		      && f->GroupeDe(anim->conditions[0], groupe) && groupe.size() == 11);
		// Les conditions jugées : sans arme (WeaponEquiped « unarmed » vrai, les autres armes fausses), rien demandé
		// (ActionRequest faux), le reste faux. Le groupe d'animations (six armes en OR : bat, twobyfour, polo, cricket,
		// W_Crutch, leadpipe) est faux à mains nues ; les attaques s'ouvrent (WeaponEquiped « unarmed ») mais aucune
		// branche n'aboutit sans demande.
		struct : CJugeAction {
			int Juger(const CActionTreeFile &f, int32 d, uint32 type, const uint32 *) const override {
				if(type == ActionHash("WeaponEquiped")) return f.ChaineCitee((uint32)d + 8) == "unarmed";
				return 0;
			}
		} sansArme;
		VERIF(anim && b.Juger(*anim, sansArme) == 0);
		struct : CJugeAction {
			int Juger(const CActionTreeFile &f, int32 d, uint32 type, const uint32 *) const override {
				return type == ActionHash("WeaponEquiped") && f.ChaineCitee((uint32)d + 8) == "leadpipe";
			}
		} tuyau;
		VERIF(anim && b.Juger(*anim, tuyau) == 1);   // le dernier OR du groupe
		VERIF(b.Juger(*attaques, sansArme) == 1 && b.Choisir(attaques, sansArme) == nil);
		VERIF(b.Choisir(arbre, sansArme) == pris);
		struct : CJugeAction { int Juger(const CActionTreeFile &, int32, uint32, const uint32 *) const override { return -1; } } inconnu;
		VERIF(b.Juger(*anim, inconnu) == -1 && b.Choisir(attaques, inconnu) == nil);
		printf("  descente : /Global/TrainingPlayer → %s/%s\n", "Default", pris ? pris->nom.c_str() : "?");
	}
	// FUN_0072ad00 / FUN_0072ae00 : AreaTransitionButes (DAT_020c8910) dans Globals.cat ; +0x18 la durée du fondu
	// au noir d'un changement de zone (0,65 s), +0x1c celle du retour (0,5 s), les défauts de CameraFade(-1).
	{
		CActionTrack t;
		VERIF(CActionBank::Butes("AreaTransitionButes", t));
		VERIF(CActionBank::ButesReel(t, 24) == 0.65f && CActionBank::ButesReel(t, 28) == 0.5f);
		CActionTrack c;
		VERIF(CActionBank::Butes("TimeCycleButes", c) && c.Mot(24) == 5);   // un entier : le diviseur de GetCurrentDay(true)
		VERIF(!CActionBank::Butes("PasDeTellesButes", c));
		printf("  AreaTransitionButes : fondu au noir %.2f s, retour %.2f s\n", CActionBank::ButesReel(t, 24), CActionBank::ButesReel(t, 28));
	}
	// FUN_005f6f20 : les arbres globaux du démarrage ; HitTree (la parade que 1_02 attend) en fait partie.
	{
		CActionBank g;
		int n = g.ChargerGlobaux();
		VERIF(n == 68 && g.EstCharge("Act/HitTree.act") && g.EstCharge("Act/Weapons/EdgarShield.act"));
		VERIF(g.Trouver("/Global/HitTree/Standing/BlockHits") != nil && g.Trouver("/Global/Player") != nil);
		VERIF(g.ChargerGlobaux() == 68 && g.fichiers.size() == 68);   // déjà chargés : rien n'est relu
		// PedMePlaying : par le nom, le nœud joué ou un de ses parents (FUN_005f46a0 / FUN_005f4f30).
		VERIF(g.Charger("Act/Globals/GlobalLoco.act"));   // le joueur y renvoie par une référence (Player.cat)
		const CActionNode *bloc = g.Trouver("/Global/GlobalLoco/Locomotion/Combat/0_BLOCK_0");
		VERIF(bloc != nil && g.JoueNom(bloc, "0_BLOCK_0") && g.JoueNom(bloc, "combat") && g.JoueNom(bloc, "GlobalLoco"));
		VERIF(!g.JoueNom(bloc, "BlockHits") && !g.JoueNom(nil, "0_BLOCK_0"));
		printf("  %d arbres globaux chargés, %zu banques sous /Global, /Global/HitTree/Standing/BlockHits trouvé\n", n, g.Global()->enfants.size());
	}
	printf("test_actionbank : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
