// Marcher dans des volumes de collision construits à la main : ne lit aucun
// fichier du jeu. Un sol, une marche, un mur, une rampe, une sphère.
#include "collision/Marche.h"
#include "collision/Balayes.h"
#include "collision/ModelePieton.h"
#include <cmath>
#include <functional>
#include <cstdio>

static int g_ko = 0;
static void Verifier(bool ok, const char *quoi){ if(!ok){ printf("ECHEC : %s\n", quoi); g_ko++; } }

// Pavé de (x0, y0, z0) à (x1, y1, z1).
static void Pave(CMondeCollision &m, float x0, float y0, float z0, float x1, float y1, float z1){
	CVector k[8];
	for(int i = 0; i < 8; i++) k[i] = CVector(i & 1 ? x1 : x0, i & 2 ? y1 : y0, i & 4 ? z1 : z0);
	m.AjouterBoite(k);
}

int
main(void)
{
	CMondeCollision m;
	Pave(m, -10, -10, -1, 10, 10, 0);            // sol, dessus à z = 0
	Pave(m, 2, -10, 0, 10, 10, 0.3f);            // marche de 30 cm à partir de x = 2
	Pave(m, 6, -10, 0.3f, 7, 10, 3);             // mur à x = 6
	// rampe de x = -10 à -4, de z = 0 à 2
	m.AjouterTriangle(CVector(-4, -5, 0), CVector(-10, -5, 2), CVector(-10, 5, 2));
	m.AjouterTriangle(CVector(-4, -5, 0), CVector(-10, 5, 2), CVector(-4, 5, 0));
	m.AjouterSphere(CVector(0, 6, 0), 1.0f);     // un rocher

	float z;
	Verifier(m.Sol(0, 0, 1, 10, &z) && fabsf(z) < 1e-4f, "sol à z = 0");
	Verifier(m.Sol(3, 0, 1, 10, &z) && fabsf(z - 0.3f) < 1e-4f, "marche à z = 0,3");
	Verifier(!m.Sol(50, 50, 1, 10, &z), "rien hors du monde");
	Verifier(m.Sol(-7, 0, 5, 10, &z) && fabsf(z - 1.0f) < 1e-3f, "rampe à mi-hauteur : z = 1");
	Verifier(m.Sol(0, 6, 5, 10, &z) && fabsf(z - 1.0f) < 1e-4f, "sommet du rocher : z = 1");

	// Chute puis appui au sol.
	CMarcheur p; p.pos = CVector(0, 0, 3);
	for(int i = 0; i < 120; i++) p.Avancer(m, 0, 0, 1 / 60.0f);
	Verifier(p.auSol && fabsf(p.pos.z) < 1e-4f, "le marcheur tombe et se pose sur le sol");

	// Monter la marche en avançant vers +x, puis buter sur le mur.
	for(int i = 0; i < 600; i++) p.Avancer(m, 0.02f, 0, 1 / 60.0f);
	Verifier(fabsf(p.pos.z - 0.3f) < 1e-3f, "la marche de 30 cm est montée");
	Verifier(p.pos.x <= 6 - p.rayon + 0.02f && p.pos.x > 5, "le mur arrête le marcheur à un rayon de distance");
	printf("  devant le mur : x = %.3f (mur à 6, rayon %.2f)\n", p.pos.x, p.rayon);

	// Le mur ne soulève pas : on reste sur la marche.
	Verifier(fabsf(p.pos.z - 0.3f) < 1e-3f, "le mur ne soulève pas le marcheur");

	// Monter la rampe vers -x.
	CMarcheur r; r.pos = CVector(-3, 0, 0);
	for(int i = 0; i < 400; i++) r.Avancer(m, -0.01f, 0, 1 / 60.0f);
	float attendu = (-4 - r.pos.x) / 6 * 2;
	Verifier(r.auSol && fabsf(r.pos.z - attendu) < 0.02f, "la rampe se monte en suivant sa pente");
	printf("  sur la rampe : x = %.2f, z = %.3f (attendu %.3f)\n", r.pos.x, r.pos.z, attendu);

	// Trop haut pour une marche : un pavé de 1 m bloque.
	CMondeCollision m2;
	Pave(m2, -10, -10, -1, 10, 10, 0);
	Pave(m2, 2, -10, 0, 10, 10, 1.0f);
	CMarcheur b; b.pos = CVector(0, 0, 0);
	for(int i = 0; i < 300; i++) b.Avancer(m2, 0.02f, 0, 1 / 60.0f);
	Verifier(b.pos.x < 2 && fabsf(b.pos.z) < 1e-3f, "une marche d'un mètre ne se monte pas");

	// La grille ne change aucun résultat : un monde de marches, de murs et de
	// rampes au hasard, 2 000 requêtes de sol et de poussée, avec et sans.
	{
		CMondeCollision m;
		uint32 h = 12345;
		auto Hasard = [&](float a, float b){ h = h * 1103515245u + 12345u; return a + (b - a) * ((h >> 8) & 0xffff) / 65535.0f; };
		for(int k = 0; k < 400; k++){
			CVector c(Hasard(-40, 40), Hasard(-40, 40), Hasard(0, 3));
			m.AjouterTriangle(c, CVector(c.x + Hasard(-3, 3), c.y + Hasard(-3, 3), c.z + Hasard(-1, 1)), CVector(c.x + Hasard(-3, 3), c.y + Hasard(-3, 3), c.z + Hasard(-1, 1)));
		}
		CMondeCollision g = m; g.Indexer(4.0f);
		int ecarts = 0;
		for(int k = 0; k < 2000; k++){
			float x = Hasard(-45, 45), y = Hasard(-45, 45), z1 = 0, z2 = 0;
			bool s1 = m.Sol(x, y, 5, 10, &z1), s2 = g.Sol(x, y, 5, 10, &z2);
			if(s1 != s2 || (s1 && z1 != z2)) ecarts++;
			CVector c1(x, y, Hasard(0, 3)), c2 = c1;
			int32 n1 = m.Repousser(c1, 0.3f), n2 = g.Repousser(c2, 0.3f);
			if(n1 != n2 || c1.x != c2.x || c1.y != c2.y) ecarts++;
		}
		Verifier(ecarts == 0, "grille : mêmes sols et mêmes poussées que sans elle");
		printf("  grille : 2 000 requêtes, %d écart\n", ecarts);
	}

	printf("test_marche : %s\n", g_ko ? "ECHEC" : "ok");
	// Le trajet d'un point : le premier volume croisé, la fraction du trajet et la normale tournée contre le sens du trajet.
	{
		CMondeCollision t;
		Pave(t, -10, -10, -1, 10, 10, 0);            // sol, dessus à z = 0
		Pave(t, 6, -10, 0, 7, 10, 3);                // mur à x = 6
		t.AjouterSphere(CVector(0, 6, 0), 1.0f);     // un rocher
		for(int tour = 0; tour < 2; tour++){
			float f; CVector n;
			Verifier(t.Trajet(CVector(0, 0, 2), CVector(0, 0, -2), &f, &n) && fabsf(f - 0.5f) < 1e-5f && fabsf(n.z - 1.0f) < 1e-5f, "le trajet qui tombe touche le sol à mi-chemin, normale vers le haut");
			Verifier(t.Trajet(CVector(1, 0, 1), CVector(3, 0, -1), &f, &n) && fabsf(f - 0.5f) < 1e-5f && fabsf(n.z - 1.0f) < 1e-5f && fabsf(n.x) < 1e-5f, "le trajet en biais touche le sol, la normale reste celle du sol");
			Verifier(t.Trajet(CVector(5, 0, 1), CVector(9, 0, 1), &f, &n) && fabsf(f - 0.25f) < 1e-5f && fabsf(n.x + 1.0f) < 1e-5f, "le trajet vers +x touche la première face du mur, normale vers -x");
			Verifier(t.Trajet(CVector(9, 0, 1), CVector(5, 0, 1), &f, &n) && fabsf(f - 0.5f) < 1e-5f && fabsf(n.x - 1.0f) < 1e-5f, "le trajet vers -x touche l'autre face, normale vers +x");
			Verifier(!t.Trajet(CVector(0, 0, 2), CVector(5, 0, 2), &f, &n), "rien de croisé au-dessus du sol");
			Verifier(!t.Trajet(CVector(0, 0, 2), CVector(0, 0, 0.5f), &f, &n), "le trajet qui s'arrête avant le sol ne le touche pas");
			Verifier(t.Trajet(CVector(0, 6, 3), CVector(0, 6, 0.5f), &f, &n) && fabsf(f - 0.8f) < 1e-5f && fabsf(n.z - 1.0f) < 1e-5f, "le trajet entre dans le rocher par son sommet");
			Verifier(!t.Trajet(CVector(0, 6, 0.5f), CVector(0, 6, 0.2f), &f, &n), "un départ dans le rocher ne compte pas");
			t.Indexer(4.0f);                             // le second tour avec la grille : mêmes réponses
		}
	}
	// Les tests de collision balayés de bully.exe (src/collision/Balayes.h) : matrices, sommets, deux sphères.
	{
		typedef CCollisionBalayee B;
		auto pres = [](float a, float b) { return fabsf(a - b) < 1e-5f; };
		// Un quart de tour autour de z (la droite vers +y, l'avant vers −x), posé en (10, 20, 30).
		B::Matrice m;
		float r[16] = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 10, 20, 30, 0};
		for(int i = 0; i < 16; i++) m.m[i] = r[i];
		const float p[3] = {1, 2, 3};
		float q[4], v[4], back[4];
		B::Point(q, m.m, p);
		Verifier(pres(q[0], 8) && pres(q[1], 21) && pres(q[2], 33), "FUN_0045d3b0 : le point passé par la matrice");
		B::Tourner(v, m.m, p);
		Verifier(pres(v[0], -2) && pres(v[1], 1) && pres(v[2], 3), "FUN_00414560 : le vecteur tourné, sans la position");
		B::Matrice inv; inv.m[15] = 7;
		B::Inverser(m.m, inv.m);
		B::Point(back, inv.m, q);
		Verifier(pres(back[0], 1) && pres(back[1], 2) && pres(back[2], 3) && inv.m[15] == 7, "FUN_00413620 : l'inverse ramène le point, le quatrième réel non écrit");
		Verifier(pres(inv.m[12], -20) && pres(inv.m[13], 10) && pres(inv.m[14], -30), "FUN_00413620 : la position de l'inverse");
		B::Matrice prod; prod.o40 = 99;
		B::Multiplier(&prod, inv.m, m.m);
		bool ident = prod.o40 == 0;
		for(int j = 0; j < 4; j++) for(int i = 0; i < 3; i++) ident = ident && pres(prod.m[j * 4 + i], (i == j) ? 1.0f : 0.0f);
		Verifier(ident, "FUN_00413780 : l'inverse fois la matrice, l'identité, le mot +0x40 à zéro");
		B::Multiplier(&prod, m.m, m.m);
		Verifier(pres(prod.m[0], -1) && pres(prod.m[5], -1) && pres(prod.m[12], -10) && pres(prod.m[13], 30) && pres(prod.m[14], 60), "FUN_00413780 : deux quarts de tour, un demi-tour, les positions composées");
		const int16 som[6] = {128, -256, 64, 0, 1, -1};
		float s3[3];
		B::Sommet(s3, som, 0);
		Verifier(s3[0] == 1.0f && s3[1] == -2.0f && s3[2] == 0.5f, "FUN_0056e530 : un sommet, trois entiers sur 128");
		B::Sommet(s3, som, 1);
		Verifier(s3[0] == 0.0f && s3[1] == 0.0078125f && s3[2] == -0.0078125f, "FUN_0056e530 : le second sommet, 6 octets plus loin");
		// Deux sphères de 0,5 m : a part de l'origine et avance de 4 m en x, b attend en (2, 0, 0).
		const float a[4] = {0, 0, 0, 0.5f}, zero[3] = {0, 0, 0}, da[3] = {4, 0, 0};
		float f = -1, pt[3];
		const float b[4] = {2, 0, 0, 0.5f};
		Verifier(B::SpheresBalayees(a, b, 1.0f, da, zero, &f, pt) && pres(f, 0.25f) && pres(pt[0], 1.5f) && pt[1] == 0.0f, "FUN_00576a00 : touché au quart du trajet, en x = 1,5");
		Verifier(B::SpheresBalayees(b, a, 1.0f, zero, da, &f, pt) && pres(f, 0.25f) && pres(pt[0], 1.5f), "FUN_00576a00 : le même vu de b");
		const float dm[3] = {-4, 0, 0};
		Verifier(!B::SpheresBalayees(a, b, 1.0f, dm, zero, &f, pt), "FUN_00576a00 : a s'éloigne de b (d · e > 0), rien");
		const float bl[4] = {5, 0, 0, 0.5f}, d1[3] = {1, 0, 0};
		Verifier(!B::SpheresBalayees(a, bl, 1.0f, d1, zero, &f, pt), "FUN_00576a00 : le plus proche après t, encore à 3 m, rien");
		const float bc[4] = {2, 3, 0, 0.5f};
		Verifier(!B::SpheresBalayees(a, bc, 1.0f, da, zero, &f, pt), "FUN_00576a00 : passe à 3 m de côté, le discriminant négatif");
		const float bo[4] = {0.5f, 0, 0, 0.5f};
		f = -1;
		Verifier(B::SpheresBalayees(a, bo, 1.0f, da, zero, &f, pt) && f == 0.0f && pt[0] == 0.25f, "FUN_00576a00 : déjà en contact, à 0, au milieu des centres");
		f = -1;
		Verifier(B::SpheresBalayees(a, bo, 1.0f, zero, zero, &f, pt) && f == 0.0f && pt[0] == 0.25f, "FUN_00576a00 : sans déplacement, en contact, au milieu");
		Verifier(!B::SpheresBalayees(a, b, 1.0f, zero, zero, &f, pt), "FUN_00576a00 : sans déplacement, à 2 m, rien");
		const float d2[3] = {2, 0, 0};
		Verifier(B::SpheresBalayees(a, b, 1.0f, d2, zero, &f, pt) && pres(f, 0.5f) && pres(pt[0], 1.5f), "FUN_00576a00 : le contact à la moitié, la fin du trajet juste au contact");
		const float bd[4] = {2, 0, 0, 0.5f}, db[3] = {-2, 0, 0};
		Verifier(B::SpheresBalayees(a, bd, 1.0f, d2, db, &f, pt) && pres(f, 0.25f) && pres(pt[0], 1.0f), "FUN_00576a00 : les deux bougent, se croisent au milieu");
		// Une sphère contre un triangle (FUN_00575510) et ses pièces. Le triangle (0, 0, 0), (4, 0, 0), (0, 4, 0), de normale +z.
		{
			float n0[3] = {0, 0, 0};
			Verifier(B::DiviserParRacine(2.0f, 16.0f) == 0.5f && B::DiviserParRacine(1.0f, 0.0f) > 3e38f && B::DiviserParRacine(-1.0f, 0.0f) < -3e38f, "FUN_00414ad0 : a / √|b|, FLT_MAX signé pour une racine nulle");
			float v3[3] = {3, 0, 4};
			Verifier(pres(B::Normaliser(v3), 0.2f) && pres(v3[0], 0.6f) && pres(v3[2], 0.8f) && B::Normaliser(n0) > 3e38f && n0[0] == 0.0f, "FUN_00414cc0 : normalisé, le facteur rendu, le vecteur nul reste nul");
			const float t0[3] = {0, 0, 0}, t1[3] = {4, 0, 0}, t2[3] = {0, 4, 0}, nz[3] = {0, 0, 1};
			const float *tri[3] = {t0, t1, t2};
			float pied[3];
			const float pp[3] = {1, 3, 0};
			Verifier(pres(B::Distance2ADroite(pp, t0, t1, pied), 9.0f) && pres(pied[0], 1.0f) && pied[1] == 0.0f && pres(B::DistanceADroite(pp, t0, t1, nullptr), 3.0f), "FUN_00574fe0 et FUN_00575100 : la distance à la droite et le pied");
			B::Plan pl;
			const float n2[3] = {0, 0, 2}, h[3] = {0, 0, 1};
			B::PlanParPoint(&pl, h, n2);
			Verifier(pl.n[2] == 1.0f && pl.d == -1.0f, "FUN_00575140 : le plan par un point, la normale normalisée");
			const float bas[3] = {4, 0, -1};
			B::PlanParTroisPoints(&pl, t0, t1, bas);
			Verifier(pres(pl.n[1], 1.0f) && pl.n[0] == 0.0f && pl.d == 0.0f, "FUN_005751a0 : le plan de trois points, normale w × u");
			const float a2[2] = {0, 0}, b2[2] = {2, 0}, c2[2] = {1, -1}, d2[2] = {1, 1}, e2[2] = {0, 1};
			float t = -1;
			Verifier(B::Droites2D(a2, b2, c2, d2, &t) && pres(t, 0.5f) && !B::Droites2D(a2, b2, e2, d2, &t), "FUN_00575260 : les deux droites se coupent à t = 0,5, parallèles : faux");
			const float sp[4] = {0, 0, 5, 1};
			const float q0[3] = {0, 0, 0}, q1[3] = {0, 0, 1}, q2[3] = {3, 0, 0}, q3[3] = {3, 0, 5};
			int32 nb = 0; float r1 = 0, r2 = 0;
			Verifier(B::DroiteSphere(sp, q0, q1, &nb, &r1, &r2) && nb == 2 && pres(r1, 6.0f) && pres(r2, 4.0f), "FUN_00575310 : la droite entre et sort de la sphère, t1 = 6 et t2 = 4");
			Verifier(!B::DroiteSphere(sp, q2, q3, &nb, &r1, &r2), "FUN_00575310 : à 3 m, le discriminant négatif");
			const float pin[3] = {1, 1, 0}, pout[3] = {3, 3, 0}, pbord[3] = {2, 0, 0};
			Verifier(B::DansTriangle(t0, t1, t2, pin) && !B::DansTriangle(t0, t1, t2, pout) && B::DansTriangle(t0, t1, t2, pbord) && !B::DansTriangle(t0, t1, t1, pin), "FUN_00574dc0 : dedans, dehors, sur le bord, le triangle plat");
			float f, nc[3], pc[3]; uint8 face = 9;
			// De face : la sphère de 0,5 m en (1, 1, 2) descend de 3 m.
			const float sf[4] = {1, 1, 2, 0.5f}, desc[3] = {0, 0, -3};
			Verifier(B::SphereTriangle(tri, nz, sf, desc, &f, nc, pc, &face) && pres(f, 1.5f) && face == 1 && nc[2] == 1.0f && pres(pc[0], 1.0f) && pres(pc[2], 0.0f), "FUN_00575510 : la face, à 1,5 m le long du déplacement");
			const float mont[3] = {0, 0, 3};
			Verifier(!B::SphereTriangle(tri, nz, sf, mont, &f, nc, pc, &face), "FUN_00575510 : la sphère qui s'éloigne de la face, rien");
			const float sd[4] = {1, 1, -1, 0.5f};
			Verifier(!B::SphereTriangle(tri, nz, sd, desc, &f, nc, pc, &face), "FUN_00575510 : derrière le plan de plus d'un rayon, rien");
			// Un sommet : la sphère en (−2, 0, 0) avance vers +x en descendant un peu.
			const float ss[4] = {-2, 0, 0, 0.5f}, av[3] = {3, 0, -0.1f};
			Verifier(B::SphereTriangle(tri, nz, ss, av, &f, nc, pc, &face) && fabsf(f - 1.5f) < 0.01f && face == 0 && pc[0] == 0.0f && pc[1] == 0.0f && nc[0] < -0.99f, "FUN_00575510 : le sommet (0, 0, 0), à 1,5 m environ, la normale vers le centre");
			// Une arête : la sphère en (2, −1, 0) avance vers +y en descendant un peu.
			const float sa[4] = {2, -1, 0, 0.5f}, avy[3] = {0, 3, -0.1f};
			Verifier(B::SphereTriangle(tri, nz, sa, avy, &f, nc, pc, &face) && fabsf(f - 0.5f) < 0.01f && face == 0 && fabsf(pc[0] - 2.0f) < 1e-3f && fabsf(pc[1]) < 1e-3f && nc[1] < -0.99f, "FUN_00575510 : l'arête (0, 0, 0) – (4, 0, 0), à 0,5 m, en x = 2");
			const float sl[4] = {6, -1, 0, 0.5f};
			Verifier(!B::SphereTriangle(tri, nz, sl, avy, &f, nc, pc, &face), "FUN_00575510 : à côté de l'arête, rien");
		}
		// Le test général (FUN_0056c770) : un sol de deux triangles (z = 0, de −5 à 5) et une sphère, posés en (10, 0, 0) ; a est une
		// sphère de 0,5 m.
		{
			struct Contact { float point[3] = {0, 0, 0}; float normale[3] = {0, 0, 0}; uint32 o1c = 9; uint16 o20 = 0, o22 = 0; float fraction = 0; };
			static const int16 som[12] = {-640, -640, 0, -640, 640, 0, 640, -640, 0, 640, 640, 0};
			static const B::TriangleCol tris[2] = {{0, 1, 2, 7, 0}, {3, 2, 1, 8, 0}};
			B::SphereCol sb[1] = {{{3, 0, 1}, 0.5f, 5, 1, {0, 0}}};
			B::SphereCol sa[1] = {{{0, 0, 0}, 0.5f, 2, 0, {0, 0}}};
			B::DonneesCol mb, ma;
			mb.spheres = sb; mb.nSpheres = 1; mb.sommets = som; mb.triangles = tris; mb.nTriangles = 2;
			ma.spheres = sa; ma.nSpheres = 1;
			B::Matrice m1, m2, m4;
			m4.m[12] = 10;
			auto pas = [](const B::BoiteCol &, const float *, const float *, float *, float *, float *, uint8 *, int32 *) { return false; };
			int vus = 0;
			auto tous = [&](const float *, float, const std::function<void(int)> &) { vus++; };
			auto place = [](B::Matrice &m, float x, float y, float z) { m.m[12] = x; m.m[13] = y; m.m[14] = z; };
			Contact c;
			// a tombe de 3 m en (11, 1) : le sol à 0,5 m sous son centre, à la moitié du trajet.
			place(m1, 11, 1, 2); place(m2, 11, 1, -1);
			Verifier(B::TestGeneral(m1.m, m2.m, &ma, m4.m, m4.m, &mb, &c, false, false, false, false, false, 0xf, nullptr, true, pas, tous) == 1
			         && pres(c.fraction, 0.5f) && pres(c.normale[2], 1.0f) && pres(c.point[0], 11.0f) && pres(c.point[1], 1.0f) && pres(c.point[2], 0.0f)
			         && c.o22 == 8 && c.o20 == 2 && c.o1c == 0, "FUN_0056c770 : le sol, à la moitié, le second triangle, sa surface en +0x22");
			Verifier(B::TestGeneral(m1.m, m2.m, &ma, m4.m, m4.m, &mb, &c, false, false, false, false, false, 0x7, nullptr, true, pas, tous) == 0
			         && c.fraction > 3e38f, "FUN_0056c770 : sans le bit 8, les triangles ne comptent pas, la fraction à FLT_MAX");
			// Vers le haut : la face du sol n'est pas testée de dos.
			place(m1, 11, 1, -1); place(m2, 11, 1, 2);
			Verifier(B::TestGeneral(m1.m, m2.m, &ma, m4.m, m4.m, &mb, &c, false, false, false, false, false, 0xf, nullptr, true, pas, tous) == 0, "FUN_0056c770 : par en dessous, le sol se traverse");
			// L'arbre : seuls les triangles qu'il rend sont testés, avec la sphère de b grossie de la longueur du déplacement.
			place(m1, 11, 1, 2); place(m2, 11, 1, -1);
			mb.arbre = true;
			float rayon = 0;
			auto premier = [&](const float *, float r, const std::function<void(int)> &v) { rayon = r; v(0); };
			auto second = [&](const float *, float r, const std::function<void(int)> &v) { v(1); };
			Verifier(B::TestGeneral(m1.m, m2.m, &ma, m4.m, m4.m, &mb, &c, false, false, false, false, false, 0xf, nullptr, true, pas, premier) == 0 && pres(rayon, 3.5f), "FUN_0056c770 : l'arbre ne rend que le premier triangle, rien ; le rayon demandé L + r");
			Verifier(B::TestGeneral(m1.m, m2.m, &ma, m4.m, m4.m, &mb, &c, false, false, false, false, false, 0xf, nullptr, true, pas, second) == 1 && pres(c.fraction, 0.5f), "FUN_0056c770 : l'arbre rend le second, touché");
			mb.arbre = false;
			Verifier(vus == 0, "FUN_0056c770 : sans arbre, les triangles sont tous pris dans l'ordre");
			// La sphère de b, avec b tourné d'un quart de tour : elle est en (10, 3, 1) ; a avance de 4 m vers +y à z = 1.
			float r[16] = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 10, 0, 0, 0};
			for(int i = 0; i < 16; i++) m4.m[i] = r[i];
			place(m1, 10, 0, 1); place(m2, 10, 4, 1);
			Verifier(B::TestGeneral(m1.m, m2.m, &ma, m4.m, m4.m, &mb, &c, false, false, false, false, false, 0xf, nullptr, true, pas, tous) == 1
			         && pres(c.fraction, 0.5f) && pres(c.normale[1], -1.0f) && pres(c.point[1], 2.5f) && c.o22 == (5 | 1 << 8) && c.o20 == 2,
			         "FUN_0056c770 : la sphère de b tournée, à la moitié, la normale des centres, les mots +0x10 des deux sphères");
			Verifier(B::TestGeneral(m1.m, m2.m, &ma, m4.m, m4.m, &mb, &c, false, false, false, false, true, 0xf, nullptr, true, pas, tous) == 0, "FUN_0056c770 : l'argument 12 saute la sphère dont l'octet +0x11 n'est pas nul");
			// Une boîte : la fonction des boîtes reçoit la sphère et le déplacement dans l'espace de b ; sa normale est tournée par m4.
			B::BoiteCol bx[1] = {{{-1, -1, -1}, 0, {1, 1, 1}, 0, 4, 3, {0, 0}}};
			mb.boites = bx; mb.nBoites = 1;
			place(m1, 10, 0, 2); place(m2, 10, 0, -1);
			float sv[4] = {0, 0, 0, 0}, rv[3] = {0, 0, 0};
			auto boite = [&](const B::BoiteCol &, const float *s, const float *rb, float *f, float *n, float *p, uint8 *o, int32 *k) {
				for(int i = 0; i < 4; i++) sv[i] = s[i];
				for(int i = 0; i < 3; i++) rv[i] = rb[i];
				*f = 1.0f; n[0] = 1; n[1] = 0; n[2] = 0; p[0] = 0; p[1] = 0; p[2] = 1; ++*k; return true;
			};
			Verifier(B::TestGeneral(m1.m, m2.m, &ma, m4.m, m4.m, &mb, &c, false, false, false, false, false, 0xf, nullptr, true, boite, tous) == 1
			         && pres(c.fraction, 1.0f / 3.0f) && pres(c.normale[1], 1.0f) && pres(c.point[0], 10.0f) && pres(c.point[2], 1.0f) && c.o22 == (4 | 3 << 8)
			         && pres(sv[2], 2.0f) && sv[3] == 0.5f && pres(rv[2], -3.0f), "FUN_0056c770 : la boîte à 1 m sur 3, plus près que le sol ; ce qu'elle reçoit");
			// L'argument 11 : la sphère dans la boîte, sans contact balayé : la fraction 1, la normale (0, 0, 1).
			place(m1, 10, 0, 0.5f); place(m2, 10, 0, 0.5f);
			Verifier(B::TestGeneral(m1.m, m2.m, &ma, m4.m, m4.m, &mb, &c, false, false, false, true, false, 0x2, nullptr, true, pas, tous) == 1
			         && c.fraction == 1.0f && c.normale[2] == 1.0f && pres(c.point[2], 0.5f), "FUN_0056c770 et FUN_00565ad0 : l'argument 11, la sphère dans la boîte");
			const float dedans[3] = {0, 0, 0.5f}, dessus[3] = {0, 0, 2};
			Verifier(B::DansBoite(bx[0], dedans) && !B::DansBoite(bx[0], dessus), "FUN_00565ad0 : dedans, et dehors au-dessus");
			const float bord[3] = {1, 0, 0};
			Verifier(!B::DansBoite(bx[0], bord), "FUN_00565ad0 : sur la face, dehors (strictement)");
			Verifier(B::TestGeneral(m1.m, m2.m, (const B::DonneesCol *)nullptr, m4.m, m4.m, &mb, &c, false, false, false, false, false, 0xf, nullptr, true, pas, tous) == 0, "FUN_0056c770 : sans données, 0");
		}
		// Une boîte (FUN_00576660), face par face (FUN_00575db0, FUN_00574f90) : la boîte de −1 à 1 et une sphère de 0,5 m.
		{
			const float mn[3] = {-1, -1, -1}, mx[3] = {1, 1, 1};
			const float coins[12] = {1, 1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
			const float pin[3] = {0.5f, -0.5f, 1}, pout[3] = {1.5f, 0, 1};
			Verifier(B::DansPolygone(4, coins, pin) && !B::DansPolygone(4, coins, pout) && !B::DansPolygone(2, coins, pin), "FUN_00574f90 : dans le quadrilatère par son éventail, dehors, deux coins");
			float f = 0, nc[3], pc[3]; uint8 face = 9; int32 compte = 0;
			const float s1[4] = {0, 0, 3, 0.5f}, desc[3] = {0, 0, -3};
			Verifier(B::BoiteBalayee(mn, mx, s1, desc, &f, nc, pc, &face, &compte) && pres(f, 1.5f) && nc[2] == 1.0f && pres(pc[2], 1.0f) && face == 1 && compte == 1, "FUN_00576660 : le dessus de la boîte, à 1,5 m, une face comptée");
			const float s2[4] = {-3, 0, 0, 0.5f}, av[3] = {3, 0, -0.1f};
			compte = 0;
			Verifier(B::BoiteBalayee(mn, mx, s2, av, &f, nc, pc, &face, &compte) && fabsf(f - 1.5009f) < 1e-3f && nc[0] == -1.0f && pres(pc[0], -1.0f) && compte == 1, "FUN_00576660 : la face −x, de côté");
			const float s3[4] = {0, 0, 3, 0.5f}, mont[3] = {0, 0, 3};
			compte = 0;
			Verifier(!B::BoiteBalayee(mn, mx, s3, mont, &f, nc, pc, &face, &compte) && f > 3e38f && compte == 0, "FUN_00576660 : la sphère qui s'éloigne, rien, la fraction à FLT_MAX");
			float fq = 0;
			const float sq[4] = {0, 0, -2, 0.5f}, nz[3] = {0, 0, 1};
			Verifier(!B::SphereQuadrilatere(coins, nz, sq, desc, &fq, nc, pc, &face) && fq > 3e38f, "FUN_00575db0 : derrière la face de plus d'un rayon, la fraction déjà à FLT_MAX");
			// La boîte dans le test général, par la fonction des boîtes : la chute de 3 m sur une boîte posée en (10, 0, 0).
			struct Contact { float point[3] = {0, 0, 0}; float normale[3] = {0, 0, 0}; uint32 o1c = 0; uint16 o20 = 0, o22 = 0; float fraction = 0; };
			B::SphereCol sa[1] = {{{0, 0, 0}, 0.5f, 2, 0, {0, 0}}};
			B::BoiteCol bx[1] = {{{-1, -1, -1}, 0, {1, 1, 1}, 0, 6, 0, {0, 0}}};
			B::DonneesCol ma, mb;
			ma.spheres = sa; ma.nSpheres = 1; mb.boites = bx; mb.nBoites = 1;
			B::Matrice m1, m2, m4;
			m4.m[12] = 10; m1.m[12] = 10; m1.m[14] = 3; m2.m[12] = 10; m2.m[14] = 0;
			auto boite = [](const B::BoiteCol &b, const float *s, const float *rb, float *f, float *n, float *p, uint8 *o, int32 *k) {
				return B::BoiteBalayee(b.min, b.max, s, rb, f, n, p, o, k);
			};
			auto aucun = [](const float *, float, const std::function<void(int)> &) {};
			Contact c;
			Verifier(B::TestGeneral(m1.m, m2.m, &ma, m4.m, m4.m, &mb, &c, false, false, false, false, false, 0xf, nullptr, true, boite, aucun) == 1
			         && pres(c.fraction, 0.5f) && pres(c.point[0], 10.0f) && pres(c.point[2], 1.0f) && c.normale[2] == 1.0f && c.o22 == 6, "FUN_0056c770 et FUN_00576660 : la chute sur la boîte, à la moitié");
		}
		// Le test fin contre un piéton (FUN_00469f70) : un piéton de quatre sphères posé en (5, 0, 0) (la sphère 0 l'englobe, la
		// sphère 3 est la tête), un projectile de 5 cm qui avance de 10 m en x.
		{
			struct Contact { float point[3] = {0, 0, 0}; float normale[3] = {0, 0, 0}; uint32 o1c = 9; uint16 o20 = 0, o22 = 0; float fraction = 0; };
			B::SphereCol sp[4] = {{{0, 0, 1}, 1.0f, 10, 0, {0, 0}}, {{0, 0, 0.4f}, 0.3f, 11, 0, {0, 0}}, {{0, 0, 1}, 0.3f, 12, 0, {0, 0}}, {{0, 0, 1.6f}, 0.3f, 13, 0, {0, 0}}};
			B::SphereCol pr[1] = {{{0, 0, 0}, 0.05f, 3, 0, {0, 0}}};
			B::DonneesCol ped, proj;
			ped.spheres = sp; ped.nSpheres = 4; proj.spheres = pr; proj.nSpheres = 1;
			B::Matrice m1, m2, m4;
			m4.m[12] = 5;
			m1.m[14] = 1.6f; m2.m[12] = 10; m2.m[14] = 1.6f;
			Contact c;
			float f0 = (5.0f - sqrtf(1.05f * 1.05f - 0.36f)) / 10.0f;
			Verifier(B::TestPieton(m1.m, m2.m, &proj, m4.m, &ped, &c) == 1 && fabsf(c.fraction - f0) < 1e-4f && c.o1c == 3 && c.o22 == 13 && c.o20 == 3
			         && pres(c.point[0], 4.7f) && pres(c.point[2], 1.6f) && pres(c.normale[0], -1.0f), "FUN_00469f70 : la sphère 0 donne la fraction, la sphère 3 (le trajet allongé de 100 fois) le point, la normale et le rang");
			m1.m[14] = 0.4f; m2.m[14] = 0.4f;
			Verifier(B::TestPieton(m1.m, m2.m, &proj, m4.m, &ped, &c) == 1 && fabsf(c.fraction - f0) < 1e-4f && c.o1c == 0 && c.o22 == 10 && pres(c.normale[2], -0.6f / 1.05f),
			         "FUN_00469f70 : la sphère 3 manquée, le contact reste celui de la sphère 0, la sphère 1 n'est pas essayée");
			sp[0].rayon = 0.1f; sp[0].centre[2] = 0;
			m1.m[14] = 1.0f; m2.m[14] = 1.0f;
			Verifier(B::TestPieton(m1.m, m2.m, &proj, m4.m, &ped, &c) == 1 && pres(c.fraction, 0.465f) && c.o1c == 2 && c.o22 == 12, "FUN_00469f70 : la sphère 0 manquée, les suivantes sans allonger, la sphère 2 touchée");
			m1.m[14] = 3.0f; m2.m[14] = 3.0f;
			Verifier(B::TestPieton(m1.m, m2.m, &proj, m4.m, &ped, &c) == 0 && c.fraction > 3e38f, "FUN_00469f70 : au-dessus du piéton, rien, la fraction à FLT_MAX");
			Verifier(B::TestPieton(m1.m, m2.m, (const B::DonneesCol *)nullptr, m4.m, &ped, &c) == 0, "FUN_00469f70 : sans données, 0");
		}
		// Le modèle d'un piéton (FUN_00572680) : un piéton en (10, 20, 5) de cap un quart de tour (son avant vers −x, sa droite vers
		// +y), six os donnés dans le monde, la fiche de Jimmy (os 2, 12, 18, 2, 4, 7).
		{
			typedef CModelePieton P;
			P::Fiche fiche;
			const int32 rangs[6] = {2, 12, 18, 2, 4, 7};
			const float rayons[6] = {0.24f, 0.24f, 0.19f, 0.1f, 0.2f, 0.2f};
			for(int k = 0; k < 6; k++) { fiche.os[k] = rangs[k]; fiche.rayon[k] = rayons[k]; }
			// les os dans le repère du piéton (x à droite, y devant, z en haut), puis dans le monde : (10 − y, 20 + x, 5 + z)
			auto local = [](int32 rang, float *l) {
				l[0] = 0; l[1] = 0; l[2] = 0;
				if(rang == 2) l[2] = 1.0f;
				else if(rang == 12) { l[1] = 0.05f; l[2] = 1.4f; }
				else if(rang == 18) { l[1] = 0.1f; l[2] = 1.7f; }
				else if(rang == 4) { l[0] = -0.1f; l[2] = 0.4f; }
				else if(rang == 7) { l[0] = 0.1f; l[2] = 0.4f; }
			};
			auto os = [&](int32 rang, float *p) { float l[3]; local(rang, l); p[0] = 10.0f - l[1]; p[1] = 20.0f + l[0]; p[2] = 5.0f + l[2]; };
			B::Matrice m4;
			m4.m[0] = 0; m4.m[1] = 1; m4.m[4] = -1; m4.m[5] = 0; m4.m[12] = 10; m4.m[13] = 20; m4.m[14] = 5;
			float pos[3] = {10, 20, 5};
			float cap = P::Cap(m4.m);
			P ped;
			ped.Construire(pos, cap, fiche, os);
			bool centres = pres(cap, 1.5707964f);
			for(int k = 0; k < 6; k++) {
				float l[3]; local(rangs[k], l);
				for(int i = 0; i < 3; i++) centres = centres && fabsf(ped.spheres[k].centre[i] - l[i]) < 1e-5f;
				centres = centres && ped.spheres[k].rayon == rayons[k];
			}
			Verifier(centres, "FUN_00572680 : les six os ramenés au repère du piéton (FUN_00412670, FUN_004130d0, FUN_00571280, FUN_00570e40), les six rayons de la fiche");
			float r = sqrtf(0.01f + 1.7f * 1.7f) + 0.19f;
			Verifier(fabsf(ped.rayon - r) < 1e-5f && ped.centre[0] == 0 && ped.centre[1] == 0 && ped.centre[2] == 0
			         && ped.min[0] == -ped.rayon && ped.min[1] == -ped.rayon && ped.min[2] == 0 && ped.max[0] == ped.rayon && ped.max[2] == ped.rayon,
			         "FUN_00572680 : l'englobante part de la sphère la plus loin (la tête), la boîte de (−R, −R, 0) à (R, R, R) (FUN_0056ea30, FUN_005646e0)");
			// un projectile de 5 cm qui vient de −x à la hauteur de la tête : il touche la sphère 2, posée par la matrice du piéton
			struct Contact { float point[3] = {0, 0, 0}; float normale[3] = {0, 0, 0}; uint32 o1c = 9; uint16 o20 = 0, o22 = 0; float fraction = 0; };
			B::SphereCol pr[1] = {{{0, 0, 0}, 0.05f, 3, 0, {0, 0}}};
			B::DonneesCol proj; proj.spheres = pr; proj.nSpheres = 1;
			B::Matrice m1, m2;
			m1.m[12] = 0; m1.m[13] = 20; m1.m[14] = 6.7f; m2.m[12] = 20; m2.m[13] = 20; m2.m[14] = 6.7f;
			Contact c;
			Verifier(B::TestPieton(m1.m, m2.m, &proj, m4.m, &ped, &c) == 1 && c.o1c == 2 && fabsf(c.fraction - (9.9f - 0.24f) / 20.0f) < 1e-4f && pres(c.normale[0], -1.0f),
			         "FUN_00572680 et FUN_00469f70 : le projectile touche la sphère de la tête, à 24 cm de l'os");
			// à la hauteur du bassin : la sphère 0 (24 cm) donne la fraction, la sphère 3 (10 cm, le même os) le rang
			m1.m[14] = 6.0f; m2.m[14] = 6.0f;
			Verifier(B::TestPieton(m1.m, m2.m, &proj, m4.m, &ped, &c) == 1 && c.o1c == 3 && fabsf(c.fraction - (10.0f - 0.29f) / 20.0f) < 1e-4f,
			         "FUN_00572680 et FUN_00469f70 : au bassin, la fraction de la sphère 0 et le rang de la sphère 3");
			// tous les os à l'origine d'un piéton de cap nul : le plus grand carré reste 0 et le rang reste 0
			auto plat = [&](int32, float *p) { p[0] = 10; p[1] = 20; p[2] = 5; };
			ped.Construire(pos, 0.0f, fiche, plat);
			Verifier(ped.rayon == 0.24f && ped.spheres[5].centre[2] == 0, "FUN_00572680 : tous les os à l'origine, l'englobante a le rayon de la sphère 0");
			float m[16] = {1, 2, 0, 0, 3, 4, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1};
			P::TournerZ(m, 1.5707964f);
			Verifier(pres(m[0], -2) && pres(m[1], 1) && pres(m[4], -4) && pres(m[5], 3) && pres(m[12], -6) && pres(m[13], 5) && m[14] == 7 && m[10] == 1,
			         "FUN_004130d0 : un quart de tour autour de z, la position tournée avec les trois axes");
		}
	}
	return g_ko ? 1 : 0;
}