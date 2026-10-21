#include "SpeechBank.h"
#include "../core/FileMgr.h"
#include <cstdlib>
#include <cstring>

static uint32 Mot(const uint8 *p){ uint32 v; memcpy(&v, p, 4); return v; }

bool
CSpeechBank::Charger(void)
{
	evenements.clear(); echantillons.clear();
	static uint8 db[0x30000];
	int32 n = CFileMgr::LoadFile("audio\\CONFIG\\SpeechDB.bin", db, sizeof(db), "rb");
	if(n < 4) return false;
	uint32 nev = Mot(db), p = 4;
	for(uint32 i = 0; i < nev; i++){
		if(p + 8 > (uint32)n) return false;
		uint16 c; memcpy(&c, db + p + 2, 2); p += 8;
		if(p + 4u * c > (uint32)n) return false;
		std::vector<CSpeechItem> e;
		for(uint16 k = 0; k < c; k++, p += 4){
			CSpeechItem it; it.voix = db[p]; it.ligne = db[p + 1]; memcpy(&it.echantillon, db + p + 2, 2);
			e.push_back(it);
		}
		evenements.push_back(e);
	}
	int32 fd = CFileMgr::OpenFile("audio\\PLAYLIST\\Speech.bin", "rb", 0);
	if(fd == 0) return false;
	uint8 tete[8];
	bool ok = CFileMgr::ReadExact(fd, tete, 8) && memcmp(tete, "Hash", 4) == 0;
	if(ok){
		uint32 m = Mot(tete + 4);
		std::vector<uint8> t(12u * m);
		ok = CFileMgr::ReadExact(fd, t.data(), (int32)t.size());
		for(uint32 k = 0; ok && k < m; k++) echantillons.push_back({Mot(&t[12 * k]), Mot(&t[12 * k + 4]), Mot(&t[12 * k + 8])});
	}
	CFileMgr::CloseFile(fd);
	return ok && p == (uint32)n;
}

std::vector<int>
CSpeechBank::Candidats(int evenement, int modele, int ligne) const
{
	std::vector<int> r;
	if(evenement < 0 || evenement >= (int)evenements.size()) return r;
	for(const CSpeechItem &it : evenements[evenement])
		if(it.voix == (uint8)modele && (ligne & 0x7ff) == it.ligne) r.push_back(it.echantillon);
	return r;
}

int
CSpeechBank::DureeMs(int echantillon) const
{
	if(echantillon < 0 || echantillon >= (int)echantillons.size()) return -1;
	int32 fd = CFileMgr::OpenFile("audio\\PLAYLIST\\Speech.bin", "rb", 0);
	if(fd == 0) return -1;
	uint8 w[0x100];
	CFileMgr::Seek(fd, (int32)(echantillons[echantillon].position + 0x800), 0);
	bool ok = CFileMgr::ReadExact(fd, w, sizeof(w));
	CFileMgr::CloseFile(fd);
	if(!ok || memcmp(w, "WBND", 4) != 0) return -1;
	// En-tête XACT : signature, version, version d'en-tête, puis cinq segments (position, longueur) ; le
	// segment 1 est la table des ondes, 24 octets par onde : durée et drapeaux, format, région jouée…
	uint32 entree = Mot(w + 20);
	if(entree + 8 > sizeof(w)) return -1;
	uint32 duree = Mot(w + entree) >> 4, format = Mot(w + entree + 4);
	uint32 frequence = (format >> 5) & 0x3ffff;
	if(frequence == 0) return -1;
	return (int)((unsigned long long)duree * 1000 / frequence);
}

uint32
CSpeechBank::Cle(const char *nom)
{
	uint32 u = 0;
	char c0 = *nom;
	if(c0 == '"') nom++;
	for(char c; (c = *nom) != 0 && !(c0 == '"' && c == '"'); nom++){
		if((uint8)(c - 'A') < 26) c += ' ';
		else if(c == '\\') c = '/';
		u = (uint32)((int32)(signed char)c + u) * 0x401;
		u ^= u >> 6;
	}
	return ((u * 9) >> 11 ^ u * 9) * 0x8001;
}
