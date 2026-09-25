// Décodage des pixels d'un NiPixelData vers du RGBA 8 bits par canal.
//
// Formats relevés sur les 35 635 blocs des .nft (voir docs/nft.md) :
//   pixelFormat 0  RGB 24 bits          134 blocs
//   pixelFormat 1  RGBA 32 bits         134 blocs
//   pixelFormat 2  palette 8 bits       127 blocs, avec un NiPalette
//   pixelFormat 4  DXT1 (BC1)        31 714 blocs, 8 octets par bloc de 4 × 4
//   pixelFormat 6  DXT5 (BC3)         3 526 blocs, 16 octets par bloc de 4 × 4
// Les numéros suivent l'énumération PixelFormat de Gamebryo (5 serait DXT3,
// absent des données).
#pragma once
#include "../common.h"

struct NifPixelData;
struct NifPalette;

enum eNifPixelFormat {
	NIF_PX_RGB8 = 0, NIF_PX_RGBA8 = 1, NIF_PX_PAL8 = 2,
	NIF_PX_DXT1 = 4, NIF_PX_DXT3 = 5, NIF_PX_DXT5 = 6
};

// Décode le niveau de mipmap `mip` de `px` dans `rgba` (largeur × hauteur × 4
// octets, alloué par l'appelant, voir MipSize). `palette` n'est lu que pour
// le format 2. Renvoie faux pour un format inconnu ou un tampon trop court.
//
// Pas de cas grand-boutiste : dans les 131 fichiers CS_*, seuls les champs du
// NIF sont inversés, les pixels sont restés en petit-boutiste. Mesuré sur
// 1 738 088 blocs DXT1 : c0 > c1 dans 79,5 % des blocs lus tels quels (86,5 %
// dans les fichiers petit-boutistes), 49,7 % une fois les mots inversés,
// autrement dit du hasard.
bool DecodeNifPixels(const NifPixelData &px, const NifPalette *palette, uint32 mip, uint8 *rgba);
bool MipSize(const NifPixelData &px, uint32 mip, uint32 *w, uint32 *h);

// Blocs isolés, exposés pour les tests. `out` reçoit 16 pixels RGBA en
// lignes de 4. `swap16` reste disponible pour des données qui en auraient
// besoin ; celles du jeu n'en ont pas.
void DecodeDxt1Block(const uint8 *block, uint8 out[64], bool swap16);
void DecodeDxt5Block(const uint8 *block, uint8 out[64], bool swap16);
