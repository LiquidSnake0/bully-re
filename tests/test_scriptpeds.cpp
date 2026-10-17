// Le pool des piétons des scripts : 24 entrées, le joueur en 0, -1 quand c'est plein, les handles
// libérés repris, et le ramassage par objet script. Aucun fichier du jeu.
//   build/tests/test_scriptpeds
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

	printf("test_scriptpeds : %s\n", echecs == 0 ? "OK" : "ECHEC");
	return echecs != 0;
}
