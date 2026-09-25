#include "TextureDecode.h"
#include "NifFile.h"
#include <cstring>

static inline uint16
Lire16(const uint8 *p, bool swap)
{
	return swap ? (uint16)(p[0] << 8 | p[1]) : (uint16)(p[1] << 8 | p[0]);
}

// RGB 5:6:5 étendu à 8 bits en répliquant les bits hauts, comme les
// décodeurs matériels.
static inline void
Couleur565(uint16 c, uint8 *o)
{
	uint8 r = (uint8)(c >> 11 & 0x1f), g = (uint8)(c >> 5 & 0x3f), b = (uint8)(c & 0x1f);
	o[0] = (uint8)(r << 3 | r >> 2);
	o[1] = (uint8)(g << 2 | g >> 4);
	o[2] = (uint8)(b << 3 | b >> 2);
	o[3] = 255;
}

// Palette de 4 couleurs d'un bloc BC1. `bloc1` : le mode à trois couleurs plus
// transparent existe en DXT1 quand c0 <= c1, jamais dans la partie couleur
// d'un DXT5.
static void
PaletteCouleurs(uint16 c0, uint16 c1, bool bloc1, uint8 pal[4][4])
{
	Couleur565(c0, pal[0]);
	Couleur565(c1, pal[1]);
	if(!bloc1 || c0 > c1){
		for(int k = 0; k < 3; k++){
			pal[2][k] = (uint8)((2 * pal[0][k] + pal[1][k]) / 3);
			pal[3][k] = (uint8)((pal[0][k] + 2 * pal[1][k]) / 3);
		}
		pal[2][3] = pal[3][3] = 255;
	}else{
		for(int k = 0; k < 3; k++){
			pal[2][k] = (uint8)((pal[0][k] + pal[1][k]) / 2);
			pal[3][k] = 0;
		}
		pal[2][3] = 255;
		pal[3][3] = 0;
	}
}

static void
IndicesCouleurs(const uint8 *p, bool swap, const uint8 pal[4][4], uint8 out[64])
{
	// Les 32 bits d'indices sont deux mots de 16 bits : le premier porte les
	// lignes 0 et 1, le second les lignes 2 et 3.
	uint32 idx = (uint32)Lire16(p, swap) | (uint32)Lire16(p + 2, swap) << 16;
	for(int i = 0; i < 16; i++)
		memcpy(out + i * 4, pal[idx >> (2 * i) & 3], 4);
}

void
DecodeDxt1Block(const uint8 *b, uint8 out[64], bool swap16)
{
	uint8 pal[4][4];
	PaletteCouleurs(Lire16(b, swap16), Lire16(b + 2, swap16), true, pal);
	IndicesCouleurs(b + 4, swap16, pal, out);
}

void
DecodeDxt5Block(const uint8 *b, uint8 out[64], bool swap16)
{
	// Alpha : deux valeurs de référence puis 48 bits d'indices de 3 bits.
	// En grand-boutiste, les huit octets sont eux aussi groupés par mots de
	// 16 bits inversés : on les remet dans l'ordre avant de les lire.
	uint8 a[8];
	for(int k = 0; k < 8; k += 2){
		a[k] = swap16 ? b[k + 1] : b[k];
		a[k + 1] = swap16 ? b[k] : b[k + 1];
	}
	uint8 alpha[8];
	alpha[0] = a[0];
	alpha[1] = a[1];
	if(alpha[0] > alpha[1]){
		for(int k = 1; k < 7; k++)
			alpha[k + 1] = (uint8)(((7 - k) * alpha[0] + k * alpha[1]) / 7);
	}else{
		for(int k = 1; k < 5; k++)
			alpha[k + 1] = (uint8)(((5 - k) * alpha[0] + k * alpha[1]) / 5);
		alpha[6] = 0;
		alpha[7] = 255;
	}
	uint64_t bits = 0;
	for(int k = 0; k < 6; k++)
		bits |= (uint64_t)a[2 + k] << (8 * k);

	uint8 pal[4][4];
	PaletteCouleurs(Lire16(b + 8, swap16), Lire16(b + 10, swap16), false, pal);
	IndicesCouleurs(b + 12, swap16, pal, out);
	for(int i = 0; i < 16; i++)
		out[i * 4 + 3] = alpha[bits >> (3 * i) & 7];
}

