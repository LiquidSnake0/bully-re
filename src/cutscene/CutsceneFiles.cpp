#include "CutsceneFiles.h"
#include "../core/CdStream.h"
#include "../core/FileMgr.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

int32 CCutsceneFiles::image = -1;

namespace {

// Les fichiers sont du texte à fins de ligne CR LF, complétés de zéros jusqu'au secteur.
struct Lignes {
	const char *p, *fin;
	std::string ligne;
	Lignes(const uint8 *d, uint32 n) : p((const char*)d), fin((const char*)d + n){ while(fin > p && fin[-1] == 0) fin--; }
	bool Suivante(void){
		while(p < fin){
			const char *d = p;
			while(p < fin && *p != '\n') p++;
			const char *f = p; if(p < fin) p++;
			while(f > d && (f[-1] == '\r' || f[-1] == ' ' || f[-1] == '\t')) f--;
			while(d < f && (*d == ' ' || *d == '\t')) d++;
			if(d == f) continue;
			ligne.assign(d, f);
			return true;
		}
		return false;
	}
};

// Les n premiers nombres d'une ligne « a,b,c, » ; faux s'il en manque (sscanf du moteur rend moins que n).
bool Nombres(const std::string &s, float *v, int n){
	const char *c = s.c_str();
	for(int i = 0; i < n; i++){
		char *e; v[i] = strtof(c, &e); if(e == c) return false;
		c = e; while(*c == ',' || *c == ' ') c++;
	}
	return true;
}

}

// FUN_006c4480, « Read Object names » : les noms en minuscules dans le moteur (strlwr), les « f_ »
// déplacés en fin de tableau ; ici on garde la casse du fichier et deux listes.
bool
CCutsceneInf::Load(const uint8 *data, uint32 size)
{
	objets.clear(); faciaux.clear();
	Lignes l(data, size);
	if(!l.Suivante()) return false;
	int n = atoi(l.ligne.c_str());
	if(n < 0) return false;
	for(int i = 0; i < n; i++){
		if(!l.Suivante()) return false;
		if(strncasecmp(l.ligne.c_str(), "f_", 2) == 0) faciaux.push_back(l.ligne);
		else objets.push_back(l.ligne);
	}
	return true;
}

// « Read facial weight set » : l'objet i a son facial si un nom « f_ » + le sien est dans la liste.
bool
CCutsceneInf::AFacial(size_t i) const
{
	if(i >= objets.size()) return false;
	for(const std::string &f : faciaux)
		if(strcasecmp(f.c_str() + 2, objets[i].c_str()) == 0) return true;
	return false;
}

// « Load Subtitles » : la première ligne est comptée comme les autres par le moteur (DAT_020c5c00 lignes
// de 0x20 octets, la première donnant « 1, 1, 30.000 ») ; on la met à part.
bool
CCutsceneSub::Load(const uint8 *data, uint32 size)
{
	lignes.clear(); imagesParSeconde = 0;
	Lignes l(data, size);
	bool premiere = true;
	while(l.Suivante()){
		int a, b; char cle[256];
		if(sscanf(l.ligne.c_str(), "{%d}{%d}%255s", &a, &b, cle) != 3) return false;
		if(premiere){ imagesParSeconde = (float)atof(cle); premiere = false; continue; }
		Ligne li; li.debut = a; li.fin = b; li.cle = cle;
		lignes.push_back(li);
	}
	return !premiere;
}

