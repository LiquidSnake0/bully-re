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

// Matrice d'un placement « Ipl$ » : le quaternion (x, y, z, w) des .ipb donne
// R par la formule transposée de l'usuelle, c'est son conjugué qui tourne v.
// Vérifié sur les modèles dont le nœud porte la même rotation que leur
// placement (docs/ipl.md, tests/test_placement). L'échelle par axe est
// appliquée avant la rotation, dans les colonnes de R.
void NifPlacementRotation(const float q[4], float r[3][3]);
NifTransform NifFromPlacement(const CVector &pos, const CVector &scale, const float q[4]);

// Appelée pour chaque forme rencontrée depuis la racine, avec sa
// transformation composée jusqu'à l'espace du modèle.
typedef void (*NifShapeFn)(const CNifFile &f, int32 bloc, const NifGeometry &g,
                           const NifGeometryData &d, const NifTransform &x, void *ctx);
//
// `espaceEntite` : sous « Scene Root », un nœud porte le nom du modèle et sa
// transformation est celle que l'entité remplace par sa propre matrice. Les
// grandes pièces d'intérieur y gardent leur position monde, certains props
// un décalage d'artiste (DPI_Teacup : −38,7 en x). En mode entité, la racine
// et ses enfants directs sont composés comme l'identité ; c'est l'espace des
// collisions et des placements. Voir docs/nif.md.
void NifWalkShapes(const CNifFile &f, NifShapeFn fn, void *ctx, bool espaceEntite = true);
