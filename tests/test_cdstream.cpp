// Lit Stream/World.dir et Objects/ide.dir avec CdStream recréé.
//   BULLY_DATA=<racine du jeu> build/tests/test_cdstream
#include "../src/core/CdStream.h"
#include <cstdio>
#include <cstring>
static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)
int main(void)
{
	int32 w = CdStream::AddImage("Stream\\World.img");
	VERIF(w == 0);
	CdImage &img = CdStream::ms_images[0];
	printf("World : %d entrées, première %s (%u, %u)\n", img.m_numEntries, img.m_entries[0].name, img.m_entries[0].offset, img.m_entries[0].size);
	VERIF(img.m_numEntries == 11980);
	VERIF(strcmp(img.m_entries[0].name, "Algie1.lur") == 0 && img.m_entries[0].offset == 0 && img.m_entries[0].size == 4);
	VERIF(img.Find("algie1.lur") == &img.m_entries[0]);
	int32 i = CdStream::AddImage("Objects\\ide.img");
	VERIF(i == 1);
	CdImage &ide = CdStream::ms_images[1];
	printf("ide : %d entrées, première %s\n", ide.m_numEntries, ide.m_entries[0].name);
	VERIF(ide.m_numEntries > 0);
	printf(echecs ? "%d échec(s)\n" : "tout passe\n", echecs);
	return echecs != 0;
}
