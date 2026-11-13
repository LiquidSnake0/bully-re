// La table des projectiles en vol : 32 entrées de 0x34 octets à partir de 0xc3a038 (la boucle de FUN_004d4590 s'arrête à 0xc3a700),
// l'objet de chacune dans DAT_00c3a6b8 (FUN_004d3c80). Une entrée : +4 le tireur (0xc3a03c), +8 l'heure de sa création (0xc3a040 :
// DAT_00c1a9b4, en ms), l'octet +0x14 actif (0xc3a04c), +0x1c les dégâts (0xc3a054), +0x30 la fiche ProjectileButes (0xc3a068).
// Lu dans l'index le 07.11.2026 (FUN_004d4c40, la création ; FUN_004d4590, FUN_004d3f60, FUN_004d3dc0).
#pragma once
#include "../common.h"
#include <cmath>

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
	// FUN_004d45e0, la fin d'un projectile hors de l'eau (lue à l'assembleur de 0x4d4717 à 0x4d4739, puis 0x4d4868) : il vit tant que
	// son explosion n'est pas faite (l'octet +0x16 de l'entrée) et que maintenant ne dépasse pas sa création plus sa vie (cmp, jbe :
	// non signé), ou que sa vie est nulle ; sinon FUN_004d3dc0(i, 1) le détruit. Un projectile qui a explosé à un contact disparaît
	// donc à l'image suivante ; les autres restent dans le monde jusqu'à la fin de leur vie.
	bool Vit(int i, uint32 maintenant, uint32 vie) const {
		if(i < 0 || i >= kTaille || !actif[i] || explose[i]) return false;
		return maintenant <= creation[i] + vie || vie == 0;
	}
	// FUN_004d3dc0(i, explosion) (lue dans l'index), pour une entrée active : l'octet actif à 0 ; son effet (+0x20, +0x24) arrêté et
	// rendu ; puis, avec l'objet du projectile : avec son second argument, FUN_004d37d0 au point de l'objet relevé de 0,05
	// (_DAT_009068b8), la normale (0, 0, 1), sans contact (l'explosion de la fin de vie, si elle n'est pas déjà faite) ; l'objet détruit
	// et sa référence rendue. FUN_004d45e0 passe 1 à la fin de la vie et 0 dans l'eau, où elle fait elle-même l'explosion avec
	// l'octet +0x8c ou +0x8d de la fiche (ExploseDansLEau ; lue à l'assembleur de 0x4d4684 à 0x4d4712 : le point du projectile relevé
	// de 0,05, la normale (0, 0, 1), sans contact). Rend vrai quand l'explosion de la fin de vie est à faire.
	bool Detruire(int i, bool explosion) {
		if(i < 0 || i >= kTaille || !actif[i]) return false;
		actif[i] = false;
		return explosion && Exploser(i);
	}
	static bool ExploseDansLEau(bool partout, bool auSol) { return partout || auSol; }
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

