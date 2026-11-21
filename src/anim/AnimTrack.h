// Une couche du lecteur d'animations (AM_AnimTrack, table virtuelle 0x941f90 : le nom vient du RTTI) et ses deux places.
//
// Le lecteur d'un piéton ([piéton + 0x18]) tient sept couches par rang en +0x18 (FUN_00612360, src/anim/Hxd.h). Une couche,
// lue à l'assembleur dans FUN_006b6570, FUN_006bd320 et les fonctions de ce fichier : la table virtuelle en +0, le maillon
// de sa liste en +4, son masque d'os en +8, son poids en +0xc, le reste de son fondu en +0x10 et sa longueur en +0x14, deux
// places de 20 octets en +0x18 et +0x2c, le mélange des deux places en +0x40 et son rang en +0x44.
// Une place : l'enregistrement de l'animation dans le HXD en +0 (0x40 octets : la durée en +0xc, les drapeaux en +0x18,
// CHxdAnim), l'instant en +4, l'instant d'avant en +8, la vitesse en +0xc, des drapeaux en +0x10 : le bit 0 dit que la
// place joue, le bit 1 qu'elle boucle, le bit 2 qu'elle a passé sa première image (le bit 3 n'est pas lu ici).
#pragma once
#include "Hxd.h"

struct CPlaceAnim {
	enum { JOUE = 1, BOUCLE = 2, PARTIE = 4 };
	const CHxdAnim *anim = nil;        // +0
	float t = 0;                       // +4
	float avant = 0;                   // +8
	float vitesse = 0;                 // +0xc
	uint32 drapeaux = 0;               // +0x10

	// FUN_006b2560(dt, actif) (méthode de la place, ret 8, lue en entier de 0x6b2560 à 0x6b26e1, 386 octets) : l'avance
	// d'une place d'une image. Sans animation, rien. Le pas est vitesse × dt. Une place qui joue range son instant en +8 ; un
	// instant d'avant négatif remet +4 et +8 à zéro. Une durée de moins de 0,001 s en valeur absolue (0x900c04) : le bit 0
	// tombe, l'instant à zéro, fini. **La première image ne fait qu'allumer le bit 2** (si la place joue) : l'instant
	// n'avance qu'à partir de la deuxième. Ensuite, une place qui ne joue pas ne fait rien ; sinon l'instant avance du pas,
	// puis, si `actif` (l'appelante, 006b26f0, le dit vrai quand la place pèse), la fonction 006b20c0 (1 183 octets, non
	// lue : le crochet `suite`) reçoit dt. Un instant devenu négatif (une vitesse négative) : la boucle ajoute la durée, et
	// s'il reste négatif il vaut la durée ; sans boucle il vaut 0 et le bit 0 tombe. Un instant passé la durée (strictement) :
	// la boucle retire la durée, et s'il la dépasse encore il vaut 0 ; sans boucle il vaut la durée et le bit 0 tombe. À chaque
	// tour de boucle, le bit 2 des drapeaux de l'enregistrement (+0x18, ce qu'il dit n'est pas lu) éteint le bit 2 de la place :
	// l'image d'après n'avance pas.
	template<class F> void Avancer(float dt, bool actif, F suite) {
		if(anim == nil) return;
		float pas = vitesse * dt;
		bool attente = (drapeaux & PARTIE) == 0, joue = (drapeaux & JOUE) != 0;
		if(joue) avant = t;
		if(avant < 0.0f){ t = 0.0f; avant = 0.0f; }
		float d = anim->duree;
		if((d < 0.0f ? -d : d) < 0.001f){ drapeaux &= ~(uint32)JOUE; t = 0.0f; return; }
		if(attente){ if(joue) drapeaux |= PARTIE; return; }
		if(!joue) return;
		t += pas;
		if(actif) suite(dt);
		if(t < 0.0f){
			if(drapeaux & BOUCLE){
				t += d;
				if(t < 0.0f) t = d;
				if(anim->drapeaux & 4) drapeaux &= ~(uint32)PARTIE;
			}else{
				t = 0.0f;
				drapeaux &= ~(uint32)JOUE;
			}
		}
		if(d >= t) return;
		if(drapeaux & BOUCLE){
			t -= d;
			if(d < t) t = 0.0f;
			if(anim->drapeaux & 4) drapeaux &= ~(uint32)PARTIE;
		}else{
			t = d;
			drapeaux &= ~(uint32)JOUE;
		}
	}
	void Avancer(float dt, bool actif) { Avancer(dt, actif, [](float){}); }
};

