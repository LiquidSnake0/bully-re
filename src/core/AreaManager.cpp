#include "AreaManager.h"
#include <cstring>

// FUN_00415ed0 : tout à zéro, le compte à rebours à 14 images.
CAreaManager::CAreaManager(void)
{
	memset(scripts, 0, sizeof(scripts));
	etat = AREA_REPOS;
	demandeEnAttente = false; pret = false; immediat = false; ecranNoirGarde = false; fonduNoirSignale = false;
	compteARebours = 0; attenteImages = 14;
}

// AreaRegisterAreaScript (0x414f10) : strncpy dans la case du code, seulement si le nom fait
// moins de 0x65 caractères ; rien ne borne le code dans le moteur, on borne ici à la table.
void
CAreaManager::EnregistrerScript(int32 zone, const char *script)
{
	if(script == nil || zone < 0 || zone >= AREA_MAX) return;
	size_t n = strlen(script);
	if(n >= AREA_SCRIPT_MAX) return;
	memset(scripts[zone], 0, AREA_SCRIPT_CASE);
	memcpy(scripts[zone], script, n);
}

const char *
CAreaManager::Script(int32 zone) const
{
	if(zone < 0 || zone >= AREA_MAX) return "";
	return scripts[zone];
}

// FUN_00415640 : refusé si l'état n'est pas 0, si une demande attend, ou si la seconde machine
// (+0x868, +0x86d) travaille ; sinon la demande est rangée et +0x86c posé.
bool
CAreaManager::Demander(const CVector &pos, float cap, int32 zone, const CVector *camera, bool imm)
{
	if(EnChargement()) return false;
	demande.pos = pos;
	demande.cap = cap;
	demande.zone = zone;
	demande.aCamera = camera != nil;
	if(camera) demande.camera = *camera;
	immediat = imm;
	demandeEnAttente = true;
	return true;
}

// AreaIsLoading (0x415230) : +0x864, +0x86c, +0x868 ou +0x86d non nuls.
bool
CAreaManager::EnChargement(void) const
{
	return etat != AREA_REPOS || demandeEnAttente;
}

bool
CAreaManager::Avancer(int fondu, bool scriptsDecharges, bool scriptDeZoneCharge)
{
	switch(etat){
	default:
		// Les portes (les paires déclencheur / point de +0x10) ne sont pas recréées ici : seule la
		// demande d'un script, ou d'une porte jouée par l'outil, fait partir la machine.
		if(demandeEnAttente){ etat = AREA_DEBUT; demandeEnAttente = false; return false; }
		break;
	case AREA_DEBUT:
		if(zoneVisible == demande.zone){
			etat = scriptDeZoneCharge ? AREA_MEME_ZONE : AREA_DECHARGER;    // FUN_005dbb60 : l'objet script de la zone existe
			return false;
		}
		if(!ecranNoirGarde && fondu != FONDU_NOIR){ etat = AREA_FONDU_DEMANDE; return true; }   // FUN_004f11a0() != 1
		etat = AREA_DECHARGER;
		return false;
	case AREA_FONDU_DEMANDE:
		if(!immediat){ compteARebours = attenteImages; etat = AREA_ATTENTE; return true; }
		evenements.fonduNoir = true;                  // CameraFade(DAT_020c8910 + 0x18, 1, 0)
		etat = AREA_FONDU;
		return true;
	case AREA_FONDU:
		if(fondu == FONDU_NOIR){
			if(!fonduNoirSignale){ fonduNoirSignale = true; return true; }   // FUN_0055e700(), une image
			etat = AREA_DECHARGER; fonduNoirSignale = false;
			return false;
		}
		break;
	case AREA_ATTENTE:
		if(--compteARebours < 1){ evenements.fonduNoir = true; etat = AREA_FONDU; return true; }   // CameraFade(0, 1, 1)
		break;
	case AREA_DECHARGER:
		immediat = false; attenteImages = 14;
		evenements.decharger = true;                  // FUN_005dbb20
		etat = AREA_CHARGER;
		return false;
	case AREA_CHARGER:
		if(scriptsDecharges){                         // FUN_005dbae0 == 0
			evenements.charger = Script(demande.zone);   // FUN_005dc020(FUN_00414f00(&zone), 1)
			etat = AREA_PRET;
			return false;
		}
		break;
	case AREA_MEME_ZONE:
		etat = AREA_ARRIVEE_MEME_ZONE;
		return false;
	case AREA_PRET:
		if(pret){ pret = false; etat = AREA_ARRIVEE; return false; }
		break;
	case AREA_ARRIVEE:
		zoneVisible = demande.zone;                   // FUN_00415750
		evenements.zoneChangee = true;
		evenements.joueurPose = true;                 // FUN_00416ae0 puis FUN_004162f0
		etat = ecranNoirGarde ? AREA_FIN : AREA_FONDU_RETOUR;
		return false;
	case AREA_ARRIVEE_MEME_ZONE:
		evenements.joueurPose = true;
		etat = AREA_FIN;
		return false;
	case AREA_FONDU_RETOUR:
		// Le moteur attend ici un délai (+0x850, constante non lue) ou que le secteur du joueur soit
		// chargé ; sans diffusion, on rend l'écran tout de suite.
		evenements.fonduRetour = true;                // CameraFade(DAT_020c8910 + 0x1c, 0, 0)
		etat = AREA_FONDU_RETOUR_FINI;
		return false;
	case AREA_FONDU_RETOUR_FINI:
		// Le moteur regarde la progression du fondu (DAT_00c3cd28 contre DAT_020c8910 + 0x24) ou DAT_00a45a4f.
		if(fondu != FONDU_EN_COURS){ etat = AREA_FIN; return false; }
		break;
	case AREA_FIN:
		demandeEnAttente = false;
		pret = false;
		etat = AREA_REPOS;
		return false;
	}
	return true;
}

void
CAreaManager::AvancerImage(int fondu, bool scriptsDecharges, bool scriptDeZoneCharge)
{
	evenements = Evenements();
	// Garde-fou : la machine ne boucle jamais plus de quelques états dans une image.
	for(int i = 0; i < 16; i++)
		if(Avancer(fondu, scriptsDecharges, scriptDeZoneCharge)) break;
}
