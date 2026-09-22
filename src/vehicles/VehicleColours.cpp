// CVehicleModelInfo::LoadVehicleColours, 0x005355b0, recréé depuis bully.exe.
// Même fichier et même logique que Vice City : une section « col » de
// couleurs RGB, une section « car » qui donne à chaque modèle jusqu'à huit
// paires de couleurs. Différences : la table fait 256 entrées RGBA en
// 0x00ce6d28, la ligne « car » accepte 16 entiers, et la lecture passe par
// un objet fichier (0x4264c0 / 0x42d4b0) plutôt que par work_buff.
#include "VehicleColours.h"
#include <cstdio>
#include <cstring>

CRGBA CVehicleModelInfo::ms_vehicleColourTable[NUM_VEHICLE_COLOURS];

void
CVehicleModelInfo::LoadVehicleColours(void)
{
	char line[1024];
	char name[64];
	int32 r, g, b;
	int32 pairs[16];
	int section = 0;
	int numColours = 0;

	memset(ms_vehicleColourTable, 0, sizeof(ms_vehicleColourTable));   // 0x100 mots à zéro

	CFileStream file;
	file.Open("Config\\Dat\\CARCOLS.DAT");                  // 0x4264c0
	while(file.ReadLine(line, sizeof(line))){                // 0x42d4b0
		// blancs de tête, puis virgules et retours chariot → espaces, coupe au '\n'
		int i = 0;
		while(line[i] < '!' && line[i] != '\0' && line[i] != '\n') i++;
		int j = i;
		while(line[j] != '\n' && line[j] != '\0'){
			if(line[j] == ',' || line[j] == '\r') line[j] = ' ';
			j++;
		}
		line[j] = '\0';
		char *p = line + i;
		if(*p == '#' || *p == '\0') continue;

		if(section == 0){
			if(p[0] == 'c' && p[1] == 'o' && p[2] == 'l') section = 1;
			else if(p[0] == 'c' && p[1] == 'a' && p[2] == 'r') section = 2;
		}else if(p[0] == 'e' && p[1] == 'n' && p[2] == 'd'){
			section = 0;
		}else if(section == 1){
			sscanf(line, "%d %d %d", &r, &g, &b);
			if(numColours < NUM_VEHICLE_COLOURS){
				ms_vehicleColourTable[numColours].r = (uint8)r;
				ms_vehicleColourTable[numColours].g = (uint8)g;
				ms_vehicleColourTable[numColours].b = (uint8)b;
				ms_vehicleColourTable[numColours].a = 0xff;
				numColours++;
			}
		}else if(section == 2){
			int n = sscanf(line, "%s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", name,
				&pairs[0], &pairs[1], &pairs[2], &pairs[3], &pairs[4], &pairs[5], &pairs[6], &pairs[7],
				&pairs[8], &pairs[9], &pairs[10], &pairs[11], &pairs[12], &pairs[13], &pairs[14], &pairs[15]);
			CVehicleModelInfo *mi = FindVehicleModelInfoByName(name);   // 0x51c0b0
			if(mi == nil) continue;
			mi->m_numColours = (uint8)((n - 1) / 2);                  // +0x1bc
			for(int k = 0; k < mi->m_numColours; k++){
				mi->m_colours1[k] = (uint8)pairs[2 * k];                // +0x1ac
				mi->m_colours2[k] = (uint8)pairs[2 * k + 1];            // +0x1b4
			}
		}
	}
	file.Close();
}
