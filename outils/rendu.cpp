// Rend un modèle du jeu en image, avec le rasteriseur logiciel de src/render.
//   BULLY_DATA=<racine du jeu> build/outils/rendu <modèle> [sortie.ppm] [azimut] [élévation] [taille]
// Caméra orthographique en orbite autour de la boîte du modèle, Z vers le
// haut. Même chaîne que nif2obj pour les textures.
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/core/IdeBinary.h"
#include "../src/gamebryo/NifFile.h"
#include "../src/gamebryo/NifTransform.h"
#include "../src/gamebryo/TextureDecode.h"
#include "../src/render/SoftRaster.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <map>
#include <string>
#include <vector>

static std::map<std::string, std::string> g_txdDe;
static std::string Minuscules(const char *s){ std::string r(s); for(char &c : r) c = (char)tolower((unsigned char)c); return r; }
static std::string BaseNom(const char *c){ std::string s = Minuscules(c); size_t p = s.find_last_of("\\/"); if(p != std::string::npos) s = s.substr(p+1); size_t d = s.rfind('.'); if(d != std::string::npos) s = s.substr(0, d); return s; }
template<class E> static int32 Retenir(const E &e){ g_txdDe[Minuscules(e.model)] = e.txd; return 0; }
void RegisterModelRange(uint16, uint32) {}

