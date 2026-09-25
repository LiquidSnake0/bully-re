// Rend un modèle du jeu en image, avec le rasteriseur logiciel de src/render.
//   BULLY_DATA=<racine du jeu> build/outils/rendu <modèle> [sortie.ppm] [azimut] [élévation] [taille]
// Caméra orthographique en orbite autour de la boîte du modèle, Z vers le
// haut. Même chaîne que nif2obj pour les textures.
#include "commun.h"
#include "../src/gamebryo/NifTransform.h"
#include "scene.h"

int
main(int argc, char **argv)
{
	if(argc < 2){ fprintf(stderr, "usage : rendu <modèle> [sortie.ppm] [azimut] [élévation] [taille]\n"); return 2; }
	std::string modele = argv[1];
	std::string sortie = argc > 2 ? argv[2] : modele + ".ppm";
	float azim = argc > 3 ? (float)atof(argv[3]) : 35.0f, elev = argc > 4 ? (float)atof(argv[4]) : 25.0f;
	int32 taille = argc > 5 ? atoi(argv[5]) : 800;

	outil::Archives a;
	if(!outil::Ouvrir(a)) return 1;
	outil::Scene s;
	if(!s.AjouterModele(a, modele, NifIdentity())){ fprintf(stderr, "%s : modèle introuvable ou sans géométrie\n", modele.c_str()); return 1; }
	bool ok = s.Rendre(sortie, azim, elev, taille);
	printf("%s : %zu triangles, %zu textures → %s%s\n", modele.c_str(), s.tri.size() / 3, s.texNoms.size(), sortie.c_str(), ok ? "" : " (échec d'écriture)");
	return ok ? 0 : 1;
}
