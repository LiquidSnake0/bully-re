#include "Collectibles.h"
#include "../core/ActionTree.h"
#include <cstdio>
#include <cstring>
#include <strings.h>

static const char *const kTypes[NUM_COLLECTIBLE_TYPES] = { "Gnome", "Radio", "RubberBand", "GG_Card", "Pumpkin", "Tombstone", "Artifact" };

int
CCollectibles::Type(const char *nom)
{
	for(int i = 0; i < NUM_COLLECTIBLE_TYPES; i++) if(strcmp(kTypes[i], nom) == 0) return i;
	return -1;
}

// FUN_0042b830 → FUN_00429b50 : la ligne suivante d'un tampon (le pointeur, la taille, la position) : au plus 350 caractères
// (0x15e) copiés dans un tampon fixe (DAT_00bd0a08), arrêtés au zéro, à « \n » ou « \r » (« \r\n » compte pour deux), les
// caractères de contrôle et les virgules changés en espaces, les blancs de tête sautés ; la position avance de ce qui a été lu.
static const char *
LigneSuivante(const char *texte, size_t taille, size_t &pos, char (&tampon)[351])
{
	size_t reste = taille - pos;
	size_t n = reste > 0x15d ? 0x15e : reste;
	memset(tampon, 0, sizeof(tampon));
	memcpy(tampon, texte + pos, n);
	size_t lu = 0, i = 0;
	while(tampon[i] != '\0' && tampon[i] != '\n' && tampon[i] != '\r' && lu < reste){
		if((signed char)tampon[i] < ' ' || tampon[i] == ',') tampon[i] = ' ';
		lu++; i++;
	}
	if(tampon[i] == '\r' && lu < reste && tampon[i + 1] == '\n'){ tampon[i + 1] = '\0'; lu++; }
	tampon[i] = '\0';
	lu++;
	if(pos + lu <= taille) pos += lu;
	const char *p = tampon;
	while(*p != '\0' && (signed char)*p < '!') p++;
	return p;
}

void
CCollectibles::Initialiser(const char *texte, size_t taille)
{
	memset(pris, 0, sizeof(pris)); memset(disponible, 0, sizeof(disponible));
	for(CEntreeCollectible &e : entrees) e = CEntreeCollectible();
	memset(parType, 0, sizeof(parType));
	nombre = 0;
	tousLesElastiques = false;
	Lire(texte, taille);
}

void
CCollectibles::Lire(const char *texte, size_t taille)
{
	size_t pos = 0;
	char tampon[351], mot[64];
	while(pos != taille){
		const char *l = LigneSuivante(texte, taille, pos, tampon);
		if(*l == '\0' || *l == '#') continue;
		mot[0] = '\0';
		sscanf(l, "%63s", mot);
		int type = Type(mot);
		while(pos != taille){
			l = LigneSuivante(texte, taille, pos, tampon);
			if(strcasecmp(l, "end") == 0) break;
			if(sscanf(l, "%63s", mot) > 0) Ajouter(type, mot);
		}
	}
}

void
CCollectibles::Ajouter(int type, const char *nom)
{
	if(type < 0 || type >= NUM_COLLECTIBLE_TYPES || nombre >= kTaille) return;
	int i = nombre;
	pris[i] = 0; disponible[i] = 1;
	entrees[i] = CEntreeCollectible();
	entrees[i].type = type;
	entrees[i].hache = ActionHash(nom);
	entrees[i].ramassage = -1;
	// Les tables de positions par numéro ne sont écrites que par le constructeur (FUN_00426560, à zéro) : la position reste nulle.
	parType[type]++;
	nombre++;
}

int
CCollectibles::Indice(uint32 hache) const
{
	for(int i = 0; i < nombre; i++) if(entrees[i].hache == hache) return i;
	return -1;
}

bool
CCollectibles::AuSol(int i) const
{
	if(i < 0 || i >= nombre) return false;
	int t = entrees[i].type;
	return t == COLLECTIBLE_GNOME || t == COLLECTIBLE_PUMPKIN || t == COLLECTIBLE_TOMBSTONE;
}

int16
CCollectibles::PrisDuType(int type) const
{
	int16 n = 0;
	for(int i = 0; i < nombre; i++) if(entrees[i].type == type && pris[i]) n++;
	return n;
}

void
CCollectibles::PoserDisponible(int type, bool b)
{
	for(int i = 0; i < nombre; i++) if(entrees[i].type == type) disponible[i] = b ? 1 : 0;
}

bool
CCollectibles::Marquer(int i, bool b, CRappelCollectible *rappel)
{
	if(i < 0 || i >= nombre) return false;
	pris[i] = b ? 1 : 0;
	if(!b) return false;
	const CEntreeCollectible &e = entrees[i];
	if(e.type == COLLECTIBLE_RUBBERBAND && PrisDuType(COLLECTIBLE_RUBBERBAND) >= parType[COLLECTIBLE_RUBBERBAND]) tousLesElastiques = true;
	if(rappel){
		rappel->type = e.type; rappel->pris = PrisDuType(e.type); rappel->nombre = parType[e.type];
		rappel->hache = e.hache; rappel->ramassage = e.ramassage; rappel->indice = i;
	}
	return true;
}

int
CCollectibles::ToutMarquer(int type, bool b, CRappelCollectible *rappels, int max)
{
	int n = 0;
	for(int i = 0; i < nombre; i++){
		if(entrees[i].type != type) continue;
		CRappelCollectible r;
		if(Marquer(i, b, &r) && rappels && n < max) rappels[n++] = r;
	}
	return n;
}
