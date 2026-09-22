#include "IdeBinary.h"
#include <cstring>
#include <strings.h>

int32 (*CIdeBinary::ms_pedHandler)(const CPedIdeEntry &e) = nil;
int32 (*CIdeBinary::ms_objHandler)(const CObjIdeEntry &e) = nil;
int32 CIdeBinary::ms_numPeds;
int32 (*CIdeBinary::ms_carHandler)(const CCarIdeEntry &e) = nil;
int32 CIdeBinary::ms_numObjs;
int32 (*CIdeBinary::ms_weapHandler)(const CWeapIdeEntry &e) = nil;
static void LireSimple(CIdeReader &r, CSimpleIdeEntry &e, int32 tag);

int32 (*CIdeBinary::ms_panmHandler)(const CPanmIdeEntry &e) = nil;
int32 (*CIdeBinary::ms_2dfxHandler)(const C2dEffectIdeEntry &e) = nil;
int32 CIdeBinary::ms_numTobjs, CIdeBinary::ms_numAccs, CIdeBinary::ms_numPanms, CIdeBinary::ms_num2dfx;
int32 CIdeBinary::ms_firstAccsId = -1, CIdeBinary::ms_lastAccsId = -1;
int32 CIdeBinary::ms_firstPanmId = -1, CIdeBinary::ms_lastPanmId = -1;
int32 (*CIdeBinary::ms_simpleHandler)(const CSimpleIdeEntry &e) = nil;
int32 CIdeBinary::ms_numCars, CIdeBinary::ms_numWeaps, CIdeBinary::ms_numItems, CIdeBinary::ms_numCashScnd, CIdeBinary::ms_numClth;
int32 CIdeBinary::ms_firstWeaponId = -1, CIdeBinary::ms_lastWeaponId = -1;
int32 CIdeBinary::ms_firstItemId = -1, CIdeBinary::ms_lastItemId = -1;
int32 CIdeBinary::ms_firstClothId = -1, CIdeBinary::ms_lastClothId = -1;
int32 CIdeBinary::ms_firstBikeId = -1, CIdeBinary::ms_lastBikeId = -1;
int32 CIdeBinary::ms_firstVehicleId = -1, CIdeBinary::ms_lastVehicleId = -1;

// 0x0042bfd0 : count, puis par entrée id, deux chaînes, un entier, quatre
// chaînes, quatre groupes d'animation, un entier, cinq chaînes. Dans le
// binaire suit la création du modelinfo (0x51c810 ou 0x51b210 si le slot
// existe déjà) et le repérage des slots « spfirst » / « splast »
// (0xa136d0 / 0xa136d4), non recréés ici.
void
CIdeBinary::LoadPeds(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CPedIdeEntry e;
		e.id = r.Int();
		r.String(e.model, sizeof(e.model));
		r.String(e.txd, sizeof(e.txd));
		e.female = r.Int();
		r.String(e.size, sizeof(e.size));
		r.String(e.type, sizeof(e.type));
		r.String(e.stat, sizeof(e.stat));
		for(int k = 0; k < 4; k++) r.String(e.animGroup[k], sizeof(e.animGroup[k]));
		e.unique = r.Int();
		r.String(e.actionRoot, sizeof(e.actionRoot));
		r.String(e.actionFile, sizeof(e.actionFile));
		r.String(e.aiRoot, sizeof(e.aiRoot));
		r.String(e.aiFile, sizeof(e.aiFile));
		r.String(e.name, sizeof(e.name));
		ms_numPeds++;
		if(ms_pedHandler) ms_pedHandler(e);
	}
}

// 0x00429d30 : chaque bit IDE est reporté sur un bit du modelinfo ; le bit
// 0x100 (spécial) est toujours remis à zéro ici, 0x429e70 le repose ensuite.
uint32
CIdeBinary::ConvertFlags(uint32 f)
{
	uint32 m = 0;
	if(f & 0x1) m |= SMI_WET_ROAD_REFLECTION;
	if(f & 0x2) m |= SMI_NO_FADE;
	if(f & 0xc) m |= SMI_DRAW_LAST;
	if(f & 0x8) m |= SMI_ADDITIVE;
	if(f & 0x40) m |= SMI_FLAG_IDE_40;
	if(f & 0x80) m |= SMI_FLAG_IDE_80;
	if(f & 0x100) m |= SMI_FLAG_IDE_100;
	if(f & 0x200) m |= SMI_FLAG_IDE_200;
	if(f & 0x400) m |= SMI_FLAG_IDE_400;
	if(f & 0x10) m |= SMI_FLAG_IDE_10;
	if(f & 0x1000) m |= SMI_FLAG_IDE_1000;
	if(f & 0x2000) m |= SMI_FLAG_IDE_2000;
	if(f & 0x4000) m |= SMI_FLAG_IDE_4000;
	if(f & 0x10000) m |= SMI_FLAG_IDE_10000;
	if(f & 0x20000) m |= SMI_FLAG_IDE_20000;
	return m;
}

