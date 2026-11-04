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

struct CEntreeInventaire {
	int16 munitions = 0;          // +0
	int16 arme = -1;              // +2
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
	// Les entrées d'un piéton, libérées (le piéton supprimé, ou PedClearAllWeapons).
	void Vider(int ped);
};
