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
	// La fonction 0046ca40 (le déplacement d'un CPhysical dans le monde, 2 884 octets, non recréée en entier ; ici sa part pour un projectile, +0xc4 non nul, lue à
	// l'assembleur de 0x46cd98 à 0x46ce47 et de 0x46d0d7 à 0x46d22e ; le balayage qui trouve le contact n'est pas recréé). Au
	// contact le plus proche, à la fraction f du déplacement de l'image, le projectile est reposé à l'ancienne position plus le
	// déplacement fois (f − 0,005) (le double 0x90c740), arrondi en simple précision.
	static float Recul(float fraction) { return (float)((double)fraction - 0.005); }
	// Puis il rebondit, au premier tour de la boucle des contacts (les autres CPhysical glissent le long de la normale et refont
	// jusqu'à trois tours). La normale du contact, dont le z positif est mis à 0 quand l'entité touchée a un +0xf0 (`aPlat` ; fldz,
	// fcom, test ah,5, jp), est normalisée (0x414cc0, supposée). Contre un piéton (le genre de l'entité touchée, +0x108 & 7, vaut 3)
	// le coefficient tangentiel est _DAT_00a24040 et le normal _DAT_00a2403c, 0,75 tous les deux ; sinon les deux sont l'élasticité
	// +0x164 du projectile (le +0x5c de sa fiche). La vitesse se partage en sa part le long de la normale, vn = n × (v·n) (le produit
	// scalaire arrondi en simple précision : vz nz + (vy ny + vx nx)), et le reste vt = v − vn ; la nouvelle vitesse est
	// tangentiel × vt − normal × vn. Deux modèles ont leur cas à part, non suivis : celui de DAT_00bf44dc ne rebondit pas (vitesse à
	// 0, avancé de 0,1 le long de sa direction, le bit 0x2 de +0x1bc baissé : il se plante, supposé) et celui de DAT_00bf439c reçoit en
	// plus deux poussées de 0,03 sur une normale au z sous 0,7 en valeur absolue.
	static constexpr float kTangentielPieton = 0.75f, kNormalPieton = 0.75f;
	static void Rebondir(float *v, const float *normale, bool pieton, bool aPlat, float elasticite) {
		float n[3] = {normale[0], normale[1], normale[2]};
		if(aPlat && 0.0f < n[2]) n[2] = 0.0f;
		float l = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
		if(l > 0.0f){ n[0] /= l; n[1] /= l; n[2] /= l; }
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
