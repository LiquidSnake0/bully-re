// La banque des missions : Act/Missions.act et les sous-arbres qu'il cite, chaque nœud qui
// porte une piste MissionData (docs/missions.md). Reconstruit FUN_006ac5d0 (chargement) et
// FUN_006ab7e0 (parcours) du gestionnaire de missions de bully.exe.
#pragma once
#include "../common.h"
#include <string>
#include <vector>

// Une condition d'entrée d'un nœud de mission, telle que la fabrique FUN_0061a4f0 la construit sur
// l'enregistrement (docs/missions.md, « Les conditions ») : son type et ses champs +4 à +0x18.
struct CMissionCondition {
	int Genre(void) const;     // emplacement 2 de la table virtuelle : 0, ConditionNOT 1, ConditionOR 2
	uint32 type = 0;           // HashString du nom de classe : MissionCond, TimeCond, ConditionFalse…
	uint32 mots[6] = {0, 0, 0, 0, 0, 0};
};

// Ce que les conditions lisent de la partie : l'entrée de 6 octets de chaque mission (+0x688 du
// gestionnaire), dont le mot +2 compte les réussites (FUN_006ac910 l'incrémente), et l'heure du jeu
// (DAT_00bceb4b, DAT_00bceb4a).
struct CEtatMissions {
	std::vector<uint16> reussites;   // une par entrée de la banque
	int heure = 8, minute = 0;
	// La mission en cours (FUN_006aa630 : le second emplacement, +0x5a0, s'il joue, sinon le premier,
	// +0x4bc) et la mission de cours active (FUN_006a9ed0 : la première dont le bit 0 de l'état est posé
	// et dont le type, champ 40, vaut 1) ; -1 pour aucune.
	int courante = -1, coursCourant = -1;
	// Le jour du calendrier (+0x58 de l'objet 0x020c35e8, soit DAT_020c3640, que GetCurrentDay, 0x5c0c00,
	// rend) : 0 au démarrage (mémoire mise à zéro), puis posé entre 0 et 4 par FUN_0061d320.
	int jour = 0;
	int joursParCycle = 5;           // TimeCycleButes, position 24 (+0x18), dans Globals.cat
};

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
	bool lancerSeul = false; // champ 51 (+0x33) : FUN_006ad130 la lance dès qu'elle est activée (1_02, pas 1_01)
	bool rejouable = false;  // champ 44 (+0x2c) : FUN_006ad130 l'active encore après une réussite
	int conditions = 0;      // les conditions d'entrée du nœud (si MissionCond…) ; 0 = toujours activable
	std::vector<CMissionCondition> conds;   // ces conditions, lues
	std::string corona;      // MissionButtonTrigger (champ 24) de l'enfant « Active » : la liste de points où le joueur la lance
	float coronaDemiCote = 0;   // champ 28 : le demi-côté du pavé où le joueur doit être (FUN_006a8650 → FUN_005c2910) : 2 m pour 1_01, 1 m ailleurs
	int coronaMarqueur = 0;     // champ 32 : le type du marqueur dessiné au point (+0x20)
	bool coronaMarqueurVisible = false;   // champ 36 : l'octet +0x24 qui autorise le marqueur
	int coronaTaille = 0;       // champ 40, un entier : la taille passée au marqueur (+0x28) : 25 ou 23, 3 pour 1_01
	std::string coronaInvite;   // champ 48 : la clé de texte affichée dans le pavé (BUT_LAUNCH, BUT_MISSION)
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

	// Une condition, comme sa méthode d'évaluation (emplacement 1 de sa table virtuelle) : 1 vraie,
	// 0 fausse, -1 classe pas encore lue (le nom de la classe dans *classe quand on le connaît).
	//  MissionCond (FUN_006a7db0) : la mission nommée au champ +4 a réussi au moins une fois (mot +2
	//    de son entrée non nul) ; une mission inconnue de la banque rend faux. Deux noms désignent une
	//    mission du moment : « CurrentMission » (FUN_006aa630) et « CurrentClassMission » (FUN_006a9ed0).
	//  TimeCond (FUN_006a7380 → FUN_00425cf0) : l'heure du jeu est dans [+4 h +0xc min, +8 h +0x10 min[,
	//    l'intervalle pouvant passer minuit.
	//  DayCond (FUN_006a7480 → FUN_006a8b30) : le jour du calendrier vaut +4 ; avec l'octet +8, c'est le
	//    jour modulo le +0x18 de TimeCycleButes (DAT_020c88ac, la piste de Globals.cat : 5, le même
	//    diviseur que GetCurrentDay écrit en dur).
	//  ConditionFalse (FUN_006094f0) : toujours fausse.
	//  ConditionNOT et ConditionOR (FUN_00609b30) : vraies ; ce sont des opérateurs de la liste.
	int Evaluer(const CMissionCondition &c, const CEtatMissions &etat, const char **classe = nil) const;
	// FUN_005fb9b0, la liste de conditions du nœud que FUN_006ad130 fait tester (aucune : vraie). Chaque
	// condition a un genre (emplacement 2 de sa table : 0, NOT 1, OR 2). Le résultat part à vrai ; une
	// condition après une condition simple le remplace, après un NOT le remplace par son contraire ; un
	// résultat faux arrête la liste, sauf si l'élément suivant est un OR, qui est sauté ; après un OR
	// atteint avec un résultat vrai, les conditions sont sautées jusqu'au prochain opérateur. -1 si une
	// condition évaluée n'est pas d'une classe lue.
	int Activable(int index, const CEtatMissions &etat, const char **classe = nil) const;
private:
	bool LireFichier(int32 image, const char *fichier);
};
