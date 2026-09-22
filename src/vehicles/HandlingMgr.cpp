// cHandlingDataMgr::LoadHandlingData, 0x004c9d30, recréé depuis bully.exe.
// Structure de reVC : le fichier entier dans work_buff, une ligne à la fois,
// « ; » commentaire, « ! » vélo, « $ » lu puis ignoré (les hydravions de Vice
// City), « % » bateau, sinon une ligne de véhicule ; ";the end" est reconnu
// mais n'arrête pas la boucle dans le binaire, c'est la fin du tampon qui l'arrête.
#include "HandlingMgr.h"
#include "../core/FileMgr.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

cHandlingDataMgr mod_HandlingManager;
const char *cHandlingDataMgr::HandlingFilename = "HANDLING.CFG";   // 0x00a357c0

// 0x00a357c8 : la table de Vice City, dont les motos sont remplacées par les
// vélos de Bully (ids 84 à 99), bateaux en 100 à 110.
static const char aVehicleNames[NUM_HANDLING_IDS][HANDLING_NAME_LEN] = {
	"REB", "IDAHO", "STINGER", "LINERUN", "PEREN", "SENTINEL",
	"PATRIOT", "FIRETRUK", "TRASH", "STRETCH", "MANANA", "INFERNUS",
	"PONY", "MULE", "CHEETAH", "AMBULAN", "FBICAR", "MOONBEAM",
	"TAXI", "KURUMA", "BOBCAT", "MRWHOOP", "BFINJECT", "POLICE",
	"ENFORCER", "SECURICA", "BANSHEE", "BUS", "RHINO", "BARRACKS",
	"TRAIN", "HELI", "DODO", "COACH", "CABBIE", "STALLION",
	"RUMPO", "RCBANDIT", "MAFIA", "AIRTRAIN", "DEADDODO", "FLATBED",
	"YANKEE", "GOLFCART", "VOODOO", "WASHING", "CUBAN", "ROMERO",
	"PACKER", "ADMIRAL", "GANGBUR", "ZEBRA", "TOPFUN", "GLENDALE",
	"OCEANIC", "HERMES", "SABRE1", "SABRETUR", "FORMULA", "PHEONIX",
	"WALTON", "REGINA", "COMET", "DELUXO", "BURRITO", "COPSUV",
	"ARCADE1", "ARCADE2", "ARCADE3", "SPAND", "BAGGAGE", "KAUFMAN",
	"RANCHER", "FBIRANCH", "VIRGO", "GREENWOO", "HOTRING", "SANDKING",
	"BLISTAC", "BOXVILLE", "BENSON", "DESPERAD", "LOVEFIST", "BLOODRA",
	"BLOODRB", "BIKE", "CUSTOMMIN", "CUSTOMMAX", "BMXRACE", "RETRO",
	"CRAPBMX", "BANBIKE", "MTNBIKE", "OLADBIKE", "RACER", "SKATEBRD",
	"MOPED", "DIRTBIKE", "ANGEL", "FREEWAY", "PREDATOR", "SPEEDER",
	"REEFER", "RIO", "SQUALO", "TROPIC", "COASTGRD", "DINGHY",
	"MARQUIS", "CUPBOAT", "SEAPLANE", "SPARROW", "SEASPAR", "MAVERICK",
	"COASTMAV", "POLMAV", "HUNTER", "RCBARON", "RCGOBLIN", "RCCOPTER",
};
const char *cHandlingDataMgr::ms_aVehicleNames = &aVehicleNames[0][0];

// 0x004c9a20 : compare le mot au début de chaque entrée de la table (sur la
// longueur de l'entrée), retourne l'index ou numWords si absent.
int32
cHandlingDataMgr::FindExactWord(const char *word, const char *table, int32 wordLen, int32 numWords)
{
	for(int32 i = 0; i < numWords; i++, table += wordLen){
		int32 len = (int32)strlen(table);
		if(strncmp(word, table, len) == 0) return i;
	}
	return numWords;
}

extern const float HANDLING_ACCEL_SCALE;   // 0x00900de8, appliquée à fEngineAcceleration à la lecture

