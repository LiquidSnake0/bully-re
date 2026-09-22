#include "IdeBinary.h"
#include <cstring>

int32 (*CIdeBinary::ms_pedHandler)(const CPedIdeEntry &e) = nil;
int32 CIdeBinary::ms_numPeds;

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

// 0x0042c970
bool
CIdeBinary::Load(const uint8 *data, uint32 size)
{
	CIdeReader r(data, size);
	while(!r.AtEnd()){
		int32 tag = r.Int();
		switch(tag){
		case IDE_PEDS: LoadPeds(r); break;
		case IDE_PATH: r.Int(); break;                  // « path » : un dword sauté
		default: return false;                          // section pas encore recréée
		}
		int32 first = r.Int();
		int32 last = r.Int();
		if(first != -1) RegisterModelRange((uint16)first, (uint32)(last - first + 1));
	}
	return true;
}
