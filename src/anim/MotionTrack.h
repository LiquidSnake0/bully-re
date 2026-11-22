// La couche de la locomotion du lecteur d'animations (AM_MotionTrack, table virtuelle 0x94d2f8 : le nom vient du RTTI), ce
// que sa mise à jour (FUN_00731b90, 2 122 octets, recréée dans CLecteurAnim::AvancerMotion de src/anim/AnimTrack.h) calcule chaque
// image : ici ses fonctions de calcul, qui ne lisent aucune mémoire du jeu.
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

	// FUN_00730340(enregistrement, HXD) (méthode de la couche, ret 8, lue en entier de 0x730340 à 0x7303d0, 145 octets) : la
	// vitesse naturelle d'une animation, en mètres par seconde. Une durée (+0xc de l'enregistrement) de moins de 1e-5 s en
	// valeur absolue (0x8ff230), ou égale : 0. Sinon le trajet de l'animation, un vecteur que la fonction 006073a0 du HXD
	// rend pour le mot +0x32 de l'enregistrement (126 octets, lue : elle évalue deux fois, par la fonction 006b3cc0 de 810
	// octets, non lue, avec 0 puis −1 pour dernier argument, et rend la différence des deux vecteurs ; supposé : la position
	// de la racine au début et à la fin de l'animation), puis √|x² + y²| / durée (la racine par 0085c4d0, supposé le sqrt de
	// la bibliothèque C). Ici le trajet est donné.
	static float VitesseNaturelle(float duree, float dx, float dy) {
		if(!(std::fabs(duree) > 1e-5f)) return 0.0f;
		float n2 = std::fabs((float)((double)dx * dx + (double)dy * dy));
		float n = (float)std::sqrt((double)n2);
		return n / duree;
	}

	// La cadence de la boucle de locomotion, dans la mise à jour 00731b90 (lue de 0x73218e à 0x732243) : la vitesse de la
	// place 0 de la couche (+0x24), que la place 1 reprend (+0x38). stat : la stat 0x14 du piéton sur 100 (FUN_00476f10,
	// le double 0x900130) ; v : la vitesse du piéton, le réel +0x1c de l'objet en +0x14 de ce que rend la méthode +0x40 de
	// sa table virtuelle (non lue : supposé sa vitesse de déplacement, en mètres par seconde), gardée en +0x8c de la couche ;
	// nat : la vitesse naturelle de l'animation (FUN_00730340). Une vitesse de piéton négative : stat. Sinon
	// (v / nat) × stat, et stat seule si la vitesse naturelle n'est pas strictement positive : **l'animation est jouée à la
	// vitesse qui fait avancer ses pieds comme le piéton avance**. La mise à jour ne l'écrit que pour la couche qui mène
	// (sans piste meneuse, ou elle-même) et si l'entier 0xbc7338 n'est pas nul (il vaut 1 dans l'exécutable).
	static float Cadence(float v, float nat, float stat) {
		if(v < 0.0f) return stat;
		float k = 1.0f;
		if(nat > 0.0f) k = v / nat;
		return k * stat;
	}

	// FUN_0072e690(x, durée, m0, m2) (cdecl, lue en entier de 0x72e690 à 0x72e74e, 191 octets) : l'instant d'une animation
	// pour une phase demandée x entre 0 et 1, où 0 est la marque m0 et 0,5 la marque m2 (les marques 0 et 2 de l'entrée : les
	// deux appuis, supposé). A = m0 / durée ; B = m2 / durée − A, plus 1 s'il n'est pas strictement positif (la part du
	// tour qui va de m0 à m2) ; passé 0,5 (le double 0x8ff1f8) R = B + ((x − 0,5) × 2) × (1 − B), sinon R = B × 2x ;
	// P = A + R, moins 1 s'il passe 1 ; elle rend P × durée. La mise à jour 00731b90 s'en sert quand une phase est demandée
	// à la couche (+0x88, −1 sans demande ; lue de 0x732021 à 0x732060).
	static float InstantDeLaPhase(float x, float duree, float m0, float m2) {
		float a = m0 / duree;
		float b = (float)((double)m2 / duree - a);
		if(!(b > 0.0f)) b = (float)(b + 1.0);
		float r;
		if(x > 0.5f) r = (float)(b + ((double)x - 0.5) * 2.0 * (1.0 - b));
		else r = (float)((double)b * (x + (double)x));
		float p = (float)((double)a + r);
		if(p > 1.0f) return (float)(((double)p - 1.0) * duree);
		return (float)((double)p * duree);
	}

	// FUN_00432910(a, b, tolérance) (cdecl, lue en entier de 0x432910 à 0x43293f, 48 octets) : vrai si |a − b| ne passe pas
	// la tolérance. La mise à jour 00731b90 compare ainsi l'instant d'une couche qui suit à celui que la meneuse lui donne,
	// à 0,06 s près (0xbc733c).
	static bool Proche(float a, float b, float tol) {
		float d = std::fabs((float)(a - b));
		return !(tol < d);
	}

	// FUN_005fbe50(angle, vecteur) (cdecl, lue en entier de 0x5fbe50 à 0x5fbeb7, 104 octets) : le vecteur tourné de
	// l'angle autour de la verticale, sa hauteur gardée : c = cos(angle) (FUN_0085ad50), s = sin(angle) (FUN_0085aec0),
	// x' = c x − s y, y' = c y + s x. La direction lissée de la couche de locomotion (00730110) tourne ainsi vers
	// l'intention du piéton.
	static void Tourner(float angle, float *v) {
		float c = (float)std::cos((double)angle), s = (float)std::sin((double)angle);
		float x = v[0], y = v[1];
		v[0] = (float)((double)c * x - (double)s * y);
		v[1] = (float)((double)c * y + (double)s * x);
	}
};
