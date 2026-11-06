// Les composants du HUD (src/hud/HudComponents.h), sans fichier du jeu : tous cachés au démarrage (FUN_0053cc70),
// ToggleHUDComponentVisibility, le verrou, HUDClearAllElements, HUDSaveVisibility / HUDRestoreVisibility, et la
// condition HudComponentVisible (FUN_0065aee0).
//   build/tests/test_hud
#include "../src/hud/HudComponents.h"
#include "../src/hud/EcranDeFin.h"
#include "../src/hud/ObjectifsMission.h"
#include "../src/hud/ButtonHistory.h"
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
	// L'écran de fin (src/hud/EcranDeFin.h) : MinigameSetCompletion, MinigameAddCompletionMsg, MinigameIsShowingCompletion.
	CEcranDeFin e;
	VERIF(!e.Montre());
	e.Poser("M_PASS", true, 500.0f, nil);
	VERIF(e.Montre() && e.reussi && e.argent == 500.0f && e.minuterie == 2.0f && e.nombreMessages == 0);
	e.Ajouter("MRESPECT_NP5", 2); e.Ajouter("", 1); e.Ajouter("MRESPECT_BM10", 1);
	VERIF(e.nombreMessages == 2 && e.messages[1].texte == "MRESPECT_BM10" && e.minuterieMessages == 2.0f);
	for(int k = 0; k < 6; k++) e.Ajouter("X", k);
	VERIF(e.nombreMessages == 5);
	int images = 0;
	while(e.Montre() && images < 1000){ e.Avancer(33.0f / 20.0f); images++; }
	VERIF(images >= 60 && images <= 61 && e.minuterie == 0.0f);   // 2 s par pas de 0,033 s
	// Les objectifs de mission (src/hud/ObjectifsMission.h) : MissionObjectiveAdd, MissionObjectiveComplete, MissionObjectiveRemove.
	CObjectifsMission om;
	VERIF(om.Ajouter("1_05_MOBJ_01", 0, false, 1000) == 0 && om.tete == 0 && om.nombre == 1 && om.echeance == 181000);
	VERIF(om.Ajouter("1_05_MOBJ_00", 0, false, 2000) == 1 && om.tete == 1 && om.objectifs[1].suivant == 0 && om.rappel == "1_05_MOBJ_00");
	VERIF(om.Ajouter("X", 0, true, 3000) == -1 && om.nombre == 2);   // l'écran de fin se montre
	om.Terminer(0, true, 4000);
	VERIF(om.objectifs[0].fait && !om.objectifs[1].fait);
	om.Retirer(0, 5000);
	VERIF(om.nombre == 1 && om.tete == 1 && om.objectifs[1].suivant == -1 && !om.objectifs[0].pris && om.libres == 0);
	VERIF(om.Ajouter("1_05_MOBJ_02", 0, false, 6000) == 0 && om.tete == 0 && om.objectifs[0].suivant == 1);
	for(int k = 0; k < 8; k++) om.Ajouter("Y", 0, false, 7000);
	VERIF(om.nombre == 10 && om.Ajouter("Z", 0, false, 8000) == -1);
	// L'historique des boutons dans le mode de l'arbre (FUN_0053b370, FUN_0053c250) : la première leçon de 1_06_01, le bouton 6 tenu.
	{
		CButtonHistory h;
		h.parArbre = true;
		h.Effacer();
		VERIF(h.echecAnnonce && h.nombre == 0);
		h.Ajouter(6, true, 0x2ee);
		h.IgnorerManette(false, 1000);
		VERIF(h.nombre == 1 && h.tenus == 1 && h.masque == 1u << 6 && h.pas[1].bouton == 6);   // le pas tenu recopié derrière
		CButtonHistory::Evenements e = h.Avancer(1100, true, true, true);
		VERIF(!e.reussie && !e.ratee && !e.bonBouton);
		h.Rapporter(6, 0, 1200);                // pressé : le bon bouton, pas encore tenu
		e = h.Avancer(1200, true, true, true);
		VERIF(!e.reussie && !e.ratee && h.boutonRapporte == 0 && h.courant == 0);
		e = h.Avancer(1950, true, true, true);   // 750 ms après le dernier rapport : pas encore
		VERIF(!e.ratee);
		e = h.Avancer(1951, true, true, true);
		VERIF(e.ratee && e.bouton == 6 && !e.expiree && h.echouee);
		e = h.Avancer(1990, true, true, true);   // l'échec n'est annoncé qu'une fois
		VERIF(!e.ratee);
		h.Effacer(); h.Ajouter(6, true, 0x2ee); h.IgnorerManette(false, 3000);
		h.Rapporter(6, 3, 3100);                // tenu
		e = h.Avancer(3100, true, true, true);
		VERIF(e.reussie && e.bonBouton && h.manetteIgnoree && !h.echouee);
		e = h.Avancer(3200, true, true, true);
		VERIF(!e.reussie);                       // la manette est ignorée après la réussite
		// Hors du mode de l'arbre, un rapport ne compte pas.
		CButtonHistory m;
		m.Rapporter(9, 1, 10);
		VERIF(m.boutonRapporte == 0);
		// La durée de la séquence (+0xb4) ne court pas dans ce mode (+0xb8 reste à 0).
		m.parArbre = true; m.Effacer(); m.Ajouter(9, false, 1000000000); m.PoserDuree(5000); m.IgnorerManette(false, 0);
		e = m.Avancer(100000, true, true, true);
		VERIF(!e.ratee && !e.expiree);
	}
	printf("test_hud : %s\n", echecs ? "ÉCHEC" : "OK");
	return echecs ? 1 : 0;
}
