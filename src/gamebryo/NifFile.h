// Fichiers NIF de Gamebryo 2.3 (« Gamebryo File Format, Version 20.3.0.9 »),
// les 5 724 modèles `.nif` de Stream/World.img. bully.exe vérifie la ligne
// d'en-tête en 0x75ab30 ; le reste du chargeur est le NiStream de Gamebryo,
// compilé sans RTTI (seul NiAllocator apparaît), donc non nommé.
//
// Disposition relevée sur les données (voir docs/nif.md) :
//   ligne d'en-tête terminée par \n, version u32 (0x14030009), boutisme u8,
//   version utilisateur u32, nombre de blocs u32, nombre de types u16,
//   types (u32 longueur + texte), type de chaque bloc u16, taille de chaque
//   bloc u32, nombre de chaînes u32, longueur max u32, chaînes (u32 + texte),
//   nombre de groupes u32, puis les blocs bout à bout.
// Les tailles de blocs permettent de ne décoder que les types connus et de
// sauter les autres sans se perdre.
#pragma once
#include "../common.h"

enum eNifBlock {
	NIF_INCONNU = 0, NIF_NODE, NIF_TRISHAPE, NIF_TRISTRIPS, NIF_TRISHAPEDATA, NIF_TRISTRIPSDATA,
	NIF_SOURCETEXTURE, NIF_MATERIALPROPERTY, NIF_TEXTURINGPROPERTY
};

struct NifBlock {
	int32 typeIndex;                // index dans la table des types
	eNifBlock kind;
	uint32 offset;                  // depuis le début du fichier
	uint32 size;
	void *data;                     // NifNode*, NifGeometry*, NifGeometryData*… selon kind, ou nil
};

struct NifMatrix33 { float m[3][3]; };

// NiNode et NiTriShape / NiTriStrips partagent NiAVObject
struct NifAVObject {
	int32 name;                     // index de chaîne, -1 si aucun
	int32 numExtra; int32 *extra;
	int32 controller;
	uint16 flags;
	CVector translation;
	NifMatrix33 rotation;
	float scale;
	int32 numProperties; int32 *properties;
	int32 collision;
};

struct NifNode : NifAVObject {
	int32 numChildren; int32 *children;
	int32 numEffects; int32 *effects;
};

struct NifGeometry : NifAVObject {
	int32 data;                     // NiTriShapeData / NiTriStripsData
	int32 skin;
	int32 numMaterials;
	int32 activeMaterial;
	uint8 dirty;
};

struct NifGeometryData {
	int32 groupId;
	uint16 numVertices;
	uint8 keepFlags, compressFlags;
	CVector *vertices;              // nil si absents
	uint16 dataFlags;               // bits 0-5 : nombre de jeux d'UV
	CVector *normals;
	CVector center; float radius;
	float (*colors)[4];
	int32 numUVSets;
	float (*uv)[2];                 // numUVSets × numVertices
	uint16 consistency;
	int32 additionalData;
	// triangles (NiTriShapeData) ou bandes converties en triangles (NiTriStripsData)
	uint16 numTriangles;
	uint16 (*triangles)[3];
	uint16 numStrips;
};

struct NifSourceTexture {
	int32 name;
	uint8 external;
	int32 fileName;                 // index de chaîne
	int32 pixelData;
	uint32 pixelLayout, useMipmaps, alphaFormat;
	uint8 isStatic, directRender, persistRenderData;
};

struct NifMaterialProperty {
	int32 name;
	float ambient[3], diffuse[3], specular[3], emissive[3];
	float glossiness, alpha;
};

struct NifTexturingProperty {
	int32 name;
	uint16 flags;
	uint32 textureCount;
	int32 baseTexture;              // ref NiSourceTexture, -1 si aucune
	uint16 baseFlags, baseUVSet;
};

class CNifFile
{
public:
	uint32 version, userVersion;
	uint8 endian;
	int32 numTypes; char **types;
	int32 numBlocks; NifBlock *blocks;
	int32 numStrings; char **strings;
	int32 numGroups;
	int32 numDecoded;               // blocs d'un type connu décodés sans erreur
	const uint8 *m_data; uint32 m_size;

	bool Load(const uint8 *data, uint32 size);
	void Free(void);
	const char *String(int32 i) const { return i >= 0 && i < numStrings ? strings[i] : ""; }
	const char *TypeName(int32 block) const { return types[blocks[block].typeIndex]; }
	static eNifBlock KindOf(const char *type);
};
