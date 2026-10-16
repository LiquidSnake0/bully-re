// Le démarrage des scripts du jeu, image par image (docs/lua.md, « La boucle des threads »).
// Recrée ce que fait le moteur : CreateLuaState (util.lua dans _G), l'objet script de main.lua
// (CreateNameSpace, 0x5d8d40), le thread gamemain (ThreadNameSpace, 0x5d8e40), puis
// LuaScript_Update (0x5da960) à chaque image : réveil des threads dont l'heure est passée,
// globale Alive, lua_resume, retrait des threads finis ou en erreur.
// Wait, CreateThread, TerminateThread et GetTimer sont réels ; le reste de l'API est en
// bouchons qui renvoient nil et journalisent l'appel.
// Les missions (docs/missions.md) : la table est lue dans la banque d'arbres d'actions
// (Missions.cat et ses sous-arbres) ; ForceStartMission suit la machine à états du moteur
// (FUN_006ad3e0) : script de la mission (FUN_005dbf90), MissionSetup jusqu'au bout
// (FUN_006acba0), main (FUN_006abf20), puis MissionCleanup (FUN_006abed0).
//   BULLY_DATA=<racine du jeu> [LANCER_JOURNAL=lignes] build/outils/lancer_lua [secondes] [script.lua [fonction]] [--mission NOM]
// Par défaut : 10 s de jeu, main.lua, gamemain.
#include "../src/core/FileMgr.h"
#include "../src/core/CdStream.h"
#include "../src/core/Horloge.h"
#include "../src/core/ActionTree.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <map>
#include <string>
#include <vector>
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

static const char *kImg = "Scripts\\Scripts.img";
static int32 image = -1;
static uint32 maintenant = 0;           // CTimer::m_snTimeInMilliseconds (0xc1a9b4)
static CHorloge horloge;                // ClockGet / ClockSet / ClockSetTickRate (src/core/Horloge.h)

// Un emplacement de thread (0x44 octets dans l'objet script, à partir de +0x48).
enum EtatThread { NOUVEAU = 0, EN_COURS = 1, ARRET_DEMANDE = 2, TUE = 3, FINI = 4 };
struct Thread {
	lua_State *co = nil;                 // +0x00
	int ref = LUA_NOREF;                 // +0x04, garde la coroutine en vie
	int etat = NOUVEAU;                  // +0x10
	int reveil = 0;                      // +0x1c, posé par Wait
	std::string fonction;                // +0x24
	int id = 0;
};
struct Script {
	std::string fichier;                 // +0x04
	std::vector<Thread> threads;         // [0] = premier niveau, puis CreateThread
	int courant = -1;                    // +0x114c
	bool fin = false;                    // +0x1150 : le script se termine (MissionCleanup)
};
// Les objets script vivants : [0] = main.lua, puis la mission en cours.
static std::vector<Script> scripts;
static int scriptCourant = -1;          // l'objet script dont un thread tourne en ce moment

// Une mission de la banque : le nœud (nom en clair ou haché) et son script (champ 28 de MissionData).
struct Mission { std::string nom; uint32 hachage; std::string script; };
static std::vector<Mission> missions;

// La mission en cours et sa machine à états, réduite de FUN_006ad3e0 :
// 1 MissionSetup tourne, 2 main tourne, 3 MissionCleanup tourne.
enum EtatMission { AUCUNE = 0, PREPARATION = 1, DEROULEMENT = 2, NETTOYAGE = 3 };
static struct {
	int etat = AUCUNE;
	int script = -1;
	std::string nom;
	bool reussie = false;
} mission;

static int prochainId = 1;
static std::map<std::string, int> appels;
static int journalRestant = 400;
static std::vector<std::string> derniers;   // les derniers appels, affichés en cas d'erreur

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

static const char *
NomThreadCourant()
{
	if(scriptCourant < 0) return "(premier niveau)";
	const Script &sc = scripts[scriptCourant];
	if(sc.courant < 0 || sc.courant >= (int)sc.threads.size()) return "(premier niveau)";
	return sc.threads[sc.courant].fonction.c_str();
}

