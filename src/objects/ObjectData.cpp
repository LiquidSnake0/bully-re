// CObjectData::Initialise, 0x004d0c90, recréé depuis bully.exe.
#include "ObjectData.h"
#include "../core/Tokenizer.h"
#include "../vehicles/VehicleColours.h"   // CFileStream
#include <cstring>
#include <strings.h>
#include <cmath>

CObjectInfo CObjectData::ms_aObjectInfo[NUM_OBJECT_DATA];
CObjectAlias CObjectData::ms_aAliases[NUM_OBJECT_ALIASES];
int32 CObjectData::ms_numObjectInfos;
int32 CObjectData::ms_numAliases;
bool CObjectData::ms_bPrehashedNames = false;
bool CObjectData::ms_bSkipPostInit = false;
uint32 CObjectData::ms_defaultDamageableClass = 0;

static char gNameBuf[OBJECT_NAME_BUF];          // 0x00c34e50

static bool
EstNone(const char *s)
{
	return strncmp(s, "None", 5) == 0;         // comparaison sur 5 octets dans le binaire
}

// 0x004d0a60
uint8
CObjectData::ParseVolumeTable(const char *word)
{
	if(strcasecmp(word, "small") == 0) return VOL_SMALL;
	if(strcasecmp(word, "medium") == 0) return VOL_MEDIUM;
	if(strcasecmp(word, "large") == 0) return VOL_LARGE;
	if(strcasecmp(word, "speech") == 0) return VOL_SPEECH;
	if(strcasecmp(word, "extralarge") == 0) return VOL_EXTRALARGE;
	if(strcasecmp(word, "jumbo") == 0) return VOL_JUMBO;
	if(strcasecmp(word, "supersize") == 0) return VOL_SUPERSIZE;
	return strcasecmp(word, "generic") != 0;
}

