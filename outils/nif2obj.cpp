// Exporte un modèle du jeu en OBJ, avec ses textures, pour le voir dans
// n'importe quel logiciel 3D (Blender : import OBJ, axe Z vers le haut).
//
//   BULLY_DATA=<racine du jeu> build/outils/nif2obj <modèle> [dossier de sortie]
//
// Chaîne suivie, la même que le jeu :
//   1. le nom du modèle donne son dictionnaire de textures (colonne TXD des
//      définitions binaires .idb de Objects/ide.img) ; à défaut, un .nft du
//      même nom ;
//   2. le .nif est lu dans Stream/World.img et parcouru depuis la racine en
//      composant les transformations de chaque nœud ;
//   3. chaque géométrie prend la texture de base de son NiTexturingProperty,
//      dont le nom de fichier (.tga d'origine) est cherché parmi les
//      NiSourceTexture du .nft ; ses pixels sont décodés et écrits en TGA.
//
// La composition des transformations est dans src/gamebryo/NifTransform,
// avec la convention vérifiée contre les boîtes de collision.
#include "commun.h"
#include "../src/gamebryo/NifTransform.h"
#include <sys/stat.h>
using namespace outil;

// --- Export ---------------------------------------------------------------

struct Export {
	const CNifFile *nif;
	std::map<std::string, outil::Texture> textures; // base du .tga → pixels
	std::map<std::string, std::string> ecrites;  // base du .tga → fichier TGA écrit
	std::string dossier;
	FILE *obj, *mtl;
	int sommets = 0, uvs = 0, triangles = 0, formes = 0, texturees = 0;
	std::vector<std::string> manquantes;
};

static bool
EcrireTga(const std::string &chemin, const uint8 *rgba, uint32 w, uint32 h)
{
	FILE *f = fopen(chemin.c_str(), "wb");
	if(f == nil) return false;
	uint8 en[18] = {0};
	en[2] = 2;                                     // vraies couleurs, non compressé
	en[12] = (uint8)(w & 0xff); en[13] = (uint8)(w >> 8);
	en[14] = (uint8)(h & 0xff); en[15] = (uint8)(h >> 8);
	en[16] = 32;
	en[17] = 0x28;                                 // origine en haut à gauche, 8 bits d'alpha
	fwrite(en, 1, 18, f);
	std::vector<uint8> ligne(w * 4);
	for(uint32 y = 0; y < h; y++){
		for(uint32 x = 0; x < w; x++){
			const uint8 *p = rgba + (y * w + x) * 4;
			ligne[x*4] = p[2]; ligne[x*4+1] = p[1]; ligne[x*4+2] = p[0]; ligne[x*4+3] = p[3];
		}
		fwrite(ligne.data(), 1, ligne.size(), f);
	}
	fclose(f);
	return true;
}

// Rend le nom de matériau à utiliser, en écrivant la texture la première fois.
static std::string
Materiau(Export &e, const std::string &base)
{
	if(base.empty()) return "";                    // forme sans texture de base
	auto deja = e.ecrites.find(base);
	if(deja != e.ecrites.end()) return deja->second;
	auto t = e.textures.find(base);
	if(t == e.textures.end()){ e.manquantes.push_back(base); return ""; }
	uint32 w, h;
	if(!MipSize(*t->second.px, 0, &w, &h)) return "";
	std::vector<uint8> rgba((size_t)w * h * 4);
	if(!DecodeNifPixels(*t->second.px, t->second.pal, 0, rgba.data())) return "";
	std::string fichier = base + ".tga";
	if(!EcrireTga(e.dossier + "/" + fichier, rgba.data(), w, h)) return "";
	fprintf(e.mtl, "newmtl %s\nKd 1 1 1\nmap_Kd %s\n\n", base.c_str(), fichier.c_str());
	e.ecrites[base] = base;
	return base;
}

static std::string
TextureDeBase(const Export &e, const NifAVObject &o)
{
	const CNifFile &f = *e.nif;
	for(int32 i = 0; i < o.numProperties; i++){
		int32 b = o.properties[i];
		if(b < 0 || b >= f.numBlocks || f.blocks[b].kind != NIF_TEXTURINGPROPERTY || !f.blocks[b].data) continue;
		const NifTexturingProperty *tp = (const NifTexturingProperty*)f.blocks[b].data;
		int32 s = tp->baseTexture;
		if(s < 0 || s >= f.numBlocks || f.blocks[s].kind != NIF_SOURCETEXTURE || !f.blocks[s].data) continue;
		return BaseNom(f.String(((const NifSourceTexture*)f.blocks[s].data)->fileName));
	}
	return "";
}

