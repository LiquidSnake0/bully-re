// Le déplacement d'un piéton par ses animations : la vitesse que CPed tire du déplacement que son lecteur d'animations
// rend à chaque image (FUN_00473260) et l'application de la vitesse (CPed::ApplyMoveSpeed, FUN_0047d510, la méthode 37 de
// la table de CPed, celle de CPhysical étant FUN_00469760, CVolProjectile::Deplacer dans src/objects/Projectiles.h).
//
// Le lecteur d'animations d'un piéton est en [piéton+0x114] (pas [piéton+0x18] : c'est le RwObject). SetModelIndex
// (FUN_004788d0, lu de 0x478943 à 0x47898c) lui donne deux adresses dans le piéton : [lecteur+0xa4] = piéton+0x1720 (le
// déplacement de l'image) et [lecteur+0xa8] = piéton+0x172c (la rotation de l'image, un quaternion). L'évaluation de la
// pose (006be730) et l'évaluation sans pose (FUN_006bd150) y écrivent ce qu'elles mêlent (src/anim/AnimTrack.h,
// CLecteurAnim::DeplacementDesRangs et EvaluerSansPose). Le CCutsceneObject fait de même vers son +0x124 (004cedd0, sa méthode 8, lue).
#pragma once
#include "../common.h"
#include "../objects/Projectiles.h"
#include <cmath>

struct CDeplacementPieton {
	CVector pos;                      // +0x4 (ou la translation de la matrice +0x14 quand le piéton en a une)
	float capEntite = 0.0f;           // +0x10 : le cap de l'entité, que la matrice suit (FUN_00412ad0)
	CVector vitesse;                  // +0x124 : la vitesse (m par pas de CTimer)
	float echelle = 1.0f;             // +0x154 : l'échelle du pas de CPhysical
	float cap = 0.0f;                 // +0x1700 : le cap voulu, en radians ; l'avant du piéton est (−sin, cos, 0)
	float vitesseZAvant = 0.0f;       // +0x1704 : lu par la branche du bit 2 de +0x1bc (non suivi)
	CVector vitesseAnim;              // +0x1708 : la vitesse tirée des animations (FUN_00473260)
	CVector deplAnim;                 // +0x1720 : [lecteur+0xa4], le déplacement de l'image dans le repère du piéton
	float rotAnim[4] = {0.0f, 0.0f, 0.0f, 1.0f};   // +0x172c : [lecteur+0xa8], la rotation de l'image (x, y, z, w)
	bool capFige = false;             // +0x1324 non nul : le cap +0x10 ne suit pas +0x1700
	bool capFige2 = false;            // l'octet +0x1c98 : de même
	bool parAnimations = true;        // +0x1334 : la vitesse en x et y (et z) est celle des animations
	bool locomotion = false;          // +0x1474 : le déplacement de CPhysical (FUN_00469760) à la place
	bool tourneParAnim = false;       // +0x1484 : avec +0x1474, la rotation de l'image tourne le piéton
	bool sansPlat = false;            // l'octet +0x1558 : ni cap ni déplacement en x et y
	bool pese = false;                // le bit 0x2 de +0x1bc
	bool fige = false;                // le bit 0x1 de +0x1bd : la vitesse tombe à 0, rien ne bouge

	// FUN_00473260 (méthode du piéton, lue en entier à l'assembleur de 0x473260 à 0x473361, 258 octets) : avec +0x1334,
	// le déplacement de l'image tourné du cap +0x1700 (c = cos, s = sin : FUN_0085ad50 et FUN_0085aec0, le _CIcos et le
	// _CIsin du CRT), x = dx·c − s·dy, y = c·dy + dx·s, z = dz, puis multiplié par 1 / dt (dt = CTimer::ms_fTimeStep,
	// 0xc1a9a4) : **la vitesse que les animations donnent**, rangée en +0x1708. Sans +0x1334, +0x1708 reprend la vitesse
	// +0x124. Si dt n'est pas strictement positif, +0x1708 est mis à zéro.
	void VitesseDesAnimations(float dt) {
		if(parAnimations){
			float c = (float)cos((double)cap), s = (float)sin((double)cap);
			float x = deplAnim.x * c - s * deplAnim.y;
			float y = c * deplAnim.y + deplAnim.x * s;
			float z = deplAnim.z;
			float inv = 1.0f / dt;
			vitesseAnim = CVector(x * inv, y * inv, inv * z);
		}else
			vitesseAnim = vitesse;
		if(!(0.0f < dt)) vitesseAnim = CVector(0.0f, 0.0f, 0.0f);
	}

