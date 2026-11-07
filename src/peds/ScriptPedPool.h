// Les piétons vus par les scripts : un pool de 24 (ms_pPedPool, docs/pools.md), l'entrée 0 étant
// le joueur (lua_PlayerGetPedIndex, 0x5d5260, renvoie 0). Un handle est l'index dans le pool ;
// -1 quand le pool est plein (FUN_005cf6b0 rend -1, que PedCreatePoint transmet au script).
// Chaque piéton créé par un script est noté à son objet script (FUN_005d94d0 ; PedDelete le
// retire, FUN_005d9520) : c'est ce qui permet de ramasser ce qu'une mission a laissé.
// Ici, le piéton n'a qu'un modèle, une position, un cap, une santé, et un geste à la fois :
// aller quelque part (PedMoveToPoint / PedMoveToXYZ) ou attaquer quelqu'un (PedAttack) ; pas de
// monde, pas d'animation, pas de collision : c'est ce qu'il faut à lancer_lua pour que les
// scripts avancent et voient les piétons bouger, arriver et mourir.
#pragma once
#include "../common.h"
#include <vector>

enum { SCRIPT_PED_POOL_SIZE = 24, SCRIPT_PED_JOUEUR = 0 };
// Les drapeaux de +0x1314 (CScriptPed::drapeaux) : jusqu'à 123 dans les arbres d'actions relevés.
// Les drapeaux d'un piéton : 0x89 mots en +0x1314 (PedSetFlag, 0x5cc0b0, n'écrit que sous 0x89).
enum { SCRIPT_PED_DRAPEAUX = 0x89, DRAPEAU_INTOUCHABLE = 12, DRAPEAU_INVULNERABLE_JOUEUR = 20, DRAPEAU_INVULNERABLE = 30 };

// Le deuxième argument de PedMoveToPoint / PedMoveToXYZ dans les scripts : 0 et 1 dans LibPed
// et les missions pour marcher, 2 pour courir, 3 pour sprinter (1_01 : `PedMoveToPoint(k, 3, …)`
// pour les deux élèves qui courent). Les vitesses sont des ordres de grandeur, à mesurer sur
// le jeu (docs/missions.md).
enum eScriptPedAllure { ALLURE_MARCHE = 0, ALLURE_MARCHE_VITE = 1, ALLURE_COURSE = 2, ALLURE_SPRINT = 3 };
float ScriptPedVitesse(int allure);   // m/s
// FUN_005c6180 : l'allure d'un script (le deuxième argument de PedMoveToXYZ, PedMoveToPoint…) en vitesse d'intention de
// mouvement : 0 → 0,3 (marche), 1 → 0,7 (course), 2 → 1,1 (sprint), 3 → 1,5, 4 → -1 ; une autre valeur, ou pas
// d'argument (FUN_005d8240 rend -1), la vitesse par défaut de l'appelant (0,7 pour PedMoveToXYZ, _DAT_00900e00).
float ScriptPedIntentionVitesse(int allure, float defaut = 0.7f);

// FUN_00625970 : la vitesse que le pas d'une piste de locomotion de l'IA (CombatLoco, FUN_0062ea40) passe à la navigation
// (FUN_004a4eb0, qui en fait la vitesse de l'intention de mouvement), selon le mode que FUN_0061b430 a rangé (+0x24) avec
// le point visé : 2 et 4 (rester sur place) 0 ; 12 FUN_0061b820 : t = (d² au point +0x60 − AIButes +0x2c) × 0,04, borné à
// [0, 1], puis 1 − (1 − t)⁸ + 0,5 ; 7 et 9 FUN_0061b8e0 : min(d² au point +0x2c / 64, 1) × 1,3 + 0,2 ; tout autre mode
// 1,5 (_DAT_0090fb9c). L'approche de CombatLoco (FUN_0062c740, hors de l'image de tirage) est le mode 11 vers la cible,
// ou 10 sur place une fois dans le petit rayon (+0x6c) : 1,5, la vitesse du sprint de FUN_005c6180.
enum { PAS_SUR_PLACE = 2, PAS_ATTENDRE = 4, PAS_APPROCHE_7 = 7, PAS_APPROCHE_9 = 9, PAS_ARRIVE = 10, PAS_VERS_CIBLE = 11, PAS_RALENTI = 12 };
float ScriptPedVitesseDuPas(int mode, float d2Cible, float d2Point, float rayonButes2c);
// La vitesse que la navigation donne à l'intention de mouvement (FUN_005e4c60 → FUN_005e4a60), une fois le but posé par
// FUN_004a4eb0 (le navigateur est en +0xdac du piéton : la vitesse demandée en +0x534, le rayon d'arrivée en +0x528, que
// FUN_005e09c0 recopie en +0x450 si le piéton n'est pas immobile, l'octet +0x4e4 de PedSetStationary 0x5c7950 et de la
// piste SetStationary FUN_00626920). 0 si le piéton est immobile, si la vitesse demandée est sous 0,1 (le double
// 0x900128) ou s'il est arrivé (FUN_005e2fb0 : à plat, à moins du rayon d'arrivée du but ; FUN_004a4eb0 pose alors 0
// lui-même). À moins de 3 m du point (d² < 9, 0x900e04), elle dépend de l'écart de cap entre le devant du piéton
// (FUN_00415350) et la direction du point (FUN_004331c0, FUN_00432830) : jusqu'à π (le double 0x900160), la vitesse
// × cos(écart) (FUN_0085ad50, le _CIcos du CRT), × d / 0,3 sous 0,3 m (d² < 0,09, 0x90cfc8 ; FUN_0040df70, le double
// 0x900d78), au moins 0,1 (0x900170, FUN_00414340 = max) ; au-delà, 0. Lu à l'assembleur (0x5e4a60 … 0x5e4c4f).
// Non suivis : les points du chemin (+0x508, +0x53f), FUN_00473b90 et FUN_005e2720 qui sautent le ralenti, l'appui
// qu'FUN_005e4c60 simule (FUN_005f3930(4)) à l'octet +0x551.
float ScriptPedVitesseDuTrajet(float vitesse, float d2Point, float ecartCap, float rayonArrivee, bool immobile = false);

// Le choix du point d'une piste CombatLoco (CombatLocoTrack 0x9349e0), à chaque image de sa mise à jour (FUN_0062ea40) :
// FUN_0062e120 appelle FUN_0062c540, puis joue le sous-état (+0x28) :
// - la garde FUN_0062c540 (hors du sous-état 3) : plus loin de la cible que le grand rayon + 2 m (le double 0x900980), ou
//   le piéton à +0x1448 (non suivi), FUN_0061b6a0 ; sinon une minuterie (+0x4c) court tant que la cible n'est pas vue
//   (FUN_0048d320), et passé 1 000 ms, FUN_0061b6a0. FUN_0061b6a0 : le sous-état 3, un tirage forcé (+0x78, +0x79) et un
//   angle tiré entre ±π/4 (+0x58, DAT_00b0c0a0) ;
// - 0, l'approche (FUN_0062c740) : à l'image de tirage (+0x78), un point tiré autour de la cible, à une distance entre les
//   deux rayons et à un angle de π/8 à 3π/8 (DAT_00b0c0a4, DAT_00b0c0a0) d'un côté ou de l'autre du cap de la cible vers
//   le piéton (FUN_00433200) ; s'il est valable (FUN_006254c0 : la navigation, non suivie, supposé vrai), le mode 8 vers
//   lui et le sous-état 1. Sinon, plus près de la cible que le petit rayon, le mode 10 sur place ; plus loin, le mode 11
//   vers la cible ;
// - 1, l'aller (FUN_0061b700) : une minuterie (+0x44) ; à moins de 0,3 m du point (le double 0x933490 = 0,09), le
//   sous-état 2 ; passé 10 000 ms, le sous-état 0 ;
// - 2, l'attente (FUN_00625740) : une minuterie (+0x48) ; passé 300 ms, un rand() impair ramène au sous-état 0, sinon la
//   minuterie repart ;
// - 3, le retour (FUN_0062e120 en 0x6257c0) : dans le grand rayon et la cible vue, le sous-état 0 ; sinon le mode 12 vers la
//   cible (la recherche de chemin FUN_005e9df0 n'est pas suivie).
// Chaque changement de sous-état force le tirage de l'image suivante (+0x79). Les minuteries comptent le pas de l'image en
// ms (FUN_00446720 : 0,02 × CTimer::ms_fTimeStep × 1000). Non suivis : l'arme à distance (+0x7a, FUN_00625540), le vélo,
// la hauteur (|dz| > 2 m, le mode 1), les piétons à +0x9b4 / +0x9b8.
struct CCombatLoco {
	int32 sousEtat = 0;          // +0x28
	int32 mode = 11;             // +0x24 (FUN_0061b430)
	CVector point;               // +0x2c
	CVector decalage;            // +0x38 : le point moins la cible
	int32 minuterieAller = 0;    // +0x44
	int32 minuterieAttente = 0;  // +0x48
	int32 minuterieVue = 0;      // +0x4c
	float angle = 0;             // +0x58
	bool tirage = false;         // +0x78 : FUN_00465910(8) à cette image
	bool forcer = false;         // +0x79 : la prochaine image tire
};
// Le suivi d'un trajet (PATH) : l'objectif FollowPath (FollowPathObjective, table 0x90ca24, type 12 ; constructeur
// FUN_0046fc20, posé par PedFollowPath 0x5cfbf0 : le trajet +0x10, le nœud de départ +0x14, le mode +0x24, le sens +0x28 à 1,
// la vitesse +0x3c par FUN_005c6180, les rappels Lua +0x2c, +0x30, +0x34) et la piste FollowPath (FollowPathTrack 0x93800c :
// lancement FUN_0062c290, mise à jour FUN_0062eb70, arrêt FUN_0062c2e0, fin FUN_00621c80 qui retire l'objectif, pas
// FUN_0062ee20), qui en recopie les champs (FUN_0062c1e0).
struct CSuiviTrajet {
	int32 noeud = 0;             // +0x20 : le nœud visé
	int32 precedent = -1;        // +0x24
	int32 mode = 1;              // +0x30 : 0 une fois, 1 en boucle, 2 aller-retour, 3 sur place
	int32 sens = 1;              // +0x34 : le pas de l'aller-retour
	int32 plan = -1;             // +0x44 : le nœud dont on guette le passage, -1 aucun
	CVector planPoint;           // +0x48 : ce nœud
	CVector planNormale;         // +0x54 : la normale du plan de passage, vers la suite du trajet
};
// Un nœud du trajet tel que la piste le lit (FUN_005ec760) : sa position, son attente (+0x1c, en secondes) et ses actions (+0x20).
// Une action d'un nœud de trajet (la liste +0x20 du nœud, FUN_005ec760 : des pointeurs en [0], leur nombre en [2]) : son type
// (+0 : 0 tourner, 1 s'arrêter), son lacet (+4, degrés), son attente (+0x10, secondes).
struct CActionNoeud { int32 type = 0; float lacet = 0; float attente = 0; };
struct CNoeudTrajet { CVector pos; float attente = 0; bool actions = false; std::vector<CActionNoeud> listeActions; };
// FUN_0061acc0 : le nœud qui suit `noeud` selon le mode (le sens change aux bouts de l'aller-retour) ; faux à la fin du mode 0,
// pour un mode inconnu ou un trajet vide. Le mode 3 garde le nœud.
bool ScriptPedSuiviSuivant(CSuiviTrajet &s, int nombre, int32 &noeud);
// FUN_0061ad60 : le piéton doit-il s'arrêter à ce nœud ? Oui s'il a des actions ou une attente, au dernier nœud des modes 0
// et 3, aux deux bouts du mode 2 ; jamais en boucle (mode 1).
bool ScriptPedSuiviArret(const CSuiviTrajet &s, const std::vector<CNoeudTrajet> &noeuds, int32 noeud);
// FUN_0061afa0(precedent, noeud) : le plan de passage du nœud, sauf pour un nœud d'arrêt (alors plus de plan) ou sans suite :
// sa normale est la direction vers le nœud suivant ; avec un précédent et un virage (la somme des deux directions unitaires
// au carré au moins _DAT_00900c18), la perpendiculaire à plat de leur bissectrice, tournée vers la suite.
void ScriptPedSuiviPoserPlan(CSuiviTrajet &s, const std::vector<CNoeudTrajet> &noeuds, int32 precedent, int32 noeud);
// FUN_0062eaa0 : le piéton a-t-il passé le plan (le produit de la normale et du nœud moins le piéton ≤ 0) ?
bool ScriptPedSuiviPlanPasse(const CSuiviTrajet &s, const CVector &pos);
// FUN_0062eb70 (0x62ec10 … 0x62ec90, lu à l'assembleur) : le nœud est atteint quand la distance à plat du piéton au nœud
// (FUN_0085c4d0, la racine) est sous son rayon (+0x51c du piéton, PedPathNodeReachedDistance 0x5cd930) multiplié par 3
// (_DAT_00900c14) à vélo, sauf au dernier nœud d'un trajet suivi une fois (mode 0), où le facteur reste 1.
bool ScriptPedSuiviAtteint(const CSuiviTrajet &s, int nombre, const CVector &pos, const CVector &noeud, float rayon, bool velo);
// L'attente d'un nœud atteint (FUN_0062eb70, 0x62ed86) : s'il a une attente positive (+0x1c, en secondes), la piste attend
// jusqu'à maintenant + attente × 1000 ms (_DAT_0090d3d8 = -1000, soustrait) et le piéton prend le lacet du nœud moins 90
// degrés (FUN_0062c300 → FUN_00621c00 ; à vélo, plus 180). Rend l'échéance en ms, 0 sans attente.
uint32 ScriptPedSuiviEcheance(float attente, uint32 maintenantMs);
// FUN_005e7560 (le but lissé de FUN_00621a30) : sur le segment a → b, le point à `avance` mètres devant la projection du
// piéton (`t` rend la fraction atteinte, plus de 1 au-delà de b, alors b) ; si le piéton s'écarte du segment de plus que
// `avance`, sa projection. L'avance est 3 × la vitesse de l'intention + 3 (_DAT_00900c14, _DAT_00900988).
CVector ScriptPedSuiviBut(const CVector &a, const CVector &b, const CVector &pos, float avance, float &t);
float ScriptPedSuiviCap(float lacetNoeud, bool velo);
// FUN_0062e780 : l'action `index` (+0x2c de la piste) du nœud : un virage (type 0, FUN_00621c00 : la direction de l'intention de
// mouvement prise au lacet moins 90 degrés, sa vitesse à 0) ou un arrêt (type 1 : la vitesse de l'intention, +0x1c du composant
// +0x2e0, à 0) ; l'index avance et l'échéance (+0x1c de la piste) vaut maintenant + attente × 1000 (_DAT_0090d3d8 = -1000,
// soustrait, tronqué), même pour une attente nulle. Rend vrai et l'action jouée ; faux quand la liste est finie (l'index revient
// à 0, l'échéance à 0 : le nœud suivant, FUN_0062e5f0).
bool ScriptPedSuiviAction(const CNoeudTrajet &n, int32 &index, uint32 maintenantMs, uint32 &echeance, CActionNoeud &action);

