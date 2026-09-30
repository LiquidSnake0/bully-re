// Le squelette d'un modèle animé : transformations de tous les nœuds, pose
// (une rotation en plus par os), et déformation des sommets par leurs os.
//
// Formule vérifiée sur les données (docs/nif.md, « Le squelette ») : un
// sommet v d'une forme à peau devient
//     Σ_os poids · (M_os ∘ S_os)(v)
// où M_os est la transformation de l'os dans le modèle (celle du nœud, comme
// pour une forme) et S_os la transformation de liaison lue dans NiSkinData,
// sans transposition. La transformation d'ensemble de NiSkinData est
// l'identité sur les piétons essayés et n'entre pas dans le calcul. Avec les
// os dans leur pose du fichier, on retrouve les sommets tels qu'ils sont
// stockés : 2,3 mm d'écart au plus sur Jimmy (PLAYER), 5,7 cm sur Zoe, dont
// le squelette est rangé dans une pose un peu différente de la liaison.
#pragma once
#include "NifTransform.h"

// Transformation de chaque bloc (nœuds et formes) dans l'espace du modèle,
// avec les mêmes règles que NifWalkShapes. `pose` (facultatif, numBlocks
// matrices) ajoute à chaque nœud une rotation locale, appliquée après la
// sienne : c'est ce qui plie un os et entraîne ses enfants. Les blocs qui ne
// sont ni nœud ni forme gardent l'identité.
void NifWorldTransforms(const CNifFile &f, NifTransform *out, const NifMatrix33 *pose = nil, bool espaceEntite = true);

// Sommets déformés d'une forme à peau, dans l'espace du modèle (`out` :
// numVertices). Un sommet sans poids suit la transformation de sa forme.
// Rend faux si la forme n'a pas de peau lisible.
bool NifSkinVertices(const CNifFile &f, int32 bloc, const NifTransform *mondes, CVector *out);

// Le nœud qui porte ce nom, ou −1.
int32 NifFindNode(const CNifFile &f, const char *nom);

// Rotation d'un angle (radians) autour d'un axe du repère local (0 x, 1 y, 2 z).
NifMatrix33 NifAxisRotation(int32 axe, float angle);
