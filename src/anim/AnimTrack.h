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
#include "MotionTrack.h"

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

// Une entrée de la couche de locomotion (AM_MotionTrack, src/anim/MotionTrack.h) : la copie d'une piste
// LocomotionAnimationNew (0x34 octets). Le jeu cherche son enregistrement dans le HXD à chaque image par son haché
// (00607610) et en calcule la vitesse naturelle (FUN_00730340) ; ici les deux sont portés par l'entrée, posés une fois.
struct CMotionEntree {
	uint32 hache = 0;                  // +0x18
	float seuil = 0;                   // +0x1c : un angle en radians
	float marques[4] = {0, 0, 0, 0};   // +0x20 … +0x2c : des instants de l'animation, en secondes
	const CHxdAnim *enr = nil;
	float naturelle = 0;               // m/s
};

// Ce que la couche de locomotion lit du piéton à chaque image. L'objet M est le +0x14 de ce que rend la méthode +0x40 de
// la table virtuelle du piéton (0x90d0d4 ; FUN_00474a80, 12 octets, lue : le composant d'actions +0x2e0 plus 0x300).
struct CMotionPieton {
	float stat = 1.0f;                 // FUN_00476f10(piéton, 0x14) / 100 (le double 0x900130)
	float vitesse = -1.0f;             // +0x1c de M : la vitesse que la piste Locomotion pose (00613480), −1 sans elle
	float allure = -1.0f;              // +0x20 de M (non suivi)
	float intention[3] = {0, 0, 0};    // +0 … +8 du composant d'actions : la direction de l'intention de mouvement
	float intentionVitesse = 0;        // +0x1c du composant d'actions
	float avant[3] = {0, 1, 0};        // FUN_00415350 : l'avant du piéton
	// Les appuis des pieds, sur M : les octets +0x30 et +0x31, et le temps écoulé depuis chacun (+0x34 et +0x38).
	bool pas[2] = {false, false};
	float depuisPas[2] = {0, 0};
	// FUN_005f3610(vrai ou faux) pour le pied 0 et FUN_005f3640 pour le pied 1 (méthodes de M, ret 4, lues en entier de
	// 0x5f3610 à 0x5f363e et de 0x5f3640 à 0x5f366e) : faux efface l'octet de l'appui ; vrai le pose et remet son temps à
	// 0, **sauf si le dernier appui de ce pied a moins de 0,1 s** (0xb0a4ec). La piste LocomotionEvent (0060ec30) lit ces
	// octets, joue le bruit du pas et les efface. Ce qui fait avancer les deux temps n'est pas lu.
	void Appui(int32 pied, bool pose) {
		if(!pose){ pas[pied] = false; return; }
		if(0.1f > depuisPas[pied]) return;
		pas[pied] = true;
		depuisPas[pied] = 0.0f;
	}
};

