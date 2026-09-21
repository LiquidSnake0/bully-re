// CSurfaceTable::Initialise, 0x0045b250. Le corps décompilé est celui de
// reVC à l'identique : lecture du fichier dans work_buff (0x1c000 octets),
// puis pour chaque ligne un nom de surface et une matrice triangulaire de
// limites d'adhérence, symétrisée. Un '-' vaut 0, ';' ouvre un commentaire.
#include "SurfaceTable.h"
#include "FileMgr.h"
#include <cstdio>

float CSurfaceTable::ms_aAdhesiveLimitTable[NUMADHESIVEGROUPS][NUMADHESIVEGROUPS];

void
CSurfaceTable::Initialise(const char *filename)
{
	int lineno, fieldno;
	char *line;
	char surfname[256];
	float adhesiveLimit;

	CFileMgr::LoadFile(filename, work_buff, sizeof(work_buff), "r");   // 0x42d030

	line = (char*)work_buff;
	for(lineno = 0; lineno < NUMADHESIVEGROUPS; lineno++){
		// blancs et commentaires
		while(*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r' || *line == ';'){
			if(*line == ';'){
				while(*line != '\n' && *line != '\r')
					line++;
			}else
				line++;
		}

		sscanf(line, "%s", surfname);
		while(!(*line == ' ' || *line == '\t' || *line == ','))
			line++;

		for(fieldno = 0; fieldno <= lineno; fieldno++){
			while(*line == ' ' || *line == '\t' || *line == ',')
				line++;
			adhesiveLimit = 0.0f;
			if(*line != '-')
				sscanf(line, "%f", &adhesiveLimit);
			while(!(*line == ' ' || *line == '\t' || *line == ',' || *line == '\n'))
				line++;

			ms_aAdhesiveLimitTable[lineno][fieldno] = adhesiveLimit;
			ms_aAdhesiveLimitTable[fieldno][lineno] = adhesiveLimit;
		}
	}
}