void
cHandlingDataMgr::LoadHandlingData(void)
{
	char path[256];
	char line[200];
	const char delim[] = " \t";      // 0x912ed4 / 0x912ed6
	sprintf(path, "Config\\%s", HandlingFilename);
	int32 lu = CFileMgr::LoadFile(path, work_buff, sizeof(work_buff), "r");
	char *fin = (char*)work_buff + (lu > 0 ? lu : 0);

	char *start = (char*)work_buff;
	char *end = start + 1;
	tHandlingData *handling = nil;
	tBikeHandlingData *bike = nil;
	tBoatHandlingData *boat = nil;
	while(start < fin){
		while(end < fin && *end != '\n') end++;
		int len = (int)(end - start);
		if(len > (int)sizeof(line) - 1) len = sizeof(line) - 1;
		strncpy(line, start, len);
		line[len] = '\0';
		start = end + 1;
		end = start + 1;
		if(strncmp(line, ";the end", 8) == 0) break;

		if(line[0] == ';') continue;
		int field = 0;
		if(line[0] == '!'){
			for(char *word = strtok(line, delim); word; word = strtok(nil, delim), field++){
				if(field == 0) continue;
				if(field == 1){
					int32 id = FindExactWord(word, ms_aVehicleNames, HANDLING_NAME_LEN, NUM_HANDLING_IDS);
					bike = &BikeHandlingData[id];
					bike->nIdentifier = id;
				}else if(field <= 22){
					((float*)bike)[field - 1] = (float)atof(word);   // champs 2..22 → mots 1..21
				}
			}
			ConvertBikeDataToGameUnits(bike);                           // 0x4c9ca0
		}else if(line[0] == '$'){
			for(char *word = strtok(line, delim); word; word = strtok(nil, delim)) {}
		}else if(line[0] == '%'){
			for(char *word = strtok(line, delim); word; word = strtok(nil, delim), field++){
				if(field == 0) continue;
				if(field == 1){
					int32 id = FindExactWord(word, ms_aVehicleNames, HANDLING_NAME_LEN, NUM_HANDLING_IDS);
					boat = (uint8)(id - BOAT_HANDLING_FALLBACK_ID) < NUM_BOAT_HANDLING
						? &BoatHandlingData[id - BOAT_HANDLING_FALLBACK_ID] : &BoatHandlingFallback;
					boat->nIdentifier = id;
				}else{
					float v = (float)atof(word);
					switch(field){
					case  2: boat->fThrustY = v; break;
					case  3: boat->fThrustZ = v; break;
					case  4: boat->fThrustAppZ = v; break;
					case  5: boat->fAqPlaneForce = v; break;
					case  6: boat->fAqPlaneLimit = v; break;
					case  7: boat->fAqPlaneOffset = v; break;
					case  8: boat->fWaveAudioMult = v; break;
					case  9: boat->vecMoveRes.x = v; break;
					case 10: boat->vecMoveRes.y = v; break;
					case 11: boat->vecMoveRes.z = v; break;
					case 12: boat->vecTurnRes.x = v; break;
					case 13: boat->vecTurnRes.y = v; break;
					case 14: boat->vecTurnRes.z = v; break;
					case 15: boat->fLook_L_R_BehindCamHeight = v; break;
					}
				}
			}
		}else{
			for(char *word = strtok(line, delim); word; word = strtok(nil, delim), field++){
				switch(field){
				case  0: {
					int32 id = FindExactWord(word, ms_aVehicleNames, HANDLING_NAME_LEN, NUM_HANDLING_IDS);
					handling = &HandlingData[id];
					handling->nIdentifier = id;
					break; }
				case  1: handling->fMass = (float)atof(word); break;
				case  2: handling->Dimension.x = (float)atof(word); break;
				case  3: handling->Dimension.y = (float)atof(word); break;
				case  4: handling->Dimension.z = (float)atof(word); break;
				case  5: handling->CentreOfMass.x = (float)atof(word); break;
				case  6: handling->CentreOfMass.y = (float)atof(word); break;
				case  7: handling->CentreOfMass.z = (float)atof(word); break;
				case  8: handling->nPercentSubmerged = (int8)atol(word); break;
				case  9: handling->fTractionMultiplier = (float)atof(word); break;
				case 10: handling->fTractionLoss = (float)atof(word); break;
				case 11: handling->fTractionBias = (float)atof(word); break;
				case 12: handling->nNumberOfGears = (int8)atol(word); break;
				case 13: handling->fMaxVelocity = (float)atof(word); break;
				case 14: handling->fEngineAcceleration = (float)atof(word) * HANDLING_ACCEL_SCALE; break;
				case 15: handling->nDriveType = word[0]; break;
				case 16: handling->nEngineType = word[0]; break;
				case 17: handling->fBrakeDeceleration = (float)atof(word); break;
				case 18: handling->fBrakeBias = (float)atof(word); break;
				case 19: handling->bABS = atol(word) != 0; break;
				case 20: handling->fSteeringLock = (float)atof(word); break;
				case 21: handling->fSuspensionForceLevel = (float)atof(word); break;
				case 22: handling->fSuspensionDampingLevel = (float)atof(word); break;
				case 23: handling->fSeatOffsetDistance = (float)atof(word); break;
				case 24: handling->fCollisionDamageMultiplier = (float)atof(word); break;
				case 25: handling->nMonetaryValue = (int32)atol(word); break;
				case 26: handling->fSuspensionUpperLimit = (float)atof(word); break;
				case 27: handling->fSuspensionLowerLimit = (float)atof(word); break;
				case 28: handling->fSuspensionBias = (float)atof(word); break;
				case 29: handling->fSuspensionAntidiveMultiplier = (float)atof(word); break;
				case 30: sscanf(word, "%x", &handling->Flags); handling->nFlagsLow = (uint8)handling->Flags; break;
				case 31: handling->FrontLights = (int8)atol(word); break;
				case 32: handling->RearLights = (int8)atol(word); break;
				case 33: handling->fPedDamage = (float)atof(word); break;
				}
			}
			ConvertDataToGameUnits(handling);                           // 0x4c9ad0
		}
	}
}
