// Un marcheur dans les collisions du monde : trouver le sol sous les pieds,
// monter une marche, ne pas traverser un mur. Ce n'est pas la physique de
// bully.exe (CWorld::ProcessLineOfSight, CCollision::ProcessColModels, dont
// les dispositions ne sont pas encore retrouvées) : c'est ce qu'il faut pour
// marcher dans une scène avec les volumes de collision du jeu, et le tester
// sans fichier du jeu.
//
// Les volumes sont ceux des .col (triangles, boîtes, sphères), déjà posés dans
// le monde par l'appelant. Les boîtes sont converties en 12 triangles : une
// seule primitive à tester, et c'est ce que CColBox représente de toute façon
// pour un pied qui marche dessus.
#pragma once
#include "../common.h"
#include <vector>

struct CMondeCollision {
	std::vector<CVector> tri;           // 3 sommets par triangle, en coordonnées monde
	std::vector<CVector> sphCentre;
	std::vector<float> sphRayon;

	void AjouterTriangle(const CVector &a, const CVector &b, const CVector &c);
	// Boîte déjà transformée : ses 8 coins dans l'ordre des bits (x, y, z).
	void AjouterBoite(const CVector coins[8]);
	void AjouterSphere(const CVector &c, float r);

	// Plus haute surface touchée par la verticale en (x, y), entre zHaut et
	// zHaut − profondeur. Rend faux si rien n'est touché.
	bool Sol(float x, float y, float zHaut, float profondeur, float *z) const;
	// Repousse une sphère hors des volumes, seulement à l'horizontale (on ne se
	// fait pas soulever par un mur) ; rend le nombre de contacts.
	int32 Repousser(CVector &centre, float rayon) const;

	// Grille en plan (x, y) des triangles : chaque case liste ceux dont la
	// boîte la touche. Sol et Repousser ne testent plus que les triangles des
	// cases concernées, pris dans leur ordre d'origine : même résultat, au
	// lieu de parcourir tout le monde (25 000 triangles dehors) à chaque pas.
	// À refaire après tout ajout de triangles ; sans elle, tout est testé.
	void Indexer(float cellule = 4.0f);

	float gx0 = 0, gy0 = 0, gCellule = 0;
	int32 gNx = 0, gNy = 0;
	std::vector<int32> gDebut, gListe;                    // cases → plage dans gListe (index de triangle)
	mutable std::vector<uint32> gVu; mutable uint32 gTour = 0;
	// Triangles (index du premier sommet) dont la boîte en plan touche le
	// rectangle, triés dans l'ordre d'origine.
	void Candidats(float x0, float y0, float x1, float y1, std::vector<int32> &out) const;
};

// Le corps qui marche : pieds en `pos`, yeux à `hauteurYeux` au-dessus.
struct CMarcheur {
	CVector pos;
	float vz = 0.0f;                    // vitesse verticale, m/s
	bool auSol = false;
	float rayon = 0.3f;
	float hauteurYeux = 1.6f;
	float marche = 0.45f;               // plus haute marche qu'on monte sans sauter

	// Avance de (dx, dy) en `dt` secondes : murs, puis sol et gravité.
	void Avancer(const CMondeCollision &m, float dx, float dy, float dt);
};