// `tirer` rend un rand() du CRT (0 à 32767) ; les positions sont lues à plat.
void ScriptPedCombatLocoChoisir(CCombatLoco &c, const CVector &soi, const CVector &cible, float petit, float grand,
                                int32 pasMs, bool cibleVue, int (*tirer)(void *), void *ctx);

// Les 22 groupes d'animations d'humiliation (0x9113d0, 32 octets par nom) : « HUMIL_5-8_A » pour 0 … « HUMIL_6-1V6-1 »
// pour 21. Le groupe 0 à 12 est celui d'une victime du joueur, rangé par sa taille (CharacterSize) : 0 à 2 Medium
// (5-8), 3 et 4 Fat (5-8F), 5 à 7 Large (6-1), 8 et 9 Small (4-10), 10 à 12 Huge (6-5). nil hors de la table.
enum { SCRIPT_PED_GROUPES_HUMILIATION = 22 };
const char *ScriptPedGroupeHumiliation(int groupe);

// La bagarre, réduite : l'attaquant rejoint sa cible à la course, puis frappe à portée.
enum { SCRIPT_PED_PORTEE_CM = 120, SCRIPT_PED_COUP_PV = 10 };
static const float SCRIPT_PED_PORTEE = 1.2f;        // m
static const float SCRIPT_PED_CADENCE = 0.8f;       // s entre deux coups
static const float SCRIPT_PED_DEGATS = 10.0f;       // points de vie par coup

// Les objectifs d'un piéton (+0x5f0 : une file de 20 places en +8, leur nombre en +0x58, l'objectif par défaut en +0x5c).
// L'objectif courant est le premier de la file (FUN_004701f0), sinon celui par défaut (l'outil n'en a pas). Un objectif a
// son type en +8 ; AttackPedObjective (FUN_00470c10, table 0x… : « attaquer un piéton ») est le type 7, sa cible en +0x14,
// son style en +0x10 (le troisième argument de PedAttack, 0 à 3) ; le type 16 a aussi une cible en +0x14 (la condition
// Objective les traite ensemble, FUN_00606c00). Le compte de références (+0xc) n'est pas suivi.
// FleeObjective (le constructeur FUN_0046f940(0x16, piéton), table 0x911894) est le type 22, la cible fuie en +0x10.
enum { SCRIPT_PED_TYPE_DEFAUT = 12, SCRIPT_PED_TYPE_JOUEUR = 13 };
enum { OBJECTIF_ATTAQUER_PIETON = 7, OBJECTIF_SUIVRE_TRAJET = 12, OBJECTIF_ATTAQUER_16 = 16, OBJECTIF_FUIR = 22, SCRIPT_PED_OBJECTIFS = 20 };
struct CObjectifPieton {
	int32 type = -1; int cible = -1; int32 style = 0;
	// FollowPath (type 12, FUN_0046fc20) : le trajet (+0x10, -1 au départ), le nœud de départ (+0x14), le mode (+0x24, 1 au
	// départ), la vitesse (+0x3c, _DAT_00911e30 au départ, puis l'allure de FUN_005c6180).
	int32 trajet = -1, noeudDepart = 0, mode = 1;
	float vitesse = 0.3f;
	int32 allure = -1;           // l'allure du script (le quatrième argument), pour la vitesse du geste de l'outil
};

// Les attitudes (0 à 4 ; 2 neutre ; 5 « rien de propre », on passe au niveau suivant). CAttitudeSet (table 0x90c98c, constructeur
// FUN_0046f120 : tout à 5) : une attitude par type de piéton visé, les types 0 à 11 en +4 + 4 × type et le joueur (13) en +0x34 ;
// les autres types (12, au-delà) rendent 2 et ne s'écrivent pas (FUN_0046f3c0, FUN_0046f400).
struct CAttitudeSet {
	int32 valeurs[13] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
	static int Case(int type){ return type == SCRIPT_PED_TYPE_JOUEUR ? 12 : type >= 0 && type < 12 ? type : -1; }
	int32 Lire(int type) const { int c = Case(type); return c < 0 ? 2 : valeurs[c]; }
	void Ecrire(int type, int32 v){ int c = Case(type); if(c >= 0) valeurs[c] = v; }
};
// CVariableAttitudeSet (0x90c998, 0x70 octets) : la même table plus un écart par type (+0x38 + 4 × type, le joueur en +0x68),
// nul à la construction (FUN_0046f1f0) ; lue (FUN_0046f430), l'attitude vaut la base plus l'écart / 3 (division entière), bornée
// à 0 et 4 ; écrite (FUN_0046f4b0), la base reçoit la valeur et l'écart revient à 0.
struct CVariableAttitudeSet : CAttitudeSet {
	int32 ecarts[13] = {};
	int32 Lire(int type) const {
		int c = Case(type);
		if(c < 0) return 2;
		int32 v = valeurs[c] + ecarts[c] / 3;
		return v < 0 ? 0 : v > 4 ? 4 : v;
	}
	void Ecrire(int type, int32 v){ int c = Case(type); if(c >= 0){ valeurs[c] = v; ecarts[c] = 0; } }
};
// Le gestionnaire des attitudes entre types (FUN_0046f1f0) : douze CVariableAttitudeSet, celles par défaut (+0x70 × type), puis
// douze CAttitudeSet, les courantes (+0x540 + 0x38 × type). PedGetTypeToTypeAttitude (0x46f6e0) : 2 pour un type de départ 13 ou
// au-delà de 11 ; sinon la courante, et la défaut si elle vaut 5. PedSetTypeToTypeAttitude (0x46f760) et
// PedSetDefaultTypeToTypeAttitude (0x46f7a0) ramènent le type 13 à 12 et les autres au-delà de 11 à 13 : ces cases tombent hors
// des douze tables (l'objet voisin), l'outil ne les écrit pas.
struct CAttitudes {
	CVariableAttitudeSet defaut[12];
	CAttitudeSet courante[12];
	int32 TypeVersType(int de, int vers) const {
		if(de == SCRIPT_PED_TYPE_JOUEUR || de < 0 || de >= 12) return 2;
		int32 v = courante[de].Lire(vers);
		return v == 5 ? defaut[de].Lire(vers) : v;
	}
	void PoserTypeVersType(int de, int vers, int32 v){ if(de >= 0 && de < 12) courante[de].Ecrire(vers, v); }
	void PoserDefaut(int de, int vers, int32 v){ if(de >= 0 && de < 12) defaut[de].Ecrire(vers, v); }
};

