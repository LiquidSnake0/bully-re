// Lit FTEST11.nif depuis Stream/World.img avec CNifFile, vérifie l'en-tête
// et la géométrie contre les valeurs relevées à la main, puis parcourt tous
// les .nif de l'archive et compte les blocs connus décodés.
//   BULLY_DATA=<racine du jeu> build/tests/test_nif
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/gamebryo/NifFile.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

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

int
main(void)
{
	VERIF(CdStream::AddImage("Stream\\World.img") == 0);
	uint32 bytes;
	uint8 *b = Lire("FTEST11.nif", &bytes);
	VERIF(b != nil);
	if(b){
		CNifFile f;
		VERIF(f.Load(b, bytes));
		printf("FTEST11.nif : version %08x, %d blocs, %d types, %d chaînes, %d décodés\n", f.version, f.numBlocks, f.numTypes, f.numStrings, f.numDecoded);
		VERIF(f.version == 0x14030009 && f.numBlocks == 18 && f.numTypes == 8 && f.numStrings == 11);
		VERIF(f.numDecoded == 18 - 3);                 // tout sauf NiZBufferProperty et NiVertexColorProperty (×2)
		VERIF(strcmp(f.String(0), "Scene Root") == 0 && strcmp(f.String(4), "numbers_d.tga") == 0);
		NifNode *racine = (NifNode*)f.blocks[0].data;
		VERIF(f.blocks[0].kind == NIF_NODE && racine && racine->name == 0 && racine->flags == 0x0110);
		VERIF(racine->numProperties == 2 && racine->properties[0] == 2 && racine->properties[1] == 1);
		VERIF(racine->numChildren == 1 && racine->children[0] == 3 && racine->numEffects == 0 && racine->scale == 1.0f);
		VERIF(racine->rotation.m[0][0] == 1.0f && racine->rotation.m[1][1] == 1.0f && racine->rotation.m[2][2] == 1.0f);
		NifGeometry *forme = (NifGeometry*)f.blocks[5].data;
		VERIF(f.blocks[5].kind == NIF_TRISHAPE && forme && forme->name == 3 && forme->data == 11 && forme->skin == -1);
		VERIF(forme->numProperties == 3 && forme->properties[0] == 10 && forme->properties[2] == 6 && forme->activeMaterial == -1);
		NifGeometryData *d = (NifGeometryData*)f.blocks[11].data;
		VERIF(f.blocks[11].kind == NIF_TRISHAPEDATA && d && d->numVertices == 50 && d->vertices && d->normals && d->colors);
		printf("  forme 0 : %d sommets, %d triangles, %d jeux d'UV, rayon %.3f\n", d->numVertices, d->numTriangles, d->numUVSets, d->radius);
		VERIF(d->numUVSets == 1 && d->uv && d->numTriangles == 28 && d->triangles);
		for(int32 i = 0; i < d->numTriangles; i++) for(int k = 0; k < 3; k++) VERIF(d->triangles[i][k] < d->numVertices);
		NifSourceTexture *t = (NifSourceTexture*)f.blocks[7].data;
		VERIF(t && t->external == 1 && strcmp(f.String(t->fileName), "numbers_d.tga") == 0 && t->pixelData == -1);
		NifMaterialProperty *m = (NifMaterialProperty*)f.blocks[10].data;
		VERIF(m && strcmp(f.String(m->name), "numbers") == 0 && fabsf(m->ambient[0] - 0.588f) < 1e-3f && m->diffuse[0] == 1.0f && m->glossiness == 10.0f && m->alpha == 1.0f);
		NifTexturingProperty *tp = (NifTexturingProperty*)f.blocks[6].data;
		VERIF(tp && tp->baseTexture == 7);
		f.Free(); free(b);
	}

	// tous les .nif : en-tête lu, blocs connus décodés
	int32 fichiers = 0, entetes = 0, grandBoutistes = 0; int64_t blocs = 0, connus = 0, decodes = 0;
	struct { char type[40]; int32 n; } rates[16]; int32 numRates = 0;
	for(int32 i = 0; i < CdStream::ms_images[0].m_numEntries; i++){
		const char *n = CdStream::ms_images[0].m_entries[i].name;
		size_t l = strnlen(n, CDSTREAM_NAME_LEN);
		if(l < 4 || strncasecmp(n + l - 4, ".nif", 4) != 0) continue;
		b = Lire(n, &bytes); if(b == nil) continue;
		fichiers++;
		CNifFile f;
		if(f.Load(b, bytes)){
			entetes++; blocs += f.numBlocks; decodes += f.numDecoded; if(f.endian == 0) grandBoutistes++;
			for(int32 k = 0; k < f.numBlocks; k++){
				const char *tn = f.types[f.blocks[k].typeIndex];
				if(CNifFile::KindOf(tn) == NIF_INCONNU) continue;
				connus++;
				if(f.blocks[k].kind == NIF_INCONNU){           // connu mais mal décodé
					int32 j; for(j = 0; j < numRates; j++) if(strcmp(rates[j].type, tn) == 0) break;
					if(j == numRates && numRates < 16){ strncpy(rates[j].type, tn, 39); rates[j].n = 0; numRates++; }
					if(j < numRates) rates[j].n++;
					if(rates[j].n <= 2) printf("  raté : %s bloc %d de %s (%u o)\n", tn, k, n, f.blocks[k].size);
				}
			}
		}
		f.Free(); free(b);
	}
	printf("%d fichiers .nif (%d grand-boutistes), %d en-têtes lus, %lld blocs dont %lld de types connus, %lld décodés à l'octet près\n", fichiers, grandBoutistes, entetes, (long long)blocs, (long long)connus, (long long)decodes);
	for(int32 j = 0; j < numRates; j++) printf("  mal décodés : %s ×%d\n", rates[j].type, rates[j].n);
	VERIF(fichiers == 5724 && entetes == 5724 && decodes == connus);
	printf(echecs ? "%d échec(s)\n" : "tout passe\n", echecs);
	return echecs != 0;
}
