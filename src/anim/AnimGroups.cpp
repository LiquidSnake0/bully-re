#include "AnimGroups.h"
#include "../core/CdStream.h"
#include <cctype>
#include <cstring>

std::string
CAnimGroups::Cle(const char *nom)
{
	std::string s = nom ? nom : "";
	for(char &c : s) c = (char)tolower((unsigned char)c);
	return s;
}

int
CAnimGroups::Lire(const CdImage &img)
{
	for(int32 i = 0; i < img.m_numEntries; i++){
		char nom[CDSTREAM_NAME_LEN + 1];
		memcpy(nom, img.m_entries[i].name, CDSTREAM_NAME_LEN);
		nom[CDSTREAM_NAME_LEN] = '\0';
		size_t n = strlen(nom);
		if(n > 4 && strcasecmp(nom + n - 4, ".agr") == 0){
			nom[n - 4] = '\0';
			groupes[Cle(nom)];
		}
	}
	return Nombre();
}

bool
CAnimGroups::Existe(const char *nom) const
{
	return groupes.count(Cle(nom)) != 0;
}

bool
CAnimGroups::Charger(const char *nom)
{
	auto it = groupes.find(Cle(nom));
	if(it == groupes.end()) return false;
	it->second.etat = CHARGE;
	return true;
}

bool
CAnimGroups::Demander(const char *nom, uint32 image)
{
	auto it = groupes.find(Cle(nom));
	if(it == groupes.end()) return false;
	if(it->second.etat == CONNU){ it->second.etat = DEMANDE; it->second.image = image; }
	return true;
}

int
CAnimGroups::Avancer(uint32 image)
{
	int n = 0;
	for(auto &g : groupes)
		if(g.second.etat == DEMANDE && g.second.image < image){ g.second.etat = CHARGE; n++; }
	return n;
}

bool
CAnimGroups::EstCharge(const char *nom) const
{
	auto it = groupes.find(Cle(nom));
	return it != groupes.end() && it->second.etat == CHARGE;
}

void
CAnimGroups::Decharger(const char *nom)
{
	auto it = groupes.find(Cle(nom));
	if(it != groupes.end()) it->second.etat = CONNU;
}
