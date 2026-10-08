// Les scripts Lua du jeu dans la VM d'origine (Lua 5.0.2 de tiers/, nombres en
// float, size_t et instructions de 4 octets : voir docs/lua.md).
// Charge chaque .lur de Scripts/Scripts.img, exécute son code de premier niveau
// avec ImportScript réel (les bibliothèques sont dans la même archive) et le reste
// de l'API en bouchons, puis rapporte : chargement, exécution, fonctions définies,
// noms du moteur touchés au premier niveau, noms absents de l'API du moteur.
//   BULLY_DATA=<racine du jeu> build/outils/sonde_lua [script.lur]
// Avec SONDE_ARGS=1, chaque appel à un bouchon avec une chaîne en premier argument est affiché.
#include "../src/core/FileMgr.h"
#include "../src/core/CdStream.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <strings.h>
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

static const char *kImg = "Scripts\\Scripts.img";
static int32 image = -1;
static std::set<std::string> api;                       // docs/api-lua-tables.tsv, tables de .data (le moteur)
static std::map<std::string, int> touchesTous;          // nom -> scripts qui le touchent au premier niveau
static std::map<std::string, int> importsTous;          // bibliothèque -> scripts qui l'importent
static std::vector<std::string> touchesScript;          // noms du moteur touchés par le script en cours
static std::set<std::string> importsScript;

