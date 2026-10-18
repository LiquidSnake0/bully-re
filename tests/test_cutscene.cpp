// Les fichiers de cinématique de Cuts/Cuts.img (src/cutscene/CutsceneFiles) : les 863 entrées
// parcourues, les 122 .INF, 107 .SUB et 126 .DAT lus comme CCutsceneMgr::LoadCutsceneData et
// FUN_004f86d0, et 1-1-1 (la première cinématique du jeu) vérifiée champ par champ.
//   BULLY_DATA=<racine du jeu> build/tests/test_cutscene
#include "../src/cutscene/CutsceneFiles.h"
#include "../src/core/CdStream.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <strings.h>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)
static bool Pres(float a, float b){ return fabsf(a - b) < 1e-3f; }

int
main(void)
{
	VERIF(CCutsceneFiles::Ouvrir());
	if(CCutsceneFiles::Image() < 0){ printf("test_cutscene : ECHEC (Cuts\\Cuts.img introuvable, BULLY_DATA ?)\n"); return 1; }
	const CdImage &img = CdStream::ms_images[CCutsceneFiles::Image()];
	int entrees = 0, infs = 0, subs = 0, dats = 0, illisibles = 0, objets = 0, faciaux = 0, sousTitres = 0, fps30 = 0, cinquieme = 0, agr = 0, hxd = 0, mxd = 0;
	std::vector<uint8> b;
	for(int32 e = 0; e < img.m_numEntries; e++){
		const char *n = img.m_entries[e].name;
		entrees++;
		const char *ext = strrchr(n, '.');
		if(ext == nil) continue;
		if(strcasecmp(ext, ".agr") == 0) agr++;
		if(strcasecmp(ext, ".hxd") == 0) hxd++;
		if(strcasecmp(ext, ".mxd") == 0) mxd++;
		if(strcasecmp(ext, ".inf") == 0){
			CCutsceneInf f;
			if(CCutsceneFiles::Lire(n, b) && f.Load(b.data(), (uint32)b.size())){ infs++; objets += (int)f.objets.size(); faciaux += (int)f.faciaux.size(); }
			else { illisibles++; printf("  illisible : %s\n", n); }
		}else if(strcasecmp(ext, ".sub") == 0){
			CCutsceneSub f;
			if(CCutsceneFiles::Lire(n, b) && f.Load(b.data(), (uint32)b.size())){ subs++; sousTitres += (int)f.lignes.size(); fps30 += Pres(f.imagesParSeconde, 30); }
			else { illisibles++; printf("  illisible : %s\n", n); }
		}else if(strcasecmp(ext, ".dat") == 0){
			CCutsceneCamera f;
			if(CCutsceneFiles::Lire(n, b) && f.Load(b.data(), (uint32)b.size())){ dats++; cinquieme += !f.points.empty(); }
			else { illisibles++; printf("  illisible : %s\n", n); }
		}
	}
	printf("  %d entrées : %d .INF (%d objets, %d jeux faciaux), %d .SUB (%d sous-titres, %d à 30 images/s), %d .DAT (%d avec une cinquième section), %d .agr, %d .hxd, %d .mxd, %d illisible(s)\n",
	       entrees, infs, objets, faciaux, subs, sousTitres, fps30, dats, cinquieme, agr, hxd, mxd, illisibles);
	VERIF(entrees == 863 && illisibles == 0);
	VERIF(infs == 122 && objets == 438 && faciaux == 300);
	VERIF(subs == 107 && sousTitres == 2275 && fps30 == 107);
	VERIF(dats == 126 && cinquieme == 0);
	VERIF(agr == 170 && hxd == 170 && mxd == 168);

	// 1-1-1 : trois objets, deux avec leur jeu facial (pas la voiture), 13 / 20 / 30 / 29 clés, 61,67 s.
	CCutsceneFiles c;
	VERIF(c.Charger("1-1-1"));
	VERIF(c.aObjets && c.aSousTitres && c.aCamera);
	VERIF(c.inf.objets.size() == 3 && c.inf.faciaux.size() == 2);
	if(c.inf.objets.size() == 3){
		VERIF(c.inf.objets[0] == "CS_CAR" && c.inf.objets[1] == "CS_PLAY" && c.inf.objets[2] == "CS_TE_SECRETARY");
		VERIF(!c.inf.AFacial(0) && c.inf.AFacial(1) && c.inf.AFacial(2) && !c.inf.AFacial(3));
	}
	VERIF(Pres(c.sub.imagesParSeconde, 30) && c.sub.lignes.size() == 20);
	if(!c.sub.lignes.empty()) VERIF(c.sub.lignes[0].debut == 270 && c.sub.lignes[0].fin == 337 && c.sub.lignes[0].cle == "1-1-1.SUB0001" && c.sub.lignes.back().fin == 1695 && c.sub.lignes.back().cle == "1-1-1.SUB0020");
	VERIF(c.camera.champ.size() == 13 && c.camera.seconde.size() == 20 && c.camera.position.size() == 30 && c.camera.cible.size() == 29 && c.camera.points.empty());
	if(c.camera.champ.size() == 13 && c.camera.position.size() == 30){
		VERIF(Pres(c.camera.champ[0].valeur, 80) && Pres(c.camera.champ[5].t, 9.7f) && Pres(c.camera.champ[5].valeur, 48.597f));
		VERIF(Pres(c.camera.position[0].pos.x, 319.703f) && Pres(c.camera.position[0].pos.y, -64.1672f) && Pres(c.camera.position[0].pos.z, 11.4037f));
		VERIF(Pres(c.camera.position[6].t, 21.2333f) && Pres(c.camera.position[6].a, 0.3f) && Pres(c.camera.position[6].entree.y, 2.06662f));
		VERIF(Pres(c.camera.cible[0].pos.x, 312.904f) && Pres(c.camera.Duree(), 61.6667f));
	}
	// Ses animations : les groupes dont le nom commence par « 1-1-1_ ».
	VERIF(c.animations.size() == 4);
	int car = 0, chr = 0;
	for(const std::string &a : c.animations){ car += strcasecmp(a.c_str(), "1-1-1_CAR.agr") == 0; chr += strcasecmp(a.c_str(), "1-1-1_CHAR.agr") == 0; }
	VERIF(car == 1 && chr == 1);
	printf("  1-1-1 : %zu objets (%zu faciaux), %zu sous-titres à %.0f images/s, caméra %zu/%zu/%zu/%zu clés, %.2f s, %zu fichiers d'animation\n",
	       c.inf.objets.size(), c.inf.faciaux.size(), c.sub.lignes.size(), c.sub.imagesParSeconde, c.camera.champ.size(), c.camera.seconde.size(), c.camera.position.size(), c.camera.cible.size(), c.camera.Duree(), c.animations.size());

	// 1-1-02 n'a ni .INF ni .SUB, seulement sa caméra ; une cinématique inconnue n'a rien.
	CCutsceneFiles d;
	VERIF(d.Charger("1-1-02") && !d.aObjets && !d.aSousTitres && d.aCamera);
	VERIF(d.Charger("PAS-UNE-CINEMATIQUE") && !d.aObjets && !d.aSousTitres && !d.aCamera && d.animations.empty());
	printf("test_cutscene : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
