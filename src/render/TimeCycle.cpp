#include "TimeCycle.h"
#include <cstring>
#include <cstdlib>
#include <cmath>

bool
CTimeCycle::Load(const char *texte, size_t taille)
{
	meteos.clear(); heures.clear();
	const char *p = texte, *fin = texte + taille;
	std::string nom = "?";
	while(p < fin){
		const char *d = p; while(p < fin && *p != '\n') p++;
		std::string l(d, p); if(p < fin) p++;
		while(!l.empty() && (l.back() == '\r' || l.back() == ' ' || l.back() == '\t')) l.pop_back();
		size_t a = l.find_first_not_of(" \t");
		if(a == std::string::npos) continue;
		if(l.compare(a, 6, "//////") == 0){
			// « ////////////// Sunny » : une nouvelle section de 24 heures.
			size_t b = l.find_first_not_of("/ \t", a);
			nom = b == std::string::npos ? "?" : l.substr(b);
			meteos.push_back(nom); heures.emplace_back();
			continue;
		}
		if(l.compare(a, 2, "//") == 0) continue;
		if(heures.empty()){ meteos.push_back(nom); heures.emplace_back(); }
		float v[37]; int n = 0;
		const char *c = l.c_str() + a;
		while(n < 37){
			char *e; float x = strtof(c, &e); if(e == c) break;
			v[n++] = x; c = e;
		}
		if(n < 37) return false;
		CTimeCycleHeure t; int k = 0;
		auto Lire3 = [&](float *o){ o[0] = v[k++]; o[1] = v[k++]; o[2] = v[k++]; };
		Lire3(t.ambObjets); Lire3(t.ambMonde); Lire3(t.ambPietons); Lire3(t.soleil); Lire3(t.contreJour);
		t.contreJourInt = v[k++]; Lire3(t.cielHaut); Lire3(t.cielBas); Lire3(t.soleilCoeur); Lire3(t.soleilHalo);
		t.soleilTaille = v[k++]; t.sprTaille = v[k++]; t.sprBrillance = v[k++];
		t.ombre = v[k++]; t.ombreLumiere = v[k++]; t.ombrePoteaux = v[k++];
		t.farClp = v[k++]; t.fogSt = v[k++]; t.nightFactor = v[k++];
		heures.back().push_back(t);
	}
	for(const auto &m : heures) if(m.size() != 24) return false;
	return !heures.empty();
}

CTimeCycleHeure
CTimeCycle::Etat(int32 meteo, float h) const
{
	if(meteo < 0 || meteo >= (int32)heures.size()) meteo = 0;
	h = fmodf(h, 24.0f); if(h < 0) h += 24;
	int32 a = (int32)h, b = (a + 1) % 24; float u = h - a;
	const float *x = (const float*)&heures[meteo][a], *y = (const float*)&heures[meteo][b];
	CTimeCycleHeure r; float *o = (float*)&r;
	for(size_t i = 0; i < sizeof(r) / sizeof(float); i++) o[i] = x[i] + u * (y[i] - x[i]);
	return r;
}