void
CObjectData::Initialise(const char *filename)
{
	char line[0x200];
	CTokenizer tok;

	for(int i = 0; i < NUM_OBJECT_DATA; i++) ms_aObjectInfo[i].modelIndex = -1;
	for(int i = 0; i < NUM_OBJECT_ALIASES; i++){ ms_aAliases[i].modelIndex = -1; ms_aAliases[i].target = -1; }
	ms_numObjectInfos = 0;
	ms_numAliases = 0;

	CFileStream file;
	file.Open(filename);                               // 0x4264c0
	while(file.ReadLine(line, sizeof(line))){          // 0x42d4b0
		if(line[0] == ';') continue;
		tok.Init(line, " \t\r\n");                     // 0x913710
		if(tok.AtEnd()) continue;

		CObjectInfo *info = &ms_aObjectInfo[ms_numObjectInfos];
		info->modelIndex = -1;

		// A : ModelName
		if(!ms_bPrehashedNames){
			tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
			info->modelIndex = FindModelIndexByName(gNameBuf);
		}else
			info->modelIndex = tok.AsLong();
		tok.Next();

		// B : WeaponModelName
		if(!ms_bPrehashedNames){
			tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
			info->weaponModelIndex = EstNone(gNameBuf) ? -1 : FindModelIndexByName(gNameBuf);
		}else
			info->weaponModelIndex = tok.AsLong();
		tok.Next();

		// C, D, E : HitPoints, ManualLockTargetable, DestroyByWeaponOnly
		info->hitPointsAndLock = (info->hitPointsAndLock & 0x8000) | ((uint16)tok.AsLong() & 0x7fff);
		tok.Next();
		info->hitPointsAndLock = (uint16)(tok.AsLong() << 15) | (info->hitPointsAndLock & 0x7fff);
		tok.Next();
		info->destroyByWeaponOnly = (info->destroyByWeaponOnly & ~1) | (tok.AsLong() & 1);
		tok.Next();

		// F : DestroyedEffect(1), ou alias vers un autre modèle si c'est le dernier jeton
		tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
		tok.Next();
		if(tok.AtEnd()){
			CObjectAlias *alias = &ms_aAliases[ms_numAliases];
			alias->modelIndex = (int16)info->modelIndex;
			if(!ms_bPrehashedNames)
				alias->target = (int16)FindModelIndexByName(gNameBuf);
			else
				alias->target = (int16)atol(gNameBuf);
			ms_numAliases++;
			continue;                                  // l'entrée n'est pas comptée
		}
		info->destroyedEffect1 = EstNone(gNameBuf) ? 0xffff : GetEffectIdByName(gNameBuf);

		// G : DestroyedEffect(2)
		tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
		info->destroyedEffect2 = EstNone(gNameBuf) ? 0xffff : GetEffectIdByName(gNameBuf);
		tok.Next();
		// H, I : EffectZ-Offset
		info->effectZOffset1 = (int16)tok.AsFloat(); tok.Next();     // _ftol
		info->effectZOffset2 = (int16)tok.AsFloat(); tok.Next();
		// J : HitEffect
		tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
		info->hitEffect = EstNone(gNameBuf) ? 0xffff : GetEffectIdByName(gNameBuf);
		tok.Next();
		// K : SoundBankName
		tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
		info->soundBank = EstNone(gNameBuf) ? 0xffff : GetSoundBankIdByName(gNameBuf);
		tok.Next();
		// L : SoundLoadRange, stockée au carré
		{ int32 r = tok.AsLong(); info->soundLoadRangeSq = r * r; }
		tok.Next();
		// M : IdleSoundName : None, un flux « .rsm », « AttachToEmitter », ou un son
		tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
		info->idleVolumeAndStream &= ~1;
		info->attachToEmitter &= ~1;
		if(strcasecmp(gNameBuf, "None") == 0)
			info->idleSound = 0;
		else{
			char *dot = strchr(gNameBuf, '.');
			if(dot == nil){
				if(strcasecmp(gNameBuf, "AttachToEmitter") != 0)
					info->idleSound = GetSoundIdByName(gNameBuf);
				else
					info->attachToEmitter |= 1;
			}else if(strcasecmp(dot + 1, "rsm") == 0){
				info->idleVolumeAndStream |= 1;
				info->idleSound = GetStreamIdByName(gNameBuf, 1);
			}
			// sinon : « Warning! %s is invalid stream name in object.dat. » (0x49a500, à vide)
		}
		tok.Next();
		// N : IdleVolume, 0..1 → × 100 arrondi, décalé d'un bit
		{
			float v = tok.AsFloat();
			if(v < 0.0f) v = 0.0f; else if(v > 1.0f) v = 1.0f;
			int32 pct = (int32)lroundf(v * 100.0f);
			info->idleVolumeAndStream = (uint8)(pct * 2) | (info->idleVolumeAndStream & 1);
		}
		tok.Next();
		// O, P : BreakSoundName, HitSoundName
		tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
		info->breakSound = EstNone(gNameBuf) ? 0 : GetSoundIdByName(gNameBuf);
		tok.Next();
		tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
		info->hitSound = EstNone(gNameBuf) ? 0 : GetSoundIdByName(gNameBuf);
		tok.Next();
		// Q, R : tables de volume
		tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
		info->volumeTables = (info->volumeTables & 0xf0) | (ParseVolumeTable(gNameBuf) & 0xf);
		tok.Next();
		tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
		info->volumeTables = (info->volumeTables & 0x0f) | (uint8)(ParseVolumeTable(gNameBuf) << 4);
		// valeurs par défaut des colonnes optionnelles
		info->damageableClass = ms_defaultDamageableClass;
		info->pickupButes = 0;
		info->pickupZOffset = 0x33;
		tok.Next();
		// S : [DamageableClassName]
		if(!tok.AtEnd()){
			tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
			info->damageableClass = HashString(gNameBuf);
			tok.Next();
			// T : [PickupButesName]  (la colonne « Vandalizable? » de l'en-tête n'est pas lue)
			if(!tok.AtEnd()){
				tok.Copy(gNameBuf, OBJECT_NAME_BUF, '\0', true);
				info->pickupButes = GetPickupButesByName(gNameBuf);
				tok.Next();
				// U : [PickupZoffset]
				if(!tok.AtEnd())
					info->pickupZOffset = (int16)tok.AsFloat();
			}
		}
		ms_numObjectInfos++;
	}
	file.Close();

	// Résolution des alias : l'index de modèle visé devient l'index d'entrée.
	for(int i = 0; i < NUM_OBJECT_ALIASES; i++){
		int16 found = -1;
		for(int16 k = 0; k < NUM_OBJECT_DATA; k++)
			if(ms_aObjectInfo[k].modelIndex == ms_aAliases[i].target){ found = k; break; }
		ms_aAliases[i].target = found;
	}
	if(!ms_bSkipPostInit) ObjectDataPostInit();          // 0x4d0bc0
}