struct CScriptPed {
	bool vivant = false;         // l'entrée est prise
	int32 modele = -1;
	CVector pos;
	float cap = 0;               // degrés, 0 vers +x comme les lacets des .dat
	int32 script = -1;           // l'objet script qui l'a créé, -1 pour le joueur ou un piéton rendu ambiant
	float sante = 100;           // +0x1cb8 ; le constructeur de CPed (FUN_0047ffe0) la pose à 100, comme +0x1cbc
	float santeMax = 100;        // +0x1ca4, PedSetMaxHealth (0x5c8de0), PedGetMaxHealth (0x5c8d80)
	float santeMin = 0;          // +0x1ca8, PedSetMinHealth (0x5c8ef0)
	float regeneration = 0;      // +0x1cac : la colonne G de pedstats.dat (taux de régénération, n/100)
	int32 stats[64] = {};        // +0x1900 : les 64 colonnes B à BM de son entrée de pedstats.dat (GameGetPedStat)
	bool statsLues = false;      // l'outil : une entrée de pedstats.dat posée (PoserStats)
	// +0x1310 : le type du piéton (PREFECT 0 … COP 7, TEACHER 8, BULLY 11, DEFAULT 12, PLAYER1 13 ; CModelEnum::typesDePietons),
	// recopié du modelinfo (+0x4c) par le constructeur de CPed (FUN_0047ffe0), changé par PedSetFaction (0x5cb570, jamais en
	// 13), lu par PedGetFaction (0x5cb5c0), IsPlayer (FUN_005ffe40 : 13), IsAuthority (FUN_005ffb20) et PedType (FUN_005ffac0).
	int32 type = SCRIPT_PED_TYPE_DEFAUT;
	// +0x18d0 : la réserve de sprint, posée à la création à la colonne R de l'entrée de pedstats.dat (+0x5c de l'entrée,
	// FUN_00478e30), son maximum ; FUN_0047cde0 la remplit à chaque image (jusqu'à ce maximum), FUN_0047cee0 la dépense.
	// La condition Sprint la compare (FUN_005fda70). L'outil ne la fait pas varier.
	float sprint = 0;
	// +0x157c : un flottant que le vélo pose (FUN_004e9160, 0x4e9292 et 0x4e92d1 ; et FUN_005051b0, 0x5051ff, pour le piéton
	// DAT_00c3ce14), et dont le composant d'actions range l'adresse en +0x38 de ses trois contextes (FUN_00484d10 : +0x338,
	// +0x390, +0x3e8, vérifié à l'assembleur en 0x484df8) ; la condition ParametricTime le lit. Aucun constructeur ne
	// l'écrit (FUN_0047ffe0 pose +0x1574 … +0x1584 sans lui) : 0 hors du vélo, supposé (la mémoire du pool).
	float parametrique = 0;
	bool discretion = false;     // +0x1398 : un piéton de mission de discrétion (PedSetIsStealthMissionPed, 0x5d0110 →
	                             // FUN_005d0080) ; la condition IsStealthMissionChar le lit (FUN_00602cf0)
	// +0x714 : la classe de combat, la colonne S de l'entrée de pedstats.dat (+0x60 de l'entrée, GetCharacterClassId
	// 0x488a00 : Generic, Ranged…), que FUN_00478e30 recopie ; la condition FightClass la compare (FUN_00601440).
	int32 classeCombat = 0;
	// +0x53c (+0x180 du composant de visée +0x3bc, FUN_0048bf20) : le piéton que celui-ci regarde, nul à la construction
	// (FUN_0048c1b0) ; FUN_00488b60 le pose, non suivi. La condition RangeToFocus en mesure la distance (FUN_006008e0).
	int focus = -1;
	int32 systemeCombat = 0;     // +0x16dc, PedSetFightingSystem (0x5ca590) ; 0 à la création (FUN_00474f50)
	// +0x1314 : les drapeaux du piéton, un mot chacun (le drapeau n en +0x1314 + 4n) ; les pistes SetPedFlags les posent
	// (PedFlagsTrack 0x931380, lancement FUN_0060f8e0 : le drapeau +0x1c reçoit l'octet +0x18), la condition PedFlags
	// les lit (FUN_005fd700). Trois servent aux coups (FUN_0047b6b0) : 12 (+0x1344) et 30 (+0x138c, PedSetInvulnerable
	// 0x5c9160, PlayerSetInvulnerable 0x5d1810) écartent tout coup, 20 (+0x1364, PedSetInvulnerableToPlayer 0x5c9200)
	// ceux du joueur.
	int32 drapeaux[SCRIPT_PED_DRAPEAUX] = {};
	// +0xd44 : le groupe d'humiliation, le champ +0x200 du composant social du piéton (+0xb44, son piéton en +4),
	// -1 à la construction (FUN_0048e790, appelé par le constructeur de CPed) ; un indice dans la table des 22 groupes
	// d'animations HUMIL_ (ScriptPedGroupeHumiliation). La condition HumiliationGroupIs le lit (FUN_006039b0).
	int32 groupeHumiliation = -1;
	// Le composant social (+0xb44) : son état (+0x6c, +0xbb0 du piéton ; FUN_0048f270 le passe de 1 à 2 ou de 3 à 6 et pose
	// 3 ou 5 au partenaire, FUN_00492990 4, FUN_00495440 3 et 5) et son partenaire (+0x70, +0xbb4), nuls hors d'un échange ;
	// l'octet +0x224 (+0xd68 du piéton), dont le bit 0 écarte le piéton des échanges vus par IsSocializing. L'outil ne joue
	// pas le système social : l'état reste 0.
	int32 etatSocial = 0;
	int partenaireSocial = -1;
	uint8 drapeauxSociaux = 0;
	int cibleDePrise = -1;       // +0x155c, PedSetGrappleTarget (0x474360) : le partenaire de prise, -1 sinon
	CObjectifPieton objectifs[SCRIPT_PED_OBJECTIFS];   // +0x5f8, la file des objectifs
	int nombreObjectifs = 0;     // +0x648
	// Le composant de combat (+0x668, son piéton en +0) : la zone de combat où le piéton s'est placé autour de sa cible
	// (+0x66c, -1 aucune), le masque des trois zones qu'il peut prendre (+0x700, PedSetCombatZoneMask 0x5cd0c0 →
	// FUN_00486110 ; vrai par défaut, supposé). Comme cible : la capacité de chaque zone (+0x670, PedSetNumCombatants
	// 0x5cb7d0 ; PedRestoreNumCombatants 0x5cb9c0 remet AIButes +0x18, +0x1c, +0x20 : 1, 2, 4) et ses combattants
	// (+0x67c + 0x2c × zone : le nombre, puis les piétons).
	bool arbreIA = false;        // +0x4b4 du composant +0x2e0 : un arbre de l'IA posé (PedSetAITree) ; l'outil ne joue
	                             // alors plus le geste de PedAttack, c'est l'arbre qui mène le piéton
	int zoneCombat = -1;
	// Les droits d'attaque sur ce piéton, dans son composant de combat (+0x668) : l'octet +0x344 (+0x9ac du piéton, pris),
	// le piéton qui les tient +0x340 (+0x9a8) et leur échéance +0x348 (+0x9b0, en ms du jeu). Un seul attaquant à la fois
	// les prend (FUN_00486c40) ; la condition IsTargetAttackRightsTaken lit l'octet sur la cible de l'attaquant.
	// +0x1e0c : la minuterie d'action, en secondes du jeu (0 par le constructeur de CPed, param_1[0x783]) ; la piste
	// SetActionTimer la pose à maintenant + sa durée (FUN_00474840), la condition CheckActionTimer la compare (FUN_00474870).
	float minuterieAction = 0.0f;
	// Les piétons proches (+0x3c0 leur nombre, +0x3c4 la liste, 16 au plus) : CPed::BuildPedLists (FUN_0047d1d0).
	std::vector<int> proches;
	// +0x550 : l'instant (ms) du dernier coup du piéton (la minuterie que posent ses pistes Collision, recopiée par
	// l'appelant depuis son contrôleur d'actions) ; le composant de combat de sa cible y mesure l'échéance de son jeton.
	uint32 dernierCoup = 0;
	// Les jetons de combat sur ce piéton (son composant de combat +0x668) : quatre catégories (+0xc4 + 4k leur nombre,
	// AIButes +0x3c à +0x48 ; les entrées en +0xd4 + 0x94 k, 0x18 octets chacune : le détenteur +0, le précédent +4, le
	// filtre de classe +8, l'octet coupé +0xc, la catégorie +0x10, l'instant +0x14), et la place du cône (+0x324, son
	// précédent +0x328, son filtre +0x32c, coupée +0x330, son instant +0x338). FUN_00486cd0 les pose, FUN_00486e40 les
	// distribue à chaque image, la condition FightToken les lit (FUN_00486760).
	struct Jeton { int detenteur = -1, precedent = -1; int32 filtre = 0; bool coupe = false; int32 categorie = 0; uint32 instant = 0; };
	Jeton jetons[4][6];
	int32 nombreJetons[4] = {0, 0, 0, 0};
	Jeton jetonCone;
	// +0x1748 : un tirage par piéton, rand() du CRT / 32767, posé par la remise (FUN_00474f50, 0x4750b3) et refait à
	// chaque image par la mise à jour de CPed (FUN_004812d0, 0x4818f8 … 0x48191c, après FUN_00499340) ; Random le lit
	// sans son octet +0xe (FUN_00600a50, 0x600acc). L'appelant le tire (le rand() est le sien).
	float hasardImage = 0.0f;
	bool droitsPris = false;
	int detenteurDroits = -1;
	uint32 echeanceDroits = 0;
	bool masqueZones[3] = {true, true, true};
	int32 capaciteZones[3] = {1, 2, 4};
	std::vector<int> combattants[3];
	int agresseur = -1;          // +0x544 (+0x188 du composant de visée) : posé par la condition Attacked (FUN_00606b40) ;
	                             // un FleeObjective le fuit avec l'octet +0x19 de sa piste
	int cibleObjectif = -1;      // +0x540 (+0x184 du composant de visée en +0x3bc) : la cible d'un objectif d'attaque, que la
	                             // condition Objective pose quand elle passe (FUN_0048c330)
	bool mort = false;           // +0x1c98 : posé par la piste Death (FUN_00479110), lu par PedIsDead (0x5c8f30)
	// Les objets du piéton, des indices dans le pool des objets animés (src/objects/ObjetsAnimes), -1 sans : l'objet visé (+0x438)
	// et son point de synchronisation (+0x43c, le nom haché : FeetSync…), que la condition TargetProp pose depuis l'objet repéré
	// (FUN_004979b0 → FUN_00478610) ; l'objet repéré par le relevé (+0x444, FUN_004989f0), son point (+0x448) et sa distance
	// (+0x44c) ; les objets relevés trop loin ou moins bien placés (la liste +0x450, dix entrées de 12 octets, leur nombre en
	// +0x4c8 : FUN_0048c6b0) ; l'objet dont il se sert (+0x548, FUN_004ed6e0), qui arrête le relevé (FUN_004991e0) ; +0x1520,
	// posé à 1 par FUN_004ed6e0.
	int objetVise = -1;
	uint32 syncVise = 0;
	int objetRepere = -1;
	uint32 syncRepere = 0;
	float distanceRepere = 0.0f;
	struct ObjetProche { int objet; uint32 sync; float distance; };
	std::vector<ObjetProche> objetsProches;
	int objetUtilise = -1;
	bool utiliseObjet = false;
	// La laisse (le composant +0x358, FUN_00499910 PedSetTetherToXYZ, FUN_004998f0 la lâche) : son genre (+0x360 : 0 aucune,
	// 1 un point, 2 et 3 un piéton ou un déclencheur, que l'outil ne suit pas), son rayon (+0x364) et son point (+0x368 … +0x370) ;
	// IsTethered (FUN_00603040) lit le genre.
	int32 laisse = 0;
	float laisseRayon = 0;
	CVector laissePoint;
	// La laisse vers un piéton (PedSetTetherToPed, FUN_00499960, ecx = le piéton + 0x358 : le genre 2, la référence +0x35c, le
	// rayon +0x364, la position du piéton au moment de la pose en +0x368 … +0x370, le décalage +0x378 … +0x380, les octets
	// +0x3b4 à 1 et +0x3b5 à 0) : le piéton tenu, -1 sinon.
	int laissePieton = -1;
	int laisseDeclencheur = -1;  // +0x384 : le déclencheur d'une laisse du genre 3 (PedRestrictToTrigger, FUN_004999e0)
	CVector laisseDecalage;
	bool laisse3b4 = false, laisse3b5 = false;
	// Le groupe des alliés (+0x2f0 de CPed : le piéton lui-même, puis son meneur en +0x2f4 et son suiveur en +0x2f8, deux
	// références de FUN_005d7db0 ; un suiveur peut mener à son tour, et FUN_00485410 remonte les +0x2f4 jusqu'au premier
	// meneur). PedRecruitAlly (0x5cd550 → FUN_00485fa0), PedDismissAlly (0x5cd5d0 → FUN_00485880 → FUN_00485820) ; les
	// conditions IsAllyFollower (FUN_006037b0 → FUN_004851d0 : +0x2f4 non nul), IsAllyLeader (FUN_00603790 → FUN_004851e0 :
	// +0x2f8 non nul), HasAllyState (ConditionAllyState, FUN_00606450 : +0x318 égal au mot +4) et HasAllyMorale
	// (ConditionAllyMorale, FUN_00606470 : +0x31c).
	int meneur = -1;             // +0x2f4
	int suiveur = -1;            // +0x2f8
	bool groupeTenu = false;     // +0x2fc (l'octet +0xc du groupe) : chez le meneur, le troisième argument de FUN_00485fa0
	int32 groupe300 = 0;         // +0x300
	bool groupe304 = false;      // +0x304 : AllyIsInCombat (FUN_00606360) ne regarde que les suiveurs qui l'ont
	bool groupe305 = false;      // +0x305
	int32 groupe308 = 0, groupe30c = 0;   // +0x308, +0x30c
	int32 etatAllie = 1;         // +0x318 : 1 au recrutement et au renvoi ; SetAllyState (FUN_0062a2c0) l'écrit chez un suiveur
	int32 moralAllie = 0;        // +0x31c : 0 au recrutement ; SetAllyMorale (FUN_0062a2f0)
	// +0x13c8 : le sprint sans fin ; PedSetInfiniteSprint (0x5cde60) le pose, FUN_00485fa0 (le recrutement) et FUN_00485820 (le renvoi)
	// le mettent à 1 ; FUN_0047cee0 ne dépense pas la réserve de sprint (+0x18d0) avec lui, ni quand le meneur est le joueur.
	bool sprintSansFin = false;
	bool teleportable = true;    // +0x1c99 : PedCanTeleportOnAreaTransition (0x5cd080) ; 1 depuis le constructeur (0x480465)
	int32 mot135c = 0;           // +0x135c : PedShowHealthBar le met à 0 en montrant la barre, à 1 avec « non » (non lu ailleurs)
	int32 groupe14ec = 0;        // +0x14ec : le troisième argument de PedRecruitAlly (FUN_005d8f20), chez les deux ; 0 au renvoi
	// Le piéton critique pour la mission (PedSetMissionCritical, 0x5cca10) : l'octet +0x141c, le rappel Lua +0x18e8 (une
	// fonction gardée par lancer_lua) et +0x1420 ; FUN_004782a0 appelle le rappel quand la santé tombe à 0.
	bool critique = false;       // +0x141c
	bool rappelCritique = false; // +0x18e8 tient une fonction
	bool critique1420 = false;   // +0x1420
	bool controlable = true;     // +0x1e90 : le joueur rendu à la manette (PlayerSetControl, FUN_00445c50 : 0 quand le script le prend)
	// Le composant de vue (+0xa64, son piéton en +0) : l'angle total du cône (+0xa68, la colonne D de pedstats.dat en
	// degrés, × π / 180 par FUN_00478e30) et la portée (+0xa6c, la colonne E, en mètres) ; un cache de 16 piétons (+0xa74 :
	// le piéton, l'octet vu, le nombre d'images à vivre) que FUN_0048ccf0 fait vieillir à chaque image (CPed, FUN_004812d0).
	float visionAngle = 0, visionPortee = 0;
	// +0x51c et l'octet +0x520 (+0x160 et +0x164 du composant de visée +0x3bc) : le rayon d'arrivée à un nœud de trajet et son
	// « posé », qu'écrit PedPathNodeReachedDistance (0x5cd930) ; 1 m par défaut, posé par la remise du composant (FUN_0048c430,
	// 0x48c5d1), que la remise du piéton (FUN_00474f50, 0x474f71) appelle depuis son constructeur.
	float rayonNoeud = 1.0f;     // FUN_0048c430 (la remise du composant de visée, depuis FUN_00474f50) : fld1 en 0x48c5c9
	bool rayonNoeudPose = false;
	struct Vu { int ped = -1; bool vu = false; int32 images = 0; };
	Vu vus[16];
	// +0x1dbc et +0x1dc0 : le dernier piéton qui l'a frappé (FUN_004973d0, depuis la méthode du coup FUN_0047b810 quand son
	// 15e argument, l'octet +0x214 de la piste Collision, est posé) et le temps écoulé depuis, en secondes (FUN_00499340 : 0
	// sans agresseur).
	int dernierAgresseur = -1;
	float tempsAgresseur = 0;
	// +0x2e8 : son propre CAttitudeSet, créé à 5 par le constructeur de CPed (FUN_0047ffe0 → FUN_0046f120) ; PedSetPedToTypeAttitude
	// (0x5cb480 → 0x473080) l'écrit, PedGetPedToTypeAttitude (0x475230) le lit, puis la table du type du piéton s'il vaut 5.
	CAttitudeSet attitudes;
	bool pasCiblable = false;    // +0xc0 : PedMakeTargetable (0x5ca980) y range la négation de son second argument
	float tempsNonVu = 0;        // +0x1db8 : depuis quand le piéton ne voit plus sa cible (s), FUN_00499340 ; 0 au verrouillage
	uint32 instantVerrou = 0;    // +0x1dc4 : l'horloge du dernier verrouillage (FUN_00497d20 y ajoute un tirage, non suivi)
	bool immortel = false;       // +0x13fc : PedSetImmortalFlag (0x5ce6e0) / PedGetImmortalFlag (0x5ce730)
	// +0x4e4 (l'octet du navigateur +0xdac que FUN_005e09c0 lit) : immobile, posé par PedSetStationary (0x5c7950, le booléen
	// FUN_005d8f20) et la piste SetStationary (FUN_00626920) ; la navigation ne donne alors plus de vitesse (FUN_005e4a60).
	bool immobile = false;
	// L'arme : celle que le piéton veut (+0x18 du composant d'actions +0x2e0 : 0 rien de demandé, -1 les mains nues ; PedSetWeapon
	// la pose, la piste PropGetDesired la prend), celle en main (+0x10e, le modèle de l'objet arme +0x1d0 : -1 sans arme ; la piste
	// PropAttach la pose) et celle en passage de l'une à l'autre (+0x1574, -1 à la construction : 0x480835).
	int32 armeVoulue = 0, armeTenue = -1, armePassage = -1;
	// La charge de l'objet arme (+0x128 de +0x1d0, CChargeArme) : 0 depuis son constructeur (FUN_004da610) et à chaque changement de
	// modèle (FUN_004da6f0), posée par WeaponSetCharge, augmentée par WeaponIncrementCharge, lue puis vidée (-100) par WeaponFire.
	float armeCharge = 0.0f;
	// Les drapeaux de l'objet arme (+0x26c + 4n de +0x1d0) : SetWeaponFlags et PedSetWeaponFlag les posent, WeaponFlags les lit quand
	// l'arme tenue n'a pas d'entrée d'inventaire.
	int32 drapeauxArme[8] = {};
	uint32 instantMort = 0;      // +0x18b8 : l'horloge (ms) à la mort d'un autre que le joueur
	// Le geste en cours.
	// L'intention de mouvement du composant d'actions (+0x2e0) : sa direction (+0, +4, +8) et sa vitesse (+0x1c), que la
	// navigation pose à chaque image (FUN_004a4a50) et remet à zéro sans but (FUN_004a45c0) ; les conditions
	// MotionIntentionSpeed (FUN_005fd810) et MotionIntention (ConditionMotionIntentionDirection, FUN_005fd760) la lisent.
	// Les seuils de Locomotion.act (Free) : walk ≥ 0,1, RUN ≥ 0,5, Sprint ≥ 1,05. L'outil la pose quand il fait marcher
	// un piéton (Aller : la vitesse de son allure ; arrivé, arrêté : 0) et dans CombatLoco (lancer_lua).
	CVector intention;
	float intentionVitesse = 0;
	float armeInit = -1.0f;      // +0x120 de l'objet arme (+0x1d0, CWeapon) : -1 depuis son constructeur (FUN_004da610),
	                             // posé par une piste SetWeaponInfo (FUN_00660f50) ; WeaponNeedInit (FUN_0065b7b0) attend -1
	float armePortee = -1.0f;    // +0x124 : le +0x40 (64) de SetWeaponInfo ; la portée d'un tir sans cible va de lui à +0x120 ; -1 au
	                             // constructeur, à la remise (FUN_004d92c0) et au changement de modèle (FUN_004da6f0), comme +0x120
	int32 armeMunition = -1;     // +0x12c : l'arme du nom +0x44 (68) de SetWeaponInfo (FUN_0051ad50), le projectile des tirs
	bool endormi = false;        // +0x1358 : PedSetAsleep (0x5caa30) ; endormi, son arbre de l'IA n'avance plus (FUN_00484490)
	bool enMarche = false;       // va vers `cible` à `vitesse`
	CVector cible;
	float vitesse = 0;
	int attaque = -1;            // handle de la cible de PedAttack, -1 sinon
	int verrou = -1;             // PedLockTarget : la cible verrouillée (FUN_00497d20), -1 sinon
	int verrouMode = 0;          // son troisième argument (le genre de verrouillage)
	float prochainCoup = 0;      // s avant le prochain coup
	// Ce qui s'est passé pendant la dernière image (pour le journal de lancer_lua).
	bool arrive = false, aFrappe = false, vientDeMourir = false;
	int dernierFrappe = -1;      // la cible du dernier coup (pour le journal)
};

