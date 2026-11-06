// Les ramassages (CPickups) : ce que le joueur ramasse au sol, les armes, les munitions, les objets et les collectibles.
//
// Le pool : 300 entrées de 0x48 octets dès 0xc0a100 (FUN_00445970, la mise à jour de chaque image, va de 0xc0a100 à 0xc0f560 ;
// FUN_00443af0 cherche une place par l'octet +0x46 de 0xc0a146). Une entrée, lue dans FUN_00443af0 (la création) et FUN_00445710
// (la mise à jour) :
//   +8 … +0x10  la position (le z reçoit le décalage +0x44 de la fiche ObjectButes du modèle)
//   +0x14       une vitesse tirée de +0x40 de la fiche (non suivie)
//   +0x18       un objet script qui attend le ramassage (FUN_005d97a0 à la prise ; non suivi)
//   +0x1c       l'objet visible (un CObject de 0x22c, FUN_004453f0 le crée quand le joueur est à 40 m)
//   +0x20       la fiche de ramassage (PickupButes : celle du script, sinon +0x88 du type, sinon DAT_00c0f560)
//   +0x24       l'échéance de la réapparition (l'octet +0x20 de la fiche, sa durée +0x24)
//   +0x28, +0x2c  0,5 (_DAT_009055a0) et 0,05 (_DAT_009053a8)
//   +0x30       le modèle ; +0x34 la poignée ; +0x3c un mot (le HashString de l'instance d'un collectible)
//   +0x40       la génération (de 1 à 0xfffe, puis 1) ; la poignée est l'indice | la génération << 16
//   +0x42       l'indice du collectible (0xffff sinon) ; +0x44 actif (1) ; +0x45 un octet (la zone de l'objet fictif)
//   +0x46       les drapeaux : bit 0 l'octet +0x18 de la fiche, bit 1 l'objet visible prêt, bit 2 « à la main » (ni collectible, ni
//               le neuvième argument ; PickupCreateXYZ le pose par son cinquième, FUN_00443e90) : FUN_00445710 ne le donne pas au
//               contact, il faut l'action du joueur ; bit 3 l'entrée prise, bit 4 collectible
//               (le sixième argument), bit 5 le neuvième
#pragma once
#include "../common.h"
#include <string>

enum {
	RAMASSAGE_FICHE = 1, RAMASSAGE_OBJET_PRET = 2, RAMASSAGE_A_LA_MAIN = 4, RAMASSAGE_PRIS = 8, RAMASSAGE_COLLECTIBLE = 0x10,
	RAMASSAGE_NEUF = 0x20
};

struct CRamassage {
	CVector pos;                    // +8
	int32 modele = -1;              // +0x30
	std::string fiche;              // +0x20 : le nom de la fiche de ramassage (l'outil ne la lit pas)
	uint32 echeance = 0;            // +0x24
	uint32 mot3c = 0;               // +0x3c
	uint16 generation = 0;          // +0x40
	int16 collectible = -1;         // +0x42
	uint8 actif = 0;                // +0x44
	uint8 octet45 = 0;              // +0x45
	uint8 drapeaux = 0;             // +0x46
	int32 script = -1;              // l'objet script qui l'a créé (FUN_005db7e0 ; dans l'outil, l'indice de son script)
	bool Pris(void) const { return (drapeaux & RAMASSAGE_PRIS) != 0; }
};

class CRamassages
{
public:
	static const int kTaille = 300;
	CRamassage entrees[kTaille];

	// FUN_00443af0(x, y, z, modèle, fiche, collectible, mot3c, octet45, neuf) : la première entrée libre (bit 3 de +0x46 ; -1 si
	// les 300 sont prises) ; le bit 1 effacé, le bit 0 de la fiche (son octet +0x18, passé en `ficheBit`), la fiche, le modèle, +0x18
	// et +0x38 à 0, actif à 1, +0x42 à 0xffff, les bits 2 à 5, la position (z + `decalageZ`, le +0x44 du type), +0x3c, +0x45 ; une
	// fiche qui réapparaît (+0x20) hors des collectibles pose l'échéance (non suivie : `echeance` reste 0) ; la génération monte
	// (0xfffe repasse à 1) ; la poignée est rendue et notée en +0x34.
	int32 Creer(const CVector &pos, int32 modele, const std::string &fiche, bool ficheBit, bool collectible, uint32 mot3c,
	            uint8 octet45, bool neuf, float decalageZ = 0.0f);
	// La poignée vérifiée comme FUN_00443e90 et ses voisines : l'indice sous 300, l'entrée prise (bit 3) et la même génération.
	CRamassage *Entree(int32 h);
	int Indice(int32 h) const;
	int32 Poignee(int i) const { return i | (int32)entrees[i].generation << 16; }
	// FUN_004447a0, réduit : l'entrée libérée (son bit 3 effacé) ; l'objet visible et l'objet script ne sont pas suivis.
	void Retirer(int i) { if(i >= 0 && i < kTaille) entrees[i].drapeaux &= (uint8)~(RAMASSAGE_PRIS | RAMASSAGE_OBJET_PRET); }
};
