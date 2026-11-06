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
#include <functional>
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

// Les cinq points de synchronisation d'un objet (FUN_00520bf0 range leurs noms hachés en DAT_00c8e8e8, FUN_00520230 les rend,
// FUN_00519070 leur nombre ; les noms de FUN_00488a80, la table 0x90ea6c).
extern const char *const kPointsDeSynchro[5];   // FeetSync, FeetSyncTop, FeetSyncBottom, FeetSyncLeft, FeetSyncRight

class CObjetsAnimes
{
public:
	// Le relevé des objets proches d'un piéton (FUN_004991e0 → FUN_004989f0 → FUN_00497e90 pour un objet animé), dans la branche
	// des piétons de l'IA (la branche du joueur, FUN_00497ce0 et la visée, n'est pas suivie).
	struct Releveur {
		CVector pos;
		bool joueur = false;
		bool parScript = false;     // l'octet +0x1570 à 2 : créé par un script
		bool femme = false;         // l'octet +0x50 de son modelinfo
	};
	struct Repere { int objet = -1; uint32 sync = 0; float distance = 0.0f; };
	// FUN_004989f0 (réduit) : les objets animés à moins de 10 m en x et en y (les secteurs de _DAT_00900140 autour du piéton ;
	// l'outil prend la boîte, sans les secteurs) qui offrent une action aux piétons (+0x230) et n'ont pas le drapeau 22 (+0x1b8).
	// Pour chacun, FUN_00497e90 : écarté si le drapeau 11 (+0x18c) est posé et que le piéton n'est pas créé par un script, si la
	// fiche ne l'ouvre pas aux piétons (+0x7c), aux garçons (+0x7d) ou aux filles (+0x7e), ou si l'objet joue un nœud nommé
	// NotUseable (FUN_005f46a0 sur son contrôleur, DAT_01b652c8 ; `inutilisable`) ; sinon ses points de synchronisation un par un
	// (FUN_00467c30) : plus loin que `portee` (ou la portée de la fiche, +0x78, sans portée) ou que le meilleur, il entre dans les
	// proches (FUN_0048c6b0 : dix au plus, un par modèle, le plus près) ; sinon il devient le meilleur. Supposés : les zones
	// interdites (FUN_0046ee60), FUN_004ea2b0, le +0x138 de l'objet, les modèles DAT_00bf4460 et DAT_00bf4140, la vue
	// (FUN_00437b40, FUN_00497cc0, FUN_00496850 et son cône de 60°) ; le seul point lu est FeetSync, à la position de l'objet (le
	// point du modèle n'est pas lu).
	Repere Relever(const Releveur &p, float portee, const std::function<bool(int)> &inutilisable, std::vector<Repere> &proches) const;
	// FUN_004979b0 (lu dans l'index) : l'objet visé parmi le repéré puis les proches. Le repéré d'abord : un objet dont le genre
	// (+0x80 de sa fiche, FUN_005fa710) n'est pas LOCKER (DAT_01b65518) est écarté s'il a le drapeau 8 (+0x180, verrouillé aux
	// piétons ; le drapeau 7, +0x17c, ne compte que pour DAT_00c1aea8, non suivi) ; puis à moins de `portee` (0 : sans borne), pas
	// NotUseable, du modèle voulu (-1 : tous) et du genre voulu (FUN_00467a30 : le +0x80 de sa fiche ; 0 : tous). Puis, avec une
	// portée, les proches dans l'ordre, plus près que la portée, du modèle voulu et du genre voulu (comparé même nul). Le genre
	// DAMAGEABLE (DAT_01b654d4) et l'emplacement +0x7c de l'objet ne sont pas suivis. Rend l'objet et son point, -1 sinon.
	Repere Choisir(const Repere &repere, const std::vector<Repere> &proches, uint32 genre, float portee, int32 modele,
	               const std::function<bool(int)> &inutilisable) const;
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
	// Les portes animées, sans piéton (les scripts : PAnimOpenDoor, PAnimCloseDoor, AreaSetDoorOpen ; lus dans l'index le 06.11.2026).
	// FUN_004eebc0(octet, piéton, immédiat) : rien (faux) si la porte est déjà ouverte (le drapeau 9, +0x184) ; les droits du piéton
	// (les drapeaux 7 pour le joueur, 8 pour les autres, l'octet +0x1d du contexte, les modèles DAT_00bf42cc … DAT_00bf42dc, son
	// objectif) ne comptent pas sans piéton ; puis, si elle ne joue pas déjà Opening (FUN_005f46a0(&DAT_01b652d0, 1, 1)) et que le
	// nœud /Global/Door/DoorFunctions/Opening/ (ou, immédiat, /Global/Door/NotUseable/Opened/Immediate/ : la chaîne de 0xa76b58 ou
	// 0xa76b5c) existe (`noeud`) : les drapeaux 0 et 15 (+0x160, +0x19c) à l'octet, le drapeau 9 à 1, puis FUN_005b2df0 et la porte
	// joue le nœud (FUN_004edec0, que l'appelant fait) ; immédiat, +0x3b0 à 1 et l'emplacement +0x4c ; sans genre à la fiche (+0x80
	// nul), +0x20 et +0x3c à 0 (non suivis, ni FUN_005b2df0). Rend vrai sauf pour une porte déjà ouverte.
	bool OuvrirPorte(int i, uint8 octet, bool noeud, bool ouvreDeja);
	// FUN_004eed70(piéton) : rien (faux) si la porte est fermée (le drapeau 9 nul) ; puis, si elle ne joue pas Closing
	// (DAT_01b652d4) et que /Global/Door/DoorFunctions/Closing/ (0xa76b60) existe : les drapeaux 0, 15 et 9 à 0, FUN_005b3030 et le
	// nœud joué (sans genre, +0x20 et +0x3c à 1 et FUN_004ee540 : non suivis). Rend vrai sauf pour une porte fermée.
	bool FermerPorte(int i, bool noeud, bool fermeDeja);
	void Vider(void) { objets.clear(); }
	// La fiche PropButes d'un modèle (le +0x70 de son modelinfo, FUN_00520080), que l'appelant fournit ; nil sans fiche.
	std::function<const CFicheObjetAnime *(int32)> fiche;
	std::vector<CObjetAnime> objets;   // le pool DAT_00c0f608
private:
	int Creer(const Declencheur &d);   // FUN_0044cd20
};
