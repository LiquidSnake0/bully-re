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
