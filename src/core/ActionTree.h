// Arbres d'actions compressés (Act/Act.img, 479 fichiers .cat), docs/cat.md.
//
// bully.exe : FUN_005fb3f0 (« CompressedActionTree::load() ») lit l'en-tête
// puis le reste du fichier ; FUN_005fb220 alloue les nœuds selon les comptes
// de l'en-tête ; FUN_005fb1a0 lit une banque, FUN_005fa9c0 un nœud jouable,
// FUN_005fafb0 la liste des enfants. Les noms sont des HashString (0x576d80).
//
// En-tête, huit mots : taille du fichier ; fin de l'en-tête (début des
// données) ; début des chaînes ; fin de l'arbre ; nombre de banques ; nombre
// de nœuds jouables (feuilles comprises) ; nombre de références ; nombre de
// feuilles. Suivent deux tables de renvois (pour chaque chaîne ou valeur,
// les décalages des données qui la citent), puis l'arbre, octet par octet :
//   nœud    : nom (mot : bit de poids fort = hachage, sinon décalage dans les
//             chaînes) ; n (octet) conditions (décalages dans les données) ;
//             pour un nœud jouable, m (octet) pistes (décalages) ;
//             enfants (mot de 16 bits) ;
//   enfant  : un caractère, 'b' banque, 'n' nœud jouable, 'l' feuille,
//             'i' ou 'r' référence (deux décalages dans les chaînes : nom,
//             chemin), suivi du nœud.
// Une condition commence par le HashString du nom de sa classe (« Not »,
// « Or », « ConditionGroup », « HavePOIOfType »…), que le moteur cherche dans
// un registre de fabriques (FUN_0061a4f0).
// Une piste (FUN_005fa7e0, créée à la demande par FUN_005f6100) : un mot de
// base de 16 bits, puis des attributs, chacun un mot w de 16 bits suivi de sa
// valeur : taille 1 << ((w >> 1) & 3) octets, position w >> 3 dans l'objet,
// un autre attribut suit tant que w & 1. Base non nulle : la piste hérite de
// celle placée base octets plus loin (copiée d'abord, puis écrasée). La
// valeur en position 0 est le type (HashString du nom de classe de piste :
// Animation, Opportunity, Sequence, SoundFX…). Une piste Animation porte en
// position 24 HashString("GROUPE\NOM") de l'animation, en 28 un masque d'os.
#pragma once
#include "../common.h"
#include <cstring>
#include <string>
#include <vector>

uint32 ActionHash(const char *s);      // HashString : majuscules, h = h * 0x83 + c, 31 bits

struct CActionNode {
	char genre = 'b';                  // 'b' banque, 'n' nœud jouable, 'l' feuille, 'i' / 'r' référence
	uint32 hachage = 0;                // le nom haché (toujours rempli)
	std::string nom;                   // le nom en clair quand le fichier le donne
	std::vector<int32> conditions;     // décalages des enregistrements dans les données
	std::vector<int32> pistes;
	std::string cible;                 // une référence : le chemin du nœud visé
	std::vector<CActionNode> enfants;
};

struct CActionRenvoi { uint32 valeur; std::vector<uint32> decalages; };

// Une piste lue, héritage compris : ses attributs par position dans l'objet.
struct CActionAttribut { uint16 position; uint8 taille; uint8 valeur[8]; uint32 decalage; };   // decalage : dans les données
struct CActionTrack {
	uint32 type = 0;                   // HashString du nom de classe
	std::vector<CActionAttribut> attributs;
	const CActionAttribut *Champ(uint16 position) const {
		for(const CActionAttribut &a : attributs) if(a.position == position) return &a;
		return nil;
	}
	uint32 Mot(uint16 position) const { const CActionAttribut *a = Champ(position); uint32 v = 0; if(a && a->taille >= 4) memcpy(&v, a->valeur, 4); return v; }
};

class CActionTreeFile
{
public:
	uint32 taille = 0, finEntete = 0, chaines = 0, finArbre = 0;
	int32 nBanques = 0, nJouables = 0, nReferences = 0, nFeuilles = 0;
	std::vector<CActionRenvoi> renvois, renvois2;
	CActionNode racine;
	int32 lus[4] = {0, 0, 0, 0};       // banques, jouables (sans les feuilles), références, feuilles lues

	bool Load(const uint8 *buf, uint32 n);
	// Le type d'une condition : le hachage du nom de sa classe.
	uint32 TypeCondition(int32 decalage) const;
	// Lit la piste à ce décalage des données (le tampon doit être encore là).
	bool Piste(int32 decalage, CActionTrack &t) const;
	// La chaîne citée par l'attribut placé à ce décalage des données, s'il en cite une.
	std::string ChaineCitee(uint32 decalage) const;
	bool ComptesJustes(void) const {
		return lus[0] == nBanques && lus[1] + lus[3] == nJouables && lus[2] == nReferences && lus[3] == nFeuilles;
	}
private:
	const uint8 *b = nil; uint32 n = 0; uint32 p = 0;
	bool Noeud(CActionNode &x, int prof);
	bool Enfants(CActionNode &x, int prof);
	bool Table(std::vector<CActionRenvoi> &t);
	bool Attributs(uint32 at, CActionTrack &t, int prof) const;
	std::string Chaine(uint32 off) const;
	uint32 Mot(uint32 at) const { uint32 v; memcpy(&v, b + at, 4); return v; }
};
