// Les groupes d'animations (src/anim/AnimGroups.h) : les .agr de Stream/World.img, chargés ou demandés.
//   BULLY_DATA=<racine du jeu> build/tests/test_animgroups
#include "../src/core/CdStream.h"
#include "../src/anim/AnimGroups.h"
#include <cstdio>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	VERIF(CdStream::AddImage("Stream\\World.img") == 0);
	CAnimGroups g;
	int n = g.Lire(CdStream::ms_images[0]);
	VERIF(n == 550);
	printf("  %d groupes d'animations dans Stream/World.img\n", n);
	// Les 22 groupes d'humiliation (0x9113d0) existent tous ; la casse ne compte pas (FUN_0042e200 : stricmp).
	VERIF(g.Existe("HUMIL_5-8_A") && g.Existe("humil_6-1v6-1") && g.Existe("HUMIL_4-10_C") && !g.Existe("HUMIL_4-10_A"));
	// LoadAnimationGroup charge tout de suite ; une demande seule attend l'image suivante.
	VERIF(g.Charger("NIS_1_02") && g.EstCharge("nis_1_02") && !g.Charger("PasUnGroupe"));
	VERIF(g.Demander("HUMIL_5-8_A", 10) && !g.EstCharge("HUMIL_5-8_A"));
	VERIF(g.Avancer(10) == 0 && !g.EstCharge("HUMIL_5-8_A"));
	VERIF(g.Avancer(11) == 1 && g.EstCharge("HUMIL_5-8_A"));
	g.Decharger("HUMIL_5-8_A");
	VERIF(!g.EstCharge("HUMIL_5-8_A") && g.Existe("HUMIL_5-8_A"));
	printf("test_animgroups : %s\n", echecs == 0 ? "OK" : "ECHEC");
	return echecs != 0;
}
