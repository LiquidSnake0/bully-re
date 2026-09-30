// Visite temps réel d'une scène du jeu, à la première personne.
//   BULLY_DATA=<racine> build/outils/visite <fichier.ipb> [--pos x y z lacet tangage] [--marche]
//                                          [--image sortie.ppm] [--banc n] [--promenade n]
//                                          [--monde] [--rayon m] [--survol n]
//
// Le rendu est entièrement logiciel (src/render : Camera + RasterTrianglePersp),
// dans une image de 400 × 240, la définition de l'écran du haut de la New
// 3DS. SDL2 ne sert qu'à ouvrir la fenêtre, lire le clavier et la souris, et
// afficher l'image agrandie : rien du rendu n'en dépend, c'est ce qui le garde
// transposable.
//
// Touches : ZQSD ou WASD pour marcher, Espace / C pour monter / descendre,
// Maj pour aller vite, flèches ou souris (clic pour la capturer) pour
// regarder, F pour passer du vol à la marche (et retour), P pour une capture
// PPM, Échap pour quitter.
//
// En marche, la caméra a un corps : les volumes de collision du jeu (.col),
// posés avec la même transformation que les modèles, la portent sur le sol,
// lui font monter les marches et l'arrêtent devant les murs
// (src/collision/Marche, vérifié par tests/test_marche).
//
// --image rend une seule image depuis la position de départ et quitte ;
// --banc n rend n images en tournant sur place et donne le temps moyen. Les
// deux marchent sans fenêtre, c'est ce qui permet de vérifier le rendu sans
// écran. --promenade n marche n images droit devant (en mode marche) et
// affiche le trajet : on vérifie sans écran qu'on tient au sol et qu'un mur
// arrête.
//
// Dehors, les scènes s'enchaînent. Le monde extérieur est découpé en fichiers
// de placements : les quartiers « t* » (bâtiments, sols), les tuiles « zone_* »
// (mobilier urbain) et tGlobal (relief, pylônes, sur toute la carte). Avec un
// de ces fichiers, ou --monde, la visite charge tous ceux dont l'emprise est à
// moins de --rayon mètres (60 par défaut) et libère ceux qui s'éloignent au-delà
// du double, pendant qu'on marche : on passe d'un quartier à l'autre. Les
// intérieurs (i*) restent des scènes seules, posées à part dans le monde.
// --survol n vole n images droit devant, à 12 m/s et sans collisions, et
// affiche ce qui se charge et se libère : l'enchaînement se vérifie sans écran.
#include "commun.h"
#include "scene.h"
#include "monde.h"
#include "../src/render/Camera.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#ifndef VISITE_SANS_SDL
#include <SDL.h>
#endif

namespace {

// Une scène chargée : sa géométrie, son éclairage par triangle, ses sommets
// dans le repère caméra (recalculés à chaque image) et ses collisions.
struct Morceau {
	std::string nom;
	outil::Scene *s = nil;
	std::vector<float> ombre;
	std::vector<CVector> vue;
	CMondeCollision col;
	std::vector<int32> visibles;            // blocs retenus pour l'image en cours
	std::vector<float> profondeur;          // leur distance au plan de la caméra
	~Morceau(){ delete s; }

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
	}
};

struct Visite {
	std::vector<Morceau*> morceaux;
	RasterImage img;
	int32 dessines = 0;

	Visite(void){ img = RasterCreate(400, 240, 0, 0, 0); }

	float loin = 250;                       // au-delà, un modèle n'est pas dessiné (mètres)
	int32 modelesVus = 0, arriere = 0;
	struct Rang { float z; int32 morceau, bloc; };
	std::vector<Rang> ordre;                // les modèles à dessiner, triés

