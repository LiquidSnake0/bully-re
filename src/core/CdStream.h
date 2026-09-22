// Archives .img / .dir, héritées de GTA (re3 : core/CdStream, Streaming).
// Bully : `Stream/World.img` (1,9 Go) et `Stream/World.dir` (11 980 entrées),
// ajoutés par `CdStreamAddImage` (0x73a740) au démarrage. Une entrée de
// répertoire fait 32 octets : offset et taille en secteurs de 2048 octets,
// puis le nom sur 24 octets.
#pragma once
#include "../common.h"

enum { CDSTREAM_SECTOR_SIZE = 2048, CDSTREAM_NAME_LEN = 24, MAX_CDIMAGES = 8 };

struct CDirectoryEntry {
	uint32 offset;                  // en secteurs
	uint32 size;                    // en secteurs
	char name[CDSTREAM_NAME_LEN];
};

class CdImage
{
public:
	char m_path[128];
	CDirectoryEntry *m_entries;
	int32 m_numEntries;

	bool LoadDirectory(const char *dirPath);          // lit <nom>.dir
	const CDirectoryEntry *Find(const char *name) const;
};

class CdStream
{
public:
	static CdImage ms_images[MAX_CDIMAGES];
	static int32 ms_numImages;
	static int32 AddImage(const char *imgPath);       // 0x73a740 : "Stream/World.img" → lit aussi "Stream/World.dir"
};
