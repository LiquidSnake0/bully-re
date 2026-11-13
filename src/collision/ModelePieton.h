// Le modèle de collision d'un piéton, tel que bully.exe le refait avant chaque test fin d'un projectile contre un piéton
// (FUN_00469f70, CCollisionBalayee::TestPieton dans Balayes.h) : six sphères posées sur six os de son squelette, lues à
// l'assembleur dans FUN_00572680 et ses aides. Le modèle est unique (le mot 0xcf3138, ses six sphères au mot 0xcf3160) : il est
// refait pour le piéton que l'on teste, dans le repère de ce piéton, et la matrice du piéton (le m4 du test fin) le repose dans
// le monde.
//
// Les six os et les six rayons viennent de la fiche FightCollisionButes du piéton (Globals.cat) : le rang de l'os de la sphère
// k en +0x18 + 8k, son rayon en +0x1c + 8k. Les rangs sont ceux des os du squelette (les 36 os de MAINPED.HXD, docs/agr.md).
//
// Les calculs : comme dans Balayes.h, des doubles ramenés à float là où le binaire range un réel (fstp DWORD).
#pragma once
#include "Balayes.h"

struct CModelePieton {
	typedef CCollisionBalayee::SphereCol SphereCol;
	// La fiche FightCollisionButes : les mots +0x18, +0x20, +0x28, +0x30, +0x38, +0x40 (les rangs des os) et les réels +0x1c,
	// +0x24, +0x2c, +0x34, +0x3c, +0x44 (les rayons).
	struct Fiche {
		int32 os[6] = {0, 0, 0, 0, 0, 0};
		float rayon[6] = {0, 0, 0, 0, 0, 0};
	};
	float centre[3] = {0, 0, 0};   // +0x0 du modèle : le centre de la sphère qui l'englobe
	float rayon = 0;               // +0xc : son rayon
	float min[3] = {0, 0, 0};      // +0x10 : la boîte qui l'englobe
	float max[3] = {0, 0, 0};      // +0x20
	SphereCol spheres[6] = {};     // le mot 0xcf3160 : six sphères de 0x14 octets, le centre puis le rayon en +0xc
	int8 nSpheres = 6;             // pour TestPieton, qui parcourt les sphères du modèle

