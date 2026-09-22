// Placements binaires « Ipl$ » de Bully (fichiers .ipb de Stream/World.img,
// 85 fichiers). bully.exe : CStreaming::LoadCdDirectoryIPB 0x52ded0 lit le
// .dir et charge chaque .IPB dans work_buff, CIplStore (0x438f50) le garde
// en mémoire et le fait analyser par IplFileFormat (0x435780 pour le
// fichier, 0x435140 pour une section) avec des rappels IplLoaderImpl
// (vftable 0x90373c).
//
// Format : « Ipl$ » (4), 15 octets non interprétés, nombre de sections
// (u32 à l'octet 19), puis des sections [tag 4][compte u32][entrées] :
//   inst : 120 octets fixes (lecteur 0x433410)
//   spec : 128 octets (0x433470), proj : 108 octets (0x4334e0)
//   occl : 7 dwords (0x433550)
//   prop : nom à longueur préfixée (0x434700)
//   rail : nom préfixé, dword, compte, compte × 3 dwords (0x434f50)
//   perm : nom préfixé, dword, deux flottants, compte, compte dwords (0x4336f0)
//   pois : nom préfixé, dword, compte, puis compte × {quatre chaînes
//          préfixées, 21 dwords} (0x434df0 / 0x4347b0)
//   pont : nom préfixé, dword, 3 dwords, 3 dwords, dword, trois chaînes
//          préfixées, 6 dwords, deux chaînes préfixées (0x433d80)
//   trig : nom préfixé puis 19 dwords (0x4338d0)
//   pthx : nom préfixé, dword, dword, compte, compte × 9 dwords (0x434570)
// Sections rencontrées dans les 85 fichiers : inst 104, rail 42, pont 30,
// pois 29, prop 28, spec 25, perm 4, trig et pthx (zones).
#pragma once
#include "../common.h"

enum eIplSection {
	IPL_INST = 0x74736e69, IPL_OCCL = 0x6c63636f, IPL_RAIL = 0x6c696172, IPL_PERM = 0x6d726570,
	IPL_PROP = 0x706f7270, IPL_POIS = 0x73696f70, IPL_PONT = 0x746e6f70, IPL_SPEC = 0x63657073,
	IPL_TRIG = 0x67697274, IPL_PROJ = 0x6a6f7270, IPL_PTHX = 0x78687470
};

// Une entrée « inst » : modèle, nom, position, échelle, rotation (quaternion).
struct CIplInst {
	int32 modelId;                  // +0
	char name[64];                  // +4
	float unk68;                    // +68 (22.0 dans ftest.ipb)
	CVector pos;                    // +72
	CVector scale;                  // +84
	float rot[4];                   // +96, quaternion x y z w
	int32 unk112;
	uint32 unk116;
};

struct CIplRail {
	char name[64];
	int32 unk;
	int32 numPoints;
	int32 (*points)[3];             // trois dwords par point, non interprétés
};

struct CIplOccl { float a[3]; float b[4]; };

// Une entrée « pont » (zones) : un point d'intérêt nommé avec des chaînes
// de rappel ; les champs numériques ne sont pas encore interprétés.
struct CIplPont {
	char name[64];
	int32 d0;
	int32 a[3];
	int32 b[3];
	int32 d1;
	char s0[68], s1[68], s2[104];
	int32 c[6];
	char s3[68], s4[16];
};

// Un point d'intérêt (« pois ») : un groupe nommé (« Smokers »…) de
// points, chacun avec quatre chaînes (« Both », « », « Wall », « PREPPY »…)
// et 21 dwords non encore interprétés.
struct CIplPoiPoint {
	char s[4][64];
	int32 d[21];
};
struct CIplPois {
	char name[64];
	int32 unk;
	int32 numPoints;
};

class CIplFile
{
public:
	static int32 (*ms_instHandler)(const CIplInst &e);
	static int32 (*ms_railHandler)(const CIplRail &e);
	static int32 (*ms_occlHandler)(const CIplOccl &e);
	static int32 (*ms_propHandler)(const char *name);
	static int32 (*ms_pontHandler)(const CIplPont &e);
	static int32 (*ms_poisHandler)(const CIplPois &g, const CIplPoiPoint &p);
	static int32 ms_numInst, ms_numRail, ms_numSpec, ms_numProj, ms_numOccl, ms_numProp, ms_numPerm, ms_numPont, ms_numPois, ms_numTrig, ms_numPthx;
	static int32 ms_lastTag;        // tag de la dernière section rencontrée (diagnostic)

	// Analyse un fichier entier ; retourne faux sur une section non recréée.
	static bool Load(const uint8 *data, uint32 size);
};
