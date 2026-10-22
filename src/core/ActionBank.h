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
	// FUN_005f3bf0, le test de PedIsPlaying(ped, chemin, sous) : le nœud joué (+0x10 du contrôleur) est celui du
	// chemin ; avec `sous`, FUN_005f4f80 remonte aussi ses parents (emplacement +0x34) jusqu'à le trouver.
	// Faux si le chemin ne se résout pas ou si rien n'est joué.
	bool Joue(const CActionNode *courant, const char *chemin, bool sous) const;
	// La descente du contrôleur dans un arbre posé par PedSetActionTree (FUN_005dc040, puis l'emplacement +0x4c de
	// chaque nœud) : une banque (FUN_005f5490) teste ses conditions (FUN_005fb9b0, s'il en a : emplacement +0x50),
	// puis rend le premier de ses enfants, dans l'ordre, dont la descente aboutit ; un nœud jouable ou une feuille
	// (FUN_005f5500) se rend lui-même si ses conditions passent. `conditions` répond pour un nœud qui en porte
	// (vrai = elles passent) ; sans réponse, un nœud à conditions n'est pas pris. Une référence ('i', 'r') renvoie
	// à un autre fichier que l'on ne suit pas : elle n'aboutit pas. Rend nil si rien n'aboutit.
	typedef bool (*Conditions)(const CActionNode &n, void *contexte);
	const CActionNode *Choisir(const CActionNode *depuis, Conditions conditions = nil, void *contexte = nil) const;

	CActionNode racine;                      // DAT_01b649dc : sans nom, son seul enfant est la banque « Global »
	CActionNode *Global(void){ return &racine.enfants[0]; }
	const CActionNode *Global(void) const { return &racine.enfants[0]; }
	std::vector<std::string> fichiers;       // les .cat chargés, dans l'ordre
	std::vector<uint32> hachages;            // HashString de chaque nom de fichier sans dossier ni extension
	static std::string NomCat(const char *act);   // « Act/Conv/1_01.act » → « 1_01.cat »
	// FUN_0072ad00 : une piste « Butes » (réglages du jeu) trouvée par le HashString de son nom parmi les pistes d'un
	// fichier (Globals.cat pour AreaTransitionButes, TimeCycleButes…) ; FUN_0072ae00 les range au démarrage
	// (DAT_020c8910 pour AreaTransitionButes). Faux si le fichier ou la piste manque.
	static bool Butes(const char *nom, CActionTrack &piste, const char *cat = "Globals.cat");
	// FUN_005f6f20 : au démarrage, les 68 arbres de la liste PTR_s_Act_Globals_act_00b0a4f0 (Globals, Missions,
	// Player, HitTree, les armes…) chargés sous /Global par FUN_005f6c50. Rend le nombre de fichiers chargés.
	int ChargerGlobaux(void);
	static const char *const globaux[68];
	static float ButesReel(const CActionTrack &piste, uint16 position);   // un attribut de 4 octets lu comme un réel
};