bool
MipSize(const NifPixelData &px, uint32 mip, uint32 *w, uint32 *h)
{
	if(mip >= px.numMipmaps || px.mipmaps == nil) return false;
	*w = px.mipmaps[mip].width;
	*h = px.mipmaps[mip].height;
	return *w > 0 && *h > 0;
}

// Octets occupés par le niveau `mip`, pour vérifier qu'on ne lit pas au-delà.
static uint32
TailleNiveau(uint32 fmt, uint32 w, uint32 h)
{
	uint32 bw = (w + 3) / 4, bh = (h + 3) / 4;
	switch(fmt){
	case NIF_PX_RGB8:  return w * h * 3;
	case NIF_PX_RGBA8: return w * h * 4;
	case NIF_PX_PAL8:  return w * h;
	case NIF_PX_DXT1:  return bw * bh * 8;
	case NIF_PX_DXT3:
	case NIF_PX_DXT5:  return bw * bh * 16;
	default:           return 0;
	}
}

// Position de chaque canal dans un pixel RGB ou RGBA brut, d'après les types
// de canal de NiPixelFormat (0 rouge, 1 vert, 2 bleu, 3 alpha).
static void
OrdreCanaux(const NifPixelData &px, int ordre[4])
{
	ordre[0] = 0; ordre[1] = 1; ordre[2] = 2; ordre[3] = 3;
	for(int k = 0; k < 4; k++){
		uint32 t = px.channels[k].type;
		if(t <= 3) ordre[t] = k;
	}
}

bool
DecodeNifPixels(const NifPixelData &px, const NifPalette *palette, uint32 mip, uint8 *rgba)
{
	uint32 w, h;
	if(!MipSize(px, mip, &w, &h) || px.pixels == nil) return false;
	uint32 debut = px.mipmaps[mip].offset;
	uint32 taille = TailleNiveau(px.pixelFormat, w, h);
	if(taille == 0 || debut + taille > px.numPixels) return false;
	const uint8 *src = px.pixels + debut;

	switch(px.pixelFormat){
	case NIF_PX_RGB8:
	case NIF_PX_RGBA8: {
		int ordre[4];
		OrdreCanaux(px, ordre);
		int n = px.pixelFormat == NIF_PX_RGB8 ? 3 : 4;
		for(uint32 i = 0; i < w * h; i++){
			const uint8 *s = src + i * n;
			rgba[i * 4 + 0] = s[ordre[0]];
			rgba[i * 4 + 1] = s[ordre[1]];
			rgba[i * 4 + 2] = s[ordre[2]];
			rgba[i * 4 + 3] = n == 4 ? s[ordre[3]] : 255;
		}
		return true;
	}
	case NIF_PX_PAL8:
		if(palette == nil || palette->entries == nil) return false;
		for(uint32 i = 0; i < w * h; i++){
			uint32 e = src[i];
			if(e >= palette->numEntries) return false;
			memcpy(rgba + i * 4, palette->entries + e * 4, 4);
			if(!palette->hasAlpha) rgba[i * 4 + 3] = 255;
		}
		return true;
	case NIF_PX_DXT1:
	case NIF_PX_DXT5: {
		uint32 bw = (w + 3) / 4, bh = (h + 3) / 4;
		uint32 pas = px.pixelFormat == NIF_PX_DXT1 ? 8 : 16;
		uint8 bloc[64];
		for(uint32 by = 0; by < bh; by++)
			for(uint32 bx = 0; bx < bw; bx++){
				const uint8 *b = src + (by * bw + bx) * pas;
				if(px.pixelFormat == NIF_PX_DXT1) DecodeDxt1Block(b, bloc, false);
				else DecodeDxt5Block(b, bloc, false);
				// Les niveaux plus petits que 4 × 4 ne gardent que le coin utile.
				for(uint32 y = 0; y < 4 && by * 4 + y < h; y++)
					for(uint32 x = 0; x < 4 && bx * 4 + x < w; x++)
						memcpy(rgba + ((by * 4 + y) * w + bx * 4 + x) * 4, bloc + (y * 4 + x) * 4, 4);
			}
		return true;
	}
	default:
		return false;
	}
}
