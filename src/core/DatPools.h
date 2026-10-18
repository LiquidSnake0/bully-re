// Les réserves de données de mission du moteur : ce que DATLoad("1_01.DAT") verse dans quatre
// pools depuis DAT/Trigger.img, et que les scripts retrouvent par nom à travers les tables
// POINTLIST, PATH et TRIGGER (docs/trigger.md, « Les listes de points »).
//
// Dans bully.exe : POINTLIST.x appelle FUN_006d6270 (pool 0x20c7c54, nom haché à +8, indices
// à partir de 1), PATH.x FUN_005ed1b0 (pool 0x20c7c50, +8, à partir de 1), TRIGGER.x FUN_006d78c0
// (pool 0x20c7c48, +0x2c, à partir de 0) ; un « _ » en tête du nom est ignoré ; -1 si absent.
// Les points d'une liste sont chaînés dans le pool 0x20c7c88 (premier à +4, suivant à +6) :
// PedCreatePoint, PlayerSetPosPoint et GetPointFromPointList prennent le n-ième (1 par défaut).
// Les points d'intérêt (blocs POI) ont deux pools : les groupes (0x20c7c58, FUN_00447bd0 les lit :
// nom haché à +4, AREACODE à +0xc, premier point à +0x16) et les points (0x20c7c84, un CStaticPOI de
// 0xa4 octets, nom haché à +0x84). POI.x appelle FUN_00448e30 : le nom sans son premier caractère,
// les groupes parcourus du dernier au premier ; un groupe de ce nom rend son premier point, sinon le
// premier point de ce nom dans le groupe ; le handle est l'index dans le pool des points, -1 sinon.
#pragma once
#include "../common.h"
#include "TriggerFile.h"
#include <string>
#include <vector>

class CDatPools
{
public:
	// DATLoad(fichier, emplacement) : lit « 1_01.DAT » (sans la casse) dans DAT\Trigger.img et range
	// ses listes de points, ses trajets et ses déclencheurs sous cet emplacement. Les scripts
	// chargent le monde à l'emplacement 1 (Global, Population, AreaTran…), les portes et tags à 5,
	// une mission à 2 ; DATInit() suit chaque DATLoad. Faux si le fichier manque ou ne se lit pas.
	bool Charger(const char *fichier, int emplacement = 1);
	// DATUnload(emplacement) : libère tout ce qui a été chargé sous cet emplacement (une mission
	// finit par DATUnload(2)) ; les handles des autres emplacements ne bougent pas.
	void Decharger(int emplacement);
	// Tout libérer.
	void Vider(void);

	// Les trois recherches par nom (un « _ » en tête est ignoré) : le handle, ou -1.
	int ListePoints(const char *nom) const;
	int Trajet(const char *nom) const;
	int Declencheur(const char *nom) const;

	// POI.nom (FUN_00448e30) : l'index d'un point d'intérêt dans le pool des points, ou -1.
	int Poi(const char *nom) const;
	// Par handle : nil si le handle est libre ou hors du pool.
	const CTriggerPoiPoint *PoiPoint(int handle) const;
	const CTriggerPoi *PoiGroupe(int handle) const;     // le groupe du point
	const CTriggerPointList *Liste(int handle) const;
	const CTriggerPath *TrajetParHandle(int handle) const;
	const CTriggerZone *ZoneParHandle(int handle) const;
	// Le n-ième point (à partir de 1) d'une liste, ou nil.
	const CTriggerPointPoint *Point(int handle, int n) const;
	// PlayerIsInTrigger / PedIsInTrigger (FUN_006d6e50) : la position est dans la zone du déclencheur.
	bool DansDeclencheur(int handle, const CVector &pos) const;

	int fichiersCharges = 0;
private:
	template<class T> struct Entree { std::string fichier; int emplacement = 0; uint32 hachage = 0; bool libre = true; T donnee; };
	std::vector<Entree<CTriggerPointList>> listes;   // [0] réservé, comme dans le moteur
	std::vector<Entree<CTriggerPath>> trajets;       // [0] réservé
	std::vector<Entree<CTriggerZone>> zones;         // à partir de 0
	struct PoiRef { int groupe = -1; int indice = 0; };
	std::vector<Entree<CTriggerPoi>> poiGroupes;     // à partir de 0 ; les points sont dedans
	std::vector<Entree<PoiRef>> poiPoints;           // à partir de 0 : le handle que POI.x rend
	std::vector<CTriggerPerimeter> perimetres;       // les polygones des zones, par fichier
	template<class T> static int Placer(std::vector<Entree<T>> &pool, const std::string &fichier, int emplacement, uint32 h, T &&d, int premier);
	template<class T> static int Chercher(const std::vector<Entree<T>> &pool, uint32 h, int premier);
	template<class T> static void Liberer(std::vector<Entree<T>> &pool, int emplacement);
};
