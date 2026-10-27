// Le contrôleur d'actions d'un piéton (+0x10 : le nœud joué), tel que l'outil le fait tourner.
//
// Relevé dans bully.exe : FUN_005dc040 pose l'arbre (PedSetActionTree) et le contrôleur y descend (FUN_005f5490 pour
// une banque, FUN_005f5500 pour un nœud jouable) ; une piste vit dans son nœud de son instant d'entrée (+12) pendant
// sa durée de vie (+16, -1 : sans limite) ; le nœud joué se compare par PedIsPlaying (FUN_005f3bf0).
// Les passages viennent des pistes Opportunity du nœud joué (src/core/Opportunite.h, FUN_005f7d70 / FUN_005f7440 /
// FUN_005f3bc0) : chaque image, chacune tente sa chance ; une piste prise qui s'arrête dépose une demande, et la plus
// forte priorité l'emporte (à priorité égale, la première déposée).
// Quand le nœud joué finit, ses pistes Sequence s'arrêtent (table 0x93…, arrêt FUN_005f7030) : chacune descend dans
// son nœud visé et dépose une demande de priorité 0 (FUN_005f3bc0) ; la première qui aboutit l'emporte.
// Règle de l'outil en secours, faute des pistes Sequence et des autres passages pas encore lus : quand le nœud joué
//   est fini (toutes ses pistes à durée finie terminées) ou qu'une demande (ActionRequest) attend sans qu'une
//   Opportunity ait fait passer, un enfant du nœud joué qui aboutit le remplace, sinon le contrôleur redescend depuis
//   l'arbre posé. Un nœud sans fin attend une demande.
#pragma once
#include "ActionBank.h"
#include "Opportunite.h"

