// Versions hôtes des tables nom → identifiant et de l'allocateur, pour les
// tests. Elles renvoient un haché stable ou -1, ce qui suffit à vérifier le
// parseur ; les vraies tables viendront des chargeurs de modèles, d'armes et
// de missions.
#include "../../src/peds/PedStats.h"
#include "../../src/core/FileMgr.h"
#include "../../src/core/ModelInfo.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>

// Hachage de secours (djb2) : le vrai HashString (0x576d80) reste à recréer.
uint32 HashString(const char *s) { uint32 h = 5381; while(*s) h = h * 33 + (uint8)*s++; return h; }
void *GameMalloc(size_t size) { return calloc(1, size); }
void CMemoryHeap::Push(int) {}
void CMemoryHeap::Pop(void) {}

int32 GetModelIndexByName(const char *name) { return strcmp(name, "None") == 0 ? -1 : (int32)(HashString(name) & 0x7fff); }
int32 GetCharacterClassId(const char *name) { return (int32)(HashString(name) & 0xff); }
int32 GetWeaponIdByName(const char *name) { return strcmp(name, "unarmed") == 0 ? 0 : (int32)(HashString(name) & 0xff); }
int32 GetMissionIdByName(const char *name) { return (int32)(HashString(name) & 0xffff); }

// Le jeu tourne sur un système insensible à la casse : on résout chaque
// segment du chemin contre le contenu réel du dossier.
#include <dirent.h>
#include <string>
static void
ResoudreCasse(char *chemin)
{
	std::string res, seg, reste = chemin;
	if(!reste.empty() && reste[0] == '/'){ res = "/"; reste.erase(0, 1); }
	while(!reste.empty()){
		size_t k = reste.find('/');
		seg = reste.substr(0, k);
		reste = k == std::string::npos ? "" : reste.substr(k + 1);
		std::string dossier = res.empty() ? "." : res;
		DIR *d = opendir(dossier.c_str());
		std::string trouve = seg;
		if(d){
			for(dirent *e; (e = readdir(d)) != nil;)
				if(strcasecmp(e->d_name, seg.c_str()) == 0){ trouve = e->d_name; break; }
			closedir(d);
		}
		res += (res.empty() || res == "/" ? "" : "/") + trouve;
	}
	strcpy(chemin, res.c_str());
}

// Fichier texte hôte : OpenFile lit tout, LoadLine rend ligne par ligne
// avec le même nettoyage que le binaire (contrôles et virgules → espaces).
static std::vector<char> g_contenu;
static size_t g_pos;
uint8 CFileLoader::ms_lineBuffer[0x15e];

int32 CFileMgr::OpenFile(const char *path, const char *mode, int32)
{
	// chemin du jeu → chemin hôte relatif au dossier passé en variable d'environnement
	char chemin[512];
	const char *racine = getenv("BULLY_DATA");
	snprintf(chemin, sizeof(chemin), "%s/%s", racine ? racine : ".", path);
	for(char *c = chemin; *c; c++) if(*c == '\\') *c = '/';
	ResoudreCasse(chemin);
	FILE *f = fopen(chemin, mode);
	if(f == nil){ fprintf(stderr, "introuvable : %s\n", chemin); return 0; }
	g_contenu.clear();
	char buf[4096]; size_t n;
	while((n = fread(buf, 1, sizeof(buf), f)) > 0) g_contenu.insert(g_contenu.end(), buf, buf + n);
	fclose(f);
	g_pos = 0;
	return 1;
}
void CFileMgr::Seek(int32, int32 offset, int32) { g_pos = (size_t)offset; }
void CFileMgr::CloseFile(int32) {}
bool CFileMgr::Read(int32, void *buf, int32 size)
{
	if(g_pos >= g_contenu.size()) return false;
	size_t n = std::min((size_t)size, g_contenu.size() - g_pos);
	memcpy(buf, &g_contenu[g_pos], n);
	if(n < (size_t)size) ((char*)buf)[n] = 0;
	// avancer jusqu'après la fin de ligne lue, comme le fread par bloc suivi du repositionnement
	size_t fin = g_pos;
	while(fin < g_contenu.size() && g_contenu[fin] != '\n') fin++;
	g_pos = fin < g_contenu.size() ? fin + 1 : g_contenu.size();
	return true;
}

// 0x429ac0 : tampon de 0x15e, fin de ligne coupée, contrôles et virgules
// remplacés par des espaces, blancs de tête sautés.
char *
CFileLoader::LoadLine(int32 fd)
{
	memset(ms_lineBuffer, 0, sizeof(ms_lineBuffer));
	if(!CFileMgr::Read(fd, ms_lineBuffer, sizeof(ms_lineBuffer))) return nil;
	char *s = (char*)ms_lineBuffer;
	for(int i = 0; s[i] != '\0'; i++){
		if(s[i] == '\n'){ s[i] = '\0'; break; }
		if(s[i] < ' ' || s[i] == ',') s[i] = ' ';
	}
	while(*s != '\0' && *s < '!') s++;
	return s;
}
