// Sonde jetable (non construite par défaut) : mesure chaque animation des
// groupes donnés (durée, boucle, déplacement, hauteur du bassin) pour
// reconnaître une pose assise, adossée, etc. sans nom d'animation.
//   BULLY_DATA=<jeu> build/outils/sonde_poi GROUPE… [--nif MODÈLE]
#include "commun.h"
#include "../src/anim/Agr.h"
#include <cmath>

int
main(int argc, char **argv)
{
	outil::Archives a;
	if(!outil::Ouvrir(a)){ printf("archives introuvables\n"); return 1; }
	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "--nif") == 0 && i + 1 < argc){
			// Les nœuds d'un modèle de piétons : nom, translation de repos, parent implicite (ordre du fichier).
			uint32 nb; uint8 *b = outil::LireMonde(a, std::string(argv[++i]) + ".nif", &nb); CNifFile f{};
			if(b && f.Load(b, nb)) for(int32 k = 0; k < f.numBlocks && k < 400; k++) if(f.blocks[k].kind == NIF_NODE && f.blocks[k].data){
				const NifAVObject *o = (const NifAVObject*)f.blocks[k].data; const char *n = f.String(o->name);
				if(n && (strstr(n, "Root") || strstr(n, "Dummy") || strstr(n, "Pelvis") || strstr(n, "Spine") == n)) printf("  nœud %d %s t=(%.3f %.3f %.3f)\n", k, n, o->translation.x, o->translation.y, o->translation.z);
			}
			f.Free(); free(b); continue;
		}
		uint32 nb; uint8 *b = outil::LireMonde(a, std::string(argv[i]) + ".agr", &nb);
		std::vector<AgrAnim> g;
		if(!b || !AgrLireGroupe(b, nb, g)){ printf("%s : illisible\n", argv[i]); free(b); continue; }
		printf("%s : %zu animations\n", argv[i], g.size());
		for(size_t k = 0; k < g.size(); k++){
			const AgrAnim &x = g[k];
			if(!x.decodee){ printf("  %2zu non décodée (type %d)\n", k, x.type); continue; }
			CVector d = AgrDeplacement(x), r0, r1;
			float zmin = 1e9f, zmax = -1e9f; bool aR = false;
			for(int s = 0; s <= 20; s++){ CVector r; if(AgrPositionOs(x, 1, x.duree * s / 20 * 0.9999f, &r)){ aR = true; zmin = fminf(zmin, r.z); zmax = fmaxf(zmax, r.z); } }
			AgrPositionOs(x, 1, 0, &r0); AgrPositionOs(x, 1, x.duree * 0.9999f, &r1);
			float pire = 0;
			for(int o = 2; o < x.numOs - 1; o++){ float q0[4], q1[4];
				if(AgrRotation(x, o, 0, q0) && AgrRotation(x, o, x.duree * 0.9999f, q1)) pire = fmaxf(pire, 1 - fabsf(q0[0]*q1[0] + q0[1]*q1[1] + q0[2]*q1[2] + q0[3]*q1[3])); }
			// Le cap du bassin : l'axe x de l'os Root tourné par sa rotation à t = 0, projeté au sol.
			float cq[4], cm[3][3], capB = 0;
			if(AgrRotation(x, 1, 0, cq)){ AgrMatrice(cq, cm); capB = atan2f(cm[1][0], cm[0][0]) * 180 / 3.14159265f; }
			float capB2 = 0; if(AgrRotation(x, 1, 0, cq)){ AgrMatrice(cq, cm); capB2 = atan2f(cm[0][1], cm[0][0]) * 180 / 3.14159265f; }
			printf("  cap bassin %6.1f / %6.1f |", capB, capB2);
			printf("  %2zu type %d, %d os, %5.2f s, %s, flèche (%.2f %.2f), bassin %s z %.2f→%.2f [%.2f..%.2f] xy (%.2f %.2f)\n", k, x.type, x.numOs, x.duree,
			       pire < 0.01f ? "boucle" : "ne boucle pas", d.x, d.y, aR ? "" : "(sans piste)", r0.z, r1.z, zmin, zmax, r0.x, r0.y);
		}
		free(b);
	}
	return 0;
}
