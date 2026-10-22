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
	// Ses pistes PlayOnTarget (ExecuteOnTargetTrack, mise à jour FUN_006615f0) : à leur instant d'entrée, le nœud
	// visé (chemin relatif) est joué par la cible du piéton (FUN_004d5980, un piéton : FUN_005f4500), une seule fois.
	struct SurCible { float entree; std::string chemin; bool fait; };
	std::vector<SurCible> surCible;
	std::vector<const CActionNode *> pourCible;   // les nœuds à faire jouer à la cible, rendus par Avancer (vidés à chaque image)
	uint16 graine = 0;                // le mot +0x10c du piéton, pour le tirage (FUN_00465910) ; posé par l'appelant
	uint32 image = 0;                 // le compteur d'images du jeu (DAT_00c1a99c, CTimer), tenu par l'appelant
	uint32 hasard = 1;                // la graine du rand() du CRT (OpportunityRandomLatch)
	const CActionNode *demandeNoeud = nil;   // la demande retenue à cette image (FUN_005f3bc0 : +0x14)
	int32 demandePriorite = -1;              // sa priorité (+4)

	// PedSetActionTree : l'arbre posé, la descente faite tout de suite. Faux si rien n'aboutit (le nœud ne change pas).
	bool Poser(const CActionBank &b, const CActionNode *a, const CJugeAction &juge, float t);
	// Ce nœud devient le nœud joué.
	void Jouer(const CActionBank &b, const CActionNode *n, float t);
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
	const CActionNode *Descendre(const CActionBank &b, const CActionNode *depuis, const CJugeAction &juge) const;
};
