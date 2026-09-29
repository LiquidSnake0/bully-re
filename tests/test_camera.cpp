// Caméra en perspective et triangle en perspective correcte : ne lit aucun
// fichier du jeu. Vérifie la projection, le découpage au plan proche et
// l'interpolation de texture en u/w.
#include "render/Camera.h"
#include <cmath>
#include <cstdio>

static int g_ko = 0;
static void Verifier(bool ok, const char *quoi){ if(!ok){ printf("ECHEC : %s\n", quoi); g_ko++; } }
static bool Proche(float a, float b, float eps = 1e-3f){ return fabsf(a - b) < eps; }

int
main(void)
{
	Camera cam;
	cam.pos = CVector(10, 20, 2);
	cam.yaw = 0.7f; cam.pitch = 0.2f;

	// 1. Le point droit devant se projette au centre de l'écran.
	CVector f = cam.Forward();
	CVector devant(cam.pos.x + f.x * 5, cam.pos.y + f.y * 5, cam.pos.z + f.z * 5);
	CVector vd = cam.ToView(devant);
	Verifier(Proche(vd.x, 0) && Proche(vd.y, 0) && Proche(vd.z, 5), "devant : repère caméra (0, 0, 5)");
	RasterPVertex p = cam.Project(vd, 0, 0);
	Verifier(Proche(p.x, cam.w / 2.0f) && Proche(p.y, cam.h / 2.0f), "devant : centre de l'écran");

	// 2. Droite à droite, haut en haut (y écran vers le bas).
	CVector r = cam.Right(), u = cam.Up();
	CVector adroite(devant.x + r.x, devant.y + r.y, devant.z + r.z), enhaut(devant.x + u.x, devant.y + u.y, devant.z + u.z);
	Verifier(cam.Project(cam.ToView(adroite), 0, 0).x > cam.w / 2.0f, "un point à droite tombe à droite");
	Verifier(cam.Project(cam.ToView(enhaut), 0, 0).y < cam.h / 2.0f, "un point en haut tombe en haut");
	// Le repère est orthonormé et direct.
	float rf = r.x*f.x + r.y*f.y + r.z*f.z, uf = u.x*f.x + u.y*f.y + u.z*f.z, ru = r.x*u.x + r.y*u.y + r.z*u.z;
	Verifier(Proche(rf, 0) && Proche(uf, 0) && Proche(ru, 0), "axes orthogonaux");

	// 3. Découpage au plan proche.
	float uv[6] = {0, 0, 1, 0, 0, 1};
	RasterPVertex out[6];
	CVector derriere[3] = {CVector(0, 0, -1), CVector(1, 0, -2), CVector(0, 1, -1)};
	Verifier(CameraClipProject(cam, derriere, uv, out) == 0, "triangle derrière l'œil : rien");
	CVector devant3[3] = {CVector(0, 0, 3), CVector(1, 0, 3), CVector(0, 1, 3)};
	Verifier(CameraClipProject(cam, devant3, uv, out) == 1, "triangle devant : un triangle");
	CVector traverse[3] = {CVector(-1, 0, -1), CVector(1, 0, 3), CVector(0, 1, 3)};
	int32 n = CameraClipProject(cam, traverse, uv, out);
	Verifier(n == 2, "un sommet derrière : deux triangles");
	bool bornes = true;
	for(int32 i = 0; i < n * 3; i++) if(out[i].invW > 1.0f / cam.near_ + 1e-3f || out[i].invW <= 0) bornes = false;
	Verifier(bornes, "aucun sommet découpé plus près que le plan proche");

	// 4. Deux triangles d'un carré face à l'écran : pas de trou ni de double
	//    couverture sur l'arête commune, et la texture tombe juste.
	uint8 damier[4 * 4 * 4];
	for(int32 i = 0; i < 16; i++){ uint8 c = ((i % 4) + (i / 4)) % 2 ? 255 : 0; damier[i*4] = c; damier[i*4+1] = c; damier[i*4+2] = c; damier[i*4+3] = 255; }
	RasterTexture tex{4, 4, damier};
	RasterImage img = RasterCreate(64, 64, 10, 20, 30);
	RasterPVertex a{8, 8, 0.5f, 0, 0}, b{56, 8, 0.5f, 0.5f, 0}, c{56, 56, 0.5f, 0.5f, 0.5f}, d{8, 56, 0.5f, 0, 0.5f};
	RasterPVertex t1[3] = {a, b, c}, t2[3] = {a, c, d};
	RasterTrianglePersp(img, t1, &tex, 1.0f);
	RasterTrianglePersp(img, t2, &tex, 1.0f);
	int32 couverts = 0;
	for(int32 i = 0; i < 64 * 64; i++) if(img.depth[i] < 1e29f) couverts++;
	Verifier(couverts == 48 * 48, "le carré couvre exactement 48 × 48 pixels");
	// À profondeur constante, u = uw / invW : coin haut gauche u≈0 → case noire,
	// 3/8 de la largeur → u≈0.375 → deuxième case → blanche.
	Verifier(img.rgb[(10 * 64 + 10) * 3] == 0, "texture : case (0,0) noire");
	Verifier(img.rgb[(10 * 64 + 8 + 18) * 3] == 255, "texture : case (1,0) blanche");

	// 5. Perspective : un carré incliné, plus loin en haut. La moitié de la
	//    texture ne tombe PAS à la moitié de l'écran (elle remonte vers le fond).
	RasterClear(img, 0, 0, 0);
	float zp = 2.0f, zl = 6.0f;
	RasterPVertex e{0, 63, 1/zp, 0, 1/zp}, g{63, 63, 1/zp, 1/zp, 1/zp}, h2{63, 0, 1/zl, 1/zl, 0}, k{0, 0, 1/zl, 0, 0};
	RasterPVertex q1[3] = {e, g, h2}, q2[3] = {e, h2, k};
	uint8 bande[2 * 2 * 4] = {255,255,255,255, 255,255,255,255, 0,0,0,255, 0,0,0,255};   // haut blanc, bas noir
	RasterTexture tb{2, 2, bande};
	RasterTrianglePersp(img, q1, &tb, 1.0f);
	RasterTrianglePersp(img, q2, &tb, 1.0f);
	int32 frontiere = -1;
	for(int32 y = 0; y < 64; y++) if(img.rgb[(y * 64 + 32) * 3] == 0){ frontiere = y; break; }
	// v = 0.5 entre z = 6 (haut) et z = 2 (bas) : 1/z interpolé linéairement
	// à l'écran donne la ligne y = 63 · (1/4 − 1/6) / (1/2 − 1/6) ≈ 15,75.
	Verifier(frontiere >= 14 && frontiere <= 18, "perspective : la moitié de la texture tombe vers y ≈ 16, pas 32");
	printf("  frontière de la texture à y = %d (affine donnerait 32)\n", frontiere);
	RasterFree(img);

	printf("test_camera : %s\n", g_ko ? "ECHEC" : "ok");
	return g_ko ? 1 : 0;
}
