#include "ObjetsAnimes.h"
#include "../core/ActionTree.h"
#include <cstring>

CFicheObjetAnime
CFicheObjetAnime::Lire(const CActionTrack &t, const std::map<uint16, std::string> &chaines)
{
	CFicheObjetAnime f;   // FUN_00729b70 : les valeurs par défaut
	f.lue = true;
	for(const auto &c : chaines){
		if(c.first == 104) f.racine = c.second;
		else if(c.first == 108) f.fichier = c.second;
		else if(c.first == 112) f.depart = c.second;
	}
	for(int s = 0; s < 4; s++) if(const CActionAttribut *a = t.Champ((uint16)(116 + s))) f.saisons[s] = a->valeur[0] != 0;
	if(const CActionAttribut *a = t.Champ(120)) if(a->taille >= 4) memcpy(&f.distance, a->valeur, 4);
	if(const CActionAttribut *a = t.Champ(124)) f.pnj = a->valeur[0] != 0;
	if(const CActionAttribut *a = t.Champ(125)) f.garcons = a->valeur[0] != 0;
	if(const CActionAttribut *a = t.Champ(126)) f.filles = a->valeur[0] != 0;
	f.genre = t.Mot(128) & 0x7fffffff;
	return f;
}

// FUN_0044cd20 (lu dans l'index) : l'objet (FUN_004eb030(0x3b8), FUN_004ed9d0(modèle, déclencheur)), +0x11c au quatrième
// argument (0), +0x111 la zone, l'octet +0x3a8 et le mot +0x3ac au dernier (1), le modèle posé par l'emplacement +0x24, la
// matrice (FUN_00412920), FUN_00443950 ; puis, hors de la case 0, le +0x1c du déclencheur et le drapeau 13 (FUN_004ea300(0xd,
// bit 3 de +0x3f)). La collision (FUN_0056efa0, FUN_004142c0), FUN_005b2830, FUN_004ef2b0 et le chemin des piétons
// (FUN_005eaf00) ne sont pas suivis.
int
CObjetsAnimes::Creer(const Declencheur &d)
{
	size_t i = 0;
	while(i < objets.size() && !objets[i].libre) i++;
	if(i == objets.size()) objets.emplace_back();
	CObjetAnime &o = objets[i];
	uint16 g = o.generation;
	o = CObjetAnime();
	o.libre = false;
	o.generation = (uint16)(g + 1);
	o.modele = d.modele;
	o.declencheur = d.handle;
	o.pos = d.pos;
	o.lacet = d.lacet;
	o.zone = (int8)d.zone;
	o.octet3a8 = 1;
	o.drapeaux[13] = d.porte ? 1 : 0;
	o.fichier = d.fiche ? d.fiche->fichier : std::string();
	return (int)i;
}

std::vector<int>
CObjetsAnimes::CreerPourZone(int zone, const std::vector<Declencheur> &declencheurs, int saison)
{
	std::vector<int> crees;
	for(const Declencheur &d : declencheurs){
		if(d.handle < 1) continue;                                  // la boucle part de la case 1
		if(ObjetDe(d.handle) >= 0 || d.mission || d.modele == -1) continue;
		if(d.zone != zone || d.emplacement == 5) continue;
		if(d.fiche){
			// +0x74 avec la saison 0, +0x75 avec 1, +0x77 avec 3, +0x76 avec 2 (dans cet ordre, 0x4165e8 … 0x416633)
			bool s = (d.fiche->saisons[0] && saison == 0) || (d.fiche->saisons[1] && saison == 1) ||
			         (d.fiche->saisons[3] && saison == 3) || (d.fiche->saisons[2] && saison == 2);
			if(!s) continue;
		}
		crees.push_back(Creer(d));
	}
	return crees;
}

// FUN_00416490 (lu dans l'index) : le pool des objets parcouru du dernier au premier (DAT_00c0f608 : le nombre +8, les octets
// d'état +4, la taille d'une case +0xc) ; un objet dont le déclencheur (+0x23c) n'est pas la case 0, n'est pas propre à une
// mission, n'est pas de la zone `zone` (+0x3c du déclencheur) ni de l'emplacement 5 : FUN_00466650 s'il a un +0x18,
// l'emplacement +0xc, FUN_0045dc20, FUN_0045e540, le destructeur, et le +0x1c du déclencheur à 0.
std::vector<int>
CObjetsAnimes::RetirerHorsZone(int zone, const std::vector<Declencheur> &declencheurs)
{
	std::vector<int> touches;
	for(int i = (int)objets.size() - 1; i >= 0; i--){
		CObjetAnime &o = objets[i];
		if(o.libre || o.declencheur < 1) continue;
		const Declencheur *d = nil;
		for(const Declencheur &e : declencheurs) if(e.handle == o.declencheur){ d = &e; break; }
		if(d == nil || d->mission || d->zone == zone || d->emplacement == 5) continue;
		touches.push_back(o.declencheur);
		o.libre = true;
		o.declencheur = -1;
	}
	return touches;
}

void
CObjetsAnimes::RetirerDuDeclencheur(int declencheur)
{
	for(CObjetAnime &o : objets) if(!o.libre && o.declencheur == declencheur){ o.libre = true; o.declencheur = -1; }
}

int
CObjetsAnimes::ObjetDe(int declencheur) const
{
	if(declencheur < 0) return -1;
	for(size_t i = 0; i < objets.size(); i++) if(!objets[i].libre && objets[i].declencheur == declencheur) return (int)i;
	return -1;
}

void
CObjetsAnimes::PoserDrapeau(int i, int n, bool v)
{
	CObjetAnime *o = Objet(i);
	if(o == nil || n < 0 || n >= OBJET_ANIME_DRAPEAUX) return;
	o->drapeaux[n] = v ? 1 : 0;
}

bool
CObjetsAnimes::Drapeau(int i, int n) const
{
	const CObjetAnime *o = Objet(i);
	return o && n >= 0 && n < OBJET_ANIME_DRAPEAUX && o->drapeaux[n] != 0;
}
