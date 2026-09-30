// Les animations .agr (docs/agr.md) : Player_Tired.agr lu dans World.img se
// découpe en trois animations, la première de type 1002 se décode en 36
// pistes de quaternions unitaires, et les os immobiles de Jimmy (PLAYER.nif)
// démarrent sur la rotation de leur nœud : c'est ce qui fixe l'ordre des pistes.
//   BULLY_DATA=<racine du jeu> build/tests/test_agr
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/gamebryo/NifSkin.h"
#include "../src/anim/Agr.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

static uint8 *
Lire(const char *nom, uint32 *bytes)
{
	const CDirectoryEntry *d = CdStream::ms_images[0].Find(nom);
	if(d == nil) return nil;
	*bytes = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(*bytes);
	int32 fd = CFileMgr::OpenFile("Stream\\World.img", "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, *bytes);
	CFileMgr::CloseFile(fd);
	if(!ok){ free(buf); return nil; }
	return buf;
}

int
main(void)
{
	VERIF(CdStream::AddImage("Stream\\World.img") == 0);
	uint32 n; uint8 *b = Lire("Player_Tired.agr", &n);
	VERIF(b != nil);
	if(!b){ printf("test_agr : ECHEC\n"); return 1; }
	std::vector<AgrAnim> v;
	VERIF(AgrLireGroupe(b, n, v));
	VERIF(v.size() == 3);
	const AgrAnim &a = v[0];
	VERIF(a.type == 1002 && a.numCles == 961 && fabsf(a.duree - 6.0f) < 1e-5f && a.decodee);
	VERIF(v[1].numCles == 1191 && v[2].numCles == 533 && fabsf(v[2].duree - 4.0f) < 1e-5f);

	// 36 pistes non vides, dans l'ordre du temps, de quaternions unitaires.
	int cles = 0; float pire = 0;
	for(int k = 0; k < AGR_OS; k++){
		VERIF(!a.pistes[k].empty());
		for(size_t j = 0; j < a.pistes[k].size(); j++){
			const float *q = a.pistes[k][j].q;
			pire = fmaxf(pire, fabsf(sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]) - 1));
			if(j) VERIF(a.pistes[k][j].t >= a.pistes[k][j-1].t);
			cles++;
		}
	}
	VERIF(cles == 961);
	VERIF(pire < 0.01f);
	printf("  Player_Tired : 3 animations, la première en 36 pistes, 961 clés, norme à %.4f près\n", pire);

	// Les positions : la hauteur du bassin, autour de 0,845 m.
	VERIF(a.positions.size() == 60);
	VERIF(fabsf(a.positions[0].p.z - 0.845f) < 0.01f);

	// Interpolation continue : 1 ms d'écart ne tourne presque pas.
	float q0[4], q1[4];
	VERIF(AgrRotation(a, 20, 2.5f, q0) && AgrRotation(a, 20, 2.501f, q1));
	float c = fabsf(q0[0]*q1[0] + q0[1]*q1[1] + q0[2]*q1[2] + q0[3]*q1[3]);
	VERIF(c > 0.9999f);

	// L'ordre des pistes : les nœuds sous « Dummy », dans l'ordre du fichier.
	// Les os qui ne bougent pas ici démarrent sur la rotation de leur nœud.
	uint32 nn; uint8 *nb = Lire("PLAYER.nif", &nn);
	CNifFile f; VERIF(nb && f.Load(nb, nn));
	std::vector<int32> noeuds, pile(1, NifFindNode(f, "Dummy"));
	while(!pile.empty()){
		int32 x = pile.back(); pile.pop_back();
		if(x < 0 || x >= f.numBlocks || f.blocks[x].kind != NIF_NODE) continue;
		noeuds.push_back(x);
		const NifNode *nd = (const NifNode*)f.blocks[x].data;
		for(int32 i = nd->numChildren - 1; i >= 0; i--) pile.push_back(nd->children[i]);
	}
	VERIF(noeuds.size() == AGR_OS);
	const int immobiles[] = { 2, 11, 15, 16, 17, 18, 19, 23, 24, 25, 27, 31 };  // bassin, colonne, visage, clavicules, doigts
	int justes = 0;
	for(int k : immobiles){
		float m[3][3]; AgrMatrice(a.pistes[k][0].q, m);
		const NifMatrix33 &r = ((const NifAVObject*)f.blocks[noeuds[k]].data)->rotation;
		float e = 0; for(int i = 0; i < 3; i++) for(int j = 0; j < 3; j++) e = fmaxf(e, fabsf(m[i][j] - r.m[i][j]));
		if(e < 0.1f) justes++;
	}
	VERIF(justes == 12);
	printf("  ordre des pistes : %d os immobiles sur 12 sur la rotation de leur nœud\n", justes);
	f.Free(); free(nb); free(b);
	printf("test_agr : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