	void Rendre(const Camera &cam){
		RasterClear(img, 128, 150, 170);
		float foc = (cam.h * 0.5f) / tanf(cam.fovY * 0.5f);
		float mx = cam.w * 0.5f / foc, my = cam.h * 0.5f / foc;   // demi-ouverture en x/z et y/z
		// Pour une sphère, le plan latéral x = mx·z est à distance (x − mx·z) / √(1 + mx²).
		float kx = sqrtf(1 + mx * mx), ky = sqrtf(1 + my * my);
		// Les modèles visibles d'abord : sphère contre le cône de vue et la distance.
		// Seuls leurs sommets passent dans le repère caméra.
		for(Morceau *m : morceaux){
			const outil::Scene *s = m->s;
			m->visibles.clear(); m->profondeur.clear();
			for(size_t b = 0; b < s->blocs.size(); b++){
				const outil::Scene::Bloc &bl = s->blocs[b];
				CVector c = cam.ToView(bl.centre); float r = bl.rayon;
				if(c.z < cam.near_ - r || c.z > loin + r) continue;
				if(c.x - mx * c.z > r * kx || -c.x - mx * c.z > r * kx) continue;
				if(c.y - my * c.z > r * ky || -c.y - my * c.z > r * ky) continue;
				m->visibles.push_back((int32)b);
				m->profondeur.push_back(c.z);
				for(int32 i = bl.ptDebut; i < bl.ptFin; i++) m->vue[i] = cam.ToView(s->pts[i]);
			}
		}
		// Tous morceaux confondus, du plus proche au plus lointain : ce qui est
		// devant remplit la profondeur d'abord, et les pixels cachés derrière
		// s'arrêtent au test de profondeur, avant la texture.
		ordre.clear();
		for(size_t k = 0; k < morceaux.size(); k++)
			for(size_t j = 0; j < morceaux[k]->visibles.size(); j++)
				ordre.push_back({morceaux[k]->profondeur[j], (int32)k, morceaux[k]->visibles[j]});
		std::sort(ordre.begin(), ordre.end(), [](const Rang &p, const Rang &q){ return p.z < q.z; });
		dessines = 0; modelesVus = 0; arriere = 0;
		for(Morceau *m : morceaux) modelesVus += (int32)m->visibles.size();
		// Deux passes, sur tous les morceaux : les opaques et le test alpha
		// écrivent la profondeur, puis le mélange et l'ajout se posent par-dessus
		// sans l'écrire.
		// La transparence se pose de l'arrière vers l'avant : la seconde passe
		// parcourt la liste à l'envers.
		for(int passe = 0; passe < 2; passe++)
		for(size_t r = 0; r < ordre.size(); r++){
		const Rang &rg = passe == 0 ? ordre[r] : ordre[ordre.size() - 1 - r];
		Morceau *m = morceaux[rg.morceau];
		const outil::Scene *s = m->s;
		const outil::Scene::Bloc &bl = s->blocs[rg.bloc];
		for(int32 i = bl.triDebut; i < bl.triFin; i += 3){
			uint8 mode = s->triMode[i/3];
			if((mode >= RASTER_MELANGE) != (passe == 1)) continue;
			CVector v[3] = { m->vue[s->tri[i]], m->vue[s->tri[i+1]], m->vue[s->tri[i+2]] };
			// Rejet grossier contre le cône de vue : les trois sommets du même côté d'un plan.
			if(v[0].z < cam.near_ && v[1].z < cam.near_ && v[2].z < cam.near_) continue;
			if(v[0].x >  mx * v[0].z && v[1].x >  mx * v[1].z && v[2].x >  mx * v[2].z) continue;
			if(v[0].x < -mx * v[0].z && v[1].x < -mx * v[1].z && v[2].x < -mx * v[2].z) continue;
			if(v[0].y >  my * v[0].z && v[1].y >  my * v[1].z && v[2].y >  my * v[2].z) continue;
			if(v[0].y < -my * v[0].z && v[1].y < -my * v[1].z && v[2].y < -my * v[2].z) continue;
			float uv[6] = { s->uv[s->tri[i]*2], s->uv[s->tri[i]*2+1], s->uv[s->tri[i+1]*2], s->uv[s->tri[i+1]*2+1], s->uv[s->tri[i+2]*2], s->uv[s->tri[i+2]*2+1] };
			RasterPVertex o[6];
			int32 n = CameraClipProject(cam, v, uv, o);
			// Face arrière : à l'écran, les sommets tournent dans l'autre sens.
			// Le découpage garde l'ordre, un seul test suffit pour les deux moitiés.
			if(n > 0 && !s->triDeuxFaces[i/3] && (o[1].x - o[0].x) * (o[2].y - o[0].y) - (o[1].y - o[0].y) * (o[2].x - o[0].x) > 0){ arriere++; continue; }
			int32 tx = s->triTex[i/3];
			for(int32 k = 0; k < n; k++) RasterTrianglePersp(img, &o[k*3], tx >= 0 ? s->texPtr[tx] : nil, m->ombre[i/3], (eRasterMode)mode, s->triSeuil[i/3]);
			dessines += n;
		}
		}
	}
};