static void
Journal(const char *quoi, lua_State *L, int premierArg)
{
	appels[quoi]++;
	std::string args;
	int n = lua_gettop(L);
	for(int i = premierArg; i <= n && i < premierArg + 4; i++){
		if(!args.empty()) args += ", ";
		switch(lua_type(L, i)){
		case LUA_TSTRING: args += "\""; args += lua_tostring(L, i); args += "\""; break;
		case LUA_TNUMBER: { char b[32]; snprintf(b, sizeof(b), "%g", lua_tonumber(L, i)); args += b; break; }
		case LUA_TBOOLEAN: args += lua_toboolean(L, i) ? "true" : "false"; break;
		case LUA_TNIL: args += "nil"; break;
		default: args += lua_typename(L, lua_type(L, i)); break;
		}
	}
	char ligne[512];
	snprintf(ligne, sizeof(ligne), "%6.2f s  %-22s %s(%s)", maintenant / 1000.0, NomThreadCourant(), quoi, args.c_str());
	derniers.push_back(ligne);
	if(derniers.size() > 4) derniers.erase(derniers.begin());
	if(journalRestant <= 0) return;
	journalRestant--;
	printf("%s\n", ligne);
}

static void
AfficherDerniers()
{
	for(const std::string &d : derniers) printf("        juste avant : %s\n", d.c_str());
}

// --- Bouchons : un nom du moteur absent est une table qu'on peut appeler et indexer. ---
static void PousserBouchon(lua_State *L, const char *nom);

// Réponse neutre d'une nouvelle partie, d'après le nom (provisoire, en attendant le vrai moteur) :
// une question (Is…, Has…, Should…, …IsReady) répond false, un compte ou une lecture (Get…, …Count) 0.
static int
ReponseNeutre(lua_State *L, const char *nom)
{
	const char *point = strrchr(nom, '.');
	const char *n = point ? point + 1 : nom;
	// Le joueur a choisi « Histoire » au menu principal.
	if(strcmp(n, "HasStoryModeBeenSelected") == 0){ lua_pushboolean(L, 1); return 1; }
	static const char *questions[] = { "Is", "Has", "Should", "Can", "Are", "Was", "Did", "Does" };
	for(const char *q : questions)
		if(strncmp(n, q, strlen(q)) == 0 && isupper((unsigned char)n[strlen(q)])){ lua_pushboolean(L, 0); return 1; }
	if(strstr(n, "Is") && isupper((unsigned char)strstr(n, "Is")[2])){ lua_pushboolean(L, 0); return 1; }
	if(strstr(n, "Get") || strstr(n, "Count")){ lua_pushnumber(L, 0); return 1; }
	return 0;
}

