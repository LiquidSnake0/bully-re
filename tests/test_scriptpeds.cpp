// Le pool des piétons des scripts : 24 entrées, le joueur en 0, -1 quand c'est plein, les handles
// libérés repris, le ramassage par objet script ; puis les gestes : marche, sprint, arrêt, bagarre
// et mort. Aucun fichier du jeu.
//   build/tests/test_scriptpeds
#include <cstring>
#include <cmath>
#include "../src/peds/ScriptPedPool.h"
#include <cstdio>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CScriptPedPool p;
	VERIF(p.Valide(SCRIPT_PED_JOUEUR) && p.Vivants() == 1);
	// Le type (+0x1310) : PLAYER1 pour le joueur ; un piéton neuf reçoit celui de son modèle à la création (l'appelant,
	// FUN_0047ffe0), DEFAULT en attendant.
	VERIF(p.Acceder(SCRIPT_PED_JOUEUR)->type == SCRIPT_PED_TYPE_JOUEUR);
	VERIF(!p.Valide(-1) && !p.Valide(1) && !p.Valide(SCRIPT_PED_POOL_SIZE));
	VERIF(p.Acceder(1) == nil && p.Acceder(-1) == nil);

	// PedCreatePoint(85, …) depuis le script 1 : la première entrée libre après le joueur.
	int a = p.Creer(85, CVector(298.36f, -72.23f, 6.07f), 185, 1);
	VERIF(a == 1 && p.Valide(a));
	const CScriptPed *pa = p.Acceder(a);
	VERIF(pa && pa->modele == 85 && pa->cap == 185 && pa->script == 1 && pa->sante == 100 && !pa->mort);
	int b = p.Creer(99, CVector(), 0, 1);
	VERIF(b == 2);

	// Le joueur ne se supprime pas ; un piéton oui, et son handle devient invalide puis repris.
	VERIF(!p.Supprimer(SCRIPT_PED_JOUEUR) && p.Valide(SCRIPT_PED_JOUEUR));
	VERIF(p.Supprimer(a) && !p.Valide(a) && !p.Supprimer(a));
	VERIF(p.Creer(85, CVector(), 0, 2) == a);

	// 24 entrées en tout : la pleine donne -1, comme FUN_005cf6b0.
	int crees = 0;
	while(p.Creer(1, CVector(), 0, 2) >= 0) crees++;
	VERIF(p.Vivants() == SCRIPT_PED_POOL_SIZE);
	VERIF(crees == SCRIPT_PED_POOL_SIZE - 3);   // le joueur, b (script 1) et a (script 2) étaient déjà là

	// Le ramassage par script : tout ce que le script 2 a créé disparaît, le script 1 et le joueur restent.
	int liberes = p.SupprimerDuScript(2);
	VERIF(liberes == SCRIPT_PED_POOL_SIZE - 2);
	VERIF(p.Valide(SCRIPT_PED_JOUEUR) && p.Valide(b) && !p.Valide(a) && p.Vivants() == 2);
	VERIF(p.SupprimerDuScript(7) == 0);

	// Les gestes : une marche de 7 m à 1,4 m/s arrive en 5 s, à une image près, et pas avant.
	CScriptPedPool q;
	int m = q.Creer(85, CVector(0, 0, 0), 0, 1);
	VERIF(q.Aller(m, CVector(7, 0, 0), ALLURE_MARCHE) && q.Acceder(m)->enMarche);
	// L'intention de mouvement (+0x2e0 : +0 la direction, +0x1c la vitesse) : l'allure 0 vaut 0,3 (FUN_005c6180), vers +x.
	VERIF(fabsf(q.Acceder(m)->intentionVitesse - 0.3f) < 1e-6f && q.Acceder(m)->intention.x == 1.0f);
	VERIF(ScriptPedIntentionVitesse(1) == 0.7f && ScriptPedIntentionVitesse(2) == 1.1f && ScriptPedIntentionVitesse(-1) == 0.7f);
	int images = 0; bool arrive = false;
	for(; images < 400 && !arrive; images++){ q.Avancer(0.033f); arrive = q.Acceder(m)->arrive; }
	VERIF(q.Acceder(m)->intentionVitesse == 0.0f);   // arrivé : plus d'intention (FUN_004a45c0)
	VERIF(arrive && images >= 150 && images <= 153);
	VERIF(!q.Acceder(m)->enMarche && q.Acceder(m)->pos.x == 7 && q.Acceder(m)->pos.y == 0);
	q.Avancer(0.033f);
	VERIF(!q.Acceder(m)->arrive);      // l'arrivée ne se signale qu'une image
	// Le sprint va trois fois et demie plus vite que la marche ; PedStop arrête net.
	VERIF(ScriptPedVitesse(ALLURE_SPRINT) > 3 * ScriptPedVitesse(ALLURE_MARCHE));
	VERIF(q.Aller(m, CVector(7, 70, 0), ALLURE_SPRINT));
	q.Avancer(1.0f);
	VERIF(q.Acceder(m)->pos.y > 4.9f && q.Acceder(m)->pos.y < 5.1f && q.Acceder(m)->cap > 89 && q.Acceder(m)->cap < 91);
	VERIF(q.Arreter(m) && !q.Acceder(m)->enMarche);
	float yArret = q.Acceder(m)->pos.y;
	q.Avancer(1.0f);
	VERIF(q.Acceder(m)->pos.y == yArret);
	// La bagarre : l'attaquant rejoint sa cible, frappe toutes les 0,8 s, la cible meurt à 0 et il s'arrête.
	int at = q.Creer(69, CVector(0, 0, 0), 0, 1), ci = q.Creer(102, CVector(10, 0, 0), 0, 1);
	VERIF(!q.Attaquer(at, at) && !q.Attaquer(at, 23) && q.Attaquer(at, ci));
	// PedLockTarget (0x5cb060) : verrouiller une cible qui existe, lever par -1, rien pour une cible absente.
	VERIF(q.Verrouiller(at, ci, 3) && q.Acceder(at)->verrou == ci && q.Acceder(at)->verrouMode == 3);
	VERIF(!q.Verrouiller(at, 23, 3) && q.Acceder(at)->verrou == ci && !q.Verrouiller(23, ci, 3));
	VERIF(q.Verrouiller(at, -1, 0) && q.Acceder(at)->verrou == -1);
	int coups = 0, mortA = -1;
	for(int i = 0; i < 1000 && mortA < 0; i++){
		q.Avancer(0.033f);
		coups += q.Acceder(at)->aFrappe;
		if(q.Acceder(ci)->vientDeMourir) mortA = i;
	}
	VERIF(mortA > 0 && q.Acceder(ci)->mort && q.Acceder(ci)->sante <= 0);
	VERIF(coups == 10);                                   // 100 points de vie, 10 par coup
	VERIF(q.Acceder(at)->attaque == -1);                   // plus de cible
	float dx = q.Acceder(at)->pos.x - q.Acceder(ci)->pos.x;
	VERIF(dx < 0 && dx > -1.3f);                          // arrêté à portée, pas sur la cible
	// 10 m à 3,5 m/s = 2,9 s avant le premier coup, puis 9 × 0,8 s : la mort vers 10 s.
	VERIF(mortA * 0.033f > 9.5f && mortA * 0.033f < 10.5f);
	q.Avancer(0.033f);
	VERIF(!q.Acceder(ci)->vientDeMourir && q.Acceder(ci)->mort);
	// Un mort ne marche plus, n'attaque plus. PedSetHealth à 0 ne tue pas (FUN_00478190) : seule la piste Death
	// (FUN_00479110) tue, met la santé à -100 et lâche le geste ; un immortel (+0x13fc) n'en meurt pas.
	VERIF(!q.Aller(ci, CVector(), ALLURE_MARCHE) && !q.Attaquer(ci, at));
	VERIF(q.Acceder(ci)->sante == -100.0f);
	int c = q.Creer(99, CVector(0, 5, 0), 0, 1);
	VERIF(q.Aller(c, CVector(0, 9, 0), ALLURE_MARCHE));
	q.Sante(c, 0);
	VERIF(!q.Acceder(c)->mort && q.Acceder(c)->sante == 0 && q.Acceder(c)->enMarche);
	q.Acceder(c)->immortel = true;
	VERIF(!q.Mourir(c, 5000) && !q.Acceder(c)->mort);
	q.Acceder(c)->immortel = false;
	q.Verrouiller(c, at, 1);
	VERIF(q.Mourir(c, 5000) && !q.Mourir(c, 6000));
	VERIF(q.Acceder(c)->mort && q.Acceder(c)->vientDeMourir && !q.Acceder(c)->enMarche);
	VERIF(q.Acceder(c)->sante == -100.0f && q.Acceder(c)->instantMort == 5000 && q.Acceder(c)->verrou == -1);
	VERIF(!q.Blesser(c, 10, at));                          // un mort ne prend plus de coups
	// PedMakeAmbient : le piéton reste, mais n'est plus ramassé avec son script ; pas le joueur.
	VERIF(!q.RendreAmbiant(SCRIPT_PED_JOUEUR) && q.RendreAmbiant(at) && q.Acceder(at)->script == -1);
	VERIF(q.SupprimerDuScript(1) == 3 && q.Valide(at));   // m, ci et c libérés, a reste

	// FUN_0047b6b0 / FUN_004782a0 : l'invulnérabilité, l'invulnérabilité au joueur et la santé minimale.
	{
		CScriptPedPool q;
		int a = q.Creer(85, CVector(0, 0, 0), 0, 1), b = q.Creer(99, CVector(1, 0, 0), 0, 1);
		CScriptPed *pa = q.Acceder(a), *pj = q.Acceder(SCRIPT_PED_JOUEUR);
		VERIF(q.Blesser(a, 30, b) && pa->sante == 70);
		pa->drapeaux[DRAPEAU_INVULNERABLE] = 1;
		VERIF(!q.Blesser(a, 30, b) && pa->sante == 70);
		pa->drapeaux[DRAPEAU_INVULNERABLE] = 0; pa->drapeaux[DRAPEAU_INVULNERABLE_JOUEUR] = 1;
		VERIF(!q.Blesser(a, 30, SCRIPT_PED_JOUEUR) && q.Blesser(a, 30, b) && pa->sante == 40);
		pa->santeMin = 75;                                    // plus bas que la santé actuelle : elle remonte au plancher
		VERIF(q.Blesser(a, 10, b) && pa->sante == 75 && !pa->mort);
		pa->santeMin = 0;
		VERIF(q.Blesser(a, 500, b) && pa->sante == 0 && !pa->mort);   // à zéro, pas mort : la piste Death décide
		pj->drapeaux[DRAPEAU_INVULNERABLE] = 1;
		VERIF(!q.Blesser(SCRIPT_PED_JOUEUR, 10, b) && pj->sante == 100);
		pa->santeMin = 0; pa->sante = 50; pa->drapeaux[DRAPEAU_INVULNERABLE_JOUEUR] = 0;
		pa->drapeaux[DRAPEAU_INTOUCHABLE] = 1;                // le drapeau 12 (+0x1344) : aucun coup
		VERIF(!q.Blesser(a, 10, b) && pa->sante == 50);
	}
	// FUN_00478e30, FUN_00476fa0, FUN_00472ff0 : les colonnes de pedstats.dat, la santé maximale et la régénération.
	{
		CScriptPedPool q;
		int a = q.Creer(146, CVector(0, 0, 0), 0, 1);
		CScriptPed *pa = q.Acceder(a);
		VERIF(pa->sante == 100 && pa->santeMax == 100);        // le constructeur de CPed : 100 et 100
		int32 col[64] = {};
		col[4] = 120; col[5] = 50; col[63] = 20;               // F santé maximale, G régénération, BM seuil d'humiliation
		VERIF(q.PoserStats(a, col) && pa->sante == 120 && pa->santeMax == 120 && pa->santeMin == 0 && q.Stat(a, 63) == 20);
		VERIF(q.Blesser(a, 20, -1) && pa->sante == 100);
		for(int i = 0; i < 30; i++) q.Avancer(1.0f / 30);    // une seconde : + 50 / 100 point
		VERIF(fabsf(pa->sante - 100.5f) < 1e-3f);
		VERIF(q.PoserStat(a, 4, 90) && pa->santeMax == 90 && pa->sante == 90);   // le maximum baisse, la santé suit
		for(int i = 0; i < 30; i++) q.Avancer(1.0f / 30);    // au maximum : plus rien
		VERIF(pa->sante == 90);
		q.Sante(a, 0);
		for(int i = 0; i < 30; i++) q.Avancer(1.0f / 30);    // à zéro : rien non plus
		VERIF(pa->sante == 0);
	}
	// FUN_0048e790, FUN_00490ef0, FUN_004925d0, FUN_0048f5d0, FUN_0048dc60, FUN_00492950 : le groupe d'humiliation.
	{
		CScriptPedPool q;
		int a = q.Creer(146, CVector(0, 0, 0), 0, 1), b = q.Creer(146, CVector(1, 0, 0), 0, 1);
		CScriptPed *pa = q.Acceder(a), *pb = q.Acceder(b);
		VERIF(pa->groupeHumiliation == -1 && q.GroupeHumiliationDans(0, 2) == -1);   // le constructeur : -1
		VERIF(strcmp(ScriptPedGroupeHumiliation(0), "HUMIL_5-8_A") == 0 && strcmp(ScriptPedGroupeHumiliation(2), "HUMIL_5-8_C") == 0);
		VERIF(strcmp(ScriptPedGroupeHumiliation(21), "HUMIL_6-1V6-1") == 0 && ScriptPedGroupeHumiliation(22) == nil);
		// Au-dessus de la moitié de la santé : rien ; dessous, un Medium (1) tire 0, 1 ou 2.
		pa->santeMax = 95; pa->sante = 50;
		VERIF(q.SurveillerHumiliation(a, 1, 1, 0.9f) == -1 && pa->groupeHumiliation == -1);
		pa->sante = 5;
		VERIF(q.SurveillerHumiliation(a, 1, 1, 0.9f) == 2 && pa->groupeHumiliation == 2);   // 5-8_IndianBurn
		VERIF(q.SurveillerHumiliation(a, 1, 1, 0.1f) == -1 && pa->groupeHumiliation == 2);   // déjà un groupe
		// Un autre Medium prend le groupe déjà là, sans tirage.
		pb->santeMax = 100; pb->sante = 10;
		VERIF(q.SurveillerHumiliation(b, 1, 1, 0.1f) == 2 && pb->groupeHumiliation == 2);
		VERIF(q.LibererGroupeHumiliation(a) == 2 && q.LibererGroupeHumiliation(b) == 2 && q.LibererGroupeHumiliation(b) == -1);
		VERIF(q.SurveillerHumiliation(b, 1, 1, 0.1f) == 0 && q.SurveillerHumiliation(a, 3, 1, 0.7f) == 4);   // 0,33 ; 0,5
		VERIF(q.ChoisirGroupeHumiliation(a, 4, SCRIPT_PED_JOUEUR, 1, true, 0.5f) == 11);   // Huge : 6-5_B
		VERIF(q.ChoisirGroupeHumiliation(a, 1, SCRIPT_PED_JOUEUR, 1, false, 0.5f) == 17);  // 5-8VPLY
		VERIF(q.ChoisirGroupeHumiliation(a, 2, b, 0, false, 0.5f) == 19 && q.ChoisirGroupeHumiliation(a, 2, b, 3, false, 0.5f) == 20);
		VERIF(q.ChoisirGroupeHumiliation(a, 1, b, 1, true, 0.5f) == -1 && pa->groupeHumiliation == 20);   // un autre et l'octet : rien
		VERIF(q.PoserGroupeHumiliation(a, 5) == 20 && q.PoserGroupeHumiliation(a, 5) == -1);
	}
	// lua_PedAttack (0x5c9ca0) : un AttackPedObjective (FUN_00470c10, type 7) dans la file des objectifs (+0x5f0) ;
	// FUN_00471240 l'ajoute au bout, FUN_00471320 devant ; la même attaque que le dernier (le premier) n'est pas doublée.
	{
		CScriptPedPool q;
		int a = q.Creer(146, CVector(0, 0, 0), 0, 1), b = q.Creer(85, CVector(1, 0, 0), 0, 1), c = q.Creer(99, CVector(2, 0, 0), 0, 1);
		VERIF(q.ObjectifCourant(a) == nil);                    // l'objectif par défaut n'est pas suivi
		VERIF(q.Attaquer(a, SCRIPT_PED_JOUEUR, 3) && q.Acceder(a)->nombreObjectifs == 1);
		const CObjectifPieton *o = q.ObjectifCourant(a);
		VERIF(o && o->type == OBJECTIF_ATTAQUER_PIETON && o->cible == SCRIPT_PED_JOUEUR && o->style == 3);
		VERIF(q.Attaquer(a, SCRIPT_PED_JOUEUR, 3) && q.Acceder(a)->nombreObjectifs == 1);   // pas doublé
		VERIF(q.Attaquer(a, b) && q.Acceder(a)->nombreObjectifs == 2 && q.ObjectifCourant(a)->cible == SCRIPT_PED_JOUEUR);
		VERIF(q.Attaquer(a, c, 0, true) && q.Acceder(a)->nombreObjectifs == 3 && q.ObjectifCourant(a)->cible == c);
		VERIF(q.Acceder(a)->objectifs[1].cible == SCRIPT_PED_JOUEUR && q.Acceder(a)->objectifs[2].cible == b);
		VERIF(q.Attaquer(a, c, 0, true) && q.Acceder(a)->nombreObjectifs == 3);              // devant : comparé au premier
		// FUN_00470110 : les deux attaques de suite (0 et 1) ; la seconde, venue à la place de la première, est sautée.
		VERIF(q.RetirerObjectifs(a, OBJECTIF_ATTAQUER_PIETON) == 2 && q.Acceder(a)->nombreObjectifs == 1 && q.ObjectifCourant(a)->cible == SCRIPT_PED_JOUEUR);
		VERIF(q.RetirerObjectifs(a, OBJECTIF_ATTAQUER_PIETON) == 1 && q.ObjectifCourant(a) == nil);
		VERIF(q.Attaquer(a, c, 0, true) && q.Attaquer(a, b) && q.Attaquer(a, SCRIPT_PED_JOUEUR) && q.Acceder(a)->nombreObjectifs == 3);
		CObjectifPieton autre; autre.type = 9;
		for(int i = 3; i < SCRIPT_PED_OBJECTIFS; i++) VERIF(q.PousserObjectif(a, autre, false));
		VERIF(!q.PousserObjectif(a, autre, false) && q.Acceder(a)->nombreObjectifs == SCRIPT_PED_OBJECTIFS);   // 20 places
	}
	// PedJoinCombatZone (FUN_00487a70) : la zone 0 du joueur n'a qu'une place (AIButes +0x18) ; l'essai ne prend rien.
	{
		CScriptPedPool q;
		int a = q.Creer(146, CVector(0, 0, 0), 0, 1), b = q.Creer(85, CVector(1, 0, 0), 0, 1);
		VERIF(q.JoindreZoneCombat(a, SCRIPT_PED_JOUEUR, 0, true) && q.Acceder(a)->zoneCombat == -1 && q.Acceder(a)->verrou == -1);
		VERIF(q.JoindreZoneCombat(a, SCRIPT_PED_JOUEUR, 0, false) && q.Acceder(a)->zoneCombat == 0 && q.Acceder(a)->verrou == SCRIPT_PED_JOUEUR);
		VERIF(q.JoindreZoneCombat(a, SCRIPT_PED_JOUEUR, 0, true));                       // déjà dedans
		VERIF(!q.JoindreZoneCombat(b, SCRIPT_PED_JOUEUR, 0, true) && q.JoindreZoneCombat(b, SCRIPT_PED_JOUEUR, 1, false));
		q.Acceder(b)->masqueZones[2] = false;                                            // PedSetCombatZoneMask
		VERIF(!q.JoindreZoneCombat(b, SCRIPT_PED_JOUEUR, 2, true));
		VERIF(q.JoindreZoneCombat(a, SCRIPT_PED_JOUEUR, 1, false) && q.Acceder(a)->zoneCombat == 1);   // il quitte la zone 0
		VERIF(q.Acceder(SCRIPT_PED_JOUEUR)->combattants[0].empty() && q.Acceder(SCRIPT_PED_JOUEUR)->combattants[1].size() == 2);
		VERIF(q.Supprimer(a) && q.Acceder(SCRIPT_PED_JOUEUR)->combattants[1].size() == 1);
	}
	// Les manettes (FUN_0049b1b0, PedSetControllerID FUN_005c9fc0) et la condition Controller (FUN_006006f0).
	{
		CScriptPedPool q;
		int b = q.Creer(146, CVector(0, 0, 0), 0, 1);
		VERIF(q.PietonDeLaManette(0) == SCRIPT_PED_JOUEUR && q.PietonDeLaManette(1) == -1);
		VERIF(q.MeneParLaManette(SCRIPT_PED_JOUEUR, false, false) && !q.MeneParLaManette(SCRIPT_PED_JOUEUR, true, false));
		VERIF(q.MeneParLaManette(SCRIPT_PED_JOUEUR, true, true) && !q.MeneParLaManette(b, false, true));
		VERIF(!q.PoserManette(b, 2) && q.PoserManette(b, 1) && q.PietonDeLaManette(1) == b);
		VERIF(q.MeneParLaManette(b, true, false) && q.MeneParLaManette(b, false, true) && !q.MeneParLaManette(b, false, false));
		VERIF(!q.PoserManette(17, 0) && q.PietonDeLaManette(0) == SCRIPT_PED_JOUEUR);
	}
	// L'état social (+0xbb0, +0xbb4) : IsSocialStateWait (FUN_00603b80) et IsSocializing (FUN_006039d0).
	{
		CScriptPedPool q;
		q.Acceder(SCRIPT_PED_JOUEUR)->type = SCRIPT_PED_TYPE_JOUEUR;
		int b = q.Creer(146, CVector(0, 0, 0), 0, 1);
		VERIF(!q.AttenteSociale(b) && !q.Socialise(b, false) && !q.Socialise(SCRIPT_PED_JOUEUR, true));
		CScriptPed *p = q.Acceder(b);
		p->etatSocial = 2;
		VERIF(!q.AttenteSociale(b));                   // sans partenaire, rien
		p->partenaireSocial = SCRIPT_PED_JOUEUR;
		VERIF(q.AttenteSociale(b) && q.Socialise(b, false) && q.Socialise(b, true));
		VERIF(q.Socialise(SCRIPT_PED_JOUEUR, true) && !q.Socialise(SCRIPT_PED_JOUEUR, false));
		p->etatSocial = 3;
		VERIF(!q.AttenteSociale(b) && q.Socialise(b, false));
		p->drapeauxSociaux = 1;
		VERIF(!q.Socialise(SCRIPT_PED_JOUEUR, true));
	}
	// Attacked (FUN_00606b80) : l'attaquant devient la cible (FUN_0048c3b0), la cible de l'objectif et l'agresseur.
	{
		CScriptPedPool q;
		q.Acceder(SCRIPT_PED_JOUEUR)->type = SCRIPT_PED_TYPE_JOUEUR;
		int b = q.Creer(146, CVector(0, 0, 0), 0, 1), c = q.Creer(85, CVector(1, 0, 0), 0, 1);
		VERIF(!q.PrendreAttaquant(SCRIPT_PED_JOUEUR, b) && !q.PrendreAttaquant(b, 17));
		VERIF(q.JoindreZoneCombat(b, c, 0, false) && q.Acceder(b)->zoneCombat == 0 && q.Acceder(b)->verrou == c);
		VERIF(q.PrendreAttaquant(b, SCRIPT_PED_JOUEUR));
		const CScriptPed *p = q.Acceder(b);
		VERIF(p->verrou == SCRIPT_PED_JOUEUR && p->cibleObjectif == SCRIPT_PED_JOUEUR && p->agresseur == SCRIPT_PED_JOUEUR);
		VERIF(p->zoneCombat == -1 && q.Acceder(c)->combattants[0].empty());
	}
	// FUN_00478e30 : la classe de combat (+0x714) est la colonne S ; le focus (+0x53c) naît nul (FUN_0048c1b0).
	{
		CScriptPedPool q;
		int b = q.Creer(146, CVector(0, 0, 0), 0, 1);
		int32 col[64] = {}; col[4] = 95; col[17] = 3;
		VERIF(q.Acceder(b)->classeCombat == 0 && q.Acceder(b)->focus == -1);
		VERIF(q.PoserStats(b, col) && q.Acceder(b)->classeCombat == 3 && q.Stat(b, 17) == 3);
	}
	// La vitesse du pas des pistes de locomotion de l'IA (FUN_00625970, FUN_0061b820, FUN_0061b8e0).
	VERIF(ScriptPedVitesseDuPas(PAS_SUR_PLACE, 100, 100, 1.5f) == 0.0f && ScriptPedVitesseDuPas(PAS_ATTENDRE, 100, 100, 1.5f) == 0.0f);
	VERIF(ScriptPedVitesseDuPas(PAS_VERS_CIBLE, 100, 100, 1.5f) == 1.5f && ScriptPedVitesseDuPas(PAS_ARRIVE, 0, 0, 1.5f) == 1.5f);
	VERIF(fabsf(ScriptPedVitesseDuPas(PAS_APPROCHE_7, 0, 0, 1.5f) - 0.2f) < 1e-6f && fabsf(ScriptPedVitesseDuPas(PAS_APPROCHE_9, 0, 128, 1.5f) - 1.5f) < 1e-6f);
	VERIF(fabsf(ScriptPedVitesseDuPas(PAS_APPROCHE_7, 0, 32, 1.5f) - 0.85f) < 1e-6f);
	VERIF(fabsf(ScriptPedVitesseDuPas(PAS_RALENTI, 1.0f, 0, 1.5f) - 0.5f) < 1e-6f && fabsf(ScriptPedVitesseDuPas(PAS_RALENTI, 100, 0, 1.5f) - 1.5f) < 1e-6f);
	VERIF(fabsf(ScriptPedVitesseDuPas(PAS_RALENTI, 14.0f, 0, 1.5f) - (1.0f - powf(0.5f, 8.0f) + 0.5f)) < 1e-6f);
	// La vitesse de la navigation (FUN_005e4a60) : arrivé, immobile, trop lente → 0 ; ralentie près du point par l'écart de cap.
	VERIF(ScriptPedVitesseDuTrajet(1.5f, 100, 0, 0.2f) == 1.5f && ScriptPedVitesseDuTrajet(1.5f, 0.03f, 0, 0.2f) == 0.0f);
	VERIF(ScriptPedVitesseDuTrajet(1.5f, 100, 0, 0.2f, true) == 0.0f && ScriptPedVitesseDuTrajet(0.05f, 100, 0, 0.2f) == 0.0f);
	VERIF(ScriptPedVitesseDuTrajet(1.5f, 4, 0, 0.2f) == 1.5f && fabsf(ScriptPedVitesseDuTrajet(1.5f, 4, 1.0471976f, 0.2f) - 0.75f) < 1e-5f);
	VERIF(fabsf(ScriptPedVitesseDuTrajet(1.5f, 0.0625f, 0, 0.2f) - 1.25f) < 1e-5f);    // 0,25 m : × 0,25 / 0,3
	VERIF(fabsf(ScriptPedVitesseDuTrajet(1.5f, 0.94f, 3.1f, 0.2f) - 0.1f) < 1e-6f);    // de dos : au moins 0,1
	VERIF(ScriptPedVitesseDuTrajet(1.5f, 0.94f, 3.2f, 0.2f) == 0.0f);
	// Le choix du point de CombatLoco (FUN_0062e120, FUN_0062c540, FUN_0062c740, FUN_0061b700, FUN_00625740, FUN_0061b6a0).
	{
		struct Des { int v[8]; int i; } des = {{16383, 0, 32767, 1, 2, 3, 4, 5}, 0};
		auto tirer = [](void *x) -> int { Des *d = (Des *)x; return d->v[d->i++ % 8]; };
		CCombatLoco c;
		CVector cible(0, 0, 0);
		ScriptPedCombatLocoChoisir(c, CVector(10, 0, 0), cible, 0.9f, 1.5f, 33, true, tirer, &des);   // trop loin : le retour
		VERIF(c.sousEtat == 3 && c.forcer && c.tirage && fabsf(c.angle) < 1e-3f);
		c.tirage = false;
		ScriptPedCombatLocoChoisir(c, CVector(10, 0, 0), cible, 0.9f, 1.5f, 33, true, tirer, &des);
		VERIF(c.sousEtat == 3 && c.mode == 12 && c.point.x == 0.0f);
		ScriptPedCombatLocoChoisir(c, CVector(1.2f, 0, 0), cible, 0.9f, 1.5f, 33, true, tirer, &des);   // dans le grand rayon
		VERIF(c.sousEtat == 0 && c.forcer);
		c.tirage = false;
		ScriptPedCombatLocoChoisir(c, CVector(1.2f, 0, 0), cible, 0.9f, 1.5f, 33, true, tirer, &des);
		VERIF(c.sousEtat == 0 && c.mode == 11 && c.point.x == 0.0f);
		ScriptPedCombatLocoChoisir(c, CVector(0.5f, 0, 0), cible, 0.9f, 1.5f, 33, true, tirer, &des);
		VERIF(c.mode == 10 && c.point.x == 0.5f);                                                      // sur place
		des.i = 2;   // la distance tirée 32767 (le grand rayon), l'angle 1 (≈ -π/4) : 3π/8 sous le cap de la cible au piéton
		c.tirage = true;
		ScriptPedCombatLocoChoisir(c, CVector(1.2f, 0, 0), cible, 0.9f, 1.5f, 33, true, tirer, &des);
		VERIF(c.sousEtat == 1 && c.mode == 8 && fabsf(c.point.x - 1.5f * cosf(-3.0f * 3.14159265f / 8.0f)) < 1e-3f && c.point.y < 0.0f);
		c.tirage = false;
		ScriptPedCombatLocoChoisir(c, c.point, cible, 0.9f, 1.5f, 33, true, tirer, &des);              // arrivé au point
		VERIF(c.sousEtat == 2 && c.minuterieAttente == 0);
		for(int k = 0; k < 9; k++) ScriptPedCombatLocoChoisir(c, c.point, cible, 0.9f, 1.5f, 33, true, tirer, &des);
		VERIF(c.sousEtat == 2);                                                                         // 297 ms
		des.i = 4;   // 2 : pair, l'attente repart
		ScriptPedCombatLocoChoisir(c, c.point, cible, 0.9f, 1.5f, 33, true, tirer, &des);
		VERIF(c.sousEtat == 2 && c.minuterieAttente == 0);
		des.i = 5;   // 3 : impair, retour à l'approche
		for(int k = 0; k < 10; k++) ScriptPedCombatLocoChoisir(c, c.point, cible, 0.9f, 1.5f, 33, true, tirer, &des);
		VERIF(c.sousEtat == 0);
		c.minuterieVue = 990;   // la cible pas vue plus d'une seconde : le retour
		ScriptPedCombatLocoChoisir(c, CVector(1.2f, 0, 0), cible, 0.9f, 1.5f, 33, false, tirer, &des);
		VERIF(c.sousEtat == 3 && c.minuterieVue == 0);
	}
	// FUN_005f38e0 et la garde de FUN_00484490 : mort, endormi (+0x1358) ou tous endormis (DAT_00c2b108), l'IA n'avance pas.
	{
		CScriptPedPool q;
		int b = q.Creer(146, CVector(0, 0, 0), 0, 1);
		q.Acceder(b)->intentionVitesse = 1.5f;
		VERIF(q.IAAvance(b));
		q.ViderEntrees(b);
		VERIF(q.Acceder(b)->intentionVitesse == 0.0f);
		q.Acceder(b)->endormi = true;
		VERIF(!q.IAAvance(b));
		q.Acceder(b)->endormi = false; q.sommeilGeneral = true;
		VERIF(!q.IAAvance(b));
		q.sommeilGeneral = false; q.Sante(b, 0);
		VERIF(q.IAAvance(b));                              // à santé nulle, l'IA tourne encore (FUN_00484490 lit +0x1c98)
		q.Mourir(b);
		VERIF(!q.IAAvance(b) && !q.IAAvance(-1));
	}
	// FUN_0048d320, FUN_0048cf50, FUN_0048ccf0 : la vue (le cône de la colonne D, la portée de la colonne E), son cache et
	// le dernier agresseur (+0x1dbc, +0x1dc0).
	{
		CScriptPedPool q;
		int a = q.Creer(146, CVector(0, 0, 0), 0, 1), b = q.Creer(85, CVector(5, 0, 0), 0, 1);
		int32 col[64] = {};
		col[2] = 100; col[3] = 12; col[4] = 100;              // STAT_B_STRIKER_A : 100 degrés, 12 m
		VERIF(q.PoserStats(a, col) && fabsf(q.Acceder(a)->visionAngle - 1.745329f) < 1e-4f && q.Acceder(a)->visionPortee == 12);
		auto zero = [](void *) { return 0; };                 // rand() = 0 : 4 images en cache
		VERIF(q.Voit(a, a, zero, nil) && !q.Voit(a, 23, zero, nil));
		VERIF(q.Voit(a, b, zero, nil));                       // devant, à 5 m
		q.Acceder(b)->pos = CVector(-5, 0, 0);
		VERIF(q.Voit(a, b, zero, nil));                       // le cache répond encore
		for(int i = 0; i < 4; i++) q.AvancerVue(0.033f);
		VERIF(!q.Voit(a, b, zero, nil));                      // derrière : hors du cône
		q.Acceder(b)->pos = CVector(3, 3.5f, 0);              // à 49,4 degrés du devant, dans le demi-angle de 50
		for(int i = 0; i < 4; i++) q.AvancerVue(0.033f);
		VERIF(q.Voit(a, b, zero, nil));
		q.Acceder(b)->pos = CVector(12.5f, 0, 0);             // au-delà de la portée
		for(int i = 0; i < 4; i++) q.AvancerVue(0.033f);
		VERIF(!q.Voit(a, b, zero, nil));
		// Le dernier agresseur, à moins de 5 m et depuis moins de 5 s, est vu même dans le dos.
		q.Acceder(b)->pos = CVector(-4, 0, 0);
		for(int i = 0; i < 4; i++) q.AvancerVue(0.033f);
		q.Agresse(a, b);
		VERIF(q.Voit(a, b, zero, nil));
		for(int i = 0; i < 152; i++) q.AvancerVue(0.033f);   // 5,02 s
		VERIF(q.Acceder(a)->tempsAgresseur > 5.0f && !q.Voit(a, b, zero, nil));
	}
	// FUN_0061acc0, FUN_0061ad60, FUN_0061afa0, FUN_0062eaa0 : le suivi d'un trajet (le nœud suivant selon le mode, les
	// nœuds d'arrêt, le plan de passage d'un nœud et son franchissement).
	{
		CSuiviTrajet s;
		int32 n = 2;
		s.mode = 0; VERIF(!ScriptPedSuiviSuivant(s, 3, n) && n == 3);   // une fois : fini après le dernier
		n = 2; s.mode = 1; VERIF(ScriptPedSuiviSuivant(s, 3, n) && n == 0);   // en boucle
		n = 2; s.mode = 2; s.sens = 1; VERIF(ScriptPedSuiviSuivant(s, 3, n) && n == 1 && s.sens == -1);   // aller-retour
		n = 0; VERIF(ScriptPedSuiviSuivant(s, 3, n) && n == 1 && s.sens == 1);
		n = 1; s.mode = 3; VERIF(ScriptPedSuiviSuivant(s, 3, n) && n == 1);
		s.mode = 7; VERIF(!ScriptPedSuiviSuivant(s, 3, n));
		std::vector<CNoeudTrajet> t(4);
		t[0].pos = CVector(0, 0, 0); t[1].pos = CVector(10, 0, 0); t[2].pos = CVector(10, 10, 0); t[3].pos = CVector(20, 10, 0);
		s.mode = 0;
		VERIF(!ScriptPedSuiviArret(s, t, 1) && ScriptPedSuiviArret(s, t, 3));
		t[2].attente = 2.0f; VERIF(ScriptPedSuiviArret(s, t, 2)); t[2].attente = 0;
		s.mode = 1; VERIF(!ScriptPedSuiviArret(s, t, 3));
		s.mode = 2; VERIF(ScriptPedSuiviArret(s, t, 0) && ScriptPedSuiviArret(s, t, 3) && !ScriptPedSuiviArret(s, t, 2));
		s.mode = 0; s.sens = 1;
		ScriptPedSuiviPoserPlan(s, t, -1, 1);                 // sans précédent : la normale va vers le nœud 2
		VERIF(s.plan == 1 && fabsf(s.planNormale.y - 1.0f) < 1e-5f);
		ScriptPedSuiviPoserPlan(s, t, 0, 1);                  // un virage à 90 degrés : la bissectrice
		VERIF(s.plan == 1 && fabsf(s.planNormale.x - 0.7071068f) < 1e-4f && fabsf(s.planNormale.y - 0.7071068f) < 1e-4f);
		VERIF(!ScriptPedSuiviPlanPasse(s, CVector(9, -1, 0)) && ScriptPedSuiviPlanPasse(s, CVector(10, 0.5f, 0)));
		ScriptPedSuiviPoserPlan(s, t, 2, 3);                  // le dernier nœud est un arrêt : pas de plan
		VERIF(s.plan == -1 && !ScriptPedSuiviPlanPasse(s, CVector(30, 10, 0)));
		// FUN_0062eb70 : l'arrivée à plat sous le rayon (× 3 à vélo, sauf au dernier nœud d'un trajet suivi une fois),
		// l'échéance d'une attente et le cap pris au nœud (FUN_0062c300).
		s.mode = 0; s.noeud = 1;
		VERIF(ScriptPedSuiviAtteint(s, 4, CVector(9.5f, 0, 5), t[1].pos, 0.6f, false));     // la hauteur ne compte pas
		VERIF(!ScriptPedSuiviAtteint(s, 4, CVector(9, 0, 0), t[1].pos, 0.6f, false) && ScriptPedSuiviAtteint(s, 4, CVector(9, 0, 0), t[1].pos, 0.6f, true));
		s.noeud = 3;
		VERIF(!ScriptPedSuiviAtteint(s, 4, CVector(19, 10, 0), t[3].pos, 0.6f, true));        // dernier nœud, mode 0 : × 1
		s.mode = 1; VERIF(ScriptPedSuiviAtteint(s, 4, CVector(19, 10, 0), t[3].pos, 0.6f, true));
		VERIF(ScriptPedSuiviEcheance(0.0f, 5000) == 0 && ScriptPedSuiviEcheance(2.5f, 5000) == 7500);
		VERIF(ScriptPedSuiviCap(90.0f, false) == 0.0f && ScriptPedSuiviCap(90.0f, true) == 270.0f);
		// FUN_005e7560 : le but lissé, 4 m devant la projection sur le segment (0,0) → (10,0) ; au-delà du bout, le bout ;
		// trop loin du segment, la projection.
		float f = 0;
		CVector b = ScriptPedSuiviBut(t[0].pos, t[1].pos, CVector(2, 1, 0), 4.0f, f);
		VERIF(fabsf(b.x - 6.0f) < 1e-4f && b.y == 0.0f && fabsf(f - 0.6f) < 1e-5f);
		b = ScriptPedSuiviBut(t[0].pos, t[1].pos, CVector(8, 0, 0), 4.0f, f);
		VERIF(b.x == 10.0f && f > 1.0f);
		b = ScriptPedSuiviBut(t[0].pos, t[1].pos, CVector(5, 6, 0), 4.0f, f);
		VERIF(b.x == 5.0f && b.y == 0.0f);
		CScriptPedPool rq;
		VERIF(rq.Acceder(rq.Creer(85, CVector(), 0, 1))->rayonNoeud == 1.0f);   // FUN_0048c430 : 1 m par défaut
	}
	{
		// RegisterPedEventHandler (0x5bbdf0) : la table des paires (piéton, rappel). Ajouter une seconde fois ne double pas la
		// paire (FUN_005b4600), retirer fait avancer les suivantes (FUN_005b4130), rien n'est retiré pendant un appel.
		CGestionnairesPietons g;
		g.Ajouter(6); g.Ajouter(5); g.Ajouter(6);
		VERIF(g.paires.size() == 2 && !g.Pret(6) && g.Case(7) == nil);
		*g.Case(6) = 42;
		VERIF(g.Pret(6) && !g.Pret(5));
		g.enAppel = true;
		VERIF(g.Retirer(6) == CGestionnairesPietons::SANS_RAPPEL && g.paires.size() == 2);
		g.enAppel = false;
		VERIF(g.Retirer(6) == 42 && g.paires.size() == 1 && g.paires[0].ped == 5 && !g.Pret(6));
		// FUN_0047b810 : la méthode du coup ne prévient pas les scripts d'un coup sur un piéton du drapeau 12, ni sur un
		// invulnérable (30) sauf le joueur, ni d'un coup du joueur sur un invulnérable au joueur (20).
		CScriptPedPool c;
		int davis = c.Creer(99, CVector(), 0, 1), bully = c.Creer(146, CVector(), 0, 1);
		VERIF(c.CoupRecevable(bully, SCRIPT_PED_JOUEUR) && c.CoupRecevable(davis, SCRIPT_PED_JOUEUR));
		c.Acceder(davis)->drapeaux[DRAPEAU_INVULNERABLE] = 1;
		c.Acceder(bully)->drapeaux[DRAPEAU_INVULNERABLE_JOUEUR] = 1;
		c.Acceder(SCRIPT_PED_JOUEUR)->drapeaux[DRAPEAU_INVULNERABLE] = 1;
		VERIF(!c.CoupRecevable(davis, SCRIPT_PED_JOUEUR) && !c.CoupRecevable(bully, SCRIPT_PED_JOUEUR) && c.CoupRecevable(bully, davis));
		VERIF(c.CoupRecevable(SCRIPT_PED_JOUEUR, bully));
		c.Acceder(SCRIPT_PED_JOUEUR)->drapeaux[DRAPEAU_INTOUCHABLE] = 1;
		VERIF(!c.CoupRecevable(SCRIPT_PED_JOUEUR, bully));
	}
	{
		// FUN_0062e780 : les actions d'un nœud de trajet, une par une (le premier nœud de hallspatrol_1A, Patrol_School.dat :
		// un virage vers 165 degrés et 2 s d'attente), l'échéance même pour une attente nulle, puis la fin de la liste.
		CNoeudTrajet n;
		CActionNoeud a1; a1.type = 0; a1.lacet = 165.0f; a1.attente = 2.0f;
		CActionNoeud a2; a2.type = 1; a2.attente = 0.0f;
		n.actions = true; n.listeActions = {a1, a2};
		int32 index = 0; uint32 echeance = 0; CActionNoeud joue;
		VERIF(ScriptPedSuiviAction(n, index, 5000, echeance, joue) && joue.type == 0 && joue.lacet == 165.0f && index == 1 && echeance == 7000);
		VERIF(ScriptPedSuiviAction(n, index, 7033, echeance, joue) && joue.type == 1 && index == 2 && echeance == 7033);
		VERIF(!ScriptPedSuiviAction(n, index, 7066, echeance, joue) && index == 0 && echeance == 0);
	}
	{
		// Les attitudes : la courante, puis la défaut si elle vaut 5 (0x46f6e0) ; l'écart d'un CVariableAttitudeSet compté par
		// tiers et borné (FUN_0046f430), remis à 0 par l'écriture (FUN_0046f4b0) ; le piéton d'abord, puis son type (0x475230).
		CScriptPedPool c;
		CAttitudes &t = c.attitudesTypes;
		// Jamais posée, la défaut vaut 5, que la lecture d'un CVariableAttitudeSet borne à 4.
		VERIF(t.TypeVersType(11, SCRIPT_PED_TYPE_JOUEUR) == 4 && t.TypeVersType(SCRIPT_PED_TYPE_JOUEUR, 11) == 2 && t.TypeVersType(12, 0) == 2);
		t.PoserDefaut(11, SCRIPT_PED_TYPE_JOUEUR, 1);           // gamemain : PedSetDefaultTypeToTypeAttitude(11, 13, 1)
		VERIF(t.TypeVersType(11, SCRIPT_PED_TYPE_JOUEUR) == 1);
		t.defaut[11].ecarts[12] = 10;                           // 1 + 10 / 3 = 4
		VERIF(t.TypeVersType(11, SCRIPT_PED_TYPE_JOUEUR) == 4);
		t.defaut[11].ecarts[12] = -7;                           // 1 - 2 : borné à 0
		VERIF(t.TypeVersType(11, SCRIPT_PED_TYPE_JOUEUR) == 0);
		t.PoserDefaut(11, SCRIPT_PED_TYPE_JOUEUR, 1);
		VERIF(t.defaut[11].ecarts[12] == 0);
		t.PoserTypeVersType(11, SCRIPT_PED_TYPE_JOUEUR, 3);
		VERIF(t.TypeVersType(11, SCRIPT_PED_TYPE_JOUEUR) == 3);
		t.PoserDefaut(12, 0, 4);                                // hors des douze tables : rien
		VERIF(t.TypeVersType(0, 0) == 4);
		int b = c.Creer(146, CVector(), 0, 1);
		c.Acceder(b)->type = 11;
		VERIF(c.Attitude(b, SCRIPT_PED_TYPE_JOUEUR) == 3);
		c.Acceder(b)->attitudes.Ecrire(SCRIPT_PED_TYPE_JOUEUR, 0);   // 1_02 : PedSetPedToTypeAttitude(1, 13, 0)
		VERIF(c.Attitude(b, SCRIPT_PED_TYPE_JOUEUR) == 0 && c.Attitude(-1, 0) == 2);
		// La méthode du coup écarte un coup sans arme sur une cible qui aime le type de l'attaquant (4).
		c.Acceder(b)->attitudes.Ecrire(SCRIPT_PED_TYPE_JOUEUR, 4);
		VERIF(!c.CoupRecevable(b, SCRIPT_PED_JOUEUR));
	}
	{
		// FUN_004992a0 : la cible de l'objectif d'attaque (style 3 : valable sans autre jugement), puis l'agresseur récent d'un
		// piéton qui le hait (attitude 0) ; FUN_00499340 lâche une cible morte ; FUN_00497080 en style 0 : la portée de 30 m.
		CScriptPedPool c;
		CScriptPedPool::ButesCibles b;
		int bully = c.Creer(146, CVector(0, 0, 0), 0, 1), eleve = c.Creer(102, CVector(3, 0, 0), 0, 1);
		c.Acceder(bully)->type = 11; c.Acceder(eleve)->type = 1;
		auto tirer = [](void *) -> int { return 0; };
		c.Attaquer(bully, SCRIPT_PED_JOUEUR, 3);
		c.SuivreCibles(0.033f, 1000, b, tirer, nil);
		VERIF(c.Acceder(bully)->verrou == SCRIPT_PED_JOUEUR && c.Acceder(bully)->verrouMode == 3 && c.Acceder(eleve)->verrou == -1);
		c.Agresse(eleve, bully);                                 // l'élève, frappé par le bully, ne l'aime pas
		c.attitudesTypes.PoserDefaut(1, 11, 0);
		c.SuivreCibles(0.033f, 1033, b, tirer, nil);
		VERIF(c.Acceder(eleve)->verrou == bully && c.Acceder(eleve)->verrouMode == 0);
		c.attitudesTypes.PoserDefaut(1, 11, 2);                  // neutre et pas une autorité : il ne riposte pas
		int autre = c.Creer(102, CVector(3, 0, 0), 0, 1);
		c.Acceder(autre)->type = 1; c.Agresse(autre, bully);
		c.SuivreCibles(0.033f, 1066, b, tirer, nil);
		VERIF(c.Acceder(autre)->verrou == -1);
		c.Acceder(bully)->mort = true;                           // une cible morte est lâchée
		c.SuivreCibles(0.033f, 1100, b, tirer, nil);
		VERIF(c.Acceder(eleve)->verrou == -1);
		c.Acceder(bully)->mort = false;
		c.Acceder(bully)->pos = CVector(40, 0, 0);
		VERIF(!c.CibleValable(eleve, bully, 0, true, true, b, tirer, nil) && c.CibleValable(eleve, bully, 0, false, true, b, tirer, nil));
		VERIF(c.CibleValable(eleve, bully, 3, true, true, b, tirer, nil) && !c.CibleValable(eleve, eleve, 3, false, true, b, tirer, nil));
	}
	{
		// Les droits d'attaque (FUN_00486c40, FUN_004861c0, FUN_00486c00, FUN_00487780) : un seul détenteur, rendus par lui
		// seul, lâchés quand il change de cible, échus après 15 s.
		CScriptPedPool c;
		int a = c.Creer(146, CVector(), 0, 1), b = c.Creer(102, CVector(), 0, 1);
		VERIF(!c.PrendreDroits(a, 1000));                       // sans cible, rien
		c.Verrouiller(a, SCRIPT_PED_JOUEUR, 0); c.Verrouiller(b, SCRIPT_PED_JOUEUR, 0);
		VERIF(c.PrendreDroits(a, 1000) && c.DroitsPrisSurCible(a) && c.DroitsPrisSurCible(b));
		VERIF(!c.RendreDroits(b) && c.DroitsPrisSurCible(a));   // b ne les tient pas
		c.Verrouiller(a, b, 0);                                  // a change de cible : il lâche ceux du joueur
		VERIF(!c.DroitsPrisSurCible(b) && c.Acceder(SCRIPT_PED_JOUEUR)->detenteurDroits == -1);
		VERIF(c.PrendreDroits(b, 2000));
		c.AvancerDroits(16999); VERIF(c.DroitsPrisSurCible(b));
		c.AvancerDroits(17000); VERIF(!c.DroitsPrisSurCible(b));
	}
	{
		// Les piétons proches (FUN_0047d1d0 : 30 m à plat, triés) et les jetons de combat (FUN_00486e40) : un attaquant à 3 m
		// dans la zone 0 du joueur reçoit un jeton de la catégorie 0 ; à 1 m, face au joueur, la place du cône.
		CScriptPedPool c;
		int32 n[4] = {2, 1, 1, 1};
		c.PoserJetons(SCRIPT_PED_JOUEUR, n);
		int a = c.Creer(146, CVector(3, 0, 0), 180, 1), loin = c.Creer(102, CVector(40, 0, 0), 0, 1);
		c.ReleverProches(SCRIPT_PED_JOUEUR, true);
		VERIF(c.Acceder(SCRIPT_PED_JOUEUR)->proches.size() == 1 && c.Acceder(SCRIPT_PED_JOUEUR)->proches[0] == a);
		c.Verrouiller(a, SCRIPT_PED_JOUEUR, 0); c.Verrouiller(loin, SCRIPT_PED_JOUEUR, 0);
		auto zero = [](void *) -> int { return 0; };
		CScriptPedPool::ButesJetons b;
		c.DistribuerJetons(SCRIPT_PED_JOUEUR, 1000, b, zero, nil);
		VERIF(!c.TientJeton(a));                                 // hors de toute zone de combat : rien
		c.Acceder(SCRIPT_PED_JOUEUR)->combattants[0].push_back(a);
		c.DistribuerJetons(SCRIPT_PED_JOUEUR, 1033, b, zero, nil);
		VERIF(c.TientJeton(a) && c.Acceder(SCRIPT_PED_JOUEUR)->jetons[0][0].detenteur == a && !c.TientJeton(loin));
		c.Acceder(SCRIPT_PED_JOUEUR)->jetons[0][0].coupe = true; VERIF(!c.TientJeton(a));
		c.Acceder(SCRIPT_PED_JOUEUR)->jetons[0][0].coupe = false;
		c.DistribuerJetons(a, 12000, b, zero, nil);              // l'attaquant n'a pas frappé depuis 11 s : son jeton échoit
		VERIF(!c.TientJeton(a));
		c.Acceder(a)->pos = CVector(1, 0, 0);                    // à 1 m, tourné vers le joueur (180 degrés)
		c.DistribuerJetons(SCRIPT_PED_JOUEUR, 12033, b, zero, nil);
		VERIF(c.Acceder(SCRIPT_PED_JOUEUR)->jetonCone.detenteur == a && c.TientJeton(a));
	}
	{
		// Les gestionnaires des déclencheurs (RegisterTriggerEventHandler 0x5bbfa0) : dix entrées en place, le tas que
		// SetNumberOfHandledTriggerEventObjects agrandit, une entrée retirée quand sa dernière case se vide, la table du
		// propriétaire vidée à la fin de son script.
		CGestionnairesDeclencheurs g;
		for(int t = 0; t < 10; t++) VERIF(g.Ajouter(1, t));
		VERIF(!g.Ajouter(1, 10) && g.Ajouter(1, 3));             // pleine ; une entrée déjà là est rendue
		g.tables[1].entrees.clear();
		g.Dimensionner(1, 12, 7);
		VERIF(g.tables[1].enPlus == 2 && g.tables[1].proprietaire == 7);
		for(int t = 0; t < 12; t++) VERIF(g.Ajouter(1, t));
		VERIF(!g.Ajouter(1, 12));
		g.Dimensionner(1, 3, 8);                                   // d'un autre script, la table pleine : rien
		VERIF(g.tables[1].proprietaire == 7 && g.tables[1].entrees.size() == 12);
		*g.Case(1, 5, CGestionnairesDeclencheurs::SORTIE_PIETON) = 41;
		*g.Case(1, 5, CGestionnairesDeclencheurs::DEDANS) = 42;
		VERIF(g.Pret(1, 5, 3) && !g.Pret(1, 5, 0) && !g.Pret(0, 5, 3) && g.Case(1, 99, 0) == nil);
		VERIF(g.Suivie(1, 5) && !g.Suivie(1, 4));
		*g.Case(1, 4, CGestionnairesDeclencheurs::PROJECTILE) = 43;
		VERIF(!g.Suivie(1, 4));                                    // le projectile seul ne fait pas suivre les piétons
		VERIF(g.Retirer(1, 5, 3) == 41 && g.Pret(1, 5, 5) && g.tables[1].entrees.size() == 12);
		VERIF(g.Retirer(1, 5, 5) == 42 && g.Case(1, 5, 5) == nil && g.tables[1].entrees.size() == 11);
		g.Ajouter(0, 2); *g.Case(0, 2, 1) = 50; g.tables[0].entrees[0].scripts[1] = 7;
		std::vector<int> l = g.OublierScript(7);                   // propriétaire de la table 1, auteur d'un rappel de la table 0
		VERIF(l.size() == 2 && g.tables[1].entrees.empty() && g.tables[1].proprietaire == -1 && g.tables[0].entrees.empty());

		// FUN_006d6f60 : l'entrée (0 pour un piéton, 1 pour le joueur) et l'événement 5 à la première image dedans, 5 ensuite,
		// la sortie (3 ou 4) à la première image dehors ; un déclencheur « joueur seul » ne suit pas les autres piétons.
		typedef CGestionnairesDeclencheurs G;
		g.Ajouter(1, 30); for(int e = 0; e < G::EVENEMENTS; e++) *g.Case(1, 30, e) = 60 + e;
		bool dedans = true, joueurSeul = false;
		auto dans = [&](int){ return dedans; };
		auto seul = [&](int){ return joueurSeul; };
		std::vector<G::Appel> v = g.Suivre(4, false, dans, seul);
		VERIF(v.size() == 2 && v[0].evenement == G::ENTREE_PIETON && v[1].evenement == G::DEDANS && g.dedans.size() == 1);
		v = g.Suivre(4, false, dans, seul);
		VERIF(v.size() == 1 && v[0].evenement == G::DEDANS);
		v = g.Suivre(SCRIPT_PED_JOUEUR, true, dans, seul);
		VERIF(v.size() == 2 && v[0].evenement == G::ENTREE_JOUEUR && g.dedans.size() == 2);
		dedans = false;
		v = g.Suivre(4, false, dans, seul);
		VERIF(v.size() == 1 && v[0].evenement == G::SORTIE_PIETON && g.dedans.size() == 1);
		v = g.Suivre(4, false, dans, seul);
		VERIF(v.empty());
		dedans = true; joueurSeul = true;
		VERIF(g.Suivre(4, false, dans, seul).empty() && g.Suivre(SCRIPT_PED_JOUEUR, true, dans, seul).size() == 1);
	}
	{
		// Le groupe des alliés : PedRecruitAlly (FUN_00485fa0), la chaîne des suiveurs, PedDismissAlly (FUN_00485880).
		CScriptPedPool g;
		int algie = g.Creer(4, CVector(1, 0, 0), 0, 1), autre = g.Creer(5, CVector(2, 0, 0), 0, 1), prefet = g.Creer(6, CVector(), 0, 1);
		g.Acceder(prefet)->type = 0;
		CScriptPed *j = g.Acceder(SCRIPT_PED_JOUEUR);
		j->pos = CVector(10, 0, 0); j->type = SCRIPT_PED_TYPE_JOUEUR;
		VERIF(g.RecruterAllie(SCRIPT_PED_JOUEUR, algie, true));
		CScriptPed *a = g.Acceder(algie);
		VERIF(j->suiveur == algie && a->meneur == SCRIPT_PED_JOUEUR && a->etatAllie == 1 && a->moralAllie == 0 && a->groupe304);
		// La laisse de 4 m au premier meneur, posée à sa position ; l'attitude envers son type à 4 ; l'octet +0xc du meneur.
		VERIF(a->laisse == 2 && a->laissePieton == SCRIPT_PED_JOUEUR && a->laisseRayon == 4.0f && a->laissePoint.x == 10.0f);
		VERIF(a->attitudes.Lire(SCRIPT_PED_TYPE_JOUEUR) == 4 && j->groupeTenu && a->laisse3b4 && a->laisse3b5);
		VERIF(g.PremierMeneur(algie) == SCRIPT_PED_JOUEUR && g.PremierMeneur(SCRIPT_PED_JOUEUR) == -1);
		VERIF(g.DansSesSuiveurs(SCRIPT_PED_JOUEUR, algie) && g.DansSonGroupe(algie, SCRIPT_PED_JOUEUR) && !g.DansSonGroupe(autre, algie));
		// FUN_00477ab0 : le genre 0x10 vise le suiveur d'un meneur, le meneur d'un suiveur ; 4 l'objet visé ; 1 le partenaire de prise.
		VERIF(g.CibleDeGenre(SCRIPT_PED_JOUEUR, 0x10).pieton == algie && g.CibleDeGenre(algie, 0x10).pieton == SCRIPT_PED_JOUEUR);
		a->objetVise = 7; a->syncVise = 3; a->cibleDePrise = autre;
		VERIF(g.CibleDeGenre(algie, 4).objet == 7 && g.CibleDeGenre(algie, 4).point == 3 && g.CibleDeGenre(algie, 4).pieton == -1);
		VERIF(g.CibleDeGenre(algie, 1).pieton == autre && g.CibleDeGenre(algie, 9).pieton == -1 && g.CibleDeGenre(algie, 9).objet == -1);
		a->objetVise = -1; a->syncVise = 0; a->cibleDePrise = -1;
		// PedHasAlly : le meneur et le suiveur en ont un, pas l'autre ; PedIsModel : le modèle de création.
		VERIF(g.AUnAllie(SCRIPT_PED_JOUEUR) && g.AUnAllie(algie) && !g.AUnAllie(autre) && !g.AUnAllie(-1));
		VERIF(g.EstDuModele(algie, 4) && !g.EstDuModele(algie, 5) && !g.EstDuModele(-1, 4));
		// Déjà suiveur : rien ne change. Un suiveur qui mène à son tour : la laisse va au premier meneur.
		a->etatAllie = 3;
		VERIF(g.RecruterAllie(SCRIPT_PED_JOUEUR, algie, true) && a->etatAllie == 3);
		VERIF(g.RecruterAllie(algie, autre, true) && g.PremierMeneur(autre) == SCRIPT_PED_JOUEUR);
		VERIF(g.Acceder(autre)->laissePieton == SCRIPT_PED_JOUEUR && g.DansSesSuiveurs(SCRIPT_PED_JOUEUR, autre));
		// Sans forcer : ni préfet, ni attitude autre que 4.
		VERIF(!g.RecruterAllie(SCRIPT_PED_JOUEUR, prefet, false));
		// Renvoyer le premier suiveur lui fait d'abord renvoyer le sien ; les champs de FUN_00485820, la laisse lâchée.
		VERIF(g.RenvoyerAllie(SCRIPT_PED_JOUEUR, algie));
		VERIF(j->suiveur == -1 && a->meneur == -1 && a->suiveur == -1 && a->laisse == 0 && a->etatAllie == 1 && a->sprintSansFin);
		VERIF(g.Acceder(autre)->meneur == -1 && g.Acceder(autre)->laisse == 0);
		// Un nouveau suiveur remplace l'ancien ; le piéton supprimé n'est plus tenu par personne.
		VERIF(g.RecruterAllie(SCRIPT_PED_JOUEUR, algie, true) && g.RecruterAllie(SCRIPT_PED_JOUEUR, autre, true));
		VERIF(j->suiveur == autre && a->meneur == -1);
		VERIF(g.Supprimer(autre) && j->suiveur == -1);
	}
	printf("test_scriptpeds : %s\n", echecs == 0 ? "OK" : "ECHEC");
	return echecs != 0;
}