	// FUN_00412670(vecteur) (méthode d'une matrice, ret 4, lue à l'assembleur de 0x412670 à 0x4126ab, 60 octets) : l'identité,
	// puis les quatre mots du vecteur en +0x30 : une matrice qui ne fait que déplacer.
	static void Posee(float *m, const float *v) {
		static const float identite[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
		memcpy(m, identite, sizeof(identite));
		memcpy(m + 12, v, 4 * sizeof(float));
	}
	// FUN_004130d0(angle) (méthode d'une matrice, ret 4, lue de 0x4130d0 à 0x4131c5, 246 octets) : la matrice tournée autour de
	// z. Le cosinus et le sinus de l'angle sont arrondis à float, puis chacune des quatre lignes (la position comprise) :
	// x' = c x − s y, y' = c y + s x.
	static void TournerZ(float *m, float angle) {
		float c = (float)cos((double)angle), s = (float)sin((double)angle);
		for(int i = 0; i < 16; i += 4) {
			float x = m[i], y = m[i + 1];
			m[i] = (float)((double)c * x - (double)s * y);
			m[i + 1] = (float)((double)c * y + (double)s * x);
		}
	}
	// FUN_00571280(source) (méthode d'une matrice, ret 4, lue de 0x571280 à 0x5712e6, 103 octets) : la copie des seize réels.
	static void Copier(float *m, const float *source) {
		for(int i = 0; i < 16; i++) m[i] = source[i];
	}
	// FUN_00570e40(sortie, compte, entree) (méthode d'une matrice, ret 0xc, lue de 0x570e40 à 0x5710e7, 680 octets) : `compte`
	// points de 16 octets passés par la matrice, quatre par tour puis le reste un par un, avec la formule de FUN_0045d3b0
	// (CCollisionBalayee::Point) ; le quatrième mot de chaque point de la sortie est un mot de la pile, non écrit ici.
	static void Points(const float *m, float (*sortie)[4], int32 compte, const float (*entree)[4]) {
		for(int32 i = 0; i < compte; i++) {
			const float *p = entree[i];
			float x = (float)(((double)p[1] * m[4] + (double)p[0] * m[0]) + (double)p[2] * m[8] + m[12]);
			float y = (float)(((double)p[1] * m[5] + (double)p[0] * m[1]) + (double)m[9] * p[2] + m[13]);
			float z = (float)(((double)p[0] * m[2] + (double)p[1] * m[6]) + (double)p[2] * m[10] + m[14]);
			sortie[i][0] = x; sortie[i][1] = y; sortie[i][2] = z;
		}
	}
	// FUN_0056ea30(rayon, centre) (méthode de la sphère englobante d'un modèle, ret 8, lue de 0x56ea30 à 0x56ea4d, 30 octets) :
	// le rayon en +0xc, le centre en +0.
	void Englobante(float r, const float *c) {
		rayon = r;
		centre[0] = c[0]; centre[1] = c[1]; centre[2] = c[2];
	}
	// FUN_005646e0(min, max) (méthode de la boîte englobante, le +0x10 du modèle, ret 8, lue de 0x5646e0 à 0x56474b, 108
	// octets) : le coin bas en +0, le coin haut en +0x10 (quatre mots chacun, le quatrième non initialisé chez l'appelant).
	void Boite(const float *bas, const float *haut) {
		for(int i = 0; i < 3; i++) { min[i] = bas[i]; max[i] = haut[i]; }
	}
	// Le cap d'un piéton qui a une matrice, comme FUN_00572680 le prend de 0x572714 à 0x572731 : atan2(−avant.x, avant.y),
	// arrondi à float ; l'avant d'un piéton de cap a est (−sin a, cos a, 0).
	static float Cap(const float *matrice) {
		return (float)atan2(-(double)matrice[4], (double)matrice[5]);
	}
	// FUN_00572680(piéton) (cdecl, lue à l'assembleur de 0x572680 à 0x572c78, 1 529 octets). `pos` est la position du piéton (le
	// +0x30 de sa matrice, ou son +4 sans matrice), `cap` son cap (Cap(matrice), ou son +0x10 sans matrice), `os(rang, p)` rend
	// la position dans le monde de l'os de ce rang (le +0x24 du NiTransform de 0x34 octets de l'os, dans la table
	// [[squelette + 0x14] + 8], le squelette étant le +0x114 du piéton), `fiche` sa fiche FightCollisionButes (celle du chemin
	// « Fighting », nom du modèle, à défaut DAT_020c88a0).
	//  - la position fois −1 (le double 0x900550), une matrice qui déplace d'autant (FUN_00412670), tournée de −cap autour de z
	//    (FUN_004130d0), copiée (FUN_00571280) : la matrice qui ramène le monde au repère du piéton ;
	//  - les positions des six os, passées par cette matrice (FUN_00570e40), sont les centres des six sphères ;
	//  - le carré de la distance de chaque centre à l'origine, (x² + y²) + z² arrondi à float ; le plus grand est gardé avec le
	//    rang de sa sphère : pour la sphère 0, il faut d > 0 (sinon le rang devient les 32 bits de d : 0 pour une distance nulle,
	//    et le plus grand reste 0) ; pour les suivantes, il faut strictement plus que le plus grand ;
	//  - les six rayons de la fiche vont en +0xc de chaque sphère ; les octets +0x10 et +0x11 des sphères (la surface et la pièce)
	//    ne sont pas écrits : ils restent ceux de la mise en place, non lue (nuls, supposé) ;
	//  - R = racine(|le plus grand|), arrondie à float, plus le rayon de la sphère de ce rang : la sphère englobante a pour
	//    centre l'origine et pour rayon R (FUN_0056ea30), la boîte va de (−R, −R, 0) à (R, R, R) (FUN_005646e0) : le repère d'un
	//    piéton a son origine au sol.
	// Le binaire lit la sphère du rang sans garde ; ici un rang hors des six sphères (une distance qui n'est pas un nombre) donne
	// un rayon nul (une garde de l'hôte).
	template<class OS>
	void Construire(const float *pos, float cap, const Fiche &fiche, OS &&os) {
		float v[4] = {(float)((double)pos[0] * -1.0), (float)((double)pos[1] * -1.0), (float)((double)pos[2] * -1.0), 0.0f};
		float m[16], ramene[16];
		Posee(m, v);
		TournerZ(m, -cap);
		Copier(ramene, m);
		float monde[6][4], locaux[6][4];
		for(int k = 0; k < 6; k++) { monde[k][3] = 0.0f; os(fiche.os[k], monde[k]); }
		Points(ramene, locaux, 6, monde);
		float grand = 0.0f;
		uint32 rang = 0;
		for(int k = 0; k < 6; k++) {
			float *c = spheres[k].centre;
			c[0] = locaux[k][0]; c[1] = locaux[k][1]; c[2] = locaux[k][2];
			float d = (float)(((double)c[0] * c[0] + (double)c[1] * c[1]) + (double)c[2] * c[2]);
			if(k == 0) {
				if(d > 0.0f) { grand = d; rang = 0; }
				else memcpy(&rang, &d, sizeof(rang));
			} else if(grand < d) { grand = d; rang = (uint32)k; }
		}
		for(int k = 0; k < 6; k++) spheres[k].rayon = fiche.rayon[k];
		float racine = (float)sqrt((double)fabsf(grand));
		float r = (float)((double)racine + (rang < 6 ? spheres[rang].rayon : 0.0f));
		static const float origine[3] = {0.0f, 0.0f, 0.0f};
		Englobante(r, origine);
		float bas[3] = {-r, -r, 0.0f}, haut[3] = {r, r, r};
		Boite(bas, haut);
	}
};