static uint8 *
LireEntree(int32 image, const char *imgPath, const char *nom, uint32 *bytes)
{
	const CDirectoryEntry *d = CdStream::ms_images[image].Find(nom);
	if(d == nil) return nil;
	*bytes = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(*bytes);
	int32 fd = CFileMgr::OpenFile(imgPath, "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, *bytes);
	CFileMgr::CloseFile(fd);
	if(!ok){ free(buf); return nil; }
	return buf;
}

struct Tex { std::vector<uint8> rgba; RasterTexture rt; };
struct Scene {
	const CNifFile *nif;
	std::map<std::string, Tex> textures;
	std::vector<CVector> pts; std::vector<float> uv; std::vector<int32> tri; std::vector<int32> triTex; // par triangle : index de texture, -1
	std::vector<std::string> texNoms;
};

static int32
TextureDe(Scene &s, const NifGeometry &g)
{
	const CNifFile &f = *s.nif;
	for(int32 i = 0; i < g.numProperties; i++){
		int32 b = g.properties[i];
		if(b < 0 || b >= f.numBlocks || f.blocks[b].kind != NIF_TEXTURINGPROPERTY || !f.blocks[b].data) continue;
		int32 st = ((const NifTexturingProperty*)f.blocks[b].data)->baseTexture;
		if(st < 0 || st >= f.numBlocks || f.blocks[st].kind != NIF_SOURCETEXTURE || !f.blocks[st].data) continue;
		std::string nom = BaseNom(f.String(((const NifSourceTexture*)f.blocks[st].data)->fileName));
		if(s.textures.find(nom) == s.textures.end()) return -1;
		for(size_t k = 0; k < s.texNoms.size(); k++) if(s.texNoms[k] == nom) return (int32)k;
		s.texNoms.push_back(nom); return (int32)s.texNoms.size() - 1;
	}
	return -1;
}

static void
Forme(const CNifFile &, int32, const NifGeometry &g, const NifGeometryData &d, const NifTransform &t, void *ctx)
{
	Scene &s = *(Scene*)ctx;
	if(!d.vertices || !d.triangles) return;
	int32 tex = TextureDe(s, g);
	int32 base = (int32)s.pts.size();
	bool avecUv = d.uv && d.numUVSets > 0;
	for(int i = 0; i < d.numVertices; i++){
		s.pts.push_back(NifApply(t, d.vertices[i]));
		s.uv.push_back(avecUv ? d.uv[i][0] : 0); s.uv.push_back(avecUv ? d.uv[i][1] : 0);
	}
	for(int i = 0; i < d.numTriangles; i++){
		s.tri.push_back(base + d.triangles[i][0]); s.tri.push_back(base + d.triangles[i][1]); s.tri.push_back(base + d.triangles[i][2]);
		s.triTex.push_back(avecUv ? tex : -1);
	}
}

int
main(int argc, char **argv)
{
	if(argc < 2){ fprintf(stderr, "usage : rendu <modèle> [sortie.ppm] [azimut] [élévation] [taille]\n"); return 2; }
	std::string modele = argv[1];
	std::string sortie = argc > 2 ? argv[2] : modele + ".ppm";
	float azim = argc > 3 ? (float)atof(argv[3]) : 35.0f, elev = argc > 4 ? (float)atof(argv[4]) : 25.0f;
	int32 taille = argc > 5 ? atoi(argv[5]) : 800;

	int32 monde = CdStream::AddImage("Stream\\World.img");
	int32 ide = CdStream::AddImage("Objects\\ide.img");
	if(monde < 0 || ide < 0){ fprintf(stderr, "archives introuvables, BULLY_DATA ?\n"); return 1; }
	CIdeBinary::ms_objHandler = Retenir<CObjIdeEntry>; CIdeBinary::ms_pedHandler = Retenir<CPedIdeEntry>;
	CIdeBinary::ms_carHandler = Retenir<CCarIdeEntry>; CIdeBinary::ms_weapHandler = Retenir<CWeapIdeEntry>;
	CIdeBinary::ms_panmHandler = Retenir<CPanmIdeEntry>; CIdeBinary::ms_simpleHandler = Retenir<CSimpleIdeEntry>;
	{
		const CdImage &img = CdStream::ms_images[ide];
		for(int32 k = 0; k < img.m_numEntries; k++){
			const char *n = img.m_entries[k].name; size_t L = strlen(n);
			if(L < 4 || strcasecmp(n + L - 4, ".idb") != 0) continue;
			uint32 bytes; uint8 *buf = LireEntree(ide, "Objects\\ide.img", n, &bytes);
			if(!buf) continue;
			uint32 utile; memcpy(&utile, buf, 4);
			if(utile + 4 <= bytes) CIdeBinary::Load(buf + 4, utile);
			free(buf);
		}
	}
	auto it = g_txdDe.find(Minuscules(modele.c_str()));
	std::string txd = it != g_txdDe.end() ? it->second : modele;

	uint32 nb, tb = 0;
	uint8 *nifBuf = LireEntree(monde, "Stream\\World.img", (modele + ".nif").c_str(), &nb);
	if(!nifBuf){ fprintf(stderr, "%s.nif absent\n", modele.c_str()); return 1; }
	CNifFile nif, nft;
	if(!nif.Load(nifBuf, nb)){ fprintf(stderr, "%s.nif illisible\n", modele.c_str()); return 1; }
	Scene s; s.nif = &nif;
	uint8 *nftBuf = LireEntree(monde, "Stream\\World.img", (txd + ".nft").c_str(), &tb);
	if(nftBuf && nft.Load(nftBuf, tb))
		for(int32 i = 0; i < nft.numBlocks; i++){
			if(nft.blocks[i].kind != NIF_SOURCETEXTURE || !nft.blocks[i].data) continue;
			const NifSourceTexture *st = (const NifSourceTexture*)nft.blocks[i].data;
			int32 p = st->pixelData;
			if(p < 0 || p >= nft.numBlocks || nft.blocks[p].kind != NIF_PIXELDATA || !nft.blocks[p].data) continue;
			const NifPixelData *px = (const NifPixelData*)nft.blocks[p].data;
			const NifPalette *pal = nil;
			if(px->palette >= 0 && px->palette < nft.numBlocks && nft.blocks[px->palette].kind == NIF_PALETTE) pal = (const NifPalette*)nft.blocks[px->palette].data;
			uint32 w, h; if(!MipSize(*px, 0, &w, &h)) continue;
			Tex &t = s.textures[BaseNom(nft.String(st->fileName))];
			t.rgba.resize((size_t)w * h * 4);
			if(!DecodeNifPixels(*px, pal, 0, t.rgba.data())){ t.rgba.clear(); continue; }
			t.rt.w = w; t.rt.h = h; t.rt.rgba = t.rgba.data();
		}
	NifWalkShapes(nif, Forme, &s);
	if(s.tri.empty()){ fprintf(stderr, "aucune géométrie\n"); return 1; }

	// boîte et caméra
	CVector mn = s.pts[0], mx = s.pts[0];
	for(auto &p : s.pts){ mn.x = fminf(mn.x, p.x); mn.y = fminf(mn.y, p.y); mn.z = fminf(mn.z, p.z); mx.x = fmaxf(mx.x, p.x); mx.y = fmaxf(mx.y, p.y); mx.z = fmaxf(mx.z, p.z); }
	CVector c((mn.x+mx.x)/2, (mn.y+mx.y)/2, (mn.z+mx.z)/2);
	float R = fmaxf(fmaxf(mx.x-mn.x, mx.y-mn.y), mx.z-mn.z) / 2; if(R < 1e-3f) R = 1;
	float a = azim * 3.14159265f / 180, e = elev * 3.14159265f / 180, k = taille * 0.42f / R;
	std::vector<RasterVertex> ecran(s.pts.size());
	for(size_t i = 0; i < s.pts.size(); i++){
		float x = s.pts[i].x - c.x, y = s.pts[i].y - c.y, z = s.pts[i].z - c.z;
		float x1 = x*cosf(a) - y*sinf(a), y1 = x*sinf(a) + y*cosf(a);
		float y2 = y1*cosf(e) - z*sinf(e), z2 = y1*sinf(e) + z*cosf(e);
		ecran[i].x = taille/2.0f + x1*k; ecran[i].y = taille/2.0f - z2*k; ecran[i].z = y2;   // y2 = profondeur
		ecran[i].u = s.uv[i*2]; ecran[i].v = s.uv[i*2+1];
	}
	RasterImage img = RasterCreate(taille, taille, 52, 56, 60);
	float lum[3] = {0.4f, -0.6f, 0.7f}; float ln = sqrtf(lum[0]*lum[0]+lum[1]*lum[1]+lum[2]*lum[2]); for(float &q : lum) q /= ln;
	for(size_t i = 0; i < s.tri.size(); i += 3){
		const CVector &A = s.pts[s.tri[i]], &B = s.pts[s.tri[i+1]], &C = s.pts[s.tri[i+2]];
		float ux = B.x-A.x, uy = B.y-A.y, uz = B.z-A.z, wx = C.x-A.x, wy = C.y-A.y, wz = C.z-A.z;
		float nx = uy*wz-uz*wy, ny = uz*wx-ux*wz, nz = ux*wy-uy*wx, nn = sqrtf(nx*nx+ny*ny+nz*nz); if(nn < 1e-9f) nn = 1;
		float shade = 0.45f + 0.55f * fabsf((nx*lum[0]+ny*lum[1]+nz*lum[2])/nn);
		int32 tx = s.triTex[i/3];
		const RasterTexture *rt = tx >= 0 ? &s.textures[s.texNoms[tx]].rt : nil;
		RasterVertex v[3] = { ecran[s.tri[i]], ecran[s.tri[i+1]], ecran[s.tri[i+2]] };
		RasterTriangle(img, v, rt, shade);
	}
	bool ok = RasterWritePPM(img, sortie.c_str());
	printf("%s : %zu triangles, %zu textures → %s%s\n", modele.c_str(), s.tri.size()/3, s.texNoms.size(), sortie.c_str(), ok ? "" : " (échec d'écriture)");
	RasterFree(img); nif.Free(); nft.Free(); free(nifBuf); free(nftBuf);
	return ok ? 0 : 1;
}
