// Une scène à rendre : des modèles placés, leurs textures, et le rendu.
#pragma once
#include "commun.h"
#include "../src/gamebryo/NifTransform.h"
#include <cmath>

namespace outil {

struct Scene {
	Archives *arch = nil;
	std::map<std::string, Dictionnaire*> dicos;          // txd → textures décodées
	std::vector<CVector> pts; std::vector<float> uv;
	std::vector<int32> tri, triTex;                      // par triangle : index dans texNoms, -1
	std::vector<std::string> texNoms; std::vector<const RasterTexture*> texPtr;
	int modeles = 0, manquants = 0;

	~Scene(){ for(auto &d : dicos) delete d.second; }

	Dictionnaire *Dico(const std::string &txd){
		std::string k = Minuscules(txd.c_str());
		auto it = dicos.find(k); if(it != dicos.end()) return it->second;
		Dictionnaire *d = new Dictionnaire;
		if(!ChargerDictionnaire(*arch, txd, *d, true)){ delete d; d = nil; }
		dicos[k] = d; return d;
	}
	int32 IndexTexture(Dictionnaire *d, const std::string &nom){
		if(d == nil || nom.empty()) return -1;
		auto t = d->textures.find(nom); if(t == d->textures.end() || t->second.rgba.empty()) return -1;
		std::string cle = nom + "@" + std::to_string((uintptr_t)d);
		for(size_t k = 0; k < texNoms.size(); k++) if(texNoms[k] == cle) return (int32)k;
		texNoms.push_back(cle); texPtr.push_back(&t->second.rt); return (int32)texNoms.size() - 1;
	}
	struct Ctx { Scene *s; Dictionnaire *d; NifTransform place; };
	static void Forme(const CNifFile &f, int32, const NifGeometry &g, const NifGeometryData &d, const NifTransform &t, void *ctx){
		Ctx &c = *(Ctx*)ctx; Scene &s = *c.s;
		if(!d.vertices || !d.triangles) return;
		bool avecUv = d.uv && d.numUVSets > 0;
		int32 tex = avecUv ? s.IndexTexture(c.d, TextureDeBase(f, g)) : -1;
		int32 base = (int32)s.pts.size();
		for(int i = 0; i < d.numVertices; i++){
			s.pts.push_back(NifApply(c.place, NifApply(t, d.vertices[i])));
			s.uv.push_back(avecUv ? d.uv[i][0] : 0); s.uv.push_back(avecUv ? d.uv[i][1] : 0);
		}
		for(int i = 0; i < d.numTriangles; i++){
			s.tri.push_back(base + d.triangles[i][0]); s.tri.push_back(base + d.triangles[i][1]); s.tri.push_back(base + d.triangles[i][2]);
			s.triTex.push_back(tex);
		}
	}
	// Ajoute un modèle placé par `place` (transformation du modèle vers le monde).
	bool AjouterModele(Archives &a, const std::string &modele, const NifTransform &place){
		arch = &a;
		uint32 nb; uint8 *buf = LireMonde(a, modele + ".nif", &nb);
		if(buf == nil){ manquants++; return false; }
		CNifFile nif;
		if(!nif.Load(buf, nb)){ free(buf); manquants++; return false; }
		size_t avant = tri.size();
		Ctx c{this, Dico(TxdDe(a, modele)), place};
		NifWalkShapes(nif, Forme, &c);
		nif.Free(); free(buf);
		modeles++;
		return tri.size() > avant;
	}
	// `coupe` : fraction de la hauteur au-dessus de laquelle les triangles ne
	// sont pas dessinés (1 = tout dessiner), pour regarder dans une pièce.
	bool Rendre(const std::string &sortie, float azim, float elev, int32 taille, float coupe = 1.0f){
		if(tri.empty()) return false;
		CVector mn = pts[0], mx = pts[0];
		for(auto &p : pts){ mn.x = fminf(mn.x, p.x); mn.y = fminf(mn.y, p.y); mn.z = fminf(mn.z, p.z); mx.x = fmaxf(mx.x, p.x); mx.y = fmaxf(mx.y, p.y); mx.z = fmaxf(mx.z, p.z); }
		float zCoupe = mn.z + (mx.z - mn.z) * coupe;
		CVector c((mn.x+mx.x)/2, (mn.y+mx.y)/2, (mn.z+mx.z)/2);
		float R = fmaxf(fmaxf(mx.x-mn.x, mx.y-mn.y), mx.z-mn.z) / 2; if(R < 1e-3f) R = 1;
		float a = azim * 3.14159265f / 180, e = elev * 3.14159265f / 180, k = taille * 0.46f / R;
		std::vector<RasterVertex> ecran(pts.size());
		for(size_t i = 0; i < pts.size(); i++){
			float x = pts[i].x - c.x, y = pts[i].y - c.y, z = pts[i].z - c.z;
			float x1 = x*cosf(a) - y*sinf(a), y1 = x*sinf(a) + y*cosf(a);
			float y2 = y1*cosf(e) - z*sinf(e), z2 = y1*sinf(e) + z*cosf(e);
			ecran[i].x = taille/2.0f + x1*k; ecran[i].y = taille/2.0f - z2*k; ecran[i].z = y2;
			ecran[i].u = uv[i*2]; ecran[i].v = uv[i*2+1];
		}
		RasterImage img = RasterCreate(taille, taille, 52, 56, 60);
		float lum[3] = {0.4f, -0.6f, 0.7f}; float ln = sqrtf(lum[0]*lum[0]+lum[1]*lum[1]+lum[2]*lum[2]); for(float &q : lum) q /= ln;
		for(size_t i = 0; i < tri.size(); i += 3){
			const CVector &A = pts[tri[i]], &B = pts[tri[i+1]], &C = pts[tri[i+2]];
			if(fminf(fminf(A.z, B.z), C.z) > zCoupe) continue;
			float ux = B.x-A.x, uy = B.y-A.y, uz = B.z-A.z, wx = C.x-A.x, wy = C.y-A.y, wz = C.z-A.z;
			float nx = uy*wz-uz*wy, ny = uz*wx-ux*wz, nz = ux*wy-uy*wx, nn = sqrtf(nx*nx+ny*ny+nz*nz); if(nn < 1e-9f) nn = 1;
			float shade = 0.45f + 0.55f * fabsf((nx*lum[0]+ny*lum[1]+nz*lum[2])/nn);
			int32 tx = triTex[i/3];
			RasterVertex v[3] = { ecran[tri[i]], ecran[tri[i+1]], ecran[tri[i+2]] };
			RasterTriangle(img, v, tx >= 0 ? texPtr[tx] : nil, shade);
		}
		bool ok = RasterWritePPM(img, sortie.c_str());
		RasterFree(img);
		return ok;
	}
};

} // namespace outil
