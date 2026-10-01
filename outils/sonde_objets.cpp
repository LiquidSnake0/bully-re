// Sonde jetable (non construite par défaut) : pour chaque modèle animé
// (section panm), compare les têtes de piste de son .agr aux nœuds de son NIF.
//   BULLY_DATA=<jeu> build/outils/sonde_objets [nom]
#include "commun.h"
#include "../src/anim/Agr.h"
#include <cmath>
static bool outil_AgrBouge(const AgrAnim &x){
	for(int32 k = 0; k < x.numOs; k++){ const std::vector<AgrCle> &p = x.pistes[k];
		for(size_t j = 1; j < p.size(); j++){ float c = fabsf(p[0].q[0]*p[j].q[0] + p[0].q[1]*p[j].q[1] + p[0].q[2]*p[j].q[2] + p[0].q[3]*p[j].q[3]); if(c < 0.999f) return true; } }
	return false;
}


int
main(int argc, char **argv)
{
	outil::Archives a;
	if(!outil::Ouvrir(a)){ printf("archives introuvables\n"); return 1; }
	(void)argv; (void)argc;
	const std::vector<CPanmIdeEntry> &g_panm = a.panm;
	printf("%zu modèles panm\n", g_panm.size());
	int vus = 0;
	for(const CPanmIdeEntry &e : g_panm){
		if(argc > 1 && strcasecmp(argv[1], e.model) != 0) continue;
		uint32 nb, na; uint8 *bn = outil::LireMonde(a, std::string(e.model) + ".nif", &nb), *ba = outil::LireMonde(a, std::string(e.agr) + ".agr", &na);
		if(!bn || !ba){ free(bn); free(ba); continue; }
		CNifFile f{}; std::vector<AgrAnim> g;
		if(!f.Load(bn, nb) || !AgrLireGroupe(ba, na, g) || g.empty()){ f.Free(); free(bn); free(ba); continue; }
		// Les nœuds en profondeur depuis la racine, dans l'ordre du fichier.
		std::vector<int32> noeuds, pile(1, 0);
		while(!pile.empty()){
			int32 x = pile.back(); pile.pop_back();
			if(x < 0 || x >= f.numBlocks || f.blocks[x].kind != NIF_NODE || !f.blocks[x].data) continue;
			noeuds.push_back(x);
			const NifNode *nd = (const NifNode*)f.blocks[x].data;
			for(int32 i = nd->numChildren - 1; i >= 0; i--) pile.push_back(nd->children[i]);
		}
		const AgrAnim &an = g[0];
		printf("\n%s (%s, type %d, %d os, %zu nœuds) :", e.model, e.agr, an.type, an.numOs, noeuds.size());
		for(size_t i = 0; i < noeuds.size() && i < 40; i++) printf(" [%zu]%s", i, f.String(((const NifAVObject*)f.blocks[noeuds[i]].data)->name));
		printf("\n");
		for(int32 k = 0; k < an.numOs && k < 40; k++){
			if(an.pistes[k].empty()) continue;
			float m[3][3]; AgrMatrice(an.pistes[k][0].q, m);
			CVector p(0, 0, 0); bool aP = !an.positions[k].empty(); if(aP) p = an.positions[k][0].p;
			// Le nœud le plus proche en rotation (et en translation si connue).
			int best = -1; float be = 1e9f;
			for(size_t i = 0; i < noeuds.size(); i++){
				const NifAVObject *o = (const NifAVObject*)f.blocks[noeuds[i]].data;
				float er = 0; for(int r = 0; r < 3; r++) for(int c = 0; c < 3; c++) er = fmaxf(er, fabsf(m[r][c] - o->rotation.m[r][c]));
				float et = aP ? fmaxf(fmaxf(fabsf(p.x - o->translation.x), fabsf(p.y - o->translation.y)), fabsf(p.z - o->translation.z)) : 0;
				if(er + et < be){ be = er + et; best = (int)i; }
			}
			printf("  piste %2d → nœud %2d %s (écart %.4f)%s\n", k, best, best >= 0 ? f.String(((const NifAVObject*)f.blocks[noeuds[best]].data)->name) : "", be, best == k + 1 ? "  = k+1" : best == k ? "  = k" : "");
		}
		// Détail des animations du groupe : type, os, durée, pistes de position.
		for(size_t gi = 0; gi < g.size(); gi++){
			const AgrAnim &x = g[gi];
			printf("  anim %zu : type %d, %d os, %.2f s, bouge %d ;", gi, x.type, x.numOs, x.duree, (int)outil_AgrBouge(x));
			for(int32 k = 0; k < x.numOs; k++) if(!x.positions[k].empty()) printf(" pos[%d] %zu (%.2f %.2f %.2f → %.2f %.2f %.2f)", k, x.positions[k].size(),
				x.positions[k].front().p.x, x.positions[k].front().p.y, x.positions[k].front().p.z, x.positions[k].back().p.x, x.positions[k].back().p.y, x.positions[k].back().p.z);
			printf("\n");
		}
		for(size_t i = 0; i < noeuds.size(); i++){ const NifAVObject *o = (const NifAVObject*)f.blocks[noeuds[i]].data; printf("  nœud %zu %s t=(%.3f %.3f %.3f) bloc %d\n", i, f.String(o->name), o->translation.x, o->translation.y, o->translation.z, noeuds[i]); }
		f.Free(); free(bn); free(ba);
		if(++vus >= (argc > 1 ? 1 : 8)) break;
	}
	return 0;
}
