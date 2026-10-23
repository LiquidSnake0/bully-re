#include "ModelEnum.h"
#include "IdeBinary.h"
#include "CdStream.h"
#include "FileMgr.h"
#include <cctype>
#include <cstdlib>
#include <cstring>

static CModelEnum *gEnCours;

static std::string
Majuscules(const char *s)
{
	std::string m = s;
	for(char &c : m) c = (char)toupper((unsigned char)c);
	return m;
}

static void
Noter(const char *nom, int32 id)
{
	if(nom[0] == '\0') return;
	if(!gEnCours->parNom.insert({Majuscules(nom), id}).second) gEnCours->doublons++;
}

static int32 Ped(const CPedIdeEntry &e){ Noter(e.model, e.id); if(e.stat[0]) gEnCours->statDuPieton.insert({e.id, e.stat}); return 0; }
static int32 Obj(const CObjIdeEntry &e){ Noter(e.model, e.id); return 0; }
static int32 Car(const CCarIdeEntry &e){ Noter(e.model, e.id); return 0; }
static int32 Weap(const CWeapIdeEntry &e){ Noter(e.model, e.id); return 0; }
static int32 Panm(const CPanmIdeEntry &e){ Noter(e.model, e.id); return 0; }
static int32 Simple(const CSimpleIdeEntry &e){ Noter(e.model, e.id); return 0; }

// 0x52dc50 : le jeu note les plages d'identifiants ; ici rien à en faire.
void RegisterModelRange(uint16 first, uint32 count){ (void)first; (void)count; }

bool
CModelEnum::Charger(void)
{
	int32 image = CdStream::AddImage("Objects\\ide.img");
	if(image < 0) return false;
	const CdImage &img = CdStream::ms_images[image];
	gEnCours = this;
	CIdeBinary::ms_pedHandler = Ped; CIdeBinary::ms_objHandler = Obj; CIdeBinary::ms_carHandler = Car;
	CIdeBinary::ms_weapHandler = Weap; CIdeBinary::ms_panmHandler = Panm; CIdeBinary::ms_simpleHandler = Simple;
	CIdeBinary::ms_2dfxHandler = nil;
	for(int32 i = 0; i < img.m_numEntries; i++){
		const CDirectoryEntry &d = img.m_entries[i];
		size_t n = strlen(d.name);
		if(n < 4 || strcasecmp(d.name + n - 4, ".idb") != 0) continue;
		uint32 octets = d.size * CDSTREAM_SECTOR_SIZE;
		uint8 *buf = (uint8*)malloc(octets);
		int32 fd = CFileMgr::OpenFile("Objects\\ide.img", "rb", 1);
		CFileMgr::Seek(fd, d.offset * CDSTREAM_SECTOR_SIZE, 0);
		bool ok = CFileMgr::ReadExact(fd, buf, octets);
		CFileMgr::CloseFile(fd);
		uint32 utile;
		if(ok){ memcpy(&utile, buf, 4); if(utile + 4 <= octets) CIdeBinary::Load(buf + 4, utile); }
		free(buf);
	}
	gEnCours = nil;
	return true;
}

int32
CModelEnum::Index(const char *nom) const
{
	auto it = parNom.find(Majuscules(nom));
	return it == parNom.end() ? -1 : it->second;
}
