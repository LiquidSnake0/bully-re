// La synchronisation labiale : chargement (FUN_0068df50) et lecture (FUN_0068dd30).
#include "Lip.h"
#include <cstring>

static uint16 Lire16(const uint8 *p){ return (uint16)(p[0] | p[1] << 8); }
static uint32 Lire32(const uint8 *p){ return (uint32)p[0] | (uint32)p[1] << 8 | (uint32)p[2] << 16 | (uint32)p[3] << 24; }

bool
CLipFile::Load(const uint8 *buf, uint32 n)
{
	repliques.clear(); donnees.clear();
	if(n < 4) return false;
	uint32 nb = Lire16(buf), taille = Lire16(buf + 2);
	uint32 debut = 4 + nb * 24;
	if(debut + taille > n) return false;
	for(uint32 i = 0; i < nb; i++){
		const uint8 *p = buf + 4 + i * 24;
		CLipReplique r;
		r.id = Lire32(p);
		uint32 f = Lire32(p + 4); memcpy(&r.duree, &f, 4);
		r.images = Lire16(p + 8);
		r.depart = Lire16(p + 10);
		r.sonDecalage = Lire32(p + 12);
		r.sonTaille = Lire32(p + 16);
		r.hachage = Lire32(p + 20);
		if(r.depart + (r.images + 3u) / 4 > taille) return false;
		repliques.push_back(r);
	}
	donnees.assign(buf + debut, buf + debut + taille);
	return true;
}

int32
CLipFile::Niveau(const CLipReplique &r, int32 k) const
{
	if(k < 0 || k >= r.images) return 0;
	uint8 o = donnees[r.depart + k / 4];
	return o >> ((3 - k % 4) * 2) & 3;
}

float
CLipFile::Ouverture(const CLipReplique &r, float u) const
{
	if(r.images == 0) return 0;
	if(u < 0) u = 0;
	if(u > 1) u = 1;
	float x = u * (float)(r.images - 1);
	int32 i = (int32)x;                    // _ftol : troncature
	int32 j = i + 1 < r.images ? i + 1 : i;
	float f = x - (float)i;
	return ((1.0f - f) * (float)Niveau(r, i) + f * (float)Niveau(r, j)) * (1.0f / 3.0f);
}
