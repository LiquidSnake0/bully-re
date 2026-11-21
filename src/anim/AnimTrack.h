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
