// Composition des transformations d'un arbre NIF et parcours des formes.
//
// Convention vérifiée sur les données (voir docs/nif.md, « Sens des
// rotations ») : un NiAVObject transforme un point local p en
//     R · (s · p) + t
// avec R lue ligne par ligne telle qu'elle est stockée. Sur les 3 842 modèles
// appariés à leur collision, seule cette convention donne des boîtes
// englobantes quasi identiques ; la transposée n'en donne aucune.
#pragma once
#include "../common.h"
#include "NifFile.h"

struct NifTransform {
	float r[3][3];
	float s;
	CVector t;
};

NifTransform NifIdentity(void);
CVector NifRotate(const float r[3][3], const CVector &v);
NifTransform NifCompose(const NifTransform &parent, const NifAVObject &o);
CVector NifApply(const NifTransform &x, const CVector &v);

// Appelée pour chaque forme rencontrée depuis la racine, avec sa
// transformation composée jusqu'à l'espace du modèle.
typedef void (*NifShapeFn)(const CNifFile &f, int32 bloc, const NifGeometry &g,
                           const NifGeometryData &d, const NifTransform &x, void *ctx);
void NifWalkShapes(const CNifFile &f, NifShapeFn fn, void *ctx);
