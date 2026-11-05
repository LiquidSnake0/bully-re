#include "Patrouilles.h"

// FUN_004720e0, lu à l'assembleur (0x4720e0 … 0x47218e) : rien sans trajet ou si son octet +5 vaut 0xff ; la boucle sur les
// entrées compare le +8 de leur trajet au sien et s'arrête à la première égale ; sinon une entrée neuve (FUN_0045c980 sur le
// pool DAT_020c7c5c, ses champs comme le constructeur 0x471ce0), son trajet en +0, le propriétaire en +0x10, rangée en fin.
bool
CPatrouilles::Ajouter(int trajet, uint32 hachage, int32 typePatrouille, int proprietaire)
{
	if(trajet < 0 || (typePatrouille & 0xff) == 0xff) return false;
	if(Trouver(hachage) >= 0) return false;
	CEntreePatrouille e;
	e.trajet = trajet; e.hachage = hachage; e.proprietaire = proprietaire;
	entrees.push_back(e);
	return true;
}

int
CPatrouilles::Trouver(uint32 hachage) const
{
	for(int i = 0; i < (int)entrees.size(); i++) if(entrees[i].hachage == hachage) return i;
	return -1;
}

int
CPatrouilles::Lacher(CEntreePatrouille &e)
{
	int p = e.pieton;
	e.pieton = -1;
	return p;
}

// FUN_00472250 (0x472250 … 0x4722cc) : l'entrée trouvée, FUN_00471d20 sur elle, le préfet lâché s'il y en a un, +0xf à 1.
int
CPatrouilles::Arreter(uint32 hachage)
{
	int i = Trouver(hachage);
	if(i < 0) return -1;
	int p = Lacher(entrees[i]);
	entrees[i].arretee = true;
	return p;
}

void
CPatrouilles::Reprendre(uint32 hachage)
{
	int i = Trouver(hachage);
	if(i >= 0) entrees[i].arretee = false;
}

// FUN_00471fd0 (0x471fd0 … 0x472046) : rien sans trajet ni si c vaut -1 ; l'entrée trouvée reçoit a (+0x14), b (+0x18),
// c (+0x24), l'octet (+0xe), et +0xf à 0.
void
CPatrouilles::Initialiser(uint32 hachage, int32 a, int32 b, int32 c, bool discret)
{
	if(c == -1) return;
	int i = Trouver(hachage);
	if(i < 0) return;
	CEntreePatrouille &e = entrees[i];
	e.v14 = a; e.v18 = b; e.v24 = c; e.discret = discret; e.arretee = false;
}

// FUN_004721a0 : pour chaque entrée, la première de même haché (elle-même : Ajouter n'en laisse pas deux) repart.
void
CPatrouilles::ReprendreToutes(void)
{
	actives = true;
	for(CEntreePatrouille &e : entrees) entrees[Trouver(e.hachage)].arretee = false;
}

std::vector<int>
CPatrouilles::ArreterToutes(void)
{
	std::vector<int> lache;
	actives = false;
	for(size_t i = 0; i < entrees.size(); i++){ int p = Arreter(entrees[i].hachage); if(p >= 0) lache.push_back(p); }
	return lache;
}

// FUN_00472750 / FUN_00472880 : le préfet lâché (deux fois FUN_00471d20 dans le moteur, la seconde sans effet), la place
// rendue au pool (l'octet 0x80, et +0x14 du pool ramené à elle si elle est plus basse), la dernière entrée dans la case.
int
CPatrouilles::Oter(int i)
{
	int p = Lacher(entrees[i]);
	entrees[i] = entrees.back();
	entrees.pop_back();
	return p;
}

int
CPatrouilles::Retirer(uint32 hachage)
{
	int i = Trouver(hachage);
	return i < 0 ? -1 : Oter(i);
}

// FUN_00472880 : la boucle avance son index même après avoir ôté une case ; la dernière, venue à sa place, n'est pas revue.
std::vector<int>
CPatrouilles::RetirerDe(int proprietaire)
{
	std::vector<int> lache;
	for(int i = 0; i < (int)entrees.size(); i++)
		if(entrees[i].proprietaire == proprietaire){ int p = Oter(i); if(p >= 0) lache.push_back(p); }
	return lache;
}

std::vector<int>
CPatrouilles::Vider(void)
{
	std::vector<int> lache;
	for(CEntreePatrouille &e : entrees){ int p = Lacher(e); if(p >= 0) lache.push_back(p); }
	entrees.clear();
	return lache;
}
