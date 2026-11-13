// Les tests de collision d'un volume qui se déplace (le balayage), tels que bully.exe les fait : CCollision, lu à l'assembleur.
// Ce sont les pièces du test général 0056c770 (non lu en entier), que la méthode +0xa4 d'un CProjectile (FUN_0046a340,
// CVolProjectile::TesterCollision dans src/objects/Projectiles.h) appelle pour toute entité qui n'est pas un piéton : il prend deux
// modèles de collision, la matrice d'avant et celle d'après le déplacement du premier, celle du second, et cherche la plus petite
// fraction du déplacement à laquelle une sphère du premier touche un volume du second.
//
// Une matrice de bully.exe : quatre lignes de quatre réels, la droite (+0x0), l'avant (+0x10), le haut (+0x20) et la position
// (+0x30) ; le quatrième réel de chaque ligne n'est lu par aucune des fonctions d'ici. `Matrice` lui ajoute le mot +0x40, que le
// produit de deux matrices (FUN_00413780) remet à zéro.
//
// Les calculs : le binaire calcule sur la pile x87 (80 bits) et arrondit à 32 bits à chaque fstp ; ici les produits et les sommes
// passent par des doubles et sont ramenés à float là où le binaire range un réel (fstp DWORD), ce qui suffit aux essais.
#pragma once
#include "../common.h"
#include <cmath>

