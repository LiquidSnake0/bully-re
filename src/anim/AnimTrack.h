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
