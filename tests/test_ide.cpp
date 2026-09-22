// Extrait default.idb de Objects/ide.img avec CdStream, le parcourt avec
// CIdeBinary, et vérifie les sections « peds » et « objs » contre
// Objects/default.ide, puis la section « objs » d'un fichier monde.
//   BULLY_DATA=<racine du jeu> build/tests/test_ide
#include "../src/core/CdStream.h"
#include "../src/core/IdeBinary.h"
#include "../src/core/FileMgr.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)
static CPedIdeEntry g_peds[400];
static int32 g_n;
static int32 Garder(const CPedIdeEntry &e) { if(g_n < 400) g_peds[g_n++] = e; return 0; }
static CObjIdeEntry g_objs[400];
static int32 g_no;
static int32 GarderObj(const CObjIdeEntry &e) { if(g_no < 400) g_objs[g_no++] = e; return 0; }

// lit un .idb entier de ide.img dans un tampon fraîchement alloué
static uint8 *
Lire(const char *nom, uint32 *utile)
{
	const CDirectoryEntry *d = CdStream::ms_images[0].Find(nom);
	if(d == nil) return nil;
	uint32 bytes = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(bytes);
	int32 fd = CFileMgr::OpenFile("Objects\\ide.img", "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, bytes);
	CFileMgr::CloseFile(fd);
	if(!ok) return nil;
	memcpy(utile, buf, 4);
	return buf;
}
void RegisterModelRange(uint16 first, uint32 count) { printf("plage de modèles %u + %u\n", first, count); }

int
main(void)
{
	VERIF(CdStream::AddImage("Objects\\ide.img") == 0);
	uint32 utile;
	uint8 *buf = Lire("default.idb", &utile);
	VERIF(buf != nil);
	if(buf == nil) return 1;
	printf("default.idb : %u octets utiles\n", utile);
	VERIF(utile == 71176);

	CIdeBinary::ms_pedHandler = Garder;
	CIdeBinary::ms_objHandler = GarderObj;
	bool complet = CIdeBinary::Load(buf + 4, utile - 4);
	printf("peds lus : %d ; toutes les sections connues : %s\n", CIdeBinary::ms_numPeds, complet ? "oui" : "non");
	VERIF(CIdeBinary::ms_numPeds == 259);
	VERIF(g_n >= 3);
	CPedIdeEntry &p = g_peds[0];
	VERIF(p.id == 0 && strcmp(p.model, "player") == 0 && strcmp(p.txd, "player") == 0 && p.female == 0);
	VERIF(strcmp(p.size, "Medium") == 0 && strcmp(p.type, "PLAYER1") == 0 && strcmp(p.stat, "STAT_PLAYER") == 0);
	VERIF(strcmp(p.animGroup[0], "Grap") == 0 && strcmp(p.animGroup[3], "null") == 0 && p.unique == -1);
	VERIF(strcmp(p.actionRoot, "/Global/Player") == 0 && strcmp(p.actionFile, "Act/Player.act") == 0);
	VERIF(strcmp(p.aiRoot, "/Global/PlayerAI") == 0 && strcmp(p.aiFile, "Act/PlayerAI.act") == 0);
	VERIF(strcmp(p.name, "N_Jimmy") == 0);
	CPedIdeEntry &z = g_peds[2];
	VERIF(z.id == 2 && strcmp(z.model, "DOgirl_Zoe_EG") == 0 && z.female == 1 && strcmp(z.stat, "STAT_GS_FEMALE_A") == 0);
	VERIF(strcmp(z.animGroup[0], "F_Girls") == 0 && strcmp(z.animGroup[3], "Straf_Female") == 0 && strcmp(z.name, "N_Zoe") == 0);

	// Section objs de default.idb, contre la ligne
	// « 571 Foreign_wheel Foreign_wheel 1 100 0  0 1 0 255 0 0 0 » de default.ide.
	// L'ordre du fichier est peds, cars, weap, cash, item, scnd, objs, peds,
	// clth : tant que cars..scnd ne sont pas recréées, on entre directement
	// sur le tag objs (octet 65368) et on s'arrête au second bloc peds (65856).
	VERIF(memcmp(buf + 65368, "sjbo", 4) == 0);
	VERIF(CIdeBinary::Load(buf + 65368, 65856 - 65368));
	printf("objs lus dans default.idb : %d\n", CIdeBinary::ms_numObjs);
	VERIF(CIdeBinary::ms_numObjs == 6 && g_no == 6);
	CObjIdeEntry &o = g_objs[0];
	VERIF(o.type == 0 && o.id == 571 && strcmp(o.model, "Foreign_wheel") == 0 && strcmp(o.txd, "Foreign_wheel") == 0);
	VERIF(o.numObjs == 1 && o.drawDist[0] == 100.0f && o.flags == 0);
	VERIF(o.unk1 == 0.0f && o.unk2 == 1.0f && o.unk3 == 0 && o.byte0b == 255 && o.byte2d == 0 && o.byte2e == 0 && o.byte2f == 0);
	VERIF(g_objs[5].id == 576 && strcmp(g_objs[5].model, "wheel_van") == 0);

	// un fichier monde : ifunhous.idb commence par objs, première entrée
	// 10621 fun_libwalls, distance 30
	g_no = 0; CIdeBinary::ms_numObjs = 0;
	uint8 *buf2 = Lire("ifunhous.idb", &utile);
	VERIF(buf2 != nil);
	if(buf2){
		CIdeBinary::Load(buf2 + 4, utile - 4);
		printf("objs lus dans ifunhous.idb : %d\n", CIdeBinary::ms_numObjs);
		VERIF(g_no > 0 && g_objs[0].id == 10621 && strcmp(g_objs[0].model, "fun_libwalls") == 0 && g_objs[0].drawDist[0] == 30.0f);
	}

	VERIF(CIdeBinary::ConvertFlags(0x84) == (SMI_DRAW_LAST | SMI_FLAG_IDE_80));
	VERIF(CIdeBinary::ConvertFlags(0x8) == (SMI_DRAW_LAST | SMI_ADDITIVE));
	VERIF(CIdeBinary::ConvertFlags(0x22000) == (SMI_FLAG_IDE_2000 | SMI_FLAG_IDE_20000));
	VERIF(CIdeBinary::IsSpecialObjectId(0x281) && !CIdeBinary::IsSpecialObjectId(0x280) && CIdeBinary::IsSpecialObjectId(0x2edf) && !CIdeBinary::IsSpecialObjectId(0x2ee0));
	VERIF(CIdeBinary::IsNogOrWalkable("NOG_truc") && CIdeBinary::IsNogOrWalkable("walkable_x") && !CIdeBinary::IsNogOrWalkable("nogx"));
	printf(echecs ? "%d échec(s)\n" : "tout passe\n", echecs);
	return echecs != 0;
}
