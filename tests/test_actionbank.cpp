// L'arbre d'actions global et le chemin d'un nœud (src/core/ActionBank) : 1_01.cat greffé sous /Global,
// les chemins que 1_01.lua donne à PedSetActionNode, « .. », la casse, le chargement à la demande
// de Player.act (FUN_004849a0), deux fichiers sous la même racine.
//   BULLY_DATA=<racine du jeu> build/tests/test_actionbank
#include <cmath>
#include "../src/core/ActionBank.h"
#include "../src/core/ActionController.h"
#include "../src/core/Opportunite.h"
#include "../src/core/ConditionListe.h"
#include <cstdio>
#include <cstring>
#include <string>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

// La première Opportunity de l'arbre qui vise ce chemin, et son nœud.
static bool
ChercherOpportunite(const CActionBank &b, const CActionNode &n, const char *chemin, COpportunite &o)
{
	for(const COpportunite &x : COpportunite::DuNoeud(b, n)) if(x.chemin == chemin){ o = x; return true; }
	for(const CActionNode &e : n.enfants) if(ChercherOpportunite(b, e, chemin, o)) return true;
	return false;
}

// Le chemin du nœud joué, vu avec la clé du contrôleur.
static std::string
CheminVu(const CActionBank &b, const CActionController &c)
{
	uint32 avant = b.cle;
	b.cle = c.Cle();
	std::string r = b.CheminDe(c.joue);
	b.cle = avant;
	return r;
}

