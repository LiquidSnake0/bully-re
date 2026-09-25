// Sens des rotations d'un arbre NIF, tranché par les collisions.
//
// Chaque modèle de collision porte une boîte englobante dans l'espace du
// modèle. On compose l'arbre NIF sous les deux conventions possibles,
// v' = R·v (celle de NifTransform) et v' = Rᵀ·v, et on compare les
// dimensions de la boîte des sommets à celles de la collision. Sur les
// modèles à pièces tournées, une seule convention colle.
//   BULLY_DATA=<racine du jeu> build/tests/test_transform
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/core/IdeBinary.h"
#include "../src/gamebryo/NifFile.h"
#include "../src/gamebryo/NifTransform.h"
#include "../src/collision/ColModel.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <map>
#include <string>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC ligne %d : %s\n", __LINE__, #cond); echecs++; } }while(0)

static std::string Min(const char *s){ std::string r(s); for(char &c : r) c = (char)tolower((unsigned char)c); return r; }
static std::map<int32, std::string> g_idNom;
static std::map<std::string, CBox> g_col;
template<class E> static int32 Ret(const E &e){ g_idNom[e.id] = Min(e.model); return 0; }
void RegisterModelRange(uint16, uint32) {}
static void Garder(int32 id, const char *, CColModel *m, uint8){ auto it = g_idNom.find(id); if(it != g_idNom.end()) g_col[it->second] = m->boundingBox; m->RemoveCollisionVolumes(); free(m); }

