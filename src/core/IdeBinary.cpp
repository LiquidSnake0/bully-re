#include "IdeBinary.h"
#include <cstring>
#include <strings.h>

int32 (*CIdeBinary::ms_pedHandler)(const CPedIdeEntry &e) = nil;
int32 (*CIdeBinary::ms_objHandler)(const CObjIdeEntry &e) = nil;
int32 CIdeBinary::ms_numPeds;
int32 CIdeBinary::ms_numObjs;

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
void
CIdeBinary::LoadObjs(CIdeReader &r)
{
	int32 count = r.Int();
	for(int32 i = 0; i < count; i++){
		CObjIdeEntry e;
		memset(&e, 0, sizeof(e));
		e.type = r.Int();
		if(e.type < 0 || e.type > 5) return;            // le binaire saute au default et laisse tout tel quel
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
		ms_numObjs++;
		if(ms_objHandler) ms_objHandler(e);
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
		case IDE_PATH: r.Int(); break;                  // « path » : un dword sauté
		default: return false;                          // section pas encore recréée
		}
		int32 first = r.Int();
		int32 last = r.Int();
		if(first != -1) RegisterModelRange((uint16)first, (uint32)(last - first + 1));
	}
	return true;
}
