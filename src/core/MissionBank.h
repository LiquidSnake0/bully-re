// La banque des missions : Act/Missions.act et les sous-arbres qu'il cite, chaque nœud qui
// porte une piste MissionData (docs/missions.md). Reconstruit FUN_006ac5d0 (chargement) et
// FUN_006ab7e0 (parcours) du gestionnaire de missions de bully.exe.
#pragma once
#include "../common.h"
#include <string>
#include <vector>

// Une entrée du tableau +0x4b8 du gestionnaire : la piste MissionData copiée, avec le nœud.
struct CMissionData {
	std::string banque;      // le fichier .cat d'où elle vient
	std::string nom;         // le nom du nœud en clair quand le fichier le donne (MissionGetName)
	uint32 hachage = 0;      // le nom haché, toujours rempli
	std::string titre;       // champ 24, titre de travail
	std::string script;      // champ 28, le script Lua ; vide ou « none.lua » pour les missions sans script
	std::string cle;         // champ 32, la clé du titre affiché
	bool afficherTitre = false;   // champ 36
	int type = 0;            // champ 40 : 0 histoire, 1 cours, 3 boulots et courses, 4 tutoriels, 5 mini-jeux
	bool Jouable(void) const { return !script.empty() && script != "none.lua"; }
};

class CMissionBank
{
public:
	std::vector<CMissionData> missions;   // dans l'ordre du parcours, comme +0x4b8
	std::vector<std::string> banques;     // les fichiers lus, Missions.cat puis ses références dans l'ordre

	// FUN_006ac5d0 : lit Missions.cat dans Act/Act.img, puis chaque sous-arbre cité par une référence.
	// Renvoie faux si l'image ou la racine manquent.
	bool Charger(void);
	// MissionGetIndex : par nom en clair (sans la casse) ou par hachage.
	const CMissionData *Trouver(const char *nom) const;
	int Index(const char *nom) const;
private:
	bool LireFichier(int32 image, const char *fichier);
};
