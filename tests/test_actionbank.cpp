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
		// Les conditions propres d'une Opportunity (+0x3c) : dans les 68 arbres du démarrage, aucune n'en porte ; le
		// contrôleur ne les juge donc pas. Le relevé compte aussi les Opportunity et les loquets.
		{
			struct Releve { const CActionBank &b; int occasions = 0, loquets = 0, avecConditions = 0, periodes = 0;
				void Dans(const CActionNode &x){
					for(const COpportunite &o : COpportunite::DuNoeud(b, x)){
						occasions++; loquets += o.loquet; avecConditions += o.conditions != 0; periodes += o.periode > 1;
					}
					for(const CActionNode &e : x.enfants) Dans(e);
				} } r{g, 0, 0, 0, 0};
			r.Dans(*g.Global());
			VERIF(r.occasions > 1000 && r.loquets > 0 && r.avecConditions == 0);
			printf("  %d Opportunity dans les arbres du démarrage (%d loquets, %d à période, %d à conditions propres)\n", r.occasions, r.loquets, r.periodes, r.avecConditions);
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
		printf("  %d arbres globaux chargés, %zu banques sous /Global, /Global/HitTree/Standing/BlockHits trouvé\n", n, g.Global()->enfants.size());
	}
	printf("test_actionbank : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
