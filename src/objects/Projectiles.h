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
	bool explose[kTaille] = {};   // l'octet +0x16 (0xc3a04e) : FUN_004d37d0 a fait son explosion
	bool auSol[kTaille] = {};     // l'octet +0x18 (0xc3a050) : FUN_004d3860 a vu le premier contact au sol
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
		explose[i] = false; auSol[i] = false;   // FUN_004d4c40, de 0x4d5564 à 0x4d556c : +0x14 à 1, +0x16 et +0x18 à 0
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
	// FUN_004d37d0 (lue dans l'index ; le choc de l'explosion, appelée par FUN_004d3860 et par la chute dans l'eau de FUN_004d45e0) :
	// une seule fois par projectile (l'octet +0x16 de l'entrée) ; la première fois, avec un rayon +0x94 de la fiche au-dessus de 0,
	// FUN_004d2b40 pose l'explosion (son point et sa normale, la fiche, le tireur +4 et l'objet du projectile), puis, avec l'octet +0x40
	// de la fiche, le stimulus +0x44 naît du tireur (PedCreateStimulus(+0x44, tireur, 0, 0, -1, -1)). Rend vrai la première fois.
	bool Exploser(int i) {
		if(i < 0 || i >= kTaille || explose[i]) return false;
		explose[i] = true;
		return true;
	}
};

// Le choc d'un projectile (FUN_004d3860, lue à l'assembleur de 0x4d3860 à 0x4d3c14 ; appelée par le contact du CPhysical, FUN_0046ad90,
// avec le projectile, l'entité touchée, le point de contact et un octet « fort »). Dans l'ordre :
//  1. avec l'octet +0x40 de la fiche, le stimulus +0x44 naît du projectile (PedCreateStimulus(+0x44, projectile, 0, 0, -1, -1)) ;
//  2. l'explosion (Explose) : FUN_004d37d0 au point du contact relevé de 0,05 (_DAT_009068b8, la somme par FUN_00412850), avec la
//     normale (+0x10 du contact) et la surface (l'octet +0x22) ; puis le stimulus 2 du tireur (StimulusDeLObjet) ;
//  3. le compteur d'impacts, les trois bits bas de +0x23c du projectile : remis à 0 pour RBandBall (DAT_00bf432c) ; puis l'effet
//     d'impact (Effet), joué par FUN_0059ba80 (l'effet de FUN_00595a30, le contact, sa surface, +0xc4 de la fiche, le compteur) ;
//  4. le premier contact au sol (PremierSol) : FUN_004d3370 (une animation de l'objet du projectile, non suivie) et l'octet +0x18.
struct CChocProjectile {
	// L'explosion au contact, par les octets +0x8c, +0x8d et +0x8e de la fiche (de 0x4d38ab à 0x4d3964). Aucun : rien. +0x8c : toujours.
	// Sinon, avec +0x8e : sur un piéton (le genre 3 de l'entité, +0x108 & 7), ou sur un véhicule (genre 2) dont +0x46c vaut 1 et dont
	// la place 0 (+0x1c4, FUN_004ce410) est occupée ; le +0x8d ne compte alors plus. Sinon (+0x8d seul) : sur une normale dont le z
	// n'est pas sous 0,5 (fld _DAT_009055a0, fcomp, test ah,0x41) et hors d'un piéton.
	static bool Explose(bool partout, bool auSol, bool surPieton, float normaleZ, int genre, bool vehiculeOccupe) {
		if(partout) return true;
		if(surPieton) return genre == 3 || vehiculeOccupe;
		if(!auSol) return false;
		return !(0.5f > normaleZ) && genre != 3;
	}
	// Après l'explosion (de 0x4d3a67 à 0x4d3b0c) : le stimulus 2 naît du tireur sur le rayon +0x94 de la fiche
	// (PedCreateStimulus(2, tireur, 0, contact, +0x94, -1)) quand il y a une entité, que les dégâts de l'entrée (+0x1c) sont au-dessus
	// de 0, et que l'entité est un objet (genre 4), du genre 6, ou que la surface du contact vaut 0xf ; sauf un journal (newsroll,
	// DAT_00bf4414) dans une boîte aux lettres (RMailbox, DAT_00bf4184 ; DPE_RCMail, DAT_00bf3e60).
	static bool StimulusDeLObjet(bool entite, float degats, int genre, int surface, bool journalEnBoite) {
		return entite && 0.0f < degats && (genre == 4 || genre == 6 || surface == 0xf) && !journalEnBoite;
	}
	// Le compteur d'impacts (+0x23c) : il monte de 1 tant que ses trois bits bas sont sous 5, les autres bits gardés.
	static uint32 Compter(uint32 mot) { return (mot & 7) < 5 ? (((mot + 1) ^ mot) & 7) ^ mot : mot; }
	// L'effet d'impact (de 0x4d3984 à 0x4d3bce) : avec un nom dans la fiche (+0xc0), seulement pour un choc fort ; le compteur monte.
	// Sans nom, seule la balle de baseball (DAT_00bf43e4) contre une entité qui n'est pas du genre 6 compte : le compteur monte, puis
	// « BsBallMiss » si FUN_004a8350 (+0x3c de DAT_020c8704) rend 8, « DunkBallMiss » pour 0xd, rien sinon. Rend le nom de l'effet (nil :
	// aucun) et dit dans `compte` si le compteur monte.
	static const char *Effet(const char *nomFiche, bool fort, bool baseball, bool entite, int genre, int jeu, bool *compte) {
		*compte = false;
		if(nomFiche){
			if(!fort) return nil;
			*compte = true;
			return nomFiche;
		}
		if(!baseball || !entite || genre == 6) return nil;
		*compte = true;
		return jeu == 8 ? "BsBallMiss" : jeu == 0xd ? "DunkBallMiss" : nil;
	}
	// Le premier contact au sol (de 0x4d3bd3 à 0x4d3c06) : l'octet +0x18 de l'entrée nul et une normale au z strictement au-dessus de
	// 0,5 (fld _DAT_009055a0, fcomp, test ah,5, jp).
	static bool PremierSol(bool dejaAuSol, float normaleZ) { return !dejaAuSol && 0.5f < normaleZ; }
};

