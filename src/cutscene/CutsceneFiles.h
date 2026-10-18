// Les fichiers d'une cinématique dans Cuts/Cuts.img (docs/cinematiques.md), tels que
// CCutsceneMgr::LoadCutsceneData (FUN_006c4480) les lit :
//  - NOM.INF : le nombre d'objets, puis un nom par ligne ; un nom qui commence par « f_ » est le
//    jeu de poids faciaux de l'objet du même nom (sans le « f_ ») et passe en fin de liste ;
//  - NOM.SUB : « {1}{1}30.000 » (images par seconde) puis « {début}{fin}CLÉ » par sous-titre,
//    les instants en images, la clé dans Config/Text (lu par sscanf "{%d}{%d}%s") ;
//  - NOM.DAT : la piste de la caméra (FUN_004f86d0) : quatre sections finies par « ; », deux à six
//    colonnes (FUN_004f8200 : le champ de vision, puis une piste à zéro) et deux à douze
//    (FUN_004f8420 : la position de la caméra, puis sa cible), puis une liste de positions
//    (nombre, puis « x,y,z » par ligne), vide dans les 126 fichiers du jeu.
// Les animations des objets sont à côté : NOM_CHAR.agr, NOM_CAR.agr, NOM_CS.HXD… (docs/agr.md, docs/hxd.md).
#pragma once
#include "../common.h"
#include <string>
#include <vector>

struct CCutsceneInf {
	std::vector<std::string> objets;     // CS_PLAY, CS_TE_SECRETARY, CS_CAR… dans l'ordre du fichier
	std::vector<std::string> faciaux;    // les noms « f_… », tels qu'écrits
	// DAT_020c4b80[i] : l'objet i a un jeu de poids faciaux (un « f_ » + son nom est dans la liste).
	bool AFacial(size_t i) const;
	bool Load(const uint8 *data, uint32 size);
};

struct CCutsceneSub {
	float imagesParSeconde = 0;          // la première ligne, « {1}{1}30.000 »
	struct Ligne { int32 debut = 0, fin = 0; std::string cle; };   // 0x20 octets par entrée dans le moteur
	std::vector<Ligne> lignes;
	bool Load(const uint8 *data, uint32 size);
};

// Une clé d'une piste à six colonnes (0x18 octets) : l'instant, la valeur, puis quatre nombres
// (les tangentes d'entrée et de sortie, et deux que les fichiers laissent presque toujours à zéro).
struct CCutsceneCle6 { float t = 0, valeur = 0, a = 0, b = 0, c = 0, d = 0; };
// Une clé d'une piste à douze colonnes (0x30 octets) : l'instant, un point, deux tangentes, deux nombres.
struct CCutsceneCle12 { float t = 0; CVector pos, entree, sortie; float a = 0, b = 0; };

struct CCutsceneCamera {
	std::vector<CCutsceneCle6> champ;    // première section : le champ de vision (80 → 48,6 dans 1-1-1)
	std::vector<CCutsceneCle6> seconde;  // deuxième section : à zéro partout dans le jeu
	std::vector<CCutsceneCle12> position;
	std::vector<CCutsceneCle12> cible;
	std::vector<CVector> points;         // la cinquième section
	// Faux comme FUN_004f86d0 : une section dont le compte ne tombe pas sur un « ; », une ligne qui n'a
	// pas ses six ou douze nombres, une cinquième section qui n'a pas ses trois.
	bool Load(const uint8 *data, uint32 size);
	// La dernière clé des quatre pistes, en secondes.
	float Duree(void) const;
};

// Tout ce que Cuts.img garde d'une cinématique, par son nom (« 1-1-1 »).
class CCutsceneFiles
{
public:
	std::string nom;
	bool aObjets = false, aSousTitres = false, aCamera = false;   // le .INF, le .SUB, le .DAT existent et se lisent
	CCutsceneInf inf;
	CCutsceneSub sub;
	CCutsceneCamera camera;
	std::vector<std::string> animations; // les entrées « NOM_*.agr » et « NOM_*.hxd » de Cuts.dir

	// Ouvre Cuts\Cuts.img une fois pour toutes ; faux si l'image manque.
	static bool Ouvrir(void);
	static int32 Image(void){ return image; }
	// Lit une entrée de Cuts.img dans `octets` (taille arrondie au secteur) ; faux si elle manque.
	static bool Lire(const char *entree, std::vector<uint8> &octets);
	// Charge ce qui existe pour `nom` ; faux seulement si l'image manque.
	bool Charger(const char *nom);
private:
	static int32 image;
};
