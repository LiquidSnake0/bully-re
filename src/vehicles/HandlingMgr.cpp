// cHandlingDataMgr::LoadHandlingData, 0x004c9d30, recréé depuis bully.exe.
// Structure de reVC : le fichier entier dans work_buff, une ligne à la fois,
// « ; » commentaire, « ! » vélo, « $ » lu puis ignoré (les hydravions de Vice
// City), « % » bateau, sinon une ligne de véhicule ; ";the end" est reconnu
// mais n'arrête pas la boucle dans le binaire, c'est la fin du tampon qui l'arrête.
#include "HandlingMgr.h"
#include <cmath>
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
				case 12: handling->Transmission.nNumberOfGears = (int8)atol(word); break;
				case 13: handling->Transmission.fMaxVelocity = (float)atof(word); break;
				case 14: handling->Transmission.fEngineAcceleration = (float)atof(word) * HANDLING_ACCEL_SCALE; break;
				case 15: handling->Transmission.nDriveType = word[0]; break;
				case 16: handling->Transmission.nEngineType = word[0]; break;
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
				case 30: sscanf(word, "%x", &handling->Flags); handling->Transmission.Flags = (uint8)handling->Flags; break;
				case 31: handling->FrontLights = (int8)atol(word); break;
				case 32: handling->RearLights = (int8)atol(word); break;
				case 33: handling->fPedDamage = (float)atof(word); break;
				}
			}
			ConvertDataToGameUnits(handling);                           // 0x4c9ad0
		}
	}
}

// 0x004ca5f0
void
CTransmission::InitGearRatios(void)
{
	memset(Gears, 0, sizeof(Gears));
	for(int i = 1; i <= nNumberOfGears; i++){
		tGear *prev = &Gears[i-1];
		tGear *gear = &Gears[i];
		gear->fMaxVelocity = i * (1.0f/nNumberOfGears) * fMaxVelocity;
		float diff = gear->fMaxVelocity - prev->fMaxVelocity;
		if(i < nNumberOfGears){
			Gears[i+1].fShiftDownVelocity = diff * 0.42f + prev->fMaxVelocity;     // 0x90d088
			gear->fShiftUpVelocity = diff * 0.95f + prev->fMaxVelocity;           // 0x900d40
		}else
			gear->fShiftUpVelocity = fMaxVelocity;
	}
	Gears[0].fMaxVelocity = fMaxReverseVelocity;
	Gears[0].fShiftUpVelocity = -0.01f;                                          // 0x912f04
	Gears[0].fShiftDownVelocity = fMaxReverseVelocity;
	Gears[1].fShiftDownVelocity = -0.01f;
}

// 0x004c9ad0. Les deux premiers facteurs sont des globales non
// initialisées statiquement (0xc2dec0, 0xc2dec8) dont l'écriture n'a pas
// été retrouvée ; on prend les valeurs de Vice City, 1/(50·50) et
// 1000/(60·60·50). Les autres constantes viennent de .rdata.
void
cHandlingDataMgr::ConvertDataToGameUnits(tHandlingData *handling)
{
	CTransmission &t = handling->Transmission;
	const float accelScale = 1.0f/(50.0f*50.0f);             // 0xc2dec0
	const float velScale = 1000.0f/(60.0f*60.0f*50.0f);      // 0xc2dec8

	float accel = t.fEngineAcceleration * accelScale;
	t.fEngineAcceleration = accel;
	float velocity = t.fMaxVelocity * velScale;
	t.fMaxVelocity = velocity;
	handling->fBrakeDeceleration *= accelScale;
	handling->fTurnMass = (sq(handling->Dimension.x) + sq(handling->Dimension.y)) * handling->fMass / 12.0f;   // 0x900de0
	if(handling->fTurnMass < 10.0f)                                                                             // 0x900d38
		handling->fTurnMass *= 5.0f;                                                                            // 0x900af0
	handling->fInvMass = 1.0f/handling->fMass;
	handling->fBuoyancy = handling->fMass * 0.008f * 100.0f / (uint8)handling->nPercentSubmerged;              // 0x905a70, 0x900130
	handling->fCollisionDamageMultiplier = handling->fCollisionDamageMultiplier * 2000.0f / handling->fMass;    // 0x90c070

	// vitesse de pointe atteignable : on descend par pas de 0.01 tant que
	// la résistance (section frontale × 0.5 / masse) l'emporte sur le
	// sixième de l'accélération
	float resistance = handling->Dimension.x * 0.5f * handling->Dimension.z / handling->fMass;                  // 0x8ff1f8
	for(;;){
		if(velocity <= 0.0f) break;
		velocity -= 0.01f;                                                                                      // 0x912890
		if(!(accel * (1.0f/6.0f) < -velocity * (1.0f/(velocity * resistance * velocity + 1.0f) - 1.0f)))         // 0x912ec8
			break;
	}
	t.fMaxVelocity = velocity;
	if(handling->nIdentifier >= 0x55 && handling->nIdentifier <= 0x63)
		t.fMaxReverseVelocity = -0.1f;                                                                          // 0x90653c : vélos
	else{
		float reverse = velocity * -0.35f;                                                                      // 0x912ec0
		if(reverse < -0.2f) reverse = -0.2f;                                                                    // 0x900e40 / 0x900e38
		t.fMaxReverseVelocity = reverse;
	}
	if(t.nDriveType == '4')
		t.fEngineAcceleration *= 0.25f;                                                                         // 0x900498
	else
		t.fEngineAcceleration *= 0.5f;                                                                          // 0x8ff1f8
	t.InitGearRatios();
}

// 0x004c9ca0 : comme Vice City, les angles passent en sinus ou en radians
// (0x900160 / 0x900158 = π / 180, 0x85aec0 = sin).
void
cHandlingDataMgr::ConvertBikeDataToGameUnits(tBikeHandlingData *bike)
{
	bike->fMaxLean = sinf(DEGTORAD(bike->fMaxLean));
	bike->fFullAnimLean = DEGTORAD(bike->fFullAnimLean);
	bike->fWheelieAng = sinf(DEGTORAD(bike->fWheelieAng));
	bike->fStoppieAng = sinf(DEGTORAD(bike->fStoppieAng));
}
