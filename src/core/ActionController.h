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
	struct Contact { const CActionNode *porteur; float debut, fin; uint32 drapeau; float degats; std::string surCoup; bool fait; };
	std::vector<Contact> contacts;
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
	struct Drapeau { float entree; uint32 numero; int32 valeur; bool fait; };
	std::vector<Drapeau> drapeaux;
	std::vector<Drapeau> drapeauxPoses;
	float finExecutees = -1.0f;   // la fin la plus tardive des pistes venues des nœuds exécutés (-1 : aucune)
	struct Prise { const CActionNode *soi, *cible; bool executerSoi, executerCible; };
	std::vector<Prise> prises;                    // celles qui se lancent à cette image, rendues par Avancer
	uint16 graine = 0;                // le mot +0x10c du piéton, pour le tirage (FUN_00465910) ; posé par l'appelant
	uint32 image = 0;                 // le compteur d'images du jeu (DAT_00c1a99c, CTimer), tenu par l'appelant
	uint32 hasard = 1;                // la graine du rand() du CRT (OpportunityRandomLatch)
	const CActionNode *demandeNoeud = nil;   // la demande retenue à cette image (FUN_005f3bc0 : +0x14)
	int32 demandePriorite = -1;              // sa priorité (+4)

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
private:
	void Pistes(const CActionBank &b, const CActionNode &n, float entree, int prof, const CJugeAction *juge);
	const CActionNode *Descendre(const CActionBank &b, const CActionNode *depuis, const CJugeAction &juge) const;
};
