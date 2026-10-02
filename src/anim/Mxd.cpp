#include "Mxd.h"
#include <cstring>
#include <strings.h>

namespace {
struct Lecteur {
	const uint8 *b; uint32 n, p = 0; bool ok = true;
	bool Reste(uint32 k){ if(p + k > n) ok = false; return ok; }
	uint32 U32(void){ uint32 v = 0; if(Reste(4)){ memcpy(&v, b + p, 4); p += 4; } return v; }
	float F32(void){ float v = 0; if(Reste(4)){ memcpy(&v, b + p, 4); p += 4; } return v; }
	void Sauter(uint32 k){ if(Reste(k)) p += k; }
	std::string Nom(uint32 k){
		std::string s; if(!Reste(k)) return s;
		for(uint32 i = 0; i < k && b[p + i]; i++) s += (char)b[p + i];
		p += k; return s;
	}
};
}

uint32
CMxdFile::Hachage(const char *s)
{
	uint32 h = 0;
	for(; *s; s++){ char c = *s; if(c >= 'a' && c <= 'z') c -= 32; h = (h * 0x83 + (uint8)c) & 0x7fffffff; }
	return h;
}

bool
CMxdFile::Load(const uint8 *buf, uint32 n)
{
	Lecteur r{buf, n};
	uint32 k = r.U32();
	if(!r.ok || k > 10000) return false;
	for(uint32 i = 0; i < k && r.ok; i++){
		CMxdModele m;
		m.nom = r.Nom(0x40);
		m.version = r.F32();
		// FUN_006b5710 : 0x50 octets d'un bloc.
		r.U32(); r.U32();
		m.anims = r.Nom(0x40);
		r.U32();
		uint32 np = r.U32();
		if(np > 256){ r.ok = false; break; }
		for(uint32 j = 0; j < np && r.ok; j++){
			CMxdPoint p;
			for(int c = 0; c < 3; c++) p.pos[c] = r.F32();
			for(int c = 0; c < 4; c++) p.q[c] = r.F32();
			p.os = r.Nom(0x20);
			p.numeroOs = (int32)r.U32();
			p.nom = r.Nom(0x20);
			p.hachage = Hachage(p.nom.c_str());
			m.points.push_back(p);
		}
		// Les os (12 octets chacun) et un dernier mot. Toutes les entrées des
		// fichiers du jeu sont en 1,06, au-delà des deux seuils.
		m.nOs = (int32)r.U32();
		if(m.nOs < 0 || m.nOs > 256){ r.ok = false; break; }
		r.Sauter(12 * (uint32)m.nOs);
		r.U32();
		if(r.ok) modeles.push_back(m);
	}
	lus = r.p;
	return r.ok;
}

const CMxdModele *
CMxdFile::Chercher(const char *modele) const
{
	for(const CMxdModele &m : modeles){
		size_t b = m.nom.find_last_of('\\');
		const char *court = m.nom.c_str() + (b == std::string::npos ? 0 : b + 1);
		if(strcasecmp(court, modele) == 0) return &m;
	}
	return nil;
}
