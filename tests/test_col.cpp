// Lit Stream/World.img avec CdStream et charge des fichiers de collision :
// 70wagon.col (COL3, un modèle), bike.col (COLL), puis les 488 .col.
//   BULLY_DATA=<racine du jeu> build/tests/test_col
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/collision/ColModel.h"
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

static CColModel *g_models[400];
static int32 g_ids[400];
static char g_names[400][21];
static int32 g_n;
static void Garder(int32 id, const char *name, CColModel *m, uint8) { if(g_n < 400){ g_ids[g_n] = id; strcpy(g_names[g_n], name); g_models[g_n++] = m; } else { m->RemoveCollisionVolumes(); free(m); } }
static void Vider(void) { for(int32 i = 0; i < g_n; i++){ g_models[i]->RemoveCollisionVolumes(); free(g_models[i]); } g_n = 0; }

int
main(void)
{
	VERIF(CdStream::AddImage("Stream\\World.img") == 0);
	printf("World.dir : %d entrées\n", CdStream::ms_images[0].m_numEntries);
	CColLoader::ms_handler = Garder;
	uint32 bytes;

	// 70wagon.col : COL3 version 4 drapeaux 1, modèle 294, 36 sphères, 1 boîte,
	// 14 sommets, 22 triangles, bloc KD de 12 nœuds
	uint8 *b = Lire("70wagon.col", &bytes);
	VERIF(b != nil);
	if(b){
		VERIF(CColLoader::LoadCollisionFile(b, bytes, 7));
		VERIF(g_n == 1 && g_ids[0] == 294 && g_names[0][0] == '\0');
		CColModel &m = *g_models[0];
		printf("70wagon : rayon %.3f, %d sphères, %d boîtes, %d sommets, %d triangles, %d nœuds KD, %d liens\n",
			m.boundingSphere.radius, m.pColData->numSpheres, m.pColData->numBoxes, m.pColData->numVertices, m.pColData->numTriangles, m.pColData->numKdNodes, m.numLinks);
		VERIF(m.colSlot == 7 && m.hasGeometry);
		VERIF(eq(m.boundingSphere.radius, 3.022f) && eq(m.boundingSphere.center.y, -0.085f));
		VERIF(eq(m.boundingBox.min.x, -1.21f) && eq(m.boundingBox.max.y, 2.54f));
		VERIF(m.pColData->numSpheres == 36 && m.pColData->numBoxes == 1 && m.pColData->numVertices == 14 && m.pColData->numTriangles == 22);
		CColSphere &s = m.pColData->spheres[0];
		VERIF(eq(s.center.x, -0.6218f) && eq(s.radius, 0.4321f) && s.surface == 14 && s.piece == 0);
		CColBox &bx = m.pColData->boxes[0];
		VERIF(eq(bx.min.x, -1.0916f) && eq(bx.max.z, -0.1446f) && bx.surface == 14);
		VERIF(m.pColData->vertices[0].x == 139 && m.pColData->vertices[0].y == -325 && m.pColData->vertices[0].z == -19);
		CColTriangle &t = m.pColData->triangles[0];
		VERIF(t.a == 0 && t.b == 12 && t.c == 1 && t.surface == 15 && t.flag == 1);
		VERIF(m.pColData->triangles[21].a == 6 && m.pColData->triangles[21].c == 11);
		VERIF(m.pColData->numKdNodes == 12 && m.numLinks == 0);
		free(b); Vider();
	}

	// bike.col : ancien format COLL, modèle 277
	b = Lire("bike.col", &bytes);
	VERIF(b != nil);
	if(b){
		bool ok = CColLoader::LoadCollisionFile(b, bytes, 7);
		printf("bike : %s, %d modèle(s), id %d, %d sphères, %d sommets, %d triangles\n", ok ? "ok" : "taille incohérente", g_n, g_n ? g_ids[0] : -1,
			g_n ? g_models[0]->pColData->numSpheres : -1, g_n ? g_models[0]->pColData->numVertices : -1, g_n ? g_models[0]->pColData->numTriangles : -1);
		VERIF(ok && g_n == 1 && g_ids[0] == 277);
		free(b); Vider();
	}

	// tous les .col de World.img
	int32 fichiers = 0, modeles = 0, mauvais = 0;
	for(int32 i = 0; i < CdStream::ms_images[0].m_numEntries; i++){
		const char *n = CdStream::ms_images[0].m_entries[i].name;
		size_t l = strnlen(n, CDSTREAM_NAME_LEN);
		if(l < 4 || strncasecmp(n + l - 4, ".col", 4) != 0) continue;
		b = Lire(n, &bytes);
		if(b == nil) continue;
		fichiers++;
		if(!CColLoader::LoadCollisionFile(b, bytes, 0)){ mauvais++; if(mauvais <= 5) printf("  incohérent : %s\n", n); }
		modeles += g_n;
		free(b); Vider();
	}
	printf("%d fichiers .col, %d modèles, %d incohérents\n", fichiers, modeles, mauvais);
	VERIF(fichiers == 488 && mauvais == 0);
	printf(echecs ? "%d échec(s)\n" : "tout passe\n", echecs);
	return echecs != 0;
}
