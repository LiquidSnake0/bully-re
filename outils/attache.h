// Les objets tenus : un modèle rigide accroché à un point d'attache d'un
// piéton animé (docs/mxd.md). Les points viennent de Models/Peds/MXDs.MGR,
// les instants des pistes PropAttachEx / PropDetachEx des arbres d'actions
// (docs/cat.md).
#pragma once
#include "scene.h"
#include "../src/anim/Mxd.h"

namespace outil {

// a ∘ b : b exprimé dans le repère de a.
inline NifTransform Composer(const NifTransform &a, const NifTransform &b){
	NifTransform x;
	for(int i = 0; i < 3; i++) for(int j = 0; j < 3; j++){
		x.r[i][j] = 0; for(int k = 0; k < 3; k++) x.r[i][j] += a.r[i][k] * b.r[k][j];
	}
	x.s = a.s * b.s;
	x.t = NifApply(a, b.t);
	return x;
}

// Le repère du point dans celui du modèle, d'après la dernière pose (Reposer) :
// la transformation de l'os, composée de la position et de la rotation du point.
inline bool RepereDuPoint(const Anime &an, const CMxdPoint &p, NifTransform *out){
	if(p.numeroOs < 0 || p.numeroOs >= AGR_OS) return false;
	int32 b = an.noeuds[p.numeroOs];
	if(b < 0 || b >= (int32)an.mondes.size()) return false;
	NifTransform local = NifIdentity();
	AgrMatrice(p.q, local.r);
	local.t = CVector(p.pos[0], p.pos[1], p.pos[2]);
	*out = Composer(an.mondes[b], local);
	return true;
}

// Accroche `objet` (un modèle rigide gardé par AjouterModele) au point du
// piéton `porteur`, dans le monde.
inline bool Accrocher(Scene &s, const Anime &porteur, const CMxdPoint &p, Anime &objet){
	NifTransform r;
	if(!RepereDuPoint(porteur, p, &r)) return false;
	objet.place = Composer(porteur.place, r);
	s.Replacer(objet);
	return true;
}

}