struct CJugeAction_Rien : CJugeAction { int Juger(const CActionTreeFile &, int32, uint32, const uint32 *) const override { return 0; } };

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
	VERIF(talking != nil && talking->genre == 'b' && talking->hachage == ActionHash("Talking") && talking->nom == "Talking" && talking->enfants.size() == 1);
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
	// Player.act monte des références ('r', GlobalLoco.act…) : leurs banques s'ajoutent sous /Global (FUN_005f6960).
	VERIF(b.EstCharge("Act/Player.act") && b.fichiers.size() == 2 && b.Global()->enfants.size() > 2);
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
		// Les chemins relatifs des pistes (FUN_005f59b0 depuis le nœud qui porte la piste) : « ../Locomotion »
		// (SPAWN de DEFAULT), « ../ », « ../../Attacks », « . », un segment absent.
		{
			const CActionNode *feuille = b.Trouver("/Global/TrainingPlayer/Default/Default");
			const CActionNode *banque = b.Trouver("/Global/TrainingPlayer/Default");
			VERIF(feuille && banque && b.Lignee(feuille).size() == 5 && b.Lignee(feuille).back() == feuille && b.Lignee(nil).empty());
			VERIF(b.Relatif(feuille, "../Locomotion") == b.Trouver("/Global/TrainingPlayer/Default/Locomotion"));
			VERIF(b.Relatif(feuille, "../") == banque && b.Relatif(feuille, ".") == feuille && b.Relatif(feuille, "./") == feuille);
			VERIF(b.Relatif(feuille, "../../Attacks") == b.Trouver("/Global/TrainingPlayer/Attacks"));
			VERIF(b.Relatif(feuille, "../Absent") == nil && b.Relatif(feuille, "/Global/TrainingPlayer") == b.Trouver("/Global/TrainingPlayer"));
		}
		// Les pistes Opportunity (FUN_005f7d70) : GroundStomp2 se ferme et bascule à 0,367 s ; l'occasion d'attaquer de
		// DEFAULT vise ../../Attacks avec la priorité 1 ; leur nœud visé se résout depuis le nœud qui les porte.
		{
			COpportunite o;
			const CActionNode *tp = b.Trouver("/Global/TrainingPlayer");
			VERIF(tp && ChercherOpportunite(b, *tp, "./GroundStomp2", o));
			VERIF(o.porteur && o.fermeture > 0.36f && o.fermeture < 0.37f && o.bascule > 0.36f && o.bascule < 0.37f && o.priorite == 0 && o.periode == 0 && !o.soiMeme);
			VERIF(o.porteur && b.Relatif(o.porteur, o.chemin.c_str()) != nil && !o.prise && o.cible == nil);
			VERIF(tp && ChercherOpportunite(b, *tp, "../../Attacks", o) && o.priorite == 1 && o.fermeture < 0.0f && o.bascule == 0.0f);
			VERIF(o.porteur && b.Relatif(o.porteur, o.chemin.c_str()) == b.Trouver("/Global/TrainingPlayer/Attacks"));
		}
		// FUN_00465910 : une période < 2 passe toujours ; sinon le plus petit bit posé de (graine + image) mod 0xffff + 1.
		// FUN_005fb750 : les six comparateurs (≈ à 1e-5 près) ; FUN_005fd650 : l'état d'un bouton selon le mode.
		VERIF(ComparerCondition(1.0f, 1.000001f, 0) && !ComparerCondition(1.0f, 1.1f, 0) && ComparerCondition(1.0f, 1.1f, 1));
		VERIF(ComparerCondition(0.1f, 0.2f, 2) && !ComparerCondition(0.2f, 0.2f, 2) && ComparerCondition(0.3f, 0.2f, 3));
		VERIF(ComparerCondition(0.2f, 0.2f, 4) && ComparerCondition(0.2f, 0.2f, 5) && !ComparerCondition(0.1f, 0.2f, 5) && !ComparerCondition(0.1f, 0.2f, 9));
		VERIF(EtatBoutonCondition(1, 1) && !EtatBoutonCondition(2, 1) && EtatBoutonCondition(2, 2) && EtatBoutonCondition(0, 3) && !EtatBoutonCondition(1, 3));
		// rand() du CRT : graine 1 → 41, puis 18467, 6334 (les premières valeurs connues de Visual C++).
		{
			uint32 g = 1;
			int r1 = RandCrt(g), r2 = RandCrt(g), r3 = RandCrt(g);
			VERIF(r1 == 41 && r2 == 18467 && r3 == 6334);
		}
		VERIF(TirageOpportunite(0, 0, 7) && TirageOpportunite(1, 5, 9));
		VERIF(TirageOpportunite(2, 0, 1) && !TirageOpportunite(2, 0, 2) && TirageOpportunite(4, 0, 3) && !TirageOpportunite(4, 0, 5));
		VERIF(TirageOpportunite(3, 0, 3) && !TirageOpportunite(3, 0, 1));   // 3 : il faut le bit 4, une chance sur quatre
		{
			int n = 0;
			for(uint32 i = 0; i < 4096; i++) n += TirageOpportunite(8, 77, i);
			VERIF(n == 512);
		}
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
		// Le contrôleur : posé sur TrainingPlayer, il joue Default/Default ; un nœud joué à durée finie (la parade de
		// HitTree) se termine, et le contrôleur redescend dans l'arbre posé.
		CActionController c;
		VERIF(c.Poser(b, arbre, sansArme, 10.0f) && c.joue == pris);
		VERIF(b.CheminDe(pris) == "/Global/TrainingPlayer/Default/Default");   // noms hachés appris des chemins demandés
		// Le premier nœud de TrainingPlayer dont une piste a une durée de vie finie (un coup, une provocation).
		const CActionNode *cible = nil; float d = -1.0f;
		std::vector<const CActionNode *> pile(1, arbre);
		while(!pile.empty() && cible == nil){
			const CActionNode *x = pile.back(); pile.pop_back();
			if(x != pris && (d = CActionController::Duree(b, *x)) > 0.0f){ cible = x; break; }
			for(const CActionNode &e : x->enfants) pile.push_back(&e);
		}
		printf("  contrôleur : %s dure %.2f s\n", cible ? cible->nom.c_str() : "?", d);
		VERIF(cible != nil && d > 0.0f);
		if(cible){
			c.Jouer(b, cible, 20.0f);
			VERIF(!c.Avancer(b, nil, sansArme, 20.0f + d * 0.5f) || c.joue != pris);
			c.Jouer(b, cible, 20.0f);
			VERIF(c.Avancer(b, nil, sansArme, 20.0f + d + 0.01f) && (c.joue == pris || c.joue != cible));
			VERIF(c.Temps(c.debut + 0.25f) == 0.25f);
			// Entrer dans une banque descend (FUN_005f5490) ; dans un nœud jouable, le joue.
			VERIF(c.Entrer(b, arbre, sansArme, 30.0f) && c.joue == pris);
			VERIF(c.Entrer(b, cible, sansArme, 31.0f) && c.joue == cible);
		}
		struct : CJugeAction { int Juger(const CActionTreeFile &, int32, uint32, const uint32 *) const override { return -1; } } inconnu;
		VERIF(b.Juger(*anim, inconnu) == -1 && b.Choisir(attaques, inconnu) == nil);
		// Les Sequence (FUN_005f7030) : à la fin d'un nœud à durée finie, la descente dans le nœud de sa Sequence.
		{
			struct : CJugeAction { int Juger(const CActionTreeFile &, int32, uint32, const uint32 *) const override { return 1; } } toutVrai;
			struct Trouve { const CActionBank &b; const CJugeAction &j; const CActionNode *n = nil; const CActionNode *suite = nil;
				bool Dans(const CActionNode &x){
					if(x.genre != 'b' && CActionController::Duree(b, x) > 0.0f){
						CActionController c;
						c.Jouer(b, &x, 0.0f);
						for(const std::string &ch : c.suites){
							const CActionNode *d = b.Choisir(b.Relatif(&x, ch.c_str()), j);
							if(d && d != &x && c.occasions.empty()){ n = &x; suite = d; return true; }
						}
					}
					for(const CActionNode &e : x.enfants) if(Dans(e)) return true;
					return false;
				} } t{b, toutVrai, nil, nil};
			VERIF(t.Dans(*b.Global()) && t.n != nil);
			if(t.n){
				CActionController c;
				c.Jouer(b, t.n, 0.0f);
				float d = CActionController::Duree(b, *t.n);
				VERIF(!c.Avancer(b, nil, toutVrai, d * 0.5f) && c.joue == t.n);
				VERIF(c.Avancer(b, nil, toutVrai, d + 0.01f) && c.joue == t.suite);
				printf("  Sequence : %s → %s à %.2f s\n", b.CheminDe(t.n).c_str(), b.CheminDe(t.suite).c_str(), d);
			}
		}
		// Les Opportunity dans le contrôleur (FUN_005f7d70, FUN_005f7440, FUN_005f3bc0) : dans TrainingPlayer, la
		// première Opportunity à bascule tardive (> 0) dont le nœud visé aboutit quand tout passe. Prise tout de suite,
		// elle fait passer à sa bascule, pas avant ; avec des conditions fausses, elle ne fait rien.
		{
			struct : CJugeAction { int Juger(const CActionTreeFile &, int32, uint32, const uint32 *) const override { return 1; } } toutVrai;
			struct Trouve { const CActionBank &b; const CJugeAction &j; COpportunite o; const CActionNode *suite = nil;
				bool Dans(const CActionNode &n){
					for(const COpportunite &x : COpportunite::DuNoeud(b, n)){
						if(x.chemin.empty() || x.bascule <= 0.0f || x.entree != 0.0f) continue;
						const CActionNode *d = b.Choisir(b.Relatif(x.porteur, x.chemin.c_str()), j);
						if(d && d != x.porteur){ o = x; suite = d; return true; }
					}
					for(const CActionNode &e : n.enfants) if(Dans(e)) return true;
					return false;
				} } t{b, toutVrai, COpportunite(), nil};
			const CActionNode *tp = b.Trouver("/Global/TrainingPlayer");
			VERIF(tp && t.Dans(*tp));
			if(t.suite){
				CActionController c;
				c.Jouer(b, t.o.porteur, 100.0f);
				VERIF(!c.occasions.empty());
				VERIF(!c.Avancer(b, nil, toutVrai, 100.0f + t.o.bascule * 0.5f) && c.joue == t.o.porteur);
				VERIF(c.Avancer(b, nil, toutVrai, 100.0f + t.o.bascule + 0.01f) && c.joue == t.suite);
				printf("  Opportunity : %s → %s à %.3f s\n", t.o.chemin.c_str(), b.CheminDe(t.suite).c_str(), t.o.bascule);
				CActionController d;
				d.Jouer(b, t.o.porteur, 0.0f);
				VERIF(!d.Avancer(b, nil, sansArme, t.o.bascule * 0.5f) && !d.Avancer(b, nil, sansArme, t.o.bascule + 0.01f) && d.joue == t.o.porteur);
			}
		}

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
		// Les conditions propres d'une Opportunity (+0x3c) : le mot du fichier vaut 0, mais la seconde table de renvois le
		// cite et renvoie à une liste de conditions, comme pour une ConditionGroup. Le relevé compte aussi les Opportunity
		// et les loquets.
		{
			struct Releve { const CActionBank &b; int occasions = 0, loquets = 0, avecConditions = 0, periodes = 0, conditionsLues = 0;
				void Dans(const CActionNode &x){
					for(const COpportunite &o : COpportunite::DuNoeud(b, x)){
						occasions++; loquets += o.loquet; avecConditions += !o.listeConditions.empty(); periodes += o.periode > 1;
						bool lues = !o.listeConditions.empty();
						for(int32 d : o.listeConditions) lues = lues && b.Fichier(x)->TypeCondition(d) != 0;
						conditionsLues += lues;
					}
					for(const CActionNode &e : x.enfants) Dans(e);
				} } r{g, 0, 0, 0, 0, 0};
			r.Dans(*g.Global());
			VERIF(r.occasions > 1000 && r.loquets > 0 && r.avecConditions > 0 && r.conditionsLues == r.avecConditions);
			printf("  %d Opportunity dans les arbres du démarrage (%d loquets, %d à période, %d à conditions propres)\n", r.occasions, r.loquets, r.periodes, r.avecConditions);
			// L'attente du chargement d'une humiliation (Humiliate_Init/LoadWait/LoadSpawn) n'abandonne (../../ABORT)
			// qu'après 6 s : ActionNodePlayTime 6, comparateur 5.
			const CActionNode *attente = g.Trouver("/Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init/LoadWait/LoadSpawn");
			bool abandon = false;
			if(attente)
				for(const COpportunite &o : COpportunite::DuNoeud(g, *attente)){
					if(o.chemin != "../../ABORT" || o.listeConditions.size() != 1) continue;
					uint32 type, mots[6];
					float s6 = 0.0f;
					if(g.Fichier(*attente)->Condition(o.listeConditions[0], type, mots)) memcpy(&s6, &mots[0], 4);
					abandon = type == ActionHash("ActionNodePlayTime") && s6 == 6.0f && mots[1] == 5;
				}
			VERIF(abandon);
		}
		// PlayOnTarget (FUN_006615f0) : dans les prises, un nœud fait jouer à la cible le nœud visé, une fois, à son entrée.
		{
			struct : CJugeAction { int Juger(const CActionTreeFile &, int32, uint32, const uint32 *) const override { return 0; } } rien;
			struct Trouve { const CActionBank &b; const CActionNode *n = nil, *visee = nil; float entree = 0.0f;
				bool Dans(const CActionNode &x){
					CActionController c;
					c.Jouer(b, &x, 0.0f);
					for(const CActionController::SurCible &sc : c.surCible)
						if(const CActionNode *v = b.Relatif(&x, sc.chemin.c_str())){ n = &x; visee = v; entree = sc.entree; return true; }
					for(const CActionNode &e : x.enfants) if(Dans(e)) return true;
					return false;
				} } t{g, nil, nil, 0.0f};
			const CActionNode *prises = g.Trouver("/Global/Actions/Grapples");
			VERIF(prises && t.Dans(*prises));
			if(t.n){
				CActionController c;
				c.Jouer(g, t.n, 0.0f);
				c.Avancer(g, nil, rien, t.entree);
				VERIF(c.pourCible.size() == 1 && c.pourCible[0] == t.visee);
				c.Avancer(g, nil, rien, t.entree + 0.1f);
				VERIF(c.pourCible.empty());
				printf("  PlayOnTarget : %s → la cible joue %s\n", g.CheminDe(t.n).c_str(), g.CheminDe(t.visee).c_str());
			}
		}
		// OpportunityRandomLatch (FUN_005f7510) : le repos de GlobalLoco tire sa bascule entre 2 et 5 s.
		{
			COpportunite o;
			CActionBank gl;
			gl.TrouverOuCharger("/Global/GlobalLoco", "Act/Globals/GlobalLoco.act");
			const CActionNode *loco = gl.Trouver("/Global/GlobalLoco");
			VERIF(loco && ChercherOpportunite(gl, *loco, "./Opps", o) && o.loquet && o.loquetMin == 2.0f && o.loquetMax == 5.0f);
			if(o.porteur){
				CActionController c;
				for(int i = 0; i < 20; i++){
					c.Jouer(gl, o.porteur, 0.0f);
					bool vu = false;
					for(const COpportunite &x : c.occasions) if(x.loquet){ vu = true; VERIF(x.bascule >= 2.0f && x.bascule <= 5.0f); }
					VERIF(vu);
				}
			}
		}
		VERIF(g.ChargerGlobaux() == 68 && g.fichiers.size() == 68);   // déjà chargés : rien n'est relu
		// PedMePlaying : par le nom, le nœud joué ou un de ses parents (FUN_005f46a0 / FUN_005f4f30).
		VERIF(g.Charger("Act/Globals/GlobalLoco.act"));   // le joueur y renvoie par une référence (Player.cat)
		const CActionNode *bloc = g.Trouver("/Global/GlobalLoco/Locomotion/Combat/0_BLOCK_0");
		VERIF(bloc != nil && g.JoueNom(bloc, "0_BLOCK_0") && g.JoueNom(bloc, "combat") && g.JoueNom(bloc, "GlobalLoco"));
		VERIF(!g.JoueNom(bloc, "BlockHits") && !g.JoueNom(nil, "0_BLOCK_0"));
		VERIF(g.JoueHachage(bloc, ActionHash("Combat")) && !g.JoueHachage(bloc, ActionHash("RollAway")));
		// Les inclusions : GlobalActions inclut Grapples.act sous « actions », comme les scripts le désignent.
		VERIF(g.Trouver("/Global/Actions/Grapples/Front/Grapples/GrappleAttempt/GrappleSuccess/GIVE") != nil);
		VERIF(g.Trouver("/Global/Actions/Grapples/Front/Grapples/GrappleMoves/GrappleStrikes/HitC/Charge") != nil);
		// Les pistes Collision (FUN_005f9e70) : le coup de pied au sol de TrainingPlayer porte la sienne par un Execute
		// (« ./Collision », la feuille « ped ») : GROUNDHITHEAVY entre 0,0833 et 0,217 s, 8 de dégâts.
		{
			CActionBank tp;
			VERIF(tp.TrouverOuCharger("/Global/TrainingPlayer", "Act/TrainingPlayer.act"));
			const CActionNode *lourd = tp.Trouver("/Global/TrainingPlayer/Attacks/GroundAttacks/GroundAttacks/Strikes/HeavyAttacks");
			const CActionNode *coup = lourd && !lourd->enfants.empty() && !lourd->enfants[0].enfants.empty() ? &lourd->enfants[0].enfants[0] : nil;
			VERIF(coup != nil);
			if(coup){
				CActionController c;
				c.Jouer(tp, coup, 10.0f);
				bool vu = false;
				for(const CActionController::Contact &k : c.contacts)
					if(k.drapeau == ActionHash("GROUNDHITHEAVY") && fabsf(k.debut - 0.0833f) < 1e-3f && fabsf(k.fin - 0.217f) < 1e-3f && k.degats == 8.0f) vu = true;
				VERIF(vu);
				c.Avancer(tp, nil, CJugeAction_Rien(), 10.05f);
				VERIF(c.touches.empty());
				c.Avancer(tp, nil, CJugeAction_Rien(), 10.1f);
				VERIF(c.touches.size() == 1);
				c.Avancer(tp, nil, CJugeAction_Rien(), 10.15f);
				VERIF(c.touches.empty());   // un coup ne porte qu'une fois
				printf("  Collision : %s → GROUNDHITHEAVY entre 0,083 et 0,217 s\n", tp.CheminDe(coup).c_str());
			}
		}
		// La réaction d'un piéton à terre (docs/cat.md, « Les coups ») : sous GroundAndWallHits/On_Ground, BellyUp prend
		// GROUNDHITHEAVY si ConditionFlag (+4) est le drapeau du coup, sinon « default ».
		{
			struct JugeDrapeau : CJugeAction {
				uint32 drapeau;
				explicit JugeDrapeau(uint32 d) : drapeau(d) {}
				int Juger(const CActionTreeFile &, int32, uint32 type, const uint32 mots[6]) const override {
					if(type == ActionHash("ConditionFlag")) return (mots[0] & 0x7fffffff) == drapeau;
					return -1;
				}
			};
			const CActionNode *sol = g.Trouver("/Global/HitTree/GroundAndWallHits/On_Ground");
			VERIF(sol != nil);
			if(sol){
				const CActionNode *lourd = g.Choisir(sol, JugeDrapeau(ActionHash("GROUNDHITHEAVY")));
				const CActionNode *autre = g.Choisir(sol, JugeDrapeau(ActionHash("GROUNDHITLIGHT")));
				VERIF(lourd && g.Joue(lourd, "/Global/HitTree/GroundAndWallHits/On_Ground/BellyUp/GroundHitHeavy/GroundHitHeavy", false));
				VERIF(autre && g.JoueNom(autre, "default") && !g.JoueNom(autre, "GROUNDHITHEAVY"));
				printf("  à terre : GROUNDHITHEAVY → %s\n", g.CheminDe(lourd).c_str());
			}
			// Une Sequence peut sortir de l'arbre posé (FUN_005f7030) : PushFwd/RCV finit, la cible tombe dans HitTree.
			const CActionNode *rcv = g.Trouver("/Global/Actions/Grapples/Front/Grapples/GrappleMoves/DirectionalPush/PushFwd/RCV");
			const CActionNode *tuto = g.Trouver("/Global/Actions");
			VERIF(rcv != nil && tuto != nil);
			if(rcv && tuto){
				CActionController c;
				c.arbre = tuto;
				c.Jouer(g, rcv, 0.0f);
				float d = CActionController::Duree(g, *rcv);
				VERIF(d > 0.5f && d < 1.5f);
				VERIF(c.Avancer(g, nil, JugeDrapeau(0), d + 0.05f));
				VERIF(g.JoueNom(c.joue, "OnGroundBounce"));
			}
		}
		// Les pistes d'un nœud exécuté rejoignent celles du contrôleur (FUN_005f7070, FUN_005f3e20) : un coup reçu debout
		// (HEADJAB) fait jouer une feuille de HitTree qui exécute ../HitBase, dont la piste DamageHandle (facteur 1) et
		// l'Opportunity vers KnockOuts arrivent au contrôleur ; la DamageHandle se lance à l'entrée (FUN_0060fc70).
		{
			struct JugeCoup : CJugeAction {
				int Juger(const CActionTreeFile &, int32, uint32 type, const uint32 mots[6]) const override {
					if(type == ActionHash("ConditionFlag")) return (mots[0] & 0x7fffffff) == ActionHash("HEADJAB");
					if(type == ActionHash("HitLocation") || type == ActionHash("HitTime") || type == ActionHash("stat")) return 1;
					if(type == ActionHash("MePlaying") || type == ActionHash("IsPlayer") || type == ActionHash("PedFightingSystem")) return 0;
					return -1;
				}
			};
			const CActionNode *coups = g.Trouver("/Global/HitTree");
			VERIF(coups != nil);
			if(coups){
				CActionController c;
				JugeCoup juge;
				VERIF(c.Entrer(g, coups, juge, 0.0f));
				printf("  coup reçu debout (HEADJAB) : %s, la DamageHandle et l'Opportunity de ../HitBase (exécuté)\n", g.CheminDe(c.joue).c_str());
				const CActionNode *base = g.Trouver("/Global/HitTree/Standing/Base/HitBase");
				VERIF(base != nil && c.degats.size() == 1 && c.degats[0].porteur == base && c.degats[0].facteur == 1.0f && c.degats[0].effet);
				bool ko = false;
				for(const COpportunite &o : c.occasions) if(o.porteur == base && o.chemin == "../../../KnockOuts") ko = true;
				VERIF(ko);
				VERIF(c.blessures.empty());
				c.Avancer(g, nil, juge, 0.01f);
				VERIF(c.blessures.size() == 1);
				c.Avancer(g, nil, juge, 0.02f);
				VERIF(c.blessures.empty());   // une fois, au lancement
				// La feuille finit avec sa HitAnimation (FUN_00617040 → FUN_0060ae00) ; la Sequence du nœud exécuté
				// (HitBaseStanding : ../../PostHit/Standing) est gardée pour la fin.
				// (les durées de MAINPED.HXD ne sont pas chargées ici : celle de l'animation de la feuille est posée à 0,8 s)
				if(const CActionTreeFile *f = g.Fichier(*c.joue))
					for(int32 dp : c.joue->pistes){
						CActionTrack p;
						if(f->Piste(dp, p) && p.type == ActionHash("HitAnimation")) g.dureesAnimations[p.Mot(24) & 0x7fffffff] = 0.8f;
					}
				float d = CActionController::Duree(g, *c.joue);
				VERIF(fabsf(d - 0.8f) < 1e-4f && !c.suitesExecutees.empty());
				printf("    sa HitAnimation finit à %.2f s, %zu Sequence venue d'un nœud exécuté\n", d, c.suitesExecutees.size());
			}
		}
		// Une Opportunity hors de l'arbre posé mais dans l'arbre global de son porteur fait passer : un piéton tombé dans
		// HitTree/Standing/PostHit/BellyUp/On_Ground se relève (../BellyUpGetUp) même si son arbre est Fight_Tutorial.
		{
			const CActionNode *sol = g.Trouver("/Global/HitTree/Standing/PostHit/BellyUp/On_Ground");
			const CActionNode *ailleurs = g.Trouver("/Global/Actions");
			VERIF(sol != nil && ailleurs != nil);
			if(sol && ailleurs){
				CActionController c;
				c.arbre = ailleurs;
				c.Jouer(g, sol, 0.0f);
				// Son Opportunity ne passe que si ActionNodePlayTimePast (FUN_005fbfc0) est vraie : sans piste Damage à seuil,
				// le seuil vaut -1 et elle l'est.
				struct : CJugeAction { int Juger(const CActionTreeFile &, int32, uint32 type, const uint32 *) const override {
					return type == ActionHash("ActionNodePlayTimePast"); } } passe;
				bool releve = false;
				for(int i = 1; i < 10 && !releve; i++){
					c.Avancer(g, nil, passe, i / 30.0f);
					releve = g.JoueNom(c.joue, "BellyUpGetUp");
				}
				VERIF(releve);
			}
		}
		// Les pistes de parole (FUN_006195a0, FUN_00619950) : l'attente d'une humiliation (Humiliate_Me/…/Success/Load)
		// retient sa ligne (HUMILIATION_ANIM, la ligne de la piste, tenue 2) ; RCV la lâche par SpeechStartTrack.
		{
			const CActionNode *charge = g.Trouver("/Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init/Humiliate_Me/5-8_NoogieSpit/Success/Load");
			const CActionNode *rcv = g.Trouver("/Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init/Humiliate_Me/5-8_NoogieSpit/Success/RCV");
			VERIF(charge != nil && rcv != nil);
			if(charge && rcv){
				CActionController c;
				c.Jouer(g, charge, 0.0f);
				c.Avancer(g, nil, CJugeAction_Rien(), 0.0f);
				bool tenue = false;
				for(const CActionController::Parole &p : c.parolesLancees)
					tenue = tenue || (p.evenement == 165 && p.ligne >= 0 && p.tenue == 2);
				VERIF(tenue);
				c.parolesLancees.clear();
				c.Jouer(g, rcv, 1.0f);
				for(int i = 0; i <= 30; i++) c.Avancer(g, nil, CJugeAction_Rien(), 1.0f + i / 30.0f);
				VERIF(c.departsLances == 1);
				printf("  parole : Load retient HUMILIATION_ANIM, RCV la lâche (SpeechStartTrack)\n");
			}
		}
		// FaceTarget et SeekEntityPosition, y compris dans un nœud lancé par spawn : le joueur qui attend sa victime
		// (WaitForVictim) la rejoint (spawn ./SeekEntity : SeekEntityPosition vers le partenaire de prise, arrivée à
		// 0,75 m) ; la victime qui attend le chargement (Load, qui exécute LoadWait/LoadSpawn) se tourne vers lui.
		{
			const char *init = "/Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init";
			const CActionNode *attend = g.Trouver((std::string(init) + "/WaitForVictim").c_str());
			const CActionNode *charge = g.Trouver((std::string(init) + "/Humiliate_Me/5-8_NoogieSpit/Success/Load").c_str());
			VERIF(attend != nil && charge != nil);
			if(attend && charge){
				CActionController c;
				c.Jouer(g, attend, 0.0f);
				c.Avancer(g, nil, CJugeAction_Rien(), 0.0f);
				VERIF(c.poursuitesActives.size() == 1 && c.poursuitesActives[0].genre == 1 && fabsf(c.poursuitesActives[0].arrivee - 0.75f) < 1e-4f);
				CActionController v;
				v.Jouer(g, charge, 0.0f);
				v.Avancer(g, nil, CJugeAction_Rien(), 0.0f);
				bool tourne = false;
				for(const CActionController::Regard &r : v.regardsActifs) tourne = tourne || (r.genre == 1 && r.decalage == 0.0f);
				VERIF(tourne);
				printf("  WaitForVictim rejoint sa victime (SeekEntityPosition, 0,75 m), Load la tourne vers lui (FaceTarget)\n");
			}
		}
		// Un nœud qui n'a que des Execute (durée de vie -1, FUN_005f70f0) et des Sequence, et dont l'Execute ne se résout
		// pas, finit tout de suite : la charge sans le drapeau 70 (HitC/Charge/GIVE/#542875ee) passe à ../Give.
		{
			const CActionNode *give = g.Trouver("/Global/Actions/Grapples/Front/Grapples/GrappleMoves/GrappleStrikes/HitC/Charge/GIVE");
			const CActionNode *transition = nil;
			if(give) for(const CActionNode &e : give->enfants) if(e.hachage == 0x542875ee) transition = &e;
			VERIF(transition != nil);
			if(transition){
				CActionController c;
				c.Jouer(g, transition, 0.0f);
				VERIF(c.fin == 0.0f);
				c.Avancer(g, nil, CJugeAction_Rien(), 0.0f);
				VERIF(c.joue && c.joue->genre == 'l' && g.JoueNom(c.joue, "GIVE") && c.joue != transition);
				printf("  charge sans le drapeau 70 : #542875ee finit tout de suite, puis %s\n", g.CheminDe(c.joue).c_str());
			}
		}
		// L'humiliation : SetTargetAsGrappleTarget (FUN_0062a580) envoie le piéton dans ../WaitForVictim et sa cible dans
		// ../Humiliate_Me ; la fuite d'une victime de mission (HumiliateFlee, IsMissionSpecific) lui ôte 500 points
		// (SetHealth, FUN_00627500) et son Opportunity sans bascule (-1) ne dépose sa demande qu'à la fin du nœud
		// (FUN_005f7440).
		{
			const CActionNode *init = g.Trouver("/Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init");
			const CActionNode *attente = g.Trouver("/Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init/WaitForVictim");
			const CActionNode *victime = g.Trouver("/Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init/Humiliate_Me");
			VERIF(init && attente && victime);
			if(init && attente && victime){
				CActionController c;
				VERIF(c.Entrer(g, init, CJugeAction_Rien(), 0.0f) && c.prisesCible.size() == 1);
				c.Avancer(g, nil, CJugeAction_Rien(), 0.01f);
				VERIF(c.prises.size() == 1 && c.prises[0].soi == attente && c.prises[0].cible == victime);
			}
			struct JugeMission : CJugeAction {
				int Juger(const CActionTreeFile &, int32, uint32 type, const uint32 *) const override {
					return type == ActionHash("IsMissionSpecific") ? 1 : 0;
				}
			};
			const CActionNode *fuite = g.Trouver("/Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init/HumiliateFlee");
			VERIF(fuite != nil);
			if(fuite){
				const CActionNode *n = g.Choisir(fuite, JugeMission());
				VERIF(n && n->genre == 'n');
				if(n){
					CActionController c;
					c.Jouer(g, n, 0.0f);
					VERIF(c.santes.size() == 1 && c.santes[0].mode == 2 && c.santes[0].valeur == 500.0f);
					c.Avancer(g, nil, JugeMission(), 0.01f);
					VERIF(c.santesLancees.size() == 1);
					// Aucune de ses pistes ne retient le nœud (FUN_005f4230) : il finit tout de suite, et son Opportunity prise
					// mène à RunStart.
					VERIF(c.joue && (c.joue->hachage & 0x7fffffff) == ActionHash("RunStart"));
					printf("  HumiliateFlee (victime de mission) : %s, SetHealth ôte %g, puis %s\n", g.CheminDe(n).c_str(),
					       c.santesLancees.empty() ? 0.0f : c.santesLancees[0].valeur, g.CheminDe(c.joue).c_str());
				}
			}
		}
		// SetPedFlags (FUN_0060f8e0) : WaitForVictim pose le drapeau 123 à son entrée et le remet à 0 à son arrêt (entrée -1,
		// lancée par FUN_005f3fb0 quand le nœud change).
		{
			const CActionNode *give = g.Trouver("/Global/Actions/Grapples/Front/Grapples/GrappleMoves/GrappleStrikes/HitC/Charge/GIVE");
			const CActionNode *attente = g.Trouver("/Global/Player/Social_Actions/HarassMoves/Humiliations/Humiliate_Init/WaitForVictim");
			if(attente){
				CActionController c;
				c.Jouer(g, attente, 0.0f);
				c.Avancer(g, nil, CJugeAction_Rien(), 0.01f);
				bool debut = false;
				for(const CActionController::Drapeau &d : c.drapeauxPoses) if(d.numero == 123 && d.valeur == 1) debut = true;
				size_t avant = debut ? 1 : 0;
				c.drapeauxPoses.clear();
				c.Jouer(g, give, 0.02f);                          // le nœud change : SetPedFlags 123 à 0 (entrée -1) se lance
				bool fin = false;
				for(const CActionController::Drapeau &d : c.drapeauxPoses) if(d.numero == 123 && d.valeur == 0) fin = true;
				VERIF(avant >= 1 && fin);
			}
		}
		// L'arbre de l'IA de 1_02 (AI_FightTutorial) : avec un objectif d'attaque, une cible et une zone, la descente mène à
		// Combat/…/CombatLoco, dont les pistes ManageTargets et CombatLoco retiennent le nœud et dont la piste spawn fait
		// jouer CombatActions dans son propre contrôleur (FUN_005f8860) : son ControlNode, retenu 1,03 s, puis redescendu
		// (l'octet +0x29). Une ActionNodeRequest pose l'intention tant que le nœud nommé n'est pas joué (FUN_006261f0).
		{
			CActionBank ia;
			VERIF(ia.TrouverOuCharger("/Global/AI_FightTutorial", "Act/AI/AI_FightTutorial.act"));
			struct : CJugeAction {
				int Juger(const CActionTreeFile &, int32, uint32 type, const uint32 *mots) const override {
					static const uint32 kObjectif = ActionHash("Objective"), kAcquerir = ActionHash("AcquireTarget"),
					                    kJoindre = ActionHash("JoinCombatZone"), kPoser = ActionHash("SetPedFlags"),
					                    kInterdites = ActionHash("AttacksDisabled"), kBranche = ActionHash("OpeningBranchChildHasChild"),
					                    kHorsPortee = ActionHash("IsTargetOutOfReach");
					if(type == kObjectif) return mots[0] == 7;
					if(type == kAcquerir || type == kJoindre || type == kPoser) return 1;
					if(type == kInterdites || type == kBranche || type == kHorsPortee) return 0;
					return -1;   // le reste (wait, PostHit, Offense…) ne passe pas
				}
			} combat;
			const CActionNode *racine = ia.Trouver("/Global/AI_FightTutorial");
			CActionController c;
			c.sousControleurs = true;
			VERIF(racine && c.Poser(ia, racine, combat, 0.0f));
			VERIF(c.joue && (c.joue->hachage & 0x7fffffff) == ActionHash("CombatLoco") && c.gestions.size() == 1 && c.combats.size() == 1 && c.lances.size() == 1);
			VERIF(c.finParRetenues && c.fin < 0.0f);
			c.Avancer(ia, nil, combat, 0.033f);
			VERIF(c.gestionsActives.size() == 1 && c.combatsActifs.size() == 1 && c.lances[0].lancee && c.lances[0].c);
			const CActionNode *controle = c.lances[0].c ? c.lances[0].c->joue : nil;
			VERIF(controle && controle->hachage == ActionHash("ControlNode") && c.lances[0].boucle);
			std::vector<CActionController *> tous;
			c.Recueillir(tous);
			VERIF(tous.size() == 2);
			float debut = c.lances[0].c ? c.lances[0].c->debut : 0.0f;
			for(float t = 0.066f; t < 1.2f; t += 0.033f) c.Avancer(ia, nil, combat, t);
			VERIF(c.joue && (c.joue->hachage & 0x7fffffff) == ActionHash("CombatLoco") && c.lances[0].c && c.lances[0].c->debut > debut);   // redescendu
			// ActionNodeRequest : CombatActions/…/Offense/#4ec3da49/Short/Short demande « Short » (+0x1d : vit tant qu'il le joue).
			const CActionNode *offense = ia.Trouver("/Global/AI_FightTutorial/CombatActions/CombatActions/Offense");
			const CActionNode *court = offense && !offense->enfants.empty() && !offense->enfants[0].enfants.empty() ? &offense->enfants[0].enfants[0] : nil;
			const CActionNode *feuille = court && !court->enfants.empty() ? &court->enfants[0] : nil;
			if(feuille){
				struct : CJugeAction {
					mutable int joue = 0;
					int Juger(const CActionTreeFile &, int32, uint32, const uint32 *) const override { return -1; }
					int JoueNom(uint32) const override { return joue; }
				} rien;
				CActionController d;
				d.sousControleurs = true;
				d.Jouer(ia, feuille, 0.0f, &rien);
				VERIF(d.demandesNoeud.size() == 1 && d.demandesNoeud[0].nom == ActionHash("Short") && d.demandesNoeud[0].suivre && d.finParRetenues);
				d.Avancer(ia, nil, rien, 0.033f);
				VERIF(d.intentions.size() == 1 && d.joue == feuille);           // pas encore joué : l'intention, le nœud retenu
				rien.joue = 1;
				d.Avancer(ia, nil, rien, 0.066f);
				VERIF(d.intentions.size() == 1 && d.joue == feuille);           // joué : plus d'intention à l'image suivante
				d.Avancer(ia, nil, rien, 0.1f);
				VERIF(d.intentions.empty() && d.joue == feuille && !d.demandesNoeud.empty() && d.demandesNoeud[0].vivante);
				rien.joue = 0;
				d.Avancer(ia, nil, rien, 0.133f);                               // fini : la piste meurt (+0x1e nul)
				VERIF(d.joue != feuille || (!d.demandesNoeud.empty() && !d.demandesNoeud[0].vivante));
			}else VERIF(false);
			// La feuille Block de CombatActions : sa piste ActionRequest (FUN_006260d0) appuie le bouton brut 12 au mode 2 à chaque
			// image et retient le nœud ; à l'arrêt (FUN_00626160), le bouton est lâché.
			const CActionNode *parade = ia.Trouver("/Global/AI_FightTutorial/CombatActions/CombatActions/Block/Block");
			if(parade){
				struct : CJugeAction { int Juger(const CActionTreeFile &, int32, uint32, const uint32 *) const override { return -1; } } rien;
				CActionController d;
				d.sousControleurs = true;
				d.Jouer(ia, parade, 0.0f, &rien);
				VERIF(d.demandesBouton.size() == 1 && d.demandesBouton[0].bouton == 12 && d.demandesBouton[0].mode == 2 && d.finParRetenues);
				d.Avancer(ia, nil, rien, 0.033f);
				VERIF(d.appuis.size() == 1 && d.appuis[0].first == 12 && d.joue == parade);
				d.Avancer(ia, nil, rien, 0.066f);
				VERIF(d.appuis.size() == 1 && d.laches.empty());
				d.Jouer(ia, nil, 0.1f);
				VERIF(d.laches.size() == 1 && d.laches[0] == 12);
			}else VERIF(false);
			// Sans objectif d'attaque ni focus, la descente tombe dans AMBIENT/IDLE : sa piste IDLE (FUN_0061d8e0) se lance à
			// la première image et vide les entrées du piéton ; elle retient le nœud (l'octet +8), qui ne finit pas.
			struct : CJugeAction {
				int Juger(const CActionTreeFile &, int32, uint32 type, const uint32 *) const override {
					static const uint32 kInterdites = ActionHash("AttacksDisabled");
					return type == kInterdites ? 0 : -1;
				}
			} ambiant;
			CActionController r;
			r.sousControleurs = true;
			VERIF(racine && r.Poser(ia, racine, ambiant, 0.0f));
			VERIF(r.joue && (r.joue->hachage & 0x7fffffff) == ActionHash("IDLE") && r.repos.size() == 1 && r.retenu && r.fin < 0.0f);
			r.Avancer(ia, nil, ambiant, 0.033f);
			VERIF(r.reposLances == 1);
			r.Avancer(ia, nil, ambiant, 0.066f);
			VERIF(r.reposLances == 0 && r.joue && (r.joue->hachage & 0x7fffffff) == ActionHash("IDLE"));
		}
		printf("  %d arbres globaux chargés, %zu banques sous /Global, /Global/HitTree/Standing/BlockHits trouvé\n", n, g.Global()->enfants.size());
		// Les références montées (FUN_005f6bb0 → FUN_005f6960) : Locomotion.act, référencé par B_Striker_A (un des arbres
		// globaux) et par Fight_Tutorial, n'est lu qu'une fois, sous /Global/Locomotion.act ; chaque référence le prend pour
		// cible (son nom, ses enfants) et son parent dépend de la clé (FUN_005f6800, FUN_005f6290).
		VERIF(g.Charger("Act/AI/Fight_Tutorial.act"));
		const CActionNode *banque = g.Trouver("/Global/Locomotion.act");
		const CActionNode *monte = banque && !banque->enfants.empty() ? &banque->enfants[0] : nil;
		VERIF(monte && monte->multiParent && monte->hachage == ActionHash("Locomotion") && monte->parentDefaut == banque);
		const CActionNode *refFT = g.Trouver("/Global/Fight_Tutorial/Default_KEY/Locomotion");
		const CActionNode *refBS = g.Trouver("/Global/B_Striker_A/Default_KEY/Locomotion");
		VERIF(refFT && refBS && refFT != refBS && refFT->genre == 'r' && refFT->monte == monte && refBS->monte == monte);
		const CActionNode *defautFT = g.Trouver("/Global/Fight_Tutorial/Default_KEY"), *defautBS = g.Trouver("/Global/B_Striker_A/Default_KEY");
		if(monte){
			VERIF(g.ParentMonte(*monte, ActionHash("Fight_Tutorial")) == defautFT && g.ParentMonte(*monte, ActionHash("B_Striker_A")) == defautBS);
			VERIF(g.ParentMonte(*monte, ActionHash("Locomotion.act")) == banque && g.ParentMonte(*monte, ActionHash("Personne")) == banque);
		}
		// Un chemin qui traverse la référence : ses enfants sont ceux de l'arbre monté (FUN_005f5340 / FUN_005f5360).
		const CActionNode *opps = g.Trouver("/Global/Fight_Tutorial/Default_KEY/Locomotion/ActionOpps");
		VERIF(opps && opps == g.Trouver("/Global/B_Striker_A/Default_KEY/Locomotion/ActionOpps") && opps == g.Trouver("/Global/Locomotion.act/Locomotion/ActionOpps"));
		// « .. » suit le parent de la clé courante (DAT_00b0a604) : les ActionOpps de Locomotion (« ../../../Offense »)
		// mènent à l'Offense de l'arbre où l'on est.
		if(opps){
			g.cle = ActionHash("Fight_Tutorial");
			VERIF(g.Relatif(opps, "../../../Offense") == g.Trouver("/Global/Fight_Tutorial/Offense") && g.CleDe(opps) == ActionHash("Fight_Tutorial"));
			VERIF(g.CheminDe(opps) == "/Global/Fight_Tutorial/Default_KEY/Locomotion/ActionOpps" && g.Sous(defautFT, opps) && !g.Sous(defautBS, opps));
			g.cle = ActionHash("B_Striker_A");
			VERIF(g.Relatif(opps, "../../../Offense") == g.Trouver("/Global/B_Striker_A/Offense") && g.CleDe(opps) == ActionHash("B_Striker_A"));
			g.cle = 0xffffffffu;   // sans clé : le parent par défaut, la banque
			VERIF(g.CheminDe(opps) == "/Global/Locomotion.act/Locomotion/ActionOpps");
		}
		// La descente traverse la référence (FUN_005f5470) : la banque Default_KEY de Fight_Tutorial ne tombe plus sur son
		// premier enfant jouable (idle) mais dans Locomotion, si ses conditions passent.
		if(defautFT){
			struct : CJugeAction { int Juger(const CActionTreeFile &, int32, uint32, const uint32 *) const override { return 0; } } non;
			g.cle = ActionHash("Fight_Tutorial");
			const CActionNode *d = g.Choisir(refFT, non);
			VERIF(d && g.Sous(refFT, d) && g.CheminDe(d).compare(0, 46, "/Global/Fight_Tutorial/Default_KEY/Locomotion/") == 0);
			printf("  références montées : /Global/Locomotion.act lu une fois, %zu parents ; la descente de Fight_Tutorial/Default_KEY : %s\n",
			       monte ? monte->parents.size() : 0, g.CheminDe(d).c_str());
			g.cle = 0xffffffffu;
		}
		// La pile du contexte (FUN_005f36d0) : un contrôleur posé sur /Global/Fight_Tutorial a pour clé le nom de l'arbre
		// (FUN_005f4e40), qui devient DAT_00b0a604 le temps de ses jugements et de ses chemins (FUN_005f3e20), puis la
		// clé d'avant revient. FUN_005f3700 empile une seconde clé, deux au plus.
		if(const CActionNode *ft = g.Trouver("/Global/Fight_Tutorial")){
			struct : CJugeAction {
				const CActionBank *b = nil; mutable uint32 vue = 0;
				int Juger(const CActionTreeFile &, int32, uint32, const uint32 *) const override { vue = b->cle; return 0; }
			} temoin;
			temoin.b = &g;
			g.cle = ActionHash("Personne");
			CActionController c;
			VERIF(c.Poser(g, ft, temoin, 0.0f) && c.Cle() == ActionHash("Fight_Tutorial") && c.pile.size() == 1);
			VERIF(temoin.vue == ActionHash("Fight_Tutorial") && g.cle == ActionHash("Personne"));
			std::string vu = CheminVu(g, c);
			VERIF(vu.compare(0, 46, "/Global/Fight_Tutorial/Default_KEY/Locomotion/") == 0);
			printf("  pile du contexte : %s joué avec la clé Fight_Tutorial\n", vu.c_str());
			c.Empiler(ActionHash("Player")); c.Empiler(ActionHash("HitTree"));
			VERIF(c.pile.size() == 2 && c.Cle() == ActionHash("Player"));
			VERIF(CheminVu(g, c).compare(0, 25, "/Global/Locomotion.act/Lo") == 0);   // une clé sans paire : la banque
			g.cle = 0xffffffffu;
		}
	}
	// La phase de l'attaque (FUN_005f9480, FUN_005f9e70, FUN_005f8240) et les conditions CollisionStart (FUN_00602260)
	// et AttackPhase (FUN_00601fc0) : une piste Collision lancée à 0,1 s, la fenêtre de 0,2 à 0,3 s, sans durée de vie,
	// le nœud fini à 0,8 s.
	{
		CActionController c;
		c.debut = 10.0f; c.fin = 10.8f;
		CActionController::Contact k{nil, 10.2f, 10.3f, 0u, 8.0f, std::string(), false};
		k.entree = 0.1f; k.debutBrut = 0.2f; k.finBrut = 0.3f; k.debutFenetre = 0.2f; k.finFenetre = 0.3f;
		c.contacts.push_back(k);
		c.PhasesCollision(10.05f);
		VERIF(c.phaseAttaque == 0 && !ConditionCollisionStart(c) && !ConditionAttackPhase(c, 1, 0.0f, 10050));
		c.PhasesCollision(10.1f);
		VERIF(c.phaseAttaque == 1 && ConditionCollisionStart(c) && c.echeanceAttaque == 10300);
		VERIF(ConditionAttackPhase(c, 1, 0.0f, 10100) && ConditionAttackPhase(c, 1, 0.1f, 10100) && !ConditionAttackPhase(c, 1, 0.2f, 10100));
		c.PhasesCollision(10.25f);
		VERIF(c.phaseAttaque == 2 && c.echeanceAttaque == 10550 && !ConditionAttackPhase(c, 3, 0.0f, 10250));
		c.PhasesCollision(10.4f);   // passé la fenêtre : la phase 3, l'échéance à la fin du nœud
		VERIF(c.phaseAttaque == 3 && c.echeanceAttaque == 10800);
		VERIF(ConditionAttackPhase(c, 3, 0.0f, 10400) && ConditionAttackPhase(c, 3, 0.3f, 10400) && !ConditionAttackPhase(c, 3, 0.567f, 10400));
		c.ArreterCollisions();      // le nœud change : la phase repasse à 0, l'octet +0x4e5 reste
		VERIF(c.phaseAttaque == 0 && ConditionCollisionStart(c));
		// AttackTimer (FUN_006000c0) : la minuterie (+0x550) posée à l'arrêt du coup (FUN_005f8240 → FUN_0048c040(0)).
		VERIF(c.minuterieAttaque == 10400);
		VERIF(ConditionAttackTimer(c, 0.5f, false, 3, 11000) && !ConditionAttackTimer(c, 0.5f, false, 3, 10800));
		VERIF(ConditionAttackTimer(c, 0.5f, true, 3, 11000) && c.minuterieAttaque == 11000);   // l'octet +8 la relance
		CActionController neuf;
		VERIF(!ConditionAttackTimer(neuf, 0.0f, false, 5, 1500) && ConditionAttackTimer(neuf, 1.0f, false, 5, 2500));
	}
	printf("test_actionbank : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
