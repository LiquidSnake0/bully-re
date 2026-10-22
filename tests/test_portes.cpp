// Les portes du gestionnaire de zones (FUN_00416000, ce que DATInit fait des listes SV_ et DT_) :
// AreaTran.DAT apparie sept listes SV_ à leurs déclencheurs, tschool_doors.DAT seize listes DT_ aux
// portes de l'école ; le joueur dans le déclencheur SV_BioSchool (zone 6) est demandé au point
// SV_BioSchool, en zone 2 ; une porte DT_ n'est pas passée sans son objet.
//   BULLY_DATA=<racine du jeu> build/tests/test_portes
#include <cmath>
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
	// Avec l'objet de porte en cours (+0x394, FUN_00415160) : la porte passe, le joueur où qu'il soit, vers le
	// premier point de sa liste en zone 2 ; l'état 0xd remet +0x394 à zéro.
	if(el && !el->points.empty()){
		z2.PousserPorte(entree);
		int i = z2.PasserUnePorte(d, CVector(0, 0, 0));
		VERIF(i >= 0 && &z2.portes[i] == porte && z2.demande.zone == 2 && z2.demande.pos.x == el->points[0].pos.x && !z2.retourEnReserve);
		for(int k = 0; k < 200 && z2.EnChargement(); k++){ z2.AvancerImage(FONDU_NOIR, true, true); if(z2.etat == AREA_PRET) z2.SignalerPret(); }
		VERIF(!z2.EnChargement() && z2.porteObjet == -1 && z2.zoneVisible == 2);
	}
	// Décharger les portes de l'école (emplacement 5) et refaire la table : les sept SV_ restent.
	d.Decharger(5);
	z2.ReconstruirePortes(d);
	VERIF(z2.portes.size() == 7);
	// Les autres fichiers de portes : la sortie de l'école (déclencheur dans le hall, zone 2, vers la cour, 0),
	// et les portes qui gardent le retour (FUN_0042e570).
	static const char *fichiers[] = { "ischool_doors.dat", "BDorm_Doors.dat", "tbusines_doors.dat", "tindustrial_doors.dat",
	                                  "trich_doors.dat", "tschool_garagedoors.dat", "tschool_maindoors.dat" };
	int emplacement = 6, sorties = 0, avecRetour = 0, total = 0;
	for(const char *f : fichiers) VERIF(d.Charger(f, emplacement++));
	CAreaManager z3;
	z3.ReconstruirePortes(d);
	const CAreaPorte *sortie = nil;
	for(const CAreaPorte &p : z3.portes){
		if(!p.parObjet) continue;
		total++;
		const CTriggerZone *tz = d.ZoneParHandle(p.declencheur); const CTriggerPointList *tl = d.Liste(p.liste);
		if(!tz || !tl) continue;
		if(tz->zone == 2 && tl->zone == 0){ sorties++; if(!sortie) sortie = &p; printf("    sortie de l'école : %s\n", tz->nom.c_str()); }
		avecRetour += CAreaManager::ZoneAvecRetour(tl->zone);
	}
	printf("  tous les fichiers de portes : %d portes DT_, %d du hall de l'école vers la cour, %d vers une zone qui garde le retour\n", total, sorties, avecRetour);
	VERIF(sortie != nil && avecRetour > 0);
	// Une porte qui garde le retour : y entrer met la zone visible et le joueur en réserve ; la porte suivante
	// y ramène, quelle que soit sa liste.
	for(const CAreaPorte &p : z3.portes){
		const CTriggerPointList *tl = d.Liste(p.liste);
		if(!p.parObjet || !tl || !CAreaManager::ZoneAvecRetour(tl->zone)) continue;
		z3.zoneVisible = 0;
		z3.PousserPorte(p.declencheur);
		VERIF(z3.PasserUnePorte(d, CVector(1, 2, 3), 90) >= 0 && z3.retourEnReserve && z3.retour.zone == 0 && z3.retour.cap == 270 && z3.demande.zone == tl->zone);
		// FUN_00416750 : 2,5 m en arrière le long du regard (cap 90° : regard vers -x), dos à la porte.
		VERIF(fabsf(z3.retour.pos.x - 3.5f) < 1e-4f && fabsf(z3.retour.pos.y - 2) < 1e-4f && z3.retour.pos.z == 3);
		for(int k = 0; k < 200 && z3.EnChargement(); k++){ z3.AvancerImage(FONDU_NOIR, true, true); if(z3.etat == AREA_PRET) z3.SignalerPret(); }
		z3.PousserPorte(p.declencheur);
		VERIF(z3.PasserUnePorte(d, CVector(5, 5, 5)) >= 0 && !z3.retourEnReserve && z3.demande.zone == 0 && fabsf(z3.demande.pos.x - 3.5f) < 1e-4f && z3.demande.cap == 270);
		const CTriggerZone *tz = d.ZoneParHandle(p.declencheur);
		printf("  %s : déclencheur en zone %d → zone %d, retour gardé\n", tz ? tz->nom.c_str() : "?", tz ? tz->zone : -1, tl->zone);
		break;
	}
	printf("test_portes : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
