// Rasteriseur logiciel minimal : triangles texturés avec tampon de
// profondeur, en projection orthographique (outils d'image) ou en perspective
// (rendu temps réel, voir Camera.h). Aucune dépendance, portable tel quel
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

// Sommet en perspective : x, y en pixels, invW = 1 / profondeur, et la
// texture déjà divisée par la profondeur (u·invW, v·invW). C'est ce qui rend
// l'interpolation correcte : en perspective, u n'est pas linéaire à l'écran,
// u / w l'est.
struct RasterPVertex {
	float x, y;
	float invW;
	float uw, vw;
};

RasterImage RasterCreate(int32 w, int32 h, uint8 r, uint8 g, uint8 b);
void RasterFree(RasterImage &img);
// `shade` multiplie la couleur (éclairage déjà calculé par l'appelant) ;
// sans texture, la couleur de base est gris clair.
void RasterTriangle(RasterImage &img, const RasterVertex v[3], const RasterTexture *tex, float shade);
// Remet l'image à une couleur de fond et vide le tampon de profondeur, sans
// réallouer (une fois par image en temps réel).
void RasterClear(RasterImage &img, uint8 r, uint8 g, uint8 b);
// Triangle en perspective : texture interpolée en u/w, profondeur testée sur
// 1/w (plus grand = plus proche). Le tampon garde −1/w pour rester dans la
// convention « plus petit = plus proche » du triangle orthographique.
// `mode` dit comment la couleur rejoint l'image, d'après la NiAlphaProperty
// de la forme : opaque, test alpha (pixel jeté sous `seuil`), mélange par
// l'alpha de la texture, ou ajout (halos de lumière). Les deux derniers
// testent la profondeur sans l'écrire, et se dessinent après les opaques.
enum eRasterMode { RASTER_OPAQUE = 0, RASTER_TEST, RASTER_MELANGE, RASTER_AJOUT };
void RasterTrianglePersp(RasterImage &img, const RasterPVertex v[3], const RasterTexture *tex, float shade,
                         eRasterMode mode = RASTER_OPAQUE, uint8 seuil = 128);
bool RasterWritePPM(const RasterImage &img, const char *path);
