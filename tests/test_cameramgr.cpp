// Le gestionnaire de caméras (src/render/CameraMgr.h), sans fichier du jeu : le mode 1 à l'initialisation
// (FUN_004f35d0), une demande rangée (FUN_004f2ad0) puis appliquée (FUN_004f1740), CameraSetWidescreen, et les
// conditions CameraActive (FUN_0065aea0) et CameraInWidescreen (FUN_0065afc0).
//   build/tests/test_cameramgr
#include "../src/render/CameraMgr.h"
#include <cstdio>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CCameraMgr c;
	VERIF(c.courant == CAMERA_JOUEUR && c.demande.mode == CAMERA_JOUEUR && CameraActive(c, 1) && !CameraActive(c, 13));
	c.Appliquer();                     // la demande du même mode : rien ne change, elle est effacée
	VERIF(c.courant == 1 && c.precedent == 0 && c.demande.mode == 0);
	c.Demander(13, 0, true);           // CameraSetActive(13) : rangée, pas encore appliquée
	VERIF(CameraActive(c, 1) && !CameraActive(c, 13));
	c.Appliquer();
	VERIF(CameraActive(c, 13) && c.precedent == 1 && c.demande.mode == 0);
	c.DemanderSiAutre(13);             // FUN_004f3440 : déjà le mode courant, aucune demande
	VERIF(c.demande.mode == 0);
	c.DemanderSiAutre(1);
	VERIF(c.demande.mode == 1 && c.demande.octet);
	VERIF(!CameraInWidescreen(c));
	c.LargeEcran(true, false);         // CameraSetWidescreen(true) : progressif
	VERIF(CameraInWidescreen(c) && !c.largeEcranAtteint && c.transition == 0.0f);
	c.LargeEcran(false, true);         // CameraSetWidescreen(false, false) : tout de suite
	VERIF(!CameraInWidescreen(c) && !c.largeEcranAtteint);
	c.LargeEcran(true, true);
	VERIF(c.largeEcranAtteint);
	printf("test_cameramgr : %s\n", echecs ? "ÉCHEC" : "OK");
	return echecs ? 1 : 0;
}
