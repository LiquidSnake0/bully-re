// La table des projectiles en vol : 32 entrées de 0x34 octets à partir de 0xc3a038 (la boucle de FUN_004d4590 s'arrête à 0xc3a700),
// l'objet de chacune dans DAT_00c3a6b8 (FUN_004d3c80). Une entrée : +4 le tireur (0xc3a03c), +8 l'heure de sa création (0xc3a040 :
// DAT_00c1a9b4, en ms), l'octet +0x14 actif (0xc3a04c), +0x1c les dégâts (0xc3a054), +0x30 la fiche ProjectileButes (0xc3a068).
// Lu dans l'index le 07.11.2026 (FUN_004d4c40, la création ; FUN_004d4590, FUN_004d3f60, FUN_004d3dc0).
#pragma once
#include "../common.h"

struct CPlacesProjectiles {
	enum { kTaille = 32 };
	bool actif[kTaille] = {};
	uint32 creation[kTaille] = {};
	// FUN_004d4590 : la première entrée dont l'octet actif est nul (la boucle en lit quatre par tour, dans l'ordre) ; quand les 32
	// servent, FUN_004d3f60 : l'entrée active la plus vieille (la plus petite heure de création, la première à égalité : la comparaison
	// est stricte), détruite (FUN_004d3dc0(i, 0) : l'octet actif à 0, son son et son objet libérés), puis rendue ; `evincee` la reçoit.
	// FUN_004d4c40 la prend (son octet actif à 1, l'heure de création à maintenant) ; -1 n'arrive qu'avec une table vide d'actives.
	int Prendre(uint32 maintenant, int *evincee = nil) {
		if(evincee) *evincee = -1;
		int i = 0;
		while(i < kTaille && actif[i]) i++;
		if(i == kTaille){
			uint32 meilleure = 0xffffffffu;
			i = -1;
			for(int k = 0; k < kTaille; k++)
				if(actif[k] && creation[k] < meilleure){ meilleure = creation[k]; i = k; }
			if(i < 0) return -1;
			Liberer(i);
			if(evincee) *evincee = i;
		}
		actif[i] = true;
		creation[i] = maintenant;
		return i;
	}
	// FUN_004d3dc0 : l'octet actif remis à 0.
	void Liberer(int i) { if(i >= 0 && i < kTaille) actif[i] = false; }
	// FUN_004d4940 (AreaClearAllProjectiles sans argument) : FUN_004d3dc0 sur les 32.
	void Vider(void) { for(int i = 0; i < kTaille; i++) Liberer(i); }
	// La vie d'un projectile à sa création (FUN_004d4c40, lu à l'assembleur de 0x4d4d05 à 0x4d4d38 ; gardée en +0xc de l'entrée,
	// 0xc3a044) : son quatorzième argument, ou s'il est nul la vie +0x48 de la fiche ; si elle n'est pas nulle, moins le treizième, et
	// une différence nulle devient 1 (sub, cmp eax,1, jae : non signé). Nulle, le projectile vit sans fin (FUN_004d45e0) ; plus
	// petite que le treizième, la différence est négative et l'heure de création plus elle, comparée sans signe, est déjà passée.
	static int32 Vie(int32 vieTir, int32 vieFiche, int32 ecoule) {
		int32 v = vieTir != 0 ? vieTir : vieFiche;
		if(v == 0) return 0;
		v = (int32)((uint32)v - (uint32)ecoule);
		return (uint32)v < 1u ? 1 : v;
	}
	// Les deux arguments que le tir (FUN_00664710, lu à l'assembleur de 0x66496e à 0x664a0b) passe : le treizième, le temps depuis
	// l'allumage de la mèche de l'objet arme (maintenant, DAT_00c1a9b4, moins son +0x140 ; 0 sans mèche, +0x140 nul) ; le quatorzième,
	// la vie des projectiles du tireur (+0x1cc0, en secondes : PedSetProjectileLifetime, 0x5ccc80 ; 0 par le constructeur de CPed,
	// FUN_0047ffe0) × 1000 (le double 0x900138), tronquée (fistp sous le mot de contrôle | 0xc00) ; FUN_00662d30 et FUN_00663230
	// passent la même. Le treizième ne vaut que pour une vie non nulle.
	static int32 EcouleMeche(uint32 maintenant, uint32 allumage) { return allumage != 0 ? (int32)(maintenant - allumage) : 0; }
	static int32 VieDuTireur(float secondes) { return (int32)(int64_t)((double)secondes * 1000.0); }
	// FUN_006d48d0(&niveau, &genre, point) (ecx = le gestionnaire de l'eau ; lu à l'assembleur de 0x6d48d0 à 0x6d491e, le reste dans
	// l'index) : le niveau vaut d'abord _DAT_00903494 (-1e6) et le genre -1. Un point dont le z n'est pas au-dessus de 0 (fldz, fcomp
	// [z], test ah,1 : seul 0 < z passe) dans une zone visible « dehors » (FUN_004301d0(DAT_00bd1008) : l'octet de la zone dans
	// 0x901dd0, vrai pour 0, 1, 22, 31, 42, 43, 64, 65 et 66) est dans la mer : niveau 0, genre 1, vrai. Sinon les volumes d'eau du
	// gestionnaire (+0x50 entrées de 8 octets, FUN_006d6e50 le dedans, FUN_006d66b0 le niveau, le genre en +0 de l'entrée), que l'outil
	// ne lit pas.
	static bool DansLaMer(float z, bool zoneDehors, float *niveau = nullptr, int32 *genre = nullptr) {
		if(niveau) *niveau = -1000000.0f;
		if(genre) *genre = -1;
		if(0.0f < z || !zoneDehors) return false;
		if(niveau) *niveau = 0.0f;
		if(genre) *genre = 1;
		return true;
	}
	// FUN_004d45e0 (à chaque image, lu dans l'index) : le fondu de la fin de vie. Avec un objet dessiné (+0x18 de l'objet), une vie non
	// nulle et le fondu +0x88 de la fiche non nul, quand le reste (création + vie − maintenant) passe sous +0x88, l'alpha vaut reste ×
	// 255 / +0x88 (0 au moins ; FUN_005f29a0 le pose) ; 255 sinon, puis FUN_004d3c90(objet, alpha × _DAT_008ff238).
	static int32 Alpha(uint32 creation, int32 vie, uint32 maintenant, int32 fondu) {
		if(vie == 0 || fondu == 0) return 0xff;
		int32 reste = (int32)(creation - maintenant) + vie;
		if(!(reste < fondu)) return 0xff;
		int32 a = reste * 0xff / fondu;
		return a < 0 ? 0 : a;
	}
};