struct CAnimTrack {
	int32 masque = -1;                 // +8 : le masque d'os (un pointeur dans le jeu ; ici son rang dans le HXD, -1 sans masque)
	float poids = 0;                   // +0xc
	float reste = 0;                   // +0x10 : le reste du fondu, négatif à l'entrée, positif à la sortie, 0 installée
	float fondu = 0;                   // +0x14 : sa longueur, du même signe
	CPlaceAnim places[2];              // +0x18 et +0x2c
	float melange = 0;                 // +0x40 : la part de la place 1 ; la place 0 pèse 1 - melange
	int32 rang = 0;                    // +0x44
	// La suite de l'objet quand la couche est une AM_MotionTrack (table virtuelle 0x94d2f8, 0xa8 octets), dans l'état où
	// son constructeur la laisse. FUN_00731470 (lue en entier de 0x731470 à 0x731551, 226 octets) : les deux places à
	// zéro avec la vitesse 1, le piéton (+0x48), la copie de la piste Locomotion (+0x4c), l'entrée courante (+0x50) et
	// les dix entrées (+0x54 … +0x78) à nil, l'octet +0x7c et le réel +0x8c à 0, **les réels +0x80 et +0x88 à −1**, les
	// vecteurs +0x90 et +0x9c égaux au vecteur global 0xbcc564 (255 références dans l'exécutable ; supposé le vecteur
	// nul : il est dans la zone non initialisée). Le réel +0x84 n'y est pas écrit (supposé 0, la mémoire d'une réserve).
	// **L'évaluation d'une couche** (le rang 4 de sa table virtuelle, appelé par 006be730 avec le lecteur, la pose de
	// travail, les trois réels du déplacement et un quaternion ; ret 0x10).
	// - AM_AnimTrack, FUN_006b9870 (lue en entier de 0x6b9870 à 0x6b988d, 30 octets) : **la place 0 seule**, 006b6e50
	//   (non lue) appelée avec les mêmes arguments et le rang de place 0.
	// - AM_MotionTrack, FUN_0072f440 (5 octets : un saut vers FUN_006b9890, lue en entier de 0x6b9890 à 0x6b9917, 136
	//   octets) : faux si le lecteur n'a rien en [+0x94]+0x10 ; sinon **les deux places sont évaluées dans l'ordre**
	//   (006b6e50 avec le rang 0 puis 1), la place 0 au poids 1 − (+0x40), la place 1 au poids (+0x40). La première
	//   évaluation réussie reste dans la pose donnée (la pose de travail devient alors celle de 0x20c48a0) ; **une
	//   seconde réussie y est mêlée par FUN_006bcc60, sans masque, à son poids**. Rend vrai.
	// Supposé : 006b6e50 rend faux pour une place sans animation.
	struct Evaluation { int32 place; float poids; };
	int32 Evaluations(Evaluation out[2]) const {
		out[0].place = 0; out[0].poids = motion ? 1.0f - melange : 1.0f;
		if(!motion) return 1;
		out[1].place = 1; out[1].poids = melange;
		return 2;
	}
	// FUN_006b6e50(lecteur, pose, déplacement, quaternion, place) (méthode de la couche, ret 0x14, lue en entier de
	// 0x6b6e50 à 0x6b70dd, 654 octets) : **l'évaluation d'une place**.
	// - Faux si le squelette du lecteur n'a pas de dictionnaire ([+0x94]+0x10) ou si la place n'a pas d'animation.
	// - Le poids de la place : 1 − (+0x40) pour la place 0, (+0x40) pour la place 1. **Sous 0,001 (0x900c04), le bit 2
	//   des drapeaux de la place (+0x10) est éteint et elle rend faux** : la place n'est pas évaluée.
	// - Sinon 006bd810 (2 615 octets, non lue) décode l'animation à l'instant de la place (+4) dans la pose. Selon les
	//   bits 6, 7 et 2 des drapeaux de l'enregistrement (+0x18) : le bit 6 appelle 006b67b0 ; les bits 7 et 6 ensemble
	//   006b6900 (1 359 octets), qui rend le déplacement ; sinon le bit 2 appelle 006b47b0, qui rend le déplacement, et
	//   sans le bit 6, 006b6690 (toutes non lues). Si le bit 2 de +0x98 du lecteur est levé, à la place, les deux
	//   vecteurs +0x6c et +0x38 de [+0xc] sont mis au vecteur 0xbcc564. Le déplacement est multiplié par le poids quand
	//   il est sous 1 (0088a490) et ajouté à celui de l'appelante (004150b0).
	// - **L'échelle du bassin** (de 0x6b703d à 0x6b70cb), si l'entier 0x20c48c0 est nul et que le dictionnaire a quelque
	//   chose en +0x70 : R est le réel +0x28 du descripteur du groupe de l'animation (FUN_00450a30 : le descripteur de
	//   60 octets du rang +0x20 de l'enregistrement, dans la table +0x6c du dictionnaire, nul passé le compte +0x68) ;
	//   si |R| atteint 0,001, **le réel +0x20 de l'os 1 de la pose (l'os 2 si l'enregistrement porte le drapeau 0x200)
	//   est multiplié par la norme (FUN_0040df90) du vecteur de cet os dans la table +0xa8 du squelette (12 octets par
	//   os), divisée par R**. Ce +0x28 est lu dans le fichier HXD (CHxdGroupe::reference) et la table +0xa8 dans MXDs.MGR
	//   (CMxdModele::vecteurs).
	// - **Le bit 2 des drapeaux de la place est allumé** et elle rend vrai.
	// Recréé ici : ce qui décide et ce qui touche la place. L'outil applique l'échelle à l'os 1 (EchelleDuBassinOutil).
	bool PlaceEvaluee(int32 place) {
		CPlaceAnim &p = places[place];
		if(p.anim == nil) return false;
		float w = place == 0 ? 1.0f - melange : melange;
		if(w < 0.001f){ p.drapeaux &= ~(uint32)CPlaceAnim::PARTIE; return false; }
		if(!(0.0f < w)) return true;
		p.drapeaux |= CPlaceAnim::PARTIE;
		return true;
	}
	// FUN_006b48c0(lecteur, sortie, ?) (le rang 5, +0x14, des deux tables virtuelles 0x941f90 et 0x94d2f8 ; ret 0xc, lue
	// en entier de 0x6b48c0 à 0x6b496d, 176 octets) : **le déplacement que la couche rend sans pose**. Sans animation
	// à la place 0, faux. Si le lecteur porte le bit 2 de +0x98, les vecteurs +0x6c et +0x38 de [lecteur+0xc] sont mis
	// au vecteur 0xbcc564 et rien n'est rendu (`gele`) ; sinon, si l'enregistrement de la place 0 porte le bit 2 de ses
	// drapeaux (+0x18), la sortie reçoit le déplacement de la place 0 (FUN_006b4820, ici `image` : AgrDeplacementImage) ;
	// sans ce bit la sortie n'est pas touchée. Vrai dans les trois cas. La place 1 n'est pas lue ici.
	template<class F> bool Deplacement(bool gele, F image, CVector *d) const {
		if(places[0].anim == nil) return false;
		if(gele) return true;
		if(places[0].anim->drapeaux & 4) *d = image(places[0]);
		return true;
	}
	static int32 OsDuBassin(uint32 drapeauxEnregistrement) { return (drapeauxEnregistrement & 0x200) ? 2 : 1; }
	static float EchelleDuBassin(float normeDuSquelette, float r) { return (r < 0.0f ? -r : r) < 0.001f ? 1.0f : normeDuSquelette / r; }
	// Les évaluations de la couche, chacune passée par FUN_006b6e50 : le nombre de celles qui rendent vrai.
	int32 Evaluer(void) {
		Evaluation ev[2];
		int32 n = Evaluations(ev), vraies = 0;
		for(int32 i = 0; i < n; i++) if(PlaceEvaluee(ev[i].place)) vraies++;
		return vraies;
	}
	enum { ENTREES = 10 };
	bool motion = false;               // la table virtuelle : faux AM_AnimTrack, vrai AM_MotionTrack
	int32 courante = -1;               // +0x50 : l'entrée jouée par la place 0 (un pointeur dans le jeu)
	CMotionEntree entrees[ENTREES];    // +0x54 : dix pointeurs, remplis dans l'ordre par FUN_0060e240
	int32 nEntrees = 0;
	bool synchro = false;              // +0x7c : la mise à jour 00731b90 est passée au moins une fois
	float precedent = -1.0f;           // +0x80 : l'instant de la place 0 à la fin de la mise à jour précédente
	float garde = 0.0f;                // +0x84 : l'instant de la place 0 relevé par FUN_0072f3a0
	float demande = -1.0f;             // +0x88 : une phase demandée, −1 sans demande
	float vitesse = 0.0f;              // +0x8c
	float dir[3] = {0, 0, 0};          // +0x90 : la direction lissée du mouvement
	float avant[3] = {0, 0, 0};        // +0x9c : l'avant du piéton

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