// Le vol d'un projectile : un CProjectile est un CPhysical dont la vitesse (+0x124, +0x128, +0x12c) compte des mètres par pas
// (0xc1a9a4, le pas du moteur en cinquantièmes de seconde). Son constructeur (FUN_004d2160, lu à l'assembleur de 0x4d2160 à 0x4d228c) pose le facteur de
// gravité +0x230 à 1, la masse +0x158 et +0x15c à 1, la résistance de l'air +0x160 à 0,99999 (_DAT_00912824), l'élasticité +0x164 à
// 0,75 (_DAT_0093e860), +0x168 à 0,0008 (_DAT_009137dc), +0xc4 à 1 et l'octet des contacts +0x22c à 0 ; celui de CPhysical
// (lu dans l'index) l'échelle +0x154 à 1. La création (FUN_004d4c40, relue dans l'index) y écrit la vitesse du tir, le facteur de
// gravité de la fiche (+0x80 → +0x230), son élasticité (+0x5c → +0x164), l'octet +0x20a (6, ou 7 avec l'octet +0x7c de la fiche) et
// le bit 0x2 de +0x1bc, levé hors du mode de DAT_01b65c14 : le projectile pèse.
struct CVolProjectile {
	static constexpr float kAir = 0.99999f, kElasticite = 0.75f;
	// FUN_004494d0 (la méthode +0x8c de CProjectile, lue à l'assembleur de 0x4494d0 à 0x4494e4) : la gravité du projectile par pas,
	// son facteur +0x230 fois 0,008 (le double 0x905a70), arrondie en simple précision.
	static float Gravite(float facteur) { return (float)((double)facteur * (double)0.008f); }
	// FUN_004d1510 (la méthode +0x90 de CProjectile, lue à l'assembleur de 0x4d1510 à 0x4d1555) : avec le bit 0x2 de +0x1bc, la vitesse
	// en z perd la gravité (la méthode +0x8c) fois le pas fois l'échelle +0x154.
	static float Peser(float vz, bool pese, float gravite, float pas, float echelle = 1.0f) {
		return pese ? (float)((double)vz - (double)gravite * pas * echelle) : vz;
	}
	// FUN_00469760 (la méthode +0x94 de CPhysical, le déplacement, lue à l'assembleur de 0x469760 à 0x4698bb). Avec le bit 0x1 de
	// +0x1bd, la vitesse tombe à 0 et rien ne bouge (rend faux). Sinon la méthode +0x90 d'abord (Peser), puis la position avance de
	// la vitesse fois d, d = l'échelle +0x154 fois le pas ; en z, quand l'entité pèse (le bit 0x2 de +0x1bc) et que sa méthode +0x88
	// rend 0 (celle de CProjectile, 0x828240 : xor al,al), la position perd en plus la moitié (le double 0x8ff1f8) de la gravité fois
	// d au carré (d au carré arrondi en simple précision). La vitesse en z a donc déjà perdu la gravité du pas quand elle déplace.
	static bool Deplacer(float *pos, float *v, bool fige, bool pese, float gravite, float pas, float echelle = 1.0f) {
		if(fige){ v[0] = v[1] = v[2] = 0.0f; return false; }
		v[2] = Peser(v[2], pese, gravite, pas, echelle);
		float d = echelle * pas;
		pos[0] = pos[0] + d * v[0];
		pos[1] = v[1] * d + pos[1];
		if(!pese) pos[2] = d * v[2] + pos[2];
		else pos[2] = (float)(((double)v[2] * (double)d - (double)gravite * 0.5 * (double)(float)((double)d * (double)d)) + (double)pos[2]);
		return true;
	}
	// FUN_004698c0 (la résistance de l'air d'un CPhysical, lue à l'assembleur de 0x4698c0 à 0x469a1c ; 0085cb40, la puissance à deux
	// arguments sur la pile du coprocesseur, est supposée être pow). Avec une résistance +0x160 strictement au-dessus de 0,1 (le
	// double 0x900128 : fcomp, test ah,0x41, jne), la vitesse et la vitesse angulaire (+0x130, +0x134, +0x138) prennent le facteur
	// +0x160 puissance le pas : pour un projectile, 0,99999 puissance 1,65 par image. Sinon (une traînée) : le facteur de la vitesse
	// est (1 / |1 + +0x160 × 0,5 × |v|²|) puissance le pas, chaque étape arrondie en simple précision, et la vitesse angulaire prend
	// 0,99 (le double 0x8ffcf8).
	static void Air(float *v, float *w, float air, float pas) {
		if((double)air > 0.1){
			float f = (float)pow((double)air, (double)pas);
			for(int i = 0; i < 3; i++) v[i] = v[i] * f;
			for(int i = 0; i < 3; i++) w[i] = w[i] * f;
			return;
		}
		float n2 = v[1] * v[1] + v[0] * v[0] + v[2] * v[2];
		float k = (float)((double)air * 0.5 * (double)n2);
		float a = fabsf(1.0f + k);
		float b = 1.0f / a;
		float f = (float)pow((double)b, (double)pas);
		for(int i = 0; i < 3; i++) v[i] = v[i] * f;
		for(int i = 0; i < 3; i++) w[i] = (float)((double)w[i] * 0.99);
	}
	// FUN_00469a20 (le contrôle d'un CPhysical à chaque image, sa méthode +0x30, lue à l'assembleur de 0x469a20 à 0x469c5c ; celui
	// d'un objet y arrive en dernier). Elle remet d'abord à 0 +0x1a0, +0x2c, +0x38, +0x40, +0x7c, le mot +0x1b4, +0x1a4, et le bit
	// 0x10 de +0x1bc hors d'un piéton (le genre, +0x108 & 7, autre que 3) ; avec +0x108 & 0xf8 égal à 0x10 elle rend la main là ;
	// sinon l'octet +0x181 et +0x4c passent à 0. La mesure du repos ne se fait que sans +0x34 et pour un objet (genre 4) ou un piéton
	// sans +0x30 (`mesure`) : les moyennes +0x13c (de la vitesse) et +0x148 (de la vitesse angulaire) deviennent la moitié (le double
	// 0x8ff1f8) de leur somme avec la vitesse du moment (0x412850, supposée la somme de deux vecteurs) ; le seuil est le pas fois
	// 0,003 (le double 0x90c700), au carré. Une moyenne dont le carré de la norme atteint le seuil remet le compteur +0x180 à 0
	// (fcompp, test ah,0x41, jne : l'égalité aussi). Sous le seuil pour les deux, et sans +0xb0 (`tenu`), le compteur monte : au-delà
	// de 10, il reste à 10, la méthode +0x1c(1) met l'entité au repos, les deux vitesses tombent à 0 et la fonction rend la main
	// (rend vrai ici) sans l'air. Dans tous les autres cas elle finit par la résistance de l'air (un saut vers Air).
	static bool Controler(float *v, float *w, float *mv, float *mw, unsigned char *compte, bool mesure, bool tenu, float air, float pas) {
		if(mesure){
			for(int i = 0; i < 3; i++) mv[i] = (float)((double)(float)(mv[i] + v[i]) * 0.5);
			for(int i = 0; i < 3; i++) mw[i] = (float)((double)(float)(mw[i] + w[i]) * 0.5);
			float s = (float)((double)pas * 0.003);
			float n2 = mv[1] * mv[1] + mv[0] * mv[0] + mv[2] * mv[2];
			float s2 = s * s;
			if(s2 <= n2) *compte = 0;
			else{
				n2 = mw[1] * mw[1] + mw[0] * mw[0] + mw[2] * mw[2];
				if(s2 <= n2) *compte = 0;
				else if(!tenu){
					*compte = (unsigned char)(*compte + 1);
					if(*compte > 10){
						*compte = 10;
						v[0] = v[1] = v[2] = 0.0f; w[0] = w[1] = w[2] = 0.0f;
						return true;
					}
				}
			}
		}
		Air(v, w, air, pas);
		return false;
	}
	// FUN_004d2290 (le contrôle d'un CProjectile à chaque image, sa méthode +0x30, lue à l'assembleur de 0x4d2290 à 0x4d24e9). Tenu
	// (+0xb0 non nul), rien. Sinon : le son du modèle (00598030 sur 0xcf5dc4, puis 0059a710 avec 3 000, ou 8 000 pour le modèle de
	// DAT_00bf4394 ; non suivi) ; l'effet +0xa8 de la fiche, une seule fois (le bit 0x1 de +0x22d), quand le projectile passe près
	// du joueur (DAT_00c1aea8 ; PresDuJoueur) ; puis le contrôle selon l'entrée de la table (004d3360 la rend) et la fiche :
	// - l'octet +0x15 de l'entrée (le mode de DAT_01b65c14 à la création) : un autre vol, non suivi (0x4d1960) ;
	// - un +0x10 de l'entrée non nul (le +0xf0 de la fiche, une durée) : le contrôle de l'objet, puis la poussée (Pousser) ;
	// - l'octet +0x124 de la fiche : le contrôle de l'objet, puis l'échelle de la chute (EchelleDeChute) ;
	// - sinon : les déclencheurs où le projectile entre (006d7220 sur 0x20c7e98, non recréée), puis le contrôle de l'objet.
	// Le contrôle de l'objet (celui de CObject : Freiner pour un objet dont l'octet +0x20a vaut 7) finit par celui de CPhysical
	// (Controler).
	enum { CONTROLE_AUCUN, CONTROLE_AUTRE_VOL, CONTROLE_POUSSEE, CONTROLE_CHUTE, CONTROLE_DECLENCHEURS };
	static int Controle(bool tenu, bool autreVol, uint32 dureePoussee, bool chuteRefaite) {
		if(tenu) return CONTROLE_AUCUN;
		if(autreVol) return CONTROLE_AUTRE_VOL;
		if(dureePoussee != 0) return CONTROLE_POUSSEE;
		return chuteRefaite ? CONTROLE_CHUTE : CONTROLE_DECLENCHEURS;
	}
	// Près du joueur (de 0x4d231e à 0x4d2386) : l'écart du joueur au projectile (0x414a20, supposée la différence de deux vecteurs),
	// son z relevé de 1,5 (le double 0x8ffa08), a un carré de norme qui ne dépasse pas 4 (_DAT_00900b0c : fld, fcomp, test ah,1,
	// jne) : à 2 m au plus d'un point 1,5 m au-dessus de la position du joueur.
	static bool PresDuJoueur(const float *joueur, const float *projectile) {
		float x = joueur[0] - projectile[0], y = joueur[1] - projectile[1];
		float z = (float)((double)(float)(joueur[2] - projectile[2]) + 1.5);
		float d2 = z * z + (y * y + x * x);
		return d2 <= 4.0f;
	}
	// FUN_004d2090 (lue à l'assembleur de 0x4d2090 à 0x4d20ce ; pour une fiche dont l'octet +0x124 est levé, après le contrôle) :
	// l'échelle de temps +0x154 du projectile vaut le +0x128 de la fiche tant que sa vitesse en z est strictement sous -0,1 (le
	// double 0x912830 : fcomp, test ah,5, jp), et 1 sinon. Un projectile qui retombe à plus de 0,1 m par pas vole donc à une autre
	// allure : c'est le +0x128 par lequel la visée (FUN_0065ecb0) refait la durée du vol.
	static float EchelleDeChute(float vz, float echelleFiche) { return (double)vz < (double)-0.1f ? echelleFiche : 1.0f; }
	// FUN_004d1ec0 (la poussée d'un projectile, lue à l'assembleur de 0x4d1ec0 à 0x4d208a ; pour une entrée dont le +0x10, la durée
	// +0xf0 de la fiche, n'est pas nul, après le contrôle). Tant que maintenant (0xc1a9b4) est strictement avant la création (+8 de
	// l'entrée) plus la durée (cmp, jae) : le délai est la durée fois le +0x100 de la fiche (0x85c6f0, supposée la conversion en
	// entier qui tronque) ; la fraction f vaut 0 jusqu'au délai compris (jbe), puis (maintenant - création - délai) / (durée -
	// délai), et rien ne se passe quand f atteint 1 (fcom, test ah,0x41, je ; fucom, test ah,0x44, jnp). La vitesse gagne
	// ((v × pas) × +0xf8 de la fiche) × (1 - f), chaque produit arrondi en simple précision, et sa composante z gagne en plus
	// (+0xfc de la fiche × pas) × (1 - f) : une poussée le long de la vitesse et une portance, qui s'éteignent sur la durée
	// (supposé : le vol d'une fusée).
	static bool Pousser(float *v, uint32 maintenant, uint32 creation, uint32 duree, float fractionDelai, float poussee, float portance, float pas) {
		if(maintenant >= creation + duree) return false;
		float f = 0.0f;
		uint32 delai = (uint32)(long long)((double)fractionDelai * (double)duree);
		if(maintenant > creation + delai){
			f = (float)((double)(uint32)(maintenant - creation - delai) / (double)(int32)(duree - delai));
			if(f >= 1.0f) return false;
		}
		float r = (float)(1.0 - (double)f), d[3];
		for(int i = 0; i < 3; i++){ float a = v[i] * pas; a = a * poussee; d[i] = a * r; }
		for(int i = 0; i < 3; i++) v[i] = v[i] + d[i];
		v[2] = v[2] + (float)((double)portance * (double)pas * (1.0 - (double)f));
		return true;
	}
	// FUN_004cf350 (le contrôle d'un CObject à chaque image, lue à l'assembleur de 0x4cf350 à 0x4cf4cd ; celui d'un CProjectile
	// l'appelle avant sa poussée ou sa chute). Avec +0xb0 non nul, rien. Pour un objet dont l'octet +0x20a vaut 7, et quand 006838d0
	// rend faux (l'objet de DAT_01b89150, non suivi), un freinage : sa fiche F est celle du nom +0x12c de la fiche du projectile
	// (résolue par 0072ad50 dans la table 0x1b65378), sinon celle de DAT_020c888c ; s est la norme de la vitesse (FUN_0040df90, lue de
	// 0x40df90 à 0x40dfc0 : la racine de |z² + (x² + y²)|). Quand s atteint F+0x1c (fcomp, test ah,0x41, jne : l'égalité aussi) le
	// facteur est F+0x20 ; sinon le facteur est F+0x24 et, quand s est strictement sous F+0x18, la vitesse et la vitesse angulaire
	// prennent d'abord le vecteur de DAT_00bcc564 (dans le bss ; supposé nul). Puis les deux vitesses sont multipliées par le
	// facteur. Dans tous les cas sans +0xb0 la fonction finit par le contrôle de CPhysical (un saut vers Controler).
	static float Norme(const float *v) { return sqrtf(fabsf(v[2] * v[2] + (v[0] * v[0] + v[1] * v[1]))); }
	static void Freiner(float *v, float *w, float seuilArret, float seuilVite, float facteurVite, float facteurLent) {
		float s = Norme(v), f;
		if(seuilVite <= s) f = facteurVite;
		else{
			f = facteurLent;
			if(s < seuilArret){ v[0] = v[1] = v[2] = 0.0f; w[0] = w[1] = w[2] = 0.0f; }
		}
		for(int i = 0; i < 3; i++) v[i] = v[i] * f;
		for(int i = 0; i < 3; i++) w[i] = w[i] * f;
	}
	// FUN_004cf190 (la mise au repos d'un CObject, lue à l'assembleur de 0x4cf190 à 0x4cf1c6 ; la méthode +0x1c d'un CProjectile,
	// qui redresse d'abord sa matrice quand l'octet +0x15 de son entrée est levé, finit par elle). L'argument (un octet : au repos
	// ou non) va dans +0x28, ou dans +0xb0 pour un objet dont l'octet +0x20a vaut 7 ; le compteur de repos +0x180 prend 10 quand
	// l'argument n'est pas nul, 0 sinon (neg, sbb, and 0xa). Le +0xb0 que les contrôles lisent d'abord est donc, pour ces objets,
	// le repos lui-même.
	static void Reposer(unsigned char repos, bool genre7, uint32 *o28, uint32 *ob0, unsigned char *compte) {
		if(genre7) *ob0 = repos; else *o28 = repos;
		*compte = repos ? 10 : 0;
	}
	// La fonction 0046ca40 (le déplacement d'un CPhysical dans le monde, 2 884 octets, non recréée en entier ; ici sa part pour un projectile, +0xc4 non nul, lue à
	// l'assembleur de 0x46cd98 à 0x46ce47 et de 0x46d0d7 à 0x46d22e ; le balayage d'une liste de secteur, 0046c4b0, n'est pas recréé). Au
	// contact le plus proche, à la fraction f du déplacement de l'image, le projectile est reposé à l'ancienne position plus le
	// déplacement fois (f − 0,005) (le double 0x90c740, qui vaut 0,005 en simple précision : 0,004999999888241291), arrondi en
	// simple précision.
	static float Recul(float fraction) { return (float)((double)fraction - 0.004999999888241291); }
	// Le début de la même fonction (lu à l'assembleur de son premier octet à 0x46cd98). La distance parcourue +0x19c passe à 0 ; tenue (+0xb0
	// non nul) ou sans son +0x1c, l'entité ne fait rien de plus. Quand les trois réels de sa vitesse (+0x124) et les trois de sa
	// vitesse angulaire (+0x130) sont tous nuls (fcom contre 0, test ah,0x44, jp) et que l'octet +0x108 a un des bits 0xf8, elle
	// ne se déplace pas : +0x34 passe à 0 et +0x38 à 1, et la fonction rend la main.
	static bool Immobile(const float *v, const float *w, uint32 o108) {
		for(int i = 0; i < 3; i++) if(v[i] != 0.0f || w[i] != 0.0f) return false;
		return (o108 & 0xf8) != 0;
	}
	// Sinon elle avance (ses méthodes +0xa0, qui remplit quatre listes, +0x98 et +0x94 ; non suivies) puis, quand elle descend
	// (l'ancien z strictement au-dessus du nouveau : fcom, test ah,0x41, jne) et que son +0xc4 est nul (celui d'un projectile ne
	// l'est pas : ce partage n'est pas pour lui), sa chute est mise de côté : nouveau z − ancien z − 0,05 (le double 0x900c18, 0,05
	// en simple précision), et le z de sa position devient l'ancien z + 0,05. Le premier balayage se fait donc à plat, 5 cm plus
	// haut qu'au départ ; la chute, strictement négative, se balaie à la sortie (PoserApresChute). 0 : pas de partage.
	static float PartagerChute(float ancienZ, float *z, uint32 oc4) {
		if(!(ancienZ > *z) || oc4 != 0) return 0.0f;
		float chute = (float)((double)*z - (double)ancienZ - 0.05000000074505806);
		*z = (float)(0.05000000074505806 + (double)ancienZ);
		return chute;
	}
	// Le contact d'un balayage (0x28 octets ; FUN_0046c970 et le déplacement en posent un sur la pile : trois réels nuls en +0x0,
	// une référence nulle en +0xc, libérée à la fin par 00465690, trois réels nuls en +0x10, et la fraction du déplacement en
	// +0x24, FLT_MAX, le réel 0x9009b8, tant que rien n'est touché). Supposé : +0x0 est le point et +0x10 la normale.
	struct ContactBalaye {
		float point[3] = {0.0f, 0.0f, 0.0f};
		const void *entite = nullptr;
		float normale[3] = {0.0f, 0.0f, 0.0f};
		uint32 o1c = 0;
		uint16 o20 = 0, o22 = 0;
		float fraction = 3.4028234663852886e+38f;
		// FUN_0046a920 (l'affectation d'un contact, lue à l'assembleur de 0x46a920 à 0x46a99b, ret 4) : les trois mots de +0x0 ; la
		// référence +0xc quand elle change : l'ancienne, non nulle, est déliée (00465690 avec l'adresse du champ), la nouvelle est
		// posée puis, non nulle, liée (00465680) ; les quatre mots de +0x10 à +0x1c, les deux mots courts +0x20 et +0x22, la fraction
		// +0x24. Rend le contact. `delier` et `lier` reçoivent l'entité.
		template<class D, class L> ContactBalaye &Copier(const ContactBalaye &c, D &&delier, L &&lier) {
			for(int i = 0; i < 3; i++) point[i] = c.point[i];
			if(c.entite != entite){
				if(entite) delier(entite);
				entite = c.entite;
				if(c.entite) lier(c.entite);
			}
			for(int i = 0; i < 3; i++) normale[i] = c.normale[i];
			o1c = c.o1c; o20 = c.o20; o22 = c.o22; fraction = c.fraction;
			return *this;
		}
	};
	// FUN_0046c970(listes, secteur, meilleur, arg4, quatrième) (lue à l'assembleur de 0x46c970 à 0x46ca3f, ret 0x14) : le balayage
	// d'un secteur. Les quatre listes du secteur (quatre mots de suite ; la quatrième, de rang 3, seulement quand l'octet du
	// cinquième argument vaut 1) passent une à une par 0046c4b0 (le balayage d'une liste, non recréé : `balayer`, qui reçoit le rang
	// de la liste et un contact local, le même pour les quatre listes, fait une fois avant la boucle). Quand elle rend vrai et que
	// la fraction du contact local est strictement sous celle du meilleur (fcompp, test ah,0x41, jne), le contact local est copié
	// dans le meilleur (FUN_0046a920, Copier ; les liens de la référence ne sont pas suivis ici) et la fonction rendra vrai.
	template<class B> static bool BalayerSecteur(ContactBalaye *meilleur, bool quatrieme, B &&balayer) {
		ContactBalaye local;
		bool touche = false;
		for(int i = 0; i < 4; i++){
			if(!quatrieme && i == 3) continue;
			if(!balayer(i, &local)) continue;
			if(!(local.fraction < meilleur->fraction)) continue;
			touche = true;
			meilleur->Copier(local, [](const void *){}, [](const void *){});
		}
		return touche;
	}
	// La boucle du déplacement sur les secteurs de l'entité (la liste +0x178 : le secteur en +8 du nœud, le suivant en +0x10 ; de
	// 0x46cc0b à 0x46cc80, et la même de 0x46d364 à 0x46d3d5) : FUN_0046c970 sur chaque secteur, avec un contact neuf par balayage
	// de l'entité, et le meilleur garde celui dont la fraction est strictement la plus petite. Avant elle le code de balayage (le
	// mot 0xc1ae88) monte de 1, ou repart à 1 après 0045d6f0 quand il a atteint 0xffff.
	template<class S> static bool BalayerSecteurs(int secteurs, ContactBalaye *meilleur, S &&secteur) {
		bool touche = false;
		ContactBalaye c;
		for(int i = 0; i < secteurs; i++){
			if(!secteur(i, &c)) continue;
			touche = true;
			if(c.fraction < meilleur->fraction) *meilleur = c;
		}
		return touche;
	}
	static uint16 CodeDeBalayage(uint16 code, bool *remis = nullptr) {
		if(remis) *remis = code >= 0xffff;
		return code < 0xffff ? (uint16)(code + 1) : 1;
	}
	// La sortie de la même fonction (lue à l'assembleur de 0x46d2fc à 0x46d588). Le bit 0x10 de +0x1bd est baissé. Quand la chute
	// mise de côté est strictement négative (fldz, fcomp, test ah,0x41, jne), la position d'avant la chute est gardée, le z descend
	// de la chute et un second balayage, le même, cherche ce qu'elle touche : au contact, la position devient celle d'avant la
	// chute plus l'écart fois (la fraction − 0,005, le double 0x90c740) (00414a20, la différence ; FUN_00412850, la somme ; posée
	// par 0041c390), et le bit 0x10 de +0x1bd est levé. Vrai : l'entité s'est posée.
	static bool PoserApresChute(const float *avantChute, float *pos, bool contact, float fraction) {
		if(!contact) return false;
		float f = Recul(fraction);
		for(int i = 0; i < 3; i++){ float e = pos[i] - avantChute[i]; e = e * f; pos[i] = avantChute[i] + e; }
		return true;
	}
	// Puis 00443950, +0x34 passe à 0 et +0x38 à 1, et +0x19c reçoit la distance parcourue pendant l'image : la norme (la racine,
	// 0x85c4d0, de |z² + (y² + x²)|) de la position moins celle du début de la fonction ; enfin 00469320 et la libération des
	// références des deux contacts.
	static float Distance(const float *debut, const float *pos) {
		float d[3] = {pos[0] - debut[0], pos[1] - debut[1], pos[2] - debut[2]};
		return Norme(d);
	}
	// FUN_00465bc0(centre, rayon) (lue à l'assembleur de 0x465bc0 à 0x465cb9, ret 8 ; le balayage d'une liste de secteur, 0046c4b0,
	// l'appelle sur chaque entité avant sa méthode +0xa4) : la sphère englobante de l'entité touche-t-elle la sphère donnée ? Le
	// centre de la sphère de son modèle de collision (0051ae00 le rend ; ses trois premiers réels) passe dans le monde par la matrice
	// de l'entité (FUN_0046dbd0, FUN_0041a850 : `centreMonde`) ; le rayon donné grandit du rayon du modèle (+0xc) et, pour une entité
	// dont le +0xf0 n'est pas nul (`mobile`), des valeurs absolues des trois réels de sa vitesse (|+0x128| + |+0x124|, puis
	// + |+0x12c|). Vrai quand le carré de l'écart des deux centres (y² + x², puis + z², arrondi en simple précision) est strictement
	// sous le carré du rayon (fcompp, test ah,5, jp : l'égalité rend faux).
	static bool SpheresSeTouchent(const float *centreMonde, float rayonModele, bool mobile, const float *vitesse,
	                              const float *centre, float rayon) {
		float dx = centreMonde[0] - centre[0], dy = centreMonde[1] - centre[1], dz = centreMonde[2] - centre[2];
		rayon = rayonModele + rayon;
		if(mobile) rayon = fabsf(vitesse[1]) + fabsf(vitesse[0]) + fabsf(vitesse[2]) + rayon;
		float d2 = dy * dy + dx * dx + dz * dz;
		return d2 < rayon * rayon;
	}
	// Les trois filtres de piéton du balayage d'une liste de secteur (0046c4b0 les appelle quand l'entité qui balaie est un piéton,
	// avant le test des sphères : un filtre vrai, et l'entité de la liste est passée). Trois méthodes du piéton (ecx), un argument,
	// ret 4, lues à l'assembleur en entier. Ce qu'elles lisent d'une entité, sous le nom de son offset (le sens des champs n'est pas
	// lu : aucun nom n'est donné) :
	struct EntiteFiltree {
		unsigned char o108 = 0;                                            // l'octet +0x108 ; ses trois bits bas : le genre (3, un piéton)
		const EntiteFiltree *o450 = nullptr;                               // d'une entité de genre 2 : +0x450, l'octet signé +0x454, +0x46c
		signed char o454 = 0;
		unsigned o46c = 0;
		bool o1e25 = false, o1c98 = false;                                 // d'un piéton : les octets +0x1e25 et +0x1c98
		unsigned o1520 = 0, o133c = 0, o1500 = 0, o13b8 = 0, o1310 = 0;    //              les mots +0x1520, +0x133c, +0x1500, +0x13b8, +0x1310
		const EntiteFiltree *o1554 = nullptr, *o155c = nullptr, *o430 = nullptr, *o1e94 = nullptr;   // les pointeurs +0x1554, +0x155c, +0x430, +0x1e94
	};
	// FUN_004760b0(autre) (de 0x4760b0 à 0x476152) : le piéton ne touche-t-il pas cet autre piéton ? Vrai d'emblée si son octet
	// +0x1e25 n'est pas nul (avant même le genre de l'autre) ; faux si l'autre n'est pas un piéton ; vrai si c'est lui-même, si son
	// +0x1520 n'est pas nul, si son octet +0x1c98 n'est pas nul, si son +0x133c est nul, si l'octet +0x1e25 ou +0x1c98 de l'autre
	// n'est pas nul, si le +0x133c de l'autre est nul, si le +0x1554 de l'autre n'est pas nul, ou si l'autre est le piéton du mot
	// 0xc2b118 (`exclu` ; le joueur, supposé) ; sinon vrai seulement quand les deux +0x155c se désignent l'un l'autre et que les deux
	// +0x1500 sont nuls.
	static bool IgnorePieton(const EntiteFiltree &p, const EntiteFiltree &autre, const EntiteFiltree *exclu) {
		if(p.o1e25) return true;
		if((autre.o108 & 7) != 3) return false;
		if(&autre == &p || p.o1520 != 0 || p.o1c98 || p.o133c == 0 || autre.o1e25 || autre.o1c98 || autre.o133c == 0 ||
		   autre.o1554 != nullptr || &autre == exclu)
			return true;
		return p.o155c != nullptr && p.o1500 == 0 && autre.o155c != nullptr && autre.o1500 == 0 && p.o155c == &autre && autre.o155c == &p;
	}
	// FUN_00476160(e) (de 0x476160 à 0x476198 ; 0046c4b0 ne l'appelle que pour une entité de genre 2) : vrai si le +0x1554 du piéton
	// est cette entité (la comparaison passe avant le test du pointeur nul : une entité nulle « égale » un +0x1554 nul) ; sinon vrai si
	// l'entité est de genre 2, que son +0x450 est le piéton et que son octet signé +0x454 est strictement positif (jg).
	static bool IgnoreSonGenre2(const EntiteFiltree &p, const EntiteFiltree *e) {
		if(p.o1554 == e) return true;
		return e != nullptr && (e->o108 & 7) == 2 && e->o450 == &p && e->o454 > 0;
	}
	// FUN_00476270(e) (de 0x476270 à 0x4762ba ; même appelant, même cas) : vrai si le +0x13b8 du piéton est nul et que son +0x430
	// est l'entité ; ou si son +0x1310 vaut 0xd et que l'entité est son +0x1e94 ; ou si l'entité est son +0x430, que le +0x46c de
	// l'entité vaut 1 et que les cinq bits hauts de son octet +0x108 valent 0x38. Le binaire lit ce +0x46c sans tester le pointeur
	// (une entité nulle égale à un +0x430 nul le ferait lire à l'adresse 0x46c) : ici le cas rend faux.
	static bool IgnoreGenre2Lie(const EntiteFiltree &p, const EntiteFiltree *e) {
		if(p.o13b8 == 0 && p.o430 == e) return true;
		if(p.o1310 == 0xd && e == p.o1e94) return true;
		return e == p.o430 && e != nullptr && e->o46c == 1 && (e->o108 & 0xf8) == 0x38;
	}
	// Puis il rebondit, au premier tour de la boucle des contacts (les autres CPhysical glissent le long de la normale et refont
	// jusqu'à trois tours). La normale du contact, dont le z positif est mis à 0 quand l'entité touchée a un +0xf0 (`aPlat` ; fldz,
	// fcom, test ah,5, jp), est normalisée (0x414cc0, supposée). Contre un piéton (le genre de l'entité touchée, +0x108 & 7, vaut 3)
	// le coefficient tangentiel est _DAT_00a24040 et le normal _DAT_00a2403c, 0,75 tous les deux ; sinon les deux sont l'élasticité
	// +0x164 du projectile (le +0x5c de sa fiche). La vitesse se partage en sa part le long de la normale, vn = n × (v·n) (le produit
	// scalaire arrondi en simple précision : vz nz + (vy ny + vx nx)), et le reste vt = v − vn ; la nouvelle vitesse est
	// tangentiel × vt − normal × vn. Deux modèles ont leur cas à part (Planter, Devier).
	static constexpr float kTangentielPieton = 0.75f, kNormalPieton = 0.75f;
	static void NormaleDuContact(const float *normale, bool aPlat, float *n) {
		n[0] = normale[0]; n[1] = normale[1]; n[2] = normale[2];
		if(aPlat && 0.0f < n[2]) n[2] = 0.0f;
		float l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
		if(l > 0.0f){ n[0] /= l; n[1] /= l; n[2] /= l; }
	}
	// La fléchette (de 0x46cf6f à 0x46d0d2 ; le modèle de DAT_00bf44dc, que la table des noms de modèles 0xa136e8, des paires nom
	// puis adresse lues par 0043f1e0, donne à « dart ») ne rebondit pas : sa vitesse tombe à 0 et sa position avance de 0,1 (le
	// double 0x900128) le long de la direction qu'avait cette vitesse (normalisée par 00414cc0, supposée), chaque composante
	// arrondie en simple précision ; puis le bit 0x2 de +0x1bc est baissé (elle ne pèse plus), +0x28 passe à 1 (le repos d'un objet,
	// comme Reposer l'écrit) et +0x20 à 0 : elle reste plantée. La suite (un cap ramené entre -π et π puis posé, et deux appels sur
	// sa position et la normale, 007014f0 et 00701e70) n'est pas suivie.
	static void Planter(float *pos, float *v) {
		float d[3] = {v[0], v[1], v[2]};
		v[0] = v[1] = v[2] = 0.0f;
		float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
		if(l > 0.0f){ d[0] /= l; d[1] /= l; d[2] /= l; }
		for(int i = 0; i < 3; i++){ d[i] = (float)((double)d[i] * 0.1); pos[i] = pos[i] + d[i]; }
	}
	// Le frisbee (de 0x46d231 à 0x46d2f6 ; le modèle de DAT_00bf439c, « Wfrisbee » dans la même table) reçoit après son rebond deux
	// poussées quand le z de la normale du contact est strictement sous 0,7 en valeur absolue (_DAT_00a24038 : fcom, test ah,0x41,
	// jne), toutes deux à l'échelle k = (0,7 - |nz|) / 0,7 : la normale fois 0,03 (_DAT_00a24034) fois k, puis (0, 0, 1) fois -0,03
	// (_DAT_00a24030) fois k (FUN_00412200 deux fois, puis FUN_004150b0, lue de 0x4150b0 à 0x4150cf : la somme dans le premier
	// vecteur). Contre un mur il est donc repoussé du mur et rabattu vers le bas, d'autant plus que le mur est droit.
	static bool Devier(float *v, const float *normale, bool aPlat) {
		float n[3];
		NormaleDuContact(normale, aPlat, n);
		float a = fabsf(n[2]);
		if(0.7f <= a) return false;
		float k = (0.7f - a) / 0.7f;
		for(int i = 0; i < 3; i++){ float t = n[i] * 0.03f; v[i] = v[i] + t * k; }
		float haut[3] = {0.0f, 0.0f, 1.0f};
		for(int i = 0; i < 3; i++){ float t = haut[i] * -0.03f; v[i] = v[i] + t * k; }
		return true;
	}
	static void Rebondir(float *v, const float *normale, bool pieton, bool aPlat, float elasticite) {
		float n[3];
		NormaleDuContact(normale, aPlat, n);
		float tangentiel = pieton ? kTangentielPieton : elasticite;
		float normal = -(pieton ? kNormalPieton : elasticite);
		float p = v[2] * n[2] + (v[1] * n[1] + v[0] * n[0]);
		for(int i = 0; i < 3; i++){
			float vn = n[i] * p;
			float vt = v[i] - vn;
			v[i] = tangentiel * vt + normal * vn;
		}
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
	// FUN_0046b720 (la paire d'un contact, 342 octets, lue à l'assembleur de 0x46b720 à 0x46b873 ; le déplacement 0046ca40 l'appelle
	// avec l'entité touchée, le contact et un angle). Elle remet à 0 le +0xb0 de l'entité touchée, puis fait les deux contacts
	// (FUN_0046ad90 dans un sens, puis dans l'autre) sauf, quand l'un des deux est un projectile (+0xc4 non nul ; celui de l'entité
	// touchée d'abord) et l'autre un piéton (genre 3), dans deux cas, que cette fonction rend vrais : le piéton a un +0x1310 de 0xd
	// et il est le tireur du projectile (le +4 de son entrée, par 004d3360) ; ou le projectile est bas et lent : son z moins
	// celui du piéton, moins 0,1847944 (le double 0x90c720), est strictement sous 0,5 (_DAT_009055a0 : fcomp, test ah,5, jp) et sa
	// vitesse (la racine, 0x85c4d0, du carré de la norme de +0x124) est strictement sous 0,1 (le double 0x900128 : test ah,5, jnp).
	// Une bille qui roule aux pieds d'un piéton ne le touche donc pas.
	static bool SansContact(bool pieton, int32 jeuDuPieton, bool estLeTireur, float zProjectile, float zPieton, const float *v) {
		if(!pieton) return false;
		if(jeuDuPieton == 0xd && estLeTireur) return true;
		float dz = zProjectile - zPieton;
		dz = (float)((double)dz - 0.1847944);
		if(!(dz < 0.5f)) return false;
		float n2 = v[1] * v[1] + v[0] * v[0] + v[2] * v[2];
		float n = sqrtf(fabsf(n2));
		return (double)n < 0.1;
	}
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
	// La marque qu'un projectile laisse sur un bâtiment (FUN_0046ad90, lue à l'assembleur de 0x46b468 à 0x46b691) : l'entité touchée
	// est de genre 1, le CPhysical a une fiche +0xc4, son modèle +0x10e est dans la plage des armes (DAT_00a136b0 à DAT_00a136b4) et
	// le type de l'arme (+0x18 de son modèle) a une fiche de projectile +0x80. La fiche porte deux marques, des identifiants d'effet :
	// +0x150 (le nom +0x54) quand le z de la normale du contact (+0x18) n'est pas sous 0,3 en valeur absolue
	// (_DAT_00911e30 : fabs, fcomp, test ah,5, jp), +0x152 (le nom +0x58) sinon : un mur. Une marque qui n'est pas
	// 0xffff est posée par FUN_00560f70 avec le point du contact, sa normale (+0x10) et sa surface (l'octet +0x22).
	// Rend 0 pour la marque +0x150, 1 pour la marque +0x152.
	static int MarqueDuBatiment(float normaleZ) { return (normaleZ < 0.0f ? -normaleZ : normaleZ) < 0.3f ? 1 : 0; }
};

// Les explosions : une table de 15 entrées de 0x3c octets à 0xc39b60 (+0 la fiche du projectile, +4 un effet, +0xc le tireur, +0x10
// l'objet du projectile, +0x14 le point, +0x20 la fin, +0x24 l'octet actif, +0x28 le départ, +0x2c le rayon, +0x34 le modèle, +0x38
// l'heure de la pose). FUN_004d37d0 en pose une par FUN_004d2b40 ; FUN_004d92b0 les fait vivre à chaque image par FUN_004d2aa0,
// avant les projectiles (FUN_004d45e0). FUN_004d2690 (lue dans l'index), sur une entrée : le balayage du rayon autour du point
// (non lu : il reçoit le modèle, le point, le rayon, +0x98, +0x9c et +0xa0 de la fiche, le tireur, l'octet +0x90), l'objet +0x10
// mis de côté en DAT_00c1ae70 le temps du balayage ; son effet (Toucher) ; le son, avec +0x154 de la fiche et l'argument (l'effet
// +0xb8, « WaterExplosion » ou « MissleExplode ») ; avec l'octet +0x8f, le stimulus 3 du tireur au point, sur +0xd8.
struct CPlacesExplosions {
	enum { kTaille = 15 };
	bool actif[kTaille] = {};
	bool evincable[kTaille] = {};   // l'octet +0xdc de la fiche gardée
	uint32 heure[kTaille] = {}, depart[kTaille] = {}, fin[kTaille] = {};
	float rayon[kTaille] = {}, croissance[kTaille] = {};   // +0x2c ; +0xd0 de la fiche
	// FUN_004d2b40 (lue dans l'index) : la première place libre ; la table pleine, la plus vieille (+0x38, non signé, la première des
	// ex æquo) de celles dont la fiche a l'octet +0xdc, arrêtée (son effet +4 arrêté et rendu) ; sans aucune, -1 (la fonction rend
	// 0 : pas d'explosion). L'entrée prend l'heure, le rayon +0x94 de la fiche, la fin (maintenant plus le délai +0xcc plus la durée
	// +0xc8) et le départ : 0 pour un délai nul, et FUN_004d2690(1) part tout de suite (`*toutDeSuite`) ; sinon maintenant plus le
	// délai. Avec un contact, la marque d'impact de la surface (choisie sur +0x14c de la fiche, non lu) est posée au point.
	int Prendre(uint32 maintenant, float r, uint32 delai, uint32 duree, float croit, bool evinc, int *evincee = nil, bool *toutDeSuite = nil) {
		int place = -1, vieille = -1;
		uint32 plusTot = 0xffffffff;
		for(int i = 0; i < kTaille; i++){
			if(!actif[i]){ place = i; break; }
			if(evincable[i] && heure[i] < plusTot){ plusTot = heure[i]; vieille = i; }
		}
		if(evincee) *evincee = -1;
		if(place < 0){
			if(vieille < 0) return -1;
			place = vieille;
			if(evincee) *evincee = vieille;
		}
		actif[place] = true; evincable[place] = evinc; heure[place] = maintenant; rayon[place] = r; croissance[place] = croit;
		depart[place] = delai == 0 ? 0 : delai + maintenant;
		fin[place] = duree + delai + maintenant;
		if(toutDeSuite) *toutDeSuite = delai == 0;
		return place;
	}
	// Un sac de billes (bagmrbls, DAT_00bf43e8) dont le contact a une normale au z sous 0,96 en valeur absolue (le double 0x912c40)
	// garde dans l'entrée une autre fiche que la sienne (DAT_00c39b48, non lue) ; le rayon, le délai et la durée restent les siens.
	static bool AutreFiche(bool sacDeBilles, float normaleZ) { return sacDeBilles && (normaleZ < 0.0f ? -normaleZ : normaleZ) < 0.96f; }
	// FUN_004d2aa0 (lue à l'assembleur de 0x4d2aa0 à 0x4d2b34), pour une entrée active, à chaque image. Avec un départ en attente :
	// rien tant que maintenant ne le dépasse pas (non signé), puis le départ tombe à 0 et FUN_004d2690(1) (kDepart). Sinon le rayon
	// grandit de +0xd0 de la fiche par pas (0xc1a9a4), puis FUN_004d2690(0) tant que maintenant est sous la fin (kDure), ou l'entrée
	// s'arrête (kFin : l'octet actif à 0, l'effet +4 arrêté et rendu).
	enum { kRien, kDepart, kDure, kFin };
	int Avancer(int i, uint32 maintenant, float pas) {
		if(i < 0 || i >= kTaille || !actif[i]) return kRien;
		if(depart[i] != 0){
			if(maintenant <= depart[i]) return kRien;
			depart[i] = 0;
			return kDepart;
		}
		rayon[i] += croissance[i] * pas;
		if(maintenant < fin[i]) return kDure;
		actif[i] = false;
		return kFin;
	}
	// FUN_004d2690 : quand le balayage a touché et que la fiche a l'octet +0xdd : avec un +0xe0 nul
	// l'explosion s'arrête ; sinon sa fin est ramenée à maintenant plus +0xe0 si c'est plus tôt (les deux comparées en réels).
	void Toucher(int i, uint32 maintenant, bool arret, uint32 sursis) {
		if(i < 0 || i >= kTaille || !arret) return;
		if(sursis == 0){ actif[i] = false; return; }
		if(maintenant + sursis < fin[i]) fin[i] = maintenant + sursis;
	}
};

// Les marques d'impact : une table à 0xcf1f60, son indice d'écriture en tête puis 32 places de 16 octets (la position, le genre en
// mot +0xc). Les places qui n'ont pas servi ne répondent à aucun genre (supposé : l'outil et les tests partent d'un genre 0 que
// personne ne pose).
struct CPlacesMarques {
	enum { kTaille = 32 };
	int indice = 0;
	float pos[kTaille][3] = {};
	uint16 genre[kTaille] = {};
	// FUN_00560f70(genre, point, normale, surface) (lue dans l'index) : la place de l'indice reçoit le point avancé de 0,01 fois la
	// normale (le double 0x900d30, la somme par FUN_00412850) et le genre ; l'indice avance et revient à 0 après la place 31 : un
	// anneau, la plus vieille cédant sa place. L'effet de la marque naît ensuite (non suivi).
	void Poser(uint16 g, const float *point, const float *normale) {
		for(int k = 0; k < 3; k++) pos[indice][k] = point[k] + normale[k] * 0.01f;
		genre[indice] = g;
		if(++indice > 0x1f) indice = 0;
	}
	// FUN_00560f10(genre, déclencheur) (lue dans l'index) : le nombre des 32 places de ce genre dont la position est dans le
	// déclencheur (FUN_006d6e50) ; `dedans` est ce test.
	template<class F> int Compter(uint16 g, F dedans) const {
		int n = 0;
		for(int i = 0; i < kTaille; i++) if(genre[i] == g && dedans(pos[i])) n++;
		return n;
	}
};
