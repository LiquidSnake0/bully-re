// Extrait default.idb de Objects/ide.img avec CdStream, le parcourt avec
// CIdeBinary, et vérifie la section « peds » contre Objects/default.ide.
//   BULLY_DATA=<racine du jeu> build/tests/test_ide
#include "../src/core/CdStream.h"
#include "../src/core/IdeBinary.h"
#include "../src/core/FileMgr.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)
static CPedIdeEntry g_peds[400];
static int32 g_n;
static int32 Garder(const CPedIdeEntry &e) { if(g_n < 400) g_peds[g_n++] = e; return 0; }
void RegisterModelRange(uint16 first, uint32 count) { printf("plage de modèles %u + %u\n", first, count); }

int
main(void)
{
	VERIF(CdStream::AddImage("Objects\\ide.img") == 0);
	const CDirectoryEntry *d = CdStream::ms_images[0].Find("default.idb");
	VERIF(d != nil);
	if(d == nil) return 1;
	uint32 bytes = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(bytes);
	int32 fd = CFileMgr::OpenFile("Objects\\ide.img", "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	VERIF(CFileMgr::ReadExact(fd, buf, bytes));
	CFileMgr::CloseFile(fd);
	uint32 utile; memcpy(&utile, buf, 4);
	printf("default.idb : %u octets utiles sur %u\n", utile, bytes);
	VERIF(utile == 71176);

	CIdeBinary::ms_pedHandler = Garder;
	bool complet = CIdeBinary::Load(buf + 4, utile - 4);
	printf("peds lus : %d ; toutes les sections connues : %s\n", CIdeBinary::ms_numPeds, complet ? "oui" : "non");
	VERIF(CIdeBinary::ms_numPeds == 259);
	VERIF(g_n >= 3);
	CPedIdeEntry &p = g_peds[0];
	VERIF(p.id == 0 && strcmp(p.model, "player") == 0 && strcmp(p.txd, "player") == 0 && p.female == 0);
	VERIF(strcmp(p.size, "Medium") == 0 && strcmp(p.type, "PLAYER1") == 0 && strcmp(p.stat, "STAT_PLAYER") == 0);
	VERIF(strcmp(p.animGroup[0], "Grap") == 0 && strcmp(p.animGroup[3], "null") == 0 && p.unique == -1);
	VERIF(strcmp(p.actionRoot, "/Global/Player") == 0 && strcmp(p.actionFile, "Act/Player.act") == 0);
	VERIF(strcmp(p.aiRoot, "/Global/PlayerAI") == 0 && strcmp(p.aiFile, "Act/PlayerAI.act") == 0);
	VERIF(strcmp(p.name, "N_Jimmy") == 0);
	CPedIdeEntry &z = g_peds[2];
	VERIF(z.id == 2 && strcmp(z.model, "DOgirl_Zoe_EG") == 0 && z.female == 1 && strcmp(z.stat, "STAT_GS_FEMALE_A") == 0);
	VERIF(strcmp(z.animGroup[0], "F_Girls") == 0 && strcmp(z.animGroup[3], "Straf_Female") == 0 && strcmp(z.name, "N_Zoe") == 0);
	printf(echecs ? "%d échec(s)\n" : "tout passe\n", echecs);
	return echecs != 0;
}