// Les fichiers du monde extérieur, ceux qui s'enchaînent : quartiers, tuiles et
// relief. Pas les intérieurs, ni les courses (iMGRace*), ni les terrains d'essai.
bool
Exterieur(const std::string &nom)
{
	std::string n = outil::Minuscules(nom.c_str());
	if(n == "ttest.ipb" || n == "ftest.ipb") return false;
	return n.compare(0, 5, "zone_") == 0 || n[0] == 't';
}

// Point de départ : en dehors de la boîte de la scène, en hauteur, regardant son centre.
Camera
Depart(const outil::Scene &s)
{
	// (intérieurs : la scène entière est petite, on la regarde de dehors)
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
	if(argc < 2){ fprintf(stderr, "usage : visite <fichier.ipb> [--pos x y z lacet tangage] [--marche] [--image sortie.ppm] [--banc n] [--promenade n] [--monde] [--rayon m]\n"); return 2; }
	std::string ipb = argv[1], image; int banc = 0, promenade = 0, survol = 0; bool pos = false, marche = false, monde = Exterieur(ipb); float px = 0, py = 0, pz = 0, lacet = 0, tangage = 0, rayon = 60;
	for(int i = 2; i < argc; i++){
		if(strcmp(argv[i], "--image") == 0 && i + 1 < argc) image = argv[++i];
		else if(strcmp(argv[i], "--banc") == 0 && i + 1 < argc) banc = atoi(argv[++i]);
		else if(strcmp(argv[i], "--promenade") == 0 && i + 1 < argc){ promenade = atoi(argv[++i]); marche = true; }
		else if(strcmp(argv[i], "--marche") == 0) marche = true;
		else if(strcmp(argv[i], "--survol") == 0 && i + 1 < argc) survol = atoi(argv[++i]);
		else if(strcmp(argv[i], "--monde") == 0) monde = true;
		else if(strcmp(argv[i], "--rayon") == 0 && i + 1 < argc) rayon = (float)atof(argv[++i]);
		else if(strcmp(argv[i], "--pos") == 0 && i + 5 < argc){ pos = true; px = (float)atof(argv[++i]); py = (float)atof(argv[++i]); pz = (float)atof(argv[++i]); lacet = (float)atof(argv[++i]); tangage = (float)atof(argv[++i]); }
	}

	outil::Archives a;
	if(!outil::Ouvrir(a)) return 1;
	// Collisions : chargées une fois pour tout le monde, posées par morceau.
	std::map<std::string, CColModel*> cols;
	outil::ChargerToutesCollisions(a, cols);

	Visite v;
	CMondeCollision sol;                    // les collisions de tous les morceaux chargés
	auto Charger = [&](const std::string &nom) -> bool {
		Morceau *m = new Morceau; m->nom = nom; m->s = new outil::Scene;
		if(!outil::ChargerPlacements(a, nom, *m->s)){ delete m; return false; }
		m->Preparer();
		int poses, sans; outil::PoserCollisions(a, *m->s, cols, m->col, &poses, &sans);
		printf("  collisions : %d posées (%d sans volume), %zu triangles, %zu sphères\n", poses, sans, m->col.tri.size() / 3, m->col.sphCentre.size());
		v.morceaux.push_back(m);
		return true;
	};
	auto Refaire = [&](void){
		sol = CMondeCollision();
		for(Morceau *m : v.morceaux){
			sol.tri.insert(sol.tri.end(), m->col.tri.begin(), m->col.tri.end());
			sol.sphCentre.insert(sol.sphCentre.end(), m->col.sphCentre.begin(), m->col.sphCentre.end());
			sol.sphRayon.insert(sol.sphRayon.end(), m->col.sphRayon.begin(), m->col.sphRayon.end());
		}
	};

	// Dehors : la carte des emprises, puis ce qui est à portée de (x, y).
	std::vector<outil::EmpriseIpb> carte;
	if(monde){
		for(const auto &e : outil::CarteDesIpb(a)) if(Exterieur(e.nom)) carte.push_back(e);
		printf("monde extérieur : %zu fichiers de placements, rayon %.0f m\n", carte.size(), rayon);
	}
	// Charge ce qui entre dans le rayon, libère ce qui sort du double. Rend
	// vrai si la liste a changé (il faut alors refaire les collisions).
	auto Actualiser = [&](float x, float y) -> bool {
		bool change = false;
		for(auto it = v.morceaux.begin(); it != v.morceaux.end();){
			const outil::EmpriseIpb *e = nil;
			for(const auto &c : carte) if(c.nom == (*it)->nom) e = &c;
			if(e && e->Distance(x, y) > 2 * rayon){ printf("  libère %s\n", (*it)->nom.c_str()); delete *it; it = v.morceaux.erase(it); change = true; }
			else ++it;
		}
		for(const auto &e : carte){
			if(e.Distance(x, y) > rayon) continue;
			bool deja = false; for(Morceau *m : v.morceaux) if(m->nom == e.nom) deja = true;
			if(!deja && Charger(e.nom)) change = true;
		}
		if(change) Refaire();
		return change;
	};

	Camera cam;
	if(!monde){
		if(!Charger(ipb)) return 1;
		Refaire();
		cam = Depart(*v.morceaux[0]->s);
	}else{
		// Départ au centre du fichier demandé, à hauteur d'yeux au-dessus du sol.
		const outil::EmpriseIpb *e = nil;
		for(const auto &c : carte) if(strcasecmp(c.nom.c_str(), ipb.c_str()) == 0) e = &c;
		float cx = e ? (e->xmin + e->xmax) / 2 : 0, cy = e ? (e->ymin + e->ymax) / 2 : 0;
		if(pos){ cx = px; cy = py; }
		Actualiser(cx, cy);
		if(v.morceaux.empty()){ fprintf(stderr, "rien à moins de %.0f m de (%.0f, %.0f)\n", rayon, cx, cy); return 1; }
		float z = e ? e->zmax + 60 : 100, zs;
		if(sol.Sol(cx, cy, z, 400.0f, &zs)) z = zs;
		cam.pos = CVector(cx, cy, z + 1.6f); cam.yaw = 0; cam.pitch = 0;
	}
	if(pos){ cam.pos = CVector(px, py, pz); cam.yaw = lacet * PI / 180; cam.pitch = tangage * PI / 180; }
	{
		size_t t = 0; for(Morceau *m : v.morceaux) t += m->s->tri.size() / 3;
		printf("  %zu morceau(x) chargé(s), %zu triangles, collisions : %zu triangles, %zu sphères\n", v.morceaux.size(), t, sol.tri.size() / 3, sol.sphCentre.size());
	}
	CMarcheur corps;
	auto PoserCorps = [&](void){
		corps.pos = CVector(cam.pos.x, cam.pos.y, cam.pos.z - corps.hauteurYeux);
		float z; if(sol.Sol(corps.pos.x, corps.pos.y, cam.pos.z, 100.0f, &z)) corps.pos.z = z;
		corps.vz = 0;
	};
	if(marche) PoserCorps();
	auto Pas = [&](float avant, float droite, float dt){
		float dx = cosf(cam.yaw) * avant + sinf(cam.yaw) * droite, dy = sinf(cam.yaw) * avant - cosf(cam.yaw) * droite;
		corps.Avancer(sol, dx, dy, dt);
		cam.pos = CVector(corps.pos.x, corps.pos.y, corps.pos.z + corps.hauteurYeux);
	};

	if(survol > 0){
		for(int k = 1; k <= survol; k++){
			cam.Move(12.0f / 60, 0, 0);
			if(k % 30 == 0 && Actualiser(cam.pos.x, cam.pos.y)){
				size_t t = 0; for(Morceau *m : v.morceaux) t += m->s->tri.size() / 3;
				printf("  %5.1f s : %.0f %.0f, %zu morceau(x), %zu triangles\n", k / 60.0f, cam.pos.x, cam.pos.y, v.morceaux.size(), t);
			}
		}
		if(image.empty()) return 0;
	}
	if(promenade > 0){
		printf("  départ : pieds %.2f %.2f %.2f\n", corps.pos.x, corps.pos.y, corps.pos.z);
		int arrets = 0; CVector avant = corps.pos;
		for(int k = 1; k <= promenade; k++){
			Pas(3.0f / 60, 0, 1.0f / 60);
			float fait = hypotf(corps.pos.x - avant.x, corps.pos.y - avant.y);
			if(fait < 0.3f * 3.0f / 60) arrets++;
			avant = corps.pos;
			if(monde && k % 30 == 0) Actualiser(corps.pos.x, corps.pos.y);
			if(k % 60 == 0) printf("  %4.1f s : pieds %.2f %.2f %.2f %s\n", k / 60.0f, corps.pos.x, corps.pos.y, corps.pos.z, corps.auSol ? "au sol" : "en l'air");
		}
		printf("  %d images sur %d presque immobiles (bloqué par un mur)\n", arrets, promenade);
		if(image.empty()) return 0;         // la promenade est une vérification sans fenêtre
	}

	using horloge = std::chrono::steady_clock;
	if(!image.empty() || banc > 0){
		auto t0 = horloge::now();
		int n = banc > 0 ? banc : 1;
		float yaw0 = cam.yaw;
		for(int k = 0; k < n; k++){ cam.yaw = yaw0 + k * (2 * PI / n); v.Rendre(cam); }
		double ms = std::chrono::duration<double, std::milli>(horloge::now() - t0).count() / n;
		printf("  %d image(s) 400×240, %.1f ms par image (%.0f i/s), %d triangles dessinés à la dernière (%d modèles retenus, %d faces arrière)\n", n, ms, 1000.0 / ms, v.dessines, v.modelesVus, v.arriere);
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
	if(marche) SDL_SetWindowTitle(win, "bully-re : marche (F pour voler)");
	auto avant = horloge::now(); double cumul = 0; int images = 0;
	while(!fini){
		SDL_Event e;
		while(SDL_PollEvent(&e)){
			if(e.type == SDL_QUIT) fini = true;
			else if(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE){ if(souris){ souris = false; SDL_SetRelativeMouseMode(SDL_FALSE); } else fini = true; }
			else if(e.type == SDL_MOUSEBUTTONDOWN){ souris = true; SDL_SetRelativeMouseMode(SDL_TRUE); }
			else if(e.type == SDL_MOUSEMOTION && souris){ cam.yaw -= e.motion.xrel * 0.003f; cam.pitch -= e.motion.yrel * 0.003f; }
			else if(e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_F){ marche = !marche; if(marche) PoserCorps(); }
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
		float av = (k[SDL_SCANCODE_W] ? vit : 0) - (k[SDL_SCANCODE_S] ? vit : 0), dr = (k[SDL_SCANCODE_D] ? vit : 0) - (k[SDL_SCANCODE_A] ? vit : 0);
		if(marche){
			if(k[SDL_SCANCODE_SPACE] && corps.auSol) corps.vz = 4.0f;       // un saut
			Pas(av, dr, dt > 0.1f ? 0.1f : dt);
		}else
			cam.Move(av, dr, (k[SDL_SCANCODE_SPACE] ? vit : 0) - (k[SDL_SCANCODE_C] ? vit : 0));
		float rot = 1.6f * dt;
		cam.yaw += (k[SDL_SCANCODE_LEFT] ? rot : 0) - (k[SDL_SCANCODE_RIGHT] ? rot : 0);
		cam.pitch += (k[SDL_SCANCODE_UP] ? rot : 0) - (k[SDL_SCANCODE_DOWN] ? rot : 0);
		if(cam.pitch > 1.5f) cam.pitch = 1.5f; if(cam.pitch < -1.5f) cam.pitch = -1.5f;

		if(monde){
			static float attente = 0; attente += dt;
			if(attente > 0.5f){ attente = 0; Actualiser(cam.pos.x, cam.pos.y); }
		}
		auto t0 = horloge::now();
		v.Rendre(cam);
		cumul += std::chrono::duration<double, std::milli>(horloge::now() - t0).count(); images++;
		SDL_UpdateTexture(tex, nil, v.img.rgb, 400 * 3);
		SDL_RenderClear(ren); SDL_RenderCopy(ren, tex, nil, nil); SDL_RenderPresent(ren);
		if(images == 30){
			char titre[160];
			snprintf(titre, sizeof titre, "bully-re : %s (%zu) — %s — rendu %.1f ms, %d triangles — %.1f %.1f %.1f", ipb.c_str(), v.morceaux.size(), marche ? "marche" : "vol", cumul / images, v.dessines, cam.pos.x, cam.pos.y, cam.pos.z);
			SDL_SetWindowTitle(win, titre); cumul = 0; images = 0;
		}
	}
	SDL_DestroyTexture(tex); SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
	return 0;
#endif
}
