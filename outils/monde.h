// La carte des placements : les 85 fichiers « Ipl$ » (.ipb) de World.img, avec
// l'emprise de chacun dans le monde. C'est ce qui permet de savoir, depuis
// une position, quelles scènes sont voisines et doivent être chargées.
#pragma once
#include "scene.h"
#include <strings.h>
#include <algorithm>
#include <cmath>

namespace outil {

struct EmpriseIpb {
	std::string nom;                 // tel que dans World.img, avec l'extension
	int32 placements = 0;
	float xmin = 0, xmax = 0, ymin = 0, ymax = 0, zmin = 0, zmax = 0;

	// Distance horizontale d'un point à l'emprise (0 à l'intérieur).
	float Distance(float x, float y) const {
		float dx = x < xmin ? xmin - x : (x > xmax ? x - xmax : 0);
		float dy = y < ymin ? ymin - y : (y > ymax ? y - ymax : 0);
		return sqrtf(dx * dx + dy * dy);
	}
};

inline std::vector<CIplInst> *g_carteInst = nil;
inline int32 GarderCarte(const CIplInst &e){ g_carteInst->push_back(e); return 0; }

// Lit tous les .ipb de World.img et rend leurs emprises (sans charger de modèle).
inline std::vector<EmpriseIpb> CarteDesIpb(Archives &a){
	std::vector<EmpriseIpb> carte;
	const CdImage &img = CdStream::ms_images[a.monde];
	for(int32 k = 0; k < img.m_numEntries; k++){
		const CDirectoryEntry *d = &img.m_entries[k]; size_t L = strlen(d->name);
		if(L < 4 || strcasecmp(d->name + L - 4, ".ipb") != 0) continue;
		uint32 n; uint8 *b = LireEntree(a.monde, "Stream\\World.img", d->name, &n);
		if(b == nil) continue;
		std::vector<CIplInst> inst; g_carteInst = &inst;
		CIplFile::ms_instHandler = GarderCarte;
		CIplFile::Load(b, n);
		free(b);
		EmpriseIpb e; e.nom = d->name; e.placements = (int32)inst.size();
		bool premier = true;
		for(const CIplInst &p : inst){
			if(premier){ e.xmin = e.xmax = p.pos.x; e.ymin = e.ymax = p.pos.y; e.zmin = e.zmax = p.pos.z; premier = false; continue; }
			e.xmin = fminf(e.xmin, p.pos.x); e.xmax = fmaxf(e.xmax, p.pos.x);
			e.ymin = fminf(e.ymin, p.pos.y); e.ymax = fmaxf(e.ymax, p.pos.y);
			e.zmin = fminf(e.zmin, p.pos.z); e.zmax = fmaxf(e.zmax, p.pos.z);
		}
		if(!inst.empty()) carte.push_back(e);
	}
	std::sort(carte.begin(), carte.end(), [](const EmpriseIpb &a, const EmpriseIpb &b){ return a.nom < b.nom; });
	return carte;
}

} // namespace outil
