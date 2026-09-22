// Hôte pour test_handling : les conversions en unités du jeu sont
// neutralisées pour vérifier les valeurs brutes lues dans le fichier.
#include "../../src/vehicles/HandlingMgr.h"

void cHandlingDataMgr::ConvertDataToGameUnits(tHandlingData *) {}
void cHandlingDataMgr::ConvertBikeDataToGameUnits(tBikeHandlingData *) {}

extern const float HANDLING_ACCEL_SCALE = 1.0f;   // 0x900de8 dans le jeu
