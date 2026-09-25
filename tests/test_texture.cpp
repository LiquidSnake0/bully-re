// Décodage des textures : d'abord des blocs DXT construits à la main dont le
// résultat se calcule d'après la spécification BC1 / BC3, puis toutes les
// NiPixelData des .nft de Stream/World.img, niveau 0.
//   BULLY_DATA=<racine du jeu> build/tests/test_texture
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/gamebryo/NifFile.h"
#include "../src/gamebryo/TextureDecode.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC ligne %d : %s\n", __LINE__, #cond); echecs++; } }while(0)

static bool
Pixel(const uint8 *px, int i, int r, int g, int b, int a)
{
	return px[i*4] == r && px[i*4+1] == g && px[i*4+2] == b && px[i*4+3] == a;
}

int
main(void)
{
	uint8 out[64];

	// --- DXT1, mode quatre couleurs (c0 > c1) --------------------------
	// c0 = rouge pur 0xF800, c1 = bleu pur 0x001F. Indices des quatre
	// premiers pixels : 0, 1, 2, 3 ; le reste à 0.
	// Couleur 2 = (2·c0 + c1) / 3, couleur 3 = (c0 + 2·c1) / 3.
	{
		uint8 b[8] = { 0x00, 0xF8, 0x1F, 0x00, 0xE4, 0x00, 0x00, 0x00 };  // 0b11100100
		DecodeDxt1Block(b, out, false);
		VERIF(Pixel(out, 0, 255, 0, 0, 255));
		VERIF(Pixel(out, 1, 0, 0, 255, 255));
		VERIF(Pixel(out, 2, 170, 0, 85, 255));
		VERIF(Pixel(out, 3, 85, 0, 170, 255));
		VERIF(Pixel(out, 15, 255, 0, 0, 255));
	}
	// --- DXT1, mode trois couleurs plus transparent (c0 <= c1) ---------
	{
		uint8 b[8] = { 0x1F, 0x00, 0x00, 0xF8, 0xE4, 0x00, 0x00, 0x00 };
		DecodeDxt1Block(b, out, false);
		VERIF(Pixel(out, 0, 0, 0, 255, 255));
		VERIF(Pixel(out, 1, 255, 0, 0, 255));
		VERIF(Pixel(out, 2, 127, 0, 127, 255));       // moyenne
		VERIF(out[3*4+3] == 0);                        // indice 3 : transparent
	}
	// --- Réplication des bits : blanc 0xFFFF donne 255 partout ---------
	{
		uint8 b[8] = { 0xFF, 0xFF, 0x00, 0x00, 0, 0, 0, 0 };
		DecodeDxt1Block(b, out, false);
		VERIF(Pixel(out, 5, 255, 255, 255, 255));
	}
	// --- DXT5 : alpha sur huit niveaux (a0 > a1) ------------------------
	// a0 = 255, a1 = 0 ; pixel 0 indice 0, pixel 1 indice 1, pixel 2
	// indice 2 = (6·255 + 0) / 7 = 218. Couleur : blanc partout.
	{
		uint8 b[16] = { 255, 0,  0x88, 0x00, 0, 0, 0, 0,     // indices 000 001 010 ...
		                0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0 };
		DecodeDxt5Block(b, out, false);
		VERIF(out[0*4+3] == 255);
		VERIF(out[1*4+3] == 0);
		VERIF(out[2*4+3] == 218);
		VERIF(out[3*4+3] == 255);
		VERIF(Pixel(out, 0, 255, 255, 255, 255));
	}
	// --- DXT5 : mode six niveaux plus 0 et 255 (a0 <= a1) ---------------
	{
		// indices des pixels 0 et 1 : 6 et 7 → 0 et 255
		uint8 b[16] = { 10, 200, 0x3E, 0x00, 0, 0, 0, 0,     // 110 111 = 0b111110
		                0, 0, 0, 0, 0, 0, 0, 0 };
		DecodeDxt5Block(b, out, false);
		VERIF(out[0*4+3] == 0);
		VERIF(out[1*4+3] == 255);
	}

	// --- Toutes les textures de l'archive --------------------------------
	VERIF(CdStream::AddImage("Stream\\World.img") == 0);
	const CdImage &img = CdStream::ms_images[0];
	std::map<uint32, int> ok, total;
	long long pixels = 0;
	int fichiers = 0;
	for(int32 k = 0; k < img.m_numEntries; k++){
		const CDirectoryEntry *d = &img.m_entries[k];
		size_t L = strlen(d->name);
		if(L < 4 || strcasecmp(d->name + L - 4, ".nft") != 0) continue;
		uint32 bytes = d->size * CDSTREAM_SECTOR_SIZE;
		uint8 *buf = (uint8*)malloc(bytes);
		int32 fd = CFileMgr::OpenFile("Stream\\World.img", "rb", 1);
		CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
		bool lu = CFileMgr::ReadExact(fd, buf, bytes);
		CFileMgr::CloseFile(fd);
		CNifFile f;
		if(lu && f.Load(buf, bytes)){
			fichiers++;
			for(int32 i = 0; i < f.numBlocks; i++){
				if(f.blocks[i].kind != NIF_PIXELDATA || f.blocks[i].data == nil) continue;
				NifPixelData *p = (NifPixelData*)f.blocks[i].data;
				total[p->pixelFormat]++;
				const NifPalette *pal = nil;
				if(p->palette >= 0 && p->palette < f.numBlocks && f.blocks[p->palette].kind == NIF_PALETTE)
					pal = (const NifPalette*)f.blocks[p->palette].data;
				uint32 w, h;
				if(!MipSize(*p, 0, &w, &h)) continue;
				uint8 *rgba = (uint8*)malloc((size_t)w * h * 4);
				if(DecodeNifPixels(*p, pal, 0, rgba)){ ok[p->pixelFormat]++; pixels += (long long)w * h; }
				free(rgba);
			}
			f.Free();
		}
		free(buf);
	}
	int tout = 0, bons = 0;
	for(auto &t : total){
		printf("format %u : %d / %d décodées\n", t.first, ok[t.first], t.second);
		tout += t.second; bons += ok[t.first];
	}
	printf("%d fichiers, %d textures, %d décodées, %lld pixels au niveau 0\n", fichiers, tout, bons, pixels);
	VERIF(tout == 35635);
	VERIF(bons == tout);

	printf(echecs ? "\n%d verification(s) en echec\n" : "\ntout passe\n", echecs);
	return echecs != 0;
}
