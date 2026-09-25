// Rasteriseur logiciel minimal : triangles texturés avec tampon de
// profondeur, projection orthographique. Aucune dépendance, portable tel quel
// vers une cible sans GPU utilisable, ce qui est le cas de la New 3DS dans
// un premier temps. Il sert à voir un modèle et à tester le pipeline ; ce
// n'est pas le moteur de rendu du jeu.
#pragma once
#include "../common.h"

struct RasterImage {
	int32 w, h;
	uint8 *rgb;                     // w × h × 3
	float *depth;                   // w × h, plus petit = plus proche
};

struct RasterTexture {
	uint32 w, h;
	const uint8 *rgba;              // w × h × 4, non possédé
};

struct RasterVertex {
	float x, y, z;                  // espace écran : x, y en pixels, z profondeur
	float u, v;
};

RasterImage RasterCreate(int32 w, int32 h, uint8 r, uint8 g, uint8 b);
void RasterFree(RasterImage &img);
// `shade` multiplie la couleur (éclairage déjà calculé par l'appelant) ;
// sans texture, la couleur de base est gris clair.
void RasterTriangle(RasterImage &img, const RasterVertex v[3], const RasterTexture *tex, float shade);
bool RasterWritePPM(const RasterImage &img, const char *path);
