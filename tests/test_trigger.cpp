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
	int fichiers = 0, lus = 0, trajets = 0, points = 0, actions = 0, incoherences = 0;
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
	printf("test_trigger : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
