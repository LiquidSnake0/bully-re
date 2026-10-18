// Le gestionnaire de zones (src/core/AreaManager.h), sans fichier du jeu : la table des scripts de
// zone, la demande de FUN_00415640, et la machine à états de FUN_00416e30 image par image, dans
// les trois cas qu'elle connaît : la même zone sans script chargé (la nouvelle partie), une autre
// zone demandée par un script (fondu immédiat), une autre zone par une porte (14 images d'attente).
//   build/tests/test_zones
#include "../src/core/AreaManager.h"
#include <cstdio>
#include <cstring>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CAreaManager z;
	// AreaRegisterAreaScript : 45 appels dans gamemain, codes 0 à 62 ; un nom de 100 caractères ou plus est ignoré.
	z.EnregistrerScript(0, "AreaScripts/MainMap.lua");
	z.EnregistrerScript(2, "AreaScripts/SchoolHallways.lua");
	z.EnregistrerScript(5, "AreaScripts/PrincipalOffice.lua");
	z.EnregistrerScript(62, "AreaScripts/BMXTrack.lua");
	char trop[AREA_SCRIPT_MAX + 1]; memset(trop, 'a', AREA_SCRIPT_MAX); trop[AREA_SCRIPT_MAX] = 0;
	z.EnregistrerScript(3, trop);
	char juste[AREA_SCRIPT_MAX]; memset(juste, 'b', AREA_SCRIPT_MAX - 1); juste[AREA_SCRIPT_MAX - 1] = 0;
	z.EnregistrerScript(4, juste);
	VERIF(strcmp(z.Script(2), "AreaScripts/SchoolHallways.lua") == 0 && strcmp(z.Script(62), "AreaScripts/BMXTrack.lua") == 0);
	VERIF(z.Script(3)[0] == 0 && strlen(z.Script(4)) == AREA_SCRIPT_MAX - 1 && z.Script(1)[0] == 0 && z.Script(64)[0] == 0 && z.Script(-1)[0] == 0);
	VERIF(z.zoneVisible == 0 && !z.EnChargement() && z.attenteImages == 14);

	// 1. La nouvelle partie : gamemain fait PlayerSetPosXYZArea(272.68, -73.13, 6.97, 0) ; la zone 0 est déjà
	//    visible mais son script n'est pas chargé : 1 → 5 → 6, le script de zone est demandé, puis 8, 9, 0xb, 0xc, 0xd.
	CVector grille(272.68f, -73.13f, 6.97f);
	VERIF(z.Demander(grille, 0, 0));
	VERIF(z.EnChargement() && !z.Demander(grille, 0, 2));      // une seconde demande est ignorée tant que ça charge
	z.AvancerImage(FONDU_RENDU, false, false);                   // même zone, pas de fondu : 1 → 5 → 6
	VERIF(z.etat == AREA_CHARGER && z.evenements.decharger && z.evenements.charger == nil && !z.evenements.joueurPose && !z.evenements.fonduNoir);
	z.AvancerImage(FONDU_RENDU, false, false);                   // les scripts de zone ne sont pas encore partis
	VERIF(z.etat == AREA_CHARGER && !z.evenements.decharger);
	z.AvancerImage(FONDU_RENDU, true, false);
	VERIF(z.etat == AREA_PRET && z.evenements.charger != nil && strcmp(z.evenements.charger, "AreaScripts/MainMap.lua") == 0);
	z.AvancerImage(FONDU_RENDU, true, true);                     // pas encore prêt
	VERIF(z.etat == AREA_PRET && !z.evenements.joueurPose && z.zoneVisible == 0);
	z.SignalerPret();
	z.AvancerImage(FONDU_RENDU, true, true);                     // 9 : zone et joueur posés ; 0xb : le fondu revient ; 0xc, 0xd, 0
	VERIF(z.etat == AREA_REPOS && z.evenements.joueurPose && z.evenements.zoneChangee && z.evenements.fonduRetour && z.zoneVisible == 0);
	VERIF(z.demande.pos.x == grille.x && z.demande.zone == 0);
	VERIF(!z.EnChargement());

	// 2. Un script téléporte le joueur dans le hall de l'école (PlayerSetPosPointArea(2, …), l'octet +0x8dc posé) :
	//    1 → 2 (une image), 2 → 3 avec le fondu au noir, 3 attend le fondu, puis une image de plus, 5, 6, 8, 9…
	CVector hall(-633.9f, -292.4f, 5.5f);
	VERIF(z.Demander(hall, -90, 2, nil, true));
	z.AvancerImage(FONDU_RENDU, false, true);
	VERIF(z.etat == AREA_FONDU_DEMANDE && !z.evenements.fonduNoir);
	z.AvancerImage(FONDU_RENDU, false, true);
	VERIF(z.etat == AREA_FONDU && z.evenements.fonduNoir);
	for(int i = 0; i < 5; i++) z.AvancerImage(FONDU_EN_COURS, false, true);
	VERIF(z.etat == AREA_FONDU && z.zoneVisible == 0 && z.EnChargement());   // tant que l'écran n'est pas noir
	z.AvancerImage(FONDU_NOIR, false, true);                     // l'écran est noir : une image pour FUN_0055e700
	VERIF(z.etat == AREA_FONDU);
	z.AvancerImage(FONDU_NOIR, false, true);                     // 5 : les scripts de zone sont à retirer ; 6 attend
	VERIF(z.etat == AREA_CHARGER && z.evenements.decharger && !z.immediat);
	z.AvancerImage(FONDU_NOIR, true, false);
	VERIF(z.etat == AREA_PRET && z.evenements.charger && strcmp(z.evenements.charger, "AreaScripts/SchoolHallways.lua") == 0);
	z.SignalerPret();
	z.AvancerImage(FONDU_NOIR, true, true);                      // 9 → 0xb (le fondu revient) → 0xc → 0xd → 0 dans la même image
	VERIF(z.etat == AREA_REPOS && z.zoneVisible == 2 && z.evenements.zoneChangee && z.evenements.joueurPose && z.evenements.fonduRetour);
	VERIF(z.demande.pos.x == hall.x && z.demande.cap == -90);
	// Si le fondu de retour est encore en cours, 0xc attend.
	VERIF(z.Demander(grille, 0, 0, nil, true));
	z.AvancerImage(FONDU_NOIR, false, false);                    // l'écran est resté noir : 1 → 5 → 6 sans nouveau fondu
	VERIF(z.etat == AREA_CHARGER && !z.evenements.fonduNoir && z.evenements.decharger);
	z.AvancerImage(FONDU_NOIR, true, false); z.SignalerPret();
	z.AvancerImage(FONDU_EN_COURS, true, true);
	VERIF(z.etat == AREA_FONDU_RETOUR_FINI && z.zoneVisible == 0 && z.evenements.fonduRetour);
	z.AvancerImage(FONDU_RENDU, true, true);
	VERIF(z.etat == AREA_REPOS);

	// 3. Une porte (FUN_00416e30, état 0, sans +0x8dc) : 14 images d'attente avant un fondu immédiat.
	VERIF(z.Demander(hall, -90, 2, nil, true)); z.AvancerImage(FONDU_RENDU, false, true); z.AvancerImage(FONDU_RENDU, false, true);
	z.AvancerImage(FONDU_NOIR, false, true); z.AvancerImage(FONDU_NOIR, false, true); z.AvancerImage(FONDU_NOIR, true, false);
	z.SignalerPret(); z.AvancerImage(FONDU_NOIR, true, true);
	VERIF(z.etat == AREA_REPOS && z.zoneVisible == 2);           // retour dans le hall, pour repartir par une porte
	VERIF(z.Demander(grille, 180, 0, nil, false));
	z.AvancerImage(FONDU_RENDU, false, true);                    // 0 → 1 → 2 : une autre zone, l'écran n'est pas noir
	VERIF(z.etat == AREA_FONDU_DEMANDE);
	z.AvancerImage(FONDU_RENDU, false, true);                    // 2 : pas immédiat, le compte à rebours part
	VERIF(z.etat == AREA_ATTENTE && z.compteARebours == 14 && !z.evenements.fonduNoir);
	int images = 0;
	while(z.etat == AREA_ATTENTE && images < 100){ z.AvancerImage(FONDU_RENDU, false, true); images++; }
	VERIF(images == 14 && z.etat == AREA_FONDU && z.evenements.fonduNoir);
	z.AvancerImage(FONDU_NOIR, false, true); z.AvancerImage(FONDU_NOIR, false, true);
	VERIF(z.etat == AREA_CHARGER && z.evenements.decharger);
	z.AvancerImage(FONDU_NOIR, true, false);
	VERIF(z.etat == AREA_PRET && strcmp(z.evenements.charger, "AreaScripts/MainMap.lua") == 0);
	z.SignalerPret();
	z.AvancerImage(FONDU_NOIR, true, true);
	VERIF(z.etat == AREA_REPOS && z.zoneVisible == 0 && z.evenements.joueurPose && z.demande.cap == 180);

	// 4. Même zone, script déjà chargé : 1 → 7 → 10 → 0xd → 0 dans l'image, sans fondu ni script.
	VERIF(z.Demander(hall, 0, 0, nil, true));
	z.AvancerImage(FONDU_RENDU, false, true);
	VERIF(z.etat == AREA_REPOS && z.evenements.joueurPose && !z.evenements.zoneChangee && !z.evenements.fonduNoir && !z.evenements.decharger && z.evenements.charger == nil);
	printf("  nouvelle partie, téléportation par un script, porte, même zone : la zone visible ne change qu'à l'état 9\n");
	printf("test_zones : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