struct CAnimTrack {
	int32 masque = -1;                 // +8 : le masque d'os (un pointeur dans le jeu ; ici son rang dans le HXD, -1 sans masque)
	float poids = 0;                   // +0xc
	float reste = 0;                   // +0x10 : le reste du fondu, négatif à l'entrée, positif à la sortie, 0 installée
	float fondu = 0;                   // +0x14 : sa longueur, du même signe
	CPlaceAnim places[2];              // +0x18 et +0x2c
	float melange = 0;                 // +0x40 : la part de la place 1 ; la place 0 pèse 1 - melange
	int32 rang = 0;                    // +0x44

	// FUN_006b26f0(dt) (le rang 3 de la table virtuelle 0x941f90, ret 4, lue en entier de 0x6b26f0 à 0x6b287f, 400 octets) :
	// la mise à jour d'une couche, **la fonction qui fait monter et descendre son poids**. Elle rend faux quand la couche a
	// fini de sortir, vrai sinon.
	// - Un reste négatif (l'entrée, posée par FUN_006b6570 : +0x10 = +0x14 = -fondu, le poids à 0) : un poids déjà à 1 met
	//   +0x10 et +0x14 à zéro ; sinon le poids monte de dt / |+0x14| (poids - dt / +0x14) et le reste de dt ; un poids
	//   strictement au-dessus de 1 ou un reste strictement positif : le poids à 1, +0x10 et +0x14 à zéro. La montée est donc
	//   linéaire sur le fondu, par pas d'image.
	// - Un reste nul : rien.
	// - Un reste positif (la sortie, posée par FUN_006bd320 : +0x10 = +0x14 = le fondu de sortie) : un poids sous 0,001
	//   (0x900c04) met +0x10 et +0x14 à zéro et rend faux, sans toucher au poids ; sinon le poids descend de dt / +0x14 et le
	//   reste de dt ; un poids ou un reste strictement négatif : le poids, +0x10 et +0x14 à zéro, et faux.
	// - Puis, sauf quand elle a rendu faux, ses deux places avancent (FUN_006b2560, CPlaceAnim::Avancer), chacune dite active
	//   si sa part (1 - +0x40 pour la place 0, +0x40 pour la place 1) et le poids de la couche sont strictement positifs.
	template<class F> bool Avancer(float dt, F suite) {
		if(reste < 0.0f){
			if(poids == 1.0f){
				reste = 0.0f; fondu = 0.0f;
			}else{
				poids = poids - dt / fondu;
				reste = reste + dt;
				if(poids > 1.0f || reste > 0.0f){ poids = 1.0f; reste = 0.0f; fondu = 0.0f; }
			}
		}else if(reste > 0.0f){
			if(0.001f > poids){ reste = 0.0f; fondu = 0.0f; return false; }
			poids = poids - dt / fondu;
			reste = reste - dt;
			if(poids < 0.0f || reste < 0.0f){ reste = 0.0f; poids = 0.0f; fondu = 0.0f; return false; }
		}
		for(int32 i = 0; i < 2; i++){
			float part = i == 0 ? 1.0f - melange : melange;
			bool actif = 0.0f < part && 0.0f < poids;
			places[i].Avancer(dt, actif, [&](float x){ suite(i, x); });
		}
		return true;
	}
	bool Avancer(float dt) { return Avancer(dt, [](int32, float){}); }

