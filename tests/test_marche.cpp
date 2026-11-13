// Marcher dans des volumes de collision construits à la main : ne lit aucun
// fichier du jeu. Un sol, une marche, un mur, une rampe, une sphère.
#include "collision/Marche.h"
#include "collision/Balayes.h"
#include <cmath>
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
	}
	return g_ko ? 1 : 0;
}