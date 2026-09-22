// Charge Config/Dat/OBJECT.DAT avec CObjectData::Initialise recréé et
// vérifie contre le fichier : 292 entrées, 80 alias, DrmGarbagecn.
//   BULLY_DATA=<racine du jeu> build/tests/test_objectdata
#include "../src/objects/ObjectData.h"
#include <cstdio>

int NombreDeModelesObjets(void);
static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CObjectData::Initialise("Config\\Dat\\OBJECT.DAT");
	printf("%d entrées, %d alias\n", CObjectData::ms_numObjectInfos, CObjectData::ms_numAliases);
	VERIF(CObjectData::ms_numObjectInfos == 292);
	VERIF(CObjectData::ms_numAliases == 80);
	CObjectInfo &g = CObjectData::ms_aObjectInfo[0];             // DrmGarbagecn, première ligne
	VERIF(g.modelIndex == FindModelIndexByName("DrmGarbagecn"));
	VERIF(g.weaponModelIndex == -1);                              // None
	VERIF((g.hitPointsAndLock & 0x7fff) == 30000 && (g.hitPointsAndLock >> 15) == 0);
	VERIF(g.destroyByWeaponOnly == 0);
	VERIF(g.destroyedEffect1 == 0xffff && g.destroyedEffect2 == 0xffff && g.hitEffect == 0xffff);
	VERIF(g.soundBank == GetSoundBankIdByName("objects/GrbCan.bnk"));
	VERIF(g.soundLoadRangeSq == 64);
	VERIF(g.idleSound == 0 && (g.idleVolumeAndStream & 1) == 0);
	VERIF((g.idleVolumeAndStream >> 1) == 100);                    // IdleVolume 1 → 100 %
	VERIF(g.breakSound == 0 && g.hitSound == 0);
	VERIF((g.volumeTables & 0xf) == VOL_LARGE && (g.volumeTables >> 4) == VOL_LARGE);
	VERIF(g.pickupZOffset == 0x33);
	// PGGarbagecn01 est un alias de DrmGarbagecn : cible résolue vers l'entrée 0
	CObjectAlias &a = CObjectData::ms_aAliases[0];
	VERIF(a.modelIndex == FindModelIndexByName("PGGarbagecn01"));
	VERIF(a.target == 0);
	// Aud_Speaker : 50 pv, effets nommés, portée 25, volumes medium / large
	int k = -1;
	for(int i = 0; i < CObjectData::ms_numObjectInfos; i++)
		if(CObjectData::ms_aObjectInfo[i].modelIndex == FindModelIndexByName("Aud_Speaker")) k = i;
	VERIF(k >= 0);
	if(k >= 0){
		CObjectInfo &s = CObjectData::ms_aObjectInfo[k];
		VERIF((s.hitPointsAndLock & 0x7fff) == 50);
		VERIF(s.destroyedEffect1 == GetEffectIdByName("LightShatter"));
		VERIF(s.soundLoadRangeSq == 625);
		VERIF(s.breakSound == GetSoundIdByName("BreakSparks") && s.hitSound == GetSoundIdByName("ComputerHit"));
		VERIF((s.volumeTables & 0xf) == VOL_MEDIUM && (s.volumeTables >> 4) == VOL_LARGE);
	}
	printf(echecs ? "%d échec(s)\n" : "tout passe\n", echecs);
	return echecs != 0;
}