class CScriptPedPool
{
public:
	CScriptPedPool(void);
	// FUN_005cf6b0 : la première entrée libre, ou -1 si les 24 sont prises.
	int Creer(int32 modele, const CVector &pos, float cap, int32 script);
	// lua_PedDelete : libère l'entrée ; le joueur ne se supprime pas.
	bool Supprimer(int handle);
	// lua_PedIsValid : un handle qui désigne une entrée prise.
	bool Valide(int handle) const;
	CScriptPed *Acceder(int handle);
	const CScriptPed *Acceder(int handle) const;
	// Libère tout ce qu'un objet script a créé ; renvoie le nombre libéré.
	int SupprimerDuScript(int32 script);
	int Vivants(void) const;
	// lua_PedMoveToPoint / lua_PedMoveToXYZ : le piéton part vers un point à une allure.
	bool Aller(int handle, const CVector &cible, int allure);
	// lua_PedStop (0x5c7630) : plus de geste en cours.
	bool Arreter(int handle);
	// lua_PedAttack (0x5c9ca0) : l'attaquant prend la cible ; faux si l'un des deux n'existe pas. Le moteur crée un
	// AttackPedObjective (FUN_00470c10 : type 7, la cible, le style) et le met dans la file des objectifs : au bout
	// (CPed::PushObjective, FUN_00471240), ou devant si `devant` (le quatrième argument du script quand le troisième est
	// un nombre ; FUN_00471320). L'outil garde aussi le geste (poursuite et coups), que lancer_lua n'avance pas pour un
	// piéton qui a un arbre de l'IA : c'est alors l'arbre qui le mène.
	bool Attaquer(int handle, int cible, int32 style = 0, bool devant = false);
	// FUN_00471240 (devant faux) et FUN_00471320 (devant vrai) : un objectif d'attaque (type 7) qui vise la même cible
	// (+0x14) que le dernier de la file (le premier, devant) n'est pas ajouté, ni un objectif de type 8 qui porte le même
	// +0x1c (non suivi ici) ; la file pleine (20) refuse. Vrai si l'objectif est dans la file (ajouté ou déjà là).
	bool PousserObjectif(int handle, const CObjectifPieton &o, bool devant);
	// FUN_00470110 (la mise à jour de la piste ClearCurrentObjective, FUN_0062a180) : retire de la file les objectifs du
	// type donné ; après un retrait, l'objectif qui prend sa place n'est pas examiné (l'indice avance quand même, comme
	// dans le moteur). Rend le nombre retiré.
	int RetirerObjectifs(int handle, int32 type);
	// FUN_004701f0 : l'objectif courant, le premier de la file ; nil si elle est vide (l'objectif par défaut +0x5c n'est
	// pas suivi).
	const CObjectifPieton *ObjectifCourant(int handle) const;
	// lua_PedLockTarget (0x5cb060) : rien si le piéton n'existe pas ; cible -1 lève le verrou (FUN_004989c0(0)) ;
	// sinon, si la cible existe, FUN_00497d20 la verrouille avec le mode donné. Vrai si quelque chose a changé.
	bool Verrouiller(int handle, int cible, int mode);
	// Les droits d'attaque (le composant de combat +0x668). PrendreDroits : la piste TargetAttackRightsAcquire (table
	// 0x93610c, lancement FUN_0062a1a0) : si la cible du piéton (PedGetTargetPed) est un piéton, FUN_00486c40 sur son
	// composant de combat : pris, tenus par le piéton, jusqu'à maintenant + 15 000 ms ; écrase un détenteur précédent.
	// RendreDroits : TargetAttackRightsRelease (0x936124, FUN_0062a1e0 → FUN_004861c0) : rendus si le piéton les tient.
	// LacherDroits : FUN_00486c00 (ecx = le composant de combat du piéton) : ceux qu'il tient sur sa cible, rendus ; appelée
	// par FUN_00497d20 quand la cible change, par PedLeaveCombatZone et les remises du composant de visée (FUN_0048c3b0,
	// FUN_0048c430). AvancerDroits : la mise à jour du composant de combat (FUN_00487780, lue à l'assembleur de 0x48781a à
	// 0x487838) rend les droits échus (+0x348 au plus maintenant). DroitsPrisSurCible : la condition
	// IsTargetAttackRightsTaken (0x936da4, FUN_005ffc70, lue à l'assembleur) : la cible du piéton est un piéton (l'octet
	// +0x108 & 7 vaut 3) et ses droits sont pris, par qui que ce soit (FUN_004861b0 : l'octet +0x344).
	// CPed::BuildPedLists (FUN_0047d1d0) : avec `refaire` (son argument, ou le tirage de l'image FUN_00465910(16) que fait
	// l'appelant), la liste refaite : les autres piétons vivants (pas morts, +0x1c98) à 30 m au plus à plat (le double
	// 0x900b00 = 900, comparé au carré), 50 au plus, triés par distance (FUN_004753e0), gardés à 16 ; sinon, ceux qui ont
	// disparu ou passé 30 m retirés (FUN_0047d0d0), et une passe de tri entre voisins (FUN_0047d130).
	void ReleverProches(int handle, bool refaire);
	// FUN_00486cd0 (le constructeur du composant de combat FUN_00486d50, sa remise FUN_0048c430) : chaque catégorie vidée et
	// mise à son nombre (FUN_004866e0 : détenteur, précédent et filtre à 0, la catégorie notée), la place du cône vidée.
	void PoserJetons(int handle, const int32 nombres[4]);
	// FUN_00486e40, à chaque image dans la mise à jour du composant de combat (FUN_00487780, 0x4877cf), sauf l'octet +0x1e25
	// (non suivi). Les AIButes : +0x4c et +0x50 l'échéance d'un jeton (s, la catégorie 3 prend la seconde), +0x54 et +0x58 le
	// cône d'attaque (FUN_004865a0). `tirer` : le rand() du CRT.
	struct ButesJetons { float echeance = 10.0f, echeanceVelo = 30.0f, porteeCone = 1.5f, cosCone = 0.76f; };
	void DistribuerJetons(int handle, uint32 maintenant, const ButesJetons &b, int (*tirer)(void *), void *ctx);
	// La condition FightToken (ConditionFightToken 0x936b30, FUN_00601f70, lue à l'assembleur) : la cible du piéton est un
	// piéton et il y tient un jeton qui n'est pas coupé, ou la place du cône (FUN_00486760) ; une cible qui n'est pas un
	// piéton donne vrai, pas de cible faux.
	bool TientJeton(int handle) const;
	bool PrendreDroits(int handle, uint32 maintenant);
	bool RendreDroits(int handle);
	void LacherDroits(int handle);
	void AvancerDroits(uint32 maintenant);
	bool DroitsPrisSurCible(int handle) const;
	// FUN_00478190 (lua_PedSetHealth, la piste SetHealth) : la santé (+0x1cb8), jamais sous la santé minimale (+0x1ca8).
	// Une santé nulle ne tue pas : seul le rappel Lua du piéton (+0x141c, rangé en +0x18e8, non suivi) est appelé. Le
	// piéton ne meurt que par Mourir, la piste Death.
	void Sante(int handle, float sante);
	// L'emplacement 42 de CPed (FUN_00479110), que lance la piste Death (DeathTrack 0x934aa0, FUN_0060fdc0 : l'octet
	// +0x18 de la piste en argument). Rien pour un piéton déjà mort (+0x1c98) ; rien non plus pour un immortel (+0x13fc,
	// PedSetImmortalFlag 0x5ce6e0), dont seul le rappel Lua est appelé. Sinon la santé passe à -100 (_DAT_00906520),
	// +0x1c98 à 1 ; pour un autre que le joueur, la cible est lâchée (FUN_004989c0(0)) et l'instant de la mort posé
	// (+0x18b8). Les stimulus, les statistiques du tueur (FUN_00458330, le joueur seul) et la partie perdue du joueur
	// (FUN_004311e0) ne sont pas suivis. Vrai si le piéton meurt.
	bool Mourir(int handle, uint32 maintenantMs = 0);
	// FUN_00478e30 (lua_PedSetStatsType, et la création avec l'entrée du modèle, modelinfo +0x54) : les 64 colonnes de
	// l'entrée de pedstats.dat copiées en +0x1900, la santé maximale = colonne F (entrée +0x2c), la santé = le maximum,
	// la santé minimale à 0, la régénération = colonne G (+0x30). Le reste (vision, armes, vitesse) n'est pas suivi.
	bool PoserStats(int handle, const int32 colonnes[64]);
	// GameGetPedStat (FUN_00476f10) : la colonne n ; le cas des modèles uniques de la colonne 6 n'est pas suivi.
	int32 Stat(int handle, int n) const;
	// La colonne n d'un piéton qui a reçu son entrée de pedstats.dat, `sansEntree` sinon (dans l'outil, le joueur, qui ne reçoit pas
	// encore STAT_PLAYER de son modèle 0 ; 0 pour une poignée invalide).
	int32 StatLue(int handle, int n, int32 sansEntree) const;
	// L'objet arme du piéton (+0x1d0, un CWeapon de 0x2d0 octets ; lu dans l'index le 06.11.2026). Son constructeur (FUN_004da610 :
	// la charge +0x128 à 0, les huit drapeaux +0x26c à 0 par FUN_004e8c20, +0x120 et +0x124 à -1, +0x140 et +0x264 à 0) donne les
	// valeurs de départ de CScriptPed.
	// RemettreArme : FUN_004d92c0 (avant chaque PropAttach, dans PropDetach, quand le tir vide l'arme, PedClearAllWeapons…) : +0x120
	// et +0x124 à -1 (_DAT_009009ac) ; s'il a un modèle (+0x10e), FUN_004da790(0) : plus de modèle (l'emplacement +0x20 de sa vftable
	// avec -1, FUN_004da340, FUN_004d9fa0 ; le modèle d'avant relâché, FUN_004da530) ; la charge, les drapeaux et +0x12c restent.
	void RemettreArme(int handle);
	// PrendreArme : FUN_004d92c0 puis FUN_004da6f0(modèle, 0) (PropAttach, FUN_0065fad0) : le modèle (WeaponRequestModel, toujours là
	// dans l'outil), +0x128 (la charge) à 0, +0x12c (la munition) au modèle, +0x140 à 0, +0x120 et +0x124 à -1, les huit drapeaux à 0
	// (FUN_004e8c20 sur +0x26c), +0x268 au second argument, +0x264 au type (WeaponGetType), puis le modèle posé (+0x20 de la
	// vftable). WeaponNeedInit (+0x120 à -1) fait ensuite jouer SetWeaponInfo, qui donne la vraie munition.
	void PrendreArme(int handle, int32 modele);
	// Le pas que CPed::ProcessControl (FUN_004812d0) passe au composant d'actions du piéton (+0x2e0) à chaque image, lu à
	// l'assembleur de 0x48192d à 0x481969 : (float)(0,02 (le flottant 0xa3396c) × CTimer::ms_fTimeStep (0xc1a9a4)) ×
	// (float)(la colonne 0x14 de ses stats (FUN_00476f10) / 100,0 (le double 0x900130)), des secondes. La colonne 0x14 est la V de
	// pedstats.dat, « Animation Speed factor (100 = 100% speed) » : 100 dans les 75 entrées. FUN_004850b0 le reçoit (rien si le pas
	// est nul et DAT_00c2b1cc vaut 1) et le passe à FUN_00485050, qui le donne à chaque contrôleur du composant, dont celui des actions
	// (FUN_00484040 → FUN_005f49a0 : le temps du nœud joué, +8, avance du pas ; FUN_005f4230 passe le pas à la mise à jour de chaque
	// piste vivante, l'emplacement +0xc de sa vftable : WeaponIncrementCharge, FUN_0065e810, l'ajoute à la charge).
	static float PasDuControleur(float pasTimer, int32 vitesseAnimation) {
		float a = 0.02f * pasTimer;
		float b = (float)((double)vitesseAnimation / 100.0);
		return a * b;
	}
	// Le même pour un piéton du pool : sa colonne V. Un piéton sans entrée de pedstats.dat (dans l'outil, le joueur, qui ne reçoit pas
	// encore STAT_PLAYER de son modèle 0) prend 100, la valeur de toutes les entrées, STAT_PLAYER compris.
	float PasDuControleur(int handle, float pasTimer) const;
	// GameSetPedStat (FUN_00476fa0) : la colonne n ; 4 (santé maximale) ramène la santé sous le nouveau maximum,
	// 5 pose la régénération. Les autres cas (2, 3, 0x10, 0x16, 0x17 : vision, vitesse) ne sont pas suivis.
	bool PoserStat(int handle, int n, int32 valeur);
	// FUN_0047b6b0 puis FUN_004782a0 : un coup de `degats` porté par `attaquant` (-1 : personne). Rien si la
	// cible a le drapeau 12 (+0x1344) ou est invulnérable (30, +0x138c), ou si l'attaquant est le joueur (type 13) et
	// la cible invulnérable au joueur (20, +0x1364) ; sinon la santé baisse, jamais sous 0 ni sous la santé minimale
	// (+0x1ca8).
	// Renvoie vrai si le coup a porté. FUN_004782a0 (lu en entier) : la santé tombée à 0 d'un piéton vivant, critique (+0x141c)
	// et qui a son rappel (+0x18e8, FUN_005d7f80) appelle ce rappel dans un thread neuf de son objet script (FUN_005db790,
	// FUN_005d9ac0) avec la poignée du piéton (l'emplacement 43 de CPed, +0xac), puis le lâche (FUN_005d9af0) : une fois.
	// Le pool range le piéton dans `critiquesAZero` et oublie le rappel ; lancer_lua l'appelle.
	bool Blesser(int cible, float degats, int attaquant);
	std::vector<int> critiquesAZero;
	// Les sorties anticipées de la méthode du coup (slot 15 de CPed, FUN_0047b810) avant qu'elle ne prévienne les scripts
	// (FUN_005b2190) : rien si la cible a le drapeau 12 (+0x1344, param_1[0x4d1]), si elle est invulnérable (30, +0x138c) sans
	// être le joueur, si l'attaquant est le joueur et la cible invulnérable au joueur (20, +0x1364). Ne sont pas suivis :
	// l'octet +0x1e94 du joueur, le coup plus faible qu'un coup reçu à la même image (le relevé +0x408, +0x434, +0x440). Une
	// cible dont l'attitude envers le type de l'attaquant vaut 4 n'est pas frappée par un coup sans arme (l'outil n'a pas
	// d'armes : toujours), sauf par le joueur sur sa cible verrouillée (param_14, FUN_004d5980 : non suivi, supposé faux).
	// Le choix d'une cible par l'IA (types autres que 13). Les AIButes qu'il lit (Globals.cat, DAT_020c889c) : +0x64 le temps
	// sans voir sa cible (6 s), +0x68 la portée de la visée (30 m), +0x6c le minimum de points de punition d'un agresseur (-1),
	// +0x70 et +0x74 la marge de l'attache (1 m, 2 pour la cible déjà prise).
	struct ButesCibles { float nonVuMax = 6.0f, portee = 30.0f; int32 punitionMin = -1; float marge = 1.0f, margeCible = 2.0f; };
	// FUN_00496b60 : la cible de l'objectif courant (FUN_004701f0) et son style (3 par défaut) : attaque (7) et 16, la cible +0x14
	// et le style +0x10 ; fuite (22), la cible fuie +0x10. Les autres (4, 8, 0xb, 0x13 à 0x15 : objets, véhicules) ne sont pas
	// suivis. -1 sans cible.
	int CibleDeLObjectif(int handle, int32 &style) const;
	// FUN_00496e60 : un agresseur qu'on peut prendre pour cible : un autre piéton, vivant ; si le piéton ne le hait pas (son attitude
	// envers le type de l'agresseur au-dessus de 0), seulement s'il est une autorité (FUN_0041a900 : types 0, 7, 8) et que
	// l'agresseur a au moins +0x6c points de punition (+0x1d40, 0 dans l'outil).
	bool AgresseurPossible(int handle, int agresseur, const ButesCibles &b) const;
	// FUN_00497080(cible, style, aPortee, vivante) pour un piéton de l'IA : rien sans cible, ni soi-même, ni un mort si `vivante`.
	// Les styles autres que 0 et 2 (FUN_00496960) s'arrêtent là : valable. Pour 0 et 2 : avec `aPortee`, à moins de +0x68 m
	// (FUN_00487d00) ; l'attache du piéton (FUN_004996f0, +0x34) admet la cible à sa marge près, faute d'attache toujours ; puis
	// valable pour les styles 2 et 3 (FUN_00496980), sans `aPortee`, ou si le piéton la voit (FUN_0048d320) ; sinon seulement
	// la cible déjà prise, vue il y a au plus +0x64 s. Le joueur que le script tient (+0x1e90 nul) n'est pas une cible pour une autorité
	// (FUN_0041a900 : types 0, 7, 8 ; lu en 0x497080, avant les styles). Non suivis : la conversation (PedInConversation), les zones interdites
	// (FUN_0046ee60), le modèle DAT_00bf42dc, la liste +0x198 du composant de visée (FUN_0048bf30), le joueur en voiture.
	bool CibleValable(int handle, int cible, int32 style, bool aPortee, bool vivante, const ButesCibles &b, int (*tirer)(void *), void *ctx);
	// FUN_004992a0 : la cible de l'objectif si elle est valable (FUN_004988b0, l'octet +0x1de5 levé pendant le jugement), sinon
	// la cible déjà prise si son style n'est ni 0 ni 2 (+0x1db4) ou si elle est récente (+0x1dc4), sinon l'agresseur des cinq
	// dernières secondes (FUN_00498930, _DAT_00900188) s'il est possible et valable (style 0). Verrouillé par FUN_00497d20 (+0x1db4
	// le style, +0x1db8 à 0, +0x1dc4 l'horloge). Non suivis : la riposte qui pousse un objectif d'attaque contre l'agresseur
	// (FUN_00496ee0) et les piétons proches notés (FUN_00498fc0, la liste +0x3c0).
	void ChoisirCible(int handle, uint32 maintenantMs, const ButesCibles &b, int (*tirer)(void *), void *ctx);
	// FUN_00499340, pour chaque piéton autre que le joueur : le temps sans voir sa cible avance (+0x1db8), une cible qui n'est plus
	// valable (FUN_00497080 avec son style, à portée, vivante) est lâchée (FUN_004989c0(0)), puis la cible est choisie de nouveau
	// (l'octet +0x1de4 levé à chaque image, FUN_004992a0). Un mort ne choisit plus (supposé).
	void SuivreCibles(float dt, uint32 maintenantMs, const ButesCibles &b, int (*tirer)(void *), void *ctx);
	// PedGetPedToTypeAttitude (0x475230) : l'attitude du piéton envers un type (sans piéton, 2).
	int32 Attitude(int handle, int type) const;
	CAttitudes attitudesTypes;     // le gestionnaire des attitudes entre types (FUN_0046f1f0)
	int32 respect[16] = {};        // +0x1b0 + 4 × faction des données du joueur (0xc1aea8) : SetFactionRespect, GetFactionRespect
	bool CoupRecevable(int cible, int attaquant) const;
	// FUN_00485fa0 (ecx = le meneur + 0x2f0, lu à l'assembleur de 0x485fa0 à 0x4860fc) : rien sans allié ; sans `forcer`, il
	// faut que l'allié ait été créé par la population (l'octet +0x1570 à 1 : non suivi, supposé), qu'il ne soit ni PREFECT (0),
	// ni COP (7), ni TEACHER (8), et que son attitude envers le meneur vaille 4 (FUN_004752b0). Déjà dans la chaîne des suiveurs
	// du meneur : vrai, rien ne change. Sinon l'ancien suiveur est renvoyé (FUN_00485880), l'allié devient le suiveur (+0x2f8),
	// reçoit +0x2fc 0, +0x300 0, +0x304 1, +0x305 1, +0x318 1, +0x31c 0, +0x308 0, +0x30c 0 et son meneur (+0x2f4) ; une
	// laisse de 4 m (le flottant 0x90d2cc) au premier meneur de la chaîne (FUN_00485410 ; FUN_00499960, le décalage nul de
	// 0xbcc564), +0x3b4 et +0x3b5 à 1 ; l'émotion 7 envers le meneur (FUN_0048f080 sur +0xb44, non suivie) ; son attitude
	// envers le type du meneur à 4 (FUN_00473080) ; +0x13c8 à 1 ; l'octet +0xc du groupe du meneur à `forcer` ; le marqueur et
	// la barre de santé de l'allié d'un joueur (FUN_00485e40, non suivis). Vrai si l'allié est dans le groupe.
	bool RecruterAllie(int meneur, int allie, bool forcer);
	// FUN_00485880 (ecx = le meneur + 0x2f0, lu à l'assembleur de 0x485880 à 0x4858e0) : le suiveur de l'allié d'abord, renvoyé
	// par l'allié lui-même (ecx = l'allié + 0x2f0, récursif) ; puis, si l'allié est le suiveur direct du meneur, son meneur
	// (+0x2f4) vidé, FUN_00485820 sur lui (lu de 0x485820 à 0x485879 : FUN_004852e0, le marqueur et la barre de santé ;
	// FUN_004851f0, le rappel Lua +0x141c ; +0x318, +0x13c8 et +0x14d0 à 1, +0x14ec et +0x153c à 0, la laisse lâchée s'il en a
	// une, FUN_004998f0) et le suiveur du meneur (+0x2f8) vidé. Vrai s'il était le suiveur direct.
	bool RenvoyerAllie(int meneur, int allie);
	// FUN_00485410 : le premier meneur de la chaîne (en remontant les +0x2f4), -1 sans meneur.
	int PremierMeneur(int handle) const;
	// FUN_00477ab0(genre, &point) (CPed, lu dans l'index) : l'entité qu'un piéton vise selon un genre, celle des pistes FaceTarget
	// (FUN_00620dc0, le genre +0x20) et SeekEntityPosition (FUN_0062a720, le genre +0x1c), de FUN_005feed0, FUN_005ff1d0… : 0 sa cible
	// (PedGetTargetPed : dans l'outil, la cible verrouillée), 1 son partenaire de prise (+0x155c), 4 son objet visé (+0x438, et son
	// point +0x43c rendu dans `point`), 0x10 son allié (FUN_004851e0 : un meneur vise son suiveur +0x2f8 ; sinon FUN_004851d0 : un
	// suiveur vise son meneur +0x2f4) ; les autres genres ne sont pas suivis (2 FUN_0048bf20, 3 +0x430, 6 le ramassage de son objectif
	// de type 0x17, 7 +0x544, 8 FUN_004d7630, 9 +0x4f0 + 4 × (+0x504), 10 FUN_004d6170, 0xc +0xd70, 0xd FUN_00496b60, 0xf +0x11ec) et
	// un genre inconnu ne vise rien. Rend un piéton (`pieton`) ou un objet animé (`objet`), -1 sinon.
	struct Cible { int pieton = -1; int objet = -1; uint32 point = 0; };
	Cible CibleDeGenre(int handle, int genre) const;
	// PedHasAlly(ped) (0x5cd730, lue à l'assembleur de 0x5cd730 à 0x5cd78c) : le piéton du genre 2 de l'objet script (FUN_005c7220) ;
	// vrai s'il a un meneur (+0x2f4) ou un suiveur (+0x2f8), faux sans piéton (FUN_005d8450 rend toujours un booléen).
	bool AUnAllie(int handle) const;
	// PedIsModel(ped, modèle) (0x5c92a0, lue à l'assembleur de 0x5c92a0 à 0x5c933d) : sans piéton, faux ; sinon le modèle du
	// piéton (le mot signé +0x10e) et l'entier du second argument (FUN_005d8200), tous deux passés en flottants et comparés
	// (fucompp : égaux, vrai).
	bool EstDuModele(int handle, int32 modele) const;
	// FUN_004853b0 (ecx = le meneur + 0x2f0) : `autre` est dans la chaîne des suiveurs du meneur.
	bool DansSesSuiveurs(int meneur, int autre) const;
	// FUN_00485440 (ecx = le piéton + 0x2f0) : `autre` est le piéton, un de ses meneurs ou un de ses suiveurs.
	bool DansSonGroupe(int handle, int autre) const;
	// lua_PedClearObjectives (0x5ca8e0) : la file des objectifs vidée (FUN_00470200, du premier au dernier : le rappel Lua
	// d'un objectif qui en a un est appelé avec son type, non suivi), le dernier agresseur oublié (FUN_004973d0(0) : +0x1dbc
	// à nul, +0x1dc0 à 0) ; lancer_lua arrête le contrôleur de l'IA (FUN_005f3f40). Le geste d'attaque de l'outil, que mène
	// l'objectif d'attaque, cesse avec lui.
	bool ViderObjectifs(int handle);
	// FUN_00490ef0 : le groupe d'humiliation (+0xd44) du premier piéton du pool, du dernier au premier, qui tombe entre
	// `de` et `a` ; -1 si aucun. Tous les piétons d'une même taille partagent ainsi le groupe déjà chargé.
	int GroupeHumiliationDans(int de, int a) const;
	// FUN_0048f5d0 : pose le groupe d'humiliation du piéton ; l'ancien, s'il est autre, est d'abord libéré
	// (FUN_0048dc60). Le moteur le fait seulement si le piéton a son objet animé (+0x114), supposé ici. Rend le groupe
	// libéré (-1 si aucun), dont l'appelant relâche les animations.
	int PoserGroupeHumiliation(int handle, int groupe);
	// FUN_0048dc60 (PedStopSocializing 0x490f50 l'appelle) : le groupe repasse à -1 ; rend l'ancien.
	int LibererGroupeHumiliation(int handle);
	// FUN_004925d0 : le choix du groupe d'une victime, `humiliateur` (le joueur ou un autre piéton) et `pourLeJoueur`
	// (l'octet du premier argument) donnés ; `taille` est celle de la victime, `tailleHumiliateur` celle de l'autre
	// (CharacterSize, modelinfo +0x74), `tirage` le rand() du CRT sur 32767. Le joueur (type 13) et l'octet : le groupe
	// déjà pris par un piéton de la même taille, sinon un tirage (Small 8 / 9, Medium 0 / 1 / 2, Large 5 / 6 / 7,
	// Fat 3 / 4, Huge 10 / 11 / 12, aux seuils 0,5 ou 0,33 et 0,66) ; le joueur sans l'octet : 17, 20 ou 14 (Medium, Large,
	// Huge) ; un autre piéton sans l'octet : par les deux tailles, 13 à 21. Rend le groupe posé, -1 si rien.
	int ChoisirGroupeHumiliation(int victime, int taille, int humiliateur, int tailleHumiliateur, bool pourLeJoueur, float tirage);
	// FUN_00492950 : une victime sans groupe (-1, ou au-delà de 12) dont la santé est sous la moitié de son maximum
	// (le double 0,5 de 0x8ff1f8) reçoit un groupe choisi pour le joueur (FUN_004925d0(1, joueur)). C'est le lancement
	// d'une piste ManageTargets (FUN_00627420) quand la cible du piéton est le joueur qui l'appelle. Rend le groupe posé,
	// -1 si rien.
	int SurveillerHumiliation(int victime, int taille, int tailleJoueur, float tirage);
	// PedJoinCombatZone (FUN_00487a70, ecx = le composant de combat de l'attaquant) : prendre la zone `zone` (0, 1, 2)
	// autour de `cible`. Refusé si la zone est masquée (une zone hors de 0 à 2 passe). Sauf `essai` (l'octet du dernier
	// argument : la condition JoinCombatZone ne fait que tester), la cible devient celle de l'attaquant (FUN_004989c0).
	// Vrai si l'attaquant y est déjà, ou si la zone a moins de combattants que sa capacité : alors, sauf `essai`, il
	// quitte sa zone d'avant et y entre (FUN_004878e0, FUN_00486c70, +0x66c). Le remplacement d'un combattant moins bien
	// placé (FUN_004863c0) et l'arme à distance (FUN_00486a90, seule la zone 2) ne sont pas suivis.
	bool JoindreZoneCombat(int handle, int cible, int zone, bool essai);
	// Les piétons menés par les deux manettes : DAT_00c2b120 (la manette 0), que le constructeur de CPlayerPed
	// (FUN_0049b1b0) pose au joueur, et DAT_00c2b124 (la manette 1), nul au démarrage ; PedSetControllerID(ped, n)
	// (FUN_005c9fc0) y range le piéton pour n = 0 ou 1 (une autre valeur ne fait rien), comme le lancement d'une piste
	// Controller (FUN_0061d860, l'octet +0x1c et la manette +0x18), que son arrêt (FUN_0061a890) remet à nul. -1 : aucun.
	bool PoserManette(int handle, int manette);
	int PietonDeLaManette(int manette) const { return manette == 0 || manette == 1 ? manettes[manette] : -1; }
	// La condition Controller (ConditionController, FUN_006006f0) : l'octet DAT_00a45a4c, qui vaut 1 dans le .data et
	// qu'aucune instruction n'écrit, la laisse passer ; avec l'octet +5, vrai si le piéton est mené par l'une des deux
	// manettes ; sinon par la manette 1 si l'octet +4, la manette 0 s'il est nul.
	bool MeneParLaManette(int handle, bool manette1, bool lesDeux) const;
	// La condition IsSocialStateWait (FUN_00603b80) : le piéton a un état social et un partenaire, et l'état est 2, 4 ou 6
	// (il attend la réponse de l'autre).
	bool AttenteSociale(int handle) const;
	// La condition IsSocializing (FUN_006039d0) : un état social et un partenaire ; avec `pourLeJoueur` (l'octet +4) et un
	// piéton joueur (type 13), vrai si un autre piéton du pool, du dernier au premier, est en échange avec lui (son
	// partenaire est le joueur) sans le bit 0 de son octet +0xd68.
	bool Socialise(int handle, bool pourLeJoueur) const;
	// Ce que fait la condition Attacked (FUN_00606b80) quand elle passe pour un piéton qui n'est pas le joueur et dont le
	// relevé du coup a un attaquant que sa liste (+0x198 du composant de visée, FUN_0048bf30 ; non suivie, vide) ne tient
	// pas : FUN_0048c3b0 en fait sa cible (+0x538 ; s'il en change, il quitte sa zone de combat, FUN_00487a20 et
	// FUN_00486c00), FUN_0048c330 la cible de son objectif (+0x540), FUN_00606b40 son agresseur (+0x544). Vrai si
	// quelque chose a été posé.
	bool PrendreAttaquant(int handle, int attaquant);
	// FUN_0048d320 (le composant de vue +0xa64) : le piéton voit-il `cible` ? Lui-même, oui ; le dernier piéton qui l'a frappé
	// (+0x1dbc), depuis moins de 5 s et à moins de 5 m (_DAT_00900188, FUN_00487d00), oui ; sinon la réponse du cache, ou, à
	// défaut, FUN_0048d0e0 (mise en cache pour 4 à 8 images, rand() × 5 / 0x7fff + 4) : la cible à portée à plat et dans le
	// cône (FUN_0048cf50 : produit scalaire du devant et de la direction, à plat, strictement au-dessus du cosinus du
	// demi-angle), puis la ligne de vue (FUN_00461d50, un rayon dans le monde) : l'outil n'a pas de monde, elle est dégagée.
	// Les cas du joueur (l'octet +0x1e91, la hauteur d'un piéton discret +0x1398) ne sont pas suivis. `tirer` : le rand().
	bool Voit(int handle, int cible, int (*tirer)(void *), void *ctx);
	// FUN_0048cf50 (le composant de vue, son +8 la portée) : le point est à portée, à plat (le carré de l'écart en x et y moins celui
	// de la portée n'est pas positif), puis dans le cône du devant (le produit scalaire du devant et de la direction à plat
	// strictement au-dessus du cosinus du demi-angle). Sans ligne de vue (FUN_0048d0e0 la cherche après).
	bool VoitPoint(int handle, const CVector &point) const;
	// Un projectile en vol de la table 0xc3a068 (32 entrées de 0x34 octets ; FUN_004d3c80 rend l'objet, DAT_00c3a6b8) tel que le
	// voient DetectProjectile et DetectGrenade : l'octet actif (-0x1c), le tireur (-0x2c), sa fiche ProjectileButes (+0, nulle
	// possible), et de la fiche : l'octet +0xe4 (228, la menace se voit), le rayon +0x94 (148), le flottant +0xa0 (160), les octets
	// +0x8c et +0x8d (140, 141), la vie +0x48 (72) ; de l'objet : sa position et sa vitesse (+0x124 … +0x12c, en m par pas).
	struct ProjectileVu {
		CVector pos, vitesse; int tireur = -1; bool fiche = true, menace = false, o8c = false, o8d = false;
		float rayon = 0.0f, a0 = 0.0f; int32 vie = 0;
	};
	// FUN_0048a300 (DetectProjectile, FUN_00604410 ; sur le composant +0x668 du piéton) : rien si le drapeau 19 du piéton (+0x1360) est
	// posé ; sinon, de chaque projectile actif qui n'est pas le sien, sans fiche ou d'une fiche à l'octet +0xe4, assez rapide (le carré
	// de sa vitesse moins 0,0004, le double 0x90fad8, n'est pas négatif), que le piéton voit (FUN_0048cf50), plus proche (le carré de la
	// distance, FUN_00472bb0) que le meilleur (1 000 au départ, 0x900f40), qui vient vers lui (le produit scalaire de (piéton −
	// projectile) et de la vitesse est positif) et dont la trajectoire à plat (la vitesse sans z normée, FUN_0041a3d0 ; le point le plus
	// proche du piéton sur elle) passe à moins de 3 m de lui à plat (FUN_0041a380 et le flottant 0x900c14) : vrai pour le plus proche.
	// Lu dans l'index le 06.11.2026.
	bool DetecterProjectile(int handle, const std::vector<ProjectileVu> &vus) const;
	// FUN_0048a130 (DetectGrenade, FUN_00604380) : rien avec le drapeau 19 ; sinon, de chaque projectile actif à fiche : le sien, de
	// rayon positif (+0x94) et plus loin que le rayon plus 0,5 (le carré de la distance au-dessus du carré de rayon + 0,5, le double
	// 0x8ff1f8) : vrai tout de suite ; sinon, d'une fiche à l'octet +0xe4, de rayon positif, de +0xa0 positif, avec l'octet +0x8c, le
	// +0x8d ou une vie positive, à une distance dont le carré ne dépasse pas le meilleur (_DAT_00900490 au départ) : vrai.
	// `pos` et `rayon` reçoivent la position et le rayon (+0x94) de la menace retenue ; le cas du tireur ne les touche pas.
	bool DetecterGrenade(int handle, const std::vector<ProjectileVu> &vus, float meilleurDepart, CVector *pos = nil, float *rayon = nil) const;
	// Le juge de DetectGrenade (FUN_00604380, lu à l'assembleur de 0x604380 à 0x604404) : la position et le rayon à 0 (fldz), puis
	// FUN_0048a130 avec le meilleur à 100 (_DAT_00900490) ; s'il rend vrai, vrai seulement si le piéton est à plat à moins du rayon
	// plus 1 (le double 0x8ff1d8) de la menace retenue (FUN_0041a380 ≤ 0). Quand FUN_0048a130 rend vrai pour le projectile du tireur,
	// la position reste (0, 0, 0) et le rayon est le mot de l'argument (le pointeur du contexte, un flottant minuscule ; l'outil prend
	// 0) : vrai seulement à moins de 1 m de l'origine du monde, faux en pratique.
	bool GrenadeAPortee(int handle, const std::vector<ProjectileVu> &vus) const;
	// L'image de la vue et du dernier agresseur : FUN_0048ccf0 (le cache vieillit d'une image) et FUN_00499340 (+0x1dc0
	// avance du pas, en secondes, s'il y a un agresseur, sinon 0).
	void AvancerVue(float dt);
	// FUN_004973d0 : `attaquant` devient le dernier piéton qui a frappé `handle`, le temps repart de 0.
	void Agresse(int handle, int attaquant);
	// FUN_005f38e0 (ecx = le composant d'actions +0x2e0) : vide les entrées du piéton : les 47 boutons (+0x20, 12 octets
	// chacun : la valeur passe en précédente puis à 0, l'octet d'appui à 0), l'intention (+0x254) et la vitesse de
	// l'intention de mouvement (+0x1c) ; la direction (+0) reste. Le lancement d'une piste IDLE (FUN_0061d8e0), l'IA qui
	// ne tourne pas (FUN_00484490), PlayerSetControl (0x5d12f0) et l'initialisation du composant (FUN_00484d10)
	// l'appellent. L'outil n'a que la vitesse ; l'intention, rangée par lancer_lua, ne vaut que pour l'image.
	void ViderEntrees(int handle);
	// FUN_00484490, la garde de l'image du contrôleur de l'IA : il n'avance pas (FUN_005f49a0) si le piéton est mort
	// (+0x1c98, posé par l'emplacement 42 de CPed, FUN_00479110, que lance la piste Death, FUN_0060fdc0), endormi
	// (+0x1358) ou si tous dorment (DAT_00c2b108, PedSetGlobalSleep 0x5c6e40) ; ses entrées sont alors vidées (et, mort,
	// son contrôleur arrêté, FUN_005f3f40).
	bool IAAvance(int handle) const;
	bool sommeilGeneral = false;   // DAT_00c2b108
	// PedStopSocializing(ped) (0x5cdbf0, lue à l'assembleur de 0x5cdbf0 à 0x5cdc1d : FUN_00490f50 sur le composant social +0xb44 du
	// piéton du genre 2 ; le jeu ne vérifie pas le piéton) ; FUN_00490f50 (lu dans l'index ; Ghidra le nomme PedStopSocializing) :
	// FUN_004906e0 (l'échange en cours avec le partenaire, non suivi), le partenaire (+0x70) à 0, les bits 0 à 4 et 7 de l'octet +0x224,
	// le bit 7 de +0x225 et le bit 0 de +0x226 effacés, l'état (+0x6c) et +0x118 à 0, +0xe0, +0xe4 et +0xe8 à 0x19, +0xfc à 0 ; le bit
	// 6 de +0x224 effacé si le +0x1e28 du piéton (+4) est sous 1 (FUN_00474940(0), FUN_004749e0(0) ; l'outil ne le suit pas : 0) ; le bit 1 de +0x225 si
	// FUN_004748e0 est faux ; les bits 0, 2, 3 et 6 de +0x225 et 1, 2 et 4 de +0x226 effacés, +0x12c à -1 si +0x226 avait le bit 1 sans
	// le bit 2, +0x134 à 0, +0x1f4 à 0x19, FUN_0048dc60 et FUN_0048dbc0 (la conversation), +0x20c à 0x76c (1 900), +0xec à 3 et le
	// +0x1494 du piéton à 0. L'outil garde l'état, le partenaire et l'octet +0x224 (drapeauxSociaux). Faux sans piéton.
	bool ArreterSocial(int handle);
	// lua_PedMakeAmbient (0x5ca850) : rendu à la population, plus ramassé par son script.
	bool RendreAmbiant(int handle);
	// PedMakeMissionChar(ped) (0x5ca890, lue à l'assembleur de 0x5ca890 à 0x5ca8d9) : FUN_0047b690(2, 1, 1) → FUN_004770a0(2, 10, 1,
	// 1) (CPed : qui l'a créé, l'octet +0x1570 ; lu dans l'index) : rien si l'octet vaut déjà 2 (un piéton de mission) ; sinon les
	// comptes de la population (FUN_00476470, FUN_0049fcf0) et l'octet à 2 ; puis le piéton est noté à l'objet script courant
	// (FUN_005db7e0, FUN_005d94d0), qui le ramasse à la fin de la mission. Dans l'outil, un piéton de mission est un piéton qui a un
	// script : un piéton rendu à la population (-1) passe au script `script`. Vrai s'il change.
	bool RendreMission(int handle, int32 script);
	// AreaClearAllPeds() (0x5b7e70, lue à l'assembleur de 0x5b7e70 à 0x5b7ecb) : le pool des piétons (DAT_00c0f5f0) parcouru depuis
	// la fin ; chaque piéton qui n'est ni de mission (l'octet +0x1570 à 2), ni le joueur (+0x1310 à 0xd), ni dans la liste de
	// DAT_00bcd098 (FUN_00424490 : jusqu'à +0x1804 entrées de 0x100 octets, la poignée en tête ; non suivie, vide) est détruit (la
	// première méthode de sa vftable, avec 1). Rend les poignées à détruire, dans cet ordre.
	std::vector<int> AmbiantsAVider(void) const;
	// Une image de dt secondes : marches, poursuites et coups ; pose les drapeaux d'événements. La régénération
	// (FUN_00472ff0, à chaque image depuis FUN_004812d0) : entre 0 et le maximum, la santé gagne
	// régénération / 100 × 0,02 × CTimer::ms_fTimeStep, le pas valant 1 pour 20 ms (FUN_0045b810) : régénération / 100
	// points par seconde, sans dépasser le maximum.
	void Avancer(float dt);
private:
	CScriptPed peds[SCRIPT_PED_POOL_SIZE];
	int manettes[2] = {SCRIPT_PED_JOUEUR, -1};
};

