// Les composants du HUD (src/hud/HudComponents.h), sans fichier du jeu : tous cachés au démarrage (FUN_0053cc70),
// ToggleHUDComponentVisibility, le verrou, HUDClearAllElements, HUDSaveVisibility / HUDRestoreVisibility, et la
// condition HudComponentVisible (FUN_0065aee0).
//   build/tests/test_hud
#include "../src/hud/HudComponents.h"
#include <cstdio>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CHudComponents hud;
	for(int i = 0; i < HUD_COMPOSANTS; i++) VERIF(!hud.Visible(i) && !hud.Verrouille(i) && !hud.Clignote(i));
	VERIF(!hud.Visible(-1) && !hud.Visible(HUD_COMPOSANTS));
	VERIF(!HudComponentVisible(hud, HUD_SOCIAL));
	hud.Basculer(11, true);                  // le radar
	hud.Basculer(HUD_SOCIAL, true);
	VERIF(hud.Visible(11) && HudComponentVisible(hud, HUD_SOCIAL) && !HudComponentVisible(hud, 30));
	hud.composants[11].clignote = true;
	hud.Basculer(11, false);                 // caché, il ne clignote plus
	VERIF(!hud.Visible(11) && !hud.Clignote(11));
	hud.Verrouiller(4, true);
	VERIF(hud.Verrouille(4) && !hud.Visible(4));
	hud.Basculer(11, true);
	hud.Sauver();                            // HUDSaveVisibility : rangé, puis tout caché
	VERIF(!hud.Visible(11) && !hud.Visible(HUD_SOCIAL) && hud.ranges[11] && hud.ranges[HUD_SOCIAL] && !hud.ranges[4]);
	hud.Restaurer();
	VERIF(hud.Visible(11) && hud.Visible(HUD_SOCIAL) && !hud.Visible(4));
	hud.Sauver(11);                          // le composant épargné garde sa visibilité
	VERIF(hud.Visible(11) && !hud.Visible(HUD_SOCIAL));
	hud.Restaurer(11);
	hud.ToutCacher();
	for(int i = 0; i < HUD_COMPOSANTS; i++) VERIF(!hud.Visible(i));
	VERIF(hud.Verrouille(4));                // HUDClearAllElements ne touche pas au verrou
	hud.Piste(30, true);                     // HUDComponentVisiblityTrack : l'invite d'humiliation
	VERIF(HudComponentVisible(hud, 30));
	printf("test_hud : %s\n", echecs ? "ÉCHEC" : "OK");
	return echecs ? 1 : 0;
}
