// Les objets animés (PAnim) que les déclencheurs portent : les casiers, les poubelles, les cloches… Un déclencheur dont le
// périmètre a un PLACEMENT (le modèle, FUN_006d5450 : +0x10 du périmètre, résolu par FUN_0051c240) reçoit un objet de
// 0x3b8 octets (FUN_004eb030, le constructeur FUN_004ed9d0), rangé dans le pool DAT_00c0f608 et en +0x1c du déclencheur.
//
// Quatre chemins y mènent dans bully.exe (lus dans l'index et à l'assembleur le 06.11.2026) :
// - la scène de la zone visible : FUN_00416ae0 (le monde vidé à l'arrivée d'un changement de zone, l'état 9 de FUN_00416e30)
//   retire d'abord les objets des autres zones (FUN_00416490), puis CStreaming::LoadScene (FUN_005343c0) appelle FUN_00416c70
//   → FUN_00416540(&DAT_00bd1008) : pour chaque déclencheur du pool, de la case 1 à la dernière (FUN_006d6f50, ecx = 0x20c7e98),
//   sans objet (+0x1c), pas propre à une mission (bit 0 de +0x3f), avec un modèle, de la zone visible (+0x3c), hors de
//   l'emplacement 5 de DATLoad (+0x3d : les portes et les tags), et dont la fiche du modèle (+0x70 du modelinfo, FUN_00520080)
//   admet la saison (ses octets +0x74, +0x75, +0x77, +0x76 contre DAT_00a147ec[DAT_00a147e0] = 0, 1, 3, 2) : FUN_0044cd20 crée
//   l'objet à la position (+4) et aux angles (+0x10 moins le lacet +0x38, +0x14, +0x18) du déclencheur, le range en +0x1c et pose
//   son drapeau 13 au bit 3 de +0x3f (un nom en DT_) ;
// - la section prop des .ipb (PropLoaderImpl, FUN_00438d90) : le déclencheur trouvé par son nom (FUN_006d78c0), au bit 0x10 de
//   +0x3f (que le constructeur du déclencheur FUN_006d6920 pose toujours) ; FUN_004efe60 ;
// - la méthode 8 de TriggerLoaderImpl (FUN_00436e00, les déclencheurs des .ipb) : les mêmes conditions que FUN_00416540 ;
// - AreaRegisterAnimProps (0x438700 ; en Lua 0x5b6d60) parcourt la table DAT_00bd3e78 (12 octets par entrée) jusqu'à
//   DAT_00bd5c54 : vérifié à l'assembleur, ce compte n'est écrit qu'en 0x416b07 (remis à 0 par FUN_00416ae0) et seulement lu en
//   0x438703 et 0x43877c ; la table reste vide et la fonction ne fait rien.
// L'outil suit le premier chemin, celui des déclencheurs des .dat (DAT/Trigger.img) ; les .ipb ne posent pas encore d'objets.
//
// La case 0 du pool des déclencheurs tient lieu de « pas de déclencheur » (FUN_0044cd20 et FUN_00416490 la comparent à l'objet) ;
// dans l'outil, c'est le premier déclencheur de tags.DAT, d'emplacement 5, qu'aucun chemin ne prend.
#pragma once
#include "../common.h"
#include <map>
#include <string>
#include <vector>

struct CActionTrack;
struct CActionNode;

// La fiche PropButes d'un modèle animé (le +0x70 de son modelinfo). FUN_00520460 (l'initialisation du modelinfo) la cherche
// dans l'arbre des réglages sous Props/Info (PropGlbl.cat), en profondeur (FUN_0072bd60 → FUN_0072ada0 : le premier nœud du nom
// du modèle qui porte une piste PropButes) ; à défaut, Props/Info/default (ou defaultProxy avec l'octet +0x8e). Le
// constructeur de la fiche (FUN_00729b70) pose les valeurs par défaut : les quatre saisons à 1, +0x78 à 2 (_DAT_00900bfc),
// +0x7c à 0, +0x7d et +0x7e à 1.
struct CFicheObjetAnime {
	bool lue = false;
	std::string racine, fichier, depart;            // +0x68 (104), +0x6c (108), +0x70 (112) : l'arbre de l'objet
	bool saisons[4] = {true, true, true, true};     // +0x74 … +0x77 (116 … 119)
	float distance = 2.0f;                          // +0x78 (120) : la portée d'usage quand la condition n'en donne pas
	bool pnj = false, garcons = true, filles = true; // +0x7c, +0x7d, +0x7e (124 … 126) : qui peut s'en servir
	uint32 genre = 0;                               // +0x80 (128) : le HashString du genre (LOCKER, door…)
	// Une piste PropButes lue (CActionBank::ButesCherche) : les attributs par position.
	static CFicheObjetAnime Lire(const CActionTrack &t, const std::map<uint16, std::string> &chaines);
};

