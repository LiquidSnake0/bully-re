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
	// La colonne STAT d'une entrée peds (docs/idb.md) : le nom de l'entrée de pedstats.dat que le modelinfo garde en
	// +0x54 et que FUN_00478e30 copie dans le piéton à sa création. Indice du modèle → nom (STAT_…).
	std::map<int32, std::string> statDuPieton;
	// La colonne SIZE des entrées peds : son rang dans { Small, Medium, Large, Fat, Huge } (FUN_00488a20 →
	// FUN_0073ab40, strcmp, -1 sinon), que LoadPeds (0x42bfd0) range au modelinfo en +0x74 (FUN_00522220) et que la
	// condition CharacterSize lit (FUN_00601460 → FUN_00522230). Indice du modèle → rang.
	std::map<int32, int32> tailleDuPieton;
	int32 doublons = 0;                    // un même nom sous deux indices : le premier gagne
};
