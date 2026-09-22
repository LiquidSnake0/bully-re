// Charge ftest.ipb depuis Stream/World.img avec CIplFile, puis les 85 .ipb.
//   BULLY_DATA=<racine du jeu> build/tests/test_ipl
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/core/IplFile.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)
static bool eq(float a, float b) { return fabsf(a - b) < 1e-2f; }

static uint8 *
Lire(const char *nom, uint32 *bytes)
{
	const CDirectoryEntry *d = CdStream::ms_images[0].Find(nom);
	if(d == nil) return nil;
	*bytes = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(*bytes);
	int32 fd = CFileMgr::OpenFile("Stream\\World.img", "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, *bytes);
	CFileMgr::CloseFile(fd);
	if(!ok){ free(buf); return nil; }
	return buf;
}

static CIplInst g_inst[100];
static int32 g_ni;
static int32 GarderInst(const CIplInst &e) { if(g_ni < 100) g_inst[g_ni++] = e; return 0; }
static char g_rail[64];
static int32 g_railPts;
static int32 GarderRail(const CIplRail &e) { strcpy(g_rail, e.name); g_railPts = e.numPoints; return 0; }

int
main(void)
{
	VERIF(CdStream::AddImage("Stream\\World.img") == 0);
	CIplFile::ms_instHandler = GarderInst;
	CIplFile::ms_railHandler = GarderRail;
	uint32 bytes;
	uint8 *b = Lire("ftest.ipb", &bytes);
	VERIF(b != nil);
	if(b){
		VERIF(CIplFile::Load(b, bytes));
		printf("ftest.ipb : %d inst, %d rail\n", CIplFile::ms_numInst, CIplFile::ms_numRail);
		VERIF(CIplFile::ms_numInst == 51 && g_ni == 51 && CIplFile::ms_numRail == 1);
		CIplInst &e = g_inst[0];
		VERIF(e.modelId == 3565 && strcmp(e.name, "FTEST55") == 0 && eq(e.unk68, 22.0f));
		VERIF(eq(e.pos.x, -9.9877f) && eq(e.pos.y, 21.4197f) && eq(e.pos.z, 31.0601f));
		VERIF(eq(e.scale.x, 1.0f) && eq(e.rot[3], 1.0f) && eq(e.rot[0], 0.0f));
		VERIF(g_inst[20].modelId == 10160 && strcmp(g_inst[20].name, "pxLad12M") == 0 && eq(g_inst[20].pos.y, -41.72f));
		VERIF(strcmp(g_rail, "FTestSlideRail01") == 0 && g_railPts == 2);
		free(b);
	}
	int32 fichiers = 0, entiers = 0;
	CIplFile::ms_instHandler = nil; CIplFile::ms_railHandler = nil;
	for(int32 i = 0; i < CdStream::ms_images[0].m_numEntries; i++){
		const char *n = CdStream::ms_images[0].m_entries[i].name;
		size_t l = strnlen(n, CDSTREAM_NAME_LEN);
		if(l < 4 || strncasecmp(n + l - 4, ".ipb", 4) != 0) continue;
		b = Lire(n, &bytes);
		if(b == nil) continue;
		fichiers++;
		if(CIplFile::Load(b, bytes)) entiers++;
		else { char tg[5]; memcpy(tg, &CIplFile::ms_lastTag, 4); tg[4] = 0; printf("  section « %s » non recréée dans %s\n", tg, n); }
		free(b);
	}
	printf("%d fichiers .ipb, %d lus en entier ; inst %d, rail %d, pont %d, prop %d, spec %d\n", fichiers, entiers, CIplFile::ms_numInst, CIplFile::ms_numRail, CIplFile::ms_numPont, CIplFile::ms_numProp, CIplFile::ms_numSpec);
	VERIF(fichiers == 85 && entiers >= 81);
	printf(echecs ? "%d échec(s)\n" : "tout passe\n", echecs);
	return echecs != 0;
}
