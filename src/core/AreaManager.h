// Le gestionnaire de zones de bully.exe : l'objet pointé par PTR_DAT_009e4f4c (docs/zones.md).
// Il tient la table des scripts de zone (AreaRegisterAreaScript, 0x414f10, dans DAT_009e3440),
// la zone visible (DAT_00bd1008, ce que AreaGetVisible répond) et le changement de zone du
// joueur : une demande (FUN_00415640 : position, cap, zone, caméra), puis la machine à états de
// FUN_00416e30, une image à la fois, qui fond l'écran au noir, retire les scripts de zone, charge
// celui de la nouvelle zone, et ne pose la zone visible (FUN_00415750) et le joueur (FUN_004162f0)
// qu'à l'état 9. Tout déplacement du joueur par un script passe par là : PlayerSetPosPoint
// (FUN_005d2310, zone = l'AREACODE du point), PlayerSetPosPointArea (FUN_005d2410, zone donnée),
// PlayerSetPosXYZArea, et même PlayerSetPosXYZ (0x415fb0, zone = la zone visible). C'est la
// différence entre l'AREACODE d'un point (où le joueur ira) et la zone visible (où il est).
#pragma once
#include "../common.h"

enum { AREA_MAX = 64, AREA_SCRIPT_CASE = 0x6c, AREA_SCRIPT_MAX = 0x64 };   // DAT_009e3440 : une case de 0x6c octets par code

// Les états de +0x864, dans l'ordre où FUN_00416e30 les traverse.
enum eAreaEtat {
	AREA_REPOS = 0,              // rien en cours ; une demande (+0x86c) fait passer à 1
	AREA_DEBUT = 1,              // même zone → 7 si son script est chargé, sinon 5 ; autre zone → 2, ou 5 si l'écran est déjà noir (FUN_004f11a0 == 1)
	AREA_FONDU_DEMANDE = 2,      // CameraFade(durée par défaut, vers le noir) → 3 ; sans l'octet +0x8dc : compte à rebours de 14 images (4)
	AREA_FONDU = 3,              // attend la fin du fondu (FUN_004f11a0 == 1)
	AREA_ATTENTE = 4,            // compte à rebours +0x8ec, puis fondu immédiat (CameraFade(0, vers le noir)) → 3
	AREA_DECHARGER = 5,          // FUN_005dbb20 : les scripts de zone sont marqués à retirer → 6
	AREA_CHARGER = 6,            // quand FUN_005dbae0 n'en voit plus : FUN_005dc020(script de la zone, 1) → 8
	AREA_MEME_ZONE = 7,          // → 10
	AREA_PRET = 8,               // attend +0x870 → 9
	AREA_ARRIVEE = 9,            // FUN_00415750 (zone visible), FUN_00416ae0 (le monde vidé), FUN_004162f0 (le joueur posé) → 0xb
	AREA_ARRIVEE_MEME_ZONE = 10, // le joueur posé sans changer de zone → 0xd
	AREA_FONDU_RETOUR = 0xb,     // après un délai : CameraFade(durée, retour) → 0xc
	AREA_FONDU_RETOUR_FINI = 0xc,// le fondu rendu → 0xd
	AREA_FIN = 0xd               // drapeaux remis à zéro → 0
};

// Ce que FUN_004f11a0 répond sur le fondu de la caméra : l'écran est rendu, noir, ou en train de changer.
enum eFondu { FONDU_RENDU = 0, FONDU_NOIR = 1, FONDU_EN_COURS = 2 };

// Ce que FUN_00415640 range dans le gestionnaire.
struct CAreaDemande {
	CVector pos;                 // +0x878
	float cap = 0;               // +0x884 (degrés)
	int32 zone = 0;              // +0x888
	bool aCamera = false;        // +0x88c
	CVector camera;              // +0x890
};

class CAreaManager
{
public:
	CAreaManager(void);          // FUN_00415ed0

	// AreaRegisterAreaScript (0x414f10) : le script d'une zone, copié dans sa case ; un nom de
	// 100 caractères ou plus est ignoré, un code hors table aussi.
	void EnregistrerScript(int32 zone, const char *script);
	// FUN_00414f00 : la case de la table (chaîne vide si rien n'est enregistré).
	const char *Script(int32 zone) const;

	// FUN_00415640 : une demande de changement, ignorée tant qu'un changement est en cours
	// (AreaIsLoading). `immediat` est l'octet +0x8dc que les fonctions Lua posent avant d'appeler :
	// le fondu part tout de suite au lieu d'attendre 14 images.
	bool Demander(const CVector &pos, float cap, int32 zone, const CVector *camera = nil, bool immediat = true);
	// AreaIsLoading (0x415230) : un changement est demandé ou en cours.
	bool EnChargement(void) const;
	// +0x870 : le script de la nouvelle zone est prêt ; l'état 8 l'attend.
	void SignalerPret(void){ pret = true; }

	// Ce que la machine demande au monde pendant une image, lu par celui qui la fait avancer.
	struct Evenements {
		bool fonduNoir = false;      // CameraFade(…, 1) : l'écran va au noir (durée par défaut, ou 0 après le compte à rebours)
		bool fonduRetour = false;    // CameraFade(…, 0) : l'écran revient
		bool decharger = false;      // FUN_005dbb20 : les scripts de zone sont à retirer
		const char *charger = nil;   // FUN_005dc020(…, 1) : le script de zone à charger, avec son main
		bool joueurPose = false;     // FUN_004162f0 : le joueur est en `demande.pos`, cap `demande.cap`
		bool zoneChangee = false;    // FUN_00415750 vient de poser la zone visible
	} evenements;
	// Une image de FUN_00416e30. `fondu` : la réponse de FUN_004f11a0 (eFondu) ; `scriptsDecharges` :
	// FUN_005dbae0 ne voit plus de script de zone ; `scriptDeZoneCharge` : FUN_005dbb60 trouve
	// l'objet script de la zone demandée. Comme dans le moteur (FUN_004176d0), la fonction est
	// rappelée tant qu'elle renvoie faux : les états qui s'enchaînent dans la même image le font ici.
	bool Avancer(int fondu, bool scriptsDecharges, bool scriptDeZoneCharge);
	// FUN_004176d0 : l'image entière, avec ses enchaînements ; les événements s'accumulent.
	void AvancerImage(int fondu, bool scriptsDecharges, bool scriptDeZoneCharge);

	int32 zoneVisible = 0;           // DAT_00bd1008
	int32 etat = AREA_REPOS;         // +0x864
	bool demandeEnAttente = false;   // +0x86c
	bool pret = false;               // +0x870
	bool immediat = false;           // +0x8dc
	bool ecranNoirGarde = false;     // +0x85f : finir sans rendre l'écran (0 à l'initialisation)
	bool fonduNoirSignale = false;   // +0x85e : la première image où le fondu est fini sert à FUN_0055e700
	int32 compteARebours = 0;        // +0x8ec
	int32 attenteImages = 14;        // +0x8e0
	CAreaDemande demande;
private:
	char scripts[AREA_MAX][AREA_SCRIPT_CASE];
};
