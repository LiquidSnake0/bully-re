// Les manettes de bully.exe : 0x914 octets chacune, en tableau à partir de 0x20cecf0. Lu à l'assembleur le 09.11.2026
// (FUN_007386f0 de 0x7386f0 à 0x738720, FUN_0050c730 de 0x50c730 à 0x50c7cd).
#pragma once
#include "../common.h"

struct CManette {
	// DAT_00a14810 (seize mots de .data) : le bit de chaque bouton numérique 0 à 15 dans le mot +0x1e8. Ce sont les bits des boutons
	// de XInput (0x1000 A, 0x2000 B, 0x4000 X, 0x8000 Y, 0x100 et 0x200 les boutons de tranche, 0x10 Start, 0x20 Back, 0x40 et 0x80
	// les sticks, 1 à 8 la croix), plus 0x400 (le bouton 10) et 0x800 (le bouton 12), que XInput ne définit pas : les gâchettes,
	// que le jeu y range (supposé). Le bouton 10 est celui que lisent la visée du tir (FUN_00662a40) et l'octet +0x7e du composant de
	// visée (FUN_004d5d50 : le verrou tenu).
	static constexpr uint16 kMasques[16] = {0x4, 0x8, 0x1, 0x2, 0x20, 0x10, 0x4000, 0x1000, 0x2000, 0x8000, 0x400, 0x100, 0x800, 0x200,
	                                        0x40, 0x80};
	bool numerique = false;    // +0 : la manette numérique (les bits +0x1e8) ; à 0, le clavier et la souris par la table des touches
	float valeurs[0x19] = {};  // +0x144 : la valeur brute de chaque bouton (4 octets par bouton)
	uint16 bits = 0;           // +0x1e8 : les boutons numériques enfoncés

	// FUN_0050c730(bouton) (ecx = la manette) : la valeur d'un bouton. Numérique : les boutons 0 à 15, enfoncé si son bit de
	// DAT_00a14810 est dans +0x1e8 (test, setne). Sinon : les boutons 0 à 15 et 0x18 (24, le flottant 3,36312e-44 de Ghidra), enfoncé
	// si la valeur de sa touche (FUN_0050bee0 sur l'entrée du bouton dans la table que rend FUN_00454b80(0), ecx = 0xa14810 : +0x40 ou
	// +0xa4 selon l'octet +0x108 ; `lue` ici) dépasse 0 (fcomp contre _DAT_008ff1f0, test ah,0x41 : ni inférieure ni égale). Un bouton
	// valable rend 1 enfoncé, 0 sinon ; un autre rend sa valeur brute (+0x144 + 4 × bouton ; l'outil, hors du tableau, 0).
	float Valeur(int32 bouton, float lue = 0.0f) const {
		bool valable, enfonce = false;
		if(numerique){
			valable = bouton >= 0 && bouton <= 0xf;
			if(valable) enfonce = (kMasques[bouton] & bits) != 0;
		}else{
			valable = bouton >= 0 && (bouton <= 0xf || bouton == 0x18);
			if(valable) enfonce = lue > 0.0f;
		}
		if(valable) return enfonce ? 1.0f : 0.0f;
		return bouton >= 0 && bouton < 0x19 ? valeurs[bouton] : 0.0f;
	}
	// FUN_007386f0(bouton, joueur) : la manette du joueur 0 est celle de l'index DAT_020cece8, celle du joueur 1 celle de DAT_00bc7490,
	// un autre nombre est pris pour l'index lui-même ; la manette est 0x20cecf0 + 0x914 × l'index, puis FUN_0050c730(bouton).
	static int32 Index(int32 joueur, int32 index0, int32 index1) { return joueur == 0 ? index0 : joueur == 1 ? index1 : joueur; }
	static constexpr uint32 kTaille = 0x914;
	static constexpr uint32 kTable = 0x20cecf0;
};
