// Modèles de collision, hérités de GTA (reVC : collision/). Bully lit des
// fichiers « COL3 » (aussi COLL et COL2) emballés dans Stream/World.img :
// 488 fichiers, 3 851 modèles. bully.exe : CFileLoader::LoadCollisionFile
// 0x42c790 (première fois) et 0x42c570 (rechargement), LoadCollisionModel
// 0x42bb60, lecture des bornes 0x42a970, CColSphere::Set 0x56ea50,
// CColBox::Set 0x564750.
//
// Disposition d'un modèle dans le fichier (COL3) :
//   fourcc(4) taille(4) version u16 (4 partout) drapeaux u16 (1, 2 ou 3)
//   nom[20] id i32 : 36 octets ; si le nom est vide, l'id désigne le modèle
//   bornes : centre(12) rayon(4) min(12) bourrage(4) max(12) bourrage(4)
//   n sphères (i32) puis n × {centre(12) rayon(4) surface u8 pièce u8 pad(2)}
//   n lignes (i32) puis n × 32 octets, ignorées
//   n boîtes (i32) puis n × {min(12) pad max(12) pad surface pièce pad(2)}
//   n sommets (i32) puis n × {x y z int16, ÷128}, arrondi à 4 octets
//   (COLL : n × {x y z flottants, un dword nul}, convertis par _ftol)
//   n triangles (i32) puis n × {a b c u16, surface u8, u8} (12 octets si
//   version et drapeaux valent 3 ; COLL : 4 dwords, 5 si le premier
//   triangle porte le marqueur 0xffff à +0x12 ; COL2 : 5 dwords)
//   puis, facultatifs : un bloc 0xef5dcb33 (arbre de partition, 40 octets
//   + 12 par nœud, le nombre de nœuds au 10e dword) et un bloc « LIMK »
//   (i32 compte, puis 3 dwords par triangle).
// CColModel fait 0x40 octets : sphère (0x10), boîte (0x20), slot de
// collision i16 (+0x30), compte LIMK i16 (+0x32), a de la géométrie (+0x34),
// pointeur vers les données (+0x38). Les comptes et tableaux vivent dans
// l'objet pointé (créé par 0x56d980) dont la disposition exacte n'est pas
// retrouvée ; CCollisionData ci-dessous est la nôtre.
#pragma once
#include "../common.h"

struct CSphere {
	CVector center;
	float radius;
};

struct CBox {
	CVector min; float pad0;
	CVector max; float pad1;
};

struct CColSphere : public CSphere {
	uint8 surface, piece;
	uint8 pad[2];
	void Set(float radius, const CVector &center, uint8 surf, uint8 piece);   // 0x56ea50
};

struct CColBox : public CBox {
	uint8 surface, piece;
	uint8 pad[2];
	void Set(const CVector &min, const CVector &max, uint8 surf, uint8 piece); // 0x564750
};

struct CompressedVector {
	int16 x, y, z;
	CVector Get(void) const { return CVector(x/128.0f, y/128.0f, z/128.0f); }
};

struct CColTriangle {
	uint16 a, b, c;
	uint8 surface;
	uint8 flag;                     // mis à 1 par le chargeur (+7)
};

struct CCollisionData {
	int32 numSpheres, numBoxes, numVertices, numTriangles;
	CColSphere *spheres;
	CColBox *boxes;
	CompressedVector *vertices;
	CColTriangle *triangles;
	int32 numKdNodes;               // bloc 0xef5dcb33
	uint8 *kdTree;                  // 40 octets d'en-tête + 12 par nœud, brut
	int32 (*triLinks)[3];           // bloc « LIMK », 3 dwords par triangle
};

struct CColModel {
	CSphere boundingSphere;         // +0x00
	CBox boundingBox;               // +0x10
	int16 colSlot;                  // +0x30
	int16 numLinks;                 // +0x32 (compte du bloc LIMK)
	bool hasGeometry;               // +0x34
	CCollisionData *pColData;       // +0x38
	int32 pad3c;

	void Init(void);
	void RemoveCollisionVolumes(void);
};

enum { COL_IDENT_COLL = 0x4c4c4f43, COL_IDENT_COL2 = 0x324c4f43, COL_IDENT_COL3 = 0x334c4f43, COL_IDENT_PATH = 0x48544150,
       COL_KDTREE_MAGIC = 0xef5dcb33, COL_LIMK = 0x4b4d494c };

struct ColHeader {
	uint32 ident;
	uint32 size;                    // octets qui suivent
	uint16 version;                 // COL2/COL3 seulement
	uint16 flags;
	char name[20];
	int32 modelId;
};

class CColLoader
{
public:
	// appelé pour chaque modèle du fichier : id (ou -1), nom (ou vide), modèle rempli
	static void (*ms_handler)(int32 id, const char *name, CColModel *model, uint8 colSlot);

	// 0x42bb60 : remplit model depuis les données après l'en-tête ; retourne
	// faux si la taille consommée ne colle pas
	static bool LoadCollisionModel(const uint8 *buf, uint32 size, CColModel &model, uint16 version, uint16 flags);
	// 0x42c790 : enchaîne les modèles d'un fichier (blocs PATH sautés)
	static bool LoadCollisionFile(const uint8 *buf, uint32 size, uint8 colSlot);
};
