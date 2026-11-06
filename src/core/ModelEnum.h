// MODELENUM : la quatrième énumération posée dans Lua par CreateLuaState (0x5db040, docs/lua.md).
// `MODELENUM._Taxicab` passe par 0x5db1d0 : la chaîne sans son premier caractère, puis
// FUN_0051c1e0, qui hache le nom (HashString, majuscules) et cherche le modèle de ce hachage
// dans la table des CModelInfo (12 200 entrées) ; l'indice, ou -1. Les noms sont ceux des .idb
// de Objects/ide.img (docs/idb.md) : piétons, objets, véhicules, armes, props, vêtements…
// Ici : la même table, lue depuis les .idb, sans la casse.
#pragma once
#include "../common.h"
#include <map>
#include <set>
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
	// La colonne TYPE des entrées peds : son rang parmi les 15 types de piétons (PREFECT, NERD, JOCK, DROPOUT, GREASER,
	// PREPPY, STUDENT, COP, TEACHER, TOWNPERSON, SHOPKEEP, BULLY, DEFAULT, PLAYER1, PLAYER2 ; la table 0x90e7d0, par
	// FUN_004889c0 → FUN_0073ab40, strcmp, -1 sinon), que LoadPeds range au modelinfo en +0x4c et que le constructeur de
	// CPed (FUN_0047ffe0) recopie dans le piéton (+0x1310). La colonne FEMALE, non nulle, devient l'octet +0x50 du
	// modelinfo (la condition IsFemale, FUN_005ffb50). Indice du modèle → rang, → vrai.
	std::map<int32, int32> typeDuPieton;
	std::map<int32, bool> femme;
	// Le nom du modèle tel que l'IDE l'écrit (le HashString de ce nom est le +4 du modelinfo, que PedModelID compare).
	std::map<int32, std::string> nomDuModele;
	// Les colonnes ACTIONTREE ROOT / FILE et AITREE ROOT / FILE des entrées peds, que LoadPeds (0x42bfd0) passe à
	// FUN_00522580 (le modelinfo garde la racine et le fichier de l'arbre d'actions en +0x64 et +0x68) et à FUN_00522680
	// (ceux de l'arbre de l'IA en +0x6c et +0x70) ; chacune libère les anciennes chaînes et recopie les nouvelles : une
	// entrée relue pour le même modèle (FUN_0051b210) remplace la précédente. Au démarrage, sans l'octet DAT_00bf3809,
	// l'arbre absent de l'arbre global (FUN_005f59b0) voit son fichier chargé (FUN_005f6c50, FUN_005dbd50). L'initialisation
	// du composant d'actions du piéton (FUN_00484d10) les pose. Indice du modèle → les quatre chaînes.
	struct Arbres { std::string actions, fichierActions, ia, fichierIA; };
	std::map<int32, Arbres> arbresDuPieton;
	// Les noms des entrées weap (0xa136b0 … 0xa136b4) et item (0xa136b8 … 0xa136bc) : CWeapon::RegisterCB (FUN_004da2d0)
	// cherche la fiche de chacune par son nom (FUN_0050eaa0, le +4 du modelinfo). Indice du modèle → nom.
	std::map<int32, std::string> armes, objets;
	// Les entrées panm (les objets animés : le type 10 du modelinfo, l'octet +8, que CreatePersistentEntity et FUN_00438790 testent).
	std::set<int32> animes;
	static const char *const typesDePietons[15];
	int32 doublons = 0;                    // un même nom sous deux indices : le premier gagne
};
