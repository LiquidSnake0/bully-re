// Les fonctions que bully.exe enregistre dans Lua : les 58 tables luaL_reg passées à 0x5dafe0 par
// CreateLuaState (docs/lua.md, tools/tables_lua.py, docs/api-lua-tables.tsv). C'est la frontière
// entre le moteur et les scripts : un nom qui est là est une fonction C ; un nom qui n'y est pas
// est une globale de script, et vaut nil tant qu'un script ne l'a pas posée. lancer_lua s'en sert
// pour ne bouchonner que le moteur : gMissionSuccess, gPedroCleaned ou gSecPathNode doivent
// rester nil, sinon « if gMissionSuccess then » devient vrai et la mission se termine avant de
// commencer, ou ne se termine jamais.
#pragma once
#include "../common.h"
#include <set>
#include <string>

class CLuaApi
{
public:
	// Lit le TSV (nom, fonction_c, table ; une ligne d'en-tête) ; faux si le fichier manque.
	bool Charger(const char *chemin);
	bool EstFonction(const char *nom) const { return noms.count(nom) != 0; }
	size_t Nombre(void) const { return noms.size(); }
	std::set<std::string> noms;
};
