// Les groupes d'animations : les 550 .agr de Stream/World.img (docs/agr.md), chargés à la demande.
//
// Relevé dans bully.exe : un groupe se trouve par son nom, sans la casse (FUN_0042e200 parcourt les descripteurs de
// 0x3c octets, le nom en +4, et rend aussi son indice ; inconnu : 0 et -1). LoadAnimationGroup (0x5c08a0) le demande
// au streaming (FUN_00531ed0) puis charge tout ce qui est demandé tout de suite (CStreaming::LoadAllRequestedModels,
// FUN_00533d90) ; HasAnimGroupLoaded (0x5bf6e0) répond l'octet +4 de son entrée de streaming (1 : chargé). La condition
// AnimationGroupLoaded (FUN_006024d0) est vraie pour un groupe prêt (+0x34 du descripteur) ; sinon, si son octet +8 est
// posé, elle le demande (FUN_00531ed0, sans tout charger) et reste fausse jusqu'à ce que le streaming l'ait lu.
// Ici, faute de streaming, une demande est servie à l'image suivante.
#pragma once
#include "../common.h"
#include <map>
#include <string>

class CdImage;

class CAnimGroups
{
public:
	// Les noms des .agr du répertoire (sans l'extension) ; rend leur nombre.
	int Lire(const CdImage &img);
	bool Existe(const char *nom) const;
	// LoadAnimationGroup : demandé et chargé tout de suite. Faux si le groupe n'existe pas.
	bool Charger(const char *nom);
	// FUN_00531ed0 seul : demandé à l'image `image`, chargé à la suivante (Avancer). Faux si le groupe n'existe pas.
	bool Demander(const char *nom, uint32 image);
	// Une image du streaming : les demandes d'une image antérieure sont chargées. Rend le nombre de groupes chargés.
	int Avancer(uint32 image);
	// HasAnimGroupLoaded, et le +0x34 que lit AnimationGroupLoaded.
	bool EstCharge(const char *nom) const;
	// UnLoadAnimationGroup : le groupe n'est plus chargé (ni demandé).
	void Decharger(const char *nom);
	int Nombre(void) const { return (int)groupes.size(); }
private:
	enum { CONNU = 0, DEMANDE = 1, CHARGE = 2 };
	struct Etat { int etat = CONNU; uint32 image = 0; };
	std::map<std::string, Etat> groupes;   // par nom en minuscules
	static std::string Cle(const char *nom);
};
