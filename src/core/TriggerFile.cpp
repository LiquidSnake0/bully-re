#include "TriggerFile.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>

namespace {

// Lecture ligne par ligne : chaque ligne est « MOT valeurs », indentée de tabulations.
struct Lignes {
	const char *p, *fin;
	std::string mot, reste;
	bool Suivante(void){
		while(p < fin){
			const char *d = p;
			while(p < fin && *p != '\n') p++;
			const char *f = p; if(p < fin) p++;
			while(d < f && (*d == ' ' || *d == '\t')) d++;
			while(f > d && (f[-1] == '\r' || f[-1] == ' ' || f[-1] == '\t')) f--;
			if(d == f) continue;
			const char *e = d; while(e < f && *e != ' ' && *e != '\t') e++;
			mot.assign(d, e);
			while(e < f && (*e == ' ' || *e == '\t')) e++;
			reste.assign(e, f);
			return true;
		}
		return false;
	}
};

void Nombres(const std::string &s, float *v, int n){
	const char *c = s.c_str();
	for(int i = 0; i < n; i++){
		char *e; v[i] = strtof(c, &e); if(e == c) return;
		c = e; while(*c == ',' || *c == ' ') c++;
	}
}

std::string Guillemets(const std::string &s){
	size_t a = s.find('"'), b = s.rfind('"');
	return a != std::string::npos && b > a ? s.substr(a + 1, b - a - 1) : s;
}

}

const char *const kPopCategorie[POP_NUM] = { "PREFECT", "NERD", "JOCK", "DROPOUT", "GREASER", "PREPPY", "STUDENT",
                                            "COP", "TEACHER", "TOWNPERSON", "SHOPKEEP", "BULLY" };
const char *const kMoment[MOMENT_NUM] = { "DAY", "CLASS", "NIGHT", "CURFEW" };

bool
CTriggerZone::Contient(const std::vector<CTriggerPerimeter> &perimetres, float x, float y, float z) const
{
	if(perimetre < 0 || perimetre >= (int32)perimetres.size()) return false;
	if(z < pos.z || z > pos.z + zHauteur) return false;
	const CTriggerPerimeter &p = perimetres[perimetre];
	// Dans le repère du déclencheur : translation puis rotation inverse du lacet.
	float a = -lacet * 3.14159265f / 180, c = cosf(a), s = sinf(a);
	float dx = x - pos.x, dy = y - pos.y, lx = dx * c - dy * s, ly = dx * s + dy * c;
	bool dedans = false;
	size_t n = p.x.size();
	for(size_t i = 0, j = n - 1; i < n; j = i++)
		if((p.y[i] > ly) != (p.y[j] > ly) && lx < (p.x[j] - p.x[i]) * (ly - p.y[i]) / (p.y[j] - p.y[i]) + p.x[i]) dedans = !dedans;
	return dedans;
}

