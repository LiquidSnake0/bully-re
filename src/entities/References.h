// Les références vers une entité : la liste, tenue par l'entité, des champs qui la désignent (le contact d'un balayage, le tireur
// d'un projectile, la cible d'un piéton). Recréé depuis bully.exe.
#pragma once

#include "../common.h"

// Un maillon (8 octets) : le suivant en +0x0, l'adresse du champ qui désigne l'entité en +0x4. Les maillons libres sont chaînés
// par leur +0x0 depuis le mot 0xc11d40. Supposé : la liste sert à remettre ces champs à nul quand l'entité disparaît (le
// parcours qui le fait n'est pas lu).
struct CReferenceEntite {
	CReferenceEntite *suivant = nullptr;
	void **champ = nullptr;
};

struct CReferencesEntite {
	// FUN_00450730(champ) (méthode d'une entité, lue à l'assembleur de 0x450730 à 0x450771, 66 octets, ret 4) : le champ est lié à
	// l'entité. La liste de l'entité (+0x118) est parcourue : un maillon qui porte déjà ce champ, et rien ne se fait. Sinon le
	// premier maillon libre (le mot 0xc11d40) sort de la liste des libres, passe en tête de celle de l'entité et reçoit le champ.
	// Sans maillon libre rien ne se fait, sans erreur : le champ n'est alors pas suivi. `premier` est le +0x118 de l'entité,
	// `libres` le mot 0xc11d40.
	static void Lier(CReferenceEntite **premier, CReferenceEntite **libres, void **champ) {
		for(CReferenceEntite *r = *premier; r; r = r->suivant) if(r->champ == champ) return;
		CReferenceEntite *r = *libres;
		if(r == nullptr) return;
		*libres = r->suivant;
		r->suivant = *premier;
		*premier = r;
		r->champ = champ;
	}
	// FUN_00450780(champ) (méthode d'une entité, lue à l'assembleur de 0x450780 à 0x4507b7, 56 octets, ret 4) : le champ est
	// délié. Le premier maillon de la liste de l'entité qui porte ce champ en sort (le lien qui menait à lui reçoit son suivant) et
	// passe en tête des maillons libres ; son +0x4 n'est pas effacé. Un champ absent : rien.
	static void Delier(CReferenceEntite **premier, CReferenceEntite **libres, void **champ) {
		CReferenceEntite **lien = premier;
		for(CReferenceEntite *r = *premier; r; lien = &r->suivant, r = r->suivant){
			if(r->champ != champ) continue;
			*lien = r->suivant;
			r->suivant = *libres;
			*libres = r;
			return;
		}
	}
	// FUN_00465680(entité, champ) et FUN_00465690(entité, champ) (cdecl, 15 octets chacune, lues de 0x465680 à 0x46568e et de
	// 0x465690 à 0x46569e) : les deux mêmes avec l'entité en premier argument, sans test du nul. Ce sont elles que le contact d'un
	// balayage appelle (CVolProjectile::ContactBalaye : Copier, LierEntite, Defaire), avec l'adresse de son champ +0xc.
	template<class E> static void LierEntite(E *entite, CReferenceEntite **libres, void **champ) { Lier(&entite->o118, libres, champ); }
	template<class E> static void DelierEntite(E *entite, CReferenceEntite **libres, void **champ) { Delier(&entite->o118, libres, champ); }
};