// 0x00429e70 : sept plages d'ids codées en dur
bool
CIdeBinary::IsSpecialObjectId(int32 id)
{
	static const int32 ranges[][2] = {
		{ 0x281, 0x6d0 }, { 0x6f0, 0x717 }, { 0xded, 0x23b3 }, { 0x23f1, 0x2580 },
		{ 0x2838, 0x28d1 }, { 0x297d, 0x2a58 }, { 0x2a95, 0x2a95 + 0x44a }
	};
	for(size_t i = 0; i < sizeof(ranges)/sizeof(ranges[0]); i++)
		if(id >= ranges[i][0] && id <= ranges[i][1]) return true;
	return false;
}

bool
CIdeBinary::IsNogOrWalkable(const char *n)
{
	return strncasecmp(n, "nog_", 4) == 0 || strncasecmp(n, "walkable_", 9) == 0;
}

// 0x0042aa20 : count, puis par entrée type, id, modèle, txd, nombre
// d'objets, distances, flags, trois dwords ignorés, un dword → +0xb et,
// pour les types pairs, trois dwords → +0x2d/+0x2e/+0x2f. Le binaire crée
// ensuite un CSimpleModelInfo (0x51c5f0, 0x34 octets, vftable 0x917b00),
// pose la distance (0x5272f0 → +0x24), cherche le txd (0x50e7b0 → +0x16),
// convertit les flags, pose 0x1000000 si le nom commence par nog_ ou
// walkable_, 0x100 si l'id est dans une plage spéciale, met à zéro
// +0x30..+0x32, efface le bit de l'id dans le bitset 0xc9dd58 (0x5273c0)
// et enregistre l'id dans la table des noms spéciaux (0x43f220,
// « _start_ »… à 0xa136e8) si le nom y figure.
static void
LireObj(CIdeReader &r, CObjIdeEntry &e, int32 section)
{
	memset(&e, 0, sizeof(e));
	e.section = section;
	e.type = r.Int();
	if(e.type < 0 || e.type > 5) return;              // le binaire saute au default et laisse tout tel quel
	e.id = r.Int();
	r.String(e.model, sizeof(e.model));
	r.String(e.txd, sizeof(e.txd));
	e.numObjs = r.Int();                              // non lu par le jeu, implicite dans le type
	int32 n = e.type / 2 + 1;
	for(int32 k = 0; k < n; k++) e.drawDist[k] = r.Float();
	e.flags = (uint32)r.Int();
	e.unk1 = r.Float();
	e.unk2 = r.Float();
	e.unk3 = r.Int();
	e.byte0b = r.Int();
	if((e.type & 1) == 0){
		e.byte2d = r.Int();
		e.byte2e = r.Int();
		e.byte2f = r.Int();
	}
	if(section == IDE_TOBJ){
		e.timeOn = r.Int();
		e.timeOff = r.Int();
	}
}

void
CIdeBinary::LoadObjs(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CObjIdeEntry e;
		LireObj(r, e, IDE_OBJS);
		ms_numObjs++;
		if(ms_objHandler) ms_objHandler(e);
	}
}

// 0x0042af80 : même disposition que objs, plus les heures d'allumage et
// d'extinction en queue ; CTimeModelInfo (0x51c650) avec +0x34 / +0x38, et
// enregistrement de l'id dans l'objet retourné par 0x824d30 (+0x3c).
void
CIdeBinary::LoadTobj(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CObjIdeEntry e;
		LireObj(r, e, IDE_TOBJ);
		ms_numTobjs++;
		if(ms_objHandler) ms_objHandler(e);
	}
}

// 0x0042a080 : accessoires (access.ide) ; modelinfo 0x51c710, txd, table
// des noms spéciaux, bornes des ids.
void
CIdeBinary::LoadAccs(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CSimpleIdeEntry e;
		LireSimple(r, e, IDE_ACCS);
		if(ms_firstAccsId == -1) ms_firstAccsId = e.id;
		ms_lastAccsId = e.id;
		ms_numAccs++;
		if(ms_simpleHandler) ms_simpleHandler(e);
	}
}

