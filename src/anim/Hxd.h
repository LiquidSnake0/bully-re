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
#include <strings.h>
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
	// Les poids des masques d'os, un par os (36 dans MAINPED.HXD) : 1 l'os suit l'animation masquée, 0 non, entre les deux à
	// moitié (0,5 : « UBO_Hd_Sp » sur la colonne). Le masque d'une piste Animation est son champ 28, un nom (« RightArm »,
	// « UpperBody_ALL ») que FUN_00607340 cherche (docs/cat.md).
	std::vector<std::vector<float>> poidsMasques;
	// Le flottant qui suit le compte des poids dans le fichier : le +0x28 du masque en mémoire, le fondu de sortie d'une couche
	// qui porte ce masque quand sa piste n'en donne pas (FUN_006bd320). 0,3 s pour 44 des 46 masques de MAINPED.HXD, 1 s pour
	// « Conv » et « FlashLightAimHoriz ».
	std::vector<float> fonduMasques;
	// Le fondu de sortie d'une couche à l'arrêt de sa piste, comme FUN_006bd320 le choisit (lue de 0x6bd320 à 0x6bd463) : nul
	// pour le rang 0 ; sinon le champ 56 de la piste, ou, s'il vaut -1 (0x9009ac) et que la place porte un masque (+0x8), le
	// flottant +0x28 de ce masque ; un fondu de moins de 0,001 en valeur absolue (0x900c04) retire la place aussitôt (rendu
	// 0 ici), sinon il est écrit en +0x10 et +0x14 de la place, qui reste le temps de sortir.
	float FonduDeSortie(int32 rang, const std::string &masque, float champ56) const {
		if(rang <= 0) return 0.0f;
		float f = champ56;
		int32 r = MasqueDeCouche(masque);
		if(f == -1.0f && r >= 0 && r < (int32)fonduMasques.size()) f = fonduMasques[r];
		return (f < 0.0f ? -f : f) < 0.001f ? 0.0f : f;
	}
	// FUN_00607340(nom, rang) (méthode du HXD en mémoire, ret 8, lue à l'assembleur de 0x607340 à 0x607390, 83 octets) : les
	// masques sont comptés en +0xc et rangés en +0x8, 0x2c octets chacun, le nom en tête ; chacun est comparé au nom cherché par
	// la fonction du CRT 85f5af (supposé : _stricmp, sans la casse ; elle lit d'abord la locale 0x20e6558) ; le premier égal
	// l'emporte, son rang est écrit dans `rang` s'il est donné, son entrée est rendue ; nil sinon.
	// Ici : le rang du masque de ce nom, sans tenir compte de la casse (les arbres écrivent « FACIAL », le HXD « Facial »), ou -1.
	// Le masque d'une piste lancée sur une couche 1 à 6 (FUN_00612360, de 0x6123d0 à 0x6123f8) : celui de son nom, à défaut
	// ou sans nom le premier du fichier (FUN_00607320 rend le masque d'un rang, 0 hors du compte +0xc ; elle est appelée
	// avec le rang 0 : « default » dans MAINPED.HXD, tous les os au poids 1). -1 pour un fichier sans masque.
	int32 MasqueDeCouche(const std::string &nom) const {
		int32 r = nom.empty() ? -1 : Masque(nom.c_str());
		return r >= 0 ? r : masques.empty() ? -1 : 0;
	}
	int32 Masque(const char *nom) const {
		for(size_t i = 0; i < masques.size(); i++) if(strcasecmp(masques[i].c_str(), nom) == 0) return (int32)i;
		return -1;
	}
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
	// L'animation d'un haché comme le jeu la cherche (FUN_006b1c00, lue de 0x6b1c00 à 0x6b1c60, sur la table des
	// enregistrements +0x10 du fichier, que FUN_00607610 appelle pour une piste Animation) : **le premier enregistrement de
	// ce haché dont le premier mot (+0x0) n'est pas nul**, dans l'ordre du fichier ; ce mot est l'animation décodée que le
	// rattachement de son groupe y écrit (FUN_006bf0e0, à 0x6bf2f9 : le retour de 6be480) : entre plusieurs
	// enregistrements du même haché (172 hachés de MAINPED.HXD), c'est celui d'un groupe chargé qui est pris, et sans
	// aucun chargé la recherche rend 0 (la piste ne lance rien). `charge` dit si le groupe d'un enregistrement est chargé.
	// (Un haché nul avec un nom : le haché du nom, 576d80 ; non refait ici.)
	template<class F> const CHxdAnim *ChercherCharge(uint32 hachage, F charge) const {
		for(const CHxdAnim &a : anims) if(a.hachage == hachage && charge(a)) return &a;
		return nil;
	}
	// Le rang d'un enregistrement dans le .agr de son groupe (le rattachement, comme Indice).
	int32 IndiceDe(const CHxdAnim *e) const {
		int32 k = 0;
		for(const CHxdAnim &a : anims){ if(&a == e) return k; k += a.groupe == e->groupe; }
		return -1;
	}
};
