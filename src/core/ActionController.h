// Le contrôleur d'actions d'un piéton (+0x10 : le nœud joué), tel que l'outil le fait tourner.
//
// Relevé dans bully.exe : FUN_005dc040 pose l'arbre (PedSetActionTree) et le contrôleur y descend (FUN_005f5490 pour
// une banque, FUN_005f5500 pour un nœud jouable) ; une piste vit dans son nœud de son instant d'entrée (+12) pendant
// sa durée de vie (+16, -1 : sans limite) ; le nœud joué se compare par PedIsPlaying (FUN_005f3bf0).
// Règle de l'outil, en attendant les pistes Opportunity qui décident vraiment des passages : à chaque image,
//   quand le nœud joué est fini (toutes ses pistes à durée finie terminées) ou qu'une demande (ActionRequest)
//   attend, un enfant du nœud joué qui aboutit (conditions jugées) le remplace : la suite d'un coup, d'une
//   séquence ; sinon le contrôleur redescend depuis l'arbre posé. Un nœud sans fin attend une demande.
#pragma once
#include "ActionBank.h"

class CActionController
{
public:
	const CActionNode *arbre = nil;   // l'arbre posé par PedSetActionTree
	const CActionNode *joue = nil;    // le nœud joué (+0x10)
	float debut = 0.0f;               // l'instant où le nœud joué a commencé
	float fin = -1.0f;                // l'instant où il se termine (-1 : jamais, il attend)

	// PedSetActionTree : l'arbre posé, la descente faite tout de suite. Faux si rien n'aboutit (le nœud ne change pas).
	bool Poser(const CActionBank &b, const CActionNode *a, const CJugeAction &juge, float t);
	// PedSetActionNode, la manette de l'outil : ce nœud devient le nœud joué.
	void Jouer(const CActionBank &b, const CActionNode *n, float t);
	// Une image : vrai si le nœud joué a changé. `demande` : une ActionRequest attend.
	bool Avancer(const CActionBank &b, const CActionNode *depuisRacine, const CJugeAction &juge, float t, bool demande = false);
	float Temps(float t) const { return t - debut; }   // ce que ActionNodePlayTime mesure
	// La fin d'un nœud : le plus tard des (entrée + durée de vie) de ses pistes à durée finie ; -1 si aucune n'en a.
	static float Duree(const CActionBank &b, const CActionNode &n);
private:
	const CActionNode *Descendre(const CActionBank &b, const CActionNode *depuis, const CJugeAction &juge) const;
};
