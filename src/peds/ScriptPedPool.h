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
enum { SCRIPT_PED_DRAPEAUX = 128, DRAPEAU_INTOUCHABLE = 12, DRAPEAU_INVULNERABLE_JOUEUR = 20, DRAPEAU_INVULNERABLE = 30 };

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
enum { OBJECTIF_ATTAQUER_PIETON = 7, OBJECTIF_ATTAQUER_16 = 16, OBJECTIF_FUIR = 22, SCRIPT_PED_OBJECTIFS = 20 };
struct CObjectifPieton { int32 type = -1; int cible = -1; int32 style = 0; };

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
	// +0x1310 : le type du piéton (PREFECT 0 … COP 7, TEACHER 8, BULLY 11, DEFAULT 12, PLAYER1 13 ; CModelEnum::typesDePietons),
	// recopié du modelinfo (+0x4c) par le constructeur de CPed (FUN_0047ffe0), changé par PedSetFaction (0x5cb570, jamais en
	// 13), lu par PedGetFaction (0x5cb5c0), IsPlayer (FUN_005ffe40 : 13), IsAuthority (FUN_005ffb20) et PedType (FUN_005ffac0).
	int32 type = SCRIPT_PED_TYPE_DEFAUT;
	// +0x18d0 : la réserve de sprint, posée à la création à la colonne R de l'entrée de pedstats.dat (+0x5c de l'entrée,
	// FUN_00478e30), son maximum ; FUN_0047cde0 la remplit à chaque image (jusqu'à ce maximum), FUN_0047cee0 la dépense.
	// La condition Sprint la compare (FUN_005fda70). L'outil ne la fait pas varier.
	float sprint = 0;
	bool discretion = false;     // +0x1398 : un piéton de mission de discrétion (PedSetIsStealthMissionPed, 0x5d0110 →
	                             // FUN_005d0080) ; la condition IsStealthMissionChar le lit (FUN_00602cf0)
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
	bool masqueZones[3] = {true, true, true};
	int32 capaciteZones[3] = {1, 2, 4};
	std::vector<int> combattants[3];
	int cibleObjectif = -1;      // +0x540 (+0x184 du composant de visée en +0x3bc) : la cible d'un objectif d'attaque, que la
	                             // condition Objective pose quand elle passe (FUN_0048c330)
	bool mort = false;
	// Le geste en cours.
	// L'intention de mouvement du composant d'actions (+0x2e0) : sa direction (+0, +4, +8) et sa vitesse (+0x1c), que la
	// navigation pose à chaque image (FUN_004a4a50) et remet à zéro sans but (FUN_004a45c0) ; les conditions
	// MotionIntentionSpeed (FUN_005fd810) et MotionIntention (ConditionMotionIntentionDirection, FUN_005fd760) la lisent.
	// Les seuils de Locomotion.act (Free) : walk ≥ 0,1, RUN ≥ 0,5, Sprint ≥ 1,05. L'outil la pose quand il fait marcher
	// un piéton (Aller : la vitesse de son allure ; arrivé, arrêté : 0) et dans CombatLoco (lancer_lua).
	CVector intention;
	float intentionVitesse = 0;
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
	// lua_PedSetHealth : la santé ; à zéro, le piéton meurt et lâche son geste.
	void Sante(int handle, float sante);
	// FUN_00478e30 (lua_PedSetStatsType, et la création avec l'entrée du modèle, modelinfo +0x54) : les 64 colonnes de
	// l'entrée de pedstats.dat copiées en +0x1900, la santé maximale = colonne F (entrée +0x2c), la santé = le maximum,
	// la santé minimale à 0, la régénération = colonne G (+0x30). Le reste (vision, armes, vitesse) n'est pas suivi.
	bool PoserStats(int handle, const int32 colonnes[64]);
	// GameGetPedStat (FUN_00476f10) : la colonne n ; le cas des modèles uniques de la colonne 6 n'est pas suivi.
	int32 Stat(int handle, int n) const;
	// GameSetPedStat (FUN_00476fa0) : la colonne n ; 4 (santé maximale) ramène la santé sous le nouveau maximum,
	// 5 pose la régénération. Les autres cas (2, 3, 0x10, 0x16, 0x17 : vision, vitesse) ne sont pas suivis.
	bool PoserStat(int handle, int n, int32 valeur);
	// FUN_0047b6b0 puis FUN_004782a0 : un coup de `degats` porté par `attaquant` (-1 : personne). Rien si la
	// cible a le drapeau 12 (+0x1344) ou est invulnérable (30, +0x138c), ou si l'attaquant est le joueur (type 13) et
	// la cible invulnérable au joueur (20, +0x1364) ; sinon la santé baisse, jamais sous 0 ni sous la santé minimale
	// (+0x1ca8).
	// Renvoie vrai si le coup a porté.
	bool Blesser(int cible, float degats, int attaquant);
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
	// lua_PedMakeAmbient (0x5ca850) : rendu à la population, plus ramassé par son script.
	bool RendreAmbiant(int handle);
	// Une image de dt secondes : marches, poursuites et coups ; pose les drapeaux d'événements. La régénération
	// (FUN_00472ff0, à chaque image depuis FUN_004812d0) : entre 0 et le maximum, la santé gagne
	// régénération / 100 × 0,02 × CTimer::ms_fTimeStep, le pas valant 1 pour 20 ms (FUN_0045b810) : régénération / 100
	// points par seconde, sans dépasser le maximum.
	void Avancer(float dt);
private:
	CScriptPed peds[SCRIPT_PED_POOL_SIZE];
	int manettes[2] = {SCRIPT_PED_JOUEUR, -1};
};
