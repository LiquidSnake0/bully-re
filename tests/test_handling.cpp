// Charge Config/handling.cfg avec LoadHandlingData recréé et vérifie les
// valeurs brutes de COMET (première ligne), de BIKE (« ! ») et de PREDATOR (« % »).
//   BULLY_DATA=<racine du jeu> build/tests/test_handling
#include "../src/vehicles/HandlingMgr.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)
static bool eq(float a, float b) { return fabsf(a - b) < 1e-4f; }

int
main(void)
{
	cHandlingDataMgr &m = mod_HandlingManager;
	memset(&m, 0, sizeof(m));
	m.LoadHandlingData();
	int32 comet = m.FindExactWord("COMET", cHandlingDataMgr::ms_aVehicleNames, HANDLING_NAME_LEN, NUM_HANDLING_IDS);
	printf("COMET = id %d\n", comet);
	VERIF(comet >= 0 && comet < NUM_HANDLING_IDS);
	tHandlingData &h = m.HandlingData[comet];
	VERIF(h.nIdentifier == comet);
	VERIF(eq(h.fMass, 2000.0f));
	VERIF(eq(h.Dimension.x, 1.9f) && eq(h.Dimension.y, 4.0f) && eq(h.Dimension.z, 1.6f));
	VERIF(eq(h.CentreOfMass.z, -0.80f));
	VERIF(h.nPercentSubmerged == 75);
	VERIF(eq(h.fTractionMultiplier, 1.80f) && eq(h.fTractionLoss, 0.90f) && eq(h.fTractionBias, 0.50f));
	VERIF(h.nNumberOfGears == 4);
	VERIF(eq(h.fMaxVelocity, 100.0f));
	VERIF(h.nDriveType == 'R' && h.nEngineType == 'D');
	VERIF(eq(h.fBrakeDeceleration, 18.0f) && eq(h.fBrakeBias, 0.45f) && !h.bABS);
	VERIF(eq(h.fSteeringLock, 35.4f));
	VERIF(eq(h.fSuspensionForceLevel, 1.0f) && eq(h.fSuspensionDampingLevel, 1.0f));
	VERIF(eq(h.fSeatOffsetDistance, 0.25f) && eq(h.fCollisionDamageMultiplier, 0.60f));
	VERIF(h.nMonetaryValue == 1);
	VERIF(eq(h.fSuspensionUpperLimit, 0.15f) && eq(h.fSuspensionLowerLimit, -0.15f));
	VERIF(eq(h.fSuspensionBias, 0.50f) && eq(h.fSuspensionAntidiveMultiplier, 0.3f));
	VERIF(h.Flags == 0x20103 && h.nFlagsLow == 0x03);
	VERIF(h.FrontLights == 1 && h.RearLights == 1);
	VERIF(eq(h.fPedDamage, 1000.0f));

	int32 bike = m.FindExactWord("BIKE", cHandlingDataMgr::ms_aVehicleNames, HANDLING_NAME_LEN, NUM_HANDLING_IDS);
	printf("BIKE = id %d\n", bike);
	tBikeHandlingData *b = &m.BikeHandlingData[bike];
	VERIF(b->nIdentifier == bike);
	VERIF(eq(b->fLeanFwdForce, 0.200f) && eq(b->fMaxLean, 30.0f) && eq(b->fDesLean, 0.93f));

	int32 predator = m.FindExactWord("PREDATOR", cHandlingDataMgr::ms_aVehicleNames, HANDLING_NAME_LEN, NUM_HANDLING_IDS);
	printf("PREDATOR = id %d\n", predator);
	VERIF(predator == 100);
	tBoatHandlingData &p = m.BoatHandlingData[predator - BOAT_HANDLING_FALLBACK_ID];
	VERIF(eq(p.fThrustY, 0.79f) && eq(p.fThrustZ, 0.5f) && eq(p.fWaveAudioMult, 4.0f) && eq(p.vecMoveRes.x, 0.8f));
	VERIF(eq(p.vecTurnRes.z, 0.970f) && eq(p.fLook_L_R_BehindCamHeight, 4.0f));
	printf(echecs ? "%d échec(s)\n" : "tout passe\n", echecs);
	return echecs != 0;
}