// La table des gestionnaires d'événements des piétons (RegisterPedEventHandler, 0x5bbdf0) : des paires (piéton, rappel Lua),
// dix en place (DAT_00cfaf28) puis sur le tas (DAT_00cfaf78), leur nombre en DAT_00cfaf80. Une paire n'a qu'une case de rappel :
// l'événement est un décalage de 4 octets dans la paire (FUN_005b4030 : +4 + 4 × événement), seul 0 tombe dans sa propre case.
// L'événement 0 est le coup reçu : la méthode du coup (FUN_0047b810) appelle FUN_005b2190, qui appelle le rappel du piéton
// frappé avec (sa poignée, celle de l'attaquant ; -1 sans attaquant), par l'emplacement 43 (+0xac) de CPed.
class CGestionnairesPietons
{
public:
	enum { SANS_RAPPEL = -2 };   // LUA_NOREF : une case vide (FUN_005d7f80 faux)
	struct Paire { int ped; int rappel; };
	// FUN_005b4600 : ajoute le piéton au bout s'il n'y est pas (FUN_005b4590 agrandit le tas en doublant).
	void Ajouter(int ped);
	// FUN_005b4030 : la case du rappel du piéton, nil s'il n'y est pas (le moteur rend alors la case de la première paire).
	int *Case(int ped);
	// FUN_005b42d0 : vide la case du piéton (FUN_005b3fe0) et retire la paire (FUN_005b4130, les suivantes avancent d'un
	// cran) ; rend l'ancien rappel, SANS_RAPPEL sinon. Rien pendant l'appel des rappels (DAT_00cfaf84).
	int Retirer(int ped);
	// FUN_005b40b0 : le piéton a un rappel pour l'événement 0.
	bool Pret(int ped) const;
	std::vector<Paire> paires;
	bool enAppel = false;        // DAT_00cfaf84
};

