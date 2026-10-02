#include "Hxd.h"
#include <cstring>

namespace {
struct Lecteur {
	const uint8 *b; uint32 n, p = 0; bool ok = true;
	bool Reste(uint32 k){ if(p + k > n) ok = false; return ok; }
	uint32 U32(void){ uint32 v = 0; if(Reste(4)){ memcpy(&v, b + p, 4); p += 4; } return v; }
	uint16 U16(void){ uint16 v = 0; if(Reste(2)){ memcpy(&v, b + p, 2); p += 2; } return v; }
	float F32(void){ float v = 0; if(Reste(4)){ memcpy(&v, b + p, 4); p += 4; } return v; }
	void Sauter(uint32 k){ if(Reste(k)) p += k; }
	std::string Nom(uint32 k){
		std::string s; if(!Reste(k)) return s;
		for(uint32 i = 0; i < k && b[p + i]; i++) s += (char)b[p + i];
		p += k; return s;
	}
};
}

bool
CHxdFile::Load(const uint8 *buf, uint32 n)
{
	Lecteur r{buf, n};
	version = r.F32(); mot = r.U32();
	if(!r.ok || version > 1.12f + 1e-6f) return false;
	uint32 k = r.U32();
	for(uint32 i = 0; i < k && r.ok; i++){
		masques.push_back(r.Nom(0x20));
		uint32 m = r.U32(); r.U32(); r.Sauter(4 * m);
	}
	k = r.U32();
	for(uint32 i = 0; i < k && r.ok; i++) os.push_back(r.Nom(0x20));
	uint32 nos = k;
	k = r.U32();
	for(uint32 i = 0; i < k && r.ok; i++){
		CHxdAnim a;
		a.duree = r.F32(); a.f1 = r.F32(); a.nom = r.Nom(0x40);
		a.hachage = r.U32(); a.drapeaux = r.U32(); a.taille = r.U32(); a.f5 = r.U32();
		a.nEvenements = r.U16();
		for(int32 e = 0; e < a.nEvenements && r.ok; e++){
			r.U32(); r.U32();
			if(r.U32() == 1){ r.Sauter(8); r.Sauter(0x20); }
		}
		for(int j = 0; j < 4; j++) a.w[j] = r.U16();
		r.Sauter(4);
		if(version > 1.09f + 1e-6f){ r.Sauter(4); if(version > 1.11f + 1e-6f) r.Sauter(4); }
		for(int j = 0; j < 3; j++) a.v[j] = r.F32();
		anims.push_back(std::move(a));
	}
	k = r.U32();
	for(uint32 i = 0; i < k && r.ok; i++){
		CHxdGroupe g; g.nom = r.Nom(0x20); g.taille = r.U32();
		if(version >= 1.11f - 1e-6f){ r.Sauter(0x20); r.U32(); }
		groupes.push_back(std::move(g));
	}
	r.Sauter(12 * nos);
	k = r.U32(); r.Sauter(0x30 * k);
	motFinal = r.U32();
	lus = r.p;
	return r.ok;
}
