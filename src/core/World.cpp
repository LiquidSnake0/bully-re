// CWorld::Initialise, 0x0045d430 : remise à zéro des drapeaux du monde,
// comme dans reVC (pIgnoreEntity, bDoingCarCollisions, bSecondShift…),
// puis initialisation d'un sous-système en 0x4397a0 (tas 0x1e, crée un pool
// de 0x1c octets en 0xbd5c4c) et `AreaRemoveExtraScene`.
#include "World.h"

CSector CWorld::ms_aSectors[NUMSECTORS_Y][NUMSECTORS_X];

uint8 CWorld::ms_flags6c[5];    // 0xc1ae6c .. 0xc1ae70
uint8 CWorld::ms_flags74[3];    // 0xc1ae74 .. 0xc1ae76
uint8 CWorld::ms_flags80[3];    // 0xc1ae80 .. 0xc1ae82
uint32 CWorld::ms_field68;      // 0xc1ae68

extern void WorldSubsystemInit(void);   // 0x4397a0 (thunk)
extern void AreaRemoveExtraScene(void); // 0x4297f0, aussi liaison Lua

void
CWorld::Initialise(void)
{
	ms_flags74[2] = ms_flags74[1] = ms_flags74[0] = 0;
	ms_flags6c[3] = ms_flags6c[2] = ms_flags6c[1] = ms_flags6c[0] = 0;
	ms_flags80[0] = ms_flags80[1] = ms_flags80[2] = 0;
	ms_flags6c[4] = 0;
	ms_field68 = 0;
	WorldSubsystemInit();
	AreaRemoveExtraScene();
}
