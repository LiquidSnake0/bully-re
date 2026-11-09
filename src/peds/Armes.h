// Les armes des piétons : le type d'une arme et l'inventaire.
//
// Le type d'une arme est une fiche ObjectButes (une piste « Butes » des arbres d'actions, docs/cat.md) : au démarrage,
// CWeapon::RegisterCB (FUN_004da330 → FUN_004da2d0) passe chaque modèle des plages weap (0xa136b0 … 0xa136b4) et item
// (0xa136b8 … 0xa136bc) à FUN_0050eaa0, qui range dans son modelinfo (+0x18) la fiche ObjectButes du chemin
// Weapons/Info/<nom>, sinon Items/<nom>, sinon OtherPickups/<nom> (les HashString de 0x902698, 0x9328d4, 0x9328cc et
// 0x9328bc, posés par l'initialisation statique 0x617d70 ; FUN_0072ad50). WeaponGetType (FUN_0051b300) rend ce +0x18,
// ou la fiche ObjectButes par défaut (DAT_020c88d8, celle du nœud Globals de Globals.cat, FUN_0072ae00) si l'arme vaut -1
// ou si le modelinfo n'en a pas.
//
// L'inventaire (le composant +0x1c4 du piéton, son piéton en +4) n'a pas d'entrées à lui : elles sont dans un pool commun à
// tous les piétons (ms_pPool654, DAT_00c0f654 : 57 entrées de 0x28 octets, docs/pools.md), chacune marquée de l'inventaire
// qui la tient (+0x24) ; ses munitions sont un mot en +0, l'arme un mot en +2.
#pragma once
#include "../common.h"
#include <cmath>
#include <string>

struct CTypeArme {
	bool lu = false;              // une fiche trouvée (sinon la fiche par défaut tient lieu)
	int32 genre = 0;              // +0x18 (24)
	std::string arbre;            // +0x1c (28) : l'arbre d'actions de l'arme (« /Global/SlingShot »)
	int32 maximum = 0;            // +0x34 (52) : les munitions au plus (FUN_0045c890) ; -1 : jamais retirées (FUN_0045cf70)
	std::string nomProjectile;    // +0x50 (80) : le nom de sa fiche de projectile (Weapons/Projectiles/<nom>, FUN_00728150)
	std::string nomMunition;      // +0x54 (84) : le nom du modèle des munitions (« marble » pour le lance-pierre)
	int32 inventaire = 0;         // +0x64 (100) : non nul, l'arme se range dans l'inventaire (FUN_0045cbc0)
	bool tir68 = false, tir69 = false;   // les octets +0x68 (104) et +0x69 (105) : WeaponGetType (0x5c63d0) en fait 0, 1 ou 2
	bool tireLachee = false;      // l'octet +0x2b (43) : IsWeaponFiredWhenDropped (FUN_0065bbe0)
	bool auContact = false;       // l'octet +0x25 (37) : le ramassage se prend au contact (FUN_00445710)
	float decalageRamassage = 0.0f;   // +0x44 (68) : ajouté au z d'un ramassage de ce modèle (FUN_00443af0)
	// +0x84 : le modèle des munitions, -2 tant qu'il n'est pas résolu ; FUN_00728150 le tire du nom +0x54 (FUN_0051ad50 :
	// -1 pour « unarmed » ou un nom inconnu), -1 sans nom. L'appelant le pose au chargement.
	int32 munition = -1;
};

// Ce que l'inventaire demande au reste du moteur : le type d'une arme (WeaponGetType) et les trois armes aux munitions
// sans fin (FUN_0045cdf0 rend toujours 1 : DAT_00bf4108 « marble », DAT_00bf4100 « supermarble », DAT_00bf4298 « RubBand »,
// trois modèles de la table des noms 0xa136e8). DAT_00bceaf2 (FUN_004251f0, l'astuce des munitions sans fin) vaut 0.
struct CMondeArmes {
	virtual const CTypeArme &Type(int32 arme) const = 0;
	int32 sansFin[3] = {-1, -1, -1};
	bool munitionsSansFin = false;   // DAT_00bceaf2
	virtual ~CMondeArmes(void) {}
};

// La charge de l'objet arme d'un piéton (son +0x128 ; l'objet arme est le +0x1d0 du piéton), que les pistes WeaponSetCharge et
// WeaponIncrementCharge mènent et que WeaponFire lit. Lu dans l'index le 06.11.2026.
struct CChargeArme {
	// _DAT_00906520 : la charge vide, que FUN_00664710 (le tir de WeaponFire, son emplacement +0x14) remet après chaque tir, et que
	// WeaponSetCharge pose quand l'arme est rangée, lâchée ou que le lance-pierre est gêné (Weapons.cat, SlingShotActions.cat).
	static constexpr float kVide = -100.0f;
	// FUN_0065e810 (la mise à jour de WeaponIncrementCharge, à chaque image avec son pas) : la charge gagne le pas, × 0,5
	// (_DAT_009055a0) si le modèle de l'objet arme (+0x10e) est le lance-pierre (DAT_00bf444c, « slingshot » dans la table des noms de
	// 0xa13858), × 1 sinon.
	static float Augmenter(float charge, float pas, bool lancePierre) { return (lancePierre ? 0.5f : 1.0f) * pas + charge; }
	// FUN_00661f10 (le lancement de WeaponFire) : la charge × le facteur +0x34 de la piste s'il est positif, puis bornée à [0, 1] (une
	// charge négative, ou NaN, donne 0), gardée en +0x44 pour le tir.
	static float AuTir(float charge, float facteur) {
		if(0.0f < facteur) charge *= facteur;
		if(!(0.0f <= charge)) return 0.0f;
		return 1.0f < charge ? 1.0f : charge;
	}
	// FUN_00664710 (le tir), les dégâts passés au projectile (FUN_004d4c40, son dixième argument ; lu dans l'index) : sans fiche de
	// projectile (+0x48 de la piste), 0 ; sinon ceux de la fiche, (+0x50 − +0x4c) × la charge + +0x4c ; si le flottant +0x2c (44) de
	// la piste WeaponFire n'est pas -1 (_DAT_009009ac), la charge × lui ; puis × la statistique 0x1f du tireur (FUN_00476f10 : la
	// colonne AG de pedstats.dat, « Damage_Scale », 100 dans les 75 entrées) / (float)100 (le double 0x900130).
	static float Degats(bool fiche, float sansCharge, float avecCharge, float charge, float degatsPiste, int32 echelle) {
		float d = fiche ? (avecCharge - sansCharge) * charge + sansCharge : 0.0f;
		if(degatsPiste != -1.0f) d = charge * degatsPiste;
		return ((float)echelle / 100.0f) * d;
	}
};

// La visée du tir (FUN_00662a40, le point visé du tir, et ce qu'il appelle).
struct CViseeTir {
	// FUN_00662a40 (ecx = la piste WeaponFire lancée : +0x3c son contexte, +0x40 le piéton, +0x44 la charge ; lu à l'assembleur de
	// 0x662a40 à 0x662b70) : d'où vient la cible du tir.
	// - Avec l'octet +0x31 (49) de la piste : l'objet visé du piéton (+0x438) et son point (+0x43c), que FUN_006626a0 prendra ;
	//   ailleurs, le point passé vaut -1. 4 des 81 pistes WeaponFire des .cat le portent, aucune des armes des sept missions.
	// - Sinon, une cible n'est cherchée que si le piéton n'est pas le joueur (le type +0x1310 n'est pas 0xd), si la visée automatique
	//   (+0x1ec0) est posée, si le bouton 10 de la manette ne vaut pas 0 (FUN_007386f0(10, 0), fcomp contre _DAT_008ff1f0 : 0 ;
	//   test ah,0x44 et jp : seule l'égalité refuse), si le mode du composant de visée (+8 de +0x1c8, FUN_00856820) vaut 2, ou si la
	//   caméra est une caméra de visée (le mode courant du gestionnaire, DAT_00c3ccf8 = 0xc3cc68 + 0x90, vaut 2 ou 0xd : ceux que
	//   les conditions CameraActive du nœud Controller du lance-pierre citent ; ces deux tests ne valent que pour le joueur, les octets
	//   [esp+0x10] et [esp+0x11]). Le joueur qui ne remplit aucune de ces conditions tire devant lui.
	// - La cible cherchée : +0x54c, prise une seule fois (FUN_00465690 désinscrit la référence, puis +0x54c = 0) ; sans elle, la cible
	//   du composant de visée (+0xa0, FUN_004d5980, ecx = +0x1c8) ; sans elle, FUN_004d7630 (CibleOuRepli).
	enum Source { SOURCE_AUCUNE, SOURCE_OBJET_VISE, SOURCE_54C, SOURCE_VISEE, SOURCE_REPLI };
	static bool CameraDeVisee(bool joueur, int32 modeCamera) { return joueur && (modeCamera == 2 || modeCamera == 0xd); }
	static Source
	SourceCible(bool joueur, int32 modeCamera, bool octet31, bool viseeAuto, float bouton10, int32 modeVisee, bool a54c, bool aVisee)
	{
		if(octet31) return SOURCE_OBJET_VISE;
		if(joueur && !viseeAuto && bouton10 == 0.0f && modeVisee != 2 && !CameraDeVisee(joueur, modeCamera)) return SOURCE_AUCUNE;
		if(a54c) return SOURCE_54C;
		return aVisee ? SOURCE_VISEE : SOURCE_REPLI;
	}
	// FUN_004d7630 (ecx = le composant de visée +0x1c8 ; lu dans l'index) : sa cible +0xa0 ; sans elle, la cible gardée +0xa8 ; sans
	// elle, +0x9c, puis le partenaire de prise du piéton (+0x155c de son +4), puis FUN_004d6dc0 (le choix dans la liste des candidats
	// +0x90, +0x98 entrées de 0x1c octets) ; ce choix devient la cible gardée (FUN_00465690 pour l'ancienne, FUN_00465680 pour la
	// nouvelle) et +0xc0 prend l'heure (DAT_00c1a9b4). -1 : pas de cible.
	static int
	CibleOuRepli(int a0, int &gardee, int a9c, int prise, int choisie)
	{
		if(a0 >= 0) return a0;
		if(gardee >= 0) return gardee;
		int c = a9c >= 0 ? a9c : prise >= 0 ? prise : choisie;
		gardee = c;
		return c;
	}
	// Le genre de la cible (FUN_00662a40, de 0x662b40 à 0x662b70, puis de 0x662bc5) : un piéton (les 3 bits bas de +0x108 à 3) passe par
	// son point visé (FUN_00662580) puis par les décalages et l'avance (FUN_00662760) ; une autre entité rend aussitôt le point de
	// FUN_006625f0 si sa méthode +0x14 rend vrai (FUN_007ffc70 pour CAutomobile ; FUN_00828240, xor al,al, pour CEntity, CObject,
	// CPed, CProjectile et les autres classes de rtti.json) ou celui de FUN_006626a0 (avec le point +0x43c ou -1) ; sans cible, le
	// point devant le tireur (FUN_00662180 avec une caméra de visée, FUN_0065ea50 sinon), puis FUN_00662760.
	enum Genre { GENRE_AUCUNE, GENRE_PIETON, GENRE_AUTOMOBILE, GENRE_AUTRE };
	// FUN_0065ea50(depart, portée) (ecx = la piste WeaponFire ; lu à l'assembleur de 0x65ea50 à 0x65eb39) : le point devant le tireur
	// sans cible, hors d'une caméra de visée (FUN_00662a40, de 0x662bc5 à 0x662c4f : la portée de l'objet arme, +0x124 + la charge +0x44
	// × (+0x120 − +0x124), 0,01 au moins, _DAT_0090054c). Le départ plus la portée × l'avant du tireur (FUN_00415350, ecx = le piéton
	// +0x40 ; la somme par FUN_00412850) ; si la portée dépasse 0,9 (fcomp contre le double 0x900dc0, test ah,0x41 : ni inférieure ni
	// égale), le z devient celui du tireur (le z de sa matrice, +0x14 → +0x38, ou +0xc sans matrice) : le tir part vers le sol à la
	// hauteur de ses pieds. Pour le joueur (+0x1310 = 0xd) sur un véhicule (+0x1554), s'y ajoute DAT_00b31cb8 × la vitesse du véhicule
	// (+0x124, +0x128, +0x12c) ; `vehicule` nul sans véhicule.
	static CVector
	PointDevant(const CVector &depart, const CVector &avant, float portee, float zTireur, const CVector *vehicule = nullptr, float facteur = 0.0f)
	{
		CVector r(depart.x + portee * avant.x, depart.y + portee * avant.y, depart.z + portee * avant.z);
		if((double)portee > 0.9) r.z = zTireur;   // la portée (un float) contre le double 0,9 : 0,9f (0,89999998) ne le dépasse pas
		if(vehicule){ r.x += facteur * vehicule->x; r.y += facteur * vehicule->y; r.z += facteur * vehicule->z; }
		return r;
	}
	// Les décalages de visée d'une piste WeaponFire sur une cible piéton : FUN_00662760 (lu à l'assembleur de 0x662760 à 0x6629c4 ;
	// Ghidra perd la somme en y), avant l'avance (FUN_0065ecb0). Si l'un des flottants +0x18, +0x1c, +0x20 (24, 28, 32) de la piste
	// n'est pas nul : v, l'écart du départ au point visé à plat (FUN_00414a20, z à 0) ; d, sa longueur (la racine, FUN_0085c4d0) ; s, le
	// côté (v × (0, 0, 1), FUN_004145e0, normé par FUN_00414cc0) ; le décalage vaut +0x18 × d × s + +0x1c × v + (0 × +0x20 (le double
	// 0x9009a0, nul), 0 × +0x20, +0x20). Si +0x24 (36) n'est pas nul et qu'il y a une cible, s'y ajoute +0x24 × l'avant de la cible (le
	// +0x10 … +0x18 de sa matrice, FUN_00415350). Le point visé gagne le décalage, en x, y et z.
	static CVector
	DecalageVisee(const CVector &depart, const CVector &cible, const CVector &avantCible, float c18, float c1c, float c20, float c24)
	{
		CVector o(0.0f, 0.0f, 0.0f);
		if(c18 != 0.0f || c1c != 0.0f || c20 != 0.0f){
			float vx = cible.x - depart.x, vy = cible.y - depart.y;
			float d = sqrtf(fabsf(vx * vx + vy * vy + 0.0f * 0.0f));
			float sx = vy, sy = -vx, n = sqrtf(sx * sx + sy * sy);
			if(n > 0.0f){ sx /= n; sy /= n; }
			o.x = c18 * d * sx + c1c * vx + (float)0.0 * c20;
			o.y = c18 * d * sy + c1c * vy + (float)0.0 * c20;
			o.z = c20;
		}
		if(c24 != 0.0f){ o.x += c24 * avantCible.x; o.y += c24 * avantCible.y; o.z += c24 * avantCible.z; }
		return CVector(cible.x + o.x, cible.y + o.y, cible.z + o.z);
	}
	// L'avance sur une cible qui bouge : FUN_0065ecb0 (lu à l'assembleur de 0x65ecb0 à 0x65f04a ; le pseudo-C de Ghidra perd l'ordre
	// des sommes, la pile x87 et le `pop edi` qui décale [esp] avant les deux dernières), appelée par FUN_00662760 pour une cible piéton
	// (FUN_00662a40 : le genre 3 de l'entité visée, +0x108 & 7) après le point visé (FUN_00662580). `depart` : le départ du tir ;
	// `cible` : le point visé ; `vitesse` : celle de la cible en m par pas ; `vitesseTir` : celle de la fiche (+0x60 + charge × (+0x64 −
	// +0x60), 0,4 sans fiche) ; `facteur` : le flottant +0x28 (40) de la piste WeaponFire ; `ficheT`, `fiche128`, `fiche84` : l'octet
	// +0x124, les flottants +0x128 et +0x84 de la fiche (faux, -, 1 sans fiche).
	// - d, la distance à plat (la racine du carré de l'écart en x et y, plus 0 × 0 : le z de l'écart est mis à 0) ;
	// - T = d / vitesseTir × facteur, la durée du vol en pas ; avec l'octet +0x124 de la fiche, T' = 0,5 T + 12,5 (les doubles 0x8ff1f8 et
	//   0x93c998) et, si T − T' est positif, T = √|(T − T')² / +0x128| + T' ;
	// - l'avance a = vitesse × T, puis a × (d / (vitesseTir − (a · u) / T)) / T, u la direction à plat normée (FUN_00414cc0) : la durée
	//   refaite avec la part de la vitesse de la cible le long du tir ; puis a × +0x84 de la fiche ;
	// - la cible plus a (x, y et z).
	// Une durée nulle (une cible à la verticale du départ) divise par zéro dans le moteur ; l'outil garde alors la cible.
	static CVector
	AvanceSurCible(const CVector &depart, const CVector &cible, const CVector &vitesse, float vitesseTir, float facteur, bool ficheT,
	               float fiche128, float fiche84)
	{
		float ex = cible.x - depart.x, ey = cible.y - depart.y;
		float d = sqrtf(fabsf(ex * ex + ey * ey + 0.0f * 0.0f));
		float t = d / vitesseTir * facteur;
		if(ficheT){
			float tp = (float)0.5 * t + (float)12.5;
			float e = t - tp;
			if(e > 0.0f) t = sqrtf(fabsf(e * e / fiche128)) + tp;
		}
		if(!(t > 0.0f) || d <= 0.0f) return cible;
		float ax = vitesse.x * t, ay = vitesse.y * t, az = vitesse.z * t;
		float ux = ex / d, uy = ey / d;
		float k = (d / (vitesseTir - (ay * uy + ax * ux + az * 0.0f) / t)) / t;
		ax *= k * fiche84; ay *= k * fiche84; az *= k * fiche84;
		return CVector(cible.x + ax, cible.y + ay, cible.z + az);
	}
};

// Les destinations forcées du tir d'un piéton : son composant +0x3bc (le même que PedSetProjDest prend en ecx, lu à l'assembleur de
// 0x5cd470 à 0x5cd4f1), trois places de (x, y, z) en +0x48, +0x54 et +0x60. Une place est vide quand son x vaut _DAT_0090fb98
// (1e11, le flottant 0x51ba43b7). Le constructeur du composant (FUN_0048c1b0, appelé par FUN_0047ffe0) met les trois places à zéro,
// puis leurs x à 1e11 ; FUN_0048c430 (appelée par FUN_00474f50, non suivie) ne vide que le x de la première. La visée du tir
// (FUN_00662a40, lu à l'assembleur de 0x662b7e à 0x662bc2 : ecx = +0x3bc du piéton du contexte, [+0x3c]+8) prend la première place
// avant tout point visé, sans décalage de visée ni avance (elle rend avant FUN_00662760). Lus à l'assembleur le 09.11.2026.
struct CDestinationsTir {
	static constexpr float kVide = 99999997952.0f;   // _DAT_0090fb98
	CVector place[3];
	CDestinationsTir() { Vider(); }
	// FUN_0048c1b0 (la part des destinations) : à zéro, puis les x à 1e11.
	void Vider() { for(CVector &v : place){ v = CVector(0.0f, 0.0f, 0.0f); v.x = kVide; } }
	// FUN_0048c430 : le x de la première seulement (les deux autres gardent ce qu'elles tenaient).
	void ViderPremiere() { place[0].x = kVide; }
	// PedSetProjDest (FUN_0048bd40, le nom de la liaison Lua 0x5cd470 ; lu à l'assembleur de 0x48bd40 à 0x48bd83) : la première place
	// dont le x vaut 1e11 (fcom, test ah,0x44, jnp : l'égalité arrête la recherche) reçoit le point ; trois places pleines, rien.
	bool Poser(float x, float y, float z) {
		for(CVector &v : place) if(v.x == kVide){ v = CVector(x, y, z); return true; }
		return false;
	}
	// FUN_0048bd90 (0x48bd90 à 0x48bda8) : une destination en attente, le x de la première n'est pas 1e11.
	bool EnAttente() const { return place[0].x != kVide; }
	// FUN_0048bdb0 (0x48bdb0 à 0x48bdf2) : rend la première ; la deuxième et la troisième avancent d'une place, et seul le x de la
	// troisième redevient 1e11 (ses y et z restent).
	CVector Prendre() {
		CVector r = place[0];
		place[0] = place[1];
		place[1] = place[2];
		place[2].x = kVide;
		return r;
	}
};

// Les réglages du prochain tir : des globales que les pistes de l'arme posent avant WeaponFire, que le tir (FUN_00664710) lit et
// que FUN_0065d160 remet à zéro (à la fin de chaque tir, quand le lancement de WeaponFire, FUN_00661f10, ne trouve pas d'arme en
// main, et dans FUN_00662d30, WeaponFireSpecificOrientation). Lu dans l'index le 06.11.2026 ; les lancements gardent le contexte
// de la piste en +0x24 et rendent vrai, leur mise à jour (FUN_006094f0) rend faux.
struct CReglagesTir {
	// DAT_01b65c30 … DAT_01b65c38 : WeaponFireOffset (WeaponFireOffsetTrack, table 0x935cbc, créateur 0x643070 ; lancement
	// FUN_0065d3f0 : les flottants +0x18, +0x1c et +0x20 de la piste, 24, 28 et 32 dans le fichier). Le point visé sur une cible
	// (FUN_00662380) : de côté (× le vecteur droit du tireur), devant (× son vecteur avant) et en hauteur (× la hauteur de la cible).
	float decalage[3] = {0.0f, 0.0f, 0.0f};
	// DAT_01b65c3c … DAT_01b65c44 : WeaponFireStartOffset (table 0x935cd4, créateur 0x6430b0 ; lancement FUN_0065d430, les mêmes
	// champs). Le départ du projectile (FUN_0065e880) : ce vecteur tourné par la matrice du tireur (FUN_0046dbd0, FUN_0041a850) et
	// ajouté à la douille ou à l'os de l'arme.
	float depart[3] = {0.0f, 0.0f, 0.0f};
	// DAT_01b65c48 … DAT_01b65c50 : WeaponFireRotation (table 0x935ca4, créateur 0x643030 ; lancement FUN_0065d350) : trois angles
	// en degrés, rangés en radians (× (float)π, le double 0x900160, / 180, le double 0x900158), que le tir passe au projectile
	// (FUN_004d4c40, ses 18e à 20e arguments).
	float rotation[3] = {0.0f, 0.0f, 0.0f};
	// DAT_00b31c9c : la munition tirée, -1 pour celle de l'objet arme (+0x12c) ; DAT_01b65c14 et DAT_01b65c24 … DAT_01b65c2c : remis
	// aussi, posés par d'autres pistes (non suivies).
	int32 munition = -1;
	void PoserDecalage(float cote, float devant, float haut) { decalage[0] = cote; decalage[1] = devant; decalage[2] = haut; }
	void PoserDepart(float x, float y, float z) { depart[0] = x; depart[1] = y; depart[2] = z; }
	void PoserRotation(float x, float y, float z) {
		float pi = 3.1415927f;   // (float) du double 0x900160
		rotation[0] = x * pi / 180.0f; rotation[1] = y * pi / 180.0f; rotation[2] = pi * z / 180.0f;
	}
	void Remettre(void) { *this = CReglagesTir(); }   // FUN_0065d160
	// FUN_00662380, la part du décalage (lue à l'assembleur de 0x66244b à 0x66252a) : le point de la cible (FUN_004d5eb0, le composant de
	// visée du tireur) monte de la hauteur de la cible (l'emplacement +0x6c de sa vftable) × decalage[2], avance de decalage[1] × le
	// vecteur avant du tireur (FUN_00415350 : la deuxième ligne de sa matrice, +0x10 ; sans matrice (-sin cap, cos cap, 0)) et se
	// décale de decalage[0] sur le côté : x + decalage[0] × avant.y, y − decalage[0] × avant.x, z + decalage[0] × 0 (le double
	// 0x9009a0) ; FUN_0065eb40 ajoute ensuite son terme (non suivi).
	CVector Viser(const CVector &point, float hauteurCible, const CVector &avant) const {
		CVector v = point;
		v.z = hauteurCible * decalage[2] + v.z;
		v.x += avant.x * decalage[1]; v.y += avant.y * decalage[1]; v.z += avant.z * decalage[1];
		v.x = v.x + decalage[0] * avant.y;
		v.y = -avant.x * decalage[0] + v.y;
		v.z = v.z + decalage[0] * 0.0f;
		return v;
	}
	// FUN_0065e880, la part du départ (lue de 0x65e9aa à 0x65ea4b) : le décalage de départ tourné par la matrice du tireur (droite,
	// avant, haut) et ajouté au point de départ.
	CVector Partir(const CVector &point, const CVector &droite, const CVector &avant, const CVector &haut) const {
		return CVector(point.x + droite.x * depart[0] + avant.x * depart[1] + haut.x * depart[2],
		               point.y + droite.y * depart[0] + avant.y * depart[1] + haut.y * depart[2],
		               point.z + droite.z * depart[0] + avant.z * depart[1] + haut.z * depart[2]);
	}
};

