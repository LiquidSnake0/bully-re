// La couche de la locomotion du lecteur d'animations (AM_MotionTrack, table virtuelle 0x94d2f8 : le nom vient du RTTI), ce
// que sa mise à jour (00731b90, 2 122 octets, lue à l'assembleur, pas encore recréée) calcule chaque image : ici ses fonctions
// de calcul, qui ne lisent aucune mémoire du jeu.
//
// La couche de la locomotion est une AM_AnimTrack (src/anim/AnimTrack.h) plus longue : le piéton en +0x48, l'entrée courante
// en +0x50, les entrées de la locomotion à partir de +0x54. Une entrée est la copie d'une piste LocomotionAnimationNew (0x34
// octets, FUN_0060e240) : le haché de son animation en +0x18 (le champ 24, que la mise à jour cherche dans le HXD), **un
// seuil en +0x1c (le champ 28) et quatre marques en +0x20, +0x24, +0x28 et +0x2c (les champs 32 à 44)**. Le seuil est un
// angle en radians, comparé à la mesure 0072f2a0 ; les marques sont des instants de l'animation, en secondes (supposé :
// les appuis des pieds, la mise à jour joue les bruits de pas à leur passage). Pour C_PLAYER\RUN de Walk_Jocks.cat : le
// seuil 0, les marques 0,633, 0,633, 0,3 et 0,3.
//
// Les calculs : le binaire calcule sur la pile x87 et arrondit à 32 bits à chaque fstp ; ici chaque réel rangé est un float.
#pragma once
#include "../common.h"
#include "../collision/Balayes.h"
#include <cmath>

struct CMotionTrack {
	// FUN_0072edd0 (cdecl, dix réels, lue en entier de 0x72edd0 à 0x72ee5b, 140 octets) : la phase de l'animation cible pour
	// la phase p de l'animation source. Des dix réels elle ne lit que le premier (p), le deuxième (b, la première marque de
	// la cible), le sixième (q4, la dernière marque de la source) et le dixième (s4, la dernière marque de la cible), tous en
	// parts de la durée et comptés depuis la première marque : avant la dernière marque (q4 > p), (p / q4) × s4 ; à partir
	// d'elle, s4 + ((p − q4) / (1 − q4)) × (1 − s4) ; puis b s'ajoute, et un résultat passé 1 (strictement) perd 1.
	static float Phase(float p, float b, float q4, float s4) {
		float r;
		if(q4 > p){
			float u = p / q4;
			r = u * s4;
		}else{
			float n = p - q4;
			float d = 1.0f - q4;
			float u = n / d;
			float v = 1.0f - s4;
			r = s4 + u * v;
		}
		r = r + b;
		if(r > 1.0f) r = r - 1.0f;
		return r;
	}

	// FUN_0072eff0 (cdecl, onze réels, lue en entier de 0x72eff0 à 0x72f291, 674 octets) : la synchro de la marche, l'instant
	// qu'une animation cible prend pour rester en phase avec une animation source. t et d1 : l'instant et la durée de la
	// source ; m1 : ses quatre marques ; d2 et m2 : la durée et les quatre marques de la cible. Les marques sont bornées par le
	// haut à la durée de leur animation. Un instant passé la durée de la source (strictement) est rendu tel quel. Sinon tout
	// passe en parts de durée : p = t / d1 − m1[0] / d1, et les trois autres marques de la source moins la première ; b =
	// m2[0] / d2, et les trois autres marques de la cible moins b ; chacune de ces sept différences, si elle est négative,
	// gagne 1 (les marques tournent avec la boucle). Puis la phase (FUN_0072edd0), fois d2. Le mot 0x20c9024, s'il n'est pas
	// nul, envoie vers une autre fonction de phase (0072eec0, 297 octets, non lue) : il n'est lu que là et aucune instruction
	// ne l'écrit par son adresse ; supposé nul.
	static float Synchro(float t, float d1, const float *m1, float d2, const float *m2) {
		float a[4], c[4];
		for(int i = 0; i < 4; i++) a[i] = m1[i] > d1 ? d1 : m1[i];
		for(int i = 0; i < 4; i++) c[i] = m2[i] > d2 ? d2 : m2[i];
		if(t > d1) return t;
		float A = a[0] / d1;
		float p = t / d1;
		p = p - A;
		float q[4];
		for(int i = 1; i < 4; i++){ q[i] = a[i] / d1; q[i] = q[i] - A; }
		if(p < 0.0f) p = p + 1.0f;
		for(int i = 1; i < 4; i++) if(q[i] < 0.0f) q[i] = q[i] + 1.0f;
		float B = c[0] / d2;
		float s[4];
		for(int i = 1; i < 4; i++){ s[i] = c[i] / d2; s[i] = s[i] - B; }
		for(int i = 1; i < 4; i++) if(s[i] < 0.0f) s[i] = s[i] + 1.0f;
		float r = Phase(p, B, q[3], s[3]);
		return r * d2;
	}