class CActionController
{
public:
	const CActionNode *arbre = nil;   // l'arbre posé par PedSetActionTree
	const CActionNode *joue = nil;    // le nœud joué (+0x10)
	float debut = 0.0f;               // l'instant où le nœud joué a commencé
	float fin = -1.0f;                // l'instant où il se termine (-1 : jamais, il attend)
	std::vector<COpportunite> occasions;   // les pistes Opportunity du nœud joué, vivantes
	std::vector<std::string> suites;       // les chemins de ses pistes Sequence, dans l'ordre
	std::vector<const CActionNode *> suitesExecutees;   // les nœuds visés par les Sequence des nœuds exécutés, après
	// Ses pistes PlayOnTarget (ExecuteOnTargetTrack, mise à jour FUN_006615f0) : à leur instant d'entrée, le nœud
	// visé (chemin relatif) est joué par la cible du piéton (FUN_004d5980, un piéton : FUN_005f4500), une seule fois.
	struct SurCible { float entree; std::string chemin; bool fait; };
	std::vector<SurCible> surCible;
	std::vector<const CActionNode *> pourCible;   // les nœuds à faire jouer à la cible, rendus par Avancer (vidés à chaque image)
	// Ses pistes Collision (CollisionTrack, table 0x92decc ; mise à jour FUN_005f9e70), y compris celles du nœud qu'une
	// piste Execute fait jouer à côté (« ./Collision » sous HeavyAttacks) : le coup porte entre +0x18 (24) et +0x1c
	// (28) du temps du nœud. Le moteur cherche alors le contact entre les sphères des os de l'attaquant et les
	// piétons proches ; l'outil n'a pas les os : le coup est rendu à l'appelant, qui le porte à la cible verrouillée.
	// +0x2c (44) le drapeau du coup (« GROUNDHITHEAVY »), passé à la méthode du coup de la cible (slot 15 de CPed,
	// FUN_0047b810) ; +0x34 (52) les dégâts ; +0x68 (104) un nœud de l'attaquant, joué si le coup porte sur un
	// piéton (FUN_005f4490), relatif au nœud qui porte la piste (supposé : +0x78, 120, n'est pas suivi).
	// La phase de l'attaque du piéton (+0x71c) et son échéance (+0x720, en millisecondes du jeu, DAT_00c1a9b4), que la
	// piste tient : son lancement (FUN_005f9480) pose la phase 1, l'octet +0x4e5 (CollisionStart) et l'échéance au début
	// de la fenêtre (maintenant + (+0x18) s) ; sa mise à jour (FUN_005f9e70), une fois le temps du nœud à (+0x18), pose
	// la phase 2 et l'échéance maintenant + (+0x1c) s dans la fenêtre (ou si +0x1c est négatif), puis, passé +0x1c, la
	// phase 3 et l'échéance maintenant + la durée de vie (+0x10) si elle est positive, sinon maintenant + ce qui reste de
	// l'animation (FUN_006b1df0 ; l'outil : la fin du nœud) ; son arrêt (FUN_005f8240) remet la phase à 0 (et +0xe4).
	// WeaponCollisionTrack (0x9382f0, lancement FUN_00661660) fait de même pour une arme.
	struct Contact { const CActionNode *porteur; float debut, fin; uint32 drapeau; float degats; std::string surCoup; bool fait;
	                 float entree = 0.0f, vie = -1.0f, debutFenetre = 0.0f, finFenetre = -1.0f, debutBrut = 0.0f, finBrut = -1.0f;
	                 bool lancee = false, arretee = false; };
	std::vector<Contact> contacts;
	int32 phaseAttaque = 0;          // +0x71c du piéton
	uint32 echeanceAttaque = 0;      // +0x720
	// +0x4e5 : posé à 1 par le lancement d'une piste Collision ou WeaponCollision (0x5f94aa, 0x661694) ; vérifié à
	// l'assembleur, aucune instruction ne le remet à 0 dans un CPed (la seule autre écriture, 0x6f2d68, est dans
	// FUN_006f2d10, sur un autre objet) : une fois qu'un piéton a lancé un coup, CollisionStart reste vraie.
	bool collisionDemarree = false;
	// +0x550 du piéton (+0x194 du composant de visée +0x3bc) : l'instant (ms) de son dernier coup, que l'arrêt d'une piste
	// Collision pose (FUN_005f8240 → FUN_0048c040(0) : maintenant moins 0), comme PlayerRecordAttackTime (0x5d2910) ;
	// 0 au départ. La condition AttackTimer le lit et peut le reposer.
	uint32 minuterieAttaque = 0;
	uint32 maintenantMs = 0;         // l'horloge du jeu (DAT_00c1a9b4) à la dernière image des pistes
	std::vector<Contact> touches;                 // les coups qui portent à cette image, rendus par Avancer
	// Ses pistes DamageHandle (HandleDamageTrack, table 0x9313e4 ; lancement FUN_0060fc70), y compris celles des nœuds
	// que ses Execute font jouer (HitTree/…/Base/HitBase) : à leur lancement, le piéton subit les dégâts du coup reçu
	// (le relevé +0x408 du contrôleur, ses dégâts en +0x38 : +0x440) multipliés par +0x18 (24), portés par
	// l'attaquant du relevé (+4 : +0x40c), par FUN_0047b7f0 → FUN_0047b6b0. L'octet +0x1c (28) ajoute un effet de
	// coup (FUN_0059b750), que l'outil ne joue pas.
	struct Blessure { const CActionNode *porteur; float entree; float facteur; bool effet; bool fait; };
	std::vector<Blessure> degats;
	std::vector<Blessure> blessures;              // celles qui se lancent à cette image, rendues par Avancer
	// Ses pistes SetHealth (SetHealthTrack, table 0x9350b0 ; lancement FUN_00627500) : la valeur +0x1c (28) pose la
	// santé (octet +0x18, FUN_00478190 : jamais sous la santé minimale), la rend (+0x19, FUN_00474b70 : jusqu'au
	// maximum) ou l'ôte (+0x1a, FUN_004782a0 : sans les invulnérabilités).
	struct Sante { float entree; int mode; float valeur; bool fait; };   // mode 0 poser, 1 rendre, 2 ôter
	std::vector<Sante> santes;
	std::vector<Sante> santesLancees;             // celles qui se lancent à cette image, rendues par Avancer
	// Ses pistes SetTargetAsGrappleTarget (table 0x… ; lancement FUN_0062a580) : le piéton et sa cible deviennent
	// partenaires de prise (PedSetGrappleTarget, +0x1500 = l'octet +0x18), le piéton descend dans le nœud +0x24 (36) et
	// la cible dans le nœud +0x34 (52), relatifs au nœud qui porte la piste ; chacun le joue (FUN_005f4490), ou en
	// exécute les pistes si l'octet +0x19 / +0x1a est posé (FUN_005f3e20).
	struct PriseCible { float entree; std::string soi, cible; bool executerSoi, executerCible, fait; const CActionNode *porteur = nil; };
	std::vector<PriseCible> prisesCible;
	// Ses pistes SetPedFlags (PedFlagsTrack 0x931380, lancement FUN_0060f8e0) : le drapeau +0x1c (28) du piéton reçoit
	// l'octet +0x18 (24). Une entrée négative (-1) se lance quand le nœud s'arrête (FUN_005f3fb0 avec son troisième
	// argument). Les lancées s'accumulent dans `drapeauxPoses`, que l'appelant applique et vide.
	// Ses pistes Death (DeathTrack 0x934aa0, lancement FUN_0060fdc0) : le piéton meurt (l'emplacement 42 de CPed,
	// FUN_00479110, avec l'octet +0x18), y compris celles des nœuds exécutés (HitTree/…/Dead exécute Base/DeadCleanup,
	// dont la piste Death est à 0) ; une entrée négative se lance à l'arrêt du nœud (Player.act, Humiliate_Me/…/MISSION).
	// Les lancées s'accumulent dans `mortsLancees`, que l'appelant applique et vide.
	struct Mort { float entree; uint8 octet; bool fait; };
	std::vector<Mort> morts, mortsLancees;
	struct Drapeau { float entree; uint32 numero; int32 valeur; bool fait; };
	std::vector<Drapeau> drapeaux;
	std::vector<Drapeau> drapeauxPoses;
	// Ses pistes SpeechTrack (table 0x932ed8, lancement FUN_006195a0) : l'événement de parole +0x18 (24), la ligne +0x20
	// (32) si l'octet +0x1c (28) est posé (sinon -1, une ligne au hasard), la tenue +0x28 (40) : non nulle, la ligne est
	// préchargée et retenue (une place parmi les cinq de 0x1b655d0, le rappel 0x619550) jusqu'à ce qu'une piste
	// SpeechStartTrack du piéton la lâche (lancement FUN_00619950) ou que son échéance passe (12 s si la cible du
	// piéton est le joueur ; alors la tenue 1 la joue, 2 l'annule). Une entrée négative se lance à l'arrêt du nœud.
	// Les lancées s'accumulent dans `parolesLancees` et `departsLances`, que l'appelant traite et vide.
	struct Parole { float entree; int32 evenement, ligne, tenue; bool fait; };
	std::vector<Parole> paroles, parolesLancees;
	struct Depart { float entree; bool fait; };
	std::vector<Depart> departs;
	int departsLances = 0;
	// Ses pistes FaceTarget (FaceTargetTrack 0x934b00, mise à jour FUN_00620dc0) : tant qu'elles vivent, le piéton se
	// tourne vers sa cible de genre +0x20 (32 ; FUN_00477ab0 : 0 la cible verrouillée, 1 le partenaire de prise +0x155c),
	// décalée de +0x28 (40) degrés. Ses pistes SeekEntityPosition (0x933634, lancement FUN_0062a720, mise à jour
	// FUN_0062a940) : le piéton va vers sa cible de genre +0x1c (28) jusqu'à moins de +0x28 (40) mètres. L'outil prend
	// aussi celles des nœuds qu'une piste spawn (SpawnTrack 0x92de80) fait jouer à côté, sans le reste de ces nœuds.
	// `regardsActifs` et `poursuitesActives` : celles qui vivent à cette image, rendues par Avancer.
	struct Regard { float entree, vie; int32 genre; float decalage; };
	std::vector<Regard> regards, regardsActifs;
	struct Poursuite { float entree, vie; int32 genre; float arrivee; };
	std::vector<Poursuite> poursuites, poursuitesActives;
	// Les pistes de l'IA (relevées dans AI_FightTutorial), suivies quand `sousControleurs` est posé :
	// - ManageTargets (ManageTargetsTrack 0x934d14 ; lancement FUN_006273f0, mise à jour FUN_00627420, à chaque image tant
	//   qu'elle vit) : l'octet +0x18 (24) fait du joueur la cible du piéton (FUN_004989c0) ; la cible (+0x538, FUN_0048bee0)
	//   est surveillée : si c'est le joueur, sa victime peut recevoir son groupe d'humiliation (FUN_00492950). Elle vit tant
	//   que le piéton a une cible.
	// - CombatLoco (CombatLocoTrack 0x9349e0 ; lancement FUN_0062e9e0, mise à jour FUN_0062ea40) : le piéton gagne sa zone
	//   de combat autour de sa cible (FUN_006252d0 la rejoint, PedJoinCombatZone ; FUN_006253e0 en prend les rayons dans
	//   AIButes ; FUN_0062e120 pose l'intention de mouvement). L'octet +0x18 (24) vise le joueur. Elle vit tant que le piéton
	//   a une cible et une zone.
	// - ActionNodeRequest (ActionNodeRequestTrack 0x9334dc ; lancement FUN_0061bb40, mise à jour FUN_006262a0 →
	//   FUN_006261f0) : tant que le piéton ne joue pas le nœud nommé +0x18 (24) dans son contrôleur d'actions, l'intention
	//   (+0x254 du composant +0x2e0, FUN_005f35e0) reçoit ce nom à chaque image ; une fois joué (l'octet +0x24 en mémoire),
	//   la piste vit tant qu'il le joue si l'octet +0x1d (29), sinon elle rend l'octet +0x1e (30) ; pas encore joué, elle vit
	//   si +0x1c (28) ou +0x1d.
	// L'octet +8 d'une piste retient le nœud (FUN_005f4230 : le nœud finit quand plus aucune piste vivante à l'octet +8 ne
	// reste ; une piste meurt quand sa mise à jour rend faux ou que sa durée de vie +0x10 est passée). L'outil s'en sert
	// pour ces trois pistes, quand le nœud n'a pas d'autre fin : il finit quand elles sont toutes mortes.
	// Les pistes des objectifs, suivies dans tous les contrôleurs (HumiliateFlee les porte) :
	// - ClearCurrentObjective (0x936094 ; lancement FUN_0061bbf0, mise à jour FUN_0062a180 → FUN_00470110) : retire de la
	//   file des objectifs ceux du type +0x18 (24), puis meurt (sa mise à jour rend faux) ;
	// - FleeObjective (0x935578 ; lancement FUN_00628ce0) : un FleeObjective (type 22) fuyant la cible du piéton (+0x538 ;
	//   +0x544 si l'octet +0x19 (25) ; d'abord le +4 du +0x10 du contexte, non suivi), mis devant la file (FUN_00471320),
	//   ou au bout si l'octet +0x18 (24) (CPed::PushObjective) ; rien sans cible.
	// - IDLE (IdleTrack 0x934cd8 ; lancement FUN_0061d8e0, mise à jour FUN_0061d850) : au lancement, le composant d'actions
	//   du piéton vide ses entrées (FUN_005f38e0, CScriptPedPool::ViderEntrees : la vitesse de l'intention de mouvement
	//   retombe à 0 ; vérifié à l'assembleur, ecx = +0x2e0 en 0x61d97b) ; pour un piéton à vélo (+0x1570 = 1), le vélo
	//   (+0x1554) est aussi arrêté (non suivi). Sa mise à jour la garde en vie tant que son piéton existe : un nœud IDLE
	//   (AMBIENT/IDLE d'AI_FightTutorial, à l'octet +8) ne finit pas de lui-même. `reposLances` : celles lancées à cette image.
	// - SetWeaponInfo (SetWeaponInfoTrack 0x935d04 ; lancement FUN_00660f50) : pose les réglages de l'arme du piéton : sur
	//   son objet arme (+0x1d0, un CWeapon), +0x120 = le champ +0x2c (44), +0x124 = +0x40 (64), +0x12c = l'arme du nom +0x44
	//   (68, FUN_0051ad50) ou son modèle ; sur son composant de visée (+0x1c8), les angles +0x18, +0x20, +0x30 (en degrés,
	//   passés en radians) et les champs voisins (non suivis). La condition WeaponNeedInit (FUN_0065b7b0) attend +0x120 = -1 :
	//   la banque NeedInit de Weapons.cat ne passe qu'une fois. `infosArmeLancees` : celles lancées à cette image.
	struct InfoArme { float entree, init, champ40; bool fait; };
	std::vector<InfoArme> infosArme, infosArmeLancees;
	struct Repos { float entree; bool fait; };
	std::vector<Repos> repos;
	int reposLances = 0;
	struct Effacement { float entree; int32 type; bool fait; };
	std::vector<Effacement> effacements, effacementsLances;
	struct Fuite { float entree; bool auBout, autreCible, fait; };
	std::vector<Fuite> fuites, fuitesLancees;
	struct Gestion { float entree, vie; bool joueur, retient; };
	std::vector<Gestion> gestions, gestionsActives, combats, combatsActifs;
	struct DemandeNoeud { float entree; uint32 nom; bool tenir, suivre, rendre, retient, vu, vivante; };
	// - ActionRequest (ActionRequestTrack 0x9348a0 ; lancement FUN_0061ba70, mise à jour FUN_006260d0, arrêt FUN_00626160) :
	//   à chaque image, appuie sur le bouton brut +0x18 (24) du composant d'actions dans le mode +0x1c (28) (FUN_005f3930 :
	//   3 lâché, 2 appuyé et tenu, sinon appuyé) ; le bouton 12 pose aussi l'instant de la parade (+0x4d0, que lit la
	//   condition BlockTimer, FUN_00600160), les boutons 1, 2, 3 et 8, pendant un nœud de HitTree, la minuterie du dernier
	//   coup (+0x550, non suivi). Elle vit si l'octet +0x20 (32) ; à l'arrêt, un mode 2 lâche le bouton (le mode 3).
	//   `appuis` : les boutons appuyés à cette image (bouton, mode) ; `laches` : ceux lâchés, que l'appelant vide.
	struct DemandeBouton { float entree; int32 bouton, mode; bool vivante, retient, morte, lancee; };
	std::vector<DemandeBouton> demandesBouton;
	std::vector<std::pair<int32, int32>> appuis;
	std::vector<int32> laches;
	std::vector<DemandeNoeud> demandesNoeud;
	std::vector<uint32> intentions;               // les noms posés en intention à cette image, rendus par Avancer
	bool finParRetenues = false;                  // le nœud finit à la mort de ses pistes de l'IA qui le retiennent
	bool retenu = false;                          // une piste du nœud (ou d'un nœud exécuté) a l'octet +8
	bool parLaManette = false;                    // posé par l'appelant le temps d'un Entrer / Jouer de sa manette
	// Ses pistes spawn (SpawnTrack 0x92de80 ; lancement FUN_005f8860, mise à jour FUN_005f72e0, arrêt FUN_005f7390) : la
	// piste a son propre contrôleur (+0x84) et son contexte (+0x2c, dont la racine +0x40 est le nœud visé +0x6c, résolu
	// d'avance) ; au lancement, le contrôleur descend dans le nœud visé et joue ce qu'il trouve ; à chaque image il avance
	// (FUN_005f49a0), et quand son nœud finit, l'octet +0x29 (41) le fait redescendre depuis le nœud visé (sinon il
	// s'arrête) ; l'octet +0x28 (40) le fait avancer dès le lancement. La durée de vie +0x10 (16) l'arrête. Suivies quand
	// `sousControleurs` est posé (les arbres de l'IA) ; sinon l'outil n'en garde que les FaceTarget et SeekEntityPosition.
	struct Lance { float entree, vie; const CActionNode *vise; bool boucle, lancee, morte; std::shared_ptr<CActionController> c; };
	std::vector<Lance> lances;
	bool sousControleurs = false;
	// Ce contrôleur et ceux de ses pistes spawn vivantes, en profondeur : l'appelant en traite les sorties.
	void Recueillir(std::vector<CActionController *> &tous);
	float finExecutees = -1.0f;   // la fin la plus tardive des pistes venues des nœuds exécutés (-1 : aucune)
	struct Prise { const CActionNode *soi, *cible; bool executerSoi, executerCible; };
	std::vector<Prise> prises;                    // celles qui se lancent à cette image, rendues par Avancer
	uint16 graine = 0;                // le mot +0x10c du piéton, pour le tirage (FUN_00465910) ; posé par l'appelant
	uint32 image = 0;                 // le compteur d'images du jeu (DAT_00c1a99c, CTimer), tenu par l'appelant
	uint32 hasard = 1;                // la graine du rand() du CRT (OpportunityRandomLatch)
	const CActionNode *demandeNoeud = nil;   // la demande retenue à cette image (FUN_005f3bc0 : +0x14)
	int32 demandePriorite = -1;              // sa priorité (+4)
	bool finDemandee = false;                // FUN_005f3bc0 avec son dernier octet (+1) : une Opportunity prise s'est arrêtée,
	                                         // le nœud joué finit, même sans nœud demandé (FUN_005f7440, FUN_005f49a0)

