// Implémentation hôte minimale de CFileMgr pour les tests : la version du
// jeu passe par son propre système de fichiers, ici on lit simplement le
// disque. Le tampon work_buff a la taille du jeu.
#include "../../src/core/FileMgr.h"
#include <cstdio>
#include <cstring>
#include "Chemins.h"

uint8 work_buff[0x1c000];

int32
CFileMgr::LoadFile(const char *path, uint8 *buf, int32 size, const char *mode)
{
	char chemin[512];
	CheminHote(path, chemin, sizeof(chemin));
	FILE *f = fopen(chemin, mode);
	if(f == nil) return 0;
	int32 n = (int32)fread(buf, 1, size - 1, f);
	buf[n] = 0;
	fclose(f);
	return n;
}
