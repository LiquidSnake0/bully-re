// La garde-robe du joueur : le gestionnaire des vêtements de bully.exe en 0x20c5f70 (l'ecx de toutes les fonctions
// Clothing… des scripts). Il tient 35 tenues (0x23) définies par les scripts, de 0x54 octets chacune à partir de +0xc10 :
// +0 le nom haché (HashString), +4 le second nom (« CLT_Undies »…), six pièces de 12 octets (+8 … +0x4c : le nom haché
// de la pièce et sa place, FUN_006cc1c0), et l'octet +0x50 (bit 0 : la place est prise ; bit 1 : le dernier booléen de
// ClothingDefineOutfit ; bit 4 posé). La tenue portée est l'indice +0x178c (0x20c76fc), -1 pour aucune (FUN_006cbb40,
// l'initialisation). Une tenue ne se définit que la garde-robe ouverte (DAT_020c5f68, l'objet que ClothingOverlay(true)
// crée, FUN_006cb3f0, et que ClothingOverlay(false) détruit, FUN_006cb4c0) : gamemain définit ses 30 tenues entre les deux.
//
// Ce qui n'est pas suivi : les pièces elles-mêmes (le modèle du joueur, FUN_006cadd0), l'inventaire des tenues données
// (ClothingGivePlayerOutfit, FUN_006cb190 / FUN_006cb560), la coiffure, le rendu.
#pragma once
#include "../common.h"

enum { GARDE_ROBE_TENUES = 0x23, GARDE_ROBE_PIECES = 6 };

struct CTenue {
	uint32 nom = 0, second = 0;           // +0, +4
	uint32 pieces[GARDE_ROBE_PIECES] = {};
	uint8 drapeaux = 0;                   // +0x50
};

class CClothingManager
{
public:
	CClothingManager(void);
	// ClothingOverlay(oui) (0x5baf70) : FUN_006cb3f0 ouvre la garde-robe, FUN_006cb4c0 la ferme.
	void Ouvrir(bool oui) { ouverte = oui; }
	bool Ouverte(void) const { return ouverte; }
	// FUN_006cbf70 (ClothingDefineOutfit, 0x5ba900 : huit noms et deux booléens) : rien si la garde-robe est fermée ; sinon
	// la première place libre (bit 0 de +0x50 nul), s'il en reste une parmi les 35. Rend l'indice, -1 si rien.
	int Definir(uint32 nom, uint32 second, const uint32 pieces[GARDE_ROBE_PIECES], bool dernier);
	// FUN_006cb5c0 (ClothingSetPlayerOutfit, 0x5baa40 : le nom, ou un nombre, et un booléen facultatif) : la première des
	// 35 places dont le nom est `nom` ; ses pièces passent sur le joueur, et la tenue portée devient son indice, ou -1 si
	// `pieceAPiece` (les pièces sans la tenue). Une tenue inconnue ne change rien. Vrai si elle a été trouvée.
	bool Mettre(uint32 nom, bool pieceAPiece);
	// FUN_006ca890 (ClothingIsWearingOutfit 0x5bac40, et la condition PedIsInOutfit, FUN_005fbf70) : la tenue portée a ce nom.
	bool Porte(uint32 nom) const;
	// ClothingIsWearingAnyOutfit (0x5bac80) : une tenue est portée.
	bool PorteUneTenue(void) const { return portee != -1; }
	int Portee(void) const { return portee; }
	const CTenue &Tenue(int i) const { return tenues[i]; }
private:
	CTenue tenues[GARDE_ROBE_TENUES];
	int portee = -1;                     // +0x178c
	bool ouverte = false;                // DAT_020c5f68 non nul
};