static uint8 *
Lire(const CDirectoryEntry *d, uint32 *octets)
{
	*octets = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(*octets);
	int32 fd = CFileMgr::OpenFile(kImg, "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, *octets);
	CFileMgr::CloseFile(fd);
	if(!ok){ free(buf); return nil; }
	return buf;
}

// Un nom du moteur absent : une table qu'on peut appeler (renvoie nil) et indexer
// (MODELENUM.x, shared.y), pour que le premier niveau aille le plus loin possible.
static void PousserBouchon(lua_State *L, const char *nom);

static int
AppelBouchon(lua_State *L)
{
	const char *nom = lua_tostring(L, lua_upvalueindex(1));
	touchesScript.push_back(nom);
	if(getenv("SONDE_ARGS") && lua_gettop(L) >= 2 && lua_type(L, 2) == LUA_TSTRING)
		printf("ARG %s(\"%s\")\n", nom, lua_tostring(L, 2));
	return 0;
}

static int
IndexBouchon(lua_State *L)
{
	std::string nom = lua_tostring(L, lua_upvalueindex(1));
	const char *cle = lua_tostring(L, 2);
	nom += ".";
	nom += cle ? cle : "?";
	PousserBouchon(L, nom.c_str());
	return 1;
}

static void
PousserBouchon(lua_State *L, const char *nom)
{
	lua_newtable(L);
	lua_newtable(L);
	lua_pushstring(L, "__call");
	lua_pushstring(L, nom);
	lua_pushcclosure(L, AppelBouchon, 1);
	lua_settable(L, -3);
	lua_pushstring(L, "__index");
	lua_pushstring(L, nom);
	lua_pushcclosure(L, IndexBouchon, 1);
	lua_settable(L, -3);
	lua_setmetatable(L, -2);
}

// __index des globales : un nom jamais affecté est un nom du moteur.
static int
IndexGlobal(lua_State *L)
{
	const char *nom = lua_tostring(L, 2);
	if(nom == nil){ lua_pushnil(L); return 1; }
	touchesScript.push_back(nom);
	PousserBouchon(L, nom);
	return 1;
}

// ImportScript("Library/LibTable.lua") du moteur : cherche LibTable.lur dans Scripts.img (le
// dossier et l'extension du chemin ne comptent pas), le compile et renvoie la fonction sans
// l'exécuter. C'est util.lua qui l'enveloppe en « ImportScript(f) = ancien(f)() » ; un script
// lancé sans util appelle donc la version du moteur et n'exécute rien.
static int
ImportScriptMoteur(lua_State *L)
{
	std::string chemin = luaL_checkstring(L, 1);
	size_t barre = chemin.find_last_of("/\\");
	std::string nom = barre == std::string::npos ? chemin : chemin.substr(barre + 1);
	size_t point = nom.rfind('.');
	if(point != std::string::npos) nom.resize(point);
	nom += ".lur";
	std::string cle = nom;
	for(char &c : cle) c = tolower(c);
	importsScript.insert(cle);
	const CDirectoryEntry *d = CdStream::ms_images[image].Find(nom.c_str());
	if(d == nil) return luaL_error(L, "ImportScript : %s absent de Scripts.img", nom.c_str());
	uint32 octets;
	uint8 *buf = Lire(d, &octets);
	if(buf == nil) return luaL_error(L, "ImportScript : lecture de %s impossible", nom.c_str());
	int err = luaL_loadbuffer(L, (const char*)buf, octets, nom.c_str());
	free(buf);
	if(err != 0) return lua_error(L);
	return 1;
}

// Exécute un script de l'archive dans l'état courant (pour SONDE_AVANT).
static int
ExecuterScript(lua_State *L)
{
	ImportScriptMoteur(L);
	lua_call(L, 0, 0);
	return 0;
}

static bool
ChargerApi(const char *chemin)
{
	FILE *f = fopen(chemin, "r");
	if(f == nil) return false;
	char ligne[256];
	while(fgets(ligne, sizeof(ligne), f)){
		ligne[strcspn(ligne, "\r\n")] = 0;
		char *tab1 = strchr(ligne, '\t');
		char *tab2 = tab1 ? strchr(tab1 + 1, '\t') : nil;
		if(tab2 && strncmp(tab2 + 1, ".data:", 6) == 0){ *tab1 = 0; api.insert(ligne); }
	}
	fclose(f);
	return true;
}

struct Resultat {
	std::string nom;
	bool charge = false, execute = false;
	std::string erreur;
	std::vector<std::string> fonctions;
};

static Resultat
Sonder(const char *nom, const uint8 *buf, uint32 octets, bool detail)
{
	Resultat r; r.nom = nom;
	touchesScript.clear();
	importsScript.clear();
	lua_State *L = lua_open();
	// Les bibliothèques que le jeu ouvre (tables de .rdata) : base et coroutine, table, math, debug ; ni string, ni io, ni os.
	luaopen_base(L); luaopen_table(L); luaopen_math(L); luaopen_debug(L);
	lua_settop(L, 0);
	lua_register(L, "ImportScript", ImportScriptMoteur);
	// Les globales de départ, pour ne lister ensuite que ce que le script définit.
	std::set<std::string> avant;
	lua_pushnil(L);
	while(lua_next(L, LUA_GLOBALSINDEX)){
		if(lua_type(L, -2) == LUA_TSTRING) avant.insert(lua_tostring(L, -2));
		lua_pop(L, 1);
	}
	lua_newtable(L);
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, IndexGlobal);
	lua_settable(L, -3);
	lua_setmetatable(L, LUA_GLOBALSINDEX);

	// SONDE_AVANT=util.lur,... : scripts exécutés d'abord dans le même état, comme le moteur le ferait.
	if(const char *avantScripts = getenv("SONDE_AVANT")){
		std::string liste = avantScripts;
		size_t debut = 0;
		while(debut <= liste.size()){
			size_t fin = liste.find(',', debut);
			std::string s = liste.substr(debut, fin == std::string::npos ? std::string::npos : fin - debut);
			if(!s.empty()){
				lua_pushcfunction(L, ExecuterScript);
				lua_pushstring(L, s.c_str());
				if(lua_pcall(L, 1, 0, 0) != 0){ r.erreur = std::string("avant : ") + lua_tostring(L, -1); lua_close(L); return r; }
			}
			if(fin == std::string::npos) break;
			debut = fin + 1;
		}
		touchesScript.clear();
	}

	// Le fichier est complété par des zéros jusqu'au secteur : l'undump s'arrête à la fin de la fonction principale.
	if(luaL_loadbuffer(L, (const char*)buf, octets, nom) != 0){
		r.erreur = lua_tostring(L, -1);
		lua_close(L);
		return r;
	}
	r.charge = true;
	if(lua_pcall(L, 0, 0, 0) != 0)
		r.erreur = lua_tostring(L, -1) ? lua_tostring(L, -1) : "?";
	else
		r.execute = true;
	lua_pushnil(L);
	while(lua_next(L, LUA_GLOBALSINDEX)){
		if(lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TFUNCTION &&
		   !avant.count(lua_tostring(L, -2)))
			r.fonctions.push_back(lua_tostring(L, -2));
		lua_pop(L, 1);
	}
	lua_close(L);
	std::set<std::string> vus;
	for(const std::string &a : touchesScript)
		if(vus.insert(a).second) touchesTous[a]++;
	for(const std::string &i : importsScript) importsTous[i]++;
	if(detail){
		printf("%s : chargé %s, exécuté %s%s%s\n", nom, r.charge ? "oui" : "non", r.execute ? "oui" : "non",
		       r.erreur.empty() ? "" : " : ", r.erreur.c_str());
		printf("  importe (%zu) :", importsScript.size());
		for(const std::string &i : importsScript) printf(" %s", i.c_str());
		printf("\n  fonctions définies (%zu) :", r.fonctions.size());
		for(const std::string &f : r.fonctions) printf(" %s", f.c_str());
		printf("\n  noms du moteur touchés au premier niveau (%zu) :", vus.size());
		for(const std::string &a : vus) printf(" %s%s", a.c_str(), api.count(a) ? "" : "*");
		printf("\n  (* : absent de l'API du moteur)\n");
	}
	return r;
}

