// La carte des scènes : chaque fichier de placements (.ipb) de World.img avec
// son emprise dans le monde, et ses voisines à moins de --rayon mètres.
//   BULLY_DATA=<racine> build/outils/carte [--rayon 60]
#include "commun.h"
#include "monde.h"

int
main(int argc, char **argv)
{
	float rayon = 60;
	for(int i = 1; i < argc; i++) if(strcmp(argv[i], "--rayon") == 0 && i + 1 < argc) rayon = (float)atof(argv[++i]);
	outil::Archives a;
	if(!outil::Ouvrir(a)) return 1;
	std::vector<outil::EmpriseIpb> carte = outil::CarteDesIpb(a);
	printf("%zu fichiers de placements\n", carte.size());
	for(const auto &e : carte){
		printf("%-24s %5d  x %8.1f .. %8.1f  y %8.1f .. %8.1f  z %7.1f .. %7.1f  voisines :",
		       e.nom.c_str(), e.placements, e.xmin, e.xmax, e.ymin, e.ymax, e.zmin, e.zmax);
		for(const auto &f : carte){
			if(&f == &e) continue;
			float dx = fmaxf(0, fmaxf(f.xmin - e.xmax, e.xmin - f.xmax)), dy = fmaxf(0, fmaxf(f.ymin - e.ymax, e.ymin - f.ymax));
			if(hypotf(dx, dy) <= rayon) printf(" %s", f.nom.c_str());
		}
		printf("\n");
	}
	return 0;
}