	// La pile du contexte (+0x44 le haut, +0x48 les clés) : FUN_005f36d0 pose la racine (+0x40) et y met sa clé
	// (FUN_005f4e40, le nom haché de l'arbre de tête de la racine) ; FUN_005f3700 en empile une seconde (le partenaire de
	// prise qui joue un nœud de l'arbre de son meneur, FUN_005f7c70). Le haut est la clé des parents (DAT_00b0a604) quand
	// le contrôleur résout un chemin. Vide : -1.
	std::vector<uint32> pile;
	uint32 Cle(void) const { return pile.empty() ? 0xffffffffu : pile.back(); }
	void Racine(const CActionBank &b, const CActionNode *r);
	void Empiler(uint32 k);
	// PedSetActionTree : l'arbre posé, la descente faite tout de suite. Faux si rien n'aboutit (le nœud ne change pas).
	bool Poser(const CActionBank &b, const CActionNode *a, const CJugeAction &juge, float t);
	// Ce nœud devient le nœud joué. Ses pistes Execute (lancement FUN_005f7070) font descendre le nœud visé (+0x4c, avec
	// le juge s'il est donné, sans lui sinon) et ajoutent les pistes du nœud obtenu à celles du contrôleur
	// (FUN_005f3e20, la même liste que le nœud joué : pas de contrôleur à part) ; chacune se lance quand le temps du nœud
	// atteint son entrée (FUN_005f3fb0). L'outil en garde les Opportunity, les Collision et les DamageHandle.
	void Jouer(const CActionBank &b, const CActionNode *n, float t, const CJugeAction *juge = nil);
	// PedSetActionNode (FUN_004849a0 joue le nœud par l'emplacement +0x4c, la descente) et la manette de l'outil :
	// une banque descend jusqu'au nœud qui aboutit (FUN_005f5490), un nœud jouable se joue lui-même. Faux si rien
	// n'aboutit ; le nœud joué ne change pas alors.
	bool Entrer(const CActionBank &b, const CActionNode *n, const CJugeAction &juge, float t);
	// Une image : vrai si le nœud joué a changé. `demande` : une ActionRequest attend.
	bool Avancer(const CActionBank &b, const CActionNode *depuisRacine, const CJugeAction &juge, float t, bool demande = false);
	float Temps(float t) const { return t - debut; }
	// FUN_005f7d70 puis, à l'arrêt, FUN_005f7440 : une image des pistes Opportunity du nœud joué. Rend le nœud de la
	// demande retenue, ou nil.
	const CActionNode *Occasions(const CActionBank &b, const CJugeAction &juge, float t);   // ce que ActionNodePlayTime mesure
	// La fin d'un nœud : le plus tard des (entrée + durée de vie) de ses pistes à durée finie ; -1 si aucune n'en a.
	static float Duree(const CActionBank &b, const CActionNode &n);
	// Les pistes Collision à une image (FUN_005f9480, FUN_005f9e70, FUN_005f8240) : la phase et l'échéance ci-dessus.
	void PhasesCollision(float t);
	// L'arrêt des pistes Collision lancées et pas encore arrêtées (FUN_005f8240), quand le nœud joué change.
	void ArreterCollisions(void);
private:
	void Pistes(const CActionBank &b, const CActionNode &n, float entree, int prof, const CJugeAction *juge);
	void PistesLancees(const CActionBank &b, const CActionNode &n, float entree, int prof, const CJugeAction *juge);
	const CActionNode *Descendre(const CActionBank &b, const CActionNode *depuis, const CJugeAction &juge) const;
};

// La condition CollisionStart (ConditionCollisionStart, table 0x936a30, FUN_00602260) : l'octet +0x4e5 du piéton.
bool ConditionCollisionStart(const CActionController &c);
// La condition AttackPhase (ConditionAttackPhase, table 0x9369d0, FUN_00601fc0) : la phase de l'attaque du piéton est
// +4 ; si le délai +8 est positif, il faut aussi une échéance posée et qu'elle tombe plus de +8 secondes après
// maintenant (maintenant + 1000 × (+8), arrondi, strictement avant l'échéance).
bool ConditionAttackPhase(const CActionController &c, int32 phase, float delai, uint32 maintenantMs);
// La condition AttackTimer (ConditionAttackTimerPlayTime, table 0x9369e4, FUN_006000c0) : faux si le piéton n'a jamais
// frappé (+0x550 nul) et que l'horloge n'a pas passé 2 000 ms ; sinon maintenant comparé à +0x550 + 1000 × (+4) par le
// comparateur +0xc (FUN_005fb750) ; vrai et l'octet +8 posé, la minuterie repart (FUN_0048c040(0)).
bool ConditionAttackTimer(CActionController &c, float delai, bool relancer, uint32 comparateur, uint32 maintenantMs);
