// Dictionnaires d'animations .HXD (Anim/*.HXD, Anim/hxds.dat), docs/hxd.md.
//
// bully.exe : FUN_006c1ab0 (un HXD par nom), qui lit par FUN_006b2d90 ;
// FUN_0042e640 charge Anim\hxds.dat et y lit un HXD par ligne de
// Config\Dat\PropHXDs.dat. Champs lus dans l'ordre du code, la version
// (1,09 / 1,11, constantes 0x941f74 / 0x941f70) décidant de quelques mots.
//
// Un HXD : version (f32), un mot, des masques d'os (nom de 32 octets, n
// poids), la liste des os (noms de 32 octets, HashString), les animations,
// les groupes (nom de 32 octets, taille), une donnée de 12 octets par os,
// n blocs de 0x30 octets, un mot final. Une animation : durée, un flottant,
// le nom « GROUPE\NOM » sur 64 octets, son HashString, des drapeaux, la
// taille de ses données dans le .agr plus 4, des événements, quatre mots de
// 16 bits, un vecteur.
#pragma once
#include "../common.h"
#include "../core/ActionTree.h"
#include <string>
#include <vector>

struct CHxdAnim {
	std::string nom;                   // « GROUPE\NOM »
	// En mémoire, un enregistrement de 0x40 octets (FUN_006b2d90) : durée à +0xc,
	// fondu à +0x10, hachage +0x14, drapeaux +0x18, taille +0x1c, groupe +0x20.
	// duree : la durée de jeu ; FUN_006be250 divise le temps par elle pour
	// lire les clés du .agr, stockées normalisées (HxdEtirer, docs/hxd.md).
	// fondu : le fondu par défaut (s), pris quand l'appelant en demande un négatif ;
	// 0,3 presque partout.
	float duree = 0, fondu = 0;
	uint32 hachage = 0;                // HashString(nom), ce que citent les pistes Animation (docs/cat.md)
	uint32 drapeaux = 0;
	uint32 taille = 0;                 // taille des données dans le .agr, plus 4
	uint32 groupe = 0;                 // l'indice du groupe dans la table de ce HXD (FUN_006bf0e0)
	int32 nEvenements = 0;
	uint16 w[4] = {0, 0, 0, 0};
	float v[3] = {0, 0, 0};
};
struct CHxdGroupe { std::string nom; uint32 taille = 0; };   // taille : celle du .agr, plus 4 par animation

class CHxdFile
{
public:
	float version = 0;
	uint32 mot = 0, motFinal = 0;
	std::vector<std::string> masques, os;
	std::vector<CHxdAnim> anims;
	std::vector<CHxdGroupe> groupes;
	uint32 lus = 0;                    // octets consommés

	bool Load(const uint8 *buf, uint32 n);
	// FUN_0068e380(squelette, nom) (cdecl, lue à l'assembleur de 0x68e380 à 0x68e3d7, 88 octets) : le rang de l'os qui porte
	// ce nom, ou −1. Le dictionnaire est le +0x10 du +0x94 du squelette (supposé : le HXD en mémoire) ; ses os sont comptés
	// en +0x5c et rangés en +0x60, 12 octets chacun, le haché du nom en +4. Le nom cherché est haché par HashString
	// (0x576d80, ActionHash) et le premier os du même haché l'emporte. Le binaire cherche ainsi « Jaw » et « Eyes » ; les
	// rangs des fiches FightCollisionButes (FUN_00572680, collision/ModelePieton.h) sont des rangs de cette table.
	int32 OsParNom(const char *nom) const {
		uint32 h = ActionHash(nom);
		for(int32 i = 0; i < (int32)os.size(); i++) if(ActionHash(os[i].c_str()) == h) return i;
		return -1;
	}
	// Par hachage, comme FUN_006b1c00 (qui accepte aussi un nom, haché à la volée).
	const CHxdAnim *Chercher(uint32 hachage) const {
		for(const CHxdAnim &a : anims) if(a.hachage == hachage) return &a;
		return nil;
	}
	// Le rattachement (FUN_006bf0e0, au chargement du groupe) : les animations
	// du groupe g sont, dans l'ordre des enregistrements du HXD, celles dont le
	// champ groupe vaut g ; le .agr se lit d'un trait, chacune occupant
	// taille − 4 octets. Rend l'indice de l'animation dans son .agr, et son
	// groupe, ou -1.
	int32 Indice(uint32 hachage, int32 *groupeTrouve = nil) const {
		for(size_t i = 0; i < anims.size(); i++) if(anims[i].hachage == hachage){
			int32 k = 0;
			for(size_t j = 0; j < i; j++) k += anims[j].groupe == anims[i].groupe;
			if(groupeTrouve) *groupeTrouve = (int32)anims[i].groupe;
			return k;
		}
		return -1;
	}
};
