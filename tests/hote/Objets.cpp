// Hôte pour test_objectdata : tables nom → id par haché, et post-init vide.
#include "../../src/objects/ObjectData.h"
#include <cstring>
#include <map>
#include <string>

uint32 HashString(const char *s) { uint32 h = 5381; while(*s) h = h * 33 + (uint8)*s++; return h; }

static std::map<std::string, int32> g_modeles;
int32 FindModelIndexByName(const char *name)
{
	auto it = g_modeles.find(name);
	if(it != g_modeles.end()) return it->second;
	int32 id = (int32)g_modeles.size();
	g_modeles[name] = id;
	return id;
}
int NombreDeModelesObjets(void) { return (int)g_modeles.size(); }
uint16 GetEffectIdByName(const char *name) { return (uint16)(HashString(name) & 0x7fff); }
uint16 GetSoundBankIdByName(const char *name) { return (uint16)(HashString(name) & 0x7fff); }
uint32 GetSoundIdByName(const char *name) { return HashString(name); }
uint32 GetStreamIdByName(const char *name, int32) { return HashString(name); }
uint32 GetPickupButesByName(const char *name) { return HashString(name); }
void ObjectDataPostInit(void) {}
