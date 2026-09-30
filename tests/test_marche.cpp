// Marcher dans des volumes de collision construits à la main : ne lit aucun
// fichier du jeu. Un sol, une marche, un mur, une rampe, une sphère.
#include "collision/Marche.h"
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
	return g_ko ? 1 : 0;
}
