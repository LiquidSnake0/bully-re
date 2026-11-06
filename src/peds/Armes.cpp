#include "Armes.h"

int
CInventaireArmes::Trouver(int ped, int32 arme) const
{
	for(int i = 0; i < kTaille; i++)
		if(!libre[i] && entrees[i].arme == arme && entrees[i].proprietaire == ped) return i;
	return -1;
}

// FUN_0045c980, lu à l'assembleur (0x45c980 … 0x45ca01) : depuis +0x14, la première place à l'octet 0x80 ; au bout du
// pool, un seul retour à 0 ; la place prise perd l'octet 0x80, son compteur (les 7 bits bas) avance, ses 0x28 octets à 0.
int
CInventaireArmes::Prendre(void)
{
	bool tour = false;
	int i = suivante;
	if(i >= kTaille) i = 0;
	while(!libre[i]){
		if(++i >= kTaille){
			if(tour) return -1;
			i = 0; tour = true;
		}
	}
	suivante = i;
	libre[i] = false;
	entrees[i] = CEntreeInventaire();
	entrees[i].arme = 0;
	return i;
}

void
CInventaireArmes::Liberer(int i)
{
	if(i < 0 || i >= kTaille) return;
	libre[i] = true;
	if(i < suivante) suivante = i;
}

// FUN_0045c890, lu à l'assembleur (0x45c890 … 0x45c8ea) : n en réel, 0 s'il est sous 0 (fcom contre fldz), le maximum
// s'il le dépasse, puis l'entier (_ftol, 0x85c6f0) dans le mot +0.
void
CInventaireArmes::Poser(const CMondeArmes &m, int i, int32 n)
{
	if(i < 0 || i >= kTaille) return;
	float v = (float)n, max = (float)m.Type(entrees[i].arme).maximum;
	if(v < 0.0f) v = 0.0f;
	else if(max < v) v = max;
	entrees[i].munitions = (int16)(int32)v;
}

bool
CInventaireArmes::Ajouter(const CMondeArmes &m, int ped, int32 arme, int32 n)
{
	const CTypeArme &t = m.Type(arme);
	if(t.inventaire == 0 && t.genre != 1) return false;
	int i = Prendre();
	if(i < 0) return false;
	entrees[i].arme = (int16)arme;
	entrees[i].munitions = 0;
	entrees[i].proprietaire = ped;
	Poser(m, i, n);
	return true;
}

void
CInventaireArmes::Donner(const CMondeArmes &m, int ped, int32 arme, bool force)
{
	const CTypeArme &t = m.Type(arme);
	if(Trouver(ped, arme) == -1 || force) Ajouter(m, ped, arme, 1);
	int32 munition = t.munition;
	if(munition > 0){
		int32 n = m.Type(munition).maximum;
		int i = Trouver(ped, munition);
		if(i == -1) Ajouter(m, ped, munition, n);
		else Poser(m, i, n);
	}
}

void
CInventaireArmes::AjouterMunitions(const CMondeArmes &m, int ped, int32 arme, int32 n)
{
	int i = Trouver(ped, arme);
	if(i == -1){ Ajouter(m, ped, arme, n); return; }
	Poser(m, i, entrees[i].munitions + n);
}

int32
CInventaireArmes::Munitions(const CMondeArmes &m, int ped, int32 arme) const
{
	if(arme == m.sansFin[0] || arme == m.sansFin[1] || arme == m.sansFin[2]) return 1;
	int i = Trouver(ped, arme);
	return i == -1 ? 0 : entrees[i].munitions;
}

void
CInventaireArmes::Changer(const CMondeArmes &m, int ped, int32 arme, int32 n)
{
	if(arme == -1) return;
	int i = Trouver(ped, arme);
	if(i == -1){ Ajouter(m, ped, arme, n); return; }
	if(m.munitionsSansFin && n < 0) return;
	Poser(m, i, entrees[i].munitions + n);
	if(entrees[i].munitions < 1) Liberer(i);
}

void
CInventaireArmes::Retirer(const CMondeArmes &m, int ped, int32 arme, int32 n, bool joueur, int32 rbandball)
{
	int i = Trouver(ped, arme);
	if(i == -1 || (m.munitionsSansFin && n >= 1)) return;
	if(m.Type(arme).maximum == -1) return;
	Poser(m, i, entrees[i].munitions - n);
	if(joueur && arme == rbandball) return;
	if(entrees[i].munitions < 1) Liberer(i);
}

void
CInventaireArmes::PoserDrapeau(int ped, int32 arme, int n, int32 valeur)
{
	int i = Trouver(ped, arme);
	if(i >= 0 && n >= 0 && n < 8) entrees[i].drapeaux[n] = valeur;
}

bool
CInventaireArmes::LireDrapeau(int ped, int32 arme, int n, bool &trouve) const
{
	int i = Trouver(ped, arme);
	trouve = i >= 0;
	return trouve && n >= 0 && n < 8 && entrees[i].drapeaux[n] != 0;
}

void
CInventaireArmes::Vider(int ped)
{
	for(int i = 0; i < kTaille; i++) if(!libre[i] && entrees[i].proprietaire == ped) Liberer(i);
}

int
CInventaireArmes::PoserMunitions(const CMondeArmes &m, int ped, int32 arme, int32 n)
{
	int i = Trouver(ped, arme);
	if(i == -1) return -1;
	Poser(m, i, n);
	if(entrees[i].munitions < 1) Liberer(i);
	return i;
}

int
CObjetsJoueur::Case(int32 modele) const
{
	if((uint32)modele <= 0x4f) return modele;
	uint32 i = (uint32)(modele - premier);
	return i <= 0x4f ? (int)i : 0;
}

// Le compte borné à [0, le maximum du type] : fcom contre 0 puis contre le maximum (0x541bb2 … 0x541bd6), puis l'arrondi (0x85c6f0).
static int32
Borner(const CMondeArmes &m, int32 modele, int32 v)
{
	int32 max = m.Type(modele).maximum;
	float f = (float)v;
	if(f < 0.0f) f = 0.0f;
	else if((float)max < f) f = (float)max;
	return (int32)f;
}

int32
CObjetsJoueur::Poser(const CMondeArmes &m, int32 modele, int32 n)
{
	int i = Case(modele);
	int32 avant = nombres[i];
	nombres[i] = Borner(m, modele, n);
	return avant;
}

bool
CObjetsJoueur::Ajouter(const CMondeArmes &m, int32 modele, int32 n)
{
	int i = Case(modele);
	if(premier + i == bigGulp) return false;
	nombres[i] = Borner(m, modele, nombres[i] + n);
	return true;
}