enum { OBJET_ANIME_DRAPEAUX = 32 };   // les mots +0x160 + 4n (l'outil en garde 32 : jusqu'à +0x1dc, avant l'arbre +0x228)

struct CObjetAnime {
	bool libre = true;
	int32 modele = -1;          // +0x10e
	int declencheur = -1;       // le déclencheur qui le tient (son +0x1c)
	CVector pos;                // la position du déclencheur (FUN_0044cd20, FUN_00412920)
	float lacet = 0.0f;         // le YAW du déclencheur, en degrés
	int8 zone = 0;              // +0x111 : l'AREACODE du déclencheur
	uint8 octet3a8 = 1;         // +0x3a8 : le dernier argument de FUN_0044cd20 (1 depuis FUN_00416540)
	// FUN_004ea300 : le mot +0x160 + 4n reçoit l'octet ; 4 (+0x170) et 5 (+0x174) inscrivent l'objet dans deux listes
	// (FUN_005b2bb0, FUN_005b2970) quand ils passent de 0 à autre chose. 8 (+0x180) l'objet verrouillé aux piétons, 9 (+0x184)
	// ouvert (les portes), 10 (+0x188) remis à 0 quand un piéton s'en sert (FUN_00656120), 13 la porte en DT_, 22 (+0x1b8) l'objet
	// écarté du relevé des piétons (FUN_004989f0).
	int32 drapeaux[OBJET_ANIME_DRAPEAUX] = {};
	int utilisateur = -1;       // +0x234 : le piéton qui s'en sert (FUN_004ed6e0)
	// +0x22c : le fichier de son arbre (FUN_00520810 en donne l'indice ; l'outil garde le nom), celui de sa fiche à la création ;
	// +0x230 : le nœud que l'objet fait jouer au piéton qui s'en sert (la piste PropSetPedAction, FUN_006561a0).
	std::string fichier;
	const CActionNode *actionPieton = nil;
	uint16 generation = 0;
};

class CObjetsAnimes
{
public:
	// Un déclencheur tel que FUN_00416540 le juge.
	struct Declencheur {
		int handle = -1;
		bool mission = false;     // ISMISSIONSPECIFIC, le bit 0 de +0x3f
		int32 modele = -1;        // le PLACEMENT de son périmètre résolu (-1 : vide ou inconnu)
		int zone = 0;             // +0x3c
		int emplacement = 0;      // +0x3d : l'emplacement de DATLoad
		bool porte = false;       // le bit 3 de +0x3f : le nom commence par DT_ ou _DT_ (FUN_006d6650)
		CVector pos; float lacet = 0.0f;
		const CFicheObjetAnime *fiche = nil;   // celle de son modèle ; nil : aucune (la saison passe)
	};
	// FUN_00416540 : les objets de la zone `zone` créés, dans l'ordre du pool des déclencheurs (la case 0 sautée) ; rend les
	// indices des objets créés. `saison` : DAT_00a147ec[DAT_00a147e0].
	std::vector<int> CreerPourZone(int zone, const std::vector<Declencheur> &declencheurs, int saison = 0);
	// FUN_00416490 : les objets d'un déclencheur hors de `zone`, qui n'est ni propre à une mission ni de l'emplacement 5, sont
	// retirés (leur déclencheur perd son +0x1c). Rend les déclencheurs touchés.
	std::vector<int> RetirerHorsZone(int zone, const std::vector<Declencheur> &declencheurs);
	// Le déclencheur est libéré (DATUnload) : son objet part avec lui.
	void RetirerDuDeclencheur(int declencheur);
	// Le +0x1c d'un déclencheur : l'indice de son objet, ou -1.
	int ObjetDe(int declencheur) const;
	CObjetAnime *Objet(int i) { return i >= 0 && i < (int)objets.size() && !objets[i].libre ? &objets[i] : nil; }
	const CObjetAnime *Objet(int i) const { return i >= 0 && i < (int)objets.size() && !objets[i].libre ? &objets[i] : nil; }
	int Nombre(void) const { return (int)objets.size(); }
	// FUN_004ea300 et PAnimGetPropFlag (0x5c5610 : le mot +0x160 + 4n non nul).
	void PoserDrapeau(int i, int n, bool v);
	bool Drapeau(int i, int n) const;
	void Vider(void) { objets.clear(); }
	std::vector<CObjetAnime> objets;   // le pool DAT_00c0f608
private:
	int Creer(const Declencheur &d);   // FUN_0044cd20
};
