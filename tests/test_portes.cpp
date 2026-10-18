// Les portes du gestionnaire de zones (FUN_00416000, ce que DATInit fait des listes SV_ et DT_) :
// AreaTran.DAT apparie sept listes SV_ à leurs déclencheurs, tschool_doors.DAT seize listes DT_ aux
// portes de l'école ; le joueur dans le déclencheur SV_BioSchool (zone 6) est demandé au point
// SV_BioSchool, en zone 2 ; une porte DT_ n'est pas passée sans son objet.
//   BULLY_DATA=<racine du jeu> build/tests/test_portes
#include "../src/core/AreaManager.h"
#include <cstdio>
#include <cstring>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CDatPools d;
	CAreaManager z;
	z.ReconstruirePortes(d);
	VERIF(z.portes.empty());
	VERIF(d.Charger("AreaTran.DAT", 1));
	if(d.NombreListes() == 0){ printf("test_portes : ECHEC (DAT\\Trigger.img ?)\n"); return 1; }
	z.ReconstruirePortes(d);
	int sv = 0, dt = 0;
	for(const CAreaPorte &p : z.portes){ sv += !p.parObjet; dt += p.parObjet; }
	printf("  AreaTran.DAT : %d portes SV_ (en entrant), %d DT_\n", sv, dt);
	VERIF(sv == 7 && dt == 0);
	// SV_BioSchool : le déclencheur dans le labo de biologie (zone 6), la liste arrive dans l'école (zone 2).
	int bio = d.Declencheur("SV_BioSchool"), bioListe = d.ListePoints("SV_BioSchool");
	VERIF(bio >= 0 && bioListe >= 1);
	const CAreaPorte *porte = nil;
	for(const CAreaPorte &p : z.portes) if(p.declencheur == bio) porte = &p;
	VERIF(porte != nil && porte->liste == bioListe && !porte->parObjet);
	const CTriggerZone *dz = d.ZoneParHandle(bio); const CTriggerPointList *dl = d.Liste(bioListe);
	VERIF(dz && dl && dz->zone == 6 && dl->zone == 2 && dl->sauvegarde && dl->points.size() >= 1);
	// MainSchool (zone 0) a sa liste SV_MainSchool mais pas de déclencheur de ce nom : ce n'est pas une porte
	// du gestionnaire (la porte de l'école est l'objet DT_tschool_SchoolFrontDoorL).
	VERIF(d.Declencheur("MainSchool") >= 0 && d.ListePoints("SV_MainSchool") >= 1 && d.Declencheur("SV_MainSchool") == -1);
	// Le joueur dans le déclencheur SV_BioSchool : la demande part vers le point, dans la zone 2, sans +0x8dc.
	if(dz && dl){
		CVector dedans(dz->pos.x, dz->pos.y, dz->pos.z + 0.5f);
		VERIF(d.DansDeclencheur(bio, dedans));
		VERIF(z.PasserUnePorte(d, CVector(0, 0, 0)) == -1 && !z.EnChargement());
		int i = z.PasserUnePorte(d, dedans);
		VERIF(i >= 0 && &z.portes[i] == porte && z.EnChargement() && !z.immediat);
		VERIF(z.demande.zone == 2 && z.demande.pos.x == dl->points[0].pos.x && z.demande.cap == dl->points[0].lacetTangageRoulis[0]);
		VERIF(z.PasserUnePorte(d, dedans) == -1);                 // une demande à la fois
		printf("  SV_BioSchool : déclencheur en zone %d → point (%.1f, %.1f, %.1f) en zone %d\n", dz->zone, dl->points[0].pos.x, dl->points[0].pos.y, dl->points[0].pos.z, dl->zone);
	}
	// Les portes de l'école : seize listes DT_, chacune avec son déclencheur ; la porte d'entrée va de la cour (0) au hall (2).
	VERIF(d.Charger("tschool_doors.DAT", 5));
	CAreaManager z2;
	z2.ReconstruirePortes(d);
	sv = dt = 0;
	for(const CAreaPorte &p : z2.portes){ sv += !p.parObjet; dt += p.parObjet; }
	printf("  + tschool_doors.DAT : %d SV_, %d DT_\n", sv, dt);
	VERIF(sv == 7 && dt == 16);
	int entree = d.Declencheur("DT_tschool_SchoolFrontDoorL"), entreeListe = d.ListePoints("DT_tschool_SchoolFrontDoorL");
	VERIF(entree >= 0 && entreeListe >= 1);
	porte = nil;
	for(const CAreaPorte &p : z2.portes) if(p.declencheur == entree) porte = &p;
	VERIF(porte != nil && porte->liste == entreeListe && porte->parObjet);
	const CTriggerZone *ez = d.ZoneParHandle(entree); const CTriggerPointList *el = d.Liste(entreeListe);
	VERIF(ez && el && ez->zone == 0 && el->zone == 2 && el->porte);
	if(ez){ VERIF(z2.PasserUnePorte(d, CVector(ez->pos.x, ez->pos.y, ez->pos.z + 0.5f)) == -1); }   // DT_ : sans l'objet, rien
	// Décharger les portes de l'école (emplacement 5) et refaire la table : les sept SV_ restent.
	d.Decharger(5);
	z2.ReconstruirePortes(d);
	VERIF(z2.portes.size() == 7);
	printf("test_portes : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