struct CCollisionBalayee {
	struct Matrice {
		float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
		uint32 o40 = 0;
	};
	// FUN_0045d3b0(sortie, matrice, point) (cdecl, lue à l'assembleur de 0x45d3b0 à 0x45d42f, 128 octets) : le point passé par la
	// matrice, la rotation puis la position : x = (droite.x · p.x + avant.x · p.y) + haut.x · p.z + pos.x, de même en y et en z.
	// Elle écrit 16 octets : le quatrième mot est ce qui traînait sur sa pile ([esp+0xc]), non écrit ici.
	static void Point(float *out, const float *m, const float *p) {
		float x = (float)(((double)m[4] * p[1] + (double)m[0] * p[0]) + (double)m[8] * p[2] + m[12]);
		float y = (float)(((double)m[1] * p[0] + (double)m[5] * p[1]) + (double)m[9] * p[2] + m[13]);
		float z = (float)(((double)m[2] * p[0] + (double)m[6] * p[1]) + (double)m[10] * p[2] + m[14]);
		out[0] = x; out[1] = y; out[2] = z;
	}
	// FUN_00414560(sortie, matrice, vecteur) (cdecl, lue de 0x414560 à 0x4145d5, 118 octets) : la même sans la position, un
	// vecteur tourné ; 16 octets écrits de même.
	static void Tourner(float *out, const float *m, const float *v) {
		float x = (float)(((double)m[4] * v[1] + (double)m[0] * v[0]) + (double)m[8] * v[2]);
		float y = (float)(((double)m[1] * v[0] + (double)m[5] * v[1]) + (double)m[9] * v[2]);
		float z = (float)(((double)m[2] * v[0] + (double)m[6] * v[1]) + (double)m[10] * v[2]);
		out[0] = x; out[1] = y; out[2] = z;
	}
	// FUN_00413620(matrice, sortie) (cdecl, lue de 0x413620 à 0x413774, 341 octets) : l'inverse d'une matrice orthonormée. Les
	// douze réels utiles sont d'abord copiés sur sa pile (la sortie peut être l'entrée) ; la rotation sort transposée ; la position
	// est −(droite · p, avant · p, haut · p), chaque somme partielle arrondie à float (fstp, fld) et partant de 0 (fldz), le
	// signe par le double −1 (0x900550). Les quatrièmes réels des lignes ne sont pas écrits.
	static void Inverser(const float *m, float *out) {
		float s[16];
		for(int i = 0; i < 15; i++) s[i] = m[i];
		out[0] = s[0]; out[1] = s[4]; out[2] = s[8];
		out[4] = s[1]; out[5] = s[5]; out[6] = s[9];
		out[8] = s[2]; out[9] = s[6]; out[10] = s[10];
		float px = s[12], py = s[13], pz = s[14];
		float a = (float)((double)s[0] * px + 0.0), b = (float)(a + (double)s[1] * py), c = (float)(b + (double)s[2] * pz);
		out[12] = (float)(c * -1.0);
		float d = (float)((double)s[4] * px + 0.0), e = (float)(d + (double)py * s[5]), f = (float)(e + (double)pz * s[6]);
		out[13] = (float)(f * -1.0);
		float g = (float)((double)px * s[8] + 0.0), h = (float)((double)py * s[9] + g), k = (float)(h + (double)pz * s[10]);
		out[14] = (float)(-1.0 * k);
	}
	// FUN_00413780(sortie, a, b) (cdecl, lue de 0x413780 à 0x4138c0, 321 octets) : le produit a × b. Le mot +0x40 de la sortie
	// est remis à zéro d'abord ; chaque colonne de b passe par la rotation de a, (a.avant · b1 + a.droite · b0) + a.haut · b2, et la
	// position de a s'ajoute à la quatrième. Les réels sont écrits au fil du calcul, lus directement dans a et b : comme le binaire,
	// une sortie qui serait a ou b changerait le résultat (le test général ne le fait pas).
	static void Multiplier(Matrice *sortie, const float *a, const float *b) {
		sortie->o40 = 0;
		float *o = sortie->m;
		for(int j = 0; j < 4; j++)
			for(int i = 0; i < 3; i++) {
				double v = ((double)a[4 + i] * b[j * 4 + 1] + (double)a[i] * b[j * 4]) + (double)a[8 + i] * b[j * 4 + 2];
				if(j == 3) v += a[12 + i];
				o[j * 4 + i] = (float)v;
			}
	}
	// FUN_0056e530(sortie, rang) (méthode d'un modèle de collision, lue de 0x56e530 à 0x56e580, 81 octets, ret 8) : le sommet de ce
	// rang, lu dans les données du modèle (+0x38), au tableau de leur +0xc : trois entiers de 16 bits (movsx, 6 octets par sommet),
	// chacun fois le double 1/128 (0x90c058).
	static void Sommet(float *out, const int16 *sommets, int rang) {
		const int16 *s = sommets + rang * 3;
		for(int k = 0; k < 3; k++) out[k] = (float)((double)s[k] * 0.0078125);
	}
	// FUN_00576a00(a, b, t, da, db, &fraction, point) (cdecl, lue à l'assembleur de 0x576a00 à 0x576d7d, 894 octets) : deux sphères
	// (le centre en +0x0, le rayon en +0xc) qui se déplacent, a de da et b de db pendant la durée t ; le test général la donne avec
	// t = 1, la fraction est donc celle du déplacement. Avec d = db − da (00414a20, la différence), e = b − a, R = ra + rb :
	//  - un déplacement relatif nul (|d|² ≤ 0) : touché à 0 si |e|² ≤ R², au milieu des deux centres ((a + b) × 0,5, le double
	//    0x8ff1f8) ; sinon rien ;
	//  - sinon d · e > 0 (ou indéfini) : elles s'écartent, rien ; quand le moment le plus proche tombe après t (−t |d|² > d · e, test
	//    ah,0x41 puis jnp), elles ne se touchent pas si R² < |e|² + t (2 d · e + |d|² t) ;
	//  - c = |e|² − R², le discriminant (d · e)² − |d|² c (arrondi à float) négatif : rien ; c ≤ 0 (déjà en contact) : touché à 0, au
	//    milieu ; sinon la fraction −(√|disc| + d · e) / |d|² (FUN_0085c4d0 sur la valeur absolue), ramenée à 0 si elle est négative
	//    et à t si elle dépasse t, et le point de contact sur le segment des deux centres à ce moment, à ra / R du centre de a.
	// Elle rend la fraction et le point, pas de normale (le test général la tire des centres).
	static bool SpheresBalayees(const float *a, const float *b, float t, const float *da, const float *db, float *fraction, float *point) {
		float d[3] = {(float)(db[0] - da[0]), (float)(db[1] - da[1]), (float)(db[2] - da[2])};
		float aa = (float)(((double)d[0] * d[0] + (double)d[1] * d[1]) + (double)d[2] * d[2]);
		float e[3] = {(float)(b[0] - a[0]), (float)(b[1] - a[1]), (float)(b[2] - a[2])};
		float c2 = (float)(((double)e[0] * e[0] + (double)e[1] * e[1]) + (double)e[2] * e[2]);
		float R = (float)((double)b[3] + a[3]);
		float R2 = (float)((double)R * R);
		auto milieu = [&]() {
			*fraction = 0.0f;
			for(int k = 0; k < 3; k++) point[k] = (float)((double)(float)(a[k] + b[k]) * 0.5);
			return true;
		};
		if(!(aa > 0.0f)) {
			if(!(R2 >= c2)) return false;
			return milieu();
		}
		float bb = (float)(((double)e[0] * d[0] + (double)e[1] * d[1]) + (double)e[2] * d[2]);
		if(!(bb <= 0.0f)) return false;
		double mt = -(double)t * aa;
		if(!(mt < bb || mt == bb)) {
			double X = (double)t * ((double)bb + bb + (double)aa * t);
			if(!(R2 >= c2 + X)) return false;
		}
		float c = (float)((double)c2 - R2);
		float disc = (float)((double)bb * bb - (double)aa * c);
		if(!(disc >= 0.0f)) return false;
		if(c <= 0.0f) return milieu();
		float s = (float)std::sqrt(std::fabs((double)disc));
		float t0 = (float)(-((s + (double)bb) / aa));
		*fraction = t0;
		if(t0 < 0.0f) *fraction = 0.0f;
		else if(t < t0) *fraction = t;
		float f = *fraction;
		float w[3], kw[3], p1[3];
		float k = (float)((double)a[3] / R);
		for(int i = 0; i < 3; i++) {
			w[i] = (float)(e[i] + (float)((double)f * d[i]));
			kw[i] = (float)((double)k * w[i]);
			p1[i] = (float)(a[i] + (float)((double)f * da[i]));
			point[i] = (float)(p1[i] + kw[i]);
		}
		return true;
	}
};