static void
Forme(const CNifFile &f, int32, const NifGeometry &g, const NifGeometryData &d, const NifTransform &t, void *ctx)
{
	Export &e = *(Export*)ctx;
	if(d.vertices == nil || d.triangles == nil || d.numTriangles == 0) return;
	std::string mat = Materiau(e, TextureDeBase(e, g));
	fprintf(e.obj, "o %s_%d\n", f.String(g.name)[0] ? f.String(g.name) : "forme", e.formes);
	if(!mat.empty()){ fprintf(e.obj, "usemtl %s\n", mat.c_str()); e.texturees++; }
	int base = e.sommets, baseUv = e.uvs;
	for(int i = 0; i < d.numVertices; i++){
		CVector v = NifApply(t, d.vertices[i]);
		fprintf(e.obj, "v %.5f %.5f %.5f\n", v.x, v.y, v.z);
	}
	bool avecUv = d.uv && d.numUVSets > 0;
	if(avecUv)
		for(int i = 0; i < d.numVertices; i++)       // OBJ a son origine UV en bas à gauche
			fprintf(e.obj, "vt %.5f %.5f\n", d.uv[i][0], 1.0f - d.uv[i][1]);
	for(int i = 0; i < d.numTriangles; i++){
		int a = base + d.triangles[i][0] + 1, bb = base + d.triangles[i][1] + 1, c = base + d.triangles[i][2] + 1;
		if(avecUv){
			int ua = baseUv + d.triangles[i][0] + 1, ub = baseUv + d.triangles[i][1] + 1, uc = baseUv + d.triangles[i][2] + 1;
			fprintf(e.obj, "f %d/%d %d/%d %d/%d\n", a, ua, bb, ub, c, uc);
		}else
			fprintf(e.obj, "f %d %d %d\n", a, bb, c);
	}
	e.sommets += d.numVertices;
	if(avecUv) e.uvs += d.numVertices;
	e.triangles += d.numTriangles;
	e.formes++;
}

int
main(int argc, char **argv)
{
	if(argc < 2){ fprintf(stderr, "usage : nif2obj <modèle> [dossier]\n"); return 2; }
	std::string modele = argv[1];
	if(modele.size() > 4 && strcasecmp(modele.c_str() + modele.size() - 4, ".nif") == 0) modele.resize(modele.size() - 4);
	std::string dossier = argc > 2 ? argv[2] : "export-obj/" + modele;
	mkdir("export-obj", 0755);
	mkdir(dossier.c_str(), 0755);

	outil::Archives arch;
	if(!outil::Ouvrir(arch)) return 1;
	int32 monde = arch.monde;
	auto it = arch.txdDe.find(Minuscules(modele.c_str()));
	std::string txd = it != arch.txdDe.end() ? it->second : modele;
	printf("%s : dictionnaire de textures « %s »%s\n", modele.c_str(), txd.c_str(),
	       it != arch.txdDe.end() ? " (définitions)" : " (même nom, absent des définitions)");

	uint32 nb, tb = 0;
	uint8 *nifBuf = LireEntree(monde, "Stream\\World.img", (modele + ".nif").c_str(), &nb);
	if(nifBuf == nil){ fprintf(stderr, "%s.nif absent de World.img\n", modele.c_str()); return 1; }
	CNifFile nif, nft;
	if(!nif.Load(nifBuf, nb)){ fprintf(stderr, "%s.nif illisible\n", modele.c_str()); return 1; }

	Export e;
	e.nif = &nif;
	e.dossier = dossier;
	uint8 *nftBuf = LireEntree(monde, "Stream\\World.img", (txd + ".nft").c_str(), &tb);
	if(nftBuf && nft.Load(nftBuf, tb)){
		for(int32 i = 0; i < nft.numBlocks; i++){
			if(nft.blocks[i].kind != NIF_SOURCETEXTURE || !nft.blocks[i].data) continue;
			const NifSourceTexture *st = (const NifSourceTexture*)nft.blocks[i].data;
			int32 p = st->pixelData;
			if(p < 0 || p >= nft.numBlocks || nft.blocks[p].kind != NIF_PIXELDATA || !nft.blocks[p].data) continue;
			const NifPixelData *px = (const NifPixelData*)nft.blocks[p].data;
			const NifPalette *pal = nil;
			if(px->palette >= 0 && px->palette < nft.numBlocks && nft.blocks[px->palette].kind == NIF_PALETTE)
				pal = (const NifPalette*)nft.blocks[px->palette].data;
			{ outil::Texture t; t.px = px; t.pal = pal; e.textures[BaseNom(nft.String(st->fileName))] = t; }
		}
	}else
		printf("  %s.nft introuvable : export sans textures\n", txd.c_str());

	e.obj = fopen((dossier + "/" + modele + ".obj").c_str(), "w");
	e.mtl = fopen((dossier + "/" + modele + ".mtl").c_str(), "w");
	if(!e.obj || !e.mtl){ fprintf(stderr, "impossible d'écrire dans %s\n", dossier.c_str()); return 1; }
	fprintf(e.obj, "# %s, exporté de bully.exe par bully-re (outils/nif2obj)\nmtllib %s.mtl\n", modele.c_str(), modele.c_str());
	NifWalkShapes(nif, Forme, &e);
	fclose(e.obj); fclose(e.mtl);

	printf("  %d formes, %d sommets, %d triangles, %d formes texturées, %zu textures écrites\n",
	       e.formes, e.sommets, e.triangles, e.texturees, e.ecrites.size());
	for(auto &m : e.manquantes) printf("  texture absente du dictionnaire : %s\n", m.c_str());
	printf("  → %s/%s.obj\n", dossier.c_str(), modele.c_str());
	nif.Free(); nft.Free(); free(nifBuf); free(nftBuf);
	return e.formes > 0 ? 0 : 1;
}
