// Le squelette de Jimmy (PLAYER.nif, lu dans Stream/World.img) : les blocs de
// peau se décodent, le skinning dans la pose du fichier redonne les sommets
// stockés, et plier le haut du bras déplace la main sans toucher aux pieds.
//   BULLY_DATA=<racine du jeu> build/tests/test_skin
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/gamebryo/NifSkin.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>

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

static float
Distance(const CVector &a, const CVector &b){ return sqrtf((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y) + (a.z-b.z)*(a.z-b.z)); }

int
main(void)
{
	VERIF(CdStream::AddImage("Stream\\World.img") == 0);
	uint32 bytes; uint8 *b = Lire("PLAYER.nif", &bytes);
	VERIF(b != nil);
	if(b == nil){ printf("test_skin : ECHEC\n"); return 1; }
	CNifFile f;
	VERIF(f.Load(b, bytes));

	// 1. Quatre peaux, quatre données, tous les blocs lus à l'octet près.
	int instances = 0, donnees = 0, formes = 0;
	for(int32 i = 0; i < f.numBlocks; i++){
		if(f.blocks[i].kind == NIF_SKININSTANCE) instances++;
		if(f.blocks[i].kind == NIF_SKINDATA){ donnees++; VERIF(((NifSkinData*)f.blocks[i].data)->hasWeights); }
	}
	VERIF(instances == 4 && donnees == 4);
	VERIF(NifFindNode(f, "Root L UpperArm") >= 0 && NifFindNode(f, "Root R Hand") >= 0);

	// 2. Pose du fichier : chaque sommet pondéré retombe sur sa place stockée.
	std::vector<NifTransform> mondes(f.numBlocks);
	NifWorldTransforms(f, mondes.data());
	float pire = 0;
	for(int32 i = 0; i < f.numBlocks; i++){
		if(f.blocks[i].kind != NIF_TRISHAPE && f.blocks[i].kind != NIF_TRISTRIPS) continue;
		const NifGeometry &g = *(const NifGeometry*)f.blocks[i].data;
		if(g.skin < 0) continue;
		const NifGeometryData &d = *(const NifGeometryData*)f.blocks[g.data].data;
		std::vector<CVector> v(d.numVertices);
		VERIF(NifSkinVertices(f, i, mondes.data(), v.data()));
		for(int32 k = 0; k < d.numVertices; k++) pire = fmaxf(pire, Distance(v[k], NifApply(mondes[i], d.vertices[k])));
		formes++;
	}
	VERIF(formes == 4);
	VERIF(pire < 0.005f);
	printf("  pose du fichier : %d formes, écart max %.2f mm\n", formes, pire * 1000);

	// 3. Le haut du bras gauche tourne de 36° autour de son y local : la main
	//    descend le long du corps, les pieds ne bougent pas.
	std::vector<NifMatrix33> pose(f.numBlocks, NifAxisRotation(0, 0));
	int32 ua = NifFindNode(f, "Root L UpperArm"), main_ = NifFindNode(f, "Root L Hand"), pied = NifFindNode(f, "Root L Foot");
	CVector mainAvant = mondes[main_].t, piedAvant = mondes[pied].t;
	pose[ua] = NifAxisRotation(1, 36 * 3.14159265f / 180);
	NifWorldTransforms(f, mondes.data(), pose.data());
	VERIF(mondes[main_].t.z < mainAvant.z - 0.1f);
	VERIF(fabsf(mondes[main_].t.x - mondes[ua].t.x) < 0.1f);
	VERIF(Distance(mondes[pied].t, piedAvant) < 1e-5f);
	printf("  bras gauche plié : main %.2f → %.2f m de haut, pied immobile\n", mainAvant.z, mondes[main_].t.z);

	// 4. Une rotation d'axe reste orthonormée.
	NifMatrix33 r = NifAxisRotation(2, 0.7f);
	float det = r.m[0][0]*(r.m[1][1]*r.m[2][2]-r.m[1][2]*r.m[2][1]) - r.m[0][1]*(r.m[1][0]*r.m[2][2]-r.m[1][2]*r.m[2][0]) + r.m[0][2]*(r.m[1][0]*r.m[2][1]-r.m[1][1]*r.m[2][0]);
	VERIF(fabsf(det - 1) < 1e-5f);

	f.Free(); free(b);
	printf("test_skin : %s\n", echecs ? "ECHEC" : "ok");
	return echecs ? 1 : 0;
}
