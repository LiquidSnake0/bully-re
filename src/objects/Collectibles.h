// Le gestionnaire des collectibles (CCollectiblesMgr) : les nains de jardin, les transistors du clochard, les élastiques, les cartes
// Grotto & Gremlins, les citrouilles, les tombes et les artefacts, ce que le joueur ramasse une fois pour toutes.
//
// L'objet est en 0xbceb50 (sa vftable 0x901088, posée par l'initialisation statique 0x8f1cb0 ; le constructeur FUN_00426560 met
// à zéro les positions des entrées et les quatre tables de positions par numéro). CGame::Initialise (FUN_0042ee90) l'initialise
// juste après CPickups::Init (« Collectibles », FUN_00428bf0) : les octets +4 (400) et les entrées +0x194 (0x12c0) vidés, les
// comptes +0x1454 … +0x1462 à 0, puis FUN_004287a0 lit config\dat\colctbls.dat et FUN_005d9af0 lâche le rappel des scripts.
//
// Disposition (lue à l'assembleur de 0x4261f0 à 0x42637e et de 0x4288c0 à 0x428a15) :
//   +4 + 2i     l'octet « pris » de l'entrée i (FUN_004261f0)
//   +5 + 2i     l'octet « disponible » (FUN_00426200 ; 1 à la lecture du fichier, FUN_00426340 le pose par type)
//   +0x194 + 0x18 i   l'entrée : le type (+0), le HashString du nom (+4), la poignée du ramassage qui la porte (+8, -1 au départ ;
//               FUN_004262a0 la rend, FUN_004262c0 la pose), une position (+0xc … +0x14) copiée d'une table par numéro
//   +0x1454 + 2t  le nombre d'entrées du type t (FUN_004262e0) ; +0x1462 le nombre d'entrées
//   +0x1784, +0x1b14, +0x1d00, +0x1d54 : les positions par numéro des élastiques (76), des cartes (41), des transistors (7) et des
//               nains (26), trois flottants chacune ; seul le constructeur les écrit (à zéro) : l'outil les garde nulles
//   +0x1e8c     le rappel des scripts (CollectiblesOnCollectedCallback, FUN_005d9f00 sur 0xbd09dc)
#pragma once
#include "../common.h"
#include <string>

// Les types, dans l'ordre de la table de noms 0x92c7fc (FUN_005dad80 → FUN_0073ab40 : sept chaînes comparées par strcmp,
// -1 pour un nom inconnu) : « Gnome » 0x92b31c, « Radio » 0x904980, « RubberBand » 0x92b310, « GG_Card » 0x92b308,
// « Pumpkin » 0x916798, « Tombstone » 0x916780, « Artifact » 0x92b2fc.
enum eCollectible {
	COLLECTIBLE_GNOME = 0, COLLECTIBLE_RADIO, COLLECTIBLE_RUBBERBAND, COLLECTIBLE_GG_CARD, COLLECTIBLE_PUMPKIN,
	COLLECTIBLE_TOMBSTONE, COLLECTIBLE_ARTIFACT, NUM_COLLECTIBLE_TYPES
};

struct CEntreeCollectible {
	int32 type = 0;                 // +0
	uint32 hache = 0;               // +4 : HashString du nom (« radio06 » : 0x714e6af9, le +116 de son entrée inst)
	int32 ramassage = -1;           // +8 : la poignée du ramassage (FUN_00445640) qui le porte
	CVector pos;                    // +0xc : la table par numéro (nulle, voir plus haut)
};

// Ce que FUN_004288c0 passe au rappel des scripts : CB_CollectibleCollected de main.lur reçoit ces six nombres (sa fonction 27 a
// six paramètres).
struct CRappelCollectible {
	int32 type = 0, pris = 0, nombre = 0;
	uint32 hache = 0;
	int32 ramassage = -1, indice = -1;
};

class CCollectibles
{
public:
	static const int kTaille = 200;            // 400 octets de drapeaux, 0x12c0 octets d'entrées de 0x18
	uint8 pris[kTaille] = {}, disponible[kTaille] = {};
	CEntreeCollectible entrees[kTaille];
	int16 parType[NUM_COLLECTIBLE_TYPES] = {}; // +0x1454
	int16 nombre = 0;                          // +0x1462
	bool tousLesElastiques = false;            // l'octet +0x15d de DAT_00c674f0, posé quand le dernier élastique est pris

	// FUN_005dad80 : l'indice du nom dans la table des types (strcmp), -1 sinon.
	static int Type(const char *nom);
	// FUN_00428bf0 : le gestionnaire vidé (le rappel lâché par l'appelant), puis FUN_004287a0 sur le texte de colctbls.dat.
	void Initialiser(const char *texte, size_t taille);
	// FUN_004287a0 : chaque ligne non vide qui ne commence pas par « # » donne un type (sscanf « %s » → FUN_005dad80), puis
	// chaque ligne jusqu'à « end » (stricmp) dont sscanf « %s » lit un mot ajoute une entrée de ce type (FUN_00426080).
	void Lire(const char *texte, size_t taille);
	// FUN_00426080 : une entrée de plus, pas prise et disponible, son HashString, -1 en +8 ; pour les élastiques, les cartes, les
	// transistors et les nains, la position de son numéro (sscanf « %7s%2d », « %8s%2d », « %5s%2d », « %5s%2d ») ; les comptes
	// montent. Un type inconnu (-1) écrit le compte en +0x1452 dans le jeu : l'outil ne compte pas l'entrée.
	void Ajouter(int type, const char *nom);
	// FUN_00426270 : l'indice de l'entrée de ce haché, -1 (0xffff) sinon.
	int Indice(uint32 hache) const;
	bool Pris(int i) const { return i >= 0 && i < nombre && pris[i] != 0; }                 // FUN_004261f0
	bool Disponible(int i) const { return i >= 0 && i < nombre && disponible[i] != 0; }     // FUN_00426200
	// FUN_00426210, FUN_00426230, FUN_00426250 : un nain, une citrouille, une tombe (ceux qui restent des objets à casser).
	bool AuSol(int i) const;
	int16 NombreDuType(int type) const { return type >= 0 && type < NUM_COLLECTIBLE_TYPES ? parType[type] : 0; }   // FUN_004262e0
	// FUN_004262f0 : les entrées de ce type dont l'octet « pris » est posé.
	int16 PrisDuType(int type) const;
	// FUN_00426340 : l'octet « disponible » de chaque entrée du type.
	void PoserDisponible(int type, bool b);
	int32 Ramassage(int i) const { return i >= 0 && i < nombre ? entrees[i].ramassage : -1; }   // FUN_004262a0
	void PoserRamassage(int i, int32 h) { if(i >= 0 && i < nombre) entrees[i].ramassage = h; }  // FUN_004262c0
	// FUN_004288c0 : l'octet « pris » ; posé, un élastique qui complète la série pose tousLesElastiques, et `rappel` reçoit les six
	// nombres du rappel des scripts (rendu vrai : l'appelant le joue s'il en a un).
	bool Marquer(int i, bool b, CRappelCollectible *rappel);
	// FUN_00428ba0 : Marquer(i, b) pour chaque entrée du type ; les rappels sont rendus dans l'ordre.
	int ToutMarquer(int type, bool b, CRappelCollectible *rappels, int max);
};
