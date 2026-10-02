#include "ActionTree.h"
#include <cctype>

uint32
ActionHash(const char *s)
{
	uint32 h = 0;
	for(; *s; s++) h = h * 0x83 + (uint32)(uint8)toupper((uint8)*s);
	return h & 0x7fffffff;
}

std::string
CActionTreeFile::Chaine(uint32 off) const
{
	std::string s;
	for(uint32 i = chaines + off; i < n && b[i]; i++) s += (char)b[i];
	return s;
}

bool
CActionTreeFile::Table(std::vector<CActionRenvoi> &t)
{
	if(p + 4 > n) return false;
	uint32 k = Mot(p); p += 4;
	for(uint32 i = 0; i < k; i++){
		if(p + 6 > n) return false;
		CActionRenvoi r; r.valeur = Mot(p);
		uint16 m; memcpy(&m, b + p + 4, 2); p += 6;
		if(p + 4u * m > n) return false;
		for(uint16 j = 0; j < m; j++){ r.decalages.push_back(Mot(p)); p += 4; }
		t.push_back(std::move(r));
	}
	return true;
}

bool
CActionTreeFile::Noeud(CActionNode &x, int prof)
{
	if(prof > 64 || p + 5 > n) return false;
	uint32 v = Mot(p); p += 4;
	if(v & 0x80000000) x.hachage = v & 0x7fffffff;
	else{ x.nom = Chaine(v); x.hachage = ActionHash(x.nom.c_str()); }
	uint8 c = b[p++];
	if(p + 4u * c > n) return false;
	for(uint8 i = 0; i < c; i++){ x.conditions.push_back((int32)Mot(p)); p += 4; }
	if(x.genre == 'n' || x.genre == 'l'){
		if(p >= n) return false;
		uint8 m = b[p++];
		if(p + 4u * m > n) return false;
		for(uint8 i = 0; i < m; i++){ x.pistes.push_back((int32)Mot(p)); p += 4; }
	}
	return Enfants(x, prof + 1);
}

bool
CActionTreeFile::Enfants(CActionNode &x, int prof)
{
	if(p + 2 > n) return false;
	uint16 k; memcpy(&k, b + p, 2); p += 2;
	for(uint16 i = 0; i < k; i++){
		if(p >= n) return false;
		CActionNode e; e.genre = (char)b[p++];
		switch(e.genre){
		case 'b': lus[0]++; if(!Noeud(e, prof)) return false; break;
		case 'n': lus[1]++; if(!Noeud(e, prof)) return false; break;
		case 'l': lus[3]++; if(!Noeud(e, prof)) return false; break;
		case 'i': case 'r':
			if(p + 8 > n) return false;
			lus[2]++; e.nom = Chaine(Mot(p)); e.cible = Chaine(Mot(p + 4)); e.hachage = ActionHash(e.nom.c_str()); p += 8;
			break;
		default: return false;
		}
		x.enfants.push_back(std::move(e));
	}
	return true;
}

bool
CActionTreeFile::Load(const uint8 *buf, uint32 taille_)
{
	b = buf; n = taille_;
	if(n < 0x20) return false;
	taille = Mot(0); finEntete = Mot(4); chaines = Mot(8); finArbre = Mot(12);
	nBanques = (int32)Mot(16); nJouables = (int32)Mot(20); nReferences = (int32)Mot(24); nFeuilles = (int32)Mot(28);
	if(taille > n || chaines > taille || finArbre > finEntete) return false;
	p = 0x20;
	if(!Table(renvois) || !Table(renvois2)) return false;
	if(p >= n || b[p] != 'b') return false;
	p++;
	racine.genre = 'b';
	if(!Noeud(racine, 0)) return false;
	return p == finArbre;
}

uint32
CActionTreeFile::TypeCondition(int32 decalage) const
{
	uint32 at = finEntete + (uint32)decalage;
	return at + 4 <= n ? Mot(at) & 0x7fffffff : 0;
}
