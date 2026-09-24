// Lit les textures .nft de Stream/World.img avec CNifFile. Un .nft est un
// fichier NIF qui ne contient que des blocs de texture ; les valeurs de
// EXTradar001.nft sont relevées à la main, puis les 4 469 fichiers sont
// parcourus et chaque bloc est comparé à la taille annoncée dans l'en-tête.
//   BULLY_DATA=<racine du jeu> build/tests/test_nft
#include "../src/core/CdStream.h"
#include "../src/core/FileMgr.h"
#include "../src/gamebryo/NifFile.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC ligne %d : %s\n", __LINE__, #cond); echecs++; } }while(0)

static uint8 *
Lire(const CDirectoryEntry *d, uint32 *bytes)
{
	*bytes = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(*bytes);
	int32 fd = CFileMgr::OpenFile("Stream\\World.img", "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, *bytes);
	CFileMgr::CloseFile(fd);
	if(!ok){ free(buf); return nil; }
	return buf;
}

static bool
FinitPar(const char *nom, const char *ext)
{
	size_t n = strlen(nom), e = strlen(ext);
	if(n < e) return false;
	for(size_t i = 0; i < e; i++){
		char a = nom[n-e+i], b = ext[i];
		if(a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
		if(a != b) return false;
	}
	return true;
}

int
main(void)
{
	VERIF(CdStream::AddImage("Stream\\World.img") == 0);
	const CdImage &img = CdStream::ms_images[0];

	// --- Un fichier relevé à la main --------------------------------
	// EXTradar001.nft : deux textures 8×8 et 128×128, en DXT (format 4).
	{
		const CDirectoryEntry *d = img.Find("EXTradar001.nft");
		VERIF(d != nil);
		if(d){
			uint32 bytes; uint8 *b = Lire(d, &bytes);
			VERIF(b != nil);
			if(b){
				CNifFile f;
				VERIF(f.Load(b, bytes));
				VERIF(f.version == 0x14030009);
				VERIF(f.numBlocks == 8 && f.numTypes == 4 && f.numStrings == 4);
				VERIF(f.numDecoded == 8);              // tous les blocs sont connus
				VERIF(strcmp(f.String(0), "Filename") == 0);
				VERIF(strcmp(f.String(1), "radar00.tga") == 0);
				VERIF(strcmp(f.String(3), "radar01.tga") == 0);

				NifSourceTexture *t = (NifSourceTexture*)f.blocks[0].data;
				VERIF(t != nil && t->external == 0);
				VERIF(t->fileName == 1 && t->pixelData == 3);
				VERIF(t->pixelLayout == 6 && t->useMipmaps == 2 && t->alphaFormat == 3);
				VERIF(t->isStatic == 1);

				NifExtraData *e = (NifExtraData*)f.blocks[1].data;
				VERIF(e != nil && e->name == 0 && e->value == 1);   // Filename → radar00.tga
				NifExtraData *i = (NifExtraData*)f.blocks[2].data;
				VERIF(i != nil && i->name == 2 && i->value == 9);   // NifPackMinorVersion = 9

				// 8×8 en DXT1 : (8/4)×(8/4) blocs de 8 octets = 32.
				NifPixelData *p = (NifPixelData*)f.blocks[3].data;
				VERIF(p != nil);
				if(p){
					VERIF(p->pixelFormat == 4 && p->palette == -1);
					VERIF(p->numMipmaps == 1 && p->numFaces == 1);
					VERIF(p->mipmaps && p->mipmaps[0].width == 8 && p->mipmaps[0].height == 8);
					VERIF(p->mipmaps[0].offset == 0);
					VERIF(p->numPixels == 32);
					VERIF(p->pixels != nil);
					// Quatre canaux, le premier compressé, les trois autres vides.
					VERIF(p->channels[0].type == 4 && p->channels[0].convention == 4);
					VERIF(p->channels[1].type == 19 && p->channels[1].convention == 5);
					VERIF(p->channels[3].type == 19);
				}
				// 128×128 en DXT1 : 32×32 blocs de 8 octets = 8 192.
				NifPixelData *q = (NifPixelData*)f.blocks[7].data;
				VERIF(q != nil);
				if(q){
					VERIF(q->mipmaps && q->mipmaps[0].width == 128 && q->mipmaps[0].height == 128);
					VERIF(q->numPixels == 8192);
				}
				f.Free(); free(b);
			}
		}
	}

	// --- Tous les .nft de l'archive ---------------------------------
	int32 fichiers = 0, parfaits = 0, grandBoutistes = 0;
	int32 blocs = 0, pixelData = 0, mipmaps = 0, palettes = 0, cubemaps = 0;
	long long octetsPixels = 0;
	for(int32 k = 0; k < img.m_numEntries; k++){
		const CDirectoryEntry *d = &img.m_entries[k];
		if(!FinitPar(d->name, ".nft")) continue;
		fichiers++;
		uint32 bytes; uint8 *b = Lire(d, &bytes);
		if(b == nil){ printf("ECHEC : lecture de %s\n", d->name); echecs++; continue; }
		CNifFile f;
		if(!f.Load(b, bytes)){ printf("ECHEC : %s ne se charge pas\n", d->name); echecs++; free(b); continue; }
		if(f.endian == 0) grandBoutistes++;
		bool bon = f.numDecoded == f.numBlocks;
		for(int32 i = 0; i < f.numBlocks; i++){
			const NifBlock &bl = f.blocks[i];
			if(bl.data == nil) continue;
			blocs++;
			if(bl.kind == NIF_PIXELDATA){
				NifPixelData *p = (NifPixelData*)bl.data;
				pixelData++; mipmaps += (int32)p->numMipmaps;
				octetsPixels += (long long)p->numPixels * p->numFaces;
			}else if(bl.kind == NIF_PALETTE) palettes++;
			if(strcmp(f.TypeName(i), "NiSourceCubeMap") == 0) cubemaps++;
		}
		if(bon) parfaits++;
		f.Free(); free(b);
	}
	printf("\n%d fichiers .nft (%d grand-boutistes), %d lus entièrement\n", fichiers, grandBoutistes, parfaits);
	printf("%d blocs décodés : %d NiPixelData (%d niveaux de mipmap, %lld octets de pixels), %d NiPalette, %d NiSourceCubeMap\n",
	       blocs, pixelData, mipmaps, (long long)octetsPixels, palettes, cubemaps);

	VERIF(fichiers == 4469);
	// Quatre fichiers gardent un octet parasite après un NiPixelData et
	// désalignent la suite : BBonusB, Barr01_Switch, BeerKeg, BirdBath.
	// Ils sont décrits dans docs/nft.md et attendus en échec.
	VERIF(parfaits == 4465);
	VERIF(grandBoutistes == 131);

	printf(echecs ? "\n%d verification(s) en echec\n" : "\ntout passe\n", echecs);
	return echecs != 0;
}