// Les gestionnaires des événements des déclencheurs (RegisterTriggerEventHandler 0x5bbfa0, lu à l'assembleur : Ghidra ne l'a
// pas en fonction). Deux tables de même forme : la table 0 (DAT_00cfb068, son tas DAT_00cfb180, sa capacité en plus
// DAT_00cfb184, son nombre DAT_00cfb188) pour un quatrième argument nul, la table 1 (DAT_00cfb18c, DAT_00cfb2a4, DAT_00cfb2a8,
// DAT_00cfb2ac) sinon, et sans quatrième argument. Une entrée fait 0x1c octets : le déclencheur (+0), puis une case de rappel par
// événement (+4 + 4 × événement, six). Dix entrées en place, puis le tas (FUN_005b4fc0) ; le propriétaire de la table (l'objet
// script +0x44 de SetNumberOfHandledTriggerEventObjects) en DAT_00cfb2b4 / DAT_00cfb2b8. Les événements, lus dans
// FUN_005b26b0, FUN_005b2140 et FUN_005b2770 : 0 un piéton entre, 1 le joueur entre, 2 un projectile (non suivi), 3 un piéton
// sort, 4 le joueur sort, 5 un piéton (ou le joueur) est dedans.
class CGestionnairesDeclencheurs
{
public:
	enum { SANS_RAPPEL = -2, EVENEMENTS = 6, EN_PLACE = 10, PAIRES = 100 };
	enum { ENTREE_PIETON = 0, ENTREE_JOUEUR = 1, PROJECTILE = 2, SORTIE_PIETON = 3, SORTIE_JOUEUR = 4, DEDANS = 5 };
	struct Entree {
		int declencheur = -1;
		int rappels[EVENEMENTS] = {SANS_RAPPEL, SANS_RAPPEL, SANS_RAPPEL, SANS_RAPPEL, SANS_RAPPEL, SANS_RAPPEL};
		int scripts[EVENEMENTS] = {-1, -1, -1, -1, -1, -1};   // le script du rappel (FUN_005d7f50), pour FUN_005b5ae0
	};
	struct Table {
		std::vector<Entree> entrees;
		int enPlus = 0;              // la capacité du tas, au-delà des dix entrées en place
		int proprietaire = -1;       // DAT_00cfb2b4 / DAT_00cfb2b8
	};
	// SetNumberOfHandledTriggerEventObjects(n[, table]) (FUN_005bbeb0 → FUN_005b5e50) : une table qui a des entrées et le même
	// propriétaire est vidée (FUN_005b5a60) ; si elle en a encore, rien ; sinon, au-delà de dix, le tas prend n - 10 entrées
	// (FUN_005b55b0) et le script devient le propriétaire. Un seul argument : la table 1.
	void Dimensionner(int table, int n, int script);
	// FUN_005b5d10 : l'entrée du déclencheur, ajoutée au bout si elle n'y est pas et s'il reste de la place (dix + le tas) ;
	// rend faux si la table est pleine.
	bool Ajouter(int table, int declencheur);
	// FUN_005b4ff0 : la case de l'événement de ce déclencheur, nil s'il n'a pas d'entrée (le moteur rend alors la case de la
	// première entrée).
	int *Case(int table, int declencheur, int evenement);
	// FUN_005b5100 : un rappel est posé pour cet événement de ce déclencheur (l'événement sous 6).
	bool Pret(int table, int declencheur, int evenement) const;
	// FUN_005b56f0 : vide la case (FUN_005b4f20) ; l'entrée sans plus aucun rappel est retirée (FUN_005b54c0, les suivantes
	// avancent d'un cran). Rien pendant l'appel des rappels (DAT_00cfb2b0). Rend l'ancien rappel, SANS_RAPPEL sinon.
	int Retirer(int table, int declencheur, int evenement);
	// FUN_005b5280 : l'entrée i a un rappel pour l'un des événements 0, 1, 3, 4 ou 5 (pas le projectile) : ses piétons sont
	// suivis à chaque image.
	bool Suivie(int table, int i) const;
	// FUN_005b5ae0 (FUN_005b2680, la fin d'un objet script) : la table dont il est le propriétaire est vidée et perd son
	// propriétaire ; dans l'autre, ses rappels sont vidés (FUN_005b4f70) et les entrées vides retirées. Rend les rappels lâchés.
	std::vector<int> OublierScript(int script);
	Table tables[2];
	bool enAppel = false;        // DAT_00cfb2b0
	// Les paires (déclencheur, piéton) dedans à l'image précédente (l'objet 0x20c7e98 : +4, 100 au plus, leur nombre +0x324).
	struct Paire { int declencheur; int ped; };
	std::vector<Paire> dedans;
	// FUN_006d6f60 (ecx = 0x20c7e98), appelée par la mise à jour de CPed (FUN_004812d0, en 0x481c45) pour chaque piéton : pour
	// chaque entrée suivie des deux tables (la table 0 d'abord), si le déclencheur ne suit que le joueur (le bit 2 de +0x3f,
	// AreaSetTriggerMonitoringRules 0x5b7f60) et que le piéton n'est pas le joueur, rien ; sinon, `dans` dit s'il est dedans
	// (FUN_006d6e50 sur sa position, FUN_00415310). Dehors et dans la liste : sa sortie (FUN_005b2770) et la paire retirée ;
	// dedans et dans la liste : l'événement 5 (FUN_005b2140) ; dedans et pas dans la liste (qui n'est pas pleine) : son entrée
	// (FUN_005b26b0), l'événement 5, et la paire ajoutée. Les événements à appeler sont rendus dans l'ordre (table, déclencheur,
	// événement) ; FUN_005b1ec0 appelle chaque rappel posé dans les deux tables.
	struct Appel { int table; int declencheur; int evenement; };
	template<class Dans, class SuitJoueur>
	std::vector<Appel> Suivre(int ped, bool joueur, Dans dans, SuitJoueur suitJoueurSeul);
};