static int
AppelBouchon(lua_State *L)
{
	const char *nom = lua_tostring(L, lua_upvalueindex(1));
	Journal(nom, L, 2);
	return ReponseNeutre(L, nom);
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

static int
IndexGlobal(lua_State *L)
{
	const char *nom = lua_tostring(L, 2);
	if(nom == nil){ lua_pushnil(L); return 1; }
	PousserBouchon(L, nom);
	return 1;
}

// --- Les fonctions du moteur recréées. ---

// ImportScript (0x5be5f0) : compile LibTable.lur et renvoie la fonction, sans l'exécuter.
static int
ImportScript(lua_State *L)
{
	std::string chemin = luaL_checkstring(L, 1);
	size_t barre = chemin.find_last_of("/\\");
	std::string nom = barre == std::string::npos ? chemin : chemin.substr(barre + 1);
	size_t point = nom.rfind('.');
	if(point != std::string::npos) nom.resize(point);
	nom += ".lur";
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

// Wait(ms) (0x5bfa70) : heure de réveil du thread courant, puis lua_yield.
static int
Wait(lua_State *L)
{
	int ms = (int)luaL_optnumber(L, 1, 0);
	if(scriptCourant >= 0 && scripts[scriptCourant].courant >= 0){
		Thread &t = scripts[scriptCourant].threads[scripts[scriptCourant].courant];
		if(t.reveil != -1) t.reveil = maintenant + ms;
	}
	return lua_yield(L, 0);
}

// Le moteur « prêt » : gamemain attend SystemIsReady() avant de continuer.
static int
SystemIsReady(lua_State *L)
{
	appels["SystemIsReady"]++;
	lua_pushboolean(L, 1);
	return 1;
}

// ClockGet() → heure, minute ; ClockSet(h, m) ; ClockSetTickRate(secondes de jeu par seconde réelle, …).
static int
ClockGet(lua_State *L)
{
	appels["ClockGet"]++;
	lua_pushnumber(L, horloge.Heure());
	lua_pushnumber(L, horloge.Minute());
	return 2;
}

static int
ClockSet(lua_State *L)
{
	Journal("ClockSet", L, 1);
	horloge.Regler((int32)luaL_optnumber(L, 1, 8), (int32)luaL_optnumber(L, 2, 0));
	return 0;
}

static int
ClockSetTickRate(lua_State *L)
{
	Journal("ClockSetTickRate", L, 1);
	horloge.minutesParSeconde = (float)luaL_optnumber(L, 1, 60) / 60.0f;
	return 0;
}

static int
GetTimer(lua_State *L)
{
	lua_pushnumber(L, (lua_Number)maintenant);
	return 1;
}

static int
SurErreur(lua_State *L)
{
	printf("%6.2f s  %-22s ERREUR %s\n", maintenant / 1000.0, NomThreadCourant(),
	       lua_tostring(L, 1) ? lua_tostring(L, 1) : "?");
	AfficherDerniers();
	return 1;
}

// Appel protégé d'une fonction de util.lua qui renvoie une coroutine (0x5d8d40, 0x5d8e40).
static lua_State *
AppelUtil(lua_State *L, const char *fonctionUtil, const char *fichier, const char *fonction, int *ref)
{
	lua_pushcfunction(L, SurErreur);
	int errfunc = lua_gettop(L);
	lua_pushstring(L, fonctionUtil);
	lua_rawget(L, LUA_GLOBALSINDEX);
	lua_pushstring(L, fichier);
	int nargs = 1;
	if(fonction){ lua_pushstring(L, fonction); nargs = 2; }
	if(lua_pcall(L, nargs, 1, errfunc) != 0){
		lua_settop(L, errfunc - 1);
		return nil;
	}
	lua_State *co = lua_type(L, -1) == LUA_TTHREAD ? lua_tothread(L, -1) : nil;
	if(co) *ref = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_settop(L, errfunc - 1);
	return co;
}

// ThreadNameSpace (0x5d8e40) : un thread de plus dans l'objet script n° s.
static int
NouveauThread(lua_State *L, int s, const char *fonction)
{
	Thread t;
	t.co = AppelUtil(L, "ThreadNameSpace", scripts[s].fichier.c_str(), fonction, &t.ref);
	if(t.co == nil) return 0;
	t.fonction = fonction;
	t.id = prochainId++;
	scripts[s].threads.push_back(t);
	return t.id;
}

// FUN_005dbf90(fichier, 0) : l'objet script ; CreateNameSpace exécute son premier niveau.
static int
NouveauScript(lua_State *L, const char *fichier)
{
	Script sc;
	sc.fichier = fichier;
	Thread premier;
	premier.co = AppelUtil(L, "CreateNameSpace", fichier, nil, &premier.ref);
	premier.fonction = "(premier niveau)";
	if(premier.co == nil) return -1;
	sc.threads.push_back(premier);
	scripts.push_back(sc);
	return (int)scripts.size() - 1;
}

// FUN_005d8db0 : un thread de cette fonction existe dans le script et n'est pas fini (état != 4).
static bool
ThreadEnCours(int s, const char *fonction)
{
	if(s < 0) return false;
	for(size_t i = 1; i < scripts[s].threads.size(); i++)
		if(scripts[s].threads[i].fonction == fonction && scripts[s].threads[i].etat < TUE) return true;
	return false;
}

// CreateThread("F_Nom") (0x5bf2f0) : un thread de plus dans le script courant ; renvoie son numéro.
static int
CreateThread(lua_State *L)
{
	const char *f = luaL_checkstring(L, 1);
	Journal("CreateThread", L, 1);
	int id = scriptCourant >= 0 ? NouveauThread(L, scriptCourant, f) : 0;
	lua_pushnumber(L, id);
	return 1;
}

static int
TerminateThread(lua_State *L)
{
	Journal("TerminateThread", L, 1);
	int id = (int)luaL_optnumber(L, 1, 0);
	for(Script &sc : scripts)
		for(Thread &t : sc.threads)
			if(t.id == id && t.etat < ARRET_DEMANDE) t.etat = ARRET_DEMANDE;
	return 0;
}

// LuaScript_Update (0x5da960), réduit à ce qui compte sans le reste du moteur.
static void
MettreAJourScript(lua_State *L, int s)
{
	for(size_t i = 1; i < scripts[s].threads.size(); i++){
		Thread &t = scripts[s].threads[i];
		if(t.etat >= TUE) continue;
		if(t.reveil < 0 && t.etat < ARRET_DEMANDE) t.etat = ARRET_DEMANDE;
		if(!((uint32)t.reveil < maintenant || t.etat == ARRET_DEMANDE)) continue;
		if(t.etat == NOUVEAU) t.etat = EN_COURS;
		lua_pushstring(L, "Alive");
		lua_pushboolean(L, t.etat != ARRET_DEMANDE);
		lua_rawset(L, LUA_GLOBALSINDEX);
		scriptCourant = s;
		scripts[s].courant = (int)i;
		lua_State *co = t.co;
		int r = lua_resume(co, 0);
		scripts[s].courant = -1;
		scriptCourant = -1;
		Thread &u = scripts[s].threads[i];      // CreateThread a pu agrandir le tableau pendant la reprise
		lua_Debug ar;
		if(r != 0){
			printf("%6.2f s  %-22s ERREUR %s\n", maintenant / 1000.0, u.fonction.c_str(),
			       lua_tostring(u.co, -1) ? lua_tostring(u.co, -1) : "?");
			AfficherDerniers();
			u.etat = FINI;
		}else if(lua_getstack(u.co, 0, &ar) == 0 && lua_gettop(u.co) == 0){
			printf("%6.2f s  %-22s fini\n", maintenant / 1000.0, u.fonction.c_str());
			u.etat = FINI;
		}else if(u.etat == ARRET_DEMANDE && u.fonction != "MissionCleanup"){
			u.etat = TUE;
		}
		if(u.etat >= TUE){ luaL_unref(L, LUA_REGISTRYINDEX, u.ref); u.ref = LUA_NOREF; }
	}
}

// La machine à états de la mission en cours (FUN_006ad3e0, cas 2, 9 et 10).
static void
AvancerMission(lua_State *L)
{
	switch(mission.etat){
	case PREPARATION:
		// FUN_006abf20 : MissionSetup fini, le thread main démarre.
		if(!ThreadEnCours(mission.script, "MissionSetup")){
			printf("%6.2f s  mission %s : MissionSetup fini, main démarre\n", maintenant / 1000.0, mission.nom.c_str());
			NouveauThread(L, mission.script, "main");
			mission.etat = DEROULEMENT;
		}
		break;
	case DEROULEMENT:
		// FUN_006abed0 : main fini (ou arrêté par MissionSucceed / MissionFail), MissionCleanup.
		if(!ThreadEnCours(mission.script, "main")){
			printf("%6.2f s  mission %s : main fini, MissionCleanup\n", maintenant / 1000.0, mission.nom.c_str());
			scripts[mission.script].fin = true;
			NouveauThread(L, mission.script, "MissionCleanup");
			mission.etat = NETTOYAGE;
		}
		break;
	case NETTOYAGE:
		if(!ThreadEnCours(mission.script, "MissionCleanup")){
			printf("%6.2f s  mission %s terminée (%s)\n", maintenant / 1000.0, mission.nom.c_str(),
			       mission.reussie ? "réussie" : "échouée ou arrêtée");
			for(Thread &t : scripts[mission.script].threads)
				if(t.etat < TUE){ t.etat = TUE; luaL_unref(L, LUA_REGISTRYINDEX, t.ref); t.ref = LUA_NOREF; }
			mission.etat = AUCUNE;
			mission.script = -1;
		}
		break;
	}
}

// LuaScript_Update (0x5da960), réduit à ce qui compte sans le reste du moteur.
static void
MettreAJour(lua_State *L)
{
	for(size_t s = 0; s < scripts.size(); s++) MettreAJourScript(L, (int)s);
	AvancerMission(L);
}

// RequestModel(id) (0x5c61e0), WeaponRequestModel (0x5c6340), VehicleRequestModel (0x5d5c60) :
// demandent le modèle et renvoient vrai quand il est chargé (état de diffusion 1). PedRequestModel
// (0x5c6320) ne renvoie rien et reste en bouchon. Les scripts bouclent dessus (« while not RequestModel(id) do Wait(0) end »).
// Sans diffusion dans cet outil, un modèle est chargé dès qu'on le demande : provisoire.
static int
RequestModel(lua_State *L)
{
	appels["RequestModel"]++;
	lua_pushboolean(L, 1);
	return 1;
}

// --- Les missions ---

static const Mission *
TrouverMission(const char *nom)
{
	uint32 h = ActionHash(nom) & 0x7fffffff;
	for(const Mission &m : missions){
		if(!m.nom.empty() && strcasecmp(m.nom.c_str(), nom) == 0) return &m;
		if(m.hachage == h) return &m;
	}
	return nil;
}

static void
RelirePistes(const CActionTreeFile &f, const CActionNode &n)
{
	static const uint32 kMissionData = ActionHash("MissionData") & 0x7fffffff;
	for(int32 d : n.pistes){
		CActionTrack t;
		if(!f.Piste(d, t) || t.type != kMissionData) continue;
		const CActionAttribut *a = t.Champ(28);
		Mission m{n.nom, n.hachage & 0x7fffffff, a ? f.ChaineCitee(a->decalage) : std::string()};
		if(!m.script.empty()) missions.push_back(m);
	}
	for(const CActionNode &e : n.enfants) RelirePistes(f, e);
}

// FUN_006ac5d0 : la banque des missions, Missions.cat et les sous-arbres qu'elle cite.
static void
ChargerMissions(void)
{
	int32 act = CdStream::AddImage("Act\\Act.img");
	if(act < 0){ printf("Act\\Act.img introuvable : pas de table des missions\n"); return; }
	static const char *banques[] = { "Missions", "chap1", "chap2", "chap3", "chap4", "chap5", "classes",
	                                  "Minigames", "Punish", "Tutorials", "Debug" };
	for(const char *b : banques){
		std::string nom = std::string(b) + ".cat";
		const CDirectoryEntry *d = CdStream::ms_images[act].Find(nom.c_str());
		if(d == nil) continue;
		uint32 octets = d->size * CDSTREAM_SECTOR_SIZE;
		uint8 *buf = (uint8*)malloc(octets);
		int32 fd = CFileMgr::OpenFile("Act\\Act.img", "rb", 1);
		CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
		bool ok = CFileMgr::ReadExact(fd, buf, octets);
		CFileMgr::CloseFile(fd);
		CActionTreeFile f;
		if(ok && f.Load(buf, octets)) RelirePistes(f, f.racine);
		free(buf);
	}
	printf("%zu missions dans la banque\n", missions.size());
}

// ForceStartMission(nom) : FUN_006acba0, le script de la mission puis MissionSetup.
static int
ForceStartMission(lua_State *L)
{
	const char *nom = luaL_checkstring(L, 1);
	Journal("ForceStartMission", L, 1);
	if(mission.etat != AUCUNE){ printf("        %s refusée : %s est en cours\n", nom, mission.nom.c_str()); return 0; }
	const Mission *m = TrouverMission(nom);
	if(m == nil){ printf("        %s absente de la banque\n", nom); return 0; }
	int s = NouveauScript(L, m->script.c_str());
	if(s < 0){ printf("        %s : CreateNameSpace(%s) a échoué\n", nom, m->script.c_str()); return 0; }
	mission.nom = nom;
	mission.script = s;
	mission.reussie = false;
	mission.etat = PREPARATION;
	printf("%6.2f s  mission %s : %s chargé, MissionSetup démarre\n", maintenant / 1000.0, nom, m->script.c_str());
	NouveauThread(L, s, "MissionSetup");
	return 0;
}

// MissionSucceed / MissionFail : la mission s'arrête ; main est arrêté, MissionCleanup suivra.
static void
ArreterMain(void)
{
	if(mission.script < 0) return;
	for(Thread &t : scripts[mission.script].threads)
		if(t.fonction == "main" && t.etat < ARRET_DEMANDE) t.etat = ARRET_DEMANDE;
}

static int
MissionSucceed(lua_State *L){ Journal("MissionSucceed", L, 1); mission.reussie = true; ArreterMain(); return 0; }

static int
MissionFail(lua_State *L){ Journal("MissionFail", L, 1); mission.reussie = false; ArreterMain(); return 0; }

static int
MissionActive(lua_State *L)
{
	appels["MissionActive"]++;
	lua_pushboolean(L, mission.etat != AUCUNE);
	return 1;
}

static int
MissionActiveSpecific(lua_State *L)
{
	appels["MissionActiveSpecific"]++;
	const char *nom = luaL_checkstring(L, 1);
	lua_pushboolean(L, mission.etat != AUCUNE && strcasecmp(mission.nom.c_str(), nom) == 0);
	return 1;
}

static int
MissionGetCurrentName(lua_State *L)
{
	appels["MissionGetCurrentName"]++;
	if(mission.etat == AUCUNE) lua_pushnil(L);
	else lua_pushstring(L, mission.nom.c_str());
	return 1;
}

int
main(int argc, char **argv)
{
	const char *aLancer = nil;           // --mission NOM : ForceStartMission(NOM) après une seconde
	std::vector<const char*> pos;
	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "--mission") == 0 && i + 1 < argc) aLancer = argv[++i];
		else pos.push_back(argv[i]);
	}
	if(getenv("LANCER_JOURNAL")) journalRestant = atoi(getenv("LANCER_JOURNAL"));   // lignes de journal (400)
	double secondes = pos.size() > 0 ? atof(pos[0]) : 10.0;
	const char *fichier = pos.size() > 1 ? pos[1] : "main.lua";
	const char *fonction = pos.size() > 2 ? pos[2] : "gamemain";
	image = CdStream::AddImage(kImg);
	if(image < 0){ printf("%s introuvable (BULLY_DATA ?)\n", kImg); return 1; }

	// CreateLuaState (0x5db260) : bibliothèques, API, puis util.lua dans _G.
	lua_State *L = lua_open();
	luaopen_base(L); luaopen_table(L); luaopen_math(L); luaopen_debug(L);
	lua_settop(L, 0);
	lua_register(L, "ImportScript", ImportScript);
	lua_register(L, "Wait", Wait);
	lua_register(L, "GetTimer", GetTimer);
	lua_register(L, "SystemIsReady", SystemIsReady);
	lua_register(L, "ClockGet", ClockGet);
	lua_register(L, "ClockSet", ClockSet);
	lua_register(L, "ClockSetTickRate", ClockSetTickRate);
	lua_register(L, "CreateThread", CreateThread);
	lua_register(L, "TerminateThread", TerminateThread);
	lua_register(L, "__onerror", SurErreur);
	lua_register(L, "RequestModel", RequestModel);
	lua_register(L, "WeaponRequestModel", RequestModel);
	lua_register(L, "VehicleRequestModel", RequestModel);
	lua_register(L, "ForceStartMission", ForceStartMission);
	lua_register(L, "MissionSucceed", MissionSucceed);
	lua_register(L, "MissionFail", MissionFail);
	lua_register(L, "MissionActive", MissionActive);
	lua_register(L, "MissionActiveSpecific", MissionActiveSpecific);
	lua_register(L, "MissionGetCurrentName", MissionGetCurrentName);
	ChargerMissions();
	lua_newtable(L);
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, IndexGlobal);
	lua_settable(L, -3);
	lua_setmetatable(L, LUA_GLOBALSINDEX);

	lua_pushcfunction(L, ImportScript);
	lua_pushstring(L, "util.lua");
	if(lua_pcall(L, 1, 1, 0) != 0 || lua_pcall(L, 0, 0, 0) != 0){
		printf("util.lua : %s\n", lua_tostring(L, -1));
		return 1;
	}

	if(NouveauScript(L, fichier) < 0){ printf("CreateNameSpace(%s) a échoué\n", fichier); return 1; }
	printf("%s chargé dans son espace de noms ; lancement de %s\n", fichier, fonction);
	if(NouveauThread(L, 0, fonction) == 0){ printf("ThreadNameSpace(%s, %s) a échoué\n", fichier, fonction); return 1; }

	// La boucle de jeu : une image toutes les 33 ms.
	int images = 0;
	for(maintenant = 0; maintenant < (uint32)(secondes * 1000); maintenant += 33, images++){
		horloge.Avancer(0.033f);
		if(aLancer && maintenant >= 1000){
			lua_pushcfunction(L, ForceStartMission);
			lua_pushstring(L, aLancer);
			lua_call(L, 1, 0);
			aLancer = nil;
		}
		MettreAJour(L);
	}

	size_t crees = 0;
	int vivants = 0;
	for(const Script &sc : scripts)
		for(size_t i = 1; i < sc.threads.size(); i++){ crees++; vivants += sc.threads[i].etat < TUE; }
	printf("\n%d images (%.1f s) : %zu threads créés, %d encore vivants\n", images, secondes, crees, vivants);
	for(const Script &sc : scripts)
		for(size_t i = 1; i < sc.threads.size(); i++)
			printf("  %-14s %-28s %s\n", sc.fichier.c_str(), sc.threads[i].fonction.c_str(),
			       sc.threads[i].etat < TUE ? "vivant" : sc.threads[i].etat == TUE ? "tué" : "fini");
	std::vector<std::pair<int, std::string>> tri;
	for(auto &a : appels) tri.push_back({-a.second, a.first});
	std::sort(tri.begin(), tri.end());
	printf("appels au moteur les plus fréquents :");
	for(size_t i = 0; i < tri.size() && i < 25; i++) printf(" %s (%d)", tri[i].second.c_str(), -tri[i].first);
	printf("\n");
	lua_close(L);
	return 0;
}
