// Les groupes d'animations .agr de Stream/World.img (docs/agr.md).
//
// Un fichier est une suite d'animations. Chacune : u32 0x100, u32 type (999 à
// 1004, l'encodage des images clés), u32 nombre d'images clés, u32 0, f32
// durée en secondes, puis les images clés, puis des enregistrements de
// position (u16 index d'image clé, i16 x, y, z en millimètres) jusqu'à
// l'animation suivante. L'image clé désignée donne à la fois l'instant et
// l'os : dans un pas de marche, la clé 1 (Root) porte le bassin, la clé 35
// (ARROW) la flèche au sol.
//
// Type 1002 (2 772 animations sur 3 443), décodé comme bully.exe le fait en
// 0x6b1710 : une image clé tient en 8 octets, deux mots w0, w1 :
//   w0 bits 0-10  index de l'image clé PRÉCÉDENTE du même os (0 = aucune)
//      bits 11-19 instant, en 511e de la durée
//      bit 20     signe de x ; bits 21-30 |x| en 1023e ; bit 31 signe de y
//   w1 bits 0-9   |y| ; bit 10 signe de z ; bits 11-20 |z| ; bit 21 signe de w ;
//      bits 22-31 |w|
// Les 36 premières images clés sont la pose de départ des 36 os, dans l'ordre
// des nœuds du squelette sous « Dummy » (Dummy, Root, Root Pelvis… ARROW).
// Une clé de norme nulle (bras et mains de 63 animations) est une absence.
#pragma once
#include "../common.h"
#include <vector>
#include <array>

enum { AGR_OS = 36 };

struct AgrCle { float t; float q[4]; };          // instant (s), quaternion w, x, y, z
struct AgrPosition { float t; CVector p; };      // instant (s), position (m) dans le repère du parent

struct AgrAnim {
	int32 type = 0;
	int32 numCles = 0;
	int32 numOs = 0;                             // pistes : 36 pour un piéton, moins pour un objet
	float duree = 0;
	bool decodee = false;                        // les six types (999 à 1004) se décodent
	std::vector<AgrCle> pistes[AGR_OS];          // par os, dans l'ordre du temps
	std::vector<AgrPosition> positions[AGR_OS];  // par os : l'os de l'image clé que chaque enregistrement désigne
};

// Découpe un groupe en animations et les décode, quel que soit le type.
bool AgrLireGroupe(const uint8 *buf, uint32 taille, std::vector<AgrAnim> &out);

// Rotation d'un os à l'instant t (s, ramené dans la durée), interpolée
// sphériquement entre les deux images clés qui l'encadrent. Faux si l'os n'a
// pas d'image clé.
bool AgrRotation(const AgrAnim &a, int32 os, float t, float q[4]);

// Position d'un os à l'instant t, interpolée linéairement. Faux si l'os n'a
// pas de piste de position (dans les animations de piétons : Root, le bassin,
// et ARROW, la flèche au sol dont le trajet est le déplacement du personnage).
bool AgrPositionOs(const AgrAnim &a, int32 os, float t, CVector *p);

// Déplacement de la flèche (ARROW, os 35) sur toute l'animation : ce dont le
// personnage avance à chaque cycle. (0, 0, 0) pour une animation sur place.
CVector AgrDeplacement(const AgrAnim &a);

// Interpolation sphérique entre deux quaternions (w, x, y, z), par le plus
// court chemin ; u = 0 donne a, u = 1 donne b.
void AgrSlerp(const float a[4], const float b[4], float u, float out[4]);

// Quaternion (w, x, y, z) → matrice 3 × 3, dans la convention des NiAVObject
// (docs/nif.md) : la matrice lue ligne par ligne redonne ce quaternion.
void AgrMatrice(const float q[4], float m[3][3]);
