// Lit les 479 arbres d'actions de Act/Act.img avec CActionTreeFile.
//   BULLY_DATA=<racine du jeu> build/tests/test_actiontree
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/core/ActionTree.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

static const CActionNode *
Trouver(const CActionNode &x, uint32 h)
{
	if(x.hachage == h) return &x;
	for(const CActionNode &e : x.enfants){ const CActionNode *r = Trouver(e, h); if(r) return r; }
	return nil;
}

int
main(void)
{
	VERIF(ActionHash("AI_Rat") == 0x6ad08c4a && ActionHash("RatLoco") == 0x0a90c2ac);
	int32 im = CdStream::AddImage("Act\\Act.img");
	VERIF(im >= 0);
	if(im < 0){ printf("test_actiontree : ECHEC\n"); return 1; }
	const CdImage &img = CdStream::ms_images[im];
	int fichiers = 0, lus = 0, justes = 0; long noeuds[4] = {0, 0, 0, 0}; long pistes = 0, typees = 0, animations = 0;
	bool punch = false, sitDown = false;
	CActionTreeFile rat, poi;
	for(int32 e = 0; e < img.m_numEntries; e++){
		const CDirectoryEntry &d = img.m_entries[e];
		uint32 n = d.size * CDSTREAM_SECTOR_SIZE; uint8 *b = (uint8*)malloc(n);
		int32 fd = CFileMgr::OpenFile("Act\\Act.img", "rb", 1);
		CFileMgr::Seek(fd, d.offset * CDSTREAM_SECTOR_SIZE, 0);
		bool ok = CFileMgr::ReadExact(fd, b, n); CFileMgr::CloseFile(fd);
		fichiers++;
		CActionTreeFile f;
		if(ok && f.Load(b, n)){
			lus++; justes += f.ComptesJustes();
			for(int k = 0; k < 4; k++) noeuds[k] += f.lus[k];
			// Toutes les pistes de l'arbre : un type pour chacune (par héritage au besoin).
			std::vector<const CActionNode*> pile(1, &f.racine);
			while(!pile.empty()){
				const CActionNode *x = pile.back(); pile.pop_back();
				for(const CActionNode &e : x->enfants) pile.push_back(&e);
				for(int32 dp : x->pistes){
					CActionTrack t; pistes++;
					if(!f.Piste(dp, t)) continue;
					typees++;
					if(t.type == ActionHash("Animation")){ animations++; if(t.Mot(24) == ActionHash("C_PLAYER\\PUNCH_SLOP_1")) punch = true; }
					// Dans AI_POI, le nœud jouable sous la banque sitting enchaîne la séquence ./SitDown.
					if(strcasecmp(d.name, "AI_POI.cat") == 0 && t.type == ActionHash("Sequence")){
						const CActionAttribut *c = t.Champ(32);
						if(c && f.ChaineCitee(c->decalage) == "./SitDown") sitDown = true;
					}
				}
			}
			if(strcasecmp(d.name, "AI_Rat.cat") == 0) rat = f;
			if(strcasecmp(d.name, "AI_POI.cat") == 0){
				poi = f;
				// Le type d'une condition se lit dans les données : on garde le tampon le temps de vérifier.
				const CActionNode *s = Trouver(f.racine, ActionHash("sitting"));
				VERIF(s && !s->conditions.empty() && f.TypeCondition(s->conditions[0]) == ActionHash("HavePOIOfType"));
			}
			if(strcasecmp(d.name, "AI_Rat.cat") == 0)
				VERIF(!f.racine.enfants.empty() && f.racine.enfants[0].conditions.size() == 1 && f.TypeCondition(f.racine.enfants[0].conditions[0]) == ActionHash("WeaponModelRequest"));
		}else printf("  illisible : %s\n", d.name);
		free(b);
	}
	printf("%d fichiers .cat, %d lus, %d aux comptes justes ; %ld banques, %ld nœuds jouables, %ld références, %ld feuilles\n",
	       fichiers, lus, justes, noeuds[0], noeuds[1], noeuds[2], noeuds[3]);
	VERIF(fichiers == 479 && lus == 479 && justes == 479);
	printf("%ld pistes, %ld typées, dont %ld Animation\n", pistes, typees, animations);
	VERIF(pistes == 51001 && typees == 51001 && animations == 8166);
	VERIF(punch);      // une piste Animation joue C_PLAYER\PUNCH_SLOP_1
	VERIF(sitDown);    // AI_POI : une séquence ./SitDown
	// AI_Rat : la banque AI_Rat (nom haché), deux feuilles dont RatLoco.
	VERIF(rat.racine.genre == 'b' && rat.racine.hachage == ActionHash("AI_Rat") && rat.racine.enfants.size() == 2);
	VERIF(rat.racine.enfants.size() == 2 && rat.racine.enfants[0].genre == 'l' && rat.racine.enfants[0].hachage == ActionHash("RatLoco"));
	// AI_POI : POIPoint, et sous lui les banques Hangout, sitting, spectator.
	VERIF(poi.racine.hachage == ActionHash("POIPoint"));
	VERIF(Trouver(poi.racine, ActionHash("Hangout")) && Trouver(poi.racine, ActionHash("SitDown")) && Trouver(poi.racine, ActionHash("spectator")));
	printf("test_actiontree : %s\n", echecs ? "ECHEC" : "ok");
	return echecs != 0;
}