bool
CTriggerFile::Load(const uint8 *data, uint32 size)
{
	Lignes l{(const char*)data, (const char*)data + size};
	while(l.p < l.fin && l.fin[-1] == 0) l.fin--;            // le secteur est complété de zéros
	bool entete = false, corps = false;
	while(l.Suivante()){
		// L'en-tête : les cinq comptes en tête de fichier. Plus loin, « NTRIGGERS k »
		// entre deux blocs annonce les k déclencheurs du périmètre qui précède.
		if(!corps && l.mot == "NPATHS"){ nPaths = atoi(l.reste.c_str()); entete = true; }
		else if(!corps && l.mot == "NPOINTS") nPoints = atoi(l.reste.c_str());
		else if(!corps && l.mot == "NPERIMETERS") nPerimeters = atoi(l.reste.c_str());
		else if(!corps && l.mot == "NTRIGGERS") nTriggers = atoi(l.reste.c_str());
		else if(!corps && l.mot == "NPOIS") nPois = atoi(l.reste.c_str());
		else if(l.mot == "PERIMETER"){
			corps = true;
			if(!l.Suivante() || l.mot != "BEGIN") return false;
			CTriggerPerimeter pe;
			while(l.Suivante() && l.mot != "END"){
				if(l.mot == "PLACEMENT") pe.placement = Guillemets(l.reste);
				else if(l.mot == "ISDOOR") pe.porte = l.reste.find("TRUE") != std::string::npos;
				else if(l.mot == "HEIGHT") Nombres(l.reste, &pe.hauteur, 1);
				else if(l.mot == "DEPTH") Nombres(l.reste, &pe.profondeur, 1);
				else if(l.mot == "PERIMETERPOINTX"){ float v = 0; Nombres(l.reste, &v, 1); pe.x.push_back(v); }
				else if(l.mot == "PERIMETERPOINTY"){ float v = 0; Nombres(l.reste, &v, 1); pe.y.push_back(v); }
			}
			if(l.mot != "END" || pe.x.size() != pe.y.size()) return false;
			perimetres.push_back(std::move(pe));
		}
		else if(l.mot == "TRIGGER"){
			corps = true;
			if(!l.Suivante() || l.mot != "BEGIN") return false;
			CTriggerZone z; z.perimetre = (int32)perimetres.size() - 1;
			// Les sous-blocs (POPULATIONDATA, POPULATION_DAY…) ont leurs propres
			// BEGIN / END : on suit la profondeur, et le tableau en cours.
			int prof = 1, moment = -1; bool vehicules = false;
			while(prof > 0 && l.Suivante()){
				if(l.mot == "BEGIN"){ prof++; continue; }
				if(l.mot == "END"){ prof--; if(prof <= 2) moment = -1; continue; }
				if(l.mot == "TRIGGERNAME") z.nom = Guillemets(l.reste);
				else if(l.mot == "AREACODE") z.zone = atoi(l.reste.c_str());
				else if(l.mot == "POSITION"){ float v[3] = {0, 0, 0}; Nombres(l.reste, v, 3); z.pos = CVector(v[0], v[1], v[2]); }
				else if(l.mot == "YAW") Nombres(l.reste, &z.lacet, 1);
				else if(l.mot == "ZHEIGHT") Nombres(l.reste, &z.zHauteur, 1);
				else if(l.mot == "ISMISSIONSPECIFIC") z.mission = l.reste.find("TRUE") != std::string::npos;
				else if(l.mot == "HASPOPULATIONDATA") z.aPopulation = atoi(l.reste.c_str()) != 0;
				else if(l.mot == "HASAMBIENTVEHICLEDATA") z.aVehicules = atoi(l.reste.c_str()) != 0;
				else if(l.mot.compare(0, 11, "POPULATION_") == 0 || l.mot.compare(0, 16, "AMBIENTVEHICLES_") == 0){
					vehicules = l.mot[0] == 'A';
					std::string m = l.mot.substr(vehicules ? 16 : 11);
					moment = -1; for(int k = 0; k < MOMENT_NUM; k++) if(m == kMoment[k]) moment = k;
				}
				else if(moment >= 0 && l.mot == "TOTAL"){ int v = atoi(l.reste.c_str()); if(vehicules) z.vehicules[moment][0] = v; else z.total[moment] = v; }
				else if(moment >= 0 && vehicules){
					if(l.mot == "CAR") z.vehicules[moment][1] = atoi(l.reste.c_str());
					else if(l.mot == "BIKE") z.vehicules[moment][2] = atoi(l.reste.c_str());
					else if(l.mot == "POLICECAR") z.vehicules[moment][3] = atoi(l.reste.c_str());
				}
				else if(moment >= 0)
					for(int k = 0; k < POP_NUM; k++) if(l.mot == kPopCategorie[k]) z.population[moment][k] = atoi(l.reste.c_str());
			}
			if(prof != 0) return false;
			zones.push_back(std::move(z));
		}
		else if(l.mot == "PATH"){
			corps = true;
			if(!l.Suivante() || l.mot != "BEGIN") return false;
			CTriggerPath t; CTriggerPathPoint *pt = nil; CTriggerAction *ac = nil; int32 attendus = -1;
			while(l.Suivante() && l.mot != "END"){
				if(l.mot == "PATHNAME") t.nom = Guillemets(l.reste);
				else if(l.mot == "AREACODE") t.zone = atoi(l.reste.c_str());
				else if(l.mot == "PATROLTYPE") t.patrouille = atoi(l.reste.c_str());
				else if(l.mot == "NPATHPOINTS") attendus = atoi(l.reste.c_str());
				else if(l.mot == "PATHPOINT"){ t.points.emplace_back(); pt = &t.points.back(); ac = nil; float v[3] = {0, 0, 0}; Nombres(l.reste, v, 3); pt->pos = CVector(v[0], v[1], v[2]); }
				else if(!pt) continue;
				else if(l.mot == "POINTYAWPITCHROLL") Nombres(l.reste, pt->lacetTangageRoulis, 3);
				// WAITTIME vient deux fois : celui du point, puis celui de l'action en cours.
				else if(l.mot == "WAITTIME"){ float w = 0; Nombres(l.reste, &w, 1); if(ac) ac->attente = w; else pt->attente = w; }
				else if(l.mot == "UNID") pt->unid = atoi(l.reste.c_str());
				else if(l.mot == "ACTIONTYPE"){ pt->actions.emplace_back(); ac = &pt->actions.back(); ac->type = atoi(l.reste.c_str()); }
				else if(l.mot == "ORIENTATION" && ac) Nombres(l.reste, ac->orientation, 3);
			}
			if(l.mot != "END") return false;
			// Les données du jeu ne sont pas toujours cohérentes : le trajet
			// « 1_S01_Running_Prefect_Path » annonce 9 points et en écrit 8. On
			// garde ceux qui sont là et on compte l'écart.
			if(attendus >= 0 && (int32)t.points.size() != attendus) incoherences++;
			paths.push_back(std::move(t));
		}
	}
	return entete && (int32)paths.size() == nPaths;
}