	// FUN_0072f3b0(x, a, b, c, d) (cdecl, lue en entier de 0x72f3b0 à 0x72f438, 137 octets) : x passé de l'intervalle (a, b)
	// à l'intervalle (c, d), borné : d dès que x atteint b (b ≤ x), c tant que x n'a pas passé a (x − a ≤ 0), sinon
	// c + (x − a) × ((d − c) / (b − a)). La mise à jour de la locomotion en tire le mélange de ses deux places (+0x40), avec
	// la mesure de l'angle, les seuils (+0x1c) des deux entrées qui l'encadrent, 0 et 1.
	static float Remapper(float x, float a, float b, float c, float d) {
		float xa = x - a;
		if(b <= x) return d;
		if(xa <= 0.0f) return c;
		float dc = d - c;
		float ba = b - a;
		return (float)((double)c + (double)xa * ((double)dc / ba));
	}

	// FUN_00413c10(x) (cdecl, lue en entier de 0x413c10 à 0x413c62, 84 octets) : l'arc cosinus borné, 0 pour x ≥ 1, π
	// (0x9009a8) pour x ≤ −1, sinon la fonction 0085cdd0 (supposé : l'acos de la bibliothèque C, elle n'est pas lue plus loin
	// que son aiguillage), arrondie à float.
	static float ArcCosinus(float x) {
		if(x >= 1.0f) return 0.0f;
		if(x <= -1.0f) return 3.14159274f;
		return (float)std::acos((double)x);
	}

	// FUN_0072f2a0(u, v) (deux vecteurs par valeur, ret 0x18, lue en entier de 0x72f2a0 à 0x72f399, 250 octets) : l'angle de
	// u à v, en radians, de 0 à 2π. v est normé (FUN_00414cc0), u est pris tel quel (l'appelante le norme, supposé) ; le
	// cosinus est u · v à plat (x et y seulement), borné à −1..1, l'angle son arc cosinus (FUN_00413c10) ; le côté est u · w,
	// w = (v.y, −v.x, 0) normé : s'il passe 0,0001 en valeur absolue (0x900af8) et qu'il est négatif, l'angle devient
	// 2π − angle (0x9009b0). C'est la mesure que la mise à jour de la locomotion compare aux seuils de ses entrées.
	static float Angle(const float *u, const float *v0) {
		float v[3] = { v0[0], v0[1], v0[2] };
		CCollisionBalayee::Normaliser(v);
		float w[3] = { v[1], -v[0], 0.0f };
		CCollisionBalayee::Normaliser(w);
		float cs = (float)((double)u[1] * v[1] + (double)u[0] * v[0]);
		float cote = (float)((double)u[1] * w[1] + (double)u[0] * w[0]);
		if(cs < -1.0f) cs = -1.0f; else if(cs > 1.0f) cs = 1.0f;
		float angle = ArcCosinus(cs);
		float ac = std::fabs(cote);
		if((double)ac > 0.0001f && cote < 0.0f) return (float)(6.283185307179586 - angle);
		return angle;
	}
};