	// La rotation de l'image est-elle l'identité, au sens du jeu : ses trois premiers réels (x, y, z) valent 0.
	bool SansRotation(void) const { return rotAnim[0] == 0.0f && rotAnim[1] == 0.0f && rotAnim[2] == 0.0f; }

	// FUN_0047d510 (CPed::ApplyMoveSpeed, lue à l'assembleur de 0x47d510 à 0x47d9e8 ; 1 247 octets) :
	// - FUN_00473260 d'abord ; puis, si +0x1324 est nul et l'octet +0x1c98 aussi, le cap de l'entité +0x10 prend +0x1700 ;
	// - **avec +0x1474** : le déplacement de CPhysical (FUN_00469760) ; puis, avec +0x1484 et une rotation de l'image qui
	//   n'est pas l'identité, la rotation tourne le piéton (00468300, 00472cd0, FUN_00415350 et 00432bc0 : non refait) ;
	// - **sans +0x1474** : la méthode +0x90 du piéton et le pas de ses stats (0047a130 ; non lues), puis, avec le bit 0x1
	//   de +0x1bd, la vitesse à zéro et la sortie. Sans rotation de l'image (de 0x47d6b3 à 0x47d788) : si l'octet +0x1558
	//   est nul, le cap +0x10 prend +0x1700, **la vitesse en x et y prend celle des animations** (+0x1708, avec +0x1334)
	//   et la position avance de vitesse × dt en x et en y. Puis, sans le bit 0x2 de +0x1bc et avec +0x1334, la vitesse
	//   en z prend celle des animations et la position monte de vz × dt (de 0x47d795 à 0x47d7d8) ; avec le bit 0x2,
	//   z += (+0x1704 + vz) × 0,5 (le double 0x8ff1f8) × dt (de 0x47d8e6 à 0x47d90c). Avec une rotation de l'image (de
	//   0x47d7df à 0x47d8d6) : le déplacement de l'image est tourné par elle (00468300), puis la vitesse +0x1708 et +0x124
	//   viennent de FUN_00414a20 sur la position, et le cap +0x1700 de 00432bc0 : non refait.
	// - non refait : ce qui suit sans le bit 0x2 quand l'octet +0x16cc est levé (de 0x47d916 à 0x47d9e6, le sol).
	// Rend faux quand une branche non refaite est prise (rien n'est alors bougé de plus).
	bool Appliquer(float dt) {
		VitesseDesAnimations(dt);
		if(!capFige && !capFige2) capEntite = cap;
		if(locomotion){
			float p[3] = {pos.x, pos.y, pos.z}, v[3] = {vitesse.x, vitesse.y, vitesse.z};
			CVolProjectile::Deplacer(p, v, fige, false, 0.0f, dt, echelle);
			pos = CVector(p[0], p[1], p[2]); vitesse = CVector(v[0], v[1], v[2]);
			return !(tourneParAnim && !SansRotation());
		}
		if(fige){ vitesse = CVector(0.0f, 0.0f, 0.0f); return true; }
		if(!SansRotation()) return false;
		if(!sansPlat){
			capEntite = cap;
			if(parAnimations){ vitesse.x = vitesseAnim.x; vitesse.y = vitesseAnim.y; }
			pos.x = vitesse.x * dt + pos.x;
			pos.y = vitesse.y * dt + pos.y;
		}
		if(pese){
			pos.z = (vitesseZAvant + vitesse.z) * 0.5f * dt + pos.z;
			return true;
		}
		if(parAnimations){
			vitesse.z = vitesseAnim.z;
			pos.z = vitesse.z * dt + pos.z;
		}
		return true;
	}
};
