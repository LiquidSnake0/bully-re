// Le pool des piétons des scripts : 24 entrées, le joueur en 0, -1 quand c'est plein, les handles
// libérés repris, le ramassage par objet script ; puis les gestes : marche, sprint, arrêt, bagarre
// et mort. Aucun fichier du jeu.
//   build/tests/test_scriptpeds
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
	int images = 0; bool arrive = false;
	for(; images < 400 && !arrive; images++){ q.Avancer(0.033f); arrive = q.Acceder(m)->arrive; }
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
	// Un mort ne marche plus, n'attaque plus ; PedSetHealth à 0 tue et lâche le geste.
	VERIF(!q.Aller(ci, CVector(), ALLURE_MARCHE) && !q.Attaquer(ci, at));
	int c = q.Creer(99, CVector(0, 5, 0), 0, 1);
	VERIF(q.Aller(c, CVector(0, 9, 0), ALLURE_MARCHE));
	q.Sante(c, 0);
	VERIF(q.Acceder(c)->mort && q.Acceder(c)->vientDeMourir && !q.Acceder(c)->enMarche);
	// PedMakeAmbient : le piéton reste, mais n'est plus ramassé avec son script ; pas le joueur.
	VERIF(!q.RendreAmbiant(SCRIPT_PED_JOUEUR) && q.RendreAmbiant(at) && q.Acceder(at)->script == -1);
	VERIF(q.SupprimerDuScript(1) == 3 && q.Valide(at));   // m, ci et c libérés, a reste

	// FUN_0047b6b0 / FUN_004782a0 : l'invulnérabilité, l'invulnérabilité au joueur et la santé minimale.
	{
		CScriptPedPool q;
		int a = q.Creer(85, CVector(0, 0, 0), 0, 1), b = q.Creer(99, CVector(1, 0, 0), 0, 1);
		CScriptPed *pa = q.Acceder(a), *pj = q.Acceder(SCRIPT_PED_JOUEUR);
		VERIF(q.Blesser(a, 30, b) && pa->sante == 70);
		pa->invulnerable = true;
		VERIF(!q.Blesser(a, 30, b) && pa->sante == 70);
		pa->invulnerable = false; pa->invulnerableAuJoueur = true;
		VERIF(!q.Blesser(a, 30, SCRIPT_PED_JOUEUR) && q.Blesser(a, 30, b) && pa->sante == 40);
		pa->santeMin = 75;                                    // plus bas que la santé actuelle : elle remonte au plancher
		VERIF(q.Blesser(a, 10, b) && pa->sante == 75 && !pa->mort);
		pa->santeMin = 0;
		VERIF(q.Blesser(a, 500, b) && pa->sante == 0 && pa->mort);
		pj->invulnerable = true;
		VERIF(!q.Blesser(SCRIPT_PED_JOUEUR, 10, b) && pj->sante == 100);
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
	printf("test_scriptpeds : %s\n", echecs == 0 ? "OK" : "ECHEC");
	return echecs != 0;
}