	// FUN_0072f3a0 (méthode de la couche de locomotion, lue en entier de 0x72f3a0 à 0x72f3a9, 10 octets) : +0x84 prend
	// l'instant de la place 0 (+0x1c). Son seul appelant est FUN_004741b0 (CLecteurAnim::ReleverMeneuses). Une couche qui
	// suit se cale sur cet instant de la meneuse.
	void Relever(void) { garde = places[0].t; }

	// La couche de locomotion neuve (FUN_00731470).
	static CAnimTrack Motion(void) {
		CAnimTrack c;
		c.motion = true;
		c.places[0].vitesse = 1.0f; c.places[1].vitesse = 1.0f;
		return c;
	}

	// FUN_00730110(dt) (méthode de la couche de locomotion, ret 4, lue en entier de 0x730110 à 0x730334, 549 octets) :
	// **la direction lissée du mouvement**. +0x8c prend le +0x20 de M ; d est l'intention de mouvement du piéton mise à
	// plat et normée (FUN_00414cc0) ; si +0x8c est négatif il devient la vitesse de l'intention × 3 (0xbc724c). Une
	// direction +0x90 encore égale au vecteur 0xbcc564 prend d. Sinon le cosinus de l'écart (x x + y y, borné à −1 et 1
	// par 0x9009ac) passe par FUN_00413c10 : si 300 × dt (0xbc7254) dépasse l'écart, la direction prend d ; sinon elle
	// tourne de 300 × dt (FUN_005fbe50), le signe changé (0xbc72d4 = −1) quand d est du côté de (y, −x, 0) normé (un
	// produit scalaire qui passe 0,0001 en valeur absolue, 0x900af8, et n'est pas négatif). Puis +0x9c prend l'avant du
	// piéton (FUN_00415350, recréée dans src/objects/Projectiles.h). À 30 images par seconde 300 × dt vaut 10 radians :
	// plus que tout écart, la direction suit l'intention sans retard.
	void Direction(float dt, const CMotionPieton &p) {
		vitesse = p.allure;
		float d[3] = { p.intention[0], p.intention[1], 0.0f };
		CCollisionBalayee::Normaliser(d);
		if(vitesse < 0.0f) vitesse = p.intentionVitesse * 3.0f;
		if(dir[0] == 0.0f && dir[1] == 0.0f && dir[2] == 0.0f){
			dir[0] = d[0]; dir[1] = d[1]; dir[2] = d[2];
		}else{
			float cs = (float)((double)dir[1] * d[1] + (double)dir[0] * d[0]);
			if(cs < -1.0f) cs = -1.0f; else if(cs > 1.0f) cs = 1.0f;
			float pas = 300.0f * dt;
			float ecart = CMotionTrack::ArcCosinus(cs);
			if(pas > ecart){
				dir[0] = d[0]; dir[1] = d[1]; dir[2] = d[2];
			}else{
				float w[3] = { dir[1], -dir[0], 0.0f };
				CCollisionBalayee::Normaliser(w);
				float cote = (float)((double)w[1] * d[1] + (double)w[0] * d[0]);
				if((double)std::fabs(cote) > 0.0001f && !(0.0f > cote)) pas = pas * -1.0f;
				CMotionTrack::Tourner(pas, dir);
			}
		}
		avant[0] = p.avant[0]; avant[1] = p.avant[1]; avant[2] = p.avant[2];
	}
};

