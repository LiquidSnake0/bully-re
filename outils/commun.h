// Chargement partagé par les outils : archives, définitions de modèles,
// textures d'un dictionnaire. Pas un module du moteur, juste ce que nif2obj,
// rendu et scene répètent tous les trois.
#pragma once
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/core/IdeBinary.h"
#include "../src/gamebryo/NifFile.h"
#include "../src/gamebryo/TextureDecode.h"
#include "../src/render/SoftRaster.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <map>
#include <string>
#include <vector>

// Le chargeur de définitions signale les plages d'identifiants au streaming ;
// les outils n'en ont pas besoin.
void RegisterModelRange(uint16, uint32) {}

namespace outil {

inline std::string Minuscules(const char *s){ std::string r(s); for(char &c : r) c = (char)tolower((unsigned char)c); return r; }

// Nom sans chemin ni extension, en minuscules : les NIF gardent le chemin de
// la machine d'export (« Z:\Bully\Temp\... »).
inline std::string BaseNom(const char *c){
	std::string s = Minuscules(c);
	size_t p = s.find_last_of("\\/"); if(p != std::string::npos) s = s.substr(p + 1);
	size_t d = s.rfind('.'); if(d != std::string::npos) s = s.substr(0, d);
	return s;
}

struct Archives {
	int32 monde = -1, ide = -1;
	std::map<std::string, std::string> txdDe;     // modèle → dictionnaire de textures
	std::map<int32, std::string> modeleDe;        // identifiant → modèle
};
inline Archives *g_arch = nil;
template<class E> inline int32 Retenir(const E &e){ g_arch->txdDe[Minuscules(e.model)] = e.txd; g_arch->modeleDe[e.id] = e.model; return 0; }

inline uint8 *LireEntree(int32 image, const char *imgPath, const char *nom, uint32 *bytes){
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
inline uint8 *LireMonde(const Archives &a, const std::string &nom, uint32 *bytes){ return LireEntree(a.monde, "Stream\\World.img", nom.c_str(), bytes); }

// Ouvre les deux archives et lit toutes les définitions .idb.
inline bool Ouvrir(Archives &a){
	a.monde = CdStream::AddImage("Stream\\World.img");
	a.ide = CdStream::AddImage("Objects\\ide.img");
	if(a.monde < 0 || a.ide < 0){ fprintf(stderr, "archives introuvables : BULLY_DATA doit pointer sur la racine du jeu\n"); return false; }
	g_arch = &a;
	CIdeBinary::ms_objHandler = Retenir<CObjIdeEntry>;  CIdeBinary::ms_pedHandler = Retenir<CPedIdeEntry>;
	CIdeBinary::ms_carHandler = Retenir<CCarIdeEntry>;  CIdeBinary::ms_weapHandler = Retenir<CWeapIdeEntry>;
	CIdeBinary::ms_panmHandler = Retenir<CPanmIdeEntry>; CIdeBinary::ms_simpleHandler = Retenir<CSimpleIdeEntry>;
	const CdImage &img = CdStream::ms_images[a.ide];
	for(int32 k = 0; k < img.m_numEntries; k++){
		const char *n = img.m_entries[k].name; size_t L = strlen(n);
		if(L < 4 || strcasecmp(n + L - 4, ".idb") != 0) continue;
		uint32 bytes; uint8 *buf = LireEntree(a.ide, "Objects\\ide.img", n, &bytes);
		if(buf == nil) continue;
		uint32 utile; memcpy(&utile, buf, 4);           // longueur utile en tête, docs/idb.md
		if(utile + 4 <= bytes) CIdeBinary::Load(buf + 4, utile);
		free(buf);
	}
	return true;
}

inline std::string TxdDe(const Archives &a, const std::string &modele){
	auto it = a.txdDe.find(Minuscules(modele.c_str()));
	return it != a.txdDe.end() ? it->second : modele;
}

// Un dictionnaire de textures décodé : base du .tga → RGBA du niveau 0.
struct Texture { std::vector<uint8> rgba; RasterTexture rt; const NifPixelData *px = nil; const NifPalette *pal = nil; };
struct Dictionnaire {
	CNifFile nft; uint8 *buf = nil;
	std::map<std::string, Texture> textures;
	~Dictionnaire(){ nft.Free(); free(buf); }
};
inline bool ChargerDictionnaire(const Archives &a, const std::string &txd, Dictionnaire &d, bool decoder){
	uint32 tb; d.buf = LireMonde(a, txd + ".nft", &tb);
	if(d.buf == nil || !d.nft.Load(d.buf, tb)) return false;
	for(int32 i = 0; i < d.nft.numBlocks; i++){
		if(d.nft.blocks[i].kind != NIF_SOURCETEXTURE || !d.nft.blocks[i].data) continue;
		const NifSourceTexture *st = (const NifSourceTexture*)d.nft.blocks[i].data;
		int32 p = st->pixelData;
		if(p < 0 || p >= d.nft.numBlocks || d.nft.blocks[p].kind != NIF_PIXELDATA || !d.nft.blocks[p].data) continue;
		Texture &t = d.textures[BaseNom(d.nft.String(st->fileName))];
		t.px = (const NifPixelData*)d.nft.blocks[p].data;
		if(t.px->palette >= 0 && t.px->palette < d.nft.numBlocks && d.nft.blocks[t.px->palette].kind == NIF_PALETTE)
			t.pal = (const NifPalette*)d.nft.blocks[t.px->palette].data;
		if(decoder){
			uint32 w, h; if(!MipSize(*t.px, 0, &w, &h)) continue;
			t.rgba.resize((size_t)w * h * 4);
			if(!DecodeNifPixels(*t.px, t.pal, 0, t.rgba.data())){ t.rgba.clear(); continue; }
			t.rt.w = w; t.rt.h = h; t.rt.rgba = t.rgba.data();
		}
	}
	return true;
}

// Base du .tga de la texture de base d'une forme, ou chaîne vide.
inline std::string TextureDeBase(const CNifFile &f, const NifAVObject &o){
	for(int32 i = 0; i < o.numProperties; i++){
		int32 b = o.properties[i];
		if(b < 0 || b >= f.numBlocks || f.blocks[b].kind != NIF_TEXTURINGPROPERTY || !f.blocks[b].data) continue;
		int32 s = ((const NifTexturingProperty*)f.blocks[b].data)->baseTexture;
		if(s < 0 || s >= f.numBlocks || f.blocks[s].kind != NIF_SOURCETEXTURE || !f.blocks[s].data) continue;
		return BaseNom(f.String(((const NifSourceTexture*)f.blocks[s].data)->fileName));
	}
	return "";
}

} // namespace outil
