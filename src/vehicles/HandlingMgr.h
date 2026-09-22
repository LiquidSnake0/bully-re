// cHandlingDataMgr, hérité de GTA. bully.exe : LoadHandlingData 0x4c9d30,
// ConvertDataToGameUnits 0x4c9ad0, FindExactWord 0x4c9a20 sur une table de
// 120 identifiants de 14 caractères en 0x00a357c8. Le fichier est
// `Config/handling.cfg`, signé Bill Henderson, 10/12/1999, le format de GTA III.
//
// Disposition d'une entrée (0xdc octets) lue dans LoadHandlingData ; les
// décalages entre parenthèses sont ceux du binaire. Les champs non écrits par
// le chargeur (masse inverse, moment d'inertie, flottabilité, transmission
// dérivée…) sont calculés par ConvertDataToGameUnits.
#pragma once
#include "../common.h"

enum {
	NUM_HANDLING_IDS = 120,
	HANDLING_NAME_LEN = 14,
	NUM_BIKE_HANDLING = NUM_HANDLING_IDS,   // indexé par identifiant (id × 0x5c depuis +0x48a8)
	NUM_BOAT_HANDLING = 11,          // identifiants 100 à 110 (0x9c + 0xb)
	BOAT_HANDLING_FALLBACK_ID = 100
};

// Boîte de vitesses, comme CTransmission de reVC mais sans vitesse de
// croisière : 0x5c octets, à +0x34 de tHandlingData. InitGearRatios est
// 0x4ca5f0 (les rapports de rétrogradage et de passage valent 0.42 et
// 0.95 de l'écart, contre 0.42 et 0.6667 dans Vice City).
struct tGear {
	float fMaxVelocity;
	float fShiftUpVelocity;
	float fShiftDownVelocity;
};

class CTransmission
{
public:
	tGear Gears[6];                // [+0x00] marche arrière puis 5 rapports
	char nDriveType;               // [+0x48] F / R / 4
	char nEngineType;              // [+0x49] P / D / E
	int8 nNumberOfGears;           // [+0x4a]
	uint8 Flags;                   // [+0x4b] octet bas des drapeaux
	float fEngineAcceleration;     // [+0x4c] × constante 0x900de8 (0.4) à la lecture
	float fMaxVelocity;            // [+0x50]
	float fMaxReverseVelocity;     // [+0x54]
	float fCurVelocity;            // [+0x58]

	void InitGearRatios(void);     // 0x4ca5f0
};

struct tHandlingData {
	int32 nIdentifier;             // [0x00]
	float fMass;                   // [0x04]
	float fInvMass;                // [0x08] calculé
	float fTurnMass;               // [0x0c] calculé
	CVector Dimension;             // [0x10]
	CVector CentreOfMass;          // [0x1c]
	int8 nPercentSubmerged;        // [0x28]
	uint8 pad29[3];
	float fBuoyancy;               // [0x2c] calculé
	float fTractionMultiplier;     // [0x30]
	CTransmission Transmission;    // [0x34..0x8f]
	float fBrakeDeceleration;      // [0x90]
	float fBrakeBias;              // [0x94]
	bool bABS;                     // [0x98]
	uint8 pad99[3];
	float fSteeringLock;           // [0x9c]
	float fTractionLoss;           // [0xa0]
	float fTractionBias;           // [0xa4]
	uint8 pada8[4];
	float fSuspensionForceLevel;   // [0xac]
	float fSuspensionDampingLevel; // [0xb0]
	float fSuspensionUpperLimit;   // [0xb4]
	float fSuspensionLowerLimit;   // [0xb8]
	float fSuspensionBias;         // [0xbc]
	float fSuspensionAntidiveMultiplier; // [0xc0]
	float fCollisionDamageMultiplier;    // [0xc4]
	float fPedDamage;              // [0xc8] colonne (ah), propre à Bully
	uint32 Flags;                  // [0xcc]
	float fSeatOffsetDistance;     // [0xd0]
	int32 nMonetaryValue;          // [0xd4]
	int8 FrontLights;              // [0xd8]
	int8 RearLights;               // [0xd9]
	uint8 padda[2];
};

// Ligne « ! » : 23 champs contre 16 dans Vice City, 0x5c octets.
struct tBikeHandlingData {
	int32 nIdentifier;
	float fLeanFwdCOM, fLeanFwdForce, fLeanBakCOM, fLeanBackForce;
	float fMaxLean, fFullAnimLean, fDesLean, fSpeedSteer, fSlipSteer;
	float fNoPlayerCOMz, fWheelieAng, fStoppieAng, fWheelieSteer;
	float fWheelieStabMult, fStoppieStabMult;
	float fExtra[6];               // six valeurs de plus que Vice City, à nommer
};

// Ligne « % » : la structure bateau de Vice City, 0x3c octets. Le dernier
// champ du fichier va dans fLook_L_R_BehindCamHeight.
struct tBoatHandlingData {
	int32 nIdentifier;
	float fThrustY, fThrustZ, fThrustAppZ;
	float fAqPlaneForce, fAqPlaneLimit, fAqPlaneOffset, fWaveAudioMult;
	float fLook_L_R_BehindCamHeight;
	CVector vecMoveRes;
	CVector vecTurnRes;
};

class cHandlingDataMgr
{
public:
	float field_0;                 // +0x00
	float fWheelFriction;          // +0x04 …+0x10, comme reVC
	float field_8, field_c, field_10;
	tHandlingData HandlingData[NUM_HANDLING_IDS];          // +0x14, 120 × 0xdc = 0x6720 → 0x6734
	tBikeHandlingData BikeHandlingData[NUM_BIKE_HANDLING]; // +0x48a8, indexé par id ; disposition mémoire exacte non reproduite
	tBoatHandlingData BoatHandlingData[NUM_BOAT_HANDLING]; // +0x5528
	tBoatHandlingData BoatHandlingFallback;                // +0x6c98

	void LoadHandlingData(void);                            // 0x4c9d30
	void ConvertDataToGameUnits(tHandlingData *h);          // 0x4c9ad0
	void ConvertBikeDataToGameUnits(tBikeHandlingData *b);  // 0x4c9ca0
	int32 FindExactWord(const char *word, const char *table, int32 wordLen, int32 numWords);   // 0x4c9a20

	static const char *ms_aVehicleNames;                    // 0x00a357c8, 120 × 14
	static const char *HandlingFilename;                    // PTR_s_HANDLING_CFG_00a357c0
};

extern cHandlingDataMgr mod_HandlingManager;
