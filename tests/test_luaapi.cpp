// La frontière moteur / scripts : les 1 508 fonctions de docs/api-lua-tables.tsv sont des fonctions
// du moteur, et rien d'autre ne l'est. Aucun fichier du jeu.
//   build/tests/test_luaapi   (depuis la racine du dépôt)
#include "../src/core/LuaApi.h"
#include <cstdio>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CLuaApi api;
	VERIF(!api.Charger("docs/n-existe-pas.tsv"));
	VERIF(api.Charger("docs/api-lua-tables.tsv"));
	VERIF(api.Nombre() == 1508);
	// Des fonctions du moteur, dans plusieurs tables.
	VERIF(api.EstFonction("PedMoveToPoint") && api.EstFonction("PedAttack") && api.EstFonction("CameraFade"));
	VERIF(api.EstFonction("IsMissionCompleated") && api.EstFonction("SystemAllowMissionManagerToForceRunMissions"));
	VERIF(api.EstFonction("ImportScript") && api.EstFonction("GetTimer") && api.EstFonction("Wait"));
	// Des globales de script : nil tant que le script ne les pose pas.
	VERIF(!api.EstFonction("gMissionSuccess") && !api.EstFonction("gPedroCleaned") && !api.EstFonction("gSecPathNode"));
	VERIF(!api.EstFonction("shared") && !api.EstFonction("stageFunction"));
	printf("test_luaapi : %s\n", echecs == 0 ? "OK" : "ECHEC");
	return echecs != 0;
}
