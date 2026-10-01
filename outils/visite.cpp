// Visite temps réel d'une scène du jeu, à la première personne.
//   BULLY_DATA=<racine> build/outils/visite <fichier.ipb> [--pos x y z lacet tangage] [--marche]
//                                          [--image sortie.ppm] [--banc n] [--promenade n]
//                                          [--monde] [--rayon m] [--survol n] [--pietons n] [--bras]
//                                          [--fige] [--attente] [--temps s] [--anim groupe:n] [--sans-objets]
//                                          [--vue x y z lacet tangage]
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
//
// --pietons n pose n piétons de la section « peds » du jeu en cercle, 8 m
// devant la caméra de départ, les pieds sur le sol, tournés vers le centre. Ils sont dans
// leur pose du fichier (les bras écartés), déformés par leur squelette
// (src/gamebryo/NifSkin), et ils jouent l'attente de leur catégorie tirée des
// .agr du jeu (IDLE_GSF_A pour une élève, IDLE_JOCK_A pour un sportif… ; le
// vrai jeu choisit par ses arbres d'action, c'est ici une approximation).
// Par défaut ils marchent : le pas de leur catégorie (SGEN_S, SGIRL_S…), en
// avançant de ce que parcourt la flèche ARROW de l'animation, sur le sol et
// contre les murs (le même corps que la caméra) ; bloqués, ils tournent.
// --attente les laisse sur place à jouer leur attente, --fige dans la pose
// du fichier, --bras dans une pose de démonstration (bras le long du corps).
// --temps s simule jusqu'à cet instant avant de rendre l'image. --anim
// groupe:n impose à tous la n-ième animation d'un groupe (F_Greas:2…), pour
// la regarder.
#include "commun.h"
#include "scene.h"
#include "monde.h"
#include "../src/anim/Agr.h"
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
	// Recalcule l'éclairage des seuls triangles [debut, fin[ (un objet animé).
	void PreparerTriangles(int32 debut, int32 fin){
		float lum[3] = {0.4f, -0.6f, 0.7f}; float ln = sqrtf(lum[0]*lum[0]+lum[1]*lum[1]+lum[2]*lum[2]); for(float &q : lum) q /= ln;
		for(int32 i = debut; i < fin; i += 3){
			const CVector &A = s->pts[s->tri[i]], &B = s->pts[s->tri[i+1]], &C = s->pts[s->tri[i+2]];
			float ux = B.x-A.x, uy = B.y-A.y, uz = B.z-A.z, wx = C.x-A.x, wy = C.y-A.y, wz = C.z-A.z;
			float nx = uy*wz-uz*wy, ny = uz*wx-ux*wz, nz = ux*wy-uy*wx, nn = sqrtf(nx*nx+ny*ny+nz*nz); if(nn < 1e-9f) nn = 1;
			ombre[i/3] = 0.45f + 0.55f * fabsf((nx*lum[0]+ny*lum[1]+nz*lum[2])/nn);
		}
	}
	// Pose les objets animés du morceau à l'instant t (s).
	void AnimerObjets(float t){
		for(outil::Anime *o : s->objets){
			s->Reposer(*o, *o->anim, t + o->decalage);
			if(o->bloc >= 0){ const outil::Scene::Bloc &b = s->blocs[o->bloc]; PreparerTriangles(b.triDebut, b.triFin); }
		}
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
	std::string ipb = argv[1], image; int banc = 0, promenade = 0, survol = 0, pietons = 0; bool bras = false, fige = false, attente = false, objets = true, vue = false; float vueV[5] = {0}; float temps = 0; std::string imposee; bool pos = false, marche = false, monde = Exterieur(ipb); float px = 0, py = 0, pz = 0, lacet = 0, tangage = 0, rayon = 60;
	for(int i = 2; i < argc; i++){
		if(strcmp(argv[i], "--image") == 0 && i + 1 < argc) image = argv[++i];
		else if(strcmp(argv[i], "--banc") == 0 && i + 1 < argc) banc = atoi(argv[++i]);
		else if(strcmp(argv[i], "--promenade") == 0 && i + 1 < argc){ promenade = atoi(argv[++i]); marche = true; }
		else if(strcmp(argv[i], "--marche") == 0) marche = true;
		else if(strcmp(argv[i], "--survol") == 0 && i + 1 < argc) survol = atoi(argv[++i]);
		else if(strcmp(argv[i], "--pietons") == 0 && i + 1 < argc) pietons = atoi(argv[++i]);
		else if(strcmp(argv[i], "--bras") == 0) bras = true;
		else if(strcmp(argv[i], "--fige") == 0) fige = true;
		else if(strcmp(argv[i], "--sans-objets") == 0) objets = false;
		else if(strcmp(argv[i], "--attente") == 0) attente = true;
		else if(strcmp(argv[i], "--anim") == 0 && i + 1 < argc) imposee = argv[++i];
		else if(strcmp(argv[i], "--temps") == 0 && i + 1 < argc) temps = (float)atof(argv[++i]);
		else if(strcmp(argv[i], "--monde") == 0) monde = true;
		else if(strcmp(argv[i], "--rayon") == 0 && i + 1 < argc) rayon = (float)atof(argv[++i]);
		else if(strcmp(argv[i], "--vue") == 0 && i + 5 < argc){ vue = true; for(int j = 0; j < 5; j++) vueV[j] = (float)atof(argv[++i]); }
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
		Morceau *m = new Morceau; m->nom = nom; m->s = new outil::Scene; m->s->animerObjets = objets;
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
		sol.Indexer();
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
	// Les piétons : un morceau à part, sans collisions, hors de la carte (il
	// n'est jamais libéré par l'enchaînement des scènes).
	// L'attente de chaque catégorie de piéton (le groupe .agr, sa première
	// animation décodée), chargée une fois.
	std::map<std::string, std::vector<AgrAnim>> groupes;
	auto Attente = [&](const CPedIdeEntry &e) -> const AgrAnim* {
		if(!imposee.empty()){
			size_t c = imposee.find(':'); std::string g = imposee.substr(0, c); int n = c == std::string::npos ? 0 : atoi(imposee.c_str() + c + 1);
			if(!groupes.count(g)){ std::vector<AgrAnim> v; uint32 nb; uint8 *b = outil::LireMonde(a, g + ".agr", &nb); if(b){ AgrLireGroupe(b, nb, v); free(b); } groupes[g] = v; }
			return n >= 0 && n < (int)groupes[g].size() && groupes[g][n].decodee ? &groupes[g][n] : nil;
		}
		std::string t = e.type, g;
		if(t == "JOCK") g = "IDLE_JOCK_A"; else if(t == "GREASER") g = "IDLE_GREAS_A"; else if(t == "NERD") g = "IDLE_NERD_A";
		else if(t == "PREPPY") g = "IDLE_PREP_A"; else if(t == "BULLY") g = "IDLE_BULLY_A"; else if(t == "DROPOUT") g = "IDLE_DOUT_A";
		else if(t == "PREFECT" || t == "COP") g = "IDLE_AUTH_A";
		else if(t == "TOWNPERSON") g = e.female ? "IDLE_CIVF_A" : "IDLE_CIVM_A";
		else g = e.female ? "IDLE_GSF_A" : "IDLE_GSM_A";
		if(!groupes.count(g)){
			std::vector<AgrAnim> v; uint32 nb; uint8 *b = outil::LireMonde(a, g + ".agr", &nb);
			if(b){ AgrLireGroupe(b, nb, v); free(b); }
			groupes[g] = v;
		}
		for(const AgrAnim &an : groupes[g]) if(an.decodee) return &an;
		return nil;
	};
	// Le pas de marche de chaque catégorie : dans son groupe S*_S, l'animation
	// dont la flèche avance à une vitesse de marche (0,4 à 2 m/s), la plus
	// proche de 1 m/s. SGEN_S n°2 : 0,94 m en 1 s.
	auto Imposee = [&](void) -> const AgrAnim* {
		size_t c = imposee.find(':'); std::string g = imposee.substr(0, c); int n = c == std::string::npos ? 0 : atoi(imposee.c_str() + c + 1);
		if(!groupes.count(g)){ std::vector<AgrAnim> v; uint32 nb; uint8 *b = outil::LireMonde(a, g + ".agr", &nb); if(b){ AgrLireGroupe(b, nb, v); free(b); } groupes[g] = v; }
		return n >= 0 && n < (int)groupes[g].size() && groupes[g][n].decodee ? &groupes[g][n] : nil;
	};
	// Le pas de marche d'un piéton : dans ses groupes d'animations de l'IDE
	// qui commencent par F_ (F_Girls, F_Jocks, F_Greas…, les déplacements de
	// chaque clan), sinon F_Adult puis F_Jocks, le cycle qui boucle (la pose de
	// fin est celle du début), avance droit devant, vers +y dans le repère des
	// animations, et va le plus près de 1,3 m/s, l'allure d'une marche
	// (F_Jocks n°2 : 1,48 m en 1,067 s). Les groupes S*_S, essayés d'abord,
	// reculent vers −y : des pas en arrière et des esquives.
	auto PasDeMarche = [&](const CPedIdeEntry &e) -> const AgrAnim* {
		if(!imposee.empty()){ const AgrAnim *x = Imposee(); if(x && hypotf(AgrDeplacement(*x).x, AgrDeplacement(*x).y) > 0.1f) return x; return nil; }
		for(std::string g : { std::string(e.animGroup[0]), std::string(e.animGroup[1]), std::string(e.animGroup[2]), std::string(e.animGroup[3]),
		                      std::string(e.female ? "F_Girls" : "F_Adult"), std::string("F_Jocks") }){
			if(g.compare(0, 2, "F_") != 0) continue;
			if(!groupes.count(g)){
				std::vector<AgrAnim> v; uint32 nb; uint8 *b = outil::LireMonde(a, g + ".agr", &nb);
				if(b){ AgrLireGroupe(b, nb, v); free(b); }
				groupes[g] = v;
			}
			const AgrAnim *meilleur = nil; float ecart = 1e9f;
			for(const AgrAnim &an : groupes[g]){
				if(!an.decodee) continue;
				CVector d = AgrDeplacement(an); float dist = hypotf(d.x, d.y), v = dist / an.duree;
				if(dist < 0.3f || fabsf(d.x) > 0.2f * dist || d.y < 0 || v < 0.8f || v > 1.8f) continue;
				float pire = 0;
				for(int o = 2; o < AGR_OS - 1; o++){
					float q0[4], q1[4];
					if(!AgrRotation(an, o, 0, q0) || !AgrRotation(an, o, an.duree * 0.9999f, q1)) continue;
					pire = fmaxf(pire, 1 - fabsf(q0[0]*q1[0] + q0[1]*q1[1] + q0[2]*q1[2] + q0[3]*q1[3]));
				}
				if(pire > 0.01f) continue;                                    // ne boucle pas
				if(fabsf(v - 1.3f) < ecart){ ecart = fabsf(v - 1.3f); meilleur = &an; }
			}
			if(meilleur) return meilleur;
		}
		return nil;
	};
	struct PietonAnime {
		outil::Anime *an; const AgrAnim *anim; float decalage; std::string groupe;
		bool marche = false; CMarcheur corps; CVector depart; float cap = 0, vitesse = 0, horloge = 0, bloque = 0; uint32 hasard = 1;
		// Un piéton qui a une marche et une attente alterne les deux : il marche
		// 5 à 10 s, s'arrête 2 à 4 s, repart. Chaque changement est un fondu de
		// FONDU secondes, os par os, et la vitesse au sol suit le fondu.
		const AgrAnim *pas = nil, *att = nil;
		bool enAttente = false; float tEtat = 0, dureeEtat = 0;
		const AgrAnim *avant = nil; float horlogeAvant = 0, fondu = 0;
		float Hasard(float a, float b){ hasard = hasard * 1103515245u + 12345u; return a + (b - a) * ((hasard >> 8) & 0xffff) / 65535.0f; }
	};
	const float FONDU = 0.3f;
	std::vector<PietonAnime> animes;
	Morceau *mPietons = nil;
	if(pietons > 0){
		std::vector<const CPedIdeEntry*> liste;
		for(const CPedIdeEntry &e : a.pietons) if(e.id > 1) liste.push_back(&e);   // 0 le joueur, 1 le piéton par défaut
		Morceau *m = new Morceau; m->nom = "(piétons)"; m->s = new outil::Scene; m->s->sansAidesNonTexturees = true;
		float cx = cam.pos.x + 8 * cosf(cam.yaw), cy = cam.pos.y + 8 * sinf(cam.yaw);
		int poses = 0;
		for(int k = 0; k < pietons && !liste.empty(); k++){
			const CPedIdeEntry &e = *liste[(size_t)k * liste.size() / pietons % liste.size()];
			float ang = k * 2 * PI / pietons, r = k % 2 ? 4.0f : 3.0f;
			float x = cx + r * cosf(ang), y = cy + r * sinf(ang), z;
			if(!sol.Sol(x, y, cam.pos.z + 2.0f, 50.0f, &z)) z = cam.pos.z - 1.6f;
			// Face au centre. Le modèle regarde vers −y dans son repère, et c'est
			// le conjugué du quaternion de placement qui tourne les sommets
			// (docs/ipl.md) : d'où le signe moins.
			float lacetP = -(atan2f(cy - y, cx - x) + PI / 2);
			float q[4] = { 0, 0, sinf(lacetP / 2), cosf(lacetP / 2) };
			outil::Anime *an = new outil::Anime;
			if(m->s->AjouterModele(a, e.model, NifFromPlacement(CVector(x, y, z), CVector(1, 1, 1), q), bras, an)){
				poses++;
				const AgrAnim *pas = (fige || bras || attente) ? nil : PasDeMarche(e);
				const AgrAnim *att = (fige || bras) ? nil : Attente(e);
				printf("  piéton %-22s %-10s (%.1f, %.1f, %.2f)%s\n", e.model, e.type, x, y, z, pas && att ? "  marche et attente" : pas ? "  marche" : att ? "  attente" : "");
				if(pas || att){
					PietonAnime p; p.an = an; p.anim = pas ? pas : att; p.decalage = k * 0.77f;
					p.pas = pas; p.att = att; p.hasard = 12345u + k * 7919u; p.dureeEtat = p.Hasard(5, 10);
					if(pas){
						CVector d = AgrDeplacement(*pas);
						p.marche = true; p.vitesse = hypotf(d.x, d.y) / pas->duree;
						p.cap = atan2f(cy - y, cx - x) + PI;                 // dos au centre : ils s'éloignent
						p.corps.pos = CVector(x, y, z); p.depart = p.corps.pos;
					}
					animes.push_back(p);
				}else delete an;
			}else{ delete an; printf("  piéton %-22s : modèle introuvable dans World.img\n", e.model); }
		}
		if(poses > 0){ m->Preparer(); v.morceaux.push_back(m); mPietons = m; }
		else delete m;
		printf("  %d piéton(s) posé(s)\n", poses);
	}

	// Pose tous les piétons animés à l'instant t.
	// Avance le monde des piétons jusqu'à l'instant t (pas de 1/60 s au plus :
	// la marche suit le sol et les murs pas à pas), puis les pose.
	float tPietons = 0;
	auto Animer = [&](float t){
		if(!mPietons || animes.empty()) return;
		while(tPietons < t){
			float dt = fminf(1.0f / 60, t - tPietons); tPietons += dt;
			for(PietonAnime &p : animes){
				p.horloge += dt;
				if(p.fondu > 0){ p.horlogeAvant += dt; p.fondu -= dt; }
				if(p.pas && p.att){
					p.tEtat += dt;
					if(p.tEtat > p.dureeEtat){
						p.avant = p.anim; p.horlogeAvant = p.horloge + p.decalage;
						p.enAttente = !p.enAttente;
						p.anim = p.enAttente ? p.att : p.pas;
						p.horloge = 0; p.decalage = 0; p.fondu = FONDU; p.tEtat = 0;
						p.dureeEtat = p.enAttente ? p.Hasard(2, 4) : p.Hasard(5, 10);
					}
				}
				if(!p.marche) continue;
				// La part de marche : 1 en marchant, 0 à l'arrêt, suivant le fondu entre les deux.
				float f = p.fondu > 0 ? p.fondu / FONDU : 0;
				float part = p.enAttente ? f : 1 - f;
				if(part <= 0){ p.bloque = 0; continue; }
				CVector avant = p.corps.pos;
				p.corps.Avancer(sol, cosf(p.cap) * p.vitesse * part * dt, sinf(p.cap) * p.vitesse * part * dt, dt);
				float fait = hypotf(p.corps.pos.x - avant.x, p.corps.pos.y - avant.y);
				p.bloque = fait < 0.3f * p.vitesse * part * dt ? p.bloque + dt : 0;
				if(p.bloque > 0.25f){
					// Contre un mur : un quart à un demi-tour, d'un côté au hasard.
					p.hasard = p.hasard * 1103515245u + 12345u;
					float quart = (90 + (p.hasard >> 16) % 90) * PI / 180;
					p.cap += (p.hasard >> 8 & 1) ? quart : -quart; p.bloque = 0;
				}
			}
		}
		for(PietonAnime &p : animes){
			if(p.marche){
				// Le corps animé regarde vers +y de son repère : +y suit le cap.
				float lacetP = -(p.cap - PI / 2);
				float q[4] = { 0, 0, sinf(lacetP / 2), cosf(lacetP / 2) };
				p.an->place = NifFromPlacement(p.corps.pos, CVector(1, 1, 1), q);
			}
			if(p.fondu > 0 && p.avant) mPietons->s->Reposer(*p.an, *p.avant, p.horlogeAvant, p.anim, p.horloge + p.decalage, 1 - p.fondu / FONDU);
			else mPietons->s->Reposer(*p.an, *p.anim, p.horloge + p.decalage);
		}
		mPietons->Preparer();
	};
	if(!animes.empty()){
		auto t0 = std::chrono::steady_clock::now();
		Animer(temps);
		printf("  %zu piéton(s) animé(s) : %.1f s simulées, pose et peau comprises, en %.1f ms\n", animes.size(), temps, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
		for(const PietonAnime &p : animes) if(p.marche && temps > 0)
			printf("    marche %.2f m/s (%.3f s par cycle) : %.1f m du départ, %s\n", p.vitesse, p.anim->duree,
			       hypotf(p.corps.pos.x - p.an->place.t.x, p.corps.pos.y - p.an->place.t.y) >= 0 ? hypotf(p.corps.pos.x - p.depart.x, p.corps.pos.y - p.depart.y) : 0,
			       p.corps.auSol ? "au sol" : "en l'air"),
			printf("      %s depuis %.1f s (prochain changement à %.1f s)%s, en (%.2f, %.2f, %.2f) cap %.0f°\n", p.enAttente ? "à l'arrêt" : "en marche", p.tEtat, p.dureeEtat, p.fondu > 0 ? ", en fondu" : "",
			       p.corps.pos.x, p.corps.pos.y, p.corps.pos.z, p.cap * 180 / PI);
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
	{
		auto t0 = horloge::now(); size_t n = 0;
		for(Morceau *m : v.morceaux){ m->AnimerObjets(temps); n += m->s->objets.size(); }
		if(n) printf("  %zu objet(s) animé(s) posé(s) à %.2f s en %.2f ms\n", n, temps, std::chrono::duration<double, std::milli>(horloge::now() - t0).count());
	}
	// --vue x y z lacet tangage : la caméra du rendu seulement, posée après la
	// simulation (les piétons, eux, sont posés autour de la caméra de départ).
	if(vue){ cam.pos = CVector(vueV[0], vueV[1], vueV[2]); cam.yaw = vueV[3] * PI / 180; cam.pitch = vueV[4] * PI / 180; }
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
		{ static float horlogeAnim = temps; horlogeAnim += dt > 0.1f ? 0.1f : dt; Animer(horlogeAnim); for(Morceau *m : v.morceaux) m->AnimerObjets(horlogeAnim); }
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