template<class Dans, class SuitJoueur>
std::vector<CGestionnairesDeclencheurs::Appel>
CGestionnairesDeclencheurs::Suivre(int ped, bool joueur, Dans dans, SuitJoueur suitJoueurSeul)
{
	std::vector<Appel> appels;
	// FUN_005b26b0 / FUN_005b2770 : l'entrée et la sortie du joueur sont les événements 1 et 4, celles d'un autre piéton 0 et
	// 3, appelés dans la table 0 puis dans la table 1 (FUN_005b1ec0) ; avant elles, les gestionnaires globaux de stimuli
	// (FUN_005b1e20 : 4 et 5 pour le joueur, 2 et 3 sinon), que l'outil ne suit pas.
	auto evenement = [&](int t, int e){ for(int k = 0; k < 2; k++) if(Pret(k, t, e)) appels.push_back({k, t, e}); };
	for(int table = 0; table < 2; table++){
		for(int i = 0; i < (int)tables[table].entrees.size(); i++){
			if(!Suivie(table, i)) continue;
			int t = tables[table].entrees[i].declencheur;
			if(suitJoueurSeul(t) && !joueur) continue;
			size_t j = 0;
			while(j < dedans.size() && !(dedans[j].declencheur == t && dedans[j].ped == ped)) j++;
			if(!dans(t)){
				if(j < dedans.size()){
					evenement(t, joueur ? SORTIE_JOUEUR : SORTIE_PIETON);
					dedans.erase(dedans.begin() + j);
				}
			}else if(j < dedans.size()){
				evenement(t, DEDANS);
			}else if(dedans.size() != PAIRES){
				evenement(t, joueur ? ENTREE_JOUEUR : ENTREE_PIETON);
				evenement(t, DEDANS);
				dedans.push_back({t, ped});
			}
		}
	}
	return appels;
}