struct CEntreeInventaire {
	int16 munitions = 0;          // +0
	int16 arme = -1;              // +2
	int32 drapeaux[8] = {};       // +4 + 4n : les drapeaux de l'arme (SetWeaponFlags, la condition WeaponFlags)
	int proprietaire = -1;        // +0x24 : l'inventaire qui la tient ; ici, le piéton
};

class CInventaireArmes
{
public:
	static const int kTaille = 57;
	CEntreeInventaire entrees[kTaille];
	bool libre[kTaille];          // l'octet 0x80 du pool
	int suivante = 0;             // +0x14 du pool : d'où part la recherche d'une place
	CInventaireArmes(void) { for(bool &b : libre) b = true; }

	// FUN_0045cb70 (ecx = l'inventaire) : la première entrée prise de cette arme et de cet inventaire, -1 sinon.
	int Trouver(int ped, int32 arme) const;
	// FUN_0045c980 : une place libre depuis +0x14 (elle avance, un seul retour à 0), vidée ; -1 si le pool est plein.
	int Prendre(void);
	// FUN_0045ce40 : l'entrée libérée (l'octet 0x80), +0x14 ramené dessus s'il était plus loin.
	void Liberer(int i);
	// FUN_0045c890 (ecx = l'entrée) : les munitions bornées à [0, le maximum du type] (le réel ramené en entier, 0x85c6f0) ; pour le
	// joueur, les munitions gagnées de quatre armes vont à une statistique (FUN_00458330 : 0x6d « cherryb », 0x70 « potato »,
	// 0x73 « eggproj », 0x76 « brocket »), et avec l'octet le HUD l'annonce (FUN_00540fc0, puis FUN_0055c1f0) : non suivis.
	void Poser(const CMondeArmes &m, int i, int32 n);
	// FUN_0045cbc0 : une arme sans +0x64 et d'un autre genre que 1 ne se range pas (pour le joueur, avec l'octet, le HUD
	// l'annonce : FUN_00540fc0) ; sinon une place (FUN_0045c980), l'arme, 0 munition, l'inventaire, puis Poser(n).
	bool Ajouter(const CMondeArmes &m, int ped, int32 arme, int32 n);
	// FUN_0045ccb0 (GiveWeaponToPlayer) : absente, ou avec `force`, l'arme est ajoutée avec 1 ; puis le type (FUN_00728150)
	// et, s'il a des munitions (+0x84 > 0), elles reçoivent le maximum de leur propre type (ajoutées, ou posées si elles y
	// sont déjà, FUN_0045c890).
	void Donner(const CMondeArmes &m, int ped, int32 arme, bool force = false);
	// FUN_0045cd80 (GiveAmmoToPlayer) : absente, l'arme est ajoutée avec n ; sinon ses munitions + n.
	void AjouterMunitions(const CMondeArmes &m, int ped, int32 arme, int32 n);
	// FUN_0045cdf0 : 1 pour les trois armes sans fin ; 0 si l'arme manque ; ses munitions sinon.
	int32 Munitions(const CMondeArmes &m, int ped, int32 arme) const;
	// FUN_0045cee0 : rien pour -1 ; absente, ajoutée avec n ; sinon (sans l'astuce, ou n ≥ 0) ses munitions + n, et
	// l'entrée libérée sous 1.
	void Changer(const CMondeArmes &m, int ped, int32 arme, int32 n);
	// FUN_0045cf70 : présente, sans l'astuce (ou n < 1), d'un type sans fiche ou au maximum différent de -1 : ses munitions
	// − n (Poser), et l'entrée libérée sous 1 (pour le joueur, « RBandBall », DAT_00bf432c, passe par FUN_0049abb0 à la
	// place : non suivi, l'entrée reste).
	void Retirer(const CMondeArmes &m, int ped, int32 arme, int32 n, bool joueur = false, int32 rbandball = -1);
	// FUN_0045d0f0 (ItemSetCurrentNum sur une arme) : présente, ses munitions posées à n (FUN_0045c890), l'entrée libérée sous 1 ;
	// rend l'entrée, -1 si l'arme manque (rien n'est ajouté).
	int PoserMunitions(const CMondeArmes &m, int ped, int32 arme, int32 n);
	// Les entrées d'un piéton, libérées (le piéton supprimé, ou PedClearAllWeapons).
	void Vider(int ped);
	// FUN_00486130 (ecx = le composant +0x668 du piéton, son +0 le piéton) : les munitions d'une arme pour le piéton ; 0 sous 1 ; le
	// type (WeaponGetType ; 0 sans type) et sa munition (+0x84 ; FUN_00728150 résout avant la fiche du projectile) : s'il en a une
	// (positive), les munitions de celle-ci (FUN_0045cdf0) ; sinon celles de l'arme, et 1 si elle n'en a plus mais que le piéton la
	// tient (`tenue`, le +0x10e de son objet arme). Lu dans l'index.
	int32 MunitionsPour(const CMondeArmes &m, int ped, int32 arme, int32 tenue) const;
	// FUN_00486ae0 (la condition HasRangedWeapon, FUN_00605a80, sur le composant +0x668 du piéton du contexte) : vrai si l'arme tenue
	// (FUN_00474aa0 : le +0x10e de l'objet arme n'est pas -1) a l'octet +0x68 de son type (FUN_004d9560 : le type +0x264 de l'objet arme)
	// et des munitions (FUN_00486130) ; sinon, en parcourant les entrées du piéton dans l'ordre du pool (FUN_007db5a0, FUN_0045cc60 :
	// l'octet 0x80 libre et l'inventaire +0x24), la première arme d'au moins 1 dont le type a l'octet +0x68 sans le +0x69 et qui a des
	// munitions ; faux sinon. Lu dans l'index et à l'assembleur (0x605a80 : ecx = +0x668 du piéton +8 du contexte).
	bool ArmeADistance(const CMondeArmes &m, int ped, int32 tenue) const;
	// Le drapeau n de l'entrée de cette arme (FUN_0045cb70 la trouve, FUN_0045cb40 la rend : son mot +4 + 4n). PoserDrapeau : la part
	// de SetWeaponFlags (FUN_00661d40) sur l'inventaire, rien si l'arme manque. LireDrapeau : la part de WeaponFlags (FUN_0065bb20) ;
	// faux dans `trouve` si l'arme manque (la condition lit alors l'objet arme). n hors de [0, 7] : rien, faux.
	void PoserDrapeau(int ped, int32 arme, int n, int32 valeur);
	bool LireDrapeau(int ped, int32 arme, int n, bool &trouve) const;
};