	// FUN_006b6570(table, indice, drapeaux, mélange, place, vitesse, fondu) (méthode de la couche, ret 0x1c, relue en entier
	// de 0x6b6570 à 0x6b6682, 275 octets) : une animation est lancée sur une des deux places. Dans le jeu l'enregistrement
	// est table[indice] (0x40 octets chacun, nul hors du compte) ; ici il est donné. Le compte de références du groupe de
	// l'animation (le mot +0x2c de son entrée de 60 octets) monte, celui de l'animation remplacée est rendu par 006b3c40 :
	// ce n'est pas refait, tous les groupes sont chargés ici.
	// - Le bit 0 des drapeaux annule le fondu. Un fondu strictement négatif prend celui de l'enregistrement (+0x10).
	// - Un fondu nul : +0x10 = +0x14 = 0, le poids à 1 (la couche est installée tout de suite). Sinon +0x10 = +0x14 = -fondu
	//   et le poids à 0 : c'est l'entrée que FUN_006b26f0 fait monter.
	// - La place : l'enregistrement, l'instant à 0, ou à la durée si la vitesse est strictement négative, la vitesse, et les
	//   drapeaux 1 (elle joue), 3 si le bit 1 est donné (elle boucle), plus 8 si le bit 3 est donné. Le bit 2 de la place
	//   n'est pas posé : sa première image attend (FUN_006b2560). L'instant d'avant (+8) n'est pas écrit.
	// - Pour la place 1 seulement, le mélange est écrit en +0x40.
	// Le poids et le fondu sont réécrits quelle que soit la place : lancer la place 1 d'une couche installée la fait repartir
	// de 0 si le fondu n'est pas nul.
	void Lancer(const CHxdAnim *enr, uint32 dr, float mel, int32 place, float vit, float f) {
		CPlaceAnim &p = places[place];
		if(dr & 1) f = 0.0f;
		p.anim = enr;
		if(f < 0.0f) f = enr->fondu;
		if(f == 0.0f){
			reste = 0.0f; fondu = 0.0f; poids = 1.0f;
		}else{
			reste = -f; fondu = -f; poids = 0.0f;
		}
		p.t = vit < 0.0f ? enr->duree : 0.0f;
		p.vitesse = vit;
		p.drapeaux = (dr & 2) ? 3 : 1;
		if(dr & 8) p.drapeaux |= 8;
		if(place == 1) melange = mel;
	}
};

// Le lecteur d'un piéton, pour ce qui touche ses couches : sept couches par rang (+0x18, un pointeur chacune, nul sans couche),
// sept listes de couches gardées (+0x34, douze octets par rang : la tête, la queue, le compte ; le maillon d'une couche est
// en +4) et le compte des couches posées (+0x88). Une couche gardée est la copie d'une couche remplacée ou retirée : la
// couche qui entre sur ce rang part d'elle tant que son poids n'a pas atteint 1 (l'évaluation de la pose, 006be730, 1 795
// octets, lue seulement jusqu'à sa purge : un poids de 1 vide la liste du rang).
struct CLecteurAnim {
	enum { RANGS = 7, GARDEES = 10 };  // 10 : l'entier 0xbb6128 dans l'exécutable (supposé jamais réécrit)
	CAnimTrack couches[RANGS];         // +0x18 ; `posee` dit si le pointeur du jeu n'est pas nul
	bool posee[RANGS] = {false, false, false, false, false, false, false};
	std::vector<CAnimTrack> gardees[RANGS];   // +0x34, la tête en premier
	int32 compte = 0;                  // +0x88

	// FUN_006b5950(rang) (méthode du lecteur d'un piéton, ret 4, lue en entier de 0x6b5950 à 0x6b598d, 62 octets) : la
	// liste des couches gardées d'un rang est vidée, chaque maillon retiré par la tête, le compte baissé, la couche
	// détruite (le rang 1 de sa table virtuelle).
	void Purger(int32 rang) { gardees[rang].clear(); }

	// FUN_006b9130(rang, couche) (méthode du lecteur d'un piéton, ret 8, lue en entier de 0x6b9130 à 0x6b9210, 225 octets),
	// pour une couche nulle en argument, le seul cas vu ici (FUN_006bd320) : rien sans couche sur ce rang ; si la liste du
	// rang tient déjà 9 couches ou plus ([0xbb6128] - 1), la plus légère est retirée et détruite (le poids +0xc
	// strictement sous le plus petit vu, parti du plus grand réel, 0x9009b8 : la première des plus légères) ; puis la
	// couche du rang est copiée (le rang 2 de sa table virtuelle) et la copie mise en tête de la liste. Avec une couche en
	// argument, c'est elle qui est mise en tête, sans copie.
	void Garder(int32 rang) {
		if(!posee[rang]) return;
		std::vector<CAnimTrack> &l = gardees[rang];
		if((int32)l.size() >= GARDEES - 1){
			int32 min = 0;
			for(int32 i = 1; i < (int32)l.size(); i++)
				if(l[i].poids < l[min].poids) min = i;
			l.erase(l.begin() + min);
		}
		l.insert(l.begin(), couches[rang]);
	}

