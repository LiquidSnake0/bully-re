#include "Chemins.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <strings.h>

static void
ResoudreCasse(char *chemin)
{
	std::string res, seg, reste = chemin;
	if(!reste.empty() && reste[0] == '/'){ res = "/"; reste.erase(0, 1); }
	while(!reste.empty()){
		size_t k = reste.find('/');
		seg = reste.substr(0, k);
		reste = k == std::string::npos ? "" : reste.substr(k + 1);
		std::string dossier = res.empty() ? "." : res;
		DIR *d = opendir(dossier.c_str());
		std::string trouve = seg;
		if(d){
			for(dirent *e; (e = readdir(d)) != nullptr;)
				if(strcasecmp(e->d_name, seg.c_str()) == 0){ trouve = e->d_name; break; }
			closedir(d);
		}
		res += (res.empty() || res == "/" ? "" : "/") + trouve;
	}
	strcpy(chemin, res.c_str());
}

void
CheminHote(const char *cheminJeu, char *sortie, int taille)
{
	// Un chemin absolu de la machine (build/tests/test_surface /…/Config/dat/surface.dat) est gardé tel quel ; les
	// chemins du jeu (Config\Dat\SURFACE.DAT) partent de BULLY_DATA. Sans cela, le test lisait « ./home/… », ne
	// trouvait rien, et CSurfaceTable::Initialise (0x45b250), qui suppose le fichier lu, parcourait le tampon vide
	// au-delà de sa fin : c'était le plantage de test_surface.
	const char *racine = getenv("BULLY_DATA");
	if(cheminJeu[0] == '/') snprintf(sortie, taille, "%s", cheminJeu);
	else snprintf(sortie, taille, "%s/%s", racine ? racine : ".", cheminJeu);
	for(char *c = sortie; *c; c++) if(*c == '\\') *c = '/';
	ResoudreCasse(sortie);
}
