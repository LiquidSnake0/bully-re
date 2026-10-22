#include "ActionBank.h"
#include "CdStream.h"
#include "FileMgr.h"
#include <cstdlib>
#include <cstring>

CActionBank::CActionBank(void)
{
	racine.genre = 'b';
	CActionNode global;
	global.genre = 'b';
	global.nom = "Global";
	global.hachage = ActionHash("Global");
	racine.enfants.push_back(global);
}

// FUN_005f6c50 : un « / » de tête retiré, « .cat » écrit à la place des quatre derniers caractères ;
// les fichiers de Act.img n'ont pas de dossier, on ne garde que le nom.
std::string
CActionBank::NomCat(const char *act)
{
	std::string s = act ? act : "";
	size_t barre = s.find_last_of("/\\");
	if(barre != std::string::npos) s = s.substr(barre + 1);
	size_t point = s.rfind('.');
	if(point != std::string::npos) s.resize(point);
	return s + ".cat";
}

bool
CActionBank::EstCharge(const char *act) const
{
	std::string cat = NomCat(act);
	uint32 h = ActionHash(cat.substr(0, cat.size() - 4).c_str());
	for(uint32 x : hachages) if(x == h) return true;
	return false;
}

bool
CActionBank::Charger(const char *act)
{
	if(EstCharge(act)) return true;
	static int32 image = -1;
	if(image < 0) image = CdStream::AddImage("Act\\Act.img");
	if(image < 0) return false;
	std::string cat = NomCat(act);
	const CDirectoryEntry *d = CdStream::ms_images[image].Find(cat.c_str());
	if(d == nil) return false;
	uint32 octets = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(octets);
	int32 fd = CFileMgr::OpenFile("Act\\Act.img", "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, octets);
	CFileMgr::CloseFile(fd);
	CActionTreeFile f;
	if(ok) ok = f.Load(buf, octets);
	free(buf);
	if(!ok) return false;
	// La banque de tête du fichier (« 1_01 », « Player ») devient un enfant de /Global ; une banque du
	// même nom déjà là reçoit ses enfants (vtable +0x18 de la racine).
	CActionNode *cible = nil;
	for(CActionNode &e : Global()->enfants) if(e.hachage == f.racine.hachage){ cible = &e; break; }
	if(cible == nil) Global()->enfants.push_back(std::move(f.racine));
	else for(CActionNode &e : f.racine.enfants) cible->enfants.push_back(std::move(e));
	fichiers.push_back(cat);
	hachages.push_back(ActionHash(cat.substr(0, cat.size() - 4).c_str()));
	return true;
}

// FUN_005f59b0 : la pile des nœuds traversés tient lieu de pointeur de parent pour « .. ».
const CActionNode *
CActionBank::Trouver(const char *chemin) const
{
	if(chemin == nil || (*chemin != '/' && *chemin != '\\')) return nil;
	std::vector<const CActionNode*> pile(1, &racine);
	const char *p = chemin + 1;
	while(*p){
		const char *e = p;
		while(*e && *e != '/' && *e != '\\') e++;
		std::string segment(p, e);
		p = *e ? e + 1 : e;
		if(segment.empty()) continue;
		if(segment == ".."){ if(pile.size() > 1) pile.pop_back(); continue; }
		uint32 h = ActionHash(segment.c_str());
		const CActionNode *courant = pile.back(), *suivant = nil;
		for(const CActionNode &x : courant->enfants) if(x.hachage == h){ suivant = &x; break; }
		if(suivant == nil) return nil;
		pile.push_back(suivant);
	}
	return pile.back();
}

static bool
Contient(const CActionNode &a, const CActionNode *n)
{
	for(const CActionNode &e : a.enfants) if(&e == n || Contient(e, n)) return true;
	return false;
}

bool
CActionBank::Joue(const CActionNode *courant, const char *chemin, bool sous) const
{
	const CActionNode *n = Trouver(chemin);
	if(n == nil || courant == nil) return false;
	if(courant == n) return true;
	return sous && Contient(*n, courant);   // remonter les parents de `courant` jusqu'à `n` revient à le chercher sous `n`
}

bool
CActionBank::TrouverOuCharger(const char *chemin, const char *act)
{
	if(Trouver(chemin)) return true;
	if(act == nil || *act == 0 || EstCharge(act)) return false;
	if(!Charger(act)) return false;
	return Trouver(chemin) != nil;
}

const CActionNode *
CActionBank::Choisir(const CActionNode *depuis, Conditions conditions, void *contexte) const
{
	if(depuis == nil) return nil;
	if(!depuis->conditions.empty() && (conditions == nil || !conditions(*depuis, contexte))) return nil;
	switch(depuis->genre){
	case 'n': case 'l':
		return depuis;                       // FUN_005f5500
	case 'b':
		for(const CActionNode &e : depuis->enfants){   // FUN_005f5490 : le premier enfant qui aboutit
			const CActionNode *r = Choisir(&e, conditions, contexte);
			if(r) return r;
		}
		return nil;
	default:
		return nil;
	}
}
