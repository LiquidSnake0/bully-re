// Les fichiers de déclencheurs de DAT/Trigger.img (docs/trigger.md) : du texte,
// un en-tête de comptes (NPATHS, NPOINTS, NPERIMETERS, NTRIGGERS, NPOIS) puis
// des blocs PATH / POINT / PERIMETER / TRIGGER / POI entre BEGIN et END.
//
// On lit ici les trajets (PATH) : ce sont eux que suivent les patrouilles
// (préfets, concierges, élèves des missions). 410 fichiers, 1 543 trajets,
// 8 913 points. Un point porte une position, une attente et des actions ; la
// seule action rencontrée (type 0) oriente le piéton et le fait attendre.
#pragma once
#include "../common.h"
#include <string>
#include <vector>

struct CTriggerAction {
	int32 type = 0;
	float orientation[3] = {0, 0, 0};    // lacet, tangage, roulis (degrés)
	float attente = 0;                   // secondes
};

struct CTriggerPathPoint {
	CVector pos;
	float lacetTangageRoulis[3] = {0, 0, 0};
	float attente = 0;
	int32 unid = 0;
	std::vector<CTriggerAction> actions;
};

struct CTriggerPath {
	std::string nom;
	int32 zone = 0;                      // AREACODE
	int32 patrouille = 0;                // PATROLTYPE
	std::vector<CTriggerPathPoint> points;
};

struct CTriggerFile {
	int32 nPaths = 0, nPoints = 0, nPerimeters = 0, nTriggers = 0, nPois = 0;   // l'en-tête
	std::vector<CTriggerPath> paths;
	int32 incoherences = 0;              // trajets dont NPATHPOINTS ne correspond pas aux points écrits

	// Analyse le texte d'un fichier ; faux si l'en-tête manque, si un bloc
	// PATH n'est pas fermé, ou si le nombre de trajets diffère de l'en-tête.
	bool Load(const uint8 *data, uint32 size);
};
