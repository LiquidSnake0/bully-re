// Visite temps réel d'une scène du jeu, à la première personne.
//   BULLY_DATA=<racine> build/outils/visite <fichier.ipb> [--pos x y z lacet tangage] [--marche]
//                                          [--image sortie.ppm] [--banc n] [--promenade n]
//                                          [--monde] [--rayon m] [--survol n] [--pietons n] [--bras]
//                                          [--fige] [--attente] [--temps s] [--anim groupe:n] [--sans-objets]
//                                          [--vue x y z lacet tangage] [--patrouilles n] [--poi n] [--galerie groupe[:réf][@a-b]] [--paire groupe:a,b[:réf]] [--modele m]
//                                          [--population auto|jour|classe|nuit|couvrefeu] [--heure HH:MM]
//                                          [--saison automne|hiver|ete|printemps] [--meteo n] [--sans-ciel]
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
#include "../src/core/TriggerFile.h"
#include "../src/core/Horloge.h"
#include "../src/render/TimeCycle.h"
#include "../src/render/Camera.h"
#include <algorithm>
#include <set>
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

	// Le cycle jour-nuit (Config/timecyc*.dat) : si `ciel` est donné, le fond
	// devient le dégradé du ciel, la scène prend la teinte de l'heure et se
	// fond dans le brouillard (docs/timecycle.md). Sinon, l'ancien fond fixe.
	const CTimeCycleHeure *ciel = nil; const CTimeCycleHeure *midi = nil;
	// `avant` : peint le ciel dans l'image vide, avant la scène (les surfaces
	// transparentes se posent ensuite dessus sans écrire la profondeur) ; sinon,
	// après la scène, teinte et brouillard sur les pixels qui ont une profondeur.
	void Ambiance(const Camera &cam, bool avant){
		const CTimeCycleHeure &e = *ciel;
		// Teinte : 60 % le soleil, 40 % la lumière ambiante du monde, rapportés à
		// ceux de midi (la scène telle qu'on la dessine) ; une approximation, pas
		// le calcul d'éclairage du jeu.
		float t[3];
		for(int c = 0; c < 3; c++){
			float v = 0.6f * e.soleil[c] / fmaxf(midi->soleil[c], 1) + 0.4f * e.ambMonde[c] / fmaxf(midi->ambMonde[c], 1);
			t[c] = fminf(fmaxf(v, 0.12f), 1.3f);
		}
		float foc = (cam.h * 0.5f) / tanf(cam.fovY * 0.5f), s60 = sinf(PI / 3);
		for(int32 y = 0; y < img.h; y++){
			// Le ciel selon l'élévation du rayon : le bas à l'horizon, le haut à 60°.
			float elev = cam.pitch + atanf((img.h * 0.5f - y) / foc);
			float u = fminf(fmaxf(sinf(elev) / s60, 0), 1);
			float ciel3[3] = { e.cielBas[0] + u * (e.cielHaut[0] - e.cielBas[0]), e.cielBas[1] + u * (e.cielHaut[1] - e.cielBas[1]), e.cielBas[2] + u * (e.cielHaut[2] - e.cielBas[2]) };
			for(int32 x = 0; x < img.w; x++){
				int32 i = y * img.w + x; uint8 *p = img.rgb + i * 3;
				float d = img.depth[i];
				if(d >= 1e29f){ if(avant) for(int c = 0; c < 3; c++) p[c] = (uint8)ciel3[c]; continue; }
				if(avant) continue;
				// Profondeur : −1/w, w la distance le long de l'axe de vue.
				float dist = d < 0 ? -1.0f / d : 0;
				float f = fminf(fmaxf((dist - e.fogSt) / fmaxf(e.farClp - e.fogSt, 1), 0), 1) * 0.8f;
				for(int c = 0; c < 3; c++){
					float v = p[c] * t[c];
					v = v + f * (e.cielBas[c] - v);
					p[c] = (uint8)fminf(fmaxf(v, 0), 255);
				}
			}
		}
	}
	// Les halos des lumières 2dfx, ajoutés par-dessus la scène : un disque doux
	// de la couleur de la lumière, de rayon `taille` mètres, plus fort quand le
	// jour baisse. Une lumière derrière un mur (la profondeur au centre est plus
	// proche de plus de 40 cm) n'est pas dessinée.
	int32 halos = 0;
	void Lumieres(const Camera &cam){
		halos = 0;
		if(!ciel || !midi) return;
		float jour = (ciel->soleil[0] + ciel->soleil[1] + ciel->soleil[2]) / fmaxf(midi->soleil[0] + midi->soleil[1] + midi->soleil[2], 1);
		float intensite = 0.1f + 0.9f * fminf(fmaxf((1 - jour) / 0.7f, 0), 1);
		float foc = (cam.h * 0.5f) / tanf(cam.fovY * 0.5f);
		for(Morceau *m : morceaux)
		for(const outil::Scene::Lumiere &l : m->s->lumieres){
			CVector v = cam.ToView(l.pos);
			if(v.z < cam.near_ || v.z > l.distance * 3) continue;
			RasterPVertex p = cam.Project(v, 0, 0);
			int32 cx = (int32)p.x, cy = (int32)p.y;
			if(cx < 0 || cy < 0 || cx >= img.w || cy >= img.h) continue;
			float d = img.depth[cy * img.w + cx];
			if(d < 0 && -1.0f / d < v.z - 0.4f) continue;
			float R = l.taille * foc / v.z; if(R < 1.5f) R = 1.5f; if(R > 80) R = 80;
			float k = intensite * l.alpha * fminf(1.0f, 1.5f - v.z / (l.distance * 3));
			for(int32 y = (int32)(p.y - R); y <= (int32)(p.y + R); y++){
				if(y < 0 || y >= img.h) continue;
				for(int32 x = (int32)(p.x - R); x <= (int32)(p.x + R); x++){
					if(x < 0 || x >= img.w) continue;
					float r = sqrtf((x - p.x) * (x - p.x) + (y - p.y) * (y - p.y)) / R;
					if(r >= 1) continue;
					float a = k * (1 - r) * (1 - r);
					uint8 *q = img.rgb + (y * img.w + x) * 3;
					for(int c = 0; c < 3; c++) q[c] = (uint8)fminf(q[c] + a * l.rgb[c], 255);
				}
			}
			halos++;
		}
	}
	// Les modèles à horaire (tobj) : visibles de l'heure d'allumage à celle
	// d'extinction (une plage qui finit avant de commencer passe minuit).
	void Horaires(int32 heure){
		for(Morceau *m : morceaux){
			outil::Scene *s = m->s;
			for(size_t b = 0; b < s->horaires.size() && b < s->blocs.size(); b++){
				auto h = s->horaires[b];
				if(h.first < 0) continue;
				bool on = h.first <= h.second ? (heure >= h.first && heure < h.second) : (heure >= h.first || heure < h.second);
				s->blocs[b].cache = !on;
			}
		}
	}
	void Rendre(const Camera &cam){
		RasterClear(img, 128, 150, 170);
		if(ciel && midi) Ambiance(cam, true);
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
				if(bl.cache) continue;
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
		if(ciel && midi) Ambiance(cam, false);
		Lumieres(cam);
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
	std::string ipb = argv[1], image; int banc = 0, promenade = 0, survol = 0, pietons = 0, patrouilles = 0, nPoi = 0, nGalerie = 0, moment = -1; CHorloge heureJeu; std::string saison = "automne"; int meteo = 0; bool sansCiel = false; bool bras = false, fige = false, attente = false, objets = true, vue = false; float vueV[5] = {0}; float temps = 0; std::string imposee, galerie, modeleGalerie, paire; bool pos = false, marche = false, monde = Exterieur(ipb); float px = 0, py = 0, pz = 0, lacet = 0, tangage = 0, rayon = 60;
	for(int i = 2; i < argc; i++){
		if(strcmp(argv[i], "--image") == 0 && i + 1 < argc) image = argv[++i];
		else if(strcmp(argv[i], "--banc") == 0 && i + 1 < argc) banc = atoi(argv[++i]);
		else if(strcmp(argv[i], "--promenade") == 0 && i + 1 < argc){ promenade = atoi(argv[++i]); marche = true; }
		else if(strcmp(argv[i], "--marche") == 0) marche = true;
		else if(strcmp(argv[i], "--survol") == 0 && i + 1 < argc) survol = atoi(argv[++i]);
		else if(strcmp(argv[i], "--pietons") == 0 && i + 1 < argc) pietons = atoi(argv[++i]);
		else if(strcmp(argv[i], "--bras") == 0) bras = true;
		else if(strcmp(argv[i], "--fige") == 0) fige = true;
		else if(strcmp(argv[i], "--patrouilles") == 0 && i + 1 < argc) patrouilles = atoi(argv[++i]);
		else if(strcmp(argv[i], "--poi") == 0 && i + 1 < argc) nPoi = atoi(argv[++i]);
		else if(strcmp(argv[i], "--galerie") == 0 && i + 1 < argc){ galerie = argv[++i]; nGalerie = 1; }
		else if(strcmp(argv[i], "--modele") == 0 && i + 1 < argc) modeleGalerie = argv[++i];
		else if(strcmp(argv[i], "--paire") == 0 && i + 1 < argc){ paire = argv[++i]; nGalerie = 1; }
		else if(strcmp(argv[i], "--saison") == 0 && i + 1 < argc) saison = argv[++i];
		else if(strcmp(argv[i], "--meteo") == 0 && i + 1 < argc) meteo = atoi(argv[++i]);
		else if(strcmp(argv[i], "--sans-ciel") == 0) sansCiel = true;
		else if(strcmp(argv[i], "--heure") == 0 && i + 1 < argc){ int hh = 8, mm = 0; sscanf(argv[++i], "%d:%d", &hh, &mm); heureJeu.Regler(hh, mm); }
		else if(strcmp(argv[i], "--population") == 0 && i + 1 < argc){
			std::string m = argv[++i];
			moment = m == "jour" ? MOMENT_JOUR : m == "classe" ? MOMENT_CLASSE : m == "nuit" ? MOMENT_NUIT : m == "couvrefeu" ? MOMENT_COUVREFEU : m == "auto" ? MOMENT_NUM : -1;
			if(moment < 0){ fprintf(stderr, "--population auto|jour|classe|nuit|couvrefeu\n"); return 1; }
		}
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
	// Le cycle jour-nuit de la saison (automne par défaut : chapitre 1), météo 0
	// (beau temps), à l'heure du jeu. --sans-ciel garde l'ancien fond fixe.
	static CTimeCycle cycle; static CTimeCycleHeure etatCiel;
	if(!sansCiel){
		const char *f = saison == "hiver" ? "Config\\timecycW.dat" : saison == "ete" ? "Config\\timecycS.dat" : saison == "printemps" ? "Config\\sbtimecycS.dat" : "Config\\timecycF.dat";
		static std::vector<uint8> b(1 << 20);
		int32 n = CFileMgr::LoadFile(f, b.data(), (int32)b.size(), "rb");
		if(n > 0 && cycle.Load((const char*)b.data(), (size_t)n)){
			if(meteo < 0 || meteo >= (int32)cycle.heures.size()) meteo = 0;
			v.midi = &cycle.heures[meteo][12]; v.ciel = &etatCiel;
			printf("  cycle jour-nuit : %s, météo « %s »\n", f, cycle.meteos[meteo].c_str());
		}else printf("  cycle jour-nuit : %s illisible, fond fixe\n", f);
	}
	// Les périodes de la journée (Config/timeCycl.dat) : elles fixent le moment de population.
	{
		std::vector<uint8> b(1 << 16);
		int32 n = CFileMgr::LoadFile("Config\\timeCycl.dat", b.data(), (int32)b.size(), "rb");
		if(!(n > 0 && CHorloge::ChargerPeriodes((const char*)b.data(), (size_t)n))) printf("  timeCycl.dat illisible : périodes par défaut\n");
	}
	auto MajCiel = [&](void){ if(v.ciel) etatCiel = cycle.Etat(meteo, heureJeu.minutes / 60.0f); v.Horaires(heureJeu.Heure()); };
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
		// Un piéton de patrouille suit un trajet de DAT/Trigger.img : il marche
		// vers le point `cible`, s'y arrête le temps écrit (attente du point et
		// de ses actions), tourné vers l'orientation de l'action, puis repart.
		const CTriggerPath *trajet = nil; int32 cible = 0; float attenteRestante = 0, capVise = 0;
		const CTriggerPathPoint *arret = nil; float attenteTotale = 0;   // le point où il attend, et l'attente entière
		// Un piéton de population erre sans sortir de sa zone (Population.dat).
		const CTriggerZone *zone = nil;
		int32 momentPop = -1;                 // -1 : toujours là ; sinon le moment où il est de sortie
		// Un piéton de point d'intérêt reste à son point, à l'attente, tant
		// qu'une des périodes du point est en cours.
		const CTriggerPoiPoint *poi = nil;
		float Hasard(float a, float b){ hasard = hasard * 1103515245u + 12345u; return a + (b - a) * ((hasard >> 8) & 0xffff) / 65535.0f; }
	};
	const float FONDU = 0.3f;
	std::vector<PietonAnime> animes;
	Morceau *mPietons = nil;
	// Les trajets de DAT/Trigger.img, lus une fois si on en veut.
	// --population auto : le moment de la journée vient de l'horloge du jeu.
	// La population des quatre moments est posée d'avance ; seuls les piétons du
	// moment courant sont dessinés et animés : à chaque changement, la relève.
	bool momentsTous = moment == MOMENT_NUM;
	if(momentsTous){ moment = heureJeu.Moment(); printf("  il est %02d:%02d : moment %s\n", heureJeu.Heure(), heureJeu.Minute(), kMoment[moment]); }
	std::vector<CTriggerPath> tousTrajets;
	std::vector<CTriggerPoi> tousPoi;
	// La période d'un point d'intérêt à cette heure. Les périodes de jour sont
	// celles de timeCycl.dat (FIRST_CLASS = EARLYCLASS, SECOND_CLASS = LATECLASS) ;
	// le couvre-feu se découpe en paliers de fatigue (F_StartCurfew_SlightlyTired…
	// TooTired dans STimeCycle.lur), dont les heures ne sont écrites nulle part
	// dans les données : 23 h, minuit, 1 h, 2 h (l'évanouissement de 2 h), supposées.
	auto PeriodePoi = [&](int32 h) -> int32 {
		std::string p = CHorloge::PeriodeA(h);
		if(p == "MORNING") return POI_MORNING;
		if(p == "FIRST_CLASS") return POI_EARLYCLASS;
		if(p == "LUNCH_TIME") return POI_LUNCH;
		if(p == "SECOND_CLASS") return POI_LATECLASS;
		if(p == "AFTERNOON") return POI_AFTERNOON;
		if(p == "EVENING") return POI_EVENING;
		return h == 23 ? POI_SLIGHTLYTIRED : h == 0 ? POI_TIRED : h == 1 ? POI_MORETIRED : POI_TOOTIRED;
	};
	auto Present = [&](const PietonAnime &p) -> bool {
		if(p.momentPop >= 0 && p.momentPop != heureJeu.Moment()) return false;
		if(p.poi && !p.poi->periodes[PeriodePoi(heureJeu.Heure())]) return false;
		return true;
	};
	CTriggerFile population;
	if(moment >= 0){
		int32 im = CdStream::AddImage("DAT\\Trigger.img");
		uint32 nb; uint8 *b = im >= 0 ? outil::LireEntree(im, "DAT\\Trigger.img", "Population.dat", &nb) : nil;
		if(!b || !population.Load(b, nb)) fprintf(stderr, "Population.dat illisible\n");
		free(b);
	}
	if(patrouilles > 0){
		int32 im = CdStream::AddImage("DAT\\Trigger.img");
		const CdImage &img = CdStream::ms_images[im >= 0 ? im : 0];
		for(int32 e = 0; im >= 0 && e < img.m_numEntries; e++){
			uint32 nb; uint8 *b = outil::LireEntree(im, "DAT\\Trigger.img", img.m_entries[e].name, &nb);
			CTriggerFile f; if(b && f.Load(b, nb)) for(CTriggerPath &t : f.paths) if(t.points.size() >= 2) tousTrajets.push_back(std::move(t));
			free(b);
		}
		printf("  %zu trajets lus dans DAT/Trigger.img\n", tousTrajets.size());
	}
	if(nPoi > 0){
		int32 im = CdStream::AddImage("DAT\\Trigger.img");
		const CdImage &img = CdStream::ms_images[im >= 0 ? im : 0];
		for(int32 e = 0; im >= 0 && e < img.m_numEntries; e++){
			uint32 nb; uint8 *b = outil::LireEntree(im, "DAT\\Trigger.img", img.m_entries[e].name, &nb);
			CTriggerFile f; if(b && f.Load(b, nb)) for(CTriggerPoi &q : f.pois) tousPoi.push_back(std::move(q));
			free(b);
		}
		size_t n = 0; for(const CTriggerPoi &q : tousPoi) n += q.points.size();
		printf("  %zu points d'intérêt (%zu points) lus dans DAT/Trigger.img, période %s\n", tousPoi.size(), n, kPoiPeriode[PeriodePoi(heureJeu.Heure())]);
	}
	if(pietons > 0 || patrouilles > 0 || moment >= 0 || nPoi > 0 || nGalerie > 0){
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
		// La galerie : toutes les animations d'un groupe, une par piéton, en rang
		// à 6 m devant la caméra, 1,2 m d'écart, face à elle, de gauche (n° 0) à
		// droite. Les .agr ne nomment pas leurs animations : c'est l'œil qui dit
		// laquelle est assise, adossée, etc. « --galerie groupe[:réf][@a-b] », la
		// référence du bassin debout valant 0,86 m par défaut.
		// Une paire : deux animations jouées ensemble (une prise : celui qui tient,
		// celui qui est tenu). Les deux piétons sont posés à la même origine, avec
		// la même orientation, à 5 m devant la caméra et tournés de côté ; ce sont
		// les décalages de bassin des animations qui les placent l'un par rapport à
		// l'autre. « --paire groupe:a,b[:réf] ».
		if(!paire.empty()){
			std::string g = paire.substr(0, paire.find(':')); int pa1 = 0, pa2 = 0; float ref = 0.86f;
			sscanf(paire.c_str() + g.size() + 1, "%d,%d:%f", &pa1, &pa2, &ref);
			if(!groupes.count(g)){ std::vector<AgrAnim> v; uint32 nb; uint8 *b = outil::LireMonde(a, g + ".agr", &nb); if(b){ AgrLireGroupe(b, nb, v); free(b); } groupes[g] = v; }
			const std::vector<AgrAnim> &ga = groupes[g];
			float x = cam.pos.x + 5 * cosf(cam.yaw), y = cam.pos.y + 5 * sinf(cam.yaw), z;
			if(!sol.Sol(x, y, cam.pos.z + 2.0f, 50.0f, &z)) z = cam.pos.z - 1.6f;
			float cap = cam.yaw + PI / 2, lacetP = -(cap - PI / 2);
			float q[4] = { 0, 0, sinf(lacetP / 2), cosf(lacetP / 2) };
			int qui = 0;
			for(int n : { pa1, pa2 }){
				if(n < 0 || n >= (int)ga.size() || !ga[n].decodee) continue;
				const CPedIdeEntry *choix = nil;
				for(const CPedIdeEntry *e : liste) if(strcasecmp(e->type, qui ? "NERD" : "GREASER") == 0 && e->unique >= 0 && !e->female){ choix = e; break; }
				outil::Anime *an = new outil::Anime;
				if(!choix || !m->s->AjouterModele(a, choix->model, NifFromPlacement(CVector(x, y, z), CVector(1, 1, 1), q), false, an)){ delete an; continue; }
				an->bassinRef = ref;
				PietonAnime pa; pa.an = an; pa.anim = &ga[n]; pa.att = &ga[n]; pa.enAttente = true; pa.cap = cap; pa.corps.pos = CVector(x, y, z);
				animes.push_back(pa); poses++; qui++;
				printf("  paire %s n° %d : %.2f s, %s\n", g.c_str(), n, ga[n].duree, choix->model);
			}
		}
		if(nGalerie > 0 && paire.empty()){
			// « groupe[:réf][@a-b] » : la plage a-b restreint aux animations a à b.
			size_t arob = galerie.find('@'); int de = 0, a2 = 1 << 30;
			if(arob != std::string::npos){ sscanf(galerie.c_str() + arob + 1, "%d-%d", &de, &a2); galerie = galerie.substr(0, arob); }
			size_t c = galerie.find(':'); std::string g = galerie.substr(0, c);
			float ref = c == std::string::npos ? 0.86f : (float)atof(galerie.c_str() + c + 1);
			if(!groupes.count(g)){ std::vector<AgrAnim> v; uint32 nb; uint8 *b = outil::LireMonde(a, g + ".agr", &nb); if(b){ AgrLireGroupe(b, nb, v); free(b); } groupes[g] = v; }
			const std::vector<AgrAnim> &ga = groupes[g];
			const CPedIdeEntry *choix = nil;
			for(const CPedIdeEntry *e : liste) if(modeleGalerie.empty() ? (strcasecmp(e->type, "STUDENT") == 0 && !e->female && e->unique >= 0) : strcasecmp(e->model, modeleGalerie.c_str()) == 0){ choix = e; break; }
			int fin = std::min(a2, (int)ga.size() - 1); float n = (float)(fin - de + 1);
			for(size_t i = (size_t)de; choix && (int)i <= fin; i++){
				if(!ga[i].decodee) continue;
				float lat = ((float)(i - de) - (n - 1) / 2.0f) * 1.2f;
				float x = cam.pos.x + 6 * cosf(cam.yaw) + lat * sinf(cam.yaw), y = cam.pos.y + 6 * sinf(cam.yaw) - lat * cosf(cam.yaw), z;
				if(!sol.Sol(x, y, cam.pos.z + 2.0f, 50.0f, &z)) z = cam.pos.z - 1.6f;
				float cap = cam.yaw + PI, lacetP = -(cap - PI / 2);
				float q[4] = { 0, 0, sinf(lacetP / 2), cosf(lacetP / 2) };
				outil::Anime *an = new outil::Anime;
				if(!m->s->AjouterModele(a, choix->model, NifFromPlacement(CVector(x, y, z), CVector(1, 1, 1), q), false, an)){ delete an; break; }
				an->bassinRef = ref;
				PietonAnime pa; pa.an = an; pa.anim = &ga[i]; pa.att = &ga[i]; pa.enAttente = true; pa.cap = cap; pa.corps.pos = CVector(x, y, z);
				animes.push_back(pa); poses++;
				printf("  galerie %s n° %zu : %.2f s, %s\n", g.c_str(), i, ga[i].duree, choix->model);
			}
		}
		// Les patrouilles : les trajets dont un point est à moins de `rayon` de la
		// caméra (et à moins de 6 m en hauteur), les plus proches d'abord, un
		// piéton chacun, posé sur le premier point.
		if(patrouilles > 0){
			std::vector<std::pair<float, const CTriggerPath*>> proches;
			for(const CTriggerPath &t : tousTrajets){
				// Les trajets d'ambiance seulement : pas ceux des missions (« 1_02B_… »,
				// « 3_S11_… »), ni ceux des tests (« GLOBALTESTPATH2 », « TestPath… »).
				std::string n = outil::Minuscules(t.nom.c_str());
				if(n.size() > 2 && isdigit((unsigned char)n[0]) && n[1] == '_') continue;
				if(n.find("test") != std::string::npos) continue;
				float d = 1e9f;
				for(const CTriggerPathPoint &q : t.points)
					if(fabsf(q.pos.z - (cam.pos.z - 1.6f)) < 6) d = fminf(d, hypotf(q.pos.x - cam.pos.x, q.pos.y - cam.pos.y));
				if(d < rayon) proches.push_back({d, &t});
			}
			std::sort(proches.begin(), proches.end(), [](const auto &x, const auto &y){ return x.first < y.first; });
			auto Contient = [](const std::string &n, const char *m){ std::string a = outil::Minuscules(n.c_str()); return a.find(m) != std::string::npos; };
			int k = 0;
			for(auto &pr : proches){
				if(k >= patrouilles) break;
				const CTriggerPath &t = *pr.second;
				// Un préfet pour une ronde de préfet, sinon un piéton de la liste.
				bool prefet = Contient(t.nom, "patrol") || Contient(t.nom, "prefect");
				const CPedIdeEntry *choix = nil;
				for(size_t j = 0; j < liste.size() && !choix; j++){
					const CPedIdeEntry &e = *liste[(j + k * 7) % liste.size()];
					if(!prefet || strcasecmp(e.type, "PREFECT") == 0) choix = &e;
				}
				if(!choix) continue;
				const CTriggerPathPoint &p0 = t.points[0], &p1 = t.points[1];
				float z = p0.pos.z; sol.Sol(p0.pos.x, p0.pos.y, p0.pos.z + 2.0f, 10.0f, &z);
				float cap = atan2f(p1.pos.y - p0.pos.y, p1.pos.x - p0.pos.x);
				float lacetP = -(cap - PI / 2);
				float q[4] = { 0, 0, sinf(lacetP / 2), cosf(lacetP / 2) };
				outil::Anime *an = new outil::Anime;
				if(!m->s->AjouterModele(a, choix->model, NifFromPlacement(CVector(p0.pos.x, p0.pos.y, z), CVector(1, 1, 1), q), false, an)){ delete an; continue; }
				const AgrAnim *pas = PasDeMarche(*choix), *att = Attente(*choix);
				if(!pas){ delete an; continue; }
				PietonAnime pa; pa.an = an; pa.anim = pas; pa.pas = pas; pa.att = att; pa.decalage = k * 0.53f; pa.hasard = 777u + k * 7919u;
				CVector d = AgrDeplacement(*pas);
				pa.marche = true; pa.vitesse = hypotf(d.x, d.y) / pas->duree;
				pa.cap = pa.capVise = cap; pa.corps.pos = CVector(p0.pos.x, p0.pos.y, z); pa.depart = pa.corps.pos;
				pa.trajet = &t; pa.cible = 1;
				animes.push_back(pa); poses++; k++;
				printf("  patrouille « %s » (%zu points, à %.0f m) : %s %s\n", t.nom.c_str(), t.points.size(), pr.first, choix->model, choix->type);
			}
		}
		// Les points d'intérêt : les points à moins de `rayon` de la caméra (pas
		// au-dessus d'elle, pas 50 m plus bas : une vue d'en haut les garde), actifs à l'heure du jeu, les plus proches
		// d'abord. Sur chacun, un piéton de la clique demandée (PEDTYPE, DEFAULT :
		// n'importe quel élève ou citadin) et du genre demandé, debout au point,
		// tourné selon son lacet, à l'attente.
		if(nPoi > 0){
			int32 per = PeriodePoi(heureJeu.Heure());
			std::vector<std::pair<float, std::pair<const CTriggerPoi*, const CTriggerPoiPoint*>>> proches;
			for(const CTriggerPoi &q : tousPoi) for(const CTriggerPoiPoint &pt : q.points){
				if(!pt.periodes[per] || pt.pos.z > cam.pos.z + 4 || pt.pos.z < cam.pos.z - 50) continue;
				float d = hypotf(pt.pos.x - cam.pos.x, pt.pos.y - cam.pos.y);
				if(d < rayon) proches.push_back({d, {&q, &pt}});
			}
			std::sort(proches.begin(), proches.end(), [](const auto &x, const auto &y){ return x.first < y.first; });
			printf("  %zu point(s) d'intérêt actif(s) à moins de %.0f m\n", proches.size(), rayon);
			int k = 0; std::set<std::string> dejaPoses;
			for(auto &pr : proches){
				if(k >= nPoi) break;
				const CTriggerPoi &q = *pr.second.first; const CTriggerPoiPoint &pt = *pr.second.second;
				// La clique : PEDTYPE, ou TYPE quand il nomme une catégorie (« trich_nerds » :
				// PEDTYPE DEFAULT, TYPE NERD).
				std::string clique = pt.clique;
				if(strcasecmp(clique.c_str(), "DEFAULT") == 0)
					for(const CPedIdeEntry *e : liste) if(strcasecmp(e->type, pt.type.c_str()) == 0){ clique = e->type; break; }
				// Sinon le nom du bloc, quand il nomme une clique (« trich_nerds », « trich_greasers ») :
				// une déduction, le jeu peut aussi bien la lire ailleurs.
				if(strcasecmp(clique.c_str(), "DEFAULT") == 0){
					std::string n = outil::Minuscules(q.nom.c_str());
					static const char *const mots[][2] = { {"nerd", "NERD"}, {"greaser", "GREASER"}, {"jock", "JOCK"}, {"prep", "PREPPY"},
					                                        {"bull", "BULLY"}, {"dropout", "DROPOUT"}, {"townie", "DROPOUT"} };
					for(auto &mt : mots) if(n.find(mt[0]) != std::string::npos){ clique = mt[1]; break; }
				}
				bool tous = strcasecmp(clique.c_str(), "DEFAULT") == 0;
				// L'animation selon le type du point. Les arbres d'actions (AI_POI.cat)
				// qui la choisissent ne sont pas décodés : chaque correspondance vient
				// des noms de blocs (Wall = « Smoking », Couple = « Kissing »…) et de la
				// galerie (--galerie), à l'œil. Toutes ces animations ont le bassin
				// debout à 0,86 m : il compte en absolu (Anime::bassinRef).
				std::string groupe; std::vector<int> choixAnims; bool couple = false;
				// Les noms viennent des .HXD et les usages des arbres d'actions (docs/hxd.md,
				// docs/cat.md : tools/cat.py --pistes) : Ambient/Sitting_Down/SitHigh,
				// Ambient/scripted/Wall_Smoke, Ambient/Spectator… Seules les boucles servent ici
				// (pas l'entrée ni la sortie : SMK_WALL_LIGHT, SMK_WALL_STUB…).
				if(pt.type == "Sitting_Spot"){ groupe = "Sitting_Boys"; choixAnims = {2, 3, 4, 5, 6}; }          // SIT_LAUGH / SIT_SMOKE / SIT_TALK_NPC1-3 _BENCH
				else if(pt.nom == "F_ClassSmokers"){ groupe = "POI_Smoking"; choixAnims = {7}; }              // SMK_STND_SMKB, fume debout
				else if(pt.type == "Wall"){ groupe = "POI_Smoking"; choixAnims = {3, 4}; }                    // SMK_WALL_SMKA / SMKB, fume adossé
				else if(pt.type == "Spectator"){ groupe = "NPC_Spectator"; choixAnims = {0, 1, 2}; }          // GEN_IMPRESSED03 / 01 / 02
				else if(pt.type == "Hang_Out"){ groupe = "Hang_Talking"; choixAnims = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}; }   // TALK, LISTEN, AMB_TALKING, AMB_WEIGHTSHIFT
				else if(pt.type == "Couple"){ groupe = "NPC_Love"; choixAnims = {5, 6}; couple = true; }       // KISS_LOOP_B (garçon) / KISS_LOOP_G (fille)
				// Deux piétons. Une bagarre : une prise du groupe Grap, jouée à deux depuis la
				// même origine et la même orientation (les décalages de bassin des deux
				// animations placent l'un par rapport à l'autre) ; identifiées à l'image
				// avec --paire : 7 + 6 l'un empoigne l'autre debout, 25 + 24 à califourchon
				// sur l'autre au sol, 45 + 24 les coups portés à califourchon. Un
				// harcèlement : face à face à `ecartDuo` mètres, l'agresseur provoque
				// (NPC_AggroTaunt), la victime, d'une autre clique, reste à l'attente (-1).
				bool duo = false, harcelement = false, prise = false; float ecartDuo = 0;
				if(pt.type == "Brawl"){
					// GRAP_IDLE_GV + _RCV, GRAP_MOUNT_IDLE_GV + _RCV, GRAP_MOUNT_HIT_F + MOUNT_IDLE_RCV.
					static const int paires[3][2] = { {7, 6}, {25, 24}, {45, 24} };
					groupe = "Grap"; choixAnims = { paires[k % 3][0], paires[k % 3][1] }; duo = prise = true;
				}
				else if(pt.type == "Harassment"){ groupe = "NPC_AggroTaunt"; choixAnims = {1, -1}; duo = harcelement = true; ecartDuo = 1.0f; }   // REAC_BRING_IT
				const std::vector<AgrAnim> *ga = nil;
				if(!groupe.empty()){
					if(!groupes.count(groupe)){ std::vector<AgrAnim> v; uint32 nb; uint8 *b = outil::LireMonde(a, groupe + ".agr", &nb); if(b){ AgrLireGroupe(b, nb, v); free(b); } groupes[groupe] = v; }
					ga = &groupes[groupe];
				}
				// Le lacet en degrés : le piéton regarde vers lacet + 180° (0 = +x). Établi
				// sur les places assises : les quatre « Sitting » autour de (597, −90)
				// (lacets 320, 45, 140, 230) regardent alors chacune vers l'extérieur, à
				// 2-4° près, et le banc de (530, −148) (290) tourne le dos à son mur.
				// Le corps animé regarde vers +y de son repère, comme celui qui marche.
				float capPoint = (pt.lacetTangageRoulis[0] + 180) * PI / 180;
				// Un couple : deux piétons face à face sur le point, de genres opposés quand
				// le point le permet.
				int places = couple || duo ? 2 : 1; bool premierFemme = false; std::string cliqueAgresseur;
				for(int pl = 0; pl < places; pl++){
					float cap = capPoint + (pl && !prise ? PI : 0), lacetP = -(cap - PI / 2);
					float px = pt.pos.x, py = pt.pos.y;
					if(duo && !prise){ float dd = (pl ? 0.5f : -0.5f) * ecartDuo; px += dd * cosf(capPoint); py += dd * sinf(capPoint); }
					float z = pt.pos.z; sol.Sol(px, py, pt.pos.z + 2.0f, 10.0f, &z);
					float qr[4] = { 0, 0, sinf(lacetP / 2), cosf(lacetP / 2) };
					// Le premier candidat qui convient et dont le modèle est dans le monde.
					// Les gens de tous les jours pas encore posés d'abord, puis les déjà
					// posés ; les variantes à part (unique = −1 : costumes d'Halloween,
					// sous-vêtements, Gary…) en dernier recours.
					const CPedIdeEntry *choix = nil; outil::Anime *an = nil;
					for(size_t j = 0; j < 3 * liste.size() && !choix; j++){
						const CPedIdeEntry &e = *liste[(j + k * 13 + pl * 5) % liste.size()];
						size_t passe = j / liste.size();
						if((passe == 2) != (e.unique < 0)) continue;
						if(passe == 0 && dejaPoses.count(e.model)) continue;
						if(pt.genre == "Male" && e.female) continue;
						if(pt.genre == "Female" && !e.female) continue;
						if(couple && pl == 1 && pt.genre == "Both" && passe < 2 && (bool)e.female == premierFemme) continue;
						if(harcelement && pl == 1){
							// La victime : un élève d'une autre clique que l'agresseur.
							if(strcasecmp(e.type, cliqueAgresseur.c_str()) == 0 || strcasecmp(e.type, "PREFECT") == 0 || strcasecmp(e.type, "COP") == 0 ||
							   strcasecmp(e.type, "TEACHER") == 0 || strcasecmp(e.type, "SHOPKEEP") == 0 || strcasecmp(e.type, "TOWNPERSON") == 0) continue;
						}else if(tous){
							// N'importe quel élève ou citadin : pas l'autorité, ni les commerçants.
							if(strcasecmp(e.type, "PREFECT") == 0 || strcasecmp(e.type, "COP") == 0 || strcasecmp(e.type, "TEACHER") == 0 || strcasecmp(e.type, "SHOPKEEP") == 0) continue;
						}else if(strcasecmp(e.type, clique.c_str()) != 0) continue;
						an = new outil::Anime;
						if(m->s->AjouterModele(a, e.model, NifFromPlacement(CVector(px, py, z), CVector(1, 1, 1), qr), false, an)) choix = &e;
						else{ delete an; an = nil; }
					}
					if(!choix){ printf("  point d'intérêt « %s » : aucun piéton %s %s\n", q.nom.c_str(), clique.c_str(), pt.genre.c_str()); break; }
					if(pl == 0){ premierFemme = choix->female != 0; cliqueAgresseur = choix->type; }
					const AgrAnim *att = nil; int n = -1;
					if(ga){
						n = couple || duo ? choixAnims[pl] : choixAnims[k % choixAnims.size()];
						if(couple) n = choix->female ? 6 : 5;     // _G pour la fille, _B pour le garçon
						if(n >= 0 && n < (int)ga->size() && (*ga)[n].decodee){ att = &(*ga)[n]; an->bassinRef = 0.86f; }
					}
					if(!att){ att = Attente(*choix); n = -1; }
					// Un couple : les animations avancent déjà le bassin vers l'autre (0,30 et
					// 0,50 m, NPC_Love n° 5 et 6). Chacun recule de cette avance, à l'échelle
					// de son modèle, pour que les deux bassins finissent à 35 cm.
					if(couple && n >= 0 && an->noeuds[1] >= 0){
						CVector r0; float k2 = ((const NifAVObject*)an->nif.blocks[an->noeuds[1]].data)->translation.z / an->bassinRef;
						if(AgrPositionOs(*att, 1, 0, &r0)){
							float recul = r0.y * k2 + 0.175f;
							px = pt.pos.x - recul * cosf(cap); py = pt.pos.y - recul * sinf(cap);
							sol.Sol(px, py, pt.pos.z + 2.0f, 10.0f, &z);
							an->place = NifFromPlacement(CVector(px, py, z), CVector(1, 1, 1), qr);
						}
					}
					if(!att){ delete an; printf("  point d'intérêt « %s » : pas d'attente pour %s\n", q.nom.c_str(), choix->model); break; }
					PietonAnime pa; pa.an = an; pa.anim = att; pa.att = att; pa.enAttente = true; pa.decalage = k * 0.37f; pa.poi = &pt;
					pa.cap = cap; pa.corps.pos = CVector(px, py, z); pa.depart = pa.corps.pos;
					animes.push_back(pa); poses++; dejaPoses.insert(choix->model);
					printf("  point d'intérêt « %s » %s%s%s (à %.0f m, lacet %.0f°) : %s %s, %s", q.nom.c_str(), pt.type.c_str(), pt.nom.empty() ? "" : " ", pt.nom.c_str(),
					       pr.first, pt.lacetTangageRoulis[0], choix->model, choix->type, n >= 0 ? groupe.c_str() : "attente");
					if(n >= 0) printf(" n° %d", n);
					printf("\n");
				}
				k++;
			}
		}
		// La population du jeu : la plus petite zone peuplée qui contient les pieds
		// de la caméra, et pour le moment demandé, autant de piétons de chaque
		// catégorie que le fichier l'écrit, posés au hasard à moins de 20 m, sur
		// le sol, dans la zone.
		if(moment >= 0){
			const CTriggerZone *ici = nil; float aire = 1e18f;
			CVector pieds(cam.pos.x, cam.pos.y, cam.pos.z - 1.6f);
			for(const CTriggerZone &z : population.zones){
				if(!z.aPopulation || !z.Contient(population.perimetres, pieds.x, pieds.y, pieds.z)) continue;
				const CTriggerPerimeter &pe = population.perimetres[z.perimetre];
				float A = 0; for(size_t i = 0, j = pe.x.size() - 1; i < pe.x.size(); j = i++) A += pe.x[j] * pe.y[i] - pe.x[i] * pe.y[j];
				if(fabsf(A) / 2 < aire){ aire = fabsf(A) / 2; ici = &z; }
			}
			if(!ici) printf("  population : aucune zone peuplée ici\n");
			else{
				uint32 h = 2024u; auto Alea = [&](void){ h = h * 1103515245u + 12345u; return ((h >> 8) & 0xffff) / 65535.0f; };
				int n = 0;
				for(int mo = momentsTous ? 0 : moment; mo <= (momentsTous ? MOMENT_NUM - 1 : moment); mo++){
				printf("  population : zone « %s » (%.0f m²), %s : %d piéton(s)", ici->nom.c_str(), aire, kMoment[mo], ici->total[mo]);
				for(int c = 0; c < POP_NUM; c++) if(ici->population[mo][c]) printf(", %d %s", ici->population[mo][c], kPopCategorie[c]);
				printf("\n");
				for(int c = 0; c < POP_NUM; c++){
					std::vector<const CPedIdeEntry*> modeles;
					for(const CPedIdeEntry *e : liste) if(strcasecmp(e->type, kPopCategorie[c]) == 0) modeles.push_back(e);
					for(int i = 0; i < ici->population[mo][c] && !modeles.empty(); i++){
						const CPedIdeEntry &e = *modeles[(i * 5 + n) % modeles.size()];
						float x = 0, y = 0, z = 0; bool trouve = false;
						for(int essai = 0; essai < 40 && !trouve; essai++){
							float r = 3 + 17 * Alea(), a2 = 2 * PI * Alea();
							x = pieds.x + r * cosf(a2); y = pieds.y + r * sinf(a2);
							trouve = sol.Sol(x, y, pieds.z + 20.0f, 40.0f, &z) && ici->Contient(population.perimetres, x, y, z + 0.1f);
						}
						if(!trouve) continue;
						float cap = 2 * PI * Alea(), lacetP = -(cap - PI / 2);
						float q[4] = { 0, 0, sinf(lacetP / 2), cosf(lacetP / 2) };
						outil::Anime *an = new outil::Anime;
						if(!m->s->AjouterModele(a, e.model, NifFromPlacement(CVector(x, y, z), CVector(1, 1, 1), q), false, an)){ delete an; continue; }
						const AgrAnim *pas = PasDeMarche(e), *att = Attente(e);
						if(!pas && !att){ delete an; continue; }
						PietonAnime pa; pa.an = an; pa.anim = pas ? pas : att; pa.pas = pas; pa.att = att;
						pa.decalage = n * 0.61f; pa.hasard = 4242u + n * 7919u; pa.dureeEtat = pa.Hasard(2, 10); pa.zone = ici;
						if(momentsTous) pa.momentPop = mo;
						if(pas){ CVector d = AgrDeplacement(*pas); pa.marche = true; pa.vitesse = hypotf(d.x, d.y) / pas->duree; }
						pa.cap = cap; pa.corps.pos = CVector(x, y, z); pa.depart = pa.corps.pos;
						animes.push_back(pa); poses++; n++;
						printf("    %-22s %-10s (%.1f, %.1f, %.2f)\n", e.model, e.type, x, y, z);
					}
				}
				}
			}
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
		if(!mPietons || animes.empty()){
			// Sans piétons, l'horloge avance quand même.
			if(t > tPietons){
				int32 avantMoment = heureJeu.Moment(); heureJeu.Avancer(t - tPietons); tPietons = t;
				if(heureJeu.Moment() != avantMoment)
					printf("  il est %02d:%02d : passage de %s à %s\n", heureJeu.Heure(), heureJeu.Minute(), kMoment[avantMoment], kMoment[heureJeu.Moment()]);
			}
			return;
		}
		while(tPietons < t){
			float dt = fminf(1.0f / 60, t - tPietons); tPietons += dt;
			// L'horloge du jeu : une minute par seconde ; on annonce chaque changement de moment.
			int32 avantMoment = heureJeu.Moment();
			heureJeu.Avancer(dt);
			if(heureJeu.Moment() != avantMoment)
				printf("  il est %02d:%02d : passage de %s à %s\n", heureJeu.Heure(), heureJeu.Minute(), kMoment[avantMoment], kMoment[heureJeu.Moment()]);
			for(PietonAnime &p : animes){
				// Un piéton hors de son moment n'existe pas pour l'instant : caché, figé.
				if(p.momentPop >= 0 || p.poi){
					bool absent = !Present(p);
					if(p.an->bloc >= 0) mPietons->s->blocs[p.an->bloc].cache = absent;
					if(absent) continue;
				}
				p.horloge += dt;
				if(p.fondu > 0){ p.horlogeAvant += dt; p.fondu -= dt; }
				auto Basculer = [&](bool versAttente){
					if(versAttente == p.enAttente || (versAttente && !p.att)) return;
					p.avant = p.anim; p.horlogeAvant = p.horloge + p.decalage;
					p.enAttente = versAttente; p.anim = versAttente ? p.att : p.pas;
					p.horloge = 0; p.decalage = 0; p.fondu = FONDU; p.tEtat = 0;
				};
				if(p.trajet){
					const std::vector<CTriggerPathPoint> &pts = p.trajet->points;
					if(p.enAttente || p.attenteRestante > 0){
						p.attenteRestante -= dt;
						// Les actions du point, dans l'ordre, chacune le temps écrit :
						// le piéton se tourne vers l'orientation de celle en cours.
						if(p.arret && !p.arret->actions.empty()){
							float ecoule = p.attenteTotale - p.attenteRestante, fin = 0;
							for(const CTriggerAction &ac : p.arret->actions){
								p.capVise = ac.orientation[0] * PI / 180;
								fin += ac.attente; if(ecoule < fin) break;
							}
						}
						if(p.attenteRestante <= 0) Basculer(false);
					}else{
						const CTriggerPathPoint &c = pts[p.cible];
						float dx = c.pos.x - p.corps.pos.x, dy = c.pos.y - p.corps.pos.y;
						p.capVise = atan2f(dy, dx);
						if(hypotf(dx, dy) < 0.5f || p.bloque > 2.0f){
							// Arrivé (ou coincé : on passe au point suivant). Les actions
							// d'abord, l'une après l'autre, puis l'attente du point. Une
							// action regarde vers son lacet (degrés, 0 = +x) : établi à la
							// cafétéria, où la file de CafPath1 (lacet 90) fait face au
							// comptoir, vers +y. (Pas la règle des points d'intérêt, lacet
							// + 180° : ORIENTATION et YAWPITCHROLL ne s'écrivent pas pareil.)
							float w = c.attente;
							for(const CTriggerAction &ac : c.actions) w += ac.attente;
							p.arret = &c; p.attenteTotale = w;
							if(!c.actions.empty()) p.capVise = c.actions[0].orientation[0] * PI / 180;
							p.cible = (p.cible + 1) % (int32)pts.size(); p.bloque = 0;
							if(w > 0.05f){ p.attenteRestante = w; Basculer(true); }
						}
					}
					// Le cap tourne vers le cap visé, 4 rad/s au plus.
					float e = remainderf(p.capVise - p.cap, 2 * PI), pasMax = 4.0f * dt;
					p.cap += e > pasMax ? pasMax : e < -pasMax ? -pasMax : e;
				}else if(p.pas && p.att){
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
				// Un piéton de population fait demi-tour au bord de sa zone.
				if(p.zone && !p.zone->Contient(population.perimetres, p.corps.pos.x + cosf(p.cap) * 0.6f, p.corps.pos.y + sinf(p.cap) * 0.6f, p.corps.pos.z + 0.1f)) p.cap += PI;
				p.corps.Avancer(sol, cosf(p.cap) * p.vitesse * part * dt, sinf(p.cap) * p.vitesse * part * dt, dt);
				float fait = hypotf(p.corps.pos.x - avant.x, p.corps.pos.y - avant.y);
				p.bloque = fait < 0.3f * p.vitesse * part * dt ? p.bloque + dt : 0;
				if(p.bloque > 0.25f && !p.trajet){
					// Contre un mur : un quart à un demi-tour, d'un côté au hasard.
					p.hasard = p.hasard * 1103515245u + 12345u;
					float quart = (90 + (p.hasard >> 16) % 90) * PI / 180;
					p.cap += (p.hasard >> 8 & 1) ? quart : -quart; p.bloque = 0;
				}
			}
		}
		for(PietonAnime &p : animes){
			if(!Present(p)) continue;
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
		for(const PietonAnime &p : animes) if(p.marche && temps > 0 && Present(p))
			printf("    marche %.2f m/s (%.3f s par cycle) : %.1f m du départ, %s\n", p.vitesse, p.anim->duree,
			       hypotf(p.corps.pos.x - p.an->place.t.x, p.corps.pos.y - p.an->place.t.y) >= 0 ? hypotf(p.corps.pos.x - p.depart.x, p.corps.pos.y - p.depart.y) : 0,
			       p.corps.auSol ? "au sol" : "en l'air"),
			printf("      %s depuis %.1f s (prochain changement à %.1f s)%s, en (%.2f, %.2f, %.2f) cap %.0f°\n", p.enAttente ? "à l'arrêt" : "en marche", p.tEtat, p.dureeEtat, p.fondu > 0 ? ", en fondu" : "",
			       p.corps.pos.x, p.corps.pos.y, p.corps.pos.z, p.cap * 180 / PI);
		for(const PietonAnime &p : animes) if(p.trajet)
			printf("    patrouille « %s » : vers le point %d sur %zu, %s, en (%.2f, %.2f, %.2f), à %.1f m de son départ\n", p.trajet->nom.c_str(), p.cible, p.trajet->points.size(),
			       p.enAttente ? "à l'arrêt" : "en marche", p.corps.pos.x, p.corps.pos.y, p.corps.pos.z, hypotf(p.corps.pos.x - p.depart.x, p.corps.pos.y - p.depart.y));
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
		for(int k = 0; k < n; k++){ cam.yaw = yaw0 + k * (2 * PI / n); (MajCiel(), v.Rendre(cam)); }
		double ms = std::chrono::duration<double, std::milli>(horloge::now() - t0).count() / n;
		printf("  %d image(s) 400×240, %.1f ms par image (%.0f i/s), %d triangles dessinés à la dernière (%d modèles retenus, %d faces arrière, %d halos)\n", n, ms, 1000.0 / ms, v.dessines, v.modelesVus, v.arriere, v.halos);
		if(!image.empty()){ cam.yaw = yaw0; (MajCiel(), v.Rendre(cam)); bool ok = RasterWritePPM(v.img, image.c_str()); printf("  → %s%s\n", image.c_str(), ok ? "" : " (échec)"); }
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
		(MajCiel(), v.Rendre(cam));
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