static uint8 *
Lire(int32 image, const char *imgPath, const CDirectoryEntry *d, uint32 *bytes)
{
	*bytes = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *b = (uint8*)malloc(*bytes);
	int32 fd = CFileMgr::OpenFile(imgPath, "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, b, *bytes);
	CFileMgr::CloseFile(fd);
	if(!ok){ free(b); return nil; }
	return b;
}

struct Boite { CVector mn, mx; int n; };
static void Ajouter(Boite &b, const CVector &v){
	if(b.n == 0){ b.mn = b.mx = v; }
	b.mn.x = fminf(b.mn.x, v.x); b.mn.y = fminf(b.mn.y, v.y); b.mn.z = fminf(b.mn.z, v.z);
	b.mx.x = fmaxf(b.mx.x, v.x); b.mx.y = fmaxf(b.mx.y, v.y); b.mx.z = fmaxf(b.mx.z, v.z);
	b.n++;
}
// La convention du module : R·v.
static void FormeR(const CNifFile &, int32, const NifGeometry &, const NifGeometryData &d, const NifTransform &t, void *ctx){
	Boite &b = *(Boite*)ctx; if(!d.vertices) return;
	for(int i = 0; i < d.numVertices; i++) Ajouter(b, NifApply(t, d.vertices[i]));
}
// L'autre : Rᵀ·v, recomposée à la main.
struct T { float r[3][3]; float s; CVector t; };
static T CompT(const T &p, const NifAVObject &o){
	T c; for(int i = 0; i < 3; i++) for(int j = 0; j < 3; j++){ c.r[i][j] = 0; for(int k = 0; k < 3; k++) c.r[i][j] += p.r[i][k] * o.rotation.m[j][k]; }
	c.s = p.s * o.scale; CVector tt = NifRotate(p.r, CVector(o.translation.x*p.s, o.translation.y*p.s, o.translation.z*p.s));
	c.t = CVector(p.t.x+tt.x, p.t.y+tt.y, p.t.z+tt.z); return c;
}
static void ParcT(const CNifFile &f, int32 bloc, const T &p, int prof, Boite &b){
	if(bloc < 0 || bloc >= f.numBlocks || prof > 64 || !f.blocks[bloc].data) return;
	const NifBlock &bl = f.blocks[bloc];
	if(bl.kind == NIF_NODE){ const NifNode *n = (const NifNode*)bl.data; T t = CompT(p, *n); for(int32 i = 0; i < n->numChildren; i++) ParcT(f, n->children[i], t, prof+1, b); return; }
	if(bl.kind != NIF_TRISHAPE && bl.kind != NIF_TRISTRIPS) return;
	const NifGeometry *g = (const NifGeometry*)bl.data; if(g->data < 0 || g->data >= f.numBlocks || !f.blocks[g->data].data) return;
	const NifGeometryData *d = (const NifGeometryData*)f.blocks[g->data].data; if(!d->vertices) return;
	T t = CompT(p, *g);
	for(int i = 0; i < d->numVertices; i++){ CVector v = NifRotate(t.r, CVector(d->vertices[i].x*t.s, d->vertices[i].y*t.s, d->vertices[i].z*t.s)); Ajouter(b, CVector(v.x+t.t.x, v.y+t.t.y, v.z+t.t.z)); }
}
static float Ecart(const Boite &a, const CBox &c){ return fabsf((a.mx.x-a.mn.x)-(c.max.x-c.min.x)) + fabsf((a.mx.y-a.mn.y)-(c.max.y-c.min.y)) + fabsf((a.mx.z-a.mn.z)-(c.max.z-c.min.z)); }
static float Taille(const CBox &c){ return (c.max.x-c.min.x)+(c.max.y-c.min.y)+(c.max.z-c.min.z); }

int
main(void)
{
	// --- Composition élémentaire : rotation de 90° autour de Z --------
	{
		NifAVObject o = {};
		o.scale = 2.0f; o.translation = CVector(10, 0, 0);
		// R envoie x sur y : ligne 0 = (0,-1,0), ligne 1 = (1,0,0), ligne 2 = (0,0,1)
		o.rotation.m[0][0] = 0; o.rotation.m[0][1] = -1; o.rotation.m[0][2] = 0;
		o.rotation.m[1][0] = 1; o.rotation.m[1][1] = 0;  o.rotation.m[1][2] = 0;
		o.rotation.m[2][0] = 0; o.rotation.m[2][1] = 0;  o.rotation.m[2][2] = 1;
		NifTransform t = NifCompose(NifIdentity(), o);
		CVector v = NifApply(t, CVector(1, 0, 0));        // (1,0,0)·2 → tourné → (0,2,0) → +t
		VERIF(fabsf(v.x - 10) < 1e-5f && fabsf(v.y - 2) < 1e-5f && fabsf(v.z) < 1e-5f);
		// Composer deux fois : rotation 180°, échelle 4, translation
		// (10,0,0) + R·(2·(10,0,0)) = (10,20,0). Le point (1,0,0) donne
		// R²·(4,0,0) + (10,20,0) = (6,20,0).
		NifTransform t2 = NifCompose(t, o);
		CVector w = NifApply(t2, CVector(1, 0, 0));
		VERIF(fabsf(w.x - 6) < 1e-4f && fabsf(w.y - 20) < 1e-4f && fabsf(w.z) < 1e-4f);
	}

	// --- Contre les collisions --------------------------------------------
	int32 monde = CdStream::AddImage("Stream\\World.img");
	int32 ide = CdStream::AddImage("Objects\\ide.img");
	VERIF(monde == 0 && ide == 1);
	CIdeBinary::ms_objHandler = Ret<CObjIdeEntry>; CIdeBinary::ms_pedHandler = Ret<CPedIdeEntry>;
	CIdeBinary::ms_carHandler = Ret<CCarIdeEntry>; CIdeBinary::ms_weapHandler = Ret<CWeapIdeEntry>;
	CIdeBinary::ms_panmHandler = Ret<CPanmIdeEntry>; CIdeBinary::ms_simpleHandler = Ret<CSimpleIdeEntry>;
	{
		const CdImage &img = CdStream::ms_images[ide];
		for(int32 k = 0; k < img.m_numEntries; k++){
			const CDirectoryEntry *d = &img.m_entries[k]; size_t L = strlen(d->name);
			if(L < 4 || strcasecmp(d->name + L - 4, ".idb") != 0) continue;
			uint32 by; uint8 *b = Lire(ide, "Objects\\ide.img", d, &by); if(!b) continue;
			uint32 u; memcpy(&u, b, 4); if(u + 4 <= by) CIdeBinary::Load(b + 4, u); free(b);
		}
	}
	CColLoader::ms_handler = Garder;
	const CdImage &img = CdStream::ms_images[monde];
	for(int32 k = 0; k < img.m_numEntries; k++){
		const CDirectoryEntry *d = &img.m_entries[k]; size_t L = strlen(d->name);
		if(L < 4 || strcasecmp(d->name + L - 4, ".col") != 0) continue;
		uint32 n; uint8 *b = Lire(monde, "Stream\\World.img", d, &n); if(b){ CColLoader::LoadCollisionFile(b, n, 0); free(b); }
	}
	printf("%zu identifiants nommés, %zu collisions reliées à un modèle\n", g_idNom.size(), g_col.size());

	int apparies = 0, discriminants = 0, exactR = 0, exactT = 0; float catwalkR = -1, catwalkT = -1;
	for(int32 k = 0; k < img.m_numEntries; k++){
		const CDirectoryEntry *d = &img.m_entries[k]; size_t L = strlen(d->name);
		if(L < 4 || strcasecmp(d->name + L - 4, ".nif") != 0) continue;
		std::string nom = Min(std::string(d->name, L - 4).c_str());
		auto it = g_col.find(nom); if(it == g_col.end()) continue;
		uint32 n; uint8 *b = Lire(monde, "Stream\\World.img", d, &n); if(!b) continue;
		CNifFile f; if(!f.Load(b, n)){ free(b); continue; }
		Boite bR = {CVector(), CVector(), 0}, bT = {CVector(), CVector(), 0};
		NifWalkShapes(f, FormeR, &bR);
		T id; for(int i = 0; i < 3; i++) for(int j = 0; j < 3; j++) id.r[i][j] = i == j; id.s = 1; id.t = CVector(0,0,0);
		ParcT(f, 0, id, 0, bT);
		f.Free(); free(b);
		if(bR.n == 0) continue;
		apparies++;
		float eR = Ecart(bR, it->second), eT = Ecart(bT, it->second), taille = Taille(it->second);
		if(nom == "catwalk"){ catwalkR = eR; catwalkT = eT; }
		if(fabsf(eR - eT) < 0.05f * taille) continue;      // pièces non tournées : ne départage rien
		discriminants++;
		float seuil = 0.01f * taille;
		if(eR < seuil && eT >= seuil) exactR++;
		if(eT < seuil && eR >= seuil) exactT++;
	}
	printf("%d modèles appariés, %d discriminants ; quasi exacts (< 1 %%) pour R·v : %d, pour Rᵀ·v : %d\n", apparies, discriminants, exactR, exactT);
	printf("catwalk : écart R·v %.4f, Rᵀ·v %.4f\n", catwalkR, catwalkT);
	VERIF(apparies > 3000);
	VERIF(exactR >= 10);
	VERIF(exactT == 0);
	VERIF(catwalkR >= 0 && catwalkR < 0.01f && catwalkT > 10.0f);

	printf(echecs ? "\n%d verification(s) en echec\n" : "\ntout passe\n", echecs);
	return echecs != 0;
}
