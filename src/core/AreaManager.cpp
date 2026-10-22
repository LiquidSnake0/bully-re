#include "AreaManager.h"
#include <cmath>
#include <cstring>
#include <vector>

static const float RECUL_PORTE = 2.5f;   // DAT_00900ab0 (un double)

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

// FUN_0046d960 / FUN_0046d880 : les bornes remises dans l'ordre (min, max) puis p >= min && p <= max sur
// chaque axe ; le moteur compare au point de l'entité (sa matrice si elle en a une, sinon +4).
bool
PaveContient(const CVector &p, const CVector &c, float rx, float ry, float rz, bool troisD)
{
	float x0 = c.x - rx, x1 = c.x + rx, y0 = c.y - ry, y1 = c.y + ry, z0 = c.z - rz, z1 = c.z + rz;
	if(x1 < x0){ float t = x0; x0 = x1; x1 = t; }
	if(y1 < y0){ float t = y0; y0 = y1; y1 = t; }
	if(z1 < z0){ float t = z0; z0 = z1; z1 = t; }
	if(p.x < x0 || p.x > x1 || p.y < y0 || p.y > y1) return false;
	if(troisD && (p.z < z0 || p.z > z1)) return false;
	return true;
}

void
CAreaManager::ReconstruirePortes(const CDatPools &d)
{
	portes.clear();
	for(int l = 0; l < d.NombreListes(); l++){
		const CTriggerPointList *liste = d.Liste(l);
		if(liste == nil) continue;
		uint32 h = d.HachageListe(l);
		if(!liste->sauvegarde && !liste->porte) continue;        // ni SV_ (bit 0 de +0x18) ni DT_ (bit 1)
		for(int z = 0; z < d.NombreZones(); z++){
			if(d.ZoneParHandle(z) == nil || d.HachageZone(z) != h) continue;
			CAreaPorte p; p.declencheur = z; p.liste = l; p.parObjet = !liste->sauvegarde;
			portes.push_back(p);
			if(liste->sauvegarde) break;                         // une liste SV_ : le premier déclencheur seulement
		}
	}
}

int
CAreaManager::PasserUnePorte(const CDatPools &d, const CVector &pos, float cap)
{
	if(EnChargement()) return -1;
	for(size_t i = 0; i < portes.size(); i++){
		const CAreaPorte &p = portes[i];
		const CTriggerPointList *liste = d.Liste(p.liste);
		if(liste == nil || liste->points.empty()) continue;
		if(!p.parObjet){
			if(!d.DansDeclencheur(p.declencheur, pos)) continue;
		}else{
			if(p.declencheur != porteObjet) continue;
			if(retourEnReserve){
				// La sortie d'une zone où l'on est entré par une porte : la réserve, pas la liste.
				Demander(retour.pos, retour.cap, retour.zone, nil, false);
				retourEnReserve = false;
				return (int)i;
			}
			if(ZoneAvecRetour(liste->zone)){
				// FUN_00416750, sans objet de porte (+0x1c du déclencheur nul, le cas de l'outil) : le joueur recule
				// de 2,5 m (DAT_00900ab0) le long de son regard (FUN_00415350 : (-sin cap, cos cap, 0) sans matrice,
				// normalisé par FUN_00414cc0) et repart dos à la porte, cap moins π (DAT_00900160). Avec un objet,
				// le moteur part du centre de sa boîte (non joué ici).
				const float rad = cap * 3.14159265f / 180.0f;
				retour.pos = CVector(pos.x + sinf(rad) * RECUL_PORTE, pos.y - cosf(rad) * RECUL_PORTE, pos.z);
				retour.cap = fmodf(cap + 180.0f, 360.0f);
				retour.zone = zoneVisible;
				retourEnReserve = true;
			}
		}
		const CTriggerPointPoint &pt = liste->points[0];
		const CVector *camera = liste->points.size() > 1 ? &liste->points[1].pos : nil;
		Demander(pt.pos, pt.lacetTangageRoulis[0], liste->zone, camera, false);
		return (int)i;
	}
	return -1;
}

bool
CAreaManager::ZoneAvecRetour(int32 zone)
{
	switch(zone){
	case 0xf: case 0x1a: case 0x1c: case 0x1d: case 0x21: case 0x22: case 0x27: case 0x2c: case 0x2e: return true;
	}
	return false;
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
		porteObjet = -1;                              // +0x394
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
