// Une scène à rendre : des modèles placés, leurs textures, et le rendu.
#pragma once
#include "commun.h"
#include "../src/gamebryo/NifTransform.h"
#include "../src/gamebryo/NifSkin.h"
#include "../src/anim/Agr.h"
#include "../src/core/IplFile.h"
#include "../src/collision/ColModel.h"
#include "../src/collision/Marche.h"
#include <strings.h>
#include <cmath>

namespace outil {

// Une pose de démonstration, pas une animation du jeu : les bras le long du
// corps au lieu de la pose de repos, bras écartés. Le haut du bras tourne
// autour de son axe y local (vers le bas : + à gauche, − à droite, relevé
// sur cinq piétons), de l'angle qui amène la main 5 cm à l'extérieur de
// l'épaule : il dépend de la carrure (36° pour Jimmy, 51° pour Kirby).
inline void PoseBrasBaisses(const CNifFile &f, NifMatrix33 *pose){
	std::vector<NifTransform> m(f.numBlocks);
	for(int cote = 0; cote < 2; cote++){
		int32 ua = NifFindNode(f, cote ? "Root R UpperArm" : "Root L UpperArm");
		int32 main_ = NifFindNode(f, cote ? "Root R Hand" : "Root L Hand");
		if(ua < 0 || main_ < 0) continue;
		float sens = cote ? -1.0f : 1.0f, meilleur = 0, ecart = 1e9f;
		for(int k = 0; k <= 90; k++){
			float a = sens * k * 3.14159265f / 180;
			pose[ua] = NifAxisRotation(1, a);
			NifWorldTransforms(f, m.data(), pose);
			float dehors = (m[main_].t.x - m[ua].t.x) * sens;         // vers l'extérieur du corps
			float e = fabsf(dehors - 0.05f) + (m[main_].t.z > m[ua].t.z ? 10 : 0);
			if(e < ecart){ ecart = e; meilleur = a; }
		}
		pose[ua] = NifAxisRotation(1, meilleur);
	}
}

// Un modèle animé gardé en mémoire après son ajout : son NIF, sa place, la
// plage de ses sommets dans la scène, et les 36 nœuds que les pistes des .agr
// animent (l'ordre des nœuds sous « Dummy », docs/agr.md).
struct Anime {
	CNifFile nif{}; uint8 *buf = nil;
	NifTransform place;
	int32 ptDebut = 0, ptFin = 0, bloc = -1;
	int32 noeuds[AGR_OS];
	~Anime(){ nif.Free(); free(buf); }
};

struct Scene {
	Archives *arch = nil;
	std::map<std::string, Dictionnaire*> dicos;          // txd → textures décodées
	std::vector<CVector> pts; std::vector<float> uv;
	std::vector<int32> tri, triTex;                      // par triangle : index dans texNoms, -1
	std::vector<uint8> triMode, triSeuil;                // par triangle : eRasterMode et seuil du test alpha
	std::vector<uint8> triDeuxFaces;                     // par triangle : 1 si la forme se dessine des deux côtés
	std::vector<std::string> texNoms; std::vector<const RasterTexture*> texPtr;
	int modeles = 0, manquants = 0;
	// Les piétons portent sous leur corps des formes d'aide sans texture (une
	// flèche au sol, « Editable Poly ») que le jeu ne montre pas.
	bool sansAidesNonTexturees = false;
	std::vector<CIplInst> placements;                    // gardés pour poser les collisions
	// Un bloc par modèle placé : ses plages de sommets et de triangles, et sa
	// sphère englobante. La visite rejette un modèle entier (hors du champ, trop
	// loin) avant de toucher à ses sommets.
	struct Bloc { int32 ptDebut, ptFin, triDebut, triFin; CVector centre; float rayon; };
	std::vector<Bloc> blocs;

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
	struct Ctx { Scene *s; Dictionnaire *d, *secours; NifTransform place; const NifTransform *mondes; int32 *ecrire; };
	static void Forme(const CNifFile &f, int32 bloc, const NifGeometry &g, const NifGeometryData &d, const NifTransform &t, void *ctx){
		Ctx &c = *(Ctx*)ctx; Scene &s = *c.s;
		if(!d.vertices || !d.triangles) return;
		bool avecUv = d.uv && d.numUVSets > 0;
		std::string nomTex = TextureDeBase(f, g);
		if(s.sansAidesNonTexturees && nomTex.empty()) return;
		if(c.ecrire){
			// Mise à jour d'un modèle animé : mêmes formes, mêmes sommets, dans le
			// même ordre ; seules les positions changent.
			std::vector<CVector> peau;
			if(c.mondes && g.skin >= 0){ peau.resize(d.numVertices); if(!NifSkinVertices(f, bloc, c.mondes, peau.data())) peau.clear(); }
			for(int i = 0; i < d.numVertices; i++)
				s.pts[(*c.ecrire)++] = NifApply(c.place, peau.empty() ? NifApply(t, d.vertices[i]) : peau[i]);
			return;
		}
		int32 tex = avecUv ? s.IndexTexture(c.d, nomTex) : -1;
		// Le dictionnaire de l'IDE peut être une variante (l'hiver, « _W ») qui
		// ne contient pas toutes les textures du modèle : on essaie alors celui
		// qui porte le nom du modèle.
		if(avecUv && tex < 0 && c.secours) tex = s.IndexTexture(c.secours, nomTex);
		uint8 mode = RASTER_OPAQUE, seuil = 128;
		ModeAlpha(f, g, &mode, &seuil);
		uint8 deux = DeuxFaces(f, g) ? 1 : 0;
		int32 base = (int32)s.pts.size();
		// Une forme à peau se déforme par ses os (src/gamebryo/NifSkin).
		std::vector<CVector> peau;
		if(c.mondes && g.skin >= 0){ peau.resize(d.numVertices); if(!NifSkinVertices(f, bloc, c.mondes, peau.data())) peau.clear(); }
		for(int i = 0; i < d.numVertices; i++){
			s.pts.push_back(NifApply(c.place, peau.empty() ? NifApply(t, d.vertices[i]) : peau[i]));
			s.uv.push_back(avecUv ? d.uv[i][0] : 0); s.uv.push_back(avecUv ? d.uv[i][1] : 0);
		}
		for(int i = 0; i < d.numTriangles; i++){
			s.tri.push_back(base + d.triangles[i][0]); s.tri.push_back(base + d.triangles[i][1]); s.tri.push_back(base + d.triangles[i][2]);
			s.triTex.push_back(tex); s.triMode.push_back(mode); s.triSeuil.push_back(seuil); s.triDeuxFaces.push_back(deux);
		}
	}
	// Ajoute un modèle placé par `place` (transformation du modèle vers le monde).
	// `brasBaisses` : les formes à peau prennent la pose de PoseBrasBaisses.
	// `garder` : le NIF reste en mémoire dans cet objet, pour Reposer ensuite.
	bool AjouterModele(Archives &a, const std::string &modele, const NifTransform &place, bool brasBaisses = false, Anime *garder = nil){
		arch = &a;
		uint32 nb; uint8 *buf = LireMonde(a, modele + ".nif", &nb);
		if(buf == nil){ manquants++; return false; }
		CNifFile nif;
		if(!nif.Load(buf, nb)){ free(buf); manquants++; return false; }
		size_t avant = tri.size(), ptAvant = pts.size();
		std::string txd = TxdDe(a, modele);
		// Le squelette, s'il y en a un : les nœuds dans leur pose du fichier (ou
		// la pose demandée), pour déformer les formes à peau.
		std::vector<NifTransform> mondes;
		bool peau = false;
		for(int32 i = 0; i < nif.numBlocks; i++)
			if((nif.blocks[i].kind == NIF_TRISHAPE || nif.blocks[i].kind == NIF_TRISTRIPS) && nif.blocks[i].data && ((const NifGeometry*)nif.blocks[i].data)->skin >= 0) peau = true;
		if(peau){
			std::vector<NifMatrix33> pose(nif.numBlocks, NifAxisRotation(0, 0));
			if(brasBaisses) PoseBrasBaisses(nif, pose.data());
			mondes.resize(nif.numBlocks);
			NifWorldTransforms(nif, mondes.data(), pose.data());
		}
		Ctx c{this, Dico(txd), Minuscules(txd.c_str()) != Minuscules(modele.c_str()) ? Dico(modele) : nil, place, mondes.empty() ? nil : mondes.data(), nil};
		NifWalkShapes(nif, Forme, &c);
		if(garder){
			garder->nif = nif; garder->buf = buf; garder->place = place;
			garder->ptDebut = (int32)ptAvant; garder->ptFin = (int32)pts.size();
			garder->bloc = tri.size() > avant ? (int32)blocs.size() : -1;
			for(int32 k = 0; k < AGR_OS; k++) garder->noeuds[k] = -1;
			// Les 36 nœuds animés : l'arbre sous « Dummy », dans l'ordre du fichier.
			int32 n = 0, dummy = NifFindNode(nif, "Dummy");
			std::vector<int32> pile; if(dummy >= 0) pile.push_back(dummy);
			while(!pile.empty() && n < AGR_OS){
				int32 b = pile.back(); pile.pop_back();
				if(b < 0 || b >= nif.numBlocks || nif.blocks[b].kind != NIF_NODE || !nif.blocks[b].data) continue;
				garder->noeuds[n++] = b;
				const NifNode *nd = (const NifNode*)nif.blocks[b].data;
				for(int32 i = nd->numChildren - 1; i >= 0; i--) pile.push_back(nd->children[i]);
			}
		}else{ nif.Free(); free(buf); }
		modeles++;
		if(tri.size() > avant){
			Bloc b{(int32)ptAvant, (int32)pts.size(), (int32)avant, (int32)tri.size(), CVector(0, 0, 0), 0};
			CVector mn = pts[ptAvant], mx = pts[ptAvant];
			for(size_t i = ptAvant; i < pts.size(); i++){ const CVector &p = pts[i]; mn.x = fminf(mn.x, p.x); mn.y = fminf(mn.y, p.y); mn.z = fminf(mn.z, p.z); mx.x = fmaxf(mx.x, p.x); mx.y = fmaxf(mx.y, p.y); mx.z = fmaxf(mx.z, p.z); }
			b.centre = CVector((mn.x+mx.x)/2, (mn.y+mx.y)/2, (mn.z+mx.z)/2);
			for(size_t i = ptAvant; i < pts.size(); i++){ float dx = pts[i].x-b.centre.x, dy = pts[i].y-b.centre.y, dz = pts[i].z-b.centre.z; b.rayon = fmaxf(b.rayon, dx*dx+dy*dy+dz*dz); }
			b.rayon = sqrtf(b.rayon);
			blocs.push_back(b);
		}
		return tri.size() > avant;
	}
	// Met un modèle animé dans la pose de `anim` à l'instant t (s) : chaque os
	// animé prend la rotation de sa piste à la place de la sienne (sa position
	// reste celle du fichier), puis la peau est recalculée en place.
	void Reposer(Anime &an, const AgrAnim &anim, float t){
		const CNifFile &f = an.nif;
		std::vector<NifMatrix33> pose(f.numBlocks, NifAxisRotation(0, 0));
		for(int32 k = 1; k < AGR_OS; k++){                // la piste 0 (Dummy) ne bouge pas le modèle
			int32 b = an.noeuds[k]; float q[4], m[3][3];
			if(b < 0 || !AgrRotation(anim, k, t, q)) continue;
			AgrMatrice(q, m);
			const NifMatrix33 &r = ((const NifAVObject*)f.blocks[b].data)->rotation;
			// pose = rᵀ · m : composée après la rotation du nœud, elle la remplace.
			for(int i = 0; i < 3; i++) for(int j = 0; j < 3; j++){
				float v = 0; for(int k2 = 0; k2 < 3; k2++) v += r.m[k2][i] * m[k2][j];
				pose[b].m[i][j] = v;
			}
		}
		std::vector<NifTransform> mondes(f.numBlocks);
		NifWorldTransforms(f, mondes.data(), pose.data());
		int32 ecrire = an.ptDebut;
		Ctx c{this, nil, nil, an.place, mondes.data(), &ecrire};
		NifWalkShapes(f, Forme, &c);
		if(an.bloc >= 0){
			Bloc &b = blocs[an.bloc];
			CVector mn = pts[b.ptDebut], mx = mn;
			for(int32 i = b.ptDebut; i < b.ptFin; i++){ const CVector &p = pts[i]; mn.x = fminf(mn.x, p.x); mn.y = fminf(mn.y, p.y); mn.z = fminf(mn.z, p.z); mx.x = fmaxf(mx.x, p.x); mx.y = fmaxf(mx.y, p.y); mx.z = fmaxf(mx.z, p.z); }
			b.centre = CVector((mn.x+mx.x)/2, (mn.y+mx.y)/2, (mn.z+mx.z)/2); b.rayon = 0;
			for(int32 i = b.ptDebut; i < b.ptFin; i++){ float dx = pts[i].x-b.centre.x, dy = pts[i].y-b.centre.y, dz = pts[i].z-b.centre.z; b.rayon = fmaxf(b.rayon, dx*dx+dy*dy+dz*dz); }
			b.rayon = sqrtf(b.rayon);
		}
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


// Modèles que le jeu ne dessine jamais : aides à la navigation (le binaire
// pose le drapeau 0x1000000 sur nog_ / walkable_, docs/idb.md) et maillages
// « no draw » (_ND). Leurs sommets sont souvent des triangles fantômes posés
// loin sous la scène.
inline bool JamaisDessine(const std::string &nom){
	std::string n = Minuscules(nom.c_str());
	if(n.compare(0, 4, "nog_") == 0 || n.compare(0, 5, "nogo_") == 0 || n.compare(0, 9, "walkable_") == 0) return true;
	return n.find("_nd_") != std::string::npos || (n.size() > 3 && n.compare(n.size() - 3, 3, "_nd") == 0);
}
inline std::vector<CIplInst> *g_inst = nil;
inline int32 GarderInst(const CIplInst &e){ g_inst->push_back(e); return 0; }

// Place dans `s` tous les modèles d'un fichier de placements « Ipl$ » (.ipb).
inline bool ChargerPlacements(Archives &a, const std::string &ipb, Scene &s){
	uint32 nb; uint8 *buf = LireMonde(a, ipb, &nb);
	if(buf == nil){ fprintf(stderr, "%s absent de World.img\n", ipb.c_str()); return false; }
	std::vector<CIplInst> &inst = s.placements; inst.clear(); g_inst = &inst;
	CIplFile::ms_instHandler = GarderInst;
	CIplFile::Load(buf, nb);
	free(buf);
	printf("%s : %zu placements\n", ipb.c_str(), inst.size());
	std::map<std::string, int> compte; int ignores = 0;
	for(const CIplInst &e : inst){
		std::string modele = e.name;
		if(modele.empty()){ auto it = a.modeleDe.find(e.modelId); if(it == a.modeleDe.end()) continue; modele = it->second; }
		if(JamaisDessine(modele)){ ignores++; continue; }
		if(s.AjouterModele(a, modele, NifFromPlacement(e.pos, e.scale, e.rot))) compte[modele]++;
	}
	printf("  %d modèles placés (%d introuvables, %d jamais dessinés ignorés), %zu triangles, %zu textures, %zu modèles distincts\n",
	       s.modeles, s.manquants, ignores, s.tri.size() / 3, s.texNoms.size(), compte.size());
	return !s.tri.empty();
}

// Toutes les collisions de World.img (488 fichiers .col), par nom de modèle
// en minuscules. Le modèle de collision est dans l'espace entité, comme la
// géométrie (docs/ipl.md) : la même transformation de placement les pose.
inline std::map<std::string, CColModel*> *g_cols = nil;
inline void GarderCol(int32 id, const char *nom, CColModel *m, uint8){
	std::string k = nom && nom[0] ? Minuscules(nom) : "";
	if(k.empty()){ auto it = g_arch->modeleDe.find(id); if(it != g_arch->modeleDe.end()) k = Minuscules(it->second.c_str()); }
	if(k.empty() || g_cols->count(k)){ m->RemoveCollisionVolumes(); free(m); return; }
	(*g_cols)[k] = m;
}
inline void ChargerToutesCollisions(Archives &a, std::map<std::string, CColModel*> &cols){
	g_arch = &a; g_cols = &cols;
	CColLoader::ms_handler = GarderCol;
	const CdImage &img = CdStream::ms_images[a.monde];
	for(int32 k = 0; k < img.m_numEntries; k++){
		const CDirectoryEntry *d = &img.m_entries[k]; size_t L = strlen(d->name);
		if(L < 4 || strcasecmp(d->name + L - 4, ".col") != 0) continue;
		uint32 n; uint8 *b = LireEntree(a.monde, "Stream\\World.img", d->name, &n);
		if(b){ CColLoader::LoadCollisionFile(b, n, 0); free(b); }
	}
}

// Pose dans `m` les collisions de tous les placements de la scène, y compris
// les modèles jamais dessinés. « WALKABLE_ » porte des sols. « NOGO_ » est
// une zone interdite : un volume fermé qu'on ne traverse pas. Mesuré sur
// iboxing : sans NOGO_iboxingOP, le corps sort de la zone jouable et tombe,
// parce qu'au-delà il n'y a plus aucun sol ; avec, il est arrêté au bord.
inline void PoserCollisions(Archives &a, const Scene &s, const std::map<std::string, CColModel*> &cols, CMondeCollision &m, int *poses, int *sans){
	*poses = 0; *sans = 0;
	for(const CIplInst &e : s.placements){
		std::string modele = e.name;
		if(modele.empty()){ auto it = a.modeleDe.find(e.modelId); if(it == a.modeleDe.end()) continue; modele = it->second; }
		std::string k = Minuscules(modele.c_str());
		auto it = cols.find(k);
		if(it == cols.end() || it->second->pColData == nil){ (*sans)++; continue; }
		const CCollisionData &c = *it->second->pColData;
		NifTransform t = NifFromPlacement(e.pos, e.scale, e.rot);
		for(int32 i = 0; i < c.numTriangles; i++){
			const CColTriangle &tr = c.triangles[i];
			m.AjouterTriangle(NifApply(t, c.vertices[tr.a].Get()), NifApply(t, c.vertices[tr.b].Get()), NifApply(t, c.vertices[tr.c].Get()));
		}
		for(int32 i = 0; i < c.numBoxes; i++){
			const CColBox &bx = c.boxes[i]; CVector k8[8];
			for(int j = 0; j < 8; j++) k8[j] = NifApply(t, CVector(j & 1 ? bx.max.x : bx.min.x, j & 2 ? bx.max.y : bx.min.y, j & 4 ? bx.max.z : bx.min.z));
			m.AjouterBoite(k8);
		}
		for(int32 i = 0; i < c.numSpheres; i++){
			// une échelle non uniforme déformerait la sphère : on garde la plus grande
			float sc = fmaxf(fmaxf(fabsf(e.scale.x), fabsf(e.scale.y)), fabsf(e.scale.z)); if(sc <= 0) sc = 1;
			m.AjouterSphere(NifApply(t, c.spheres[i].center), c.spheres[i].radius * sc);
		}
		(*poses)++;
	}
}

} // namespace outil