// 0x0042a4d0 : objets animés (props.ide) ; modelinfo 0x51c880, AGR passé
// à 0x51fef0 puis au slot 6, AGR de piéton à 0x520030, test alpha → +0x8f,
// collision secondaire → 0x520350, verrouillage → +0x90, bornes des ids.
void
CIdeBinary::LoadPanm(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CPanmIdeEntry e;
		memset(&e, 0, sizeof(e));
		e.id = r.Int();
		r.String(e.model, sizeof(e.model));
		r.String(e.txd, sizeof(e.txd));
		r.String(e.agr, sizeof(e.agr));
		r.String(e.pedAgr, sizeof(e.pedAgr));
		e.alphaTest = r.Int();
		e.secondaryCollision = r.Int();
		e.manualTargetLock = r.Int();
		if(ms_firstPanmId == -1) ms_firstPanmId = e.id;
		ms_lastPanmId = e.id;
		ms_numPanms++;
		if(ms_panmHandler) ms_panmHandler(e);
	}
}

// 0x0042b610 : voir C2dEffectIdeEntry ; les textures sont cherchées dans
// le txd « particle » (0x5f2170 / 0x5f1610).
void
CIdeBinary::Load2dfx(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		C2dEffectIdeEntry e;
		memset(&e, 0, sizeof(e));
		e.id = r.Int();
		for(int k = 0; k < 3; k++) e.pos[k] = r.Float();
		for(int k = 0; k < 4; k++) e.col[k] = r.Int();
		e.type = r.Int();
		r.String(e.corona, sizeof(e.corona));
		r.String(e.shadow, sizeof(e.shadow));
		e.dist = r.Float();
		e.range = r.Float();
		e.size = r.Float();
		e.shadowSize = r.Float();
		e.byte3b = r.Int();
		e.byte39 = r.Int();
		e.val2c = r.Int();
		e.val30 = r.Float();
		e.val34 = r.Int();
		e.unk = r.Int();
		e.byte3a = r.Int();
		e.flags = r.Int();
		e.bool38 = r.Int();
		ms_num2dfx++;
		if(ms_2dfxHandler) ms_2dfxHandler(e);
	}
}

// 0x0042a160 : count, puis par entrée id, huit chaînes, quatre entiers,
// un float. Le binaire prend un CVehicleModelInfo dans la réserve statique
// de 32 × 0x1e0 octets (0x51c770, 0xc771e4), résout le txd (0x50e7b0),
// appelle le slot 6 avec le premier groupe d'animation, résout le second
// (0x5349b0 → +0x1d8, sauf « null »), remplace les « _ » du nom de jeu par
// des espaces, range compRules (+0xdc) ; « car » → type 0, roue (+0x5c) et
// échelle (+0x58) ; « bike » → type 1, échelle (+0x58), roue convertie en
// float (+0xe0) et bornes des ids de vélos. Puis handling (0x4c9a90 → +0x5e),
// classe (0x4ce290 → +0x54) et fréquence (+0x62), bornes des ids de
// véhicules, et 0x425220.
void
CIdeBinary::LoadCars(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CCarIdeEntry e;
		memset(&e, 0, sizeof(e));
		e.id = r.Int();
		r.String(e.model, sizeof(e.model));
		r.String(e.txd, sizeof(e.txd));
		r.String(e.type, sizeof(e.type));
		r.String(e.handlingId, sizeof(e.handlingId));
		r.String(e.gameName, sizeof(e.gameName));
		r.String(e.animGroup, sizeof(e.animGroup));
		r.String(e.animGroup2, sizeof(e.animGroup2));
		r.String(e.vehClass, sizeof(e.vehClass));
		e.frequency = r.Int();
		e.level = r.Int();
		e.compRules = r.Int();
		e.wheelModelId = r.Int();
		e.wheelScale = r.Float();
		for(char *p = e.gameName; *p; p++) if(*p == '_') *p = ' ';
		if(strcmp(e.type, "bike") == 0){
			ms_lastBikeId = e.id;
			if(ms_firstBikeId == -1) ms_firstBikeId = e.id;
		}
		if(ms_firstVehicleId == -1) ms_firstVehicleId = e.id;
		ms_lastVehicleId = e.id;
		ms_numCars++;
		if(ms_carHandler) ms_carHandler(e);
	}
}

