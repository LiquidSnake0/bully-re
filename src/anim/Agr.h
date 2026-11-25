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

// FUN_006073a0(indice, sortie) (méthode du HXD, ret 8, lue en entier de 0x6073a0 à 0x60741d, 126 octets) : **le trajet
// d'une animation**, celui dont la couche de locomotion tire la vitesse naturelle (FUN_00730340) : la position de l'os 1
// à la dernière image moins celle de la première (FUN_006b3cc0 avec l'os 1, appelée avec l'image 0 puis −1, et la
// différence par 00414a20). FUN_006b3cc0(indice, os, sortie, image) (méthode du HXD, ret 0x10, lue en entier de 0x6b3cc0
// à 0x6b3fe9, 810 octets) rend la position d'un os à une image d'un enregistrement (0x40 octets, ses drapeaux en +0x18) :
// la racine est l'os 1, ou l'os 2 si l'enregistrement porte le drapeau 0x200. Positions à part (drapeau 8) : pour la
// racine, le premier enregistrement de position, ou le dernier du premier lot (leur nombre est le mot +0x2a ; 8 octets,
// un mot et trois entiers de 16 bits en millimètres, le double 0x900fa0 = 0,001, avec les drapeaux 1 ou 2 ; 16 octets et
// trois réels sinon) ; un second lot (le mot +0x2c) est celui du dernier os, avec le drapeau 0x40 ; tout autre os rend sa
// position de repos (la table +0x70 du HXD, 12 octets par os). Positions dans les clés (sans le drapeau 8) : la clé
// image + os, ou pour la dernière image la clé du mot +0x2e (la racine) ou +0x30. Le drapeau 0x100 ajoute à la racine le
// vecteur +0x34 de l'enregistrement : il s'annule dans la différence.
// Ici : la dernière position de l'os 1 moins sa première, dans l'animation décodée ; **avec le drapeau 0x200 l'os 1
// n'est plus la racine, les deux lectures rendent sa position de repos et le trajet est nul**. Supposé : la dernière
// position décodée de l'os 1 est le dernier enregistrement du premier lot. Non refait : les enregistrements de 16 octets
// (AgrLireGroupe lit des enregistrements de 8 octets pour tous les types sans translation dans la clé).
// **Le déplacement qu'une place rend à chaque image** (les trois réels que chaque évaluation de couche rend à côté de la
// pose, mêlés par 00412850 dans 006be730 et 006bd150).
// FUN_00412850(sortie, a, b) (lue en entier, 37 octets) : la somme de deux vecteurs. FUN_00414a20(sortie, a, b) (lue en
// entier, 37 octets) : leur différence, a − b.
// L'os dont le déplacement est pris (le début de FUN_006b45d0, de 0x6b45e7 à 0x6b460a, et le même dans FUN_006b4820 et
// FUN_006b47b0) : le dernier os du dictionnaire (+0x5c − 1 : ARROW, l'os 35 d'un piéton) si l'enregistrement porte le
// drapeau 0x40, sinon la racine, l'os 1, ou l'os 2 avec le drapeau 0x200.
inline int32 AgrOsDuDeplacement(uint32 drapeaux, int32 numOs) { return (drapeaux & 0x40) ? numOs - 1 : (drapeaux & 0x200) ? 2 : 1; }
// FUN_006b45d0(sortie, lecteur, place, position) (ret 0x18, lue en entier de 0x6b45d0 à 0x6b47a1, 466 octets) : le
// déplacement d'une place entre son instant d'avant (+8) et son instant (+4). `ici` est la position de l'os à l'instant
// (donnée par l'appelante), `avant` celle de l'instant d'avant (FUN_006be250 sur l'enregistrement, à +8).
// - Si l'instant n'est pas strictement sous l'instant d'avant (fcompp, test ah,5, jp), ou si la place ne boucle pas
//   (le bit 1 de +0x10) : ici − avant.
// - Sinon la place vient de reboucler : la position de la dernière image (FUN_006b3cc0 avec l'image −1), moins celle
//   de la première (FUN_006b3cc0 avec l'image 0) **seulement quand l'os n'est pas la racine** (le cas du drapeau
//   0x40), puis (fin − avant) + ici.
// - À la fin, si l'os est la racine (1, ou 2 avec le drapeau 0x200), la troisième composante est mise à zéro : le
//   bassin ne monte ni ne descend le piéton.
inline CVector
AgrDeplacementPlace(const CVector &ici, const CVector &avant, bool reboucle, const CVector &derniere, const CVector &premiere, bool racine)
{
	CVector d;
	if(!reboucle) d = CVector(ici.x - avant.x, ici.y - avant.y, ici.z - avant.z);
	else{
		CVector fin = derniere;
		if(!racine) fin = CVector(fin.x - premiere.x, fin.y - premiere.y, fin.z - premiere.z);
		d = CVector((fin.x - avant.x) + ici.x, (fin.y - avant.y) + ici.y, (fin.z - avant.z) + ici.z);
	}
	if(racine) d.z = 0.0f;
	return d;
}
// FUN_006b4820(couche, sortie, lecteur, place) (ret 0xc, lue en entier de 0x6b4820 à 0x6b48b8) : la position de l'os du
// déplacement à l'instant de la place (FUN_006be250 sur l'enregistrement, à +4), puis FUN_006b45d0. FUN_006b47b0 (ret
// 0x10, lue en entier, 106 octets) fait de même avec la translation de cet os prise dans une pose déjà évaluée (+0x18
// de son élément de 0x24 octets). Ici sur l'animation décodée ; faux si l'os n'y a pas de position.
// Supposé : la dernière et la première image que rend FUN_006b3cc0 sont le dernier et le premier enregistrement de
// position décodés de l'os. Non refait : le drapeau 0x100 (le vecteur +0x34 de l'enregistrement, que FUN_006b3cc0
// ajoute à la racine) : aucune des 3 365 animations de MAINPED.HXD ne le porte, ni le drapeau 0x200 ; 1 072 portent le
// drapeau 0x40 (test_hxd).
inline bool
AgrDeplacementImage(const AgrAnim &a, uint32 drapeaux, float t, float avant, bool boucle, CVector *d)
{
	int32 os = AgrOsDuDeplacement(drapeaux, a.numOs);
	*d = CVector(0.0f, 0.0f, 0.0f);
	if(!a.decodee || os < 0 || os >= AGR_OS || a.positions[os].empty()) return false;
	// Un instant égal à la durée (la place s'y arrête une image : « la durée juste atteinte n'est pas passée »,
	// FUN_006b2560) prend la dernière clé, pas la première : AgrPositionOs ramène l'instant modulo la durée, et l'instant
	// d'avant égal à la durée faisait compter, à l'image du tour de boucle, le trajet entier une seconde fois (3,17 m en
	// une image pour C_PLAYER\RUN, mesuré le 25.11 dans 1_06_01). Supposé : le décodage du jeu (006be250, non lu) rend
	// la dernière clé à cet instant.
	auto position = [&a, os](float u, CVector *p){
		if(a.duree > 0.0f && u >= a.duree && u <= a.duree * 1.000001f){ *p = a.positions[os].back().p; return true; }
		return AgrPositionOs(a, os, u, p);
	};
	CVector ici, av;
	if(!position(t, &ici) || !position(avant, &av)) return false;
	*d = AgrDeplacementPlace(ici, av, t < avant && boucle, a.positions[os].back().p, a.positions[os].front().p,
	                         os == ((drapeaux & 0x200) ? 2 : 1));
	return true;
}

inline CVector
AgrTrajetRacine(const AgrAnim &a, uint32 drapeaux)
{
	const std::vector<AgrPosition> &v = a.positions[1];
	if(!a.decodee || (drapeaux & 0x200) || v.empty()) return CVector(0, 0, 0);
	return CVector(v.back().p.x - v.front().p.x, v.back().p.y - v.front().p.y, v.back().p.z - v.front().p.z);
}

// Interpolation sphérique entre deux quaternions (w, x, y, z), par le plus
// court chemin ; u = 0 donne a, u = 1 donne b.
void AgrSlerp(const float a[4], const float b[4], float u, float out[4]);

// Quaternion (w, x, y, z) → matrice 3 × 3, dans la convention des NiAVObject
// (docs/nif.md) : la matrice lue ligne par ligne redonne ce quaternion.
void AgrMatrice(const float q[4], float m[3][3]);