// Le contact d'un projectile en vol (FUN_0046ad90, le contact d'un CPhysical, lue dans l'index ; ici sa part pour un projectile, +0xc4
// non nul, qui appelle le choc FUN_004d3860 à 0x46b0ae). L'octet +0x22c du projectile compte ses contacts.
struct CContactProjectile {
	// Le coup : contre un piéton (genre 3) ou un véhicule (genre 2), au premier contact seulement (l'octet +0x22c nul), à tous pour
	// RBandBall (DAT_00bf432c). Sur un piéton, c'est sa méthode du coup (vtable +0x3c) avec les dégâts +0x1c de l'entrée et le tireur.
	static bool Coup(int genre, uint8 contacts, bool rbandball) { return (genre == 3 || genre == 2) && (contacts == 0 || rbandball); }
	// Le tireur épargné : quand le coup irait au piéton qui a tiré (+4 de l'entrée) moins de 200 ms après la création (+8, écart non
	// signé), la fonction rend la main : ni coup, ni contact compté, ni choc.
	static bool TireurEpargne(bool surLeTireur, uint32 maintenant, uint32 creation) { return surLeTireur && (uint32)(maintenant - creation) < 200; }
	// L'octet « fort » passé au choc : faux contre un piéton ou un véhicule dont le coup est sauté, faux sous une vitesse (la norme de
	// +0x124, +0x128, +0x12c, par FUN_0085c4d0) de 0,03 (le double 0x900e28).
	static bool Fort(int genre, uint8 contacts, bool rbandball, float vitesse) {
		if((genre == 3 || genre == 2) && !Coup(genre, contacts, rbandball)) return false;
		return !(vitesse < 0.03f);
	}
	// Après le choc, sans entité de contact (ou une entité qui n'est ni un piéton ni en vol) : sous un carré de vitesse de 1e-4 (le
	// double 0x900af8 ; 1e-10, 0x90c718, quand l'octet +0x20a vaut 7) et sur une normale au z au-dessus de 0,8 (0x900ad0), les
	// vitesses +0x124 à +0x138 tombent à 0, l'octet +0x180 passe à 10 et la méthode +0x1c(1) met le projectile au repos.
	static bool AuRepos(float vitesse2, float normaleZ, bool genre7) { return vitesse2 < (genre7 ? 1e-10f : 1e-4f) && 0.8f < normaleZ; }
};
