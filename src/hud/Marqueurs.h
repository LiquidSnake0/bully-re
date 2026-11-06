// Les marqueurs du radar (les « blips ») : les flèches et les points que les missions posent sur la carte (AddBlipForChar,
// BlipAddPoint, BlipRemove). Ils sont 300 traces de 0x30 octets dans CRadar (l'objet DAT_00c674cc, dès +0x210) ; lues dans l'index et à
// l'assembleur (FUN_0054a460, FUN_0054a2c0, FUN_0054a7f0, FUN_0054a740, FUN_00548aa0, FUN_00548be0) :
//   +0x210  l'entité suivie (sa poignée de pool), ou 0 pour un point
//   +0x214 … +0x21c  la position d'un point
//   +0x224  la génération (de 1 à 0xfffd, puis 1) ; la poignée est la génération << 12 | l'indice
//   +0x228  l'heure + 2,5 s (0x9c4 ms) ; +0x22c 0,5 (_DAT_009055a0)
//   +0x230  le genre de la trace (2, 6 et 0xd : un piéton ; 4 : un point ; 1, 0xc : un véhicule ; 3, 5, 9, 10 : un objet ; 7)
//   +0x234  sa couleur ; +0x238 son icône ; +0x23c un mot à -1 ; +0x23e l'affichage (1, puis 5 pour les scripts : FUN_00548aa0)
//   +0x23f  les drapeaux : les bits 0 et 1 pris, le bit 2 au dernier argument, le bit 4 pour un point
#pragma once
#include "../common.h"

struct CTraceRadar {
	int32 entite = 0;           // +0x210
	CVector pos;                // +0x214
	int32 generation = 0;       // +0x224
	uint32 echeance = 0;        // +0x228
	int32 genre = 0;            // +0x230
	int32 couleur = 0;          // +0x234
	int32 icone = -1;           // +0x238
	uint8 affichage = 0;        // +0x23e
	uint8 drapeaux = 0;         // +0x23f
	bool Prise(void) const { return (drapeaux & 2) != 0; }
};

class CMarqueurs
{
public:
	static const int kTaille = 300;
	CTraceRadar traces[kTaille];

	// FUN_0054a460(genre, entité, couleur RVBA, couleur, icône, b) : les icônes 2, 0x11 et 0x1b ne vont qu'à un piéton (les genres 2,
	// 0xd et 6) ; la première trace libre (le bit 1 de +0x23f), -1 sinon ; le genre, 0,5 en +0x22c, l'entité, la couleur, les bits 0 et
	// 1 et le bit 2 à b, l'affichage à 1, l'icône, l'heure + 2,5 s, +0x23c à -1 ; la génération monte. Rend la poignée.
	int32 AjouterEntite(int32 genre, int32 entite, int32 couleur, int32 icone, bool b, uint32 maintenant);
	// FUN_0054a2c0(genre, x, y, z, couleur RVBA, couleur, icône, b) : rien (-1) pour le point (0, 0, 0) ; la position, l'entité 0, le
	// bit 3 effacé et le bit 4 posé. Rend la poignée.
	int32 AjouterPoint(int32 genre, const CVector &pos, int32 couleur, int32 icone, bool b, uint32 maintenant);
	// La trace d'une poignée : l'indice (poignée & 0xf0000fff) et la génération doivent s'accorder ; -1 sinon.
	int Indice(int32 h) const;
	// FUN_0054a7f0 : la trace d'une poignée valable rendue (les bits 1, 3 et 4 effacés, le genre, la couleur à 0, l'icône et +0x23c à
	// -1) ; FUN_00548be0 efface le marqueur de l'entité (son +0x54) et FUN_005dbd00 prévient l'objet script : non suivis.
	void Retirer(int32 h);
	// FUN_0054a740(genre, entité) : chaque trace de ce genre et de cette entité est rendue comme par FUN_0054a7f0 (pour un piéton suivi
	// sous les couleurs 2, 4 et 5, FUN_00528660 sur lui ou son véhicule : non suivi). Rend le nombre de traces rendues.
	int RetirerEntite(int32 genre, int32 entite);
	// FUN_00548aa0(poignée, octet) : l'affichage +0x23e d'une trace valable.
	void PoserAffichage(int32 h, uint8 octet) { int i = Indice(h); if(i >= 0) traces[i].affichage = octet; }
};