// Les objets du joueur (les modèles item de 0xa136b8 à 0xa136bc : les transistors, les cartes, les boîtes de chocolats…) : un
// tableau de 80 compteurs en DAT_00cf0060, hors de l'inventaire des armes. L'indice d'un modèle (lu à l'assembleur de 0x541ae0 à
// 0x541ce1) : le modèle lui-même s'il vaut au plus 0x4f, sinon le modèle moins le premier item (DAT_00a136b8) s'il tombe sous 0x50,
// sinon la case 0 (comparaisons non signées : un modèle négatif lit et écrit la case 0).
class CObjetsJoueur
{
public:
	static const int kTaille = 0x50;
	int32 nombres[kTaille] = {};
	int32 premier = 0;            // DAT_00a136b8 (CModelInfo::ms_firstItemId)
	// Les quatre modèles que FUN_00541ae0 traite à part (la table des noms 0xa136e8) : « ChocBox » (DAT_00bf427c, la statistique
	// 0x25), « FlowerGift » (DAT_00bf4278, 0x27), « ticket » (DAT_00bf42a4, 0xe6) et « BigGulp » (DAT_00bf429c : jamais compté).
	int32 chocolats = -1, fleurs = -1, ticket = -1, bigGulp = -1;
	int Case(int32 modele) const;
	// FUN_00541cb0 (ItemGetCurrentNum) : le compteur du modèle.
	int32 Nombre(int32 modele) const { return nombres[Case(modele)]; }
	// FUN_00541c10 (ItemSetCurrentNum) : n borné à [0, le maximum du type] (+0x34 de WeaponGetType, l'arrondi 0x85c6f0) ; le HUD
	// annonce l'ancien et le nouveau compte (FUN_00540fc0, non suivi). Rend l'ancien compte.
	int32 Poser(const CMondeArmes &m, int32 modele, int32 n);
	// FUN_00541ae0(modèle, n, annonce, x) : le BigGulp n'est pas compté (FUN_00447280(1, 0, 1) sur le joueur avec DAT_00c1af4c, puis
	// le HUD) ; les trois autres modèles à part vont d'abord à leur statistique (FUN_00458330, non suivie) ; puis l'ancien compte + n
	// borné à [0, le maximum] ; avec l'annonce, le HUD (non suivi). Rend vrai si le compteur a été écrit.
	bool Ajouter(const CMondeArmes &m, int32 modele, int32 n);
};
