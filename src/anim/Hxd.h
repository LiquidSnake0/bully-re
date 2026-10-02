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
#include <string>
#include <vector>

struct CHxdAnim {
	std::string nom;                   // « GROUPE\NOM »
	float duree = 0, f1 = 0;           // f1 : 0,3 le plus souvent (un fondu ?)
	uint32 hachage = 0;                // HashString(nom), ce que citent les pistes Animation (docs/cat.md)
	uint32 drapeaux = 0;
	uint32 taille = 0;                 // taille des données dans le .agr, plus 4
	uint32 f5 = 0;
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
	const CHxdAnim *Chercher(uint32 hachage) const {
		for(const CHxdAnim &a : anims) if(a.hachage == hachage) return &a;
		return nil;
	}
};