// FUN_006bcc60(pose, a, b, poids, masque) (cdecl, cinq arguments, lue en entier de 0x6bcc60 à 0x6bcea6, 583 octets) : le
// mélange de deux poses dans une troisième. Une pose : le nombre d'os en +0x2c, la taille d'un os en +0x24, les os à
// partir de +0x4c (36 octets copiés par os), **la fonction qui mêle un os en +0x40** (appelée (os, os de a, os de b,
// poids) ; non lue : l'outil mêle les rotations en interpolation sphérique, supposé).
// - Sans masque : un poids égal à 1 copie b (006bcbd0) ; un poids de valeur absolue sous 0,001 (0x900c04) copie a ;
//   sinon chaque os est mêlé au poids donné.
// - Avec un masque (ses poids en +0x20, leur nombre en +0x24) : le poids d'un os est celui du masque, **-1 (0x9009ac)
//   pour un os au-delà du masque**, multiplié par le poids donné quand celui-ci n'est pas 1 ; un poids d'os égal à 1 copie
//   l'os de b, une valeur absolue sous 0,001 copie l'os de a et rien d'autre ; dans les autres cas, et après la copie de
//   b, la fonction de l'os est appelée avec ce poids (le -1 lui est passé tel quel).
struct CPoseMelange {
	enum Geste { GARDER, COPIER, MELER };
	static float Nul(void) { return 0.001f; }                                   // 0x900c04
	static Geste SansMasque(float poids) { return poids == 1.0f ? COPIER : (poids < 0.0f ? -poids : poids) < Nul() ? GARDER : MELER; }
	static float PoidsOs(float poids, const float *masque, uint32 n, uint32 os) {
		float m = os < n ? masque[os] : -1.0f;
		return poids == 1.0f ? m : m * poids;
	}
	static Geste AvecMasque(float poidsOs) { return poidsOs == 1.0f ? COPIER : (poidsOs < 0.0f ? -poidsOs : poidsOs) < Nul() ? GARDER : MELER; }
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
	// Le début de l'évaluation de la pose (006be730, lue de 0x6be7bb à 0x6be81b, puis 0x6bead9 à 0x6beae1) : pour chaque
	// rang de 0 à 6 qui porte une couche, **si le poids de la couche (+0xc) n'est pas strictement sous 1, la liste des
	// gardées du rang est vidée** (FUN_006b5950) : une couche installée ne part plus de celles qu'elle a remplacées. Sous
	// 1, avec une liste non vide, le bit 0x1000 de +0x98 est posé et la liste est parcourue (la suite, le mélange, n'est
	// pas refaite). Supposé : l'évaluation est faite à chaque image pour chaque piéton (FUN_006c05b0 ne l'appelle que si
	// une pose lui est donnée, que +0x98 porte le bit 0 et que l'entier 0x20c4b10 est nul, sinon FUN_006bd150, plus bas).
	void PurgerInstallees(void) {
		for(int32 r = 0; r < RANGS; r++)
			if(posee[r] && !(1.0f > couches[r].poids)) Purger(r);
	}
	// La suite de l'évaluation, pour un rang dont la couche est sous le poids 1 (006be730, lue de 0x6be821 à 0x6beb2e) :
	// le bit 0x1000 de +0x98 posé, **la liste des gardées est retournée** (chaque maillon retiré par la tête et chaîné
	// devant le précédent, de 0x6be847 à 0x6be89c ; la méthode +0x18 de chacune est appelée : vraie, son poids s'ajoute à
	// une somme et un drapeau est levé), puis **chaque gardée est évaluée en partant de la queue de la liste, la plus
	// ancienne, jusqu'à la tête** (sa méthode +0x10, dans la pose de travail) et mêlée à la pose accumulée par
	// FUN_006bcc60 avec **son propre poids (+0xc) et son propre masque (+8)**, chacune remise en tête de la liste, qui
	// retrouve son ordre ; le bit 0x1000 retiré. **La couche du rang est évaluée ensuite** et mêlée de même avec son
	// poids et son masque (de 0x6beae1 à 0x6beb1e). La toute première évaluation de l'image n'est pas mêlée : la pose de
	// travail est alors la pose accumulée elle-même (les deux pointeurs égaux en 0x6be91a et 0x6beb05), et la pose de
	// travail devient ensuite celle de +0x148. Non refait : les trois réels du déplacement que chaque évaluation rend,
	// mêlés à côté (00412850), et la fin (de 0x6beb2e à 0x6bee33, vue au décompilé seulement).
	// L'évaluation entière pour ce qui touche les couches : la purge des rangs installés, puis chaque gardée et la
	// couche de chaque rang évaluées (FUN_006b9870 ou FUN_006b9890, donc FUN_006b6e50 par place) : **chaque place
	// évaluée retrouve son bit 2**, celui qu'un tour de boucle lui retire (FUN_006b2560) et que le remplacement d'une
	// couche demande pour la garder (FUN_006bcad0). Supposé, comme pour la purge : faite à chaque image.
	void Evaluer(void) {
		PurgerInstallees();
		for(int32 r = 0; r < RANGS; r++){
			if(!posee[r]) continue;
			for(size_t i = gardees[r].size(); i-- > 0;) gardees[r][i].Evaluer();
			couches[r].Evaluer();
		}
	}
	// FUN_006bd150(lecteur, réel) (ret 8, lue en entier de 0x6bd150 à 0x6bd2fb, 430 octets) : **l'évaluation sans pose**,
	// celle que FUN_006c05b0 appelle quand aucune pose n'est donnée : seul le déplacement du piéton est calculé. Pour
	// chaque rang de 0 à 6 qui porte une couche : un vecteur nul reçoit le déplacement de la couche (le rang 5 de sa
	// table, FUN_006b48c0 ; ici `deplacement`), puis **la liste des gardées du rang est vidée** (FUN_006b5950), quel
	// que soit le poids de la couche. Le rang 0 remplace le déplacement accumulé. Un rang plus haut le mêle : son masque
	// (+8), ou sans masque le premier du dictionnaire s'il en a (+0xc non nul, +8) ; sans aucun masque le rang est
	// sauté ; w est **le premier poids du masque** (*[masque+0x20], l'os 0), ou −1 (0x9009ac) si le masque n'a pas de
	// poids, et le déplacement devient accumulé × (1 − w) + celui du rang × w (FUN_00412850). **Le poids de la couche
	// (+0xc) n'y entre pas.** À la fin, si le réel donné est strictement positif et que [lecteur+0xa4] n'est pas nul, le
	// déplacement y est écrit ; le bit 0x2000 de +0x98 est posé ; rend 1. Non refait : le crochet de mise au point du
	// début (les entiers 0xbb6458, 0x20c48fc et 0x20c48f8, l'appel de [0x8fc05c]).
	// `premierPoids(masque, &w)` : faux si le masque (un rang du HXD, −1 sans) ne mène à aucun masque.
	template<class F, class M> CVector EvaluerSansPose(F deplacement, M premierPoids) {
		CVector acc;
		for(int32 r = 0; r < RANGS; r++){
			if(!posee[r]) continue;
			CVector v;
			deplacement(couches[r], &v);
			Purger(r);
			if(r == 0){ acc = v; continue; }
			float w;
			if(!premierPoids(couches[r].masque, &w)) continue;
			float u = 1.0f - w;
			acc = CVector(acc.x * u + v.x * w, acc.y * u + v.y * w, acc.z * u + v.z * w);
		}
		return acc;
	}
	struct Evaluee { const CAnimTrack *couche; float poids; bool gardee; };
	int32 OrdreDuRang(int32 r, std::vector<Evaluee> &ordre) const {
		ordre.clear();
		if(!posee[r]) return 0;
		if(1.0f > couches[r].poids)
			for(size_t i = gardees[r].size(); i-- > 0;) ordre.push_back({&gardees[r][i], gardees[r][i].poids, true});
		ordre.push_back({&couches[r], couches[r].poids, false});
		return (int32)ordre.size();
	}

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
		// La copie d'une couche de locomotion (le rang 2 de sa table virtuelle, FUN_007315a0, 90 octets : une couche neuve
		// de la réserve 0x20c9028 puis FUN_0072ff40, lue en entier de 0x72ff40 à 0x7300ff, 448 octets) : la base
		// (006b4510), le piéton, une copie de la piste Locomotion et de chaque entrée, l'entrée courante, +0x7c, +0x80,
		// +0x84, +0x8c et les deux vecteurs ; **la phase demandée +0x88 n'est pas copiée**, elle reste à −1.
		if(l[0].motion) l[0].demande = -1.0f;
	}

	// FUN_006b1e40(rang) (méthode du lecteur, ret 4, lue en entier de 0x6b1e40 à 0x6b1e54, 21 octets) : la couche d'un
	// rang, nil pour un rang de 7 ou plus ou une place vide.
	CAnimTrack *Couche(int32 rang) { return (uint32)rang < (uint32)RANGS && posee[rang] ? &couches[rang] : nil; }

	// FUN_00731ae0(lecteur, rang) (cdecl, lue en entier de 0x731ae0 à 0x731b8c) : **la couche de locomotion qui peut mener
	// un rang**. g : la première des couches gardées du rang si c'est une AM_MotionTrack (le rang 0 de la table virtuelle
	// compare un numéro de type, le mot 0x1b65250, rendu par 00607490) ; c : la couche du rang (FUN_006b1e40). Si c n'est
	// pas une couche de locomotion : g. Sinon g tant que le poids de c (+0xc) est sous 0,5 (0x9055a0), puis c : **pendant
	// la première moitié d'un fondu de la marche à la course, c'est la boucle quittée qui mène**.
	CAnimTrack *Meneuse(int32 rang) {
		CAnimTrack *g = !gardees[rang].empty() && gardees[rang][0].motion ? &gardees[rang][0] : nil;
		CAnimTrack *c = Couche(rang);
		if(c == nil || !c->motion) return g;
		if(g != nil && 0.5f > c->poids) return g;
		return c;
	}

	// FUN_004741b0 (méthode du piéton, lue en entier de 0x4741b0 à 0x4741e1, 50 octets) : pour chacun des sept rangs,
	// la meneuse du rang (FUN_00731ae0 sur le lecteur +0x114 du piéton), s'il y en a une, relève son instant
	// (FUN_0072f3a0) ; puis elle saute à 00467d60 (non lue). Aucun appel direct ne la vise (une table virtuelle du
	// piéton) : supposé une fois par image, après la mise à jour des couches.
	void ReleverMeneuses(void) {
		for(int32 r = 0; r < RANGS; r++)
			if(CAnimTrack *m = Meneuse(r)) m->Relever();
	}

	// FUN_006bcb70(lecteur, couche, rang) (méthode du lecteur global 0x20c4910, ret 0xc, lue en entier de 0x6bcb70 à
	// 0x6bcbd3) : **une couche déjà construite prend un rang**. La couche du rang est d'abord gardée (FUN_006b9130 avec
	// 0), puis détruite (le rang 1 de sa table virtuelle) ; sans couche le compte +0x88 monte ; la couche reçoit son rang
	// (+0x44) et prend la place. C'est ainsi que FUN_0060e240 (LocomotionAnimationNew) installe la couche de locomotion
	// que la piste Locomotion a construite (FUN_0060c5b0).
	void Installer(int32 rang, const CAnimTrack &c) {
		Garder(rang);
		if(!posee[rang]) compte++;
		couches[rang] = c;
		couches[rang].rang = rang;
		posee[rang] = true;
	}

	// FUN_006bcad0(lecteur d'un piéton, ..., couche, ...) (méthode du lecteur global, ret 0x20, lue en entier de 0x6bcad0 à
	// 0x6bcb6e, 159 octets) : une couche neuve prend son rang (+0x44) et une animation y est lancée. Si le rang porte déjà
	// une autre couche : une copie en est gardée (FUN_006b9130, sans couche en argument) seulement si sa place 0 porte une
	// animation et a passé sa première image (le bit 2 de ses drapeaux, +0x28 de la couche), puis elle est détruite ; sans
	// couche sur ce rang le compte +0x88 monte. Puis FUN_006b6570 sur la couche neuve, avec la table des enregistrements du
	// HXD du lecteur ([[lecteur + 0x94] + 0x10]) et les six autres arguments tels quels. Si la couche donnée est déjà celle
	// du rang, seul le lancement est fait : ce cas n'est pas refait ici, la couche est toujours neuve (FUN_00612360 la
	// prend par 006b9220).
	void Poser(int32 rang, const CHxdAnim *enr, uint32 dr, float mel, int32 place, float vit, float f) {
		if(posee[rang]){
			if(couches[rang].places[0].anim != nil && (couches[rang].places[0].drapeaux & CPlaceAnim::PARTIE)) Garder(rang);
		}else
			compte++;
		couches[rang] = CAnimTrack();
		couches[rang].rang = rang;
		posee[rang] = true;
		couches[rang].Lancer(enr, dr, mel, place, vit, f);
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
	// FUN_00731b90(dt) (le rang 3 de la table virtuelle 0x94d2f8, méthode de la couche, ret 4, lue en entier de 0x731b90
	// à 0x7323d7, 2 122 octets) : **la mise à jour d'une couche de locomotion**, à la place de FUN_006b26f0 qu'elle
	// appelle en sortant. Dans l'ordre :
	// 1. la meneuse : FUN_00731ae0 pour le rang 1 puis le rang 0, la première dont l'octet +0x7c n'est pas nul ;
	// 2. si la couche du rang 0, à défaut celle du rang 1, est une couche de locomotion : la direction lissée de cette
	//    couche-ci (FUN_00730110) ; avec une meneuse, sa direction +0x90 est copiée ; la direction est mise à plat et
	//    normée (FUN_00414cc0) et **l'angle** est FUN_0072f2a0(direction, avant du piéton +0x9c) ;
	// 3. sans première entrée (+0x54), elle rend vrai sans rien avancer ;
	// 4. le choix des deux entrées (0x731cd0 à 0x731d30) : `bas` part de la première ; les suivantes sont lues dans
	//    l'ordre jusqu'à la première dont le seuil (+0x1c) dépasse l'angle, qui devient `haut` ; chaque autre devient
	//    `bas` ; si aucune ne dépasse l'angle et qu'il y a plus d'une entrée, `haut` est la première (le tour revient) ;
	// 5. la source de la synchro : la meneuse si elle a un enregistrement en place 0 (son instant relevé +0x84, la durée
	//    de son enregistrement, les marques de son entrée courante) ; sinon cette couche (son instant +0x1c, la durée de
	//    son enregistrement, les marques de son entrée courante) ; sinon l'entrée `bas` elle-même ;
	// 6. l'instant de `bas` est FUN_0072eff0(instant source, durée source, marques source, durée de bas, marques de
	//    bas) ; une couche qui mène (sans meneuse, ou elle-même) et dont l'instant précédent +0x80 n'est pas négatif le
	//    fait passer par la même synchro ; l'octet +0x7c est posé (les comparaisons à 0,06 s de 0x731f48 à 0x73201b,
	//    par FUN_00432910, ne changent ni l'instant ni l'octet : il vaut toujours 1 à 0x732021) ;
	// 7. une couche qui mène et dont la phase demandée +0x88 n'est pas négative prend l'instant de FUN_0072e690(phase,
	//    durée, marque 0, marque 2), et +0x88 revient à −1 ;
	// 8. **les appuis** (0x732074 à 0x732160), pour une couche qui mène et dont l'instant précédent n'est pas négatif :
	//    la marque 0 passée entre l'instant précédent et le nouveau pose l'appui du pied 0 (FUN_005f3610(1)), la marque
	//    2 celui du pied 1 (FUN_005f3640(1)) ; « passée » : précédent ≤ marque ≤ nouveau, ou, quand la boucle a tourné
	//    (précédent > nouveau), précédent ≤ marque ou marque ≤ nouveau ;
	// 9. la place 0 prend l'enregistrement de `bas` (006b1b00 et 006b3c40 comptent ses références), l'instant, et les
	//    drapeaux 7 (joue, boucle, partie) ; `bas` devient l'entrée courante (+0x50) ;
	// 10. **la cadence** (0x73218e à 0x732243) : +0x8c prend la vitesse du piéton (+0x1c de M) ; pour une couche qui
	//    mène (et l'entier 0xbc7338, 1 dans l'exécutable), la vitesse de la place 0 est CMotionTrack::Cadence(vitesse,
	//    vitesse naturelle de bas (FUN_00730340), stat) ;
	// 11. avec `haut` : la place 1 prend son enregistrement (006b54a0), l'instant de la même synchro vers `haut`, les
	//    drapeaux 7 et la vitesse de la place 0 ; **le mélange +0x40** est FUN_0072f3b0(angle, seuil de bas, seuil de
	//    haut, 0, 1) borné à 0 et 1, le seuil de haut augmenté de 2π (0xbc7258) s'il ne dépasse pas celui de bas. Sans
	//    `haut` le mélange est 0 ;
	// 12. +0x80 prend l'instant de la place 0 ; une couche qui suit aurait ses deux vitesses à 0 si l'entier 0x20c905c
	//    n'était pas nul (zone non initialisée, supposé nul) ; puis FUN_006b26f0(dt) avance le poids et les deux places.
	// Ici l'enregistrement et la vitesse naturelle de chaque entrée sont portés par l'entrée (le jeu les cherche par
	// 00607610 et FUN_00730340 à chaque image) ; supposé : une entrée sans enregistrement rend vrai sans rien avancer.
	template<class F> bool AvancerMotion(CAnimTrack &c, float dt, CMotionPieton &p, F suite) {
		CAnimTrack *meneuse = nil;
		for(int32 r = 1; r >= 0 && meneuse == nil; r--){
			CAnimTrack *m = Meneuse(r);
			if(m != nil && m->synchro) meneuse = m;
		}
		if((posee[0] && couches[0].motion) || (posee[1] && couches[1].motion)) c.Direction(dt, p);
		if(meneuse != nil){ c.dir[0] = meneuse->dir[0]; c.dir[1] = meneuse->dir[1]; c.dir[2] = meneuse->dir[2]; }
		c.dir[2] = 0.0f;
		CCollisionBalayee::Normaliser(c.dir);
		float angle = CMotionTrack::Angle(c.dir, c.avant);
		if(c.nEntrees <= 0) return true;
		int32 bas = 0, haut = -1;
		bool autres = false;
		for(int32 i = 1; i < c.nEntrees && haut < 0; i++){
			autres = true;
			if(c.entrees[i].seuil > angle) haut = i; else bas = i;
		}
		if(autres && haut < 0) haut = 0;
		const CMotionEntree &eb = c.entrees[bas];
		if(eb.enr == nil) return true;
		float dBas = eb.enr->duree;
		float tSrc = c.places[0].t, d1 = dBas;
		const float *mSrc = eb.marques;
		if(meneuse != nil && meneuse->places[0].anim != nil && meneuse->courante >= 0){
			tSrc = meneuse->garde; d1 = meneuse->places[0].anim->duree; mSrc = meneuse->entrees[meneuse->courante].marques;
		}else if(c.courante >= 0 && c.places[0].anim != nil){
			d1 = c.places[0].anim->duree; mSrc = c.entrees[c.courante].marques;
		}
		float mSource[4] = { mSrc[0], mSrc[1], mSrc[2], mSrc[3] };   // l'entrée courante peut être celle qu'on réécrit
		bool mene = !(meneuse != nil && meneuse != &c);
		float ts = CMotionTrack::Synchro(tSrc, d1, mSource, dBas, eb.marques);
		if(c.precedent >= 0.0f && mene) c.precedent = CMotionTrack::Synchro(c.precedent, d1, mSource, dBas, eb.marques);
		c.synchro = true;
		if(mene && c.demande >= 0.0f){
			ts = CMotionTrack::InstantDeLaPhase(c.demande, dBas, eb.marques[0], eb.marques[2]);
			c.demande = -1.0f;
		}
		if(mene && c.precedent >= 0.0f)
			for(int32 pied = 0; pied < 2; pied++){
				float m = eb.marques[pied * 2];
				bool passe = c.precedent <= ts ? (c.precedent <= m && m <= ts) : (c.precedent <= m || m <= ts);
				if(passe) p.Appui(pied, true);
			}
		c.courante = bas;
		c.places[0].anim = eb.enr;
		c.places[0].t = ts;
		c.places[0].drapeaux = 7;
		c.vitesse = p.vitesse;
		if(mene) c.places[0].vitesse = CMotionTrack::Cadence(c.vitesse, eb.naturelle, p.stat);
		const CMotionEntree *eh = haut >= 0 ? &c.entrees[haut] : nil;
		if(eh != nil && eh->enr != nil){
			c.places[1].anim = eh->enr;
			c.places[1].t = CMotionTrack::Synchro(tSrc, d1, mSource, eh->enr->duree, eh->marques);
			c.places[1].drapeaux = 7;
			float sb = eb.seuil, sh = eh->seuil;
			if(!(sh > sb)) sh = sh + 6.2831855f;
			float mel = CMotionTrack::Remapper(angle, sb, sh, 0.0f, 1.0f);
			c.melange = mel < 0.0f ? 0.0f : mel > 1.0f ? 1.0f : mel;
			c.places[1].vitesse = c.places[0].vitesse;
		}else
			c.melange = 0.0f;
		c.precedent = c.places[0].t;
		return c.Avancer(dt, suite);
	}
	bool AvancerMotion(CAnimTrack &c, float dt, CMotionPieton &p) { return AvancerMotion(c, dt, p, [](int32, float){}); }

	// FUN_006c05b0 avec les couches de locomotion : les couches gardées dont le rang 6 de la table virtuelle rend vrai
	// (FUN_007ffc70 pour AM_MotionTrack) sont mises à jour par leur rang 3 (FUN_00731b90) avant la couche du rang ; elles
	// ne sont retirées que si l'entier 0x20c4b14 n'est pas nul (zone non initialisée, supposé nul) : ici jamais. La
	// couche du rang avance par sa table virtuelle : FUN_00731b90 pour une couche de locomotion, FUN_006b26f0 sinon.
	// Puis la meneuse de chaque rang relève son instant (FUN_004741b0 ; supposé à ce moment de l'image).
	void ParImage(float dt, CMotionPieton &p) {
		for(int32 r = 0; r < RANGS; r++){
			if(!posee[r]){ Purger(r); continue; }
			for(size_t i = 0; i < gardees[r].size(); i++)
				if(gardees[r][i].motion) AvancerMotion(gardees[r][i], dt, p);
			bool vit = couches[r].motion ? AvancerMotion(couches[r], dt, p) : couches[r].Avancer(dt);
			if(!vit && r > 0) Sortir(r, 0.0f);
		}
		ReleverMeneuses();
	}

	template<class F> void ParImage(float dt, F suite) {
		for(int32 r = 0; r < RANGS; r++){
			if(!posee[r]){ Purger(r); continue; }
			if(!couches[r].Avancer(dt, [&](int32 place, float x){ suite(r, place, x); }) && r > 0)
				Sortir(r, 0.0f);
		}
	}
	void ParImage(float dt) { ParImage(dt, [](int32, int32, float){}); }
};
