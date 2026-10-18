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

// Une liste de points (bloc POINT) : les endroits nommés d'une mission, où les scripts
// posent le joueur, créent un piéton (PedCreatePoint) ou mettent un marqueur. Un bloc
// porte un ou plusieurs points (NPOINTPOINTS), chacun avec sa position et son lacet.
//
// Dans bully.exe, FUN_006d59a0 lit un bloc : l'entrée du pool des listes (0x20c7c54) reçoit le
// nom haché (+8), l'AREACODE sur un octet (+0xc), l'emplacement de DATLoad (+0xd) et deux drapeaux
// (+0x18) tirés du préfixe du nom, SV_ (bit 0) et DT_ (bit 1). Chaque point est une entrée de
// 0x2c octets du pool 0x20c7c88 : position (+0), hachage de POINTLABEL (+0x10), modèle de
// POINTPICKUPNAME (+0x20, par FUN_0051c040), lacet (+0x1c), son (+0x14, +0x16), heures (+0x22,
// +0x23), fondus (+0x28, +0x29), masque météo (+0x27), POINTSML (+0x24), mission (+0xc : l'index
// de MISSIONNAME dans la banque, -1 si vide, -2 pour NONE ; « ! » ou « - » devant le nom posent
// les bits 2 et 3 de +0x2a), occlusion (bit 0 de +0x2a).
struct CTriggerPointPoint {
	CVector pos;                         // POINTPOINT
	float lacetTangageRoulis[3] = {0, 0, 0};   // POINTYAWPITCHROLL (degrés)
	std::string etiquette;               // POINTLABEL
	std::string ramassage;               // POINTPICKUPNAME : le modèle à ramasser (Rubband, GeekCard, brick…), vide sinon
	std::string sfx;                     // POINTSFXID
	std::string banqueSon;               // POINTSOUNDBANKID
	float volumeSfx = 1;                 // POINTSFXVOLUME
	int32 fonduEntree = 0, fonduSortie = 0;    // STREAMFADEINTIME, STREAMFADEOUTTIME
	int32 sml = 0;                       // POINTSML
	int32 heureDebut = 0, heureFin = 0;  // STARTHOUR, ENDHOUR
	std::string meteo;                   // WEATHERMASK, tel qu'écrit (« PSF », « R », « X »…)
	uint8 meteoMasque = 0x3f;            // le même en bits, comme FUN_00593f20 le range à +0x27 (MasqueMeteo)
	bool occlusion = false;              // USEOCCLUSION
	bool missionSpec = false;            // POINTMISSIONSPEC
	std::string mission;                 // MISSIONNAME, avec son « ! » ou « - » de tête s'il y en a un
	bool permanent = false;              // POINTPERMANENT
	int32 tag = 0;                       // POINTTAG
	std::string butes;                   // PICKUPBUTES
};

// FUN_00593f20 : le masque météo d'un point. Une lettre par temps, sans la casse : F → bit 0, W → bit 1,
// P → bit 2, S → bit 3, R → bit 4, X → bit 5 ; « all » ou la chaîne vide → les six (0x3f).
uint8 MasqueMeteo(const char *s);

struct CTriggerPointList {
	std::string nom;                     // POINTNAME
	int32 zone = 0;                      // AREACODE : l'octet +0xc ; PlayerSetPosPoint (FUN_005d2310) en fait la zone de
	                                     // destination du joueur, -1 voulant dire « rester dans la zone courante »
	bool sauvegarde = false;             // le nom commence par SV_ (bit 0 de +0x18)
	bool porte = false;                  // le nom commence par DT_ (bit 1 de +0x18)
	std::vector<CTriggerPointPoint> points;
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
	std::vector<CTriggerPointList> pointLists;   // les blocs POINT, dans l'ordre du fichier
	std::vector<CTriggerPerimeter> perimetres;
	std::vector<CTriggerZone> zones;     // les blocs TRIGGER, rattachés au périmètre qui les précède
	std::vector<CTriggerPoi> pois;
	int32 incoherences = 0;              // trajets dont NPATHPOINTS ne correspond pas aux points écrits

	// Analyse le texte d'un fichier ; faux si l'en-tête manque, si un bloc
	// bloc n'est pas fermé, ou si le nombre de trajets diffère de l'en-tête.
	bool Load(const uint8 *data, uint32 size);
};
