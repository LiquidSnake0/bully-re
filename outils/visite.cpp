// Visite temps réel d'une scène du jeu, à la première personne.
//   BULLY_DATA=<racine> build/outils/visite <fichier.ipb> [--pos x y z lacet tangage] [--image sortie.ppm] [--banc n]
//
// Le rendu est entièrement logiciel (src/render : Camera + RasterTrianglePersp),
// dans une image de 400 × 240, la définition de l'écran du haut de la New
// 3DS. SDL2 ne sert qu'à ouvrir la fenêtre, lire le clavier et la souris, et
// afficher l'image agrandie : rien du rendu n'en dépend, c'est ce qui le garde
// transposable.
//
// Touches : ZQSD ou WASD pour marcher, Espace / C pour monter / descendre,
// Maj pour aller vite, flèches ou souris (clic pour la capturer) pour
// regarder, P pour une capture PPM, Échap pour quitter.
//
// --image rend une seule image depuis la position de départ et quitte ;
// --banc n rend n images en tournant sur place et donne le temps moyen. Les
// deux marchent sans fenêtre, c'est ce qui permet de vérifier le rendu sans
// écran.
#include "commun.h"
#include "scene.h"
#include "../src/render/Camera.h"
#include <chrono>
#include <cmath>
#ifndef VISITE_SANS_SDL
#include <SDL.h>
#endif

namespace {

struct Visite {
	outil::Scene *s;
	std::vector<float> ombre;               // éclairage par triangle, calculé une fois
	std::vector<CVector> vue;               // sommets dans le repère caméra, par image
	RasterImage img;
	int32 dessines = 0;

	void Preparer(void){
		float lum[3] = {0.4f, -0.6f, 0.7f}; float ln = sqrtf(lum[0]*lum[0]+lum[1]*lum[1]+lum[2]*lum[2]); for(float &q : lum) q /= ln;
		ombre.resize(s->tri.size() / 3);
		for(size_t i = 0; i < s->tri.size(); i += 3){
			const CVector &A = s->pts[s->tri[i]], &B = s->pts[s->tri[i+1]], &C = s->pts[s->tri[i+2]];
			float ux = B.x-A.x, uy = B.y-A.y, uz = B.z-A.z, wx = C.x-A.x, wy = C.y-A.y, wz = C.z-A.z;
			float nx = uy*wz-uz*wy, ny = uz*wx-ux*wz, nz = ux*wy-uy*wx, nn = sqrtf(nx*nx+ny*ny+nz*nz); if(nn < 1e-9f) nn = 1;
			ombre[i/3] = 0.45f + 0.55f * fabsf((nx*lum[0]+ny*lum[1]+nz*lum[2])/nn);
		}
		vue.resize(s->pts.size());
		img = RasterCreate(400, 240, 0, 0, 0);
	}

