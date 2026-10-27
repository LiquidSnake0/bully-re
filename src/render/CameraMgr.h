// Le gestionnaire de caméras de bully.exe : l'objet en 0xc3cc68 (ecx de CameraSetActive), dont le mode courant (+0x90,
// DAT_00c3ccf8) est ce que CameraGetActive (0x5b9bb0) rend et ce que la condition CameraActive compare. L'initialisation
// (FUN_004f35d0) pose le mode courant à 1 (la caméra du joueur) et la demande (+0x270) à 1. Une demande (+0x270 le mode,
// +0x274, l'octet +0x278, +0x27c, +0x280) est rangée par FUN_004f2ad0 (CameraSetActive 0x5b9b30, CameraReturnToPlayer
// 0x5b8fd0 : le mode 1) ou par FUN_004f3440 (seulement si le mode diffère) ; elle n'est appliquée que par FUN_004f1740 :
// la piste CameraSetDesiredActive (0x935f14, lancement FUN_0065dd40, sur la demande 0xc3ced8) ou SetCameraTrack
// (FUN_00660a30). Appliquée, le mode d'avant passe en +0x94, la demande en +0x90, et la demande est effacée (0).
// L'écran large : DAT_00c3cd0c, que CameraSetWidescreen (0x5b8f20) pose (et sa transition DAT_00c3cd14 à 0 ; avec un
// second argument faux, DAT_00c3cd10 aussi, sans transition).
#pragma once
#include "../common.h"

enum { CAMERA_JOUEUR = 1 };

struct CCameraDemande { int32 mode = CAMERA_JOUEUR; int32 duree = 0; bool octet = false; };

class CCameraMgr
{
public:
	int32 courant = CAMERA_JOUEUR;   // +0x90
	int32 precedent = 0;             // +0x94
	CCameraDemande demande;          // +0x270
	bool largeEcran = false;         // DAT_00c3cd0c
	bool largeEcranAtteint = false;  // DAT_00c3cd10
	float transition = 0.0f;         // DAT_00c3cd14
	// FUN_004f2ad0 : la demande rangée (le moteur relance aussi FUN_004f2240, les entrées de la caméra).
	void Demander(int32 mode, int32 duree, bool octet);
	// FUN_004f3440 : une demande seulement si le mode diffère du courant (l'octet à 1).
	void DemanderSiAutre(int32 mode);
	// FUN_004f1740, quand une piste de caméra la lit : une demande du mode courant ne change rien ; sinon le mode passe.
	// Dans les deux cas la demande est effacée. Supposé : la caméra suit quelqu'un (+0x1a4), sinon le moteur l'efface
	// sans rien changer.
	void Appliquer(void);
	// CameraSetWidescreen(oui, progressif) : `immediat` quand le second argument est donné et faux.
	void LargeEcran(bool oui, bool immediat);
};

// Les conditions CameraActive (ConditionCameraActive, table 0x936768, FUN_0065aea0 : le mode courant est +4) et
// CameraInWidescreen (0x93678c, FUN_0065afc0 : DAT_00c3cd0c non nul).
bool CameraActive(const CCameraMgr &c, int32 mode);
bool CameraInWidescreen(const CCameraMgr &c);
