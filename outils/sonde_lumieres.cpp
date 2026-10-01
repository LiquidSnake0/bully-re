// Sonde jetable : les modèles à horaire (tobj) et les effets de lumière (2dfx).
#include "commun.h"
#include <map>

int
main(void)
{
	outil::Archives a;
	if(!outil::Ouvrir(a)) return 1;
	std::map<std::pair<int32, int32>, int> heures;
	for(auto &h : a.horaireDe) heures[h.second]++;
	printf("%zu modèles tobj ; heures (allumage, extinction) :\n", a.horaireDe.size());
	for(auto &h : heures) printf("  %2d → %2d : %d\n", h.first.first, h.first.second, h.second);
	int n = 0; for(auto &h : a.horaireDe) if(n++ < 12) printf("    %s %d-%d\n", h.first.c_str(), h.second.first, h.second.second);
	size_t total = 0; std::map<std::string, int> coronas; std::map<int, int> drapeaux, types;
	for(auto &e : a.effetsDe){ total += e.second.size(); for(auto &x : e.second){ coronas[x.corona]++; drapeaux[x.flags]++; types[x.type]++; } }
	printf("%zu effets 2dfx sur %zu modèles\n", total, a.effetsDe.size());
	for(auto &c : coronas) printf("  corona « %s » : %d\n", c.first.c_str(), c.second);
	for(auto &d : drapeaux) printf("  flags %d : %d\n", d.first, d.second);
	n = 0;
	for(auto &e : a.effetsDe) for(auto &x : e.second) if(n++ < 10){
		auto m = a.modeleDe.find(e.first);
		printf("    %s : pos (%.2f %.2f %.2f) couleur %d %d %d %d dist %.1f portée %.1f taille %.2f ombre %.2f flags %d b38 %d b39 %d b3a %d b3b %d v2c %d v30 %.2f v34 %d\n",
		       m != a.modeleDe.end() ? m->second.c_str() : "?", x.pos[0], x.pos[1], x.pos[2], x.col[0], x.col[1], x.col[2], x.col[3],
		       x.dist, x.range, x.size, x.shadowSize, x.flags, x.bool38, x.byte39, x.byte3a, x.byte3b, x.val2c, x.val30, x.val34);
	}
	printf("tobj -1 → 7 :"); for(auto &h : a.horaireDe) if(h.second.first == -1) printf(" %s", h.first.c_str()); printf("\n");
	std::map<std::string, int> parModele;
	for(auto &e : a.effetsDe){ auto m = a.modeleDe.find(e.first); parModele[m != a.modeleDe.end() ? m->second : "?"] += (int)e.second.size(); }
	printf("modèles avec lumières :"); for(auto &m : parModele) printf(" %s(%d)", m.first.c_str(), m.second); printf("\n");
	return 0;
}
