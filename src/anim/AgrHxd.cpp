// Les animations d'un .agr se jouent sur la durée de leur HXD (docs/hxd.md).
//
// bully.exe, FUN_006be250 (l'évaluation d'un os à l'instant t) : t est borné à
// la durée de l'enregistrement HXD (+0xc), puis divisé par elle ; la position
// entre 0 et 1 qui en sort donne la clé. Les instants des clés du .agr sont
// stockés normalisés : la durée de l'en-tête du .agr ne sert pas à les lire.
#include "AgrHxd.h"
#include <strings.h>

int32
HxdEtirer(std::vector<AgrAnim> &anims, const CHxdFile &h, const char *groupe)
{
	int32 g = -1;
	for(size_t i = 0; i < h.groupes.size(); i++)
		if(strcasecmp(h.groupes[i].nom.c_str(), groupe) == 0){ g = (int32)i; break; }
	if(g < 0) return 0;
	int32 k = 0, etirees = 0;
	for(const CHxdAnim &x : h.anims){
		if(x.groupe != (uint32)g) continue;
		if(k >= (int32)anims.size()) break;
		AgrAnim &a = anims[k++];
		if(x.duree <= 0 || a.duree <= 0 || x.duree == a.duree) continue;
		float s = x.duree / a.duree;
		for(int32 o = 0; o < AGR_OS; o++){
			for(AgrCle &c : a.pistes[o]) c.t *= s;
			for(AgrPosition &p : a.positions[o]) p.t *= s;
		}
		a.duree = x.duree;
		etirees++;
	}
	return etirees;
}
