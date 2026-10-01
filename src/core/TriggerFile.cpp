#include "TriggerFile.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

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

bool
CTriggerFile::Load(const uint8 *data, uint32 size)
{
	Lignes l{(const char*)data, (const char*)data + size};
	while(l.p < l.fin && l.fin[-1] == 0) l.fin--;            // le secteur est complété de zéros
	bool entete = false;
	while(l.Suivante()){
		if(l.mot == "NPATHS"){ nPaths = atoi(l.reste.c_str()); entete = true; }
		else if(l.mot == "NPOINTS" && paths.empty()) nPoints = atoi(l.reste.c_str());
		else if(l.mot == "NPERIMETERS") nPerimeters = atoi(l.reste.c_str());
		else if(l.mot == "NTRIGGERS" && nTriggers == 0) nTriggers = atoi(l.reste.c_str());
		else if(l.mot == "NPOIS") nPois = atoi(l.reste.c_str());
		else if(l.mot == "PATH"){
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
