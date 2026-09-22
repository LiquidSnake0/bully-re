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
	const char *racine = getenv("BULLY_DATA");
	snprintf(sortie, taille, "%s/%s", racine ? racine : ".", cheminJeu);
	for(char *c = sortie; *c; c++) if(*c == '\\') *c = '/';
	ResoudreCasse(sortie);
}
