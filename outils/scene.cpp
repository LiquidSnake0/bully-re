// Rend une scène du jeu depuis un fichier de placements « Ipl$ » (.ipb) :
// chaque entrée inst nomme un modèle, une position, une échelle par axe et un
// quaternion. Tous les modèles sont placés puis dessinés ensemble.
//   BULLY_DATA=<racine> build/outils/scene <fichier.ipb> [sortie.ppm] [azimut] [élévation] [taille] [--coupe f]
// --coupe f ne dessine pas les triangles situés au-dessus de la fraction f de
// la hauteur de la scène : on enlève le toit pour regarder dedans.
// Le sens du quaternion des placements est établi dans docs/ipl.md et
// vérifié par tests/test_placement.
#include "commun.h"
#include "scene.h"
#include "../src/core/IplFile.h"

static std::vector<CIplInst> g_inst;

// Modèles que le jeu ne dessine jamais : aides à la navigation (le binaire
// pose le drapeau 0x1000000 sur nog_ / walkable_, docs/idb.md) et maillages
// « no draw » (_ND). Leurs sommets sont souvent des triangles fantômes posés
// loin sous la scène.
static bool
JamaisDessine(const std::string &nom)
{
	std::string n = outil::Minuscules(nom.c_str());
	if(n.compare(0, 4, "nog_") == 0 || n.compare(0, 5, "nogo_") == 0 || n.compare(0, 9, "walkable_") == 0) return true;
	return n.find("_nd_") != std::string::npos || (n.size() > 3 && n.compare(n.size() - 3, 3, "_nd") == 0);
}
static int32 GarderInst(const CIplInst &e){ g_inst.push_back(e); return 0; }

int
main(int argc, char **argv)
{
	if(argc < 2){ fprintf(stderr, "usage : scene <fichier.ipb> [sortie.ppm] [azimut] [élévation] [taille] [--coupe fraction]\n"); return 2; }
	std::string ipb = argv[1]; float coupe = 1.0f; std::vector<std::string> pos;
	for(int i = 2; i < argc; i++){ if(strcmp(argv[i], "--coupe") == 0 && i + 1 < argc) coupe = (float)atof(argv[++i]); else pos.push_back(argv[i]); }
	std::string sortie = pos.size() > 0 ? pos[0] : ipb + ".ppm";
	float azim = pos.size() > 1 ? (float)atof(pos[1].c_str()) : 35.0f, elev = pos.size() > 2 ? (float)atof(pos[2].c_str()) : 35.0f;
	int32 taille = pos.size() > 3 ? atoi(pos[3].c_str()) : 1000;

	outil::Archives a;
	if(!outil::Ouvrir(a)) return 1;
	uint32 nb; uint8 *buf = outil::LireMonde(a, ipb, &nb);
	if(buf == nil){ fprintf(stderr, "%s absent de World.img\n", ipb.c_str()); return 1; }
	CIplFile::ms_instHandler = GarderInst;
	CIplFile::Load(buf, nb);
	free(buf);
	printf("%s : %zu placements\n", ipb.c_str(), g_inst.size());

	outil::Scene s;
	std::map<std::string, int> compte; int ignores = 0;
	for(const CIplInst &e : g_inst){
		std::string modele = e.name;
		if(modele.empty()){ auto it = a.modeleDe.find(e.modelId); if(it == a.modeleDe.end()) continue; modele = it->second; }
		if(JamaisDessine(modele)){ ignores++; continue; }
		if(s.AjouterModele(a, modele, NifFromPlacement(e.pos, e.scale, e.rot))) compte[modele]++;
	}
	printf("  %d modèles placés (%d introuvables, %d jamais dessinés ignorés), %zu triangles, %zu textures, %zu modèles distincts\n",
	       s.modeles, s.manquants, ignores, s.tri.size() / 3, s.texNoms.size(), compte.size());
	bool ok = s.Rendre(sortie, azim, elev, taille, coupe);
	printf("  → %s%s\n", sortie.c_str(), ok ? "" : " (échec)");
	return ok ? 0 : 1;
}
