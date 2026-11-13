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
	// La surface de chaque triangle (l'octet +6 d'un triangle des .col, +0x20 d'une boîte, douze fois), quand l'appelant la
	// donne après AjouterTriangle ou AjouterBoite ; un triangle sans surface notée vaut 0. Rien ici ne s'en sert.
	std::vector<uint8> surface;
	uint8 Surface(size_t triangle) const { return triangle < surface.size() ? surface[triangle] : 0; }

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
	// Premier volume que croise le trajet de a à b : la fraction du trajet au
	// point d'entrée (entre 0 et 1) et la normale de la surface, tournée contre
	// le sens du trajet. Les triangles se croisent des deux côtés ; une sphère
	// par son entrée seulement (un départ dedans ne compte pas). Rend faux si
	// rien n'est croisé. `volume` reçoit le rang du triangle touché (dans
	// l'ordre des ajouts, une boîte en comptant douze), ou -1 - le rang de la
	// sphère. C'est le trajet d'un point : le balayage d'un volume par
	// bully.exe n'est pas retrouvé.
	bool Trajet(const CVector &a, const CVector &b, float *fraction, CVector *normale, int32 *volume = nullptr) const;

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
