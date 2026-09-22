// Charge Config/Dat/CARCOLS.DAT avec LoadVehicleColours recréé et vérifie
// contre le fichier : 100 couleurs, 16 véhicules, valeurs connues.
//   BULLY_DATA=<racine du jeu> build/tests/test_carcols
#include "../src/vehicles/VehicleColours.h"
#include <cstdio>

int NombreDeModelesVus(void);

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CVehicleModelInfo::LoadVehicleColours();
	CRGBA *t = CVehicleModelInfo::ms_vehicleColourTable;
	VERIF(t[0].r == 5 && t[0].g == 5 && t[0].b == 5 && t[0].a == 0xff);           // black
	VERIF(t[1].r == 245 && t[1].g == 245 && t[1].b == 245);                        // white
	VERIF(t[3].r == 220 && t[3].g == 80 && t[3].b == 86);                          // cherry red
	VERIF(t[99].a == 0xff && t[100].a == 0);                                        // 100 couleurs
	VERIF(NombreDeModelesVus() == 16);
	CVehicleModelInfo *bike = FindVehicleModelInfoByName("bike");
	VERIF(bike->m_numColours == 8);
	VERIF(bike->m_colours1[0] == 13 && bike->m_colours2[0] == 13);
	VERIF(bike->m_colours1[1] == 22 && bike->m_colours2[1] == 22);
	VERIF(bike->m_colours1[7] == 51 && bike->m_colours2[7] == 51);
	printf("couleurs : %d %d %d / %d %d %d ; bike : %d paires\n", t[0].r, t[0].g, t[0].b, t[3].r, t[3].g, t[3].b, bike->m_numColours);
	printf(echecs ? "%d échec(s)\n" : "tout passe\n", echecs);
	return echecs != 0;
}
