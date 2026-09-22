// Charge Config/Dat/PEDSTATS.DAT avec le CPedStats recréé et vérifie
// quelques valeurs lues dans le fichier lui-même (STAT_PLAYER : pickup
// none, drop 20, yaw 100, range 12, health 200 ; dernière colonne 15).
//   BULLY_DATA=<racine du jeu> build/tests/test_pedstats
#include "../src/peds/PedStats.h"
#include <cstdio>
#include <cstring>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

int
main(void)
{
	CPedStats::Initialise();
	printf("%d stats chargés\n", CPedStats::ms_numPedStats);
	VERIF(CPedStats::ms_numPedStats == 75);
	CPedStat *p = &CPedStats::ms_apPedStats[0];
	printf("premier : %s\n", p->m_name);
	VERIF(strcmp(p->m_name, "STAT_PLAYER") == 0);
	VERIF(p->m_fields[PEDSTAT_PICKUP] == -1);        // none
	VERIF(p->m_fields[1] == 20);                     // C  drop likelihood
	VERIF(p->m_fields[2] == 100);                    // D  vision yaw
	VERIF(p->m_fields[3] == 12);                     // E  vision range
	VERIF(p->m_fields[4] == 200);                    // F  max health
	VERIF(p->m_fields[PEDSTAT_WEAPON1_TYPE] == 0);   // unarmed
	VERIF(p->m_fields[PEDSTAT_WEAPON1_MISSION] == -1); // init
	VERIF(p->m_fields[PEDSTAT_NUM_FIELDS - 1] == 15);  // BM humiliation
	CPedStat *e = &CPedStats::ms_apPedStats[2];
	VERIF(strcmp(e->m_name, "STAT_N_EARNEST") == 0);
	VERIF(e->m_fields[2] == 360);                    // yaw 360
	VERIF(e->m_fields[PEDSTAT_NUM_FIELDS - 1] == 0);
	for(int i = 0; i < CPedStats::ms_numPedStats; i++)
		VERIF(CPedStats::ms_apPedStats[i].m_name[0] != '\0');
	printf(echecs ? "%d échec(s)\n" : "tout passe\n", echecs);
	return echecs != 0;
}
