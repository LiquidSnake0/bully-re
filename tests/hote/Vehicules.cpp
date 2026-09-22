// Version hôte de l'objet fichier et de la recherche de modelinfo de
// véhicule, pour tester LoadVehicleColours. Les modelinfos sont créés à la
// demande, par nom, pour que le test puisse relire les paires stockées.
#include "../../src/vehicles/VehicleColours.h"
#include "Chemins.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

static std::map<std::string, CVehicleModelInfo> g_modeles;

CVehicleModelInfo *
FindVehicleModelInfoByName(const char *name)
{
	CVehicleModelInfo &mi = g_modeles[name];
	return &mi;
}

int NombreDeModelesVus(void) { return (int)g_modeles.size(); }

bool
CFileStream::Open(const char *path)
{
	char chemin[512];
	CheminHote(path, chemin, sizeof(chemin));
	m_handle = fopen(chemin, "r");
	if(m_handle == nil) fprintf(stderr, "introuvable : %s\n", chemin);
	return m_handle != nil;
}

bool
CFileStream::ReadLine(char *buf, int32 size)
{
	if(m_handle == nil) return false;
	return fgets(buf, size, (FILE*)m_handle) != nil;
}

void
CFileStream::Close(void)
{
	if(m_handle != nil) fclose((FILE*)m_handle);
	m_handle = nil;
}
