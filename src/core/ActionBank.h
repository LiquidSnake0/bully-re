// L'arbre d'actions global de bully.exe, et le chemin d'un nœud (docs/cat.md, « Le chemin d'un nœud »).
//
// FUN_005f6f20 crée la banque « Global » sous la racine (DAT_01b649dc, que « / » seul désigne) ;
// FUN_005f6c50(« Act/Conv/1_01.act ») remplace l'extension par .cat, charge le fichier (FUN_005fb3f0)
// et greffe sa banque de tête sous Global : « 1_01 » devient /Global/1_01. FUN_005f59b0 résout un
// chemin : depuis la racine s'il commence par « / » ou « \ », un segment à la fois, « .. » remontant
// au parent, et à chaque étape l'enfant dont le nom haché (HashString) est celui du segment. FUN_004849a0 (ce que PedSetActionNode
// appelle) cherche le nœud, et s'il manque charge le fichier donné (s'il ne l'est pas déjà,
// FUN_005dbe60 le vérifie par le hachage de son nom sans dossier) puis cherche de nouveau.
#pragma once
#include "ActionTree.h"

class CActionBank
{
public:
	CActionBank(void);
	// FUN_005f6c50 : « Act/Conv/1_01.act » → 1_01.cat dans Act/Act.img, greffé sous /Global ; faux si
	// le fichier manque ou ne se lit pas. Un fichier déjà chargé n'est pas relu.
	bool Charger(const char *act);
	// FUN_005dbe60 : le fichier (par le hachage de son nom sans dossier) est chargé.
	bool EstCharge(const char *act) const;
	// FUN_005f59b0 : le nœud d'un chemin absolu (« /Global/1_01/Talking »), ou nil. Un chemin relatif
	// (sans « / » de tête) passe par un registre de noms que l'on n'a pas lu : nil.
	const CActionNode *Trouver(const char *chemin) const;
	// FUN_004849a0 : trouver, sinon charger `act` et retrouver ; vrai si le nœud existe à la fin.
	bool TrouverOuCharger(const char *chemin, const char *act);

	CActionNode racine;                      // DAT_01b649dc : sans nom, son seul enfant est la banque « Global »
	CActionNode *Global(void){ return &racine.enfants[0]; }
	const CActionNode *Global(void) const { return &racine.enfants[0]; }
	std::vector<std::string> fichiers;       // les .cat chargés, dans l'ordre
	std::vector<uint32> hachages;            // HashString de chaque nom de fichier sans dossier ni extension
	static std::string NomCat(const char *act);   // « Act/Conv/1_01.act » → « 1_01.cat »
};
