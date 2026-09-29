// Caméra en perspective pour le rendu temps réel. Même repère que le jeu :
// Z vers le haut, angles en radians. `yaw` tourne autour de Z en partant de
// +X vers +Y, `pitch` lève le regard (positif = vers le haut).
//
// Tout ce que le rasteriseur ne sait pas faire est ici : passer du monde à
// l'écran, et découper les triangles qui traversent le plan proche. Sans ce
// découpage, un triangle dont un sommet est derrière l'œil se projette de
// travers sur tout l'écran, et c'est exactement ce qui arrive dès qu'on
// marche à l'intérieur d'un bâtiment.
#pragma once
#include "SoftRaster.h"

struct Camera {
	CVector pos;
	float yaw = 0.0f, pitch = 0.0f;
	float fovY = 1.0472f;           // 60 degrés vertical
	float near_ = 0.05f;            // en mètres, comme le monde du jeu
	int32 w = 400, h = 240;         // écran du haut de la New 3DS

	// Les trois axes de la caméra dans le monde.
	CVector Forward(void) const;
	CVector Right(void) const;
	CVector Up(void) const;

	// Monde → repère caméra : x à droite, y en haut, z profondeur devant l'œil.
	CVector ToView(const CVector &p) const;
	// Repère caméra → écran (z > 0 exigé) : x, y en pixels, invW = 1 / profondeur.
	RasterPVertex Project(const CVector &v, float u, float vv) const;

	// Avance dans le plan horizontal (marche) et sur la verticale.
	void Move(float avant, float droite, float haut);
};

// Un triangle déjà dans le repère caméra, découpé contre le plan proche puis
// projeté. Rend le nombre de triangles écran produits (0, 1 ou 2) dans `out`.
int32 CameraClipProject(const Camera &cam, const CVector view[3], const float uv[6], RasterPVertex out[6]);