bool
CCutsceneCamera::Load(const uint8 *data, uint32 size)
{
	champ.clear(); seconde.clear(); position.clear(); cible.clear(); points.clear();
	Lignes l(data, size);
	// FUN_004f8200 : « n, » puis n lignes de six nombres, puis une ligne « ; ».
	auto six = [&](std::vector<CCutsceneCle6> &piste) -> bool {
		if(!l.Suivante()) return false;
		int n = atoi(l.ligne.c_str());
		for(int i = 0; i < n; i++){
			if(!l.Suivante()) return false;
			float v[6]; if(!Nombres(l.ligne, v, 6)) return false;
			CCutsceneCle6 k; k.t = v[0]; k.valeur = v[1]; k.a = v[2]; k.b = v[3]; k.c = v[4]; k.d = v[5];
			piste.push_back(k);
		}
		return l.Suivante() && l.ligne[0] == ';';
	};
	// FUN_004f8420 : pareil avec douze nombres.
	auto douze = [&](std::vector<CCutsceneCle12> &piste) -> bool {
		if(!l.Suivante()) return false;
		int n = atoi(l.ligne.c_str());
		for(int i = 0; i < n; i++){
			if(!l.Suivante()) return false;
			float v[12]; if(!Nombres(l.ligne, v, 12)) return false;
			CCutsceneCle12 k; k.t = v[0]; k.pos = CVector(v[1], v[2], v[3]); k.entree = CVector(v[4], v[5], v[6]); k.sortie = CVector(v[7], v[8], v[9]); k.a = v[10]; k.b = v[11];
			piste.push_back(k);
		}
		return l.Suivante() && l.ligne[0] == ';';
	};
	if(!six(champ) || !six(seconde) || !douze(position) || !douze(cible)) return false;
	// FUN_004f86d0 : la cinquième section, « n, » puis n lignes « x,y,z » et « ; » ; un « ; » seul vaut zéro.
	if(!l.Suivante()) return true;
	int n = atoi(l.ligne.c_str());
	for(int i = 0; i < n; i++){
		if(!l.Suivante()) return false;
		float v[3]; if(!Nombres(l.ligne, v, 3)) return false;
		points.push_back(CVector(v[0], v[1], v[2]));
	}
	if(n > 0 && !(l.Suivante() && l.ligne[0] == ';')) return false;
	return true;
}

float
CCutsceneCamera::Duree(void) const
{
	float d = 0;
	if(!champ.empty()) d = champ.back().t;
	if(!seconde.empty() && seconde.back().t > d) d = seconde.back().t;
	if(!position.empty() && position.back().t > d) d = position.back().t;
	if(!cible.empty() && cible.back().t > d) d = cible.back().t;
	return d;
}

bool
CCutsceneFiles::Ouvrir(void)
{
	if(image < 0) image = CdStream::AddImage("Cuts\\Cuts.img");
	return image >= 0;
}

bool
CCutsceneFiles::Lire(const char *entree, std::vector<uint8> &octets)
{
	if(!Ouvrir()) return false;
	const CDirectoryEntry *d = CdStream::ms_images[image].Find(entree);
	if(d == nil) return false;
	octets.resize(d->size * CDSTREAM_SECTOR_SIZE);
	int32 fd = CFileMgr::OpenFile("Cuts\\Cuts.img", "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, octets.data(), (uint32)octets.size());
	CFileMgr::CloseFile(fd);
	return ok;
}

// FUN_006c4480 cherche « NOM.CUT » (absent du jeu), sinon « NOM.INF » ; puis « NOM.DAT » (DAT_020c5c09
// dit s'il existe) et « NOM.SUB » ; les animations par les groupes dont le nom commence par NOM.
bool
CCutsceneFiles::Charger(const char *n)
{
	if(!Ouvrir()) return false;
	nom = n;
	aObjets = aSousTitres = aCamera = false;
	animations.clear();
	std::vector<uint8> b;
	std::string f = nom + ".INF";
	if(Lire(f.c_str(), b)) aObjets = inf.Load(b.data(), (uint32)b.size());
	f = nom + ".SUB";
	if(Lire(f.c_str(), b)) aSousTitres = sub.Load(b.data(), (uint32)b.size());
	f = nom + ".DAT";
	if(Lire(f.c_str(), b)) aCamera = camera.Load(b.data(), (uint32)b.size());
	const CdImage &img = CdStream::ms_images[image];
	std::string prefixe = nom + "_";
	for(int32 e = 0; e < img.m_numEntries; e++){
		const char *en = img.m_entries[e].name;
		if(strncasecmp(en, prefixe.c_str(), prefixe.size()) != 0) continue;
		const char *ext = strrchr(en, '.');
		if(ext && (strcasecmp(ext, ".agr") == 0 || strcasecmp(ext, ".hxd") == 0)) animations.push_back(en);
	}
	return true;
}
