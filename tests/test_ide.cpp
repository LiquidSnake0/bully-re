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
static CWeapIdeEntry g_weaps[200];
static int32 g_nw;
static int32 GarderWeap(const CWeapIdeEntry &e) { if(g_nw < 200) g_weaps[g_nw++] = e; return 0; }
static CSimpleIdeEntry g_simples[100];
static int32 g_ns;
static int32 GarderSimple(const CSimpleIdeEntry &e) { if(g_ns < 100) g_simples[g_ns++] = e; return 0; }
static CCarIdeEntry g_cars[64];
static int32 g_nc;
static int32 GarderCar(const CCarIdeEntry &e) { if(g_nc < 64) g_cars[g_nc++] = e; return 0; }

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
	CIdeBinary::ms_carHandler = GarderCar;
	CIdeBinary::ms_weapHandler = GarderWeap;
	CIdeBinary::ms_simpleHandler = GarderSimple;
	// le dword de tête compte les octets qui le suivent
	bool complet = CIdeBinary::Load(buf + 4, utile);
	printf("peds lus : %d ; toutes les sections connues : %s\n", CIdeBinary::ms_numPeds, complet ? "oui" : "non");
	VERIF(complet);
	VERIF(CIdeBinary::ms_numPeds == 259 + 30);
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

	// section cars, contre « 272 bmxrace bmxrace bike BMXRACE BIKE V_Bike bike
	// BIKE 0 7 0 16 0.63 » et « 284 Mower Mower car WALTON WALTON MOWER null
	// CAR 0 7 0 576 0.5 »
	printf("cars lus : %d ; vélos %d..%d ; véhicules %d..%d\n", CIdeBinary::ms_numCars,
		CIdeBinary::ms_firstBikeId, CIdeBinary::ms_lastBikeId, CIdeBinary::ms_firstVehicleId, CIdeBinary::ms_lastVehicleId);
	VERIF(CIdeBinary::ms_numCars == 27 && g_nc == 27);
	CCarIdeEntry &c = g_cars[0];
	VERIF(c.id == 272 && strcmp(c.model, "bmxrace") == 0 && strcmp(c.type, "bike") == 0 && strcmp(c.handlingId, "BMXRACE") == 0);
	VERIF(strcmp(c.gameName, "BIKE") == 0 && strcmp(c.animGroup, "V_Bike") == 0 && strcmp(c.animGroup2, "bike") == 0 && strcmp(c.vehClass, "BIKE") == 0);
	VERIF(c.frequency == 0 && c.level == 7 && c.compRules == 0 && c.wheelModelId == 16 && c.wheelScale == 0.63f);
	CCarIdeEntry &m = g_cars[12];
	VERIF(m.id == 284 && strcmp(m.model, "Mower") == 0 && strcmp(m.type, "car") == 0 && strcmp(m.gameName, "WALTON") == 0);
	VERIF(strcmp(m.animGroup, "MOWER") == 0 && strcmp(m.animGroup2, "null") == 0 && m.wheelModelId == 576 && m.wheelScale == 0.5f);
	VERIF(strcmp(g_cars[13].gameName, "ARCADE3") == 0 && strcmp(g_cars[13].animGroup, "Go_Cart") == 0);
	VERIF(CIdeBinary::ms_firstBikeId == 272 && CIdeBinary::ms_lastBikeId == 283 && CIdeBinary::ms_firstVehicleId == 272 && CIdeBinary::ms_lastVehicleId == 298);

	// section weap, contre « 299 yardstick yardstick W_Stick null 1 50 0 0 »
	printf("weap lus : %d ; armes %d..%d\n", CIdeBinary::ms_numWeaps, CIdeBinary::ms_firstWeaponId, CIdeBinary::ms_lastWeaponId);
	VERIF(CIdeBinary::ms_numWeaps == 146 && g_nw == 146 && CIdeBinary::ms_firstWeaponId == 299);
	CWeapIdeEntry &w = g_weaps[0];
	VERIF(w.id == 299 && strcmp(w.model, "yardstick") == 0 && strcmp(w.animGroup, "W_Stick") == 0 && strcmp(w.animGroup2, "null") == 0);
	VERIF(w.unk == 1 && w.drawDist == 50.0f && w.byte54 == 0 && w.byte55 == 0);
	VERIF(g_weaps[4].id == 303 && strcmp(g_weaps[4].animGroup2, "SlingSh") == 0 && g_weaps[4].drawDist == 30.0f);

	// cash (2), item (67), scnd (6), clth (1)
	printf("cash+scnd %d, item %d (%d..%d), clth %d (%d..%d)\n", CIdeBinary::ms_numCashScnd, CIdeBinary::ms_numItems,
		CIdeBinary::ms_firstItemId, CIdeBinary::ms_lastItemId, CIdeBinary::ms_numClth, CIdeBinary::ms_firstClothId, CIdeBinary::ms_lastClothId);
	VERIF(CIdeBinary::ms_numCashScnd == 8 && CIdeBinary::ms_numItems == 67 && CIdeBinary::ms_numClth == 1 && g_ns == 76);
	VERIF(g_simples[0].section == IDE_CASH && g_simples[0].id == 462 && strcmp(g_simples[0].model, "coin_dollar") == 0);
	VERIF(g_simples[2].section == IDE_ITEM && g_simples[2].id == 464 && strcmp(g_simples[2].model, "ClwnPant") == 0);
	VERIF(g_simples[69].section == IDE_SCND && g_simples[69].id == 565 && strcmp(g_simples[69].model, "ammo_stink") == 0);
	VERIF(g_simples[75].section == IDE_CLTH && g_simples[75].id == 10905 && strcmp(g_simples[75].model, "B_Buzz") == 0);
	VERIF(CIdeBinary::ms_firstItemId == 464 && CIdeBinary::ms_firstClothId == 10905 && CIdeBinary::ms_lastClothId == 10905);

	// section objs de default.idb, contre la ligne
	// « 571 Foreign_wheel Foreign_wheel 1 100 0  0 1 0 255 0 0 0 » de default.ide
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
		CIdeBinary::Load(buf2 + 4, utile);
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
