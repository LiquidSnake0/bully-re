// Les réserves de données de mission (DATLoad) : 1_01.DAT lu dans DAT/Trigger.img, puis les
// recherches par nom des tables POINTLIST, PATH et TRIGGER, le n-ième point d'une liste, et
// DATUnload qui libère sans déplacer les autres handles.
//   BULLY_DATA=<racine du jeu> build/tests/test_datpools
#include "../src/core/DatPools.h"
#include <cstdio>
#include <cmath>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)
static bool Pres(float a, float b){ return fabsf(a - b) < 0.001f; }

int
main(void)
{
	CDatPools p;
	VERIF(!p.Charger("pas_un_fichier.DAT"));
	VERIF(p.Charger("1_01.DAT", 2));
	VERIF(p.fichiersCharges > 0);

	// POINTLIST._1_01_PLAYER_START : le « _ » et la casse ne comptent pas ; handle à partir de 1.
	int depart = p.ListePoints("_1_01_PLAYER_START");
	VERIF(depart >= 1);
	VERIF(p.ListePoints("1_01_Player_Start") == depart);
	VERIF(p.ListePoints("_PAS_UNE_LISTE") == -1);
	const CTriggerPointPoint *pt = p.Point(depart, 1);
	VERIF(pt != nil);
	if(pt){
		VERIF(Pres(pt->pos.x, 298.359009f) && Pres(pt->pos.y, -72.234497f) && Pres(pt->pos.z, 6.068940f));
		VERIF(Pres(pt->lacetTangageRoulis[0], 185.0f));
	}
	VERIF(p.Point(depart, 0) == pt && p.Point(depart, 50) == pt);   // un seul point : le n-ième est le dernier
	VERIF(p.Point(-1, 1) == nil && p.Point(0, 1) == nil && p.Point(9999, 1) == nil);

	// Une liste à plusieurs points : les points d'acclamation de la bagarre.
	int cheer = p.ListePoints("_1_01_CHEERPOINTS");
	VERIF(cheer >= 1 && cheer != depart);
	const CTriggerPointList *l = p.Liste(cheer);
	VERIF(l != nil && l->points.size() > 1);
	if(l && l->points.size() > 1) VERIF(p.Point(cheer, 2) == &l->points[1] && p.Point(cheer, (int)l->points.size()) == &l->points.back());

	// Les trajets et les déclencheurs du même fichier.
	int sec = p.Trajet("_1_01_SECPATH");
	VERIF(sec >= 1);
	const CTriggerPath *tr = p.TrajetParHandle(sec);
	VERIF(tr != nil && !tr->points.empty());
	VERIF(p.Trajet("_PAS_UN_TRAJET") == -1);
	int zone = p.Declencheur("_1_01_SCHOOLTRIG");
	VERIF(zone >= 0);
	VERIF(p.ZoneParHandle(zone) != nil);
	VERIF(p.Declencheur("_PAS_UN_DECLENCHEUR") == -1);

	// Le monde à l'emplacement 1 s'ajoute après ; décharger la mission (2) ne déplace pas ses handles.
	VERIF(p.Charger("Global.DAT", 1));
	int monde = p.ListePoints("_GLOBAL_PLAYER_START");
	VERIF(monde > depart || monde == -1);
	p.Decharger(2);
	VERIF(p.ListePoints("_1_01_PLAYER_START") == -1 && p.Liste(depart) == nil);
	VERIF(p.Trajet("_1_01_SECPATH") == -1 && p.Declencheur("_1_01_SCHOOLTRIG") == -1);
	if(monde >= 1) VERIF(p.ListePoints("_GLOBAL_PLAYER_START") == monde);
	// Recharger reprend les trous libérés : 1_01 retrouve des handles en dessous de ceux du monde.
	VERIF(p.Charger("1_01.DAT", 2));
	int depart3 = p.ListePoints("_1_01_PLAYER_START");
	VERIF(depart3 >= 1 && (monde == -1 || depart3 < monde));
	// Décharger un emplacement vide ne change rien.
	p.Decharger(7);
	VERIF(p.ListePoints("_1_01_PLAYER_START") == depart3);
	p.Vider();
	VERIF(p.ListePoints("_1_01_PLAYER_START") == -1 && p.fichiersCharges == 0);

	printf("test_datpools : %s\n", echecs == 0 ? "OK" : "ECHEC");
	return echecs != 0;
}
