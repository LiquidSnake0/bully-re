// Les points d'attache des modèles (Models/*/MXDs.MGR), docs/mxd.md.
//
// bully.exe : CGame_InitRW (FUN_0042e820, « LoadMGRFiles ») lit
// models\peds, objects, vehicles, wprops et accssory\MXDs.MGR par
// FUN_006c2cf0 : un u32 (nombre d'entrées), puis par entrée un nom de 64
// octets (« MODELS\PEDS\PLAYER ») lu par FUN_006c2cf0, le reste par
// FUN_006b5710 : la version (f32, 1,06 = constante 0x941fc0), 0x50 octets
// (deux mots, le nom du dictionnaire d'animations sur 64 octets, un mot, le
// nombre de points), les points (FUN_006b5620, 96 octets chacun), puis, au-delà
// d'une version seuil, n os × 12 octets et un dernier mot.
//
// Un point : position (3 f32) et rotation (quaternion w, x, y, z) dans le
// repère de l'os, le nom de l'os sur 32 octets, son numéro (l'ordre des pistes
// .agr et des os de MAINPED.HXD : 15 = Jaw, 22 = Left Hand, 30 = Right Hand),
// le nom du point sur 32 octets (« MouthCig », « RightCig »…). Les pistes
// PropAttachEx des arbres d'actions désignent le point par son HashString
// (docs/cat.md).
#pragma once
#include "../common.h"
#include <string>
#include <vector>

struct CMxdPoint {
	std::string nom;                   // « LeftCig », « MouthCig », « GrappleAnchor »…
	uint32 hachage = 0;                // HashString(nom), ce que citent les pistes PropAttachEx
	std::string os;                    // le nom de l'os dans le squelette du HXD (« Jaw », « Right Hand »)
	int32 numeroOs = -1;               // l'indice de l'os : piste .agr, nœud animé du modèle
	float pos[3] = {0, 0, 0};          // dans le repère de l'os (m)
	float q[4] = {1, 0, 0, 0};         // w, x, y, z : la convention d'AgrMatrice
};

struct CMxdModele {
	std::string nom;                   // « MODELS\PEDS\PLAYER »
	std::string anims;                 // « ANIM\MAINPED »
	float version = 0;
	std::vector<CMxdPoint> points;
	int32 nOs = 0;                     // les n × 12 octets de fin, non interprétés
	const CMxdPoint *Point(uint32 hachage) const {
		for(const CMxdPoint &p : points) if(p.hachage == hachage) return &p;
		return nil;
	}
};

class CMxdFile
{
public:
	std::vector<CMxdModele> modeles;
	uint32 lus = 0;                    // octets consommés

	bool Load(const uint8 *buf, uint32 n);
	// Par le nom court du modèle (« player », « GN_WhiteBoy »), sans casse.
	const CMxdModele *Chercher(const char *modele) const;
	static uint32 Hachage(const char *s);   // HashString (0x576d80), docs/cat.md
};