int
main(int argc, char **argv)
{
	if(!ChargerApi("docs/api-lua-tables.tsv")){ printf("docs/api-lua-tables.tsv introuvable : lancer depuis la racine du dépôt\n"); return 1; }
	image = CdStream::AddImage(kImg);
	if(image < 0){ printf("%s introuvable (BULLY_DATA ?)\n", kImg); return 1; }
	const CdImage &im = CdStream::ms_images[image];
	int charges = 0, executes = 0;
	std::map<std::string, int> fonctionsTous, causes;
	for(int32 i = 0; i < im.m_numEntries; i++){
		const CDirectoryEntry &d = im.m_entries[i];
		if(argc > 1 && strcasecmp(d.name, argv[1]) != 0) continue;
		uint32 octets;
		uint8 *buf = Lire(&d, &octets);
		if(buf == nil){ printf("%s : lecture impossible\n", d.name); continue; }
		Resultat r = Sonder(d.name, buf, octets, argc > 1);
		free(buf);
		charges += r.charge; executes += r.execute;
		for(const std::string &f : r.fonctions) fonctionsTous[f]++;
		if(!r.execute){
			if(getenv("SONDE_ECHECS")) printf("échec %s : %s\n", r.nom.c_str(), r.erreur.c_str());
			size_t deux = r.erreur.find(": ", r.erreur.find(':') + 1);
			causes[deux == std::string::npos ? r.erreur : r.erreur.substr(deux + 2)]++;
		}
	}
	if(argc > 1) return 0;
	printf("%d scripts : %d chargés, %d exécutés jusqu'au bout au premier niveau\n", im.m_numEntries, charges, executes);
	for(auto &c : causes) printf("  %3d échecs : %s\n", c.second, c.first.c_str());
	std::vector<std::pair<int, std::string>> tri;
	for(auto &f : fonctionsTous) tri.push_back({-f.second, f.first});
	std::sort(tri.begin(), tri.end());
	printf("fonctions définies le plus souvent :");
	for(size_t i = 0; i < tri.size() && i < 15; i++) printf(" %s (%d)", tri[i].second.c_str(), -tri[i].first);
	printf("\nbibliothèques importées (%zu) :", importsTous.size());
	for(auto &i : importsTous) printf(" %s (%d)", i.first.c_str(), i.second);
	int absents = 0;
	for(auto &a : touchesTous) absents += !api.count(a.first);
	printf("\nnoms du moteur touchés au premier niveau, absents de l'API du moteur (%d) :", absents);
	for(auto &a : touchesTous) if(!api.count(a.first)) printf(" %s (%d)", a.first.c_str(), a.second);
	printf("\n");
	return 0;
}
