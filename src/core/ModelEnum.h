// MODELENUM : la quatrième énumération posée dans Lua par CreateLuaState (0x5db040, docs/lua.md).
// `MODELENUM._Taxicab` passe par 0x5db1d0 : la chaîne sans son premier caractère, puis
// FUN_0051c1e0, qui hache le nom (HashString, majuscules) et cherche le modèle de ce hachage
// dans la table des CModelInfo (12 200 entrées) ; l'indice, ou -1. Les noms sont ceux des .idb
// de Objects/ide.img (docs/idb.md) : piétons, objets, véhicules, armes, props, vêtements…
// Ici : la même table, lue depuis les .idb, sans la casse.
#pragma once
#include "../common.h"
#include <map>
#include <string>

class CModelEnum
{
public:
	// Lit tous les .idb de Objects/ide.img ; faux si l'image manque.
	bool Charger(void);
	// FUN_0051c1e0 : l'indice du modèle de ce nom, ou -1.
	int32 Index(const char *nom) const;
	size_t Nombre(void) const { return parNom.size(); }
	std::map<std::string, int32> parNom;   // nom en majuscules → indice
	int32 doublons = 0;                    // un même nom sous deux indices : le premier gagne
};