// 0x00429ee0 : id, quatre chaînes, quatre dwords. Le binaire prend un
// CWeaponModelInfo dans la réserve de 150 × 0x58 octets (0x51c6b0,
// 0xc735e4), résout le txd, passe le premier groupe d'animation au slot 6,
// le second à 0x535e30 (→ +0x4c sauf « null »), range la distance (+0x50)
// et deux octets (+0x54, +0x55), et tient les bornes des ids d'armes.
void
CIdeBinary::LoadWeap(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CWeapIdeEntry e;
		memset(&e, 0, sizeof(e));
		e.id = r.Int();
		r.String(e.model, sizeof(e.model));
		r.String(e.txd, sizeof(e.txd));
		r.String(e.animGroup, sizeof(e.animGroup));
		r.String(e.animGroup2, sizeof(e.animGroup2));
		e.unk = r.Int();
		e.drawDist = r.Float();
		e.byte54 = r.Int();
		e.byte55 = r.Int();
		if(ms_firstWeaponId == -1) ms_firstWeaponId = e.id;
		ms_lastWeaponId = e.id;
		ms_numWeaps++;
		if(ms_weapHandler) ms_weapHandler(e);
	}
}

// id, modèle, txd, communs à item, cash, scnd et clth
static void
LireSimple(CIdeReader &r, CSimpleIdeEntry &e, int32 tag)
{
	memset(&e, 0, sizeof(e));
	e.section = tag;
	e.id = r.Int();
	r.String(e.model, sizeof(e.model));
	r.String(e.txd, sizeof(e.txd));
}

// 0x0042b400 : CSimpleModelInfo avec la distance constante 0x900e68 (30.0),
// +0xb = 255, octets +0x2d..+0x32 à zéro, bit 0x400000 des flags, entrée
// dans la table des noms spéciaux, bornes des ids d'objets ramassables.
void
CIdeBinary::LoadItem(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CSimpleIdeEntry e;
		LireSimple(r, e, IDE_ITEM);
		if(ms_firstItemId == -1) ms_firstItemId = e.id;
		ms_lastItemId = e.id;
		ms_numItems++;
		if(ms_simpleHandler) ms_simpleHandler(e);
	}
}

// 0x0042b510 : comme item, sans les bornes ; sert à cash et à scnd
void
CIdeBinary::LoadCashScnd(CIdeReader &r, int32 tag)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CSimpleIdeEntry e;
		LireSimple(r, e, tag);
		ms_numCashScnd++;
		if(ms_simpleHandler) ms_simpleHandler(e);
	}
}

// 0x0042a6a0 : modelinfo de vêtement (0x51c910), txd, bornes des ids
// (le dernier est un maximum, pas le dernier lu)
void
CIdeBinary::LoadClth(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CSimpleIdeEntry e;
		LireSimple(r, e, IDE_CLTH);
		if(ms_firstClothId == -1) ms_firstClothId = e.id;
		if(ms_lastClothId < e.id) ms_lastClothId = e.id;
		ms_numClth++;
		if(ms_simpleHandler) ms_simpleHandler(e);
	}
}

// 0x0042c970
bool
CIdeBinary::Load(const uint8 *data, uint32 size)
{
	CIdeReader r(data, size);
	while(!r.AtEnd()){
		int32 tag = r.Int();
		switch(tag){
		case IDE_PEDS: LoadPeds(r); break;
		case IDE_OBJS: LoadObjs(r); break;
		case IDE_CARS: LoadCars(r); break;
		case IDE_WEAP: LoadWeap(r); break;
		case IDE_ITEM: LoadItem(r); break;
		case IDE_CASH: case IDE_SCND: LoadCashScnd(r, tag); break;
		case IDE_CLTH: LoadClth(r); break;
		case IDE_TOBJ: LoadTobj(r); break;
		case IDE_ACCS: LoadAccs(r); break;
		case IDE_PANM: LoadPanm(r); break;
		case IDE_2DFX: Load2dfx(r); break;
		case IDE_PATH: r.Int(); break;                  // « path » : un dword sauté
		default: return false;                          // section pas encore recréée
		}
		int32 first = r.Int();
		int32 last = r.Int();
		if(first != -1) RegisterModelRange((uint16)first, (uint32)(last - first + 1));
	}
	return true;
}