	// FUN_006bd320(lecteur, couche, fondu) (lue de 0x6bd320 à 0x6bd465, déjà dite dans CHxdFile::FonduDeSortie pour le
	// choix du fondu), ici sur la couche elle-même : la couche d'un rang sort. Le rang 0 sort sans fondu ; un fondu de -1
	// (0x9009ac) prend celui du masque de la couche si elle en porte un (+0x28 du masque, `fonduMasque`). Un fondu d'au
	// moins 0,001 en valeur absolue (0x900c04) est écrit en +0x10 et +0x14 et la couche reste : FUN_006b26f0 fait descendre
	// son poids. Sinon elle est retirée aussitôt : son poids à 1, +0x10 et +0x14 à zéro, **une copie gardée dans la liste
	// de son rang** (FUN_006b9130, sauf au rang 0), puis le masque à nul, les deux places vidées (l'enregistrement nul,
	// les instants à 0, la vitesse à 1, les drapeaux à 0), le mélange à 0, la couche détruite, sa case à nul et le compte
	// +0x88 baissé.
	void Sortir(int32 rang, float f, float fonduMasque = -1.0f) {
		if(!posee[rang]) return;
		CAnimTrack &c = couches[rang];
		if(rang == 0) f = 0.0f;
		else if(f == -1.0f && c.masque >= 0) f = fonduMasque;
		if(!((f < 0.0f ? -f : f) < 0.001f)){ c.reste = f; c.fondu = f; return; }
		c.poids = 1.0f; c.reste = 0.0f; c.fondu = 0.0f;
		if(rang > 0) Garder(rang);
		c = CAnimTrack();
		c.places[0].vitesse = 1.0f; c.places[1].vitesse = 1.0f;
		posee[rang] = false;
		compte--;
	}

	// FUN_006c05b0(lecteur d'un piéton, dt, pose, arg) (méthode du lecteur global 0x20c4910, ret 0x10, lue en entier de
	// 0x6c05b0 à 0x6c0807, 600 octets) : **la mise à jour d'un lecteur à chaque image**, pour ce qui touche ses couches.
	// Pour chaque rang de 0 à 6 : sans couche, la liste des gardées est vidée (FUN_006b5950) ; sinon les gardées dont le
	// rang 6 de la table virtuelle rend vrai sont mises à jour (rang 3 de la table), et retirées si elles rendent faux,
	// que l'entier 0x20c4b14 n'est pas nul et qu'elles ne sont pas en tête : ce rang 6 rend faux pour AM_AnimTrack (trois
	// octets, xor al,al) et vrai pour AM_MotionTrack, **une couche gardée d'AM_AnimTrack reste donc figée**, à son instant
	// et à son poids ; AM_MotionTrack n'est pas refaite, ici toutes les gardées sont figées. Puis la couche du rang avance
	// (FUN_006b26f0) ; si elle rend faux et que le rang n'est pas 0, elle est retirée par FUN_006bd320 avec un fondu nul.
	// Non refait : les gardes de l'entrée (rien si [+0x94] est nul ou si +0x98 porte le bit 0x400 ou le bit 1 ; sans
	// squelette en [+0x94]+0x10 les bits 1 de +0x8c et 0x400 de +0x98 sont posés), une seconde liste avancée par 006b5990,
	// l'évaluation de la pose (006be730 si `pose` est donné, que +0x98 porte le bit 0 et que l'entier 0x20c4b10 est nul,
	// sinon 006bd150), 006be600 et le bit 0x400 posé en sortie.
	template<class F> void ParImage(float dt, F suite) {
		for(int32 r = 0; r < RANGS; r++){
			if(!posee[r]){ Purger(r); continue; }
			if(!couches[r].Avancer(dt, [&](int32 place, float x){ suite(r, place, x); }) && r > 0)
				Sortir(r, 0.0f);
		}
	}
	void ParImage(float dt) { ParImage(dt, [](int32, int32, float){}); }
};