	void Rendre(const Camera &cam){
		RasterClear(img, 128, 150, 170);
		for(size_t i = 0; i < s->pts.size(); i++) vue[i] = cam.ToView(s->pts[i]);
		float foc = (cam.h * 0.5f) / tanf(cam.fovY * 0.5f);
		float mx = cam.w * 0.5f / foc, my = cam.h * 0.5f / foc;   // demi-ouverture en x/z et y/z
		dessines = 0;
		// Deux passes : les opaques et le test alpha écrivent la profondeur,
		// puis le mélange et l'ajout se posent par-dessus sans l'écrire.
		for(int passe = 0; passe < 2; passe++)
		for(size_t i = 0; i < s->tri.size(); i += 3){
			uint8 mode = s->triMode[i/3];
			if((mode >= RASTER_MELANGE) != (passe == 1)) continue;
			CVector v[3] = { vue[s->tri[i]], vue[s->tri[i+1]], vue[s->tri[i+2]] };
			// Rejet grossier contre le cône de vue : les trois sommets du même côté d'un plan.
			if(v[0].z < cam.near_ && v[1].z < cam.near_ && v[2].z < cam.near_) continue;
			if(v[0].x >  mx * v[0].z && v[1].x >  mx * v[1].z && v[2].x >  mx * v[2].z) continue;
			if(v[0].x < -mx * v[0].z && v[1].x < -mx * v[1].z && v[2].x < -mx * v[2].z) continue;
			if(v[0].y >  my * v[0].z && v[1].y >  my * v[1].z && v[2].y >  my * v[2].z) continue;
			if(v[0].y < -my * v[0].z && v[1].y < -my * v[1].z && v[2].y < -my * v[2].z) continue;
			float uv[6] = { s->uv[s->tri[i]*2], s->uv[s->tri[i]*2+1], s->uv[s->tri[i+1]*2], s->uv[s->tri[i+1]*2+1], s->uv[s->tri[i+2]*2], s->uv[s->tri[i+2]*2+1] };
			RasterPVertex o[6];
			int32 n = CameraClipProject(cam, v, uv, o);
			int32 tx = s->triTex[i/3];
			for(int32 k = 0; k < n; k++) RasterTrianglePersp(img, &o[k*3], tx >= 0 ? s->texPtr[tx] : nil, ombre[i/3], (eRasterMode)mode, s->triSeuil[i/3]);
			dessines += n;
		}
	}
};

// Point de départ : en dehors de la boîte de la scène, en hauteur, regardant son centre.
Camera
Depart(const outil::Scene &s)
{
	CVector mn = s.pts[0], mx = s.pts[0];
	for(auto &p : s.pts){ mn.x = fminf(mn.x, p.x); mn.y = fminf(mn.y, p.y); mn.z = fminf(mn.z, p.z); mx.x = fmaxf(mx.x, p.x); mx.y = fmaxf(mx.y, p.y); mx.z = fmaxf(mx.z, p.z); }
	CVector c((mn.x+mx.x)/2, (mn.y+mx.y)/2, (mn.z+mx.z)/2);
	printf("  boîte de la scène : x %.1f .. %.1f, y %.1f .. %.1f, z %.1f .. %.1f\n", mn.x, mx.x, mn.y, mx.y, mn.z, mx.z);
	Camera cam;
	cam.pos = CVector(c.x - (mx.x - mn.x) * 0.7f, c.y - (mx.y - mn.y) * 0.7f, c.z + (mx.z - mn.z) * 0.8f);
	cam.yaw = atan2f(c.y - cam.pos.y, c.x - cam.pos.x);
	cam.pitch = atan2f(c.z - cam.pos.z, hypotf(c.x - cam.pos.x, c.y - cam.pos.y));
	return cam;
}

} // namespace

