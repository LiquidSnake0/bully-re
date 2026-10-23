// Les pistes Opportunity d'un nœud d'action (OpportunityTrack, table virtuelle 0x938074), docs/cat.md.
//
// Relevé dans bully.exe, la mise à jour de la piste (emplacement 3, FUN_005f7d70) ; la position d'un attribut est son
// décalage dans l'objet :
//   +0x08 (8)  un octet commun à toutes les pistes : tant qu'elle vit, la piste retient son nœud (FUN_005f4230) ;
//   +0x20 (32) le chemin du nœud visé, relatif au nœud qui porte la piste (« ./GroundStomp2 », « ../ ») ;
//   +0x28 (40) la fermeture : tant qu'elle n'est pas prise, la piste s'arrête quand le temps du nœud dépasse ce champ
//              (s'il est positif) ;
//   +0x2c (44) la bascule : une fois prise, la piste s'arrête quand le temps du nœud atteint ce champ (s'il est ≥ 0) ;
//   +0x30 (48) un octet : le nœud visé peut être le nœud joué lui-même (une reprise) ;
//   +0x34 (52) la priorité de la demande ;
//   +0x38 (56) une période : au-delà de 1, la piste ne tente sa chance qu'au tirage de FUN_00465910 ;
//   +0x3c (60) ses propres conditions, jugées avant tout (FUN_005fb9b0 sur le contexte du piéton) : le champ est cité par
//              la seconde table de renvois, comme le +4 d'une ConditionGroup, et renvoie à une liste de conditions après
//              la fin de l'arbre (80 des 205 Opportunity de Player.cat en ont : ActionRequest, ActionNodePlayTime, False…) ;
//   +0x44, +0x48 (en mémoire) le nœud retenu et l'octet « prise ».
// OpportunityRandomLatch (table 0x9380a4 ; lancement FUN_005f7510, mise à jour FUN_005f7f80) fait la même chose, mais
// sa bascule (+0x54) est tirée au lancement entre +0x4c (76) et +0x50 (80) : rand() / RAND_MAX, comme le CRT.
// Chaque image, une piste pas encore prise tente sa chance (tirage, conditions), cherche le nœud visé et y descend
// (l'emplacement +0x4c du nœud, comme PedSetActionNode) : si la descente aboutit à un autre nœud que le nœud joué
// (ou au même quand +0x30 le permet), elle est prise. Une piste sans chemin est prise sans nœud. À l'arrêt
// (emplacement 4, FUN_005f7440), une piste prise dépose sa demande au contrôleur (FUN_005f3bc0) : la demande de plus
// forte priorité l'emporte.
#pragma once
#include "ActionBank.h"

// rand() du CRT de Visual C++ (graine × 214013 + 2531011, bits 16 à 30) : de 0 à 32767 (RAND_MAX).
int RandCrt(uint32 &graine);

// FUN_00465910 : le tirage d'une période pour un piéton. graine : le mot +0x10c du piéton ; image : le compteur
// d'images DAT_00c1a99c (CTimer, FUN_0045b810). u = (graine + image) mod 0xffff + 1 ; vrai si le plus petit bit posé
// de u vaut au moins la période : une chance sur la puissance de deux au-dessus de la période. Une période < 2 passe.
bool TirageOpportunite(uint32 periode, uint16 graine, uint32 image);

struct COpportunite {
	const CActionNode *porteur = nil;   // le nœud qui porte la piste
	std::string chemin;                 // +0x20
	float entree = 0.0f;                // +0x0c (12), commun à toutes les pistes
	float vie = -1.0f;                  // +0x10 (16)
	float fermeture = -1.0f;            // +0x28
	float bascule = 0.0f;               // +0x2c
	bool soiMeme = false;               // +0x30
	bool retient = false;               // +0x08, commun à toutes les pistes : vivante, elle retient le nœud (FUN_005f4230)
	int32 priorite = 0;                 // +0x34
	uint32 periode = 0;                 // +0x38
	uint32 conditions = 0;              // +0x3c, le mot du fichier
	std::vector<int32> listeConditions; // la liste à laquelle il renvoie (vide : aucune), dans le fichier du porteur
	bool loquet = false;                // OpportunityRandomLatch
	float loquetMin = 0.0f, loquetMax = 0.0f;   // +0x4c, +0x50 : la bascule tirée entre les deux (+0x54)
	bool prise = false;                 // +0x48
	const CActionNode *cible = nil;     // +0x44
	bool arretee = false;
	// Une piste du nœud lue comme une Opportunity (ou une piste qui en hérite) ; faux sinon.
	static bool Lire(const CActionTreeFile &f, const CActionNode &porteur, const CActionTrack &p, COpportunite &o);
	// Les Opportunity d'un nœud, dans l'ordre de ses pistes.
	static std::vector<COpportunite> DuNoeud(const CActionBank &b, const CActionNode &n);
};
