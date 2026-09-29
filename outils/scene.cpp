// Rend une scène du jeu depuis un fichier de placements « Ipl$ » (.ipb) :
// chaque entrée inst nomme un modèle, une position, une échelle par axe et un
// quaternion. Tous les modèles sont placés puis dessinés ensemble.
//   BULLY_DATA=<racine> build/outils/scene <fichier.ipb> [sortie.ppm] [azimut] [élévation] [taille] [--coupe f]
// --coupe f ne dessine pas les triangles situés au-dessus de la fraction f de
// la hauteur de la scène : on enlève le toit pour regarder dedans.
// Le sens du quaternion des placements est établi dans docs/ipl.md et
// vérifié par tests/test_placement.
#include "commun.h"
#include "scene.h"

int
main(int argc, char **argv)
{
	if(argc < 2){ fprintf(stderr, "usage : scene <fichier.ipb> [sortie.ppm] [azimut] [élévation] [taille] [--coupe fraction]\n"); return 2; }
	std::string ipb = argv[1]; float coupe = 1.0f; std::vector<std::string> pos;
	for(int i = 2; i < argc; i++){ if(strcmp(argv[i], "--coupe") == 0 && i + 1 < argc) coupe = (float)atof(argv[++i]); else pos.push_back(argv[i]); }
	std::string sortie = pos.size() > 0 ? pos[0] : ipb + ".ppm";
	float azim = pos.size() > 1 ? (float)atof(pos[1].c_str()) : 35.0f, elev = pos.size() > 2 ? (float)atof(pos[2].c_str()) : 35.0f;
	int32 taille = pos.size() > 3 ? atoi(pos[3].c_str()) : 1000;

	outil::Archives a;
	if(!outil::Ouvrir(a)) return 1;
	outil::Scene s;
	if(!outil::ChargerPlacements(a, ipb, s)) return 1;
	bool ok = s.Rendre(sortie, azim, elev, taille, coupe);
	printf("  → %s%s\n", sortie.c_str(), ok ? "" : " (échec)");
	return ok ? 0 : 1;
}
