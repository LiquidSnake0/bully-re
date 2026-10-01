// Les trajets de DAT/Trigger.img (docs/trigger.md) : les 410 fichiers se lisent,
// chaque fichier rend le nombre de trajets de son en-tête, et Patrol_School
// donne la ronde « hallspatrol_1A » telle qu'écrite dans le fichier.
//   BULLY_DATA=<racine du jeu> build/tests/test_trigger
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/core/TriggerFile.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <strings.h>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	int32 im = CdStream::AddImage("DAT\\Trigger.img");
	VERIF(im >= 0);
	if(im < 0){ printf("test_trigger : ECHEC\n"); return 1; }
	const CdImage &img = CdStream::ms_images[im];
	int fichiers = 0, lus = 0, trajets = 0, points = 0, actions = 0, incoherences = 0, perim = 0, zones = 0, comptesFaux = 0;
	CTriggerFile pop;
	const CTriggerPath *ronde = nil; CTriggerFile garde;
	for(int32 e = 0; e < img.m_numEntries; e++){
		const CDirectoryEntry &d = img.m_entries[e];
		uint32 n = d.size * CDSTREAM_SECTOR_SIZE; uint8 *b = (uint8*)malloc(n);
		int32 fd = CFileMgr::OpenFile("DAT\\Trigger.img", "rb", 1);
		CFileMgr::Seek(fd, d.offset * CDSTREAM_SECTOR_SIZE, 0);
		bool ok = CFileMgr::ReadExact(fd, b, n); CFileMgr::CloseFile(fd);
		fichiers++;
		CTriggerFile f;
		if(ok && f.Load(b, n)){
			lus++; trajets += (int)f.paths.size(); incoherences += f.incoherences;
			perim += (int)f.perimetres.size(); zones += (int)f.zones.size();
			if((int)f.perimetres.size() != f.nPerimeters || (int)f.zones.size() != f.nTriggers){ comptesFaux++; printf("  comptes : %s (%zu/%d périmètres, %zu/%d déclencheurs)\n", d.name, f.perimetres.size(), f.nPerimeters, f.zones.size(), f.nTriggers); }
			if(strcasecmp(d.name, "Population.dat") == 0) pop = f;
			for(const CTriggerPath &t : f.paths){ points += (int)t.points.size(); for(const CTriggerPathPoint &p : t.points) actions += (int)p.actions.size(); }
			if(strcasecmp(d.name, "Patrol_School.dat") == 0) garde = f;
		}else printf("  illisible : %s\n", d.name);
		free(b);
	}
	printf("  %d fichiers sur %d, %d trajets, %d points, %d actions, %d trajet(s) au compte de points faux\n", lus, fichiers, trajets, points, actions, incoherences);
	VERIF(fichiers == 410 && lus == 410);
	VERIF(trajets == 1543 && points == 8913 && actions == 62);
	VERIF(incoherences == 1);                      // 1_S01_Running_Prefect_Path : 9 annoncés, 8 écrits
	for(const CTriggerPath &t : garde.paths) if(t.nom == "hallspatrol_1A") ronde = &t;
	VERIF(ronde != nil);
	if(ronde){
		VERIF(ronde->points.size() == 9 && ronde->zone == 2);
		VERIF(fabsf(ronde->points[0].pos.x + 621.971985f) < 1e-3f && fabsf(ronde->points[0].pos.y + 318.695007f) < 1e-3f);
		VERIF(ronde->points[0].actions.size() == 1 && fabsf(ronde->points[0].actions[0].orientation[0] - 165) < 1e-3f && fabsf(ronde->points[0].actions[0].attente - 2) < 1e-3f);
		VERIF(ronde->points[1].unid == 8 && ronde->points[1].actions.empty());
		printf("  hallspatrol_1A : %zu points, premier en (%.2f, %.2f), action : s'orienter à %.0f° et attendre %.0f s\n", ronde->points.size(),
		       ronde->points[0].pos.x, ronde->points[0].pos.y, ronde->points[0].actions[0].orientation[0], ronde->points[0].actions[0].attente);
	}
	printf("  %d périmètres, %d déclencheurs, %d fichier(s) aux comptes d'en-tête différents\n", perim, zones, comptesFaux);
	VERIF(comptesFaux == 1);                      // tschool_trees.dat : en-tête « NTRIGGERS 1 », fichier vide ensuite

	// Population.dat : 37 périmètres, 34 zones peuplées ; RichArea comme écrite.
	VERIF(pop.perimetres.size() == 37 && pop.zones.size() == 34);
	const CTriggerZone *riche = nil; int peuplees = 0, contiennent = 0;
	for(const CTriggerZone &z : pop.zones){
		if(z.nom == "RichArea") riche = &z;
		if(!z.aPopulation) continue;
		peuplees++;
		// Les points du périmètre sont relatifs au déclencheur : sa propre
		// position (un peu au-dessus de sa base) est dans sa zone.
		if(z.Contient(pop.perimetres, z.pos.x, z.pos.y, z.pos.z + 0.5f)) contiennent++;
		else{
			const CTriggerPerimeter &pe = pop.perimetres[z.perimetre];
			float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
			for(size_t i = 0; i < pe.x.size(); i++){ mnx = fminf(mnx, pe.x[i]); mxx = fmaxf(mxx, pe.x[i]); mny = fminf(mny, pe.y[i]); mxy = fmaxf(mxy, pe.y[i]); }
			printf("  hors de sa zone : %s en (%.1f, %.1f, %.1f), lacet %.0f, hauteur %.0f, polygone x [%.1f, %.1f] y [%.1f, %.1f]\n",
			       z.nom.c_str(), z.pos.x, z.pos.y, z.pos.z, z.lacet, z.zHauteur, mnx, mxx, mny, mxy);
		}
	}
	printf("  Population.dat : %d zones peuplées, %d contiennent leur propre position\n", peuplees, contiennent);
	// 26 sur 27 : la position de DT_ComicShop est au bord de son polygone en L
	// (x de -15,2 à 1,2), hors de la forme ; la convention relative tient (sans
	// elle, RichArea serait à 400 m de sa propre zone).
	VERIF(peuplees == 27 && contiennent == 26);
	VERIF(riche != nil);
	if(riche){
		VERIF(riche->aPopulation && riche->total[MOMENT_JOUR] == 6 && riche->population[MOMENT_JOUR][POP_PREPPY] == 1 && riche->population[MOMENT_JOUR][POP_TOWNPERSON] == 5);
		VERIF(riche->total[MOMENT_COUVREFEU] == 3 && riche->population[MOMENT_NUIT][POP_COP] == 1);
		VERIF(riche->aVehicules && riche->vehicules[MOMENT_JOUR][1] == 2 && riche->vehicules[MOMENT_COUVREFEU][3] == 1);
		VERIF(fabsf(riche->zHauteur - 60) < 1e-3f && riche->perimetre >= 0 && pop.perimetres[riche->perimetre].x.size() == 7);
		VERIF(riche->Contient(pop.perimetres, 448.9f, 350.9f, 5) && !riche->Contient(pop.perimetres, 448.9f + 400, 350.9f, 5));
		printf("  RichArea : %d le jour (1 preppy, 5 citadins), %d au couvre-feu, polygone de %zu points relatif à (%.0f, %.0f)\n",
		       riche->total[MOMENT_JOUR], riche->total[MOMENT_COUVREFEU], pop.perimetres[riche->perimetre].x.size(), riche->pos.x, riche->pos.y);
	}
	printf("test_trigger : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
