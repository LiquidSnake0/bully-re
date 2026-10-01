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

// Un périmètre : un polygone horizontal, ses points relatifs à la position du
// déclencheur qui le suit (et tournés de son lacet), plus une hauteur.
struct CTriggerPerimeter {
	std::string placement;
	bool porte = false;                  // ISDOOR
	float hauteur = 0, profondeur = 0;   // HEIGHT, DEPTH
	std::vector<float> x, y;
};

// Les catégories de piétons des tableaux de population, dans l'ordre du fichier.
enum { POP_PREFECT, POP_NERD, POP_JOCK, POP_DROPOUT, POP_GREASER, POP_PREPPY, POP_STUDENT,
       POP_COP, POP_TEACHER, POP_TOWNPERSON, POP_SHOPKEEP, POP_BULLY, POP_NUM };
// Les moments de la journée : DAY, CLASS, NIGHT, CURFEW.
enum { MOMENT_JOUR, MOMENT_CLASSE, MOMENT_NUIT, MOMENT_COUVREFEU, MOMENT_NUM };
extern const char *const kPopCategorie[POP_NUM];     // « PREFECT », « NERD »…
extern const char *const kMoment[MOMENT_NUM];        // « DAY », « CLASS »…

struct CTriggerZone {
	std::string nom;                     // TRIGGERNAME
	int32 zone = 0;                      // AREACODE
	CVector pos;                         // POSITION
	float lacet = 0;                     // YAW (degrés)
	float zHauteur = 0;                  // ZHEIGHT
	bool mission = false;                // ISMISSIONSPECIFIC
	int32 perimetre = -1;                // index dans `perimetres`, -1 si aucun
	bool aPopulation = false;
	int32 total[MOMENT_NUM] = {0};
	int32 population[MOMENT_NUM][POP_NUM] = {{0}};
	bool aVehicules = false;
	int32 vehicules[MOMENT_NUM][4] = {{0}};   // TOTAL, CAR, BIKE, POLICECAR

	// Vrai si (x, y, z) est dans la zone : dans le polygone du périmètre
	// (ramené au repère du déclencheur) et entre pos.z et pos.z + zHauteur.
	bool Contient(const std::vector<CTriggerPerimeter> &perimetres, float x, float y, float z) const;
};

// Un point d'intérêt (bloc POI) : un endroit où un piéton vient faire quelque
// chose (traîner, s'asseoir, s'adosser au mur, fumer, se battre…), pour certaines
// périodes de la journée. Les périodes sont celles de timeCycl.dat, avec la
// classe en deux (EARLYCLASS, LATECLASS) et le couvre-feu en paliers de fatigue.
enum { POI_MORNING, POI_EARLYCLASS, POI_LUNCH, POI_LATECLASS, POI_AFTERNOON, POI_EVENING,
       POI_SLIGHTLYTIRED, POI_TIRED, POI_MORETIRED, POI_TOOTIRED, POI_NUM_PERIODES };
extern const char *const kPoiPeriode[POI_NUM_PERIODES];   // « MORNING »…

struct CTriggerPoiPoint {
	std::string nom;                     // le comportement (« F_ClassSmokers »), souvent vide
	std::string type;                    // Hang_Out, Sitting_Spot, Wall, Couple, Specific_Event…
	std::string genre = "Both";          // Both, Male, Female
	std::string clique = "DEFAULT";      // PEDTYPE : DEFAULT ou une catégorie (GREASER…)
	int32 max = 0;
	CVector pos;
	float lacetTangageRoulis[3] = {0, 0, 0};
	bool periodes[POI_NUM_PERIODES] = {false};
	bool utiliseRayon = false; float rayon = 0;
	bool ignorePopulation = false; int32 limite = 0;    // OVERRIDELIMIT
};

struct CTriggerPoi {
	std::string nom;
	int32 zone = 0;
	std::vector<CTriggerPoiPoint> points;
};

struct CTriggerFile {
	int32 nPaths = 0, nPoints = 0, nPerimeters = 0, nTriggers = 0, nPois = 0;   // l'en-tête
	std::vector<CTriggerPath> paths;
	std::vector<CTriggerPerimeter> perimetres;
	std::vector<CTriggerZone> zones;     // les blocs TRIGGER, rattachés au périmètre qui les précède
	std::vector<CTriggerPoi> pois;
	int32 incoherences = 0;              // trajets dont NPATHPOINTS ne correspond pas aux points écrits

	// Analyse le texte d'un fichier ; faux si l'en-tête manque, si un bloc
	// bloc n'est pas fermé, ou si le nombre de trajets diffère de l'en-tête.
	bool Load(const uint8 *data, uint32 size);
};
