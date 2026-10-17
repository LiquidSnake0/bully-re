#include "LuaApi.h"
#include <cstdio>
#include <cstring>

bool
CLuaApi::Charger(const char *chemin)
{
	FILE *f = fopen(chemin, "r");
	if(f == nil) return false;
	char ligne[512];
	bool entete = true;
	while(fgets(ligne, sizeof(ligne), f)){
		if(entete){ entete = false; continue; }
		char *tab = strchr(ligne, '\t');
		if(tab) *tab = '\0';
		char *fin = ligne + strlen(ligne);
		while(fin > ligne && (fin[-1] == '\n' || fin[-1] == '\r' || fin[-1] == ' ')) *--fin = '\0';
		if(ligne[0] == '\0' || ligne[0] == '#') continue;
		noms.insert(ligne);
	}
	fclose(f);
	return true;
}
