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
#include <cstring>

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
	// FUN_00414ad0(a, b) (cdecl, lue de 0x414ad0 à 0x414b2b, 92 octets) : a / √|b|, la racine arrondie à float ; une racine nulle
	// (fucom, test ah,0x44) rend FLT_MAX (0x9009b8) si a ≥ 0 et −FLT_MAX (0x9009bc) si a < 0.
	static float DiviserParRacine(float a, float b) {
		float r = (float)std::sqrt(std::fabs((double)b));
		if(r == 0.0f) return a < 0.0f ? -3.4028234663852886e+38f : 3.4028234663852886e+38f;
		return (float)((double)a / r);
	}
	// FUN_00414cc0 (méthode d'un vecteur, lue de 0x414cc0 à 0x414d1c, 93 octets) : le vecteur multiplié par 1 / √((x² + y²) + z²)
	// (FUN_00414ad0 avec 1) ; un vecteur nul le reste (FLT_MAX fois 0). Elle rend le facteur sur la pile x87.
	static float Normaliser(float *v) {
		float n2 = (float)(((double)v[0] * v[0] + (double)v[1] * v[1]) + (double)v[2] * v[2]);
		float k = DiviserParRacine(1.0f, n2);
		v[0] = (float)((double)v[0] * k); v[1] = (float)((double)k * v[1]); v[2] = (float)((double)v[2] * k);
		return k;
	}
	static double ScalaireD(const float *a, const float *b) { return ((double)a[1] * b[1] + (double)a[0] * b[0]) + (double)a[2] * b[2]; }
	static float Scalaire(const float *a, const float *b) { return (float)ScalaireD(a, b); }
	// FUN_00574fe0(p, a, b, pied) (cdecl, lue de 0x574fe0 à 0x5750f8, 281 octets) : le carré de la distance de p à la droite (a, b),
	// sans borne : v = p − a, u = b − a, k = (v · u) / |u|² (arrondi à float), w = k u ; le pied a + w est écrit si `pied` n'est pas
	// nul ; elle rend |v − w|².
	static float Distance2ADroite(const float *p, const float *a, const float *b, float *pied) {
		float v[3], u[3], w[3], r[3];
		for(int i = 0; i < 3; i++) { v[i] = (float)(p[i] - a[i]); u[i] = (float)(b[i] - a[i]); }
		float d = (float)(((double)v[1] * u[1] + (double)v[0] * u[0]) + (double)v[2] * u[2]);
		float l2 = (float)(((double)u[0] * u[0] + (double)u[1] * u[1]) + (double)u[2] * u[2]);
		float k = (float)((double)d / l2);
		for(int i = 0; i < 3; i++) w[i] = (float)((double)k * u[i]);
		if(pied) for(int i = 0; i < 3; i++) pied[i] = (float)(a[i] + w[i]);
		for(int i = 0; i < 3; i++) r[i] = (float)(v[i] - w[i]);
		return (float)(((double)r[0] * r[0] + (double)r[1] * r[1]) + (double)r[2] * r[2]);
	}
	// FUN_00575100(p, a, b, pied) (cdecl, lue de 0x575100 à 0x575132, 51 octets) : la distance, √|FUN_00574fe0|.
	static float DistanceADroite(const float *p, const float *a, const float *b, float *pied) {
		return (float)std::sqrt(std::fabs((double)Distance2ADroite(p, a, b, pied)));
	}
	// Un plan : la normale en +0x0, d en +0xc ; un point x est à la distance n · x + d.
	struct Plan { float n[3]; float d; };
	// FUN_00575140(point, normale) (méthode d'un plan, lue de 0x575140 à 0x57519c, 95 octets, ret 8) : la normale copiée et
	// normalisée (FUN_00414cc0), d = −(n · point).
	static void PlanParPoint(Plan *pl, const float *point, const float *normale) {
		float n[3] = {normale[0], normale[1], normale[2]};
		Normaliser(n);
		pl->n[0] = n[0]; pl->n[1] = n[1]; pl->n[2] = n[2];
		pl->d = (float)-(((double)point[1] * n[1] + (double)point[0] * n[0]) + (double)n[2] * point[2]);
	}
	// FUN_005751a0(p0, p1, p2) (méthode d'un plan, lue de 0x5751a0 à 0x575253, 182 octets, ret 0xc) : le plan des trois points, de
	// normale w × u, u = p0 − p1 et w = p2 − p1 (00414a20), normalisée, d = −(n · p0).
	static void PlanParTroisPoints(Plan *pl, const float *p0, const float *p1, const float *p2) {
		float u[3], w[3];
		for(int i = 0; i < 3; i++) { u[i] = (float)(p0[i] - p1[i]); w[i] = (float)(p2[i] - p1[i]); }
		float n[3] = {(float)((double)u[2] * w[1] - (double)u[1] * w[2]), (float)((double)w[2] * u[0] - (double)u[2] * w[0]),
		              (float)((double)w[0] * u[1] - (double)u[0] * w[1])};
		Normaliser(n);
		pl->n[0] = n[0]; pl->n[1] = n[1]; pl->n[2] = n[2];
		pl->d = (float)-(((double)p0[1] * n[1] + (double)p0[0] * n[0]) + (double)n[2] * p0[2]);
	}
	// FUN_00575260(a, b, c, d, &t) (cdecl, lue de 0x575260 à 0x575307, 168 octets) : en deux dimensions (x et y seuls), le paramètre
	// t du point a + t (b − a) où la droite (a, b) coupe la droite (c, d) : t = ((c − a) × (c − d)) / ((b − a) × (c − d)). Un
	// déterminant nul (fucom, test ah,0x44) : faux, deux droites parallèles ; un `t` nul n'est pas écrit.
	static bool Droites2D(const float *a, const float *b, const float *c, const float *d, float *t) {
		float ex = (float)(b[0] - a[0]), ey = (float)(b[1] - a[1]), fx = (float)(c[0] - d[0]), fy = (float)(c[1] - d[1]);
		float det = (float)((double)ex * fy - (double)ey * fx);
		if(det == 0.0f) return false;
		if(t) {
			float num = (float)((double)fx * ((double)a[1] - c[1]) - (double)fy * ((double)a[0] - c[0]));
			*t = (float)((double)num / det);
		}
		return true;
	}
	// FUN_00575310(sphère, p0, p1, &compte, &t1, &t2) (cdecl, lue de 0x575310 à 0x57550d, 510 octets) : la droite p0 + t (p1 − p0)
	// contre une sphère (le centre en +0x0, le rayon en +0xc) : A = |p1 − p0|², B = 2 (p1 − p0) · (p0 − c), C = |c|² + |p0|²
	// − 2 c · p0 − r² (les doubles 2 et 4 en 0x900980 et 0x900970) ; le discriminant B² − 4AC (arrondi à float) négatif : faux ;
	// nul : une racine, −B / 2A dans t1 (compte 1) ; sinon (√|disc| − B) / 2A dans t1 et (−B − √|disc|) / 2A dans t2 (compte 2).
	// Les pointeurs nuls ne sont pas écrits.
	static bool DroiteSphere(const float *s, const float *p0, const float *p1, int32 *compte, float *t1, float *t2) {
		float dx = (float)(p1[0] - p0[0]), dy = (float)(p1[1] - p0[1]), dz = (float)(p1[2] - p0[2]);
		float A = (float)(((double)dx * dx + (double)dy * dy) + (double)dz * dz);
		float B = (float)((((double)dy * ((double)p0[1] - s[1]) + (double)dx * ((double)p0[0] - s[0])) + (double)dz * ((double)p0[2] - s[2])) * 2.0);
		double c1 = (((((double)s[0] * s[0] + (double)s[1] * s[1]) + (double)s[2] * s[2]) + (double)p0[0] * p0[0]) + (double)p0[1] * p0[1]) + (double)p0[2] * p0[2];
		double sp = ((double)s[1] * p0[1] + (double)s[0] * p0[0]) + (double)s[2] * p0[2];
		float C = (float)((c1 - 2.0 * sp) - (double)s[3] * s[3]);
		double B2 = (double)B * B, AC4 = 4.0 * A * C;
		float disc = (float)(B2 - AC4);
		if(disc < 0.0f) return false;
		if(disc == 0.0f) {
			if(compte) *compte = 1;
			if(t1) *t1 = (float)(-(double)B / (2.0 * A));
			return true;
		}
		if(compte) *compte = 2;
		if(t1) {
			float r = (float)std::sqrt(std::fabs((double)(float)(B2 - AC4)));
			*t1 = (float)((r - (double)B) / ((double)A + A));
		}
		if(t2) {
			float mb = -B;
			float r = (float)std::sqrt(std::fabs((double)(float)(B2 - AC4)));
			*t2 = (float)((mb - (double)r) / ((double)A + A));
		}
		return true;
	}
	// FUN_00574dc0(a, b, c, p) (cdecl, lue de 0x574dc0 à 0x574f85, 454 octets) : p (supposé dans le plan) est-il dans le triangle,
	// par ses coordonnées barycentriques : v0 = b − a, v1 = c − a, v2 = p − a et leurs produits scalaires ; den = d01² − d00 d11
	// (arrondi à float) ; |den| < FLT_EPSILON (le réel 0x90d198) : faux ; u = (d12 d01 − d11 d02) / den et v = (d01 d02 − d00 d12)
	// / den ; vrai si u est dans [0, 1], v ≥ 0 et u + v ≤ 1 (le double 1 en 0x8ff1d8).
	static bool DansTriangle(const float *a, const float *b, const float *c, const float *p) {
		float v0[3], v1[3], v2[3];
		for(int i = 0; i < 3; i++) { v0[i] = (float)(b[i] - a[i]); v1[i] = (float)(c[i] - a[i]); v2[i] = (float)(p[i] - a[i]); }
		float d00 = (float)(((double)v0[0] * v0[0] + (double)v0[1] * v0[1]) + (double)v0[2] * v0[2]);
		float d01 = (float)(((double)v1[0] * v0[0] + (double)v1[1] * v0[1]) + (double)v1[2] * v0[2]);
		float d11 = (float)(((double)v1[1] * v1[1] + (double)v1[0] * v1[0]) + (double)v1[2] * v1[2]);
		float d02 = (float)(((double)v0[0] * v2[0] + (double)v0[1] * v2[1]) + (double)v0[2] * v2[2]);
		float d12 = (float)(((double)v1[0] * v2[0] + (double)v1[1] * v2[1]) + (double)v1[2] * v2[2]);
		float den = (float)((double)d01 * d01 - (double)d00 * d11);
		if(std::fabs(den) < 1.1920928955078125e-07f) return false;
		float inv = (float)(1.0 / den);
		float u = (float)(((double)d12 * d01 - (double)d11 * d02) * inv);
		if(u < 0.0f || 1.0f < u) return false;
		float v = (float)(((double)d01 * d02 - (double)d00 * d12) * inv);
		if(v < 0.0f) return false;
		return !((double)u + v > 1.0);
	}
	// FUN_00575510(sommets, normale, sphère, déplacement, &fraction, normaleContact, pointContact, &face) (cdecl, lue à l'assembleur
	// de 0x575510 à 0x575dac, 2 205 octets) : une sphère qui se déplace contre un triangle (trois pointeurs vers ses sommets, sa
	// normale). Le déplacement est d'abord normalisé (FUN_00414cc0 sur une copie) : la fraction rendue est une distance le long
	// du déplacement, et c'est l'appelant qui la compare à sa longueur. Dans l'ordre :
	//  - le triangle n'est testé que de face : faux si la direction · la normale > −0,001 (le réel 0x914638) ; faux aussi si le
	//    centre est derrière son plan (FUN_00575140 par le premier sommet) de plus d'un rayon (dist < −r) ;
	//  - la fraction part de FLT_MAX ; la face, si le centre est devant le plan de plus d'un rayon (r < dist, strictement) :
	//    t = −(dist − r) / (direction · normale), le point c + t dir − r n ; dans le triangle (FUN_00574dc0) et t < fraction : la
	//    fraction, la normale du triangle, le point, et l'octet `face` à 1 ;
	//  - les trois sommets : le trajet inverse du sommet, de v à v − dir, contre la sphère de départ (FUN_00575310) ; le plus petit
	//    des deux t (t2 seulement s'il est strictement plus petit), s'il est ≥ 0 et < fraction : la fraction, la normale c − (v
	//    − t dir), le point le sommet, `face` à 0 ;
	//  - les trois arêtes (a, b) = (s0, s1), (s1, s2), (s2, s0) : le plan de l'arête et de la direction (FUN_005751a0 avec a, b et
	//    b − dir) ; si le centre en est à au plus un rayon (−r ≤ dist ≤ r) : le cercle où la sphère coupe ce plan, de centre
	//    c' = c − dist n et de rayon h = √|r² − dist²| ; e, le point de ce cercle le plus proche de la droite de l'arête (c' + h ×
	//    la direction normalisée de c' vers son pied sur la droite, FUN_00575100) ; en deux dimensions (le plan des deux axes où la
	//    normale du plan est la plus petite : x est écarté s'il est strictement le plus grand, sinon z si |ny| ≤ |nz|, sinon y),
	//    le t où la droite (e, e + dir) coupe la droite de l'arête (FUN_00575260) ; t ≥ 0, le point g = e + t dir entre a et b
	//    ((a − g) · (b − g) ≤ 0, le double nul 0x8ff1f0) et t ≤ fraction (ici l'égalité suffit) : la fraction, la normale c − e,
	//    le point g, `face` à 0.
	// Touché, la normale du contact est normalisée (FUN_00414cc0) ; elle rend vrai si l'une des trois parties a touché.
	// Le corps est écrit pour un polygone de N sommets (`SpherePolygone`) : FUN_00575510 le fait avec 3, FUN_00575db0 avec 4.
	static bool SphereTriangle(const float *const *sommets, const float *normale, const float *s, const float *deplacement, float *fraction,
	                           float *normaleContact, float *pointContact, uint8 *face) {
		return SpherePolygone<3>(sommets, normale, s, deplacement, fraction, normaleContact, pointContact, face, false,
		                         [&](const float *p) { return DansTriangle(sommets[0], sommets[1], sommets[2], p); });
	}
	template<int N, class D>
	static bool SpherePolygone(const float *const *sommets, const float *normale, const float *s, const float *deplacement, float *fraction,
	                           float *normaleContact, float *pointContact, uint8 *face, bool fractionDAbord, D &&dedans) {
		float dir[3] = {deplacement[0], deplacement[1], deplacement[2]};
		Normaliser(dir);
		float dn = Scalaire(dir, normale);
		if(dn > -0.001f) return false;
		Plan pl;
		PlanParPoint(&pl, sommets[0], normale);
		float dist = (float)(ScalaireD(pl.n, s) + pl.d);
		if(fractionDAbord) *fraction = 3.4028234663852886e+38f;
		if(-s[3] > dist) return false;
		*fraction = 3.4028234663852886e+38f;
		int genre = -1;
		if(s[3] < dist) {
			float nd = Scalaire(dir, normale);
			if(nd != 0.0f) {
				float ecart = (float)(dist - s[3]);
				float t = (float)(-(double)ecart / nd);
				float mr = -s[3];
				float p[3];
				for(int i = 0; i < 3; i++) p[i] = (float)((float)(s[i] + (float)((double)t * dir[i])) + (float)((double)mr * normale[i]));
				if(dedans(p) && !(*fraction <= t)) {
					*fraction = t;
					for(int i = 0; i < 3; i++) { normaleContact[i] = normale[i]; pointContact[i] = p[i]; }
					*face = 1;
					genre = 0;
				}
			}
		}
		for(int k = 0; k < N; k++) {
			const float *v = sommets[k];
			float q[3] = {(float)(v[0] - dir[0]), (float)(v[1] - dir[1]), (float)(v[2] - dir[2])};
			float t1 = 3.4028234663852886e+38f, t2 = 3.4028234663852886e+38f;
			int32 n;
			if(!DroiteSphere(s, v, q, &n, &t1, &t2)) continue;
			float t = t1;
			if(t2 < t1) t = t2;
			if(t < 0.0f || *fraction <= t) continue;
			*fraction = t;
			float p[3];
			for(int i = 0; i < 3; i++) {
				float w = (float)(q[i] - v[i]);
				p[i] = (float)(v[i] + (float)((double)t * w));
				normaleContact[i] = (float)(s[i] - p[i]);
				pointContact[i] = v[i];
			}
			*face = 0;
			genre = 1;
		}
		for(int k = 0; k < N; k++) {
			const float *a = sommets[k], *b = sommets[k == N - 1 ? 0 : k + 1];
			float bd[3] = {(float)(b[0] - dir[0]), (float)(b[1] - dir[1]), (float)(b[2] - dir[2])};
			Plan pa;
			PlanParTroisPoints(&pa, a, b, bd);
			float de = (float)((double)s[0] * pa.n[0] + (double)pa.n[1] * s[1] + (double)pa.n[2] * s[2] + pa.d);
			if(s[3] < de || -s[3] > de) continue;
			float r2 = (float)((double)s[3] * s[3]);
			float h = (float)std::sqrt(std::fabs((double)(float)(r2 - (double)de * de)));
			float c[3], pied[3] = {0.0f, 0.0f, 0.0f}, g[3], e[3], f[3];
			for(int i = 0; i < 3; i++) c[i] = (float)(s[i] - (double)de * pa.n[i]);
			DistanceADroite(c, a, b, pied);
			for(int i = 0; i < 3; i++) g[i] = (float)(pied[i] - c[i]);
			Normaliser(g);
			for(int i = 0; i < 3; i++) { e[i] = (float)((float)((double)g[i] * h) + c[i]); f[i] = (float)(e[i] + dir[i]); }
			float ax = std::fabs(pa.n[0]), ay = std::fabs(pa.n[1]), az = std::fabs(pa.n[2]);
			int i1 = 0, i2 = 1;
			if(!(ay >= ax) && !(az >= ax)) { i1 = 1; i2 = 2; }
			else if(!(ay < az || ay == az)) i2 = 2;
			float e2[2] = {e[i1], e[i2]}, f2[2] = {f[i1], f[i2]}, a2[2] = {a[i1], a[i2]}, b2[2] = {b[i1], b[i2]};
			float t;
			if(!Droites2D(e2, f2, a2, b2, &t)) continue;
			if(t < 0.0f) continue;
			float gp[3], ga[3], gb[3];
			for(int i = 0; i < 3; i++) { gp[i] = (float)(e[i] + (float)((double)t * dir[i])); ga[i] = (float)(a[i] - gp[i]); gb[i] = (float)(b[i] - gp[i]); }
			float pr = (float)(((double)gb[0] * ga[0] + (double)ga[1] * gb[1]) + (double)ga[2] * gb[2]);
			if(pr > 0.0f) continue;
			if(*fraction < t) continue;
			*fraction = t;
			for(int i = 0; i < 3; i++) { normaleContact[i] = (float)(s[i] - e[i]); pointContact[i] = gp[i]; }
			*face = 0;
			genre = 2;
		}
		if(genre != -1) Normaliser(normaleContact);
		return genre != -1;
	}
	// FUN_00574f90(n, coins, p) (cdecl, lue de 0x574f90 à 0x574fd6, 71 octets) : p est-il dans le polygone de n coins rangés à la
	// suite (12 octets chacun), par l'éventail (c0, c1, c2), (c0, c2, c3)… : le premier triangle qui le contient (FUN_00574dc0)
	// suffit ; n − 2 ≤ 0 : faux.
	static bool DansPolygone(int n, const float *coins, const float *p) {
		for(int i = 0; i < n - 2; i++)
			if(DansTriangle(coins, coins + 3 * (i + 1), coins + 3 * (i + 2), p)) return true;
		return false;
	}
	// FUN_00575db0(coins, normale, sphère, déplacement, &fraction, normaleContact, pointContact, &face) (cdecl, 2 214 octets, de
	// 0x575db0 à 0x5766b1) : la même chose que FUN_00575510 pour une face à quatre coins rangés à la suite. Son assembleur a été lu
	// en regard de celui de 00575510 : les mêmes comparaisons, dans le même ordre (le seuil −0,001, −r ≤ dist, r < dist strict
	// pour la face, t < fraction pour la face et les sommets, t ≤ fraction pour les arêtes, les mêmes choix d'axes) et les mêmes
	// appels ; les différences : la fraction passe à FLT_MAX avant le test du plan (un centre derrière la face de plus d'un rayon
	// la laisse donc à FLT_MAX), le plan passe par le premier coin, le point de la face est cherché dans le quadrilatère
	// (FUN_00574f90 avec 4), et les boucles des sommets et des arêtes font quatre tours ((c3, c0) pour la dernière arête).
	static bool SphereQuadrilatere(const float *coins, const float *normale, const float *s, const float *deplacement, float *fraction,
	                               float *normaleContact, float *pointContact, uint8 *face) {
		const float *sommets[4] = {coins, coins + 3, coins + 6, coins + 9};
		return SpherePolygone<4>(sommets, normale, s, deplacement, fraction, normaleContact, pointContact, face, true,
		                         [&](const float *p) { return DansPolygone(4, coins, p); });
	}
	// FUN_00576660(min, max, sphère, déplacement, &fraction, normaleContact, pointContact, &face, &compte) (cdecl, lue à
	// l'assembleur de 0x576660 à 0x5769e3, 900 octets) : une sphère qui se déplace contre une boîte alignée sur les axes (son min et
	// son max), face par face. La fraction part de FLT_MAX ; une table de six sauts (0x5769e4) pose chaque face, sa normale (−1 en
	// 0x9009ac, 0 ou 1) et ses quatre coins : −x, −y, −z, puis +x, +y, +z ; FUN_00575db0 sur chacune, et une distance strictement
	// plus petite que la fraction la remplace, avec la normale et le point, et le compte monte de 1 (le compte que le test général
	// tient pour la sphère). Elle rend vrai si une face a touché ; l'octet `face` est celui de la dernière face essayée.
	static bool BoiteBalayee(const float *mn, const float *mx, const float *s, const float *deplacement, float *fraction,
	                         float *normaleContact, float *pointContact, uint8 *face, int32 *compte) {
		*fraction = 3.4028234663852886e+38f;
		bool touche = false;
		for(int k = 0; k < 6; k++) {
			float n[3] = {0, 0, 0}, c[12];
			auto coin = [&](int i, float x, float y, float z) { c[3 * i] = x; c[3 * i + 1] = y; c[3 * i + 2] = z; };
			switch(k) {
			case 0: n[0] = -1; coin(0, mn[0], mx[1], mx[2]); coin(1, mn[0], mn[1], mx[2]); coin(2, mn[0], mn[1], mn[2]); coin(3, mn[0], mx[1], mn[2]); break;
			case 1: n[1] = -1; coin(0, mx[0], mn[1], mx[2]); coin(1, mn[0], mn[1], mx[2]); coin(2, mn[0], mn[1], mn[2]); coin(3, mx[0], mn[1], mn[2]); break;
			case 2: n[2] = -1; coin(0, mx[0], mx[1], mn[2]); coin(1, mn[0], mx[1], mn[2]); coin(2, mn[0], mn[1], mn[2]); coin(3, mx[0], mn[1], mn[2]); break;
			case 3: n[0] = 1; coin(0, mx[0], mx[1], mx[2]); coin(1, mx[0], mn[1], mx[2]); coin(2, mx[0], mn[1], mn[2]); coin(3, mx[0], mx[1], mn[2]); break;
			case 4: n[1] = 1; coin(0, mx[0], mx[1], mx[2]); coin(1, mn[0], mx[1], mx[2]); coin(2, mn[0], mx[1], mn[2]); coin(3, mx[0], mx[1], mn[2]); break;
			default: n[2] = 1; coin(0, mx[0], mx[1], mx[2]); coin(1, mn[0], mx[1], mx[2]); coin(2, mn[0], mn[1], mx[2]); coin(3, mx[0], mn[1], mx[2]); break;
			}
			float f = 3.4028234663852886e+38f, nc[3], pc[3];
			if(SphereQuadrilatere(c, n, s, deplacement, &f, nc, pc, face) && f < *fraction) {
				*fraction = f;
				for(int i = 0; i < 3; i++) { normaleContact[i] = nc[i]; pointContact[i] = pc[i]; }
				++*compte;
				touche = true;
			}
		}
		return touche;
	}
	// Les données d'un modèle de collision (le bloc que pointe le +0x38 d'un CColModel), telles que le test général les lit :
	// les sphères en +0x0 (0x14 octets : le centre, le rayon en +0xc, la surface en +0x10, un octet en +0x11 que l'argument 12
	// du test général fait sauter, le mot de 16 bits en +0x10 copié tel quel dans le contact), les boîtes en +0x8 (0x24 octets : le
	// min, le max en +0x10, le mot +0x20 copié dans le contact), les sommets en +0xc (FUN_0056e530), les triangles en +0x10 (8
	// octets : trois rangs de sommets sur 16 bits, la surface en +6), un arbre en +0x14 (non nul : les triangles candidats sont
	// demandés à 005616a0 puis 00561750, non lues), le nombre de triangles en +0x1c (16 bits, signé), de sphères en +0x1e et de
	// boîtes en +0x1f (8 bits, signés). Sur l'hôte, des pointeurs ordinaires ; `Sommet` lit les sommets compressés.
	struct SphereCol { float centre[3]; float rayon; uint8 surface, o11; uint8 pad[2]; uint16 Mot(void) const { return (uint16)(surface | o11 << 8); } };
	struct BoiteCol { float min[3]; float pad0; float max[3]; float pad1; uint8 surface, o21; uint8 pad[2]; uint16 Mot(void) const { return (uint16)(surface | o21 << 8); } };
	struct TriangleCol { uint16 a, b, c; uint8 surface, o7; };
	struct DonneesCol {
		const SphereCol *spheres = nullptr;
		const BoiteCol *boites = nullptr;
		const int16 *sommets = nullptr;
		const TriangleCol *triangles = nullptr;
		bool arbre = false;
		int16 nTriangles = 0;
		int8 nSpheres = 0, nBoites = 0;
		// Le triangle k : ses trois sommets (FUN_0056e530) et sa surface (+6).
		void Triangle(int k, float *v0, float *v1, float *v2, uint8 *surface) const {
			const TriangleCol &t = triangles[k];
			CCollisionBalayee::Sommet(v0, sommets, t.a); CCollisionBalayee::Sommet(v1, sommets, t.b); CCollisionBalayee::Sommet(v2, sommets, t.c);
			*surface = t.surface;
		}
	};
	// FUN_00565ad0(point) (méthode d'une boîte, lue de 0x565ad0 à 0x565bf0, 289 octets) : le point est-il dans la boîte, strictement
	// (min < p et p > max refusent l'égalité : fcompp, test ah,5, jp ; test ah,0x41, jne).
	static bool DansBoite(const BoiteCol &b, const float *p) {
		return b.min[0] < p[0] && b.min[1] < p[1] && b.min[2] < p[2] && p[0] < b.max[0] && p[1] < b.max[1] && p[2] < b.max[2];
	}
	// FUN_0056c770(m1, m2, a, m4, m5, b, contact, a8, a9, a10, a11, a12, a13, a14, a15) (cdecl, quinze arguments, lue à
	// l'assembleur de 0x56c770 à 0x56d970, 4 610 octets) : le test général de collision de deux modèles, a qui se déplace de la
	// matrice m1 à m2, b de m4 à m5 ; la méthode +0xa4 d'un CProjectile (FUN_0046a340) le donne avec m1 = son arg1 (la matrice
	// d'avant le déplacement), m2 et a = la matrice et le modèle de l'entité qui balaie, m4 = m5 et b = la matrice et le modèle de
	// l'entité de la liste, cinq mots nuls, a13 = 0xf, a14 nul et a15 = 1. Des données nulles chez l'un ou l'autre : 0. Un contact
	// nul est remplacé par un contact sur sa pile. Dans l'ordre :
	//  - da = pos(m2) − pos(m1) et db = pos(m5) − pos(m4) ; l'inverse de m4 (FUN_00413620) fois m1 (FUN_00413780) : la matrice qui
	//    pose a, avant son déplacement, dans l'espace de b ; la fraction du contact repart à FLT_MAX (0x9009b8) ;
	//  - pour chaque sphère de a (une seule, la première, si a8 ; celles dont l'octet +0x11 n'est pas nul sautées si a12), posée
	//    par m1 (FUN_0045d3b0 ; si a9, da devient sa position par m2 moins celle-là) :
	//    · bit 1 de a13 : contre chaque sphère de b posée par m4, FUN_00576a00 avec t = 1 ; une fraction strictement plus petite
	//      que celle du contact la remplace, avec le point, la normale (a + f da) − (b + f db) normalisée, le mot +0x10 de la sphère
	//      de a en +0x20 du contact, celui de b en +0x22 et le rang de la sphère de a en +0x1c ; touché et a10 : 1 tout de suite ;
	//    · dans l'espace de b : la sphère de a, et le déplacement relatif da − db tourné par l'inverse de m4 (FUN_00414560), de
	//      longueur L (FUN_0085c4d0 sur |·|², arrondie) ; un compteur de contacts et un octet partent de 0 ;
	//    · bit 2 : chaque boîte de b par la fonction `boite` (00576660, non lue ici : elle reçoit la boîte, son max, la sphère, le
	//      déplacement, et rend la distance, la normale, le point, l'octet et le compteur) ; une distance f < L : f / L, si elle est
	//      plus petite que celle du contact, la remplace, avec la normale tournée par m4, le point posé par m4, les mots +0x10 et
	//      +0x20 ; sinon, si a11 et que la sphère est dans la boîte (FUN_00565ad0, strictement) : la fraction 1, le point du centre
	//      posé par m4, la normale du vecteur 0x9e33dc ((0, 0, 1) dans le fichier) ; touché et a10 : 1 ;
	//    · bit 8, s'il y a des triangles : chacun (tous dans l'ordre, ou ceux que rend l'arbre pour la sphère grossie de L,
	//      la fonction `candidats`) : ses trois sommets (FUN_0056e530), sa normale (v2 − v0) × (v1 − v0) (FUN_004145e0, puis
	//      FUN_00414cc0 ; l'ordre des sommets des modèles de Bully la fait sortir de la face), FUN_00575510 ; une distance f < L
	//      compte un contact (au second, a14 non nul reçoit 1 en +1 et +2), f / L plus petite que celle du contact la remplace :
	//      la normale (avec a15, celle que rend FUN_00575510, sinon celle du triangle) tournée par m4, le point posé par m4, le mot
	//      +0x10 de la sphère, la surface du triangle en +0x22 et 0 en +0x23 ;
	//  - elle rend 1 si quelque chose a touché. La fin libère le champ +0xc du contact de sa pile (00465690), qu'elle n'écrit
	//    jamais : sans effet.
	// Supposé : le sens de l'octet que rend FUN_00575510 (1 pour la face) et des octets +1 et +2 de a14 ; a14 reçoit aussi cet
	// octet et le point ou la normale (ici écrits comme dans le binaire). `C` est le contact (point, normale, o1c, o20, o22,
	// fraction), `MA` et `MB` les données d'un modèle (`DonneesCol`, ou une vue avec les mêmes noms : nSpheres, spheres, nBoites,
	// boites, nTriangles, Triangle, arbre).
	template<class C, class MA, class MB, class B, class K>
	static uint8 TestGeneral(const float *m1, const float *m2, const MA *a, const float *m4, const float *m5, const MB *b, C *contact,
	                         bool a8, bool a9, bool a10, bool a11, bool a12, uint8 a13, uint8 *a14, bool a15, B &&boite, K &&candidats) {
		if(!a || !b) return 0;
		C local;
		if(!contact) contact = &local;
		float da[3], db[3];
		for(int i = 0; i < 3; i++) { da[i] = (float)(m2[12 + i] - m1[12 + i]); db[i] = (float)(m5[12 + i] - m4[12 + i]); }
		Matrice inv, loc;
		Inverser(m4, inv.m);
		Multiplier(&loc, inv.m, m1);
		contact->fraction = 3.4028234663852886e+38f;
		uint8 touche = 0;
		int n = a8 ? 1 : a->nSpheres;
		for(int i = 0; i < n; i++) {
			const SphereCol &sa = a->spheres[i];
			if(a12 && sa.o11) continue;
			float ca[4];
			Point(ca, m1, sa.centre);
			ca[3] = sa.rayon;
			if(a9) {
				float t[3];
				Point(t, m2, sa.centre);
				for(int k = 0; k < 3; k++) da[k] = (float)(t[k] - ca[k]);
			}
			if(a13 & 1) {
				for(int j = 0; j < b->nSpheres; j++) {
					const SphereCol &sb = b->spheres[j];
					if(a12 && sb.o11) continue;
					float cb[4], pt[3];
					Point(cb, m4, sb.centre);
					cb[3] = sb.rayon;
					float f = contact->fraction;
					if(!SpheresBalayees(ca, cb, 1.0f, da, db, &f, pt) || !(f < contact->fraction)) continue;
					contact->fraction = f;
					float na[3], nb[3], nr[3];
					for(int k = 0; k < 3; k++) {
						contact->point[k] = pt[k];
						na[k] = (float)((float)((double)da[k] * f) + ca[k]);
						nb[k] = (float)((float)((double)f * db[k]) + cb[k]);
						nr[k] = (float)(na[k] - nb[k]);
					}
					contact->o20 = sa.Mot();
					contact->o22 = sb.Mot();
					contact->o1c = (uint32)i;
					touche = 1;
					Normaliser(nr);
					for(int k = 0; k < 3; k++) contact->normale[k] = nr[k];
				}
				if(a10 && touche) return 1;
			}
			float s[4], rel[3], rb[3];
			Point(s, loc.m, sa.centre);
			s[3] = sa.rayon;
			for(int k = 0; k < 3; k++) rel[k] = (float)(da[k] - db[k]);
			Tourner(rb, inv.m, rel);
			float L = (float)std::sqrt(std::fabs((double)(float)(((double)rb[0] * rb[0] + (double)rb[1] * rb[1]) + (double)rb[2] * rb[2])));
			uint8 octet = 0;
			int32 compte = 0;
			float nB[3] = {0, 0, 0}, pB[3] = {0, 0, 0};
			if(a13 & 2) {
				for(int j = 0; j < b->nBoites; j++) {
					const BoiteCol &bx = b->boites[j];
					float f = contact->fraction;
					if(boite(bx, s, rb, &f, nB, pB, &octet, &compte) && f < L) {
						if(compte > 1 && a14) { a14[2] = 1; a14[1] = 1; }
						float q = (float)((double)f / L);
						if(!(q < contact->fraction)) continue;
						float nw[3], pw[3];
						Tourner(nw, m4, nB);
						if(a14) { a14[0] = octet; memcpy(a14 + 4, nw, 12); a14[2] = 1; }
						Point(pw, m4, pB);
						contact->fraction = q;
						for(int k = 0; k < 3; k++) { contact->normale[k] = nw[k]; contact->point[k] = pw[k]; }
						contact->o20 = sa.Mot();
						contact->o22 = bx.Mot();
						contact->o1c = (uint32)i;
						touche = 1;
					} else if(a11 && DansBoite(bx, s)) {
						float pw[3];
						Point(pw, m4, s);
						contact->fraction = 1.0f;
						for(int k = 0; k < 3; k++) contact->point[k] = pw[k];
						contact->normale[0] = 0.0f; contact->normale[1] = 0.0f; contact->normale[2] = 1.0f;
						contact->o20 = sa.Mot();
						contact->o22 = bx.Mot();
						contact->o1c = (uint32)i;
						touche = 1;
					}
				}
				if(a10 && touche) return 1;
			}
			if(b->nTriangles > 0 && (a13 & 8)) {
				auto visiter = [&](int k) {
					float v0[3], v1[3], v2[3], e1[3], e2[3], nt[3];
					uint8 surface;
					b->Triangle(k, v0, v1, v2, &surface);
					for(int q = 0; q < 3; q++) { e1[q] = (float)(v1[q] - v0[q]); e2[q] = (float)(v2[q] - v0[q]); }
					nt[0] = (float)((double)e2[1] * e1[2] - (double)e2[2] * e1[1]);
					nt[1] = (float)((double)e2[2] * e1[0] - (double)e1[2] * e2[0]);
					nt[2] = (float)((double)e1[1] * e2[0] - (double)e2[1] * e1[0]);
					Normaliser(nt);
					const float *som[3] = {v0, v1, v2};
					float f = contact->fraction;
					if(!SphereTriangle(som, nt, s, rb, &f, nB, pB, &octet) || !(f < L)) return;
					compte++;
					if(compte > 1 && a14) { a14[2] = 1; a14[1] = 1; }
					float q = (float)((double)f / L);
					if(!(q < contact->fraction)) return;
					float nw[3], pw[3];
					if(!a15) { Tourner(nw, m4, nt); for(int z = 0; z < 3; z++) nt[z] = nw[z]; }
					else { Tourner(nw, m4, nB); for(int z = 0; z < 3; z++) nB[z] = nw[z]; }
					for(int z = 0; z < 3; z++) contact->normale[z] = nw[z];
					if(a14) { memcpy(a14 + 4, nt, 12); a14[0] = octet; a14[2] = 1; }
					Point(pw, m4, pB);
					contact->fraction = q;
					for(int z = 0; z < 3; z++) contact->point[z] = pw[z];
					contact->o20 = sa.Mot();
					contact->o22 = surface;
					contact->o1c = (uint32)i;
					touche = 1;
				};
				if(b->arbre) candidats(s, (float)((double)L + s[3]), visiter);
				else for(int k = 0; k < b->nTriangles; k++) visiter(k);
			}
		}
		return touche;
	}
	// FUN_00469f70(m1, m2, a, m4, b, contact) (méthode de l'entité qui balaie, ret 0x18, lue à l'assembleur de 0x469f70 à 0x46a333,
	// 966 octets) : le test fin d'un projectile contre un piéton, que la méthode +0xa4 d'un CProjectile (FUN_0046a340) choisit pour
	// un piéton qui n'est ni le rat ni un chien ; a est le modèle du projectile qui va de m1 à m2, b le modèle du piéton (le mot
	// 0xcf3138, que FUN_00572680, CModelePieton::Construire dans ModelePieton.h, prépare juste avant), posé par m4, et qui ne bouge pas (le déplacement de b est le vecteur
	// 0xbcc564, dans la mémoire non initialisée du fichier : nul, supposé). Des données nulles chez l'un ou l'autre : 0. Elle calcule
	// l'inverse de m4 fois m1 (FUN_00413620, FUN_00413780) sans s'en servir, et met la fraction du contact à FLT_MAX. Pour chaque
	// sphère de a, posée par m1 (FUN_0045d3b0), les sphères de b posées par m4, chacune par FUN_00576a00 (t = 1, db nul) :
	//  - un contact dont la fraction est strictement plus petite que celle du contact le remplace : la fraction, le point, le mot
	//    +0x10 de la sphère de a en +0x20, celui de la sphère de b en +0x22, la normale (a + f da) − b normalisée, le rang de la
	//    sphère de b en +0x1c ;
	//  - la sphère 0 de b touchée, le déplacement est normalisé puis multiplié par 100 (le double 0x900130) et seule la sphère 3
	//    est essayée : touchée, elle donne le point, les mots, la normale et le rang 3, mais pas la fraction, qui reste celle de
	//    la sphère 0 ; puis la boucle s'arrête. Le déplacement reste allongé pour les sphères suivantes de a.
	// Les six sphères sont celles de FUN_00572680 : avec la fiche FightCollisionButes par défaut, la sphère 0 est sur le bassin
	// (os 2, 0,35 m), la sphère 1 sur le haut de la colonne (os 12), la sphère 2 sur la tête (os 16), la sphère 3 de nouveau sur
	// le bassin (os 2, 0,1 m), les sphères 4 et 5 sur les mollets (os 4 et 7). La sphère 0 n'englobe donc pas le piéton : un
	// projectile qui touche le grand bassin n'est plus essayé que contre le petit, les autres sphères passent leur tour. Le
	// binaire ne vérifie pas que b a quatre sphères ; ici, sans quatrième sphère, la boucle s'arrête (une garde de l'hôte).
	template<class C, class MA, class MB>
	static uint8 TestPieton(const float *m1, const float *m2, const MA *a, const float *m4, const MB *b, C *contact) {
		if(!a || !b) return 0;
		float da[3];
		for(int i = 0; i < 3; i++) da[i] = (float)(m2[12 + i] - m1[12 + i]);
		static const float zero[3] = {0.0f, 0.0f, 0.0f};
		contact->fraction = 3.4028234663852886e+38f;
		uint8 touche = 0;
		for(int i = 0; i < a->nSpheres; i++) {
			const SphereCol &sa = a->spheres[i];
			float ca[4];
			Point(ca, m1, sa.centre);
			ca[3] = sa.rayon;
			bool allonge = false;
			for(int j = 0; j < b->nSpheres; j++) {
				if(allonge) {
					j = 3;
					Normaliser(da);
					for(int k = 0; k < 3; k++) da[k] = (float)((double)da[k] * 100.0);
					if(j >= b->nSpheres) break;
				}
				const SphereCol &sb = b->spheres[j];
				float cb[4], pt[3];
				Point(cb, m4, sb.centre);
				cb[3] = sb.rayon;
				float f = contact->fraction;
				if(!SpheresBalayees(ca, cb, 1.0f, da, zero, &f, pt) || !(f < contact->fraction)) {
					if(allonge) break;
					continue;
				}
				if(!allonge) contact->fraction = f;
				float n[3];
				for(int k = 0; k < 3; k++) {
					contact->point[k] = pt[k];
					n[k] = (float)((float)(ca[k] + (float)((double)da[k] * f)) - cb[k]);
				}
				contact->o20 = sa.Mot();
				contact->o22 = sb.Mot();
				touche = 1;
				Normaliser(n);
				for(int k = 0; k < 3; k++) contact->normale[k] = n[k];
				contact->o1c = (uint32)j;
				if(j != 0) { if(allonge) break; continue; }
				allonge = true;
			}
		}
		return touche;
	}
};
