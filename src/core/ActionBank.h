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
#include <map>
#include "ActionTree.h"
#include <memory>

// Ce qu'une condition de nœud d'action vaut pour un piéton : la classe (type, HashString de son nom) et ses mots
// (mots[i] = champ +4 * (i + 1) de l'objet, voir CActionTreeFile::Condition) ; 1 vrai, 0 faux, -1 classe pas lue.
// NOT, OR, False et ConditionGroup sont jugés par la banque elle-même.
// Le contexte d'un jugement (l'objet que FUN_005fb9b0 passe à chaque condition) : son piéton en +8, son contrôleur en
// +0x10, et en +0x34 le nœud dont on juge les conditions, que la descente pose (la condition ActionNodeRequest,
// ConditionIntentionActionNodeRequest FUN_005fd620, compare son nom à l'intention du piéton). L'outil les tient ici :
// `noeud` est posé par CActionBank::Juger, `controleur` par le contrôleur qui juge (nil : celui des actions du piéton).
struct CActionNode;
class CActionController;
struct CJugeAction {
	virtual int Juger(const CActionTreeFile &f, int32 decalage, uint32 type, const uint32 mots[6]) const = 0;
	// PedMePlaying sur le contrôleur d'actions du piéton (FUN_005f46a0, ecx = +0x258 du composant +0x2e0) : son nœud joué,
	// ou un parent, porte ce nom haché. La piste ActionNodeRequest s'en sert (FUN_006261f0). 0 par défaut.
	virtual int JoueNom(uint32 nom) const { (void)nom; return 0; }
	// Le piéton a une cible de combat (+0x538, FUN_0048bee0 : un piéton, vivant) : les pistes ManageTargets
	// (FUN_00627420) et CombatLoco (FUN_00625270) vivent tant qu'il en a une. Vrai par défaut.
	virtual bool ACible(void) const { return true; }
	mutable const CActionNode *noeud = nil;
	mutable const CActionController *controleur = nil;
	virtual ~CJugeAction(void) {}
};

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
	// PedMePlaying(ped, nom, sous) (0x5cb910 → 0x5f4840 → FUN_005f46a0) : par le nom seul (HashString), pas le chemin :
	// le nœud joué s'appelle ainsi, ou un de ses parents (FUN_005f4f30, toujours demandé par 0x5f4840). `sous`
	// regarde aussi les contrôleurs secondaires (+0x2c), que l'outil n'a pas.
	bool JoueNom(const CActionNode *courant, const char *nom) const;
	bool JoueHachage(const CActionNode *courant, uint32 h) const;   // la même chose, le nom déjà haché (TargetPlaying)
	// La descente du contrôleur dans un arbre posé par PedSetActionTree (FUN_005dc040, puis l'emplacement +0x4c de
	// chaque nœud) : une banque (FUN_005f5490) teste ses conditions (FUN_005fb9b0, s'il en a : emplacement +0x50),
	// puis rend le premier de ses enfants, dans l'ordre, dont la descente aboutit ; un nœud jouable ou une feuille
	// (FUN_005f5500) se rend lui-même si ses conditions passent. `conditions` répond pour un nœud qui en porte
	// (vrai = elles passent) ; sans réponse, un nœud à conditions n'est pas pris. Une référence ('i', 'r') renvoie
	// à un autre fichier que l'on ne suit pas : elle n'aboutit pas. Rend nil si rien n'aboutit.
	typedef bool (*Conditions)(const CActionNode &n, void *contexte);
	const CActionNode *Choisir(const CActionNode *depuis, Conditions conditions = nil, void *contexte = nil) const;

	// Les fichiers chargés restent en mémoire, comme le bloc que FUN_005fb3f0 alloue : les conditions et les pistes
	// d'un nœud ne sont que des décalages dans ses données. Fichier(n) : le fichier d'où vient le nœud, ou nil.
	struct Source { std::vector<uint8> donnees; CActionTreeFile fichier; };
	std::vector<std::shared_ptr<Source>> sources;
	const CActionTreeFile *Fichier(const CActionNode &n) const {
		return n.source >= 0 && n.source < (int)sources.size() ? &sources[n.source]->fichier : nil;
	}

	// FUN_005fb9b0 sur les conditions d'un nœud : la liste (ConditionListe.h), un NOT ou un OR par sa place, False
	// toujours faux, une ConditionGroup par son groupe (la seconde table de renvois), le reste par `juge`.
	// 1 vrai (aussi sans condition), 0 faux, -1 si une condition décisive n'est pas lue.
	int Juger(const CActionNode &n, const CJugeAction &juge) const;
	int JugerListe(const CActionTreeFile &f, const std::vector<int32> &liste, const CJugeAction &juge, int prof = 0) const;
	// Les ancêtres d'un nœud, la racine en tête et le nœud en dernier ; vide s'il n'est pas dans l'arbre.
	std::vector<const CActionNode *> Lignee(const CActionNode *n) const;
	// FUN_005f59b0 pour un chemin relatif au nœud qui porte une piste (la référence d'une Opportunity, d'un Execute,
	// d'une Sequence : « ./GroundStomp2 », « ../ », « ../../Attacks ») : « . » est ce nœud, « .. » son parent, un
	// autre segment l'enfant de ce nom haché. Un chemin qui commence par « / » part de la racine. Nil si un segment manque.
	const CActionNode *Relatif(const CActionNode *depuis, const char *chemin) const;
	// Le chemin d'un nœud de l'arbre (« /Global/TrainingPlayer/Default/Default »), pour les journaux ; vide s'il n'y est pas.
	std::string CheminDe(const CActionNode *n) const;
	// La descente avec les conditions jugées : un nœud est pris si Juger rend 1.
	const CActionNode *Choisir(const CActionNode *depuis, const CJugeAction &juge) const;

	CActionNode racine;                      // DAT_01b649dc : sans nom, son seul enfant est la banque « Global »
	CActionNode *Global(void){ return &racine.enfants[0]; }
	const CActionNode *Global(void) const { return &racine.enfants[0]; }
	std::vector<std::string> fichiers;       // les .cat chargés, dans l'ordre
	std::vector<uint32> hachages;            // HashString de chaque nom de fichier sans dossier ni extension
	bool Inclure(CActionNode &n, int prof = 0);    // les inclusions 'i' remplacées par l'arbre de leur fichier
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

	// Les durées des animations (CHxdAnim::duree, l'enregistrement HXD +0xc que FUN_0060ae00 lit par
	// FUN_006b1df0), par HashString(« GROUPE\NOM ») : ce que cite la position 24 d'une piste Animation. Rangées par
	// l'appelant depuis MAINPED.HXD ; -1 pour une animation inconnue.
	std::map<uint32, float> dureesAnimations;
	float DureeAnimation(uint32 h) const { auto it = dureesAnimations.find(h & 0x7fffffff); return it == dureesAnimations.end() ? -1.0f : it->second; }
};
