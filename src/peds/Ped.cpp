// Recréation partielle de CPed et CPlayerPed à partir de bully.exe.
#include "PlayerPed.h"
#include "../core/Timer.h"

// Configuration globale des piétons (0x020c889c) : +0x90 seuil, +0x94 et
// +0x98 vitesses de décroissance. À nommer quand le chargeur sera lu.
struct CPedConfig { uint8 pad[0x84]; int32 clearThreshold; uint8 pad2[8]; int32 threshold; int32 decayRate; int32 playerDecayFactor; };
extern CPedConfig *gPedConfig;   // 0x020c889c
extern void PlayFrontEndSound(int32 id, int32 param);   // 0x458330

// 0x00477480, slot 45. Points de punition : la liaison Lua PedSetPunishmentPoints
// (0x5ccdf0) appelle 0x4773d0 avec la valeur, et cette méthode finit par le
// même appel : c'est IncPunishmentPoints, la jauge est +0x1d40.
void
CPed::IncPunishmentPoints(int32 amount)
{
	int16 id = m_modelIndex;
	if(id == ms_reservedPedModel[0] || id == ms_reservedPedModel[1] || id == ms_reservedPedModel[2] ||
	   id == ms_reservedPedModel[3] || id == ms_reservedPedModel[4])
		return;                                   // 0xbf42d4, 0xbf42d0, 0xbf42cc, 0xbf42dc, 0xbf42d8
	if(m_nPedState == PED_STATE_PLAYER_CONTROLLED && amount > 0)
		PlayFrontEndSound(0x9a, amount);
	if(amount < gPedConfig->threshold)
		OnSmallPunishment();                         // 0x474480
	else{
		m_field137c = 0;
		m_field1d44 = 0;
	}
	m_nPunishmentPoints += amount;
	if(m_nPunishmentPoints < 0) m_nPunishmentPoints = 0;
	SetPunishmentPoints(m_nPunishmentPoints);
}

// 0x00477530, slot 44. La jauge redescend d'un pas par tick de temps ;
// pour le joueur le pas est multiplié par un facteur quand un drapeau global
// (joueur +0x1e91, ou mode 0xe en 0xbd1008) est actif. Le drapeau +0x14a8
// est réévalué en parcourant le pool de piétons : il reste levé s'il existe
// un piéton actif dans l'état 0, 7 ou 8 pour lequel 0x48d320(this) est vrai.
extern struct CPedPool { CPed *entries; uint8 *flags; int32 size; int32 entrySize; } *gPedPool;   // 0xc0f5f0
extern uint8 *gPlayerFlag1e91;    // joueur courant (0xc1aea8) + 0x1e91
extern int32 gGameMode;           // 0xbd1008
extern bool PedPunishmentCheck(CPed *self);   // 0x48d320
extern void PlayerPunishmentDecay(int32 step);   // 0x49a6e0

void
CPed::ProcessPunishmentDecay(void)
{
	int32 step = gPedConfig->decayRate;
	if(m_nPedState == PED_STATE_PLAYER_CONTROLLED && (*gPlayerFlag1e91 != 0 || gGameMode == 0xe)){
		step *= gPedConfig->playerDecayFactor;
		m_nPunishmentLastTick = 0;
	}
	if(m_field14a8 != 0){
		m_field14a8 = 0;
		for(int32 i = gPedPool->size - 1; i >= 0; i--){
			if(gPedPool->flags[i] & 0x80) continue;                      // case libre
			CPed *ped = (CPed*)((uint8*)gPedPool->entries + gPedPool->entrySize * i);
			if(ped == nil) continue;
			int32 st = ped->m_nPedState;
			if(st != 0 && st != 8 && st != 7) continue;
			if(!PedPunishmentCheck(this)) continue;
			m_field14a8 = 1;
			break;
		}
		if(m_field14a8 != 0) goto fin;
	}
	if(m_nPunishmentPoints > 0 && m_nPunishmentLastTick < CTimer::m_snTimeInMilliseconds){
		if(m_nPedState == PED_STATE_PLAYER_CONTROLLED)
			PlayerPunishmentDecay(step);
		else
			m_nPunishmentPoints -= step;
		if(m_nPunishmentPoints < 0) m_nPunishmentPoints = 0;
	}
fin:
	if(m_field137c != 0 && m_nPunishmentPoints <= gPedConfig->clearThreshold)   // +0x84
		m_field137c = 0;
}

// 0x0049a6a0, slot 44 pour le joueur : décroissance de la base, puis un
// plancher propre au joueur.
void
CPlayerPed::ProcessPunishmentDecay(void)
{
	CPed::ProcessPunishmentDecay();
	if((uint32)m_nPunishmentPoints < m_nMinPunishmentPoints)
		m_nPunishmentPoints = m_nMinPunishmentPoints;
}