int
main(int argc, char **argv)
{
	if(argc < 2){ fprintf(stderr, "usage : visite <fichier.ipb> [--pos x y z lacet tangage] [--image sortie.ppm] [--banc n]\n"); return 2; }
	std::string ipb = argv[1], image; int banc = 0; bool pos = false; float px = 0, py = 0, pz = 0, lacet = 0, tangage = 0;
	for(int i = 2; i < argc; i++){
		if(strcmp(argv[i], "--image") == 0 && i + 1 < argc) image = argv[++i];
		else if(strcmp(argv[i], "--banc") == 0 && i + 1 < argc) banc = atoi(argv[++i]);
		else if(strcmp(argv[i], "--pos") == 0 && i + 5 < argc){ pos = true; px = (float)atof(argv[++i]); py = (float)atof(argv[++i]); pz = (float)atof(argv[++i]); lacet = (float)atof(argv[++i]); tangage = (float)atof(argv[++i]); }
	}

	outil::Archives a;
	if(!outil::Ouvrir(a)) return 1;
	outil::Scene s;
	if(!outil::ChargerPlacements(a, ipb, s)) return 1;
	Visite v; v.s = &s; v.Preparer();
	Camera cam = Depart(s);
	if(pos){ cam.pos = CVector(px, py, pz); cam.yaw = lacet * PI / 180; cam.pitch = tangage * PI / 180; }

	using horloge = std::chrono::steady_clock;
	if(!image.empty() || banc > 0){
		auto t0 = horloge::now();
		int n = banc > 0 ? banc : 1;
		float yaw0 = cam.yaw;
		for(int k = 0; k < n; k++){ cam.yaw = yaw0 + k * (2 * PI / n); v.Rendre(cam); }
		double ms = std::chrono::duration<double, std::milli>(horloge::now() - t0).count() / n;
		printf("  %d image(s) 400×240, %.1f ms par image (%.0f i/s), %d triangles dessinés à la dernière\n", n, ms, 1000.0 / ms, v.dessines);
		if(!image.empty()){ cam.yaw = yaw0; v.Rendre(cam); bool ok = RasterWritePPM(v.img, image.c_str()); printf("  → %s%s\n", image.c_str(), ok ? "" : " (échec)"); }
		return 0;
	}

#ifdef VISITE_SANS_SDL
	fprintf(stderr, "compilé sans SDL : utiliser --image ou --banc\n");
	return 2;
#else
	if(SDL_Init(SDL_INIT_VIDEO) != 0){ fprintf(stderr, "SDL : %s\n", SDL_GetError()); return 1; }
	SDL_Window *win = SDL_CreateWindow("bully-re : visite", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 480, SDL_WINDOW_RESIZABLE);
	SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
	SDL_RenderSetLogicalSize(ren, 400, 240);
	SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 400, 240);
	bool fini = false, souris = false; int captures = 0;
	auto avant = horloge::now(); double cumul = 0; int images = 0;
	while(!fini){
		SDL_Event e;
		while(SDL_PollEvent(&e)){
			if(e.type == SDL_QUIT) fini = true;
			else if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE){ if(souris){ souris = false; SDL_SetRelativeMouseMode(SDL_FALSE); } else fini = true; }
			else if(e.type == SDL_MOUSEBUTTONDOWN){ souris = true; SDL_SetRelativeMouseMode(SDL_TRUE); }
			else if(e.type == SDL_MOUSEMOTION && souris){ cam.yaw -= e.motion.xrel * 0.003f; cam.pitch -= e.motion.yrel * 0.003f; }
			else if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_p){
				char nom[64]; snprintf(nom, sizeof nom, "visite-%02d.ppm", captures++);
				RasterWritePPM(v.img, nom); printf("capture : %s (pos %.2f %.2f %.2f, lacet %.1f, tangage %.1f)\n", nom, cam.pos.x, cam.pos.y, cam.pos.z, cam.yaw * 180 / PI, cam.pitch * 180 / PI);
			}
		}
		auto maintenant = horloge::now();
		float dt = std::chrono::duration<float>(maintenant - avant).count(); avant = maintenant;
		const Uint8 *k = SDL_GetKeyboardState(nil);
		float vit = (k[SDL_SCANCODE_LSHIFT] ? 12.0f : 3.0f) * dt;
		// Positions des touches (scancodes) : ZQSD en AZERTY et WASD en QWERTY tombent au même endroit.
		cam.Move((k[SDL_SCANCODE_W] ? vit : 0) - (k[SDL_SCANCODE_S] ? vit : 0),
		         (k[SDL_SCANCODE_D] ? vit : 0) - (k[SDL_SCANCODE_A] ? vit : 0),
		         (k[SDL_SCANCODE_SPACE] ? vit : 0) - (k[SDL_SCANCODE_C] ? vit : 0));
		float rot = 1.6f * dt;
		cam.yaw += (k[SDL_SCANCODE_LEFT] ? rot : 0) - (k[SDL_SCANCODE_RIGHT] ? rot : 0);
		cam.pitch += (k[SDL_SCANCODE_UP] ? rot : 0) - (k[SDL_SCANCODE_DOWN] ? rot : 0);
		if(cam.pitch > 1.5f) cam.pitch = 1.5f; if(cam.pitch < -1.5f) cam.pitch = -1.5f;

		auto t0 = horloge::now();
		v.Rendre(cam);
		cumul += std::chrono::duration<double, std::milli>(horloge::now() - t0).count(); images++;
		SDL_UpdateTexture(tex, nil, v.img.rgb, 400 * 3);
		SDL_RenderClear(ren); SDL_RenderCopy(ren, tex, nil, nil); SDL_RenderPresent(ren);
		if(images == 30){
			char titre[160];
			snprintf(titre, sizeof titre, "bully-re : %s — rendu %.1f ms, %d triangles — %.1f %.1f %.1f", ipb.c_str(), cumul / images, v.dessines, cam.pos.x, cam.pos.y, cam.pos.z);
			SDL_SetWindowTitle(win, titre); cumul = 0; images = 0;
		}
	}
	SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
	return 0;
#endif
}
