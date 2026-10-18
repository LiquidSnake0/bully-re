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
	int fichiers = 0, lus = 0, trajets = 0, points = 0, actions = 0, incoherences = 0, perim = 0, zones = 0, comptesFaux = 0, pois = 0, poiPoints = 0, poisFaux = 0;
	int listes = 0, listesPoints = 0, listesFausses = 0; CTriggerFile premiere;
	int sauvegardes = 0, portes = 0, ramassages = 0, elastiques = 0, missions = 0, permanents = 0, tags = 0;
	CTriggerFile carnaval;
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
			pois += (int)f.pois.size(); for(const CTriggerPoi &p : f.pois) poiPoints += (int)p.points.size();
			if((int)f.pois.size() != f.nPois) poisFaux++;
			listes += (int)f.pointLists.size();
			for(const CTriggerPointList &pl : f.pointLists){
				listesPoints += (int)pl.points.size(); sauvegardes += pl.sauvegarde; portes += pl.porte;
				for(const CTriggerPointPoint &pt : pl.points){
					ramassages += !pt.ramassage.empty(); elastiques += pt.ramassage == "Rubband"; missions += !pt.mission.empty();
					permanents += pt.permanent; tags += pt.tag != 0;
				}
			}
			if((int)f.pointLists.size() != f.nPoints) listesFausses++;
			if(strcasecmp(d.name, "1_01.dat") == 0) premiere = f;
			if(strcasecmp(d.name, "PedPoi.dat") == 0) carnaval = f;
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
	// Les listes de points (blocs POINT) : le compte de chaque en-tête NPOINTS est juste.
	printf("  %d listes de points, %d points, %d fichier(s) au compte NPOINTS faux\n", listes, listesPoints, listesFausses);
	VERIF(listesFausses == 0 && listes > 0);
	VERIF(premiere.pointLists.size() == 28 && premiere.nPoints == 28);
	const CTriggerPointList *depart = nil;
	for(const CTriggerPointList &pl : premiere.pointLists) if(pl.nom == "1_01_Player_Start") depart = &pl;
	VERIF(depart != nil);
	if(depart){
		VERIF(depart->points.size() == 1 && fabsf(depart->points[0].pos.x - 298.359009f) < 1e-3f && fabsf(depart->points[0].pos.z - 6.068940f) < 1e-3f);
		VERIF(fabsf(depart->points[0].lacetTangageRoulis[0] - 185) < 1e-3f);
	}
	// Les champs d'un point, lus comme FUN_006d59a0 : 1_01_EndPoint est en zone 5 (l'octet +0xc de l'entrée),
	// tourné de -90° avec un roulis de -2°, et tout le reste vide ou à zéro.
	const CTriggerPointList *fin = nil;
	for(const CTriggerPointList &pl : premiere.pointLists) if(pl.nom == "1_01_EndPoint") fin = &pl;
	VERIF(fin != nil);
	if(fin){
		VERIF(fin->zone == 5 && !fin->sauvegarde && !fin->porte && fin->points.size() == 1);
		const CTriggerPointPoint &p = fin->points[0];
		VERIF(fabsf(p.lacetTangageRoulis[0] + 90) < 1e-3f && fabsf(p.lacetTangageRoulis[2] + 2) < 1e-3f);
		VERIF(p.etiquette.empty() && p.ramassage.empty() && p.sfx.empty() && p.banqueSon.empty() && p.meteo.empty() && p.mission.empty() && p.butes.empty());
		VERIF(fabsf(p.volumeSfx - 1) < 1e-6f && p.fonduEntree == 0 && p.fonduSortie == 0 && p.sml == 0 && p.heureDebut == 0 && p.heureFin == 0);
		VERIF(!p.occlusion && !p.missionSpec && !p.permanent && p.tag == 0 && p.meteoMasque == 0x3f);
	}
	// Le masque météo de FUN_00593f20 : « PSF » (87 points) = P, S et F ; « R » la pluie ; « X » ; vide = tout.
	VERIF(MasqueMeteo("PSF") == 0x0d && MasqueMeteo("R") == 0x10 && MasqueMeteo("X") == 0x20 && MasqueMeteo("PSFRW") == 0x1f && MasqueMeteo("") == 0x3f && MasqueMeteo("all") == 0x3f && MasqueMeteo("psf") == 0x0d);
	// Sur les 410 fichiers : 49 listes SV_ (sauvegardes) et 114 DT_ (portes, le moteur compare les trois lettres telles quelles : « dt_smokers » et « dt_SmashCar » n'en sont pas), 408 points à ramasser dont 75 élastiques,
	// 82 points rattachés à une mission, un seul permanent et un seul tag.
	printf("  %d listes SV_, %d listes DT_, %d points à ramasser (%d Rubband), %d liés à une mission, %d permanent(s), %d tag(s)\n",
	       sauvegardes, portes, ramassages, elastiques, missions, permanents, tags);
	VERIF(sauvegardes == 49 && portes == 114 && ramassages == 408 && elastiques == 75 && missions == 82 && permanents == 1 && tags == 1);
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
	// Les points d'intérêt : 338 blocs, 1 199 points, comptes d'en-tête justes.
	printf("  %d points d'intérêt, %d points, %d fichier(s) au compte NPOIS faux\n", pois, poiPoints, poisFaux);
	VERIF(pois == 338 && poiPoints == 1199 && poisFaux == 0);
	VERIF(!carnaval.pois.empty());
	if(!carnaval.pois.empty()){
		const CTriggerPoi &c = carnaval.pois[0];
		VERIF(c.nom == "F_CarnivalWalker" && c.points.size() == 7);
		const CTriggerPoiPoint &p0 = c.points[0], &p1 = c.points[1];
		VERIF(p0.type == "Specific_Event" && p0.clique == "DEFAULT" && p0.genre == "Both" && p0.nom == "F_CarnivalWalker");
		VERIF(fabsf(p0.pos.x - 148.578995f) < 1e-3f && fabsf(p0.pos.y - 438.484985f) < 1e-3f);
		VERIF(p0.periodes[POI_AFTERNOON] && !p0.periodes[POI_MORETIRED] && p0.periodes[POI_SLIGHTLYTIRED] && !p0.periodes[POI_TOOTIRED]);
		VERIF(p1.periodes[POI_MORETIRED] && p0.ignorePopulation && p0.limite == 15);
		printf("  %s : %zu points, le premier en (%.1f, %.1f), actif l'après-midi, pas « très fatigué »\n", c.nom.c_str(), c.points.size(), p0.pos.x, p0.pos.y);
	}
	printf("test_trigger : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
