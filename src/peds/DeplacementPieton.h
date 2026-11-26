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
	bool chute = false;               // les bits 0x1 et 0x2 de l'octet +0x1e24 (FUN_00473200) ; leur écriture n'est pas lue
	float gravite = 0.0f;             // +0x1d58 : ce que la vitesse en z perd par pas de CTimer (FUN_00473200)
	CVector posAvant;                 // +0x1564 : la position d'où la vitesse est refaite après une rotation (supposé : celle
	                                  // du début de l'image ; son écriture n'est pas lue)

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

	// FUN_00468300 (ecx = le piéton ; la rotation de l'image puis son déplacement en arguments ; lue à l'assembleur de 0x468300
	// à 0x46850c, ret 8) : la matrice du piéton (0046dbd0 : la sienne, +0x14, sinon une matrice tournée du cap +0x10,
	// 0046db50 et 00412ad0 : la droite (cos, sin, 0), l'avant (−sin, cos, 0)) tourne le déplacement (dx, dy, 0) (00414560 :
	// dx × la droite + dy × l'avant + 0 × le haut), ajouté à la position ; dz s'y ajoute tel quel (0x46839b). Puis
	// l'orientation : le quaternion de la matrice (004654d0, NiQuaternion::FromRotation) multiplié par celui de l'image
	// (00465370, le produit de Hamilton q(piéton) ⊗ q(image), quaternions rangés x, y, z, w), remis en matrice (00413d10 :
	// 00775a40 puis 00413b00, la matrice NiMatrix3 standard, m01 = 2xy − 2wz, m11 = 1 − 2x² − 2z²), recopiée transposée
	// dans la matrice du piéton (00412920 : la colonne 1 devient l'avant). Sans matrice, le cap +0x10 prend
	// atan2(−avant.x, avant.y) (0x4684c2, le _CIatan2 0085ceea). Enfin la position (0041c390).
	// Ici : le cap de l'entité tient lieu de matrice (le piéton tourne autour de z) ; avec matrice, supposé le même cap.
	void TournerParImage(void) {
		float c = cosf(capEntite), s = sinf(capEntite);
		pos.x += deplAnim.x * c - deplAnim.y * s;
		pos.y += deplAnim.x * s + deplAnim.y * c;
		pos.z += deplAnim.z;
		float x = rotAnim[0], y = rotAnim[1], z = rotAnim[2], w = rotAnim[3];
		float m01 = 2.0f * x * y - 2.0f * w * z, m11 = 1.0f - 2.0f * x * x - 2.0f * z * z;
		float ax = c * m01 - s * m11, ay = s * m01 + c * m11;   // l'avant : Rz(cap) × la colonne 1
		capEntite = atan2f(-ax, ay);
	}
	// FUN_00473200 (la méthode 36 de CPed, +0x90 de sa table 0x90c804, lue en entier de 0x473200 à 0x473254, 85 octets) :
	// avec le bit 0x2 de +0x1bc (pese) et les bits 0x1 et 0x2 de l'octet +0x1e24 : avec +0x1474, celle de CPhysical
	// (0046aa90, non lue) ; sans, la vitesse en z d'avant est gardée en +0x1704 et la vitesse en z (+0x12c) perd
	// +0x1d58 × CTimer::ms_fTimeStep (0xc1a9a4). Rend vrai quand la branche de CPhysical, non refaite, serait prise.
	bool Chute(float dt) {
		if(!pese || !chute) return false;
		if(locomotion) return true;
		vitesseZAvant = vitesse.z;
		vitesse.z -= gravite * dt;
		return false;
	}
	// FUN_00472cd0 (lue en entier, 52 octets) : le quaternion remis à l'identité (0, 0, 0, 1).
	void RotationIdentite(void) { rotAnim[0] = rotAnim[1] = rotAnim[2] = 0.0f; rotAnim[3] = 1.0f; }
	// FUN_00432bc0(x1, y1, x2, y2) (lue en entier de 0x432bc0 à 0x432c9d) : l'angle de (dx, dy) = (x2 − x1, y2 − y1), dans
	// le sens des caps du jeu (φ tel que (dx, dy) suive (sin φ, −cos φ), l'opposé de l'avant d'un cap φ) : dy nul, π/2 (le réel 0x903348) si dx est
	// strictement positif, sinon −π/2 (0x900fb8) ; dy négatif, −atan(dx / dy) (le _CIatan 0085cf40) ; dy positif,
	// π − atan(dx / dy) (le double 0x900160) si dx est strictement positif, sinon −π − atan(dx / dy) (0x900cf8).
	static float AngleEntrePoints(float x1, float y1, float x2, float y2) {
		float dx = x2 - x1, dy = y2 - y1;
		if(dy == 0.0f) return 0.0f < dx ? 1.5707964f : -1.5707964f;
		if(dy < 0.0f) return -atanf(dx / dy);
		return 0.0f < dx ? (float)(3.1415927410125732 - atanf(dx / dy)) : (float)(-3.1415927410125732 - atanf(dx / dy));
	}
	// Le cap +0x1700 que la fin des deux branches tire de l'avant (FUN_00415350, puis FUN_00432bc0(avant.x, avant.y, 0, 0) :
	// l'angle de (0 − avant.x, 0 − avant.y) = (sin h, −cos h), soit h).
	void CapDeLAvant(void) { cap = AngleEntrePoints(-sinf(capEntite), cosf(capEntite), 0.0f, 0.0f); }

	// FUN_0047d510 (CPed::ApplyMoveSpeed, lue à l'assembleur de 0x47d510 à 0x47d9e8 ; 1 247 octets) :
	// - FUN_00473260 d'abord ; puis, si +0x1324 est nul et l'octet +0x1c98 aussi, le cap de l'entité +0x10 prend +0x1700 ;
	// - **avec +0x1474** : le déplacement de CPhysical (FUN_00469760) ; puis, avec +0x1484 et une rotation de l'image qui
	//   n'est pas l'identité (de 0x47d59e à 0x47d610) : le déplacement de l'image mis à zéro, la rotation tourne le piéton
	//   (TournerParImage), le quaternion revient à l'identité, le cap +0x1700 prend celui de l'avant, et la fin ;
	// - **sans +0x1474** : la méthode +0x90 du piéton (Chute) et 0047a130 (non lue), puis, avec le bit 0x1
	//   de +0x1bd, la vitesse à zéro et la sortie. Sans rotation de l'image (de 0x47d6b3 à 0x47d788) : si l'octet +0x1558
	//   est nul, le cap +0x10 prend +0x1700, **la vitesse en x et y prend celle des animations** (+0x1708, avec +0x1334)
	//   et la position avance de vitesse × dt en x et en y. Puis, sans le bit 0x2 de +0x1bc et avec +0x1334, la vitesse
	//   en z prend celle des animations et la position monte de vz × dt (de 0x47d795 à 0x47d7d8) ; avec le bit 0x2,
	//   z += (+0x1704 + vz) × 0,5 (le double 0x8ff1f8) × dt (de 0x47d8e6 à 0x47d90c). Avec une rotation de l'image (de
	//   0x47d7df à 0x47d8d6, lue le 26.11) : la position +0x1564 gardée, TournerParImage, le déplacement de l'image à zéro,
	//   le quaternion à l'identité (00472cd0), **la vitesse +0x1708, puis +0x124 en x et y, prend la position moins
	//   +0x1564** (FUN_00414a20, sans division par dt), le cap +0x1700 celui de l'avant ; puis la suite commune (0x47d8dd :
	//   le bit 0x2 de +0x1bc), sans la vitesse en z des animations ni l'octet +0x1558.
	// - non refait : ce qui suit sans le bit 0x2 quand l'octet +0x16cc est levé (de 0x47d916 à 0x47d9e6, le sol).
	// Rend faux quand une branche non refaite est prise (rien n'est alors bougé de plus).
	bool Appliquer(float dt) {
		VitesseDesAnimations(dt);
		if(!capFige && !capFige2) capEntite = cap;
		if(locomotion){
			float p[3] = {pos.x, pos.y, pos.z}, v[3] = {vitesse.x, vitesse.y, vitesse.z};
			CVolProjectile::Deplacer(p, v, fige, false, 0.0f, dt, echelle);
			pos = CVector(p[0], p[1], p[2]); vitesse = CVector(v[0], v[1], v[2]);
			if(tourneParAnim && !SansRotation()){
				deplAnim = CVector(0.0f, 0.0f, 0.0f);
				TournerParImage();
				RotationIdentite();
				CapDeLAvant();
			}
			return true;
		}
		Chute(dt);
		if(fige){ vitesse = CVector(0.0f, 0.0f, 0.0f); return true; }
		if(!SansRotation()){
			CVector avant = posAvant;
			TournerParImage();
			deplAnim = CVector(0.0f, 0.0f, 0.0f);
			RotationIdentite();
			vitesseAnim = CVector(pos.x - avant.x, pos.y - avant.y, pos.z - avant.z);
			vitesse.x = vitesseAnim.x; vitesse.y = vitesseAnim.y;
			CapDeLAvant();
			if(pese) pos.z = (vitesseZAvant + vitesse.z) * 0.5f * dt + pos.z;
			return true;
		}
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
