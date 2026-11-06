// Le démarrage des scripts du jeu, image par image (docs/lua.md, « La boucle des threads »).
// Recrée ce que fait le moteur : CreateLuaState (util.lua dans _G), l'objet script de main.lua
// (CreateNameSpace, 0x5d8d40), le thread gamemain (ThreadNameSpace, 0x5d8e40), puis
// LuaScript_Update (0x5da960) à chaque image : réveil des threads dont l'heure est passée,
// globale Alive, lua_resume, retrait des threads finis ou en erreur.
// Wait, CreateThread, TerminateThread et GetTimer sont réels ; le reste des 1 508 fonctions du
// moteur (docs/api-lua-tables.tsv, src/core/LuaApi.h) est en bouchons qui journalisent l'appel
// et répondent neutre. Tout autre nom est une globale de script : nil tant qu'un script ne l'a
// pas posée, exactement comme dans le jeu (un bouchon à la place rendait « if gMissionSuccess »
// vrai, et 1_01 ne pouvait pas finir).
// Les missions (docs/missions.md) : la table est lue dans la banque d'arbres d'actions
// (Missions.cat et ses sous-arbres) ; ForceStartMission suit la machine à états du moteur
// (FUN_006ad3e0) : script de la mission (FUN_005dbf90), MissionSetup jusqu'au bout
// (FUN_006acba0), main (FUN_006abf20), puis MissionCleanup (FUN_006abed0).
// Les cinq énumérations du moteur sont posées : POINTLIST, PATH, TRIGGER (les pools de DATLoad),
// MODELENUM (les noms de modèles des .idb, src/core/ModelEnum.h) et POI (les points d'intérêt des DATLoad).
// Les piétons des scripts (src/peds/ScriptPedPool.h) marchent, courent, se battent et meurent
// image par image : PedMoveToPoint, PedMoveToXYZ, PedStop, PedAttack, PedSetHealth, PedMakeAmbient.
// Les zones (src/core/AreaManager.h, docs/zones.md) : PlayerSetPos… demande un changement de zone, joué image par
// image comme FUN_00416e30 (fondu, script de zone retiré puis chargé avec son main, zone visible et joueur posés à
// l'arrivée) ; AreaGetVisible, AreaIsLoading, AreaRegisterAreaScript, AreaSignalAreaTransitionReadyToLoad, CameraFade réels.
// --nouvelle-partie : pas de --mission ; le gestionnaire de missions (FUN_006ad130, réduit) active les
// missions dont les conditions sont vraies (MissionCond, TimeCond, ConditionFalse lues, docs/missions.md),
// lance celle dont le joueur touche le corona, et celles qui se lancent seules (champ 51) : 1_02 après 1_01.
// Les cinématiques (docs/cinematiques.md) : LoadCutscene lit les fichiers de Cuts.img, StartCutscene les joue le temps
// de la piste caméra, sous-titres au journal ; --sans-cinematiques les saute (IsCutsceneLoaded faux).
//   BULLY_DATA=<racine du jeu> [LANCER_JOURNAL=lignes] [LANCER_CONTROLEURS=1] build/outils/lancer_lua [secondes] [script.lua [fonction]] [--mission NOM | --nouvelle-partie] [--sans-cinematiques] [--entrer DECLENCHEUR@s]... [--aller LISTE[,n]@s | --aller x,y,z@s]... [--zone N@s]... [--porte DT_NOM@s]... [--jouer /Global/CHEMIN[:Act/FICHIER.act]@s[,s2…]]... [--demander N@s]... [--sequence @s]... [--suivre PIETON[,d]@s]... [--viser PIETON@s]... [--arme ARME@s]... [--viser-objet NOM@s]... [--tirer @s[,s2…]]... [--ramasser @s[,s2…]]...
// Par défaut : 10 s de jeu, main.lua, gamemain.
#include "../src/core/FileMgr.h"
#include "../src/core/CdStream.h"
#include "../src/core/Horloge.h"
#include "../src/core/ActionTree.h"
#include "../src/core/MissionBank.h"
#include "../src/core/LuaApi.h"
#include "../src/core/ModelEnum.h"
#include "../src/core/DatPools.h"
#include "../src/core/AreaManager.h"
#include "../src/core/ActionBank.h"
#include "../src/core/ActionController.h"
#include "../src/peds/Armes.h"
#include "../src/peds/Patrouilles.h"
#include "../src/objects/ObjectData.h"
#include "../src/objects/ObjetsAnimes.h"
#include "../src/objects/Collectibles.h"
#include "../src/objects/Ramassages.h"
#include "../src/core/IplFile.h"
#include "../src/anim/Hxd.h"
#include "../src/anim/AnimGroups.h"
#include "../src/core/ConditionListe.h"
#include "../src/audio/SpeechEvents.h"
#include "../src/audio/SpeechBank.h"
#include "../src/cutscene/CutsceneFiles.h"
#include "../src/peds/ScriptPedPool.h"
#include "../src/peds/Clothing.h"
#include "../src/hud/HudComponents.h"
#include "../src/hud/EcranDeFin.h"
#include "../src/hud/ObjectifsMission.h"
#include "../src/hud/ButtonHistory.h"
#include "../src/hud/Marqueurs.h"
#include "../src/render/CameraMgr.h"
#include "../src/peds/PedStats.h"
#include "../src/core/ModelInfo.h"
#include "../tests/hote/Chemins.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <map>
#include <array>
#include <set>
#include <string>
#include <vector>
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

static const char *kImg = "Scripts\\Scripts.img";
static int32 image = -1;
static uint32 maintenant = 0;           // CTimer::m_snTimeInMilliseconds (0xc1a9b4)
static CHorloge horloge;                // ClockGet / ClockSet / ClockSetTickRate (src/core/Horloge.h)

// Un emplacement de thread (0x44 octets dans l'objet script, à partir de +0x48).
enum EtatThread { NOUVEAU = 0, EN_COURS = 1, ARRET_DEMANDE = 2, TUE = 3, FINI = 4 };
struct Thread {
	lua_State *co = nil;                 // +0x00
	int ref = LUA_NOREF;                 // +0x04, garde la coroutine en vie
	int etat = NOUVEAU;                  // +0x10
	int reveil = 0;                      // +0x1c, posé par Wait
	std::string fonction;                // +0x24
	int id = 0;
};
struct Script {
	std::string fichier;                 // +0x04
	std::vector<Thread> threads;         // [0] = premier niveau, puis CreateThread
	int courant = -1;                    // +0x114c
	bool fin = false;                    // +0x1150 : le script se termine (MissionCleanup)
	bool zone = false;                   // +0x2b09 : un script de zone, chargé par le changement de zone et retiré au suivant
};
// Les objets script vivants : [0] = main.lua, puis la mission en cours.
static std::vector<Script> scripts;
static int scriptCourant = -1;          // l'objet script dont un thread tourne en ce moment

// La banque des missions (src/core/MissionBank.h) : Missions.cat et ses sous-arbres, comme FUN_006ac5d0.
static CMissionBank banque;
// Les données de mission chargées par DATLoad (src/core/DatPools.h) : listes de points, trajets,
// déclencheurs, retrouvés par nom à travers POINTLIST, PATH et TRIGGER.
static CDatPools donnees;
static CPatrouilles patrouilles;        // DAT_00c2b034 : les rondes des trajets dont PATROLTYPE n'est pas -1
static void AmenerLesAllies(int zone, const CVector &arrivee);   // FUN_0048b970, plus bas
static bool BooleenArg(lua_State *L, int i);   // FUN_005d8f20, plus bas
static void Oublier(lua_State *L, int &ref);
// Les piétons des scripts (src/peds/ScriptPedPool.h) : le joueur en 0, PedCreatePoint & co.
static CScriptPedPool pietons;
// Les modèles par nom (src/core/ModelEnum.h) : MODELENUM._Taxicab → 286, ou -1.
static CModelEnum modeles;
// Les types des armes (WeaponGetType, FUN_0051b300 : le +0x18 du modelinfo, la fiche ObjectButes) et l'inventaire commun
// (ms_pPool654), src/peds/Armes. ChargerArmes les remplit au démarrage, comme CWeapon::RegisterCB (FUN_004da2d0).
struct CArmesOutil : CMondeArmes {
	std::map<int32, CTypeArme> types;
	CTypeArme defaut;              // DAT_020c88d8 : l'ObjectButes du nœud Globals de Globals.cat
	const CTypeArme &Type(int32 arme) const override {
		auto it = arme == -1 ? types.end() : types.find(arme);
		return it == types.end() ? defaut : it->second;
	}
};
static CArmesOutil armes;
static CInventaireArmes inventaire;
static int32 armeHud = -1;                 // HUDWeapons (le composant 20) +0xc0 : PoserArmeHud
static bool hudArmesVerrouille = false;    // son octet +0x13c : PlayerWeaponHudLock
static int32 ArmeParNom(const std::string &nom);   // FUN_0051ad50
static int32 MunitionsDe(int ped, int32 arme);     // FUN_00486130
static void PoserArmeHud(int32 a);                 // FUN_0055c6f0
static void AppliquerArmes(int ped, CActionController &c);
static void RendreArbreAvantArme(int ped);
static void LacherArme(int h);
// --- Les projectiles ---
// La fiche ProjectileButes d'une arme (son +0x80, que FUN_00728150 résout depuis 1 : Weapons/Projectiles/<+0x50>, sinon la fiche par
// défaut DAT_020c88c8, la ProjectileButes de Globals) : +0x48 (72) la vie (ms, un entier), +0x4c (76) et +0x50 (80) les dégâts sans et
// avec la charge, +0x54 (84) le nom de sa marque d'impact (« Decal_SlingShot »), +0x60 (96) et +0x64 (100) la vitesse sans et avec la
// charge (m par pas de 0,02 s), +0x80 (128) le facteur de la gravité ; des réels.
struct CTypeProjectile { bool lu = false; int32 vie = 0; float degatsMin = 0, degatsMax = 0, vitesseMin = 0.4f, vitesseMax = 0.4f, gravite = 1.0f;
                         std::string marque; };
static std::map<int32, CTypeProjectile> typesProjectiles;   // par modèle d'arme (le +0x80 de son type)
static CTypeProjectile projectileDefaut;
static const CTypeProjectile &
TypeProjectile(int32 arme)
{
	auto it = typesProjectiles.find(arme);
	return it == typesProjectiles.end() ? projectileDefaut : it->second;
}
static CTypeProjectile
LireTypeProjectile(const CActionTrack &t, const std::map<uint16, std::string> &chaines)
{
	CTypeProjectile a;
	a.lu = true;
	a.vie = (int32)t.Mot(72);
	a.degatsMin = CActionBank::ButesReel(t, 76); a.degatsMax = CActionBank::ButesReel(t, 80);
	a.vitesseMin = CActionBank::ButesReel(t, 96); a.vitesseMax = CActionBank::ButesReel(t, 100);
	a.gravite = CActionBank::ButesReel(t, 128);
	auto it = chaines.find(84); if(it != chaines.end()) a.marque = it->second;
	return a;
}

// Un projectile en vol (le moteur crée un CProjectile, un CPhysical de 0x24c octets : FUN_004d4c40 → FUN_004d24f0, FUN_004d2160 ;
// sa table, 0xc3a068, est celle que lit DetectProjectile). L'outil n'en joue pas la physique : un point qui suit sa vitesse et la
// gravité à chaque pas (ms_fTimeStep, 0,02 s), jusqu'à la fin de sa vie (+0x48) ou un contact.
struct CProjectileOutil { bool actif = false; int32 modele = -1; int tireur = -1; CVector pos, vitesse; float gravite = 0, degats = 0;
                          uint32 naissance = 0, vie = 0; };
static std::vector<CProjectileOutil> projectiles;
// Les marques d'impact (les decals : FUN_00560f10 compte celles d'un genre dans un déclencheur, ObjectNumProjectileImpacts) :
// le nom de la marque de la fiche du projectile et le point du contact.
// La table des marques a 32 places de 16 octets (la position, le genre en mot +0xc) : l'outil la tient en anneau, la plus vieille
// cédant sa place (supposé).
struct CMarque { std::string nom; CVector pos; };
static std::vector<CMarque> marques;
static size_t marqueSuivante = 0;
static void
PoserMarque(const std::string &nom, const CVector &pos)
{
	if(nom.empty()) return;
	if(marques.size() < 32) marques.push_back({nom, pos});
	else{ marques[marqueSuivante] = {nom, pos}; marqueSuivante = (marqueSuivante + 1) % 32; }
}

// --- Les objets cassables (CObject : ms_pObjectPool, 275 × 0x22c ; docs/pools.md) ---
// Supposé : les entrées spec des .ipb dont le modèle a une ligne de object.dat sont des CObject (le lecteur 0x433470 passe l'entrée
// au chargeur, son emplacement +0x18, non lu). D'un CObject, l'outil garde le modèle (+0x10e), le nom haché de l'instance (+0x228),
// la position, ses points de vie (HitPoints, les 15 bits bas du +0 de sa ligne de object.dat), le bit 6 de l'octet +0x209 (posé par
// ObjectToggleVulnerability : il écarte la destruction, FUN_004d0300) et celui qui l'a détruit (+0x224, PAnimDestroyedByPed).
// Sa poignée (ce que le moteur passe aux scripts : l'octet du pool + l'indice × 0x100, FUN_005b3530) : l'indice × 0x100 + 1 (supposé).
struct CObjetOutil { int32 modele = -1; uint32 nom = 0; CVector pos; int32 sante = 0; bool invulnerable = false, detruit = false;
                     bool parArmeSeule = false, verrouillable = false; int destructeur = -1; };
static std::vector<CObjetOutil> objets;
static int journalRestant = 400;   // les lignes de journal qui restent (LANCER_JOURNAL)
static CObjetsAnimes objetsAnimes;   // les objets animés des déclencheurs : le pool DAT_00c0f608 (src/objects/ObjetsAnimes)
// Le contrôleur d'actions de chaque objet animé (+0x1bc), par indice dans le pool : figé (src/core/ActionController.h).
static std::map<int, CActionController> controleursObjets;
static int PoigneeObjet(size_t i){ return (int)(i << 8) | 1; }
static CObjetOutil *ObjetParPoignee(int h){ size_t i = (size_t)h >> 8; return h >= 0 && (h & 0xff) == 1 && i < objets.size() ? &objets[i] : nil; }

// Les gestionnaires des événements par haché (RegisterHashEventHandler 0x5bbd80) : une table d'entrées de 0x24 octets (le haché,
// huit cases de rappel), dix en place (DAT_00cfada8), puis le tas (DAT_00cfaf10) que SetNumberOfHandledHashEventObjects agrandit ;
// leur nombre DAT_00cfaf18. FUN_005b3ee0 ajoute une entrée (s'il reste de la place), FUN_005b3830 rend sa case, FUN_005b3b80 la vide.
struct CGestionnaireHache { uint32 nom; int rappels[8]; };
static std::vector<CGestionnaireHache> gestionnairesHaches;
static size_t capaciteHaches = 10;
static int objetVise = -1;   // l'objet que le joueur vise (--viser-objet), sa poignée ; -1 aucun

// --- Les ramassages (src/objects/Ramassages : CPickups, 300 entrées de 0x48 octets dès 0xc0a100) ---
// L'outil note en plus l'objet script qui a créé chacun (FUN_005db7e0, FUN_005d9760 : il est retiré avec la mission).
static CRamassages ramassages;
static CObjetsJoueur objetsJoueur;   // les objets du joueur (DAT_00cf0060, src/peds/Armes)
static CCollectibles collectibles;   // les collectibles (CCollectiblesMgr, 0xbceb50, src/objects/Collectibles)
static CMarqueurs marqueurs;   // les marqueurs du radar (CRadar, DAT_00c674cc, src/hud/Marqueurs)
static std::vector<std::pair<int32, int32>> marqueursDuScript;   // la poignée, le script qui l'a posé
// Supposé aussi : le joueur de l'outil naît en (0, 0, 0), dans FootFieldTrig (le terrain de football, dont main.lua suit
// l'entrée et la sortie du joueur dans la table 0), quand le jeu le crée au point de la sauvegarde ; il n'est suivi qu'une fois
// posé par le gestionnaire de zones (les déclencheurs, la population des collectibles).
static bool joueurPose = false;

// Les collectibles des placements : FUN_00438790 (le gestionnaire des entrées inst des .ipb, lu dans l'index) cherche le haché de
// l'instance (+0x74) parmi les collectibles (FUN_00426270) ; un collectible pris (FUN_004261f0) ou indisponible (FUN_00426200) n'est
// pas posé, sinon il devient un objet fictif (CDummyObject de 0x128 octets, FUN_00465250, FUN_004cf0b0) à la position de l'entrée, sa
// zone (+0x111) arrondie de +0x44, son haché en +0x124 et son placement en +0x110. Ce gestionnaire tourne quand CIplStore charge le
// placement autour du joueur (FUN_00439220) ; l'outil, qui lit les 85 .ipb une fois au départ sans les diffuser, garde les entrées
// collectibles et les juge à chaque image comme si leur placement était chargé (supposé).
struct CCollectiblePose { int32 modele = -1; CVector pos; int8 zone = 0; uint32 hache = 0; uint8 placement = 0; bool converti = false; };
static std::vector<CCollectiblePose> collectiblesPoses;
static uint8 placementCourant = 0;

// Le nom d'un modèle d'arme ou d'objet (le +4 de son modelinfo est son HashString), nil sinon.
static const std::string *
NomDArme(int32 a)
{
	auto it = modeles.armes.find(a);
	if(it != modeles.armes.end()) return &it->second;
	it = modeles.objets.find(a);
	return it == modeles.objets.end() ? nil : &it->second;
}
// Le gestionnaire de zones (src/core/AreaManager.h, docs/zones.md) : la zone visible (DAT_00bd1008, ce que
// AreaGetVisible répond), les scripts de zone (AreaRegisterAreaScript) et le changement de zone du joueur,
// demandé par PlayerSetPos… et joué image par image comme FUN_00416e30 : fondu au noir, scripts de zone
// retirés puis celui de la nouvelle zone chargé avec son main, zone visible et joueur posés à l'arrivée.
static CAreaManager zones;
static CCameraMgr cameras;   // le gestionnaire de caméras (0xc3cc68) : le mode 1 au démarrage (FUN_004f35d0)
static CHudComponents hud;   // DAT_00c674a0 : les 48 composants du HUD, tous cachés au démarrage (FUN_0053cc70)
// Le fondu de la caméra (CameraFade, 0x4f1980), réduit à ce que la machine des zones et les scripts en
// attendent : une durée, un sens, et « fini ». Les durées par défaut sont celles d'AreaTransitionButes (DAT_020c8910,
// lue dans Globals.cat au démarrage) : +0x18 pour le fondu au noir (0,65 s), +0x1c pour le retour (0,5 s) ;
// CameraFade(-1, sens) les prend (0x5b8ea0), le changement de zone aussi (FUN_00416e30, états 2 et 0xb).
static float fonduNoirMs = 650, fonduRetourMs = 500;
// L'arbre d'actions global (src/core/ActionBank.h) : PedSetActionNode(ped, « /Global/1_01/Talking »,
// « Act/Conv/1_01.act ») cherche le nœud et charge le fichier au besoin (FUN_004849a0) ; le nœud n'est pas
// joué (pas d'animation ici), mais le script reçoit la vraie réponse : le nœud existe ou non.
static CActionBank arbres;
// Par piéton, son contrôleur d'actions (src/core/ActionController.h) : l'arbre posé par PedSetActionTree, le nœud
// joué (+0x10) que PedSetActionNode pose, que PedIsPlaying compare, et qui avance à chaque image.
static std::map<int, CActionController> controleurs;
// Le chemin d'un nœud pour le journal, vu du contrôleur qui le joue : la clé de son contexte choisit le parent des arbres
// montés (DAT_00b0a604, FUN_005f6290).
static std::string CheminVu(const CActionController &c, const CActionNode *n);
static std::map<int, CActionController> controleursIA;   // le contrôleur de l'IA (+0x290 du composant +0x2e0), PedSetAITree
static std::string
CheminVu(const CActionController &c, const CActionNode *n)
{
	uint32 avant = arbres.cle;
	if(c.Cle() != 0xffffffffu) arbres.cle = c.Cle();
	std::string r = arbres.CheminDe(n);
	arbres.cle = avant;
	return r;
}
static int journalIA = 80;   // les passages du contrôleur de l'IA journalisés (les premiers)
// Les groupes d'animations (src/anim/AnimGroups.h) : les .agr de Stream/World.img, que les scripts chargent
// (LoadAnimationGroup) et que la condition AnimationGroupLoaded demande ; une demande est servie à l'image suivante.
static CAnimGroups groupesAnim;
static uint32 imageCourante = 0;      // le compteur d'images de la boucle (pour les demandes de groupes)
// Les lignes que les pistes SpeechTrack des arbres retiennent (la tenue +0x28 : cinq places en 0x1b655d0, le rappel
// 0x619550) : préchargées à l'image suivante (l'outil n'a pas de diffusion), jouées quand une piste SpeechStartTrack
// du piéton les lâche, ou à l'échéance selon la tenue (1 : jouée, 2 : annulée). Une nouvelle ligne retenue par le même
// piéton annule l'ancienne (FUN_005a5880).
struct ParoleTenue { int ped, evenement, ligne, tenue; uint32 image, echeance; };
static std::vector<ParoleTenue> parolesTenues;
static const CActionNode *
NoeudJoue(int ped)
{
	auto it = controleurs.find(ped);
	return it == controleurs.end() ? nil : it->second.joue;
}
static std::map<uint32, int> nonLues;               // les classes de conditions pas encore lues, jugées -1 (le bilan de fin)
// Le chapitre en cours (l'octet DAT_00a147e0 : 0 au démarrage), que ChapterSet (0x5c0b70 → FUN_00454470) pose et que
// ChapterGet (0x5c0ba0) rend ; la saison de chaque chapitre (DAT_00a147ec : 1, 1, 2, 3, 3, 3, 0 ; le chapitre 1 est
// l'automne, timecycF, le 3 l'hiver) ; et les zones « dehors » (l'octet par zone de 0x901dd0, FUN_004301d0 : 0, 1, 22,
// 31, 42, 43, 64, 65, 66), toutes lues dans bully.exe.
static uint8 chapitre = 0;
static uint8 etatPhysiqueJoueur = 0;   // DAT_00c1af4c : 0, 1, 2 (fatigué) ; PlayerGetPhysicalState (0x5d1ee0)
static const int32 kSaisonDuChapitre[7] = {1, 1, 2, 3, 3, 3, 0};
static bool ZoneDehors(int z){ static const int d[] = {0, 1, 22, 31, 42, 43, 64, 65, 66}; for(int x : d) if(x == z) return true; return false; }
static lua_State *etatLua = nil;        // pour LuaCond : la fonction du script de mission appelée par le contrôleur
// LuaCond (FUN_006a74d0) : la fonction +0x10 du script +0xc, appelée dans son espace de noms ; -1 si rien n'est appelable.
static int AppelLuaCond(const std::string &script, const std::string &fonction);
// OnMission (ConditionOnMission 0x92e6f0, FUN_005fc940) : la mission en cours a le nom haché +4 ; +4 nul : une mission en cours.
static int SurMission(uint32 hachage);
// MissionCond (CMissionCond 0x94173c, FUN_006a7db0) dans un arbre d'actions : la même classe que dans Missions.cat (le
// créateur 0x63a140 → FUN_006a7d50) ; la mission +4 a au moins une réussite (CMissionBank::Evaluer).
// MissionUnlocked (CMissionUnlockedCond 0x941778, FUN_006a72a0) : la mission +4 a réussi ou a été forcée (MissionForceCompleted).
static int ConditionMission(uint32 type, const uint32 mots[6]);
// PedIsInOutfit (ConditionPedIsInOutfit 0x936c50, FUN_005fbf70 : ecx = 0x20c5f70, puis FUN_006ca890(+4)) : la tenue que le
// joueur porte a le nom haché +4, quel que soit le piéton jugé.
static int ConditionTenue(uint32 nom);
// --demander N@s : une ActionRequest du joueur (la manette) en attente jusqu'à cet instant (ms).
static std::map<uint32, uint32> demandes;
// L'état des 47 boutons bruts des autres piétons (+0x20 du composant +0x2e0, 12 octets par bouton : l'octet d'état en +0),
// que les pistes ActionRequest de leur arbre de l'IA appuient (FUN_005f3930 : 3 lâché → 8, 2 appuyé et tenu → 3, sinon
// appuyé → 1) et que FUN_005f3720 fait vieillir à chaque image (le bit 0 tombe : 3 → 2, 1 → 0 ; 8 → 0).
enum { BOUTONS_PIETON = 47 };
static std::map<int, std::array<unsigned char, BOUTONS_PIETON>> boutonsPietons;
static std::map<int, uint32> instantsParade;   // +0x4d0 : l'instant du dernier appui du bouton 12 (FUN_006260d0)
static void
AppuyerBouton(int ped, int32 bouton, int32 mode)   // FUN_005f3930
{
	if(bouton < 0 || bouton >= BOUTONS_PIETON) return;
	auto it = boutonsPietons.find(ped);
	if(it == boutonsPietons.end()) it = boutonsPietons.emplace(ped, std::array<unsigned char, BOUTONS_PIETON>{}).first;
	it->second[bouton] = mode == 3 ? 8 : mode == 2 ? 3 : 1;
}
static void
VieillirBoutons(void)   // FUN_005f3720
{
	for(auto &e : boutonsPietons)
		for(unsigned char &b : e.second){
			if(b & 1) b &= 2;
			if((b >> 3 & 1) && b != 0) b = 0;
		}
}

// Les conditions des nœuds d'action pour un piéton (FUN_005fb9b0 les juge une à une ; la banque fait NOT, OR, False
// et ConditionGroup). Champs relevés dans les enregistrements (mots[i] = champ +4 * (i + 1) de l'objet) :
//   WeaponEquiped        +4 : le nom de l'arme (« unarmed », « any », « bat »…), cité par la première table ;
//   ActionRequest        (ConditionActionIntentionActionRequest, FUN_005fd570) +4 : le bouton brut, +8 : le mode
//                        (3 : bouton lâché, 2 : bit 0 ou 1, sinon bit 0), +0xc / +0x10 : un seuil et son comparateur
//                        sur la durée tenue, +0x14 : si non nul, seule la durée compte ;
//   ActionNodePlayTime   (FUN_005fb940) +4 : un temps (s), +8 : le comparateur de FUN_005fb750, sur le temps du nœud ;
//   LuaCond              +0x10 : « mission », +0x14 : le nom de la fonction du script (F_PlayerGroundKicks…) ;
//   TargetPlaying        +4 : un nom de nœud haché (RollAway…), +8 : 1 ; deux champs, l'enregistrement suivant
//                        commence après. La cible est celle que PedLockTarget a verrouillée ; vrai si son nœud joué,
//                        ou un de ses parents, porte ce nom (comme PedMePlaying).
//   MePlaying            +4 : un nom de nœud haché, +8 : 1 ; le nœud joué par le piéton lui-même, ou un de ses
//                        parents, porte ce nom (FUN_005f46a0, comme PedMePlaying) ;
//   ConditionFlag        +4 : un nom haché (GROUNDHITHEAVY, ITCHVIRUS…) : le drapeau du dernier coup reçu, que la piste
//                        Collision de l'attaquant porte en +0x2c (44) et passe à la méthode du coup (slot 15 de CPed,
//                        FUN_0047b810) ;
//   HitLocation          +4, +8 : la zone touchée. L'outil n'a pas les sphères du corps : supposé vrai tant qu'un
//                        coup est en cours pour ce piéton.
// L'outil n'a pas d'armes : seul « unarmed » est vrai. Une classe non lue rend -1 : la branche n'est pas prise.
// Le dernier coup reçu par un piéton : le relevé +0x408 de son contrôleur (PedGetLastHitWeapon, 0x474a90, le rend),
// rempli par la méthode du coup (slot 15 de CPed, FUN_0047b810) : +0x40c l'attaquant, +0x440 (+0x38) les dégâts,
// +0x46c le drapeau haché (+0x2c de la piste Collision) ; +0x434 (+0x2c) le temps depuis le coup, remis à 0 par le
// coup (FUN_00472b70, l'ancienne valeur passant en +0x438, +0x30), puis augmenté du pas à chaque image tant qu'il ne
// vaut pas -1 (FUN_004850b0). L'outil tient le drapeau une seconde (ConditionFlag, HitLocation).
// L'arme du coup (+0x5e du relevé, +0x466 du composant : un mot, -1 à mains nues) et son genre (+0x60, +0x468), les 5e et 19e arguments
// de la méthode du coup : un coup de piste Collision porte l'arme que l'attaquant tient (-1 sans arme), un projectile la sienne (le
// genre d'un coup de projectile n'est pas lu : 0).
struct CoupRecu { uint32 drapeau = 0; float jusqua = -1.0f; float degats = 0.0f; int attaquant = -1; float instant = -1.0f, precedent = -1.0f;
                  int32 arme = -1, genre = 0; };
static std::map<int, CoupRecu> coups;
// Un piéton qui disparaît emporte ses contrôleurs (le composant +0x2e0, libéré avec le CPed) : une entrée du pool reprise
// plus tard (PedCreatePoint) part de contrôleurs neufs.
static void OublierSuivi(int h);   // la piste FollowPath et les rappels de son objectif
static void
OublierControleurs(int h)
{
	controleurs.erase(h);
	controleursIA.erase(h);
	coups.erase(h);
	OublierSuivi(h);
	inventaire.Vider(h);   // supposé : l'inventaire (+0x1c4) part avec le CPed, ses entrées du pool avec lui
}

static float facteurCoupsJoueur = 1.0f;   // PlayerButes +0xac (172) pour l'état physique 0 (FUN_0047b810), lu au démarrage
static std::map<int, std::pair<int, uint32>> partenaires;   // le piéton qui nous a fait jouer un nœud (PlayOnTarget), jusqu'à quand (ms)
// L'intention de chaque piéton pour l'image (+0x254 du composant +0x2e0) : le nom qu'une piste ActionNodeRequest de son
// arbre de l'IA y pose (FUN_005f35e0), lu par la condition ActionNodeRequest de son arbre d'actions, effacé en fin d'image
// avec l'état des boutons (FUN_005f3720).
static std::map<int, uint32> intentionsPietons;
// Les gestionnaires des événements des déclencheurs (RegisterTriggerEventHandler, src/peds/ScriptPedPool.h) et les
// déclencheurs qui ne suivent que le joueur (le bit 2 de +0x3f, AreaSetTriggerMonitoringRules).
static CGestionnairesDeclencheurs declencheursEv;
static std::set<int> declencheursJoueurSeul;
static std::set<int> choisis;   // les piétons dont FUN_00499340 a déjà rabattu l'octet +0x1de4
static int TirerVue(void *);
// AIButes (Globals.cat, DAT_020c889c, FUN_0072ae00) : +0x18 à +0x20 la capacité des trois zones de combat ; +0x28 à
// +0x38 les rayons des zones (0,9 / 1,5 / 3 / 6 / 8 m) ; +0x54 la portée et +0x58 le cosinus du cône d'attaque.
static CActionTrack butesIA;
static float
ButeIA(uint16 position, float defaut)
{
	return butesIA.Champ(position) ? CActionBank::ButesReel(butesIA, position) : defaut;
}
static uint32 graineIA = 7;   // le rand() du CRT des conditions Random et WeightedRandom (l'outil a sa propre graine)
static int TirerCrt(void *){ return RandCrt(graineIA); }   // le même rand(), pour le pool des piétons (les jetons)
// La cible d'un piéton (PedGetTargetPed, FUN_00496fb0) : pour un autre que le joueur, +0x538 (dans l'outil, la cible
// verrouillée : PedLockTarget et FUN_00497d20 la posent) ; la cible forcée +0xbb4 n'est pas suivie. -1 sans cible.
static int
CibleDe(int ped)
{
	const CScriptPed *p = pietons.Acceder(ped);
	if(p == nil) return -1;
	const CScriptPed *c = pietons.Acceder(p->verrou);
	return c && !c->mort ? p->verrou : -1;
}
static float
DistancePietons(const CScriptPed *a, const CScriptPed *b)
{
	float dx = b->pos.x - a->pos.x, dy = b->pos.y - a->pos.y, dz = b->pos.z - a->pos.z;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}
// EnterNIS (0x5bb3c0) et ExitNIS (0x5bb400) : l'octet DAT_00cfb340 (un NIS en cours), puis chaque véhicule du pool
// (DAT_00c0f5f4) arrêté ou rendu (FUN_004cb3e0(1) / (0)) ; l'outil n'a pas de véhicules.
static bool nisEnCours = false;
// PedSetGlobalAttitude_Rumble(ambiant, mission) (0x5c6bd0) : DAT_00c2afa0 et DAT_00c2afa1 (FUN_005d8fa0, faux par défaut).
static bool rumbleAmbiant = false, rumbleMission = false;
static bool CinematiqueEnCours(void);   // DAT_020c5be4
static bool ParoleEnCours(int ped);     // FUN_005a27b0 : une ligne du piéton (ou, à -1, une ligne quelconque) se joue
// Le nom d'un modèle par son indice (le HashString de ce nom est le +4 du modelinfo) ; vide s'il est inconnu.
static std::string
NomDeModele(int32 m)
{
	static std::map<int32, std::string> parIndice;
	if(parIndice.empty()) for(const auto &e : modeles.parNom) parIndice.emplace(e.second, e.first);
	auto it = parIndice.find(m);
	return it == parIndice.end() ? std::string() : it->second;
}

static bool ObjetNonUtilisable(int i);   // l'objet animé joue NotUseable (FUN_005f46a0 sur son contrôleur)

// FUN_004991e0 (le relevé des objets proches d'un piéton, depuis TargetProp et PropTargetInteractive ; la branche des piétons de
// l'IA, CObjetsAnimes::Relever) : rien quand le piéton se sert déjà d'un objet (+0x548) ; sinon la liste des proches (+0x450, leur
// nombre en +0x4c8) refaite et, s'il y en a un, le repéré (+0x444, son point +0x448, sa distance +0x44c).
static void
ReleverObjetsProches(CScriptPed *q, float portee)
{
	if(q->objetUtilise >= 0) return;
	CObjetsAnimes::Releveur r;
	r.pos = q->pos; r.parScript = q->script >= 0;
	auto fe = modeles.femme.find(q->modele); r.femme = fe != modeles.femme.end() && fe->second;
	std::vector<CObjetsAnimes::Repere> proches;
	CObjetsAnimes::Repere m = objetsAnimes.Relever(r, portee, ObjetNonUtilisable, proches);
	q->objetsProches.clear();
	for(const CObjetsAnimes::Repere &e : proches) q->objetsProches.push_back({e.objet, e.sync, e.distance});
	if(m.objet >= 0){ q->objetRepere = m.objet; q->syncRepere = m.sync; q->distanceRepere = m.distance; }
}

// FUN_004979b0 sur le repéré et les proches relevés : l'objet choisi devient l'objet visé du piéton (+0x438, son point +0x43c).
static bool
ViserObjetChoisi(CScriptPed *q, int ped, uint32 genre, float portee, int32 modele, const char *condition, float t)
{
	std::vector<CObjetsAnimes::Repere> proches;
	for(const CScriptPed::ObjetProche &e : q->objetsProches) proches.push_back({e.objet, e.sync, e.distance});
	CObjetsAnimes::Repere repere{q->objetRepere, q->syncRepere, q->distanceRepere};
	CObjetsAnimes::Repere c = objetsAnimes.Choisir(repere, proches, genre, portee, modele, ObjetNonUtilisable);
	if(c.objet < 0) return false;
	if(q->objetVise != c.objet && journalRestant > 0){
		const CObjetAnime *o = objetsAnimes.Objet(c.objet);
		const CTriggerZone *z = o ? donnees.ZoneParHandle(o->declencheur) : nil;
		printf("%6.2f s  %s : le piéton %d vise l'objet animé du déclencheur %s, à %.2f m\n", t, condition, ped, z ? z->nom.c_str() : "?", c.distance);
	}
	q->objetVise = c.objet; q->syncVise = c.sync;
	return true;
}

struct CJugeOutil : CJugeAction {
	int ped; float t;
	// Les tirages du contexte qui juge (+0x24 et les dés +0x28 à +0x34, CActionController::tirageContexte et des) : ceux
	// du contrôleur qui juge, sinon du contrôleur d'actions du piéton ; sans contrôleur, ceux du juge, à 0.
	mutable float tirageSeul = 0.0f, desSeuls[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float &Tirage(void) const {
		const CActionController *c = controleur;
		if(c == nil){ auto it = controleurs.find(ped); if(it != controleurs.end()) c = &it->second; }
		return c ? c->tirageContexte : tirageSeul;
	}
	float &De(uint32 i) const {
		const CActionController *c = controleur;
		if(c == nil){ auto it = controleurs.find(ped); if(it != controleurs.end()) c = &it->second; }
		return c ? c->des[i & 3] : desSeuls[i & 3];
	}
	CJugeOutil(int p, float t_) : ped(p), t(t_) {}
	// Le contexte d'un objet animé (DAT_00a453f8, que l'emplacement +0x40 de l'objet rend, FUN_0044cbd0) : sans piéton (+8, jamais
	// écrit : vérifié à l'assembleur, seul le destructeur de 0x8f2507 le touche), l'objet en +0xc (DAT_00a45404, FUN_004ed7f0).
	int objet = -1;
	static CJugeOutil DeLObjet(int o, float t_){ CJugeOutil j(-1, t_); j.objet = o; return j; }
	// FUN_004eaa60 : l'objet du contexte. Sans piéton, le +0xc ; avec un piéton, l'objet dont il se sert (+0x548), sinon son
	// objet visé (+0x438) ou repéré (+0x444) s'il est animé (les 3 bits bas de +0x108 à 6) ; l'outil n'a que des objets animés.
	int ObjetDuContexte(void) const;
	int JoueNom(uint32 nom) const override { return arbres.JoueHachage(NoeudJoue(ped), nom & 0x7fffffff); }
	bool ACible(void) const override { return CibleDe(ped) >= 0; }
	// Le temps du nœud joué par le contrôleur qui juge (le +0x10 du contexte), sinon celui du contrôleur d'actions.
	float TempsDuNoeud(bool *ok) const {
		if(controleur){ *ok = true; return controleur->Temps(t); }
		auto it = controleurs.find(ped);
		*ok = it != controleurs.end();
		return *ok ? it->second.Temps(t) : 0.0f;
	}
	// LANCER_JUGES=PIETON@DEBUT-FIN : chaque jugement de ce piéton entre ces instants (s), la classe hachée et la réponse.
	int Juger(const CActionTreeFile &f, int32 d, uint32 type, const uint32 mots[6]) const override {
		int r = JugerClasse(f, d, type, mots);
		static int quiTrace = -2; static float de = 0, a = 0;
		if(quiTrace == -2){ quiTrace = -1; if(const char *e = getenv("LANCER_JUGES")) sscanf(e, "%d@%f-%f", &quiTrace, &de, &a); }
		if(ped == quiTrace && t >= de && t <= a) printf("%6.2f s  juge %d : #%08x (%08x %08x %08x %08x %08x) → %d\n", t, ped, type & 0x7fffffff, mots[0], mots[1], mots[2], mots[3], mots[4], r);
		return r;
	}
	int JugerClasse(const CActionTreeFile &f, int32 d, uint32 type, const uint32 mots[6]) const {
		static const uint32 kArme = ActionHash("WeaponEquiped"), kDemande = ActionHash("ActionRequest"),
		                    kTemps = ActionHash("ActionNodePlayTime"), kLua = ActionHash("LuaCond"),
		                    kCible = ActionHash("TargetPlaying"), kMoi = ActionHash("MePlaying"),
		                    kDrapeau = ActionHash("ConditionFlag"), kZone = ActionHash("HitLocation"),
		                    kTempsCoup = ActionHash("HitTime"), kStat = ActionHash("stat"), kJoueur = ActionHash("IsPlayer"),
		                    kSysteme = ActionHash("PedFightingSystem"), kSante = ActionHash("Health"),
		                    kPourcent = ActionHash("PercentageHealth"), kEnAttente = ActionHash("DamagePending"),
		                    kHumiliation = ActionHash("IsHealthAtHumiliationLevel"), kMission = ActionHash("IsMissionSpecific"),
		                    kTaille = ActionHash("CharacterSize"), kDrapeaux = ActionHash("PedFlags"),
		                    kGroupeHumiliation = ActionHash("HumiliationGroupIs"), kTempsPasse = ActionHash("ActionNodePlayTimePast"),
		                    kGroupeAnim = ActionHash("AnimationGroupLoaded"), kParolePrete = ActionHash("IsSpeechPreloaded"),
		                    kPositionCible = ActionHash("TargetRelativePosition");
		// Les conditions des objets animés (leurs juges, l'emplacement 1 des tables : lus dans l'index le 06.11.2026) : PropFlags
		// (0x936874, FUN_00654f90) : le mot +0x160 + 4 × (+4) de l'objet du contexte non nul ; PropPedIsUsingMe (0x937d88,
		// FUN_006551c0) : son +0x234 (un piéton s'en sert) ; PropModelID (0x937cac, FUN_00655480) : le nom haché de son modèle (+4 du
		// modelinfo) vaut +4. Sans objet, faux. PedConditionGroup (0x937080, FUN_00601d90, lue à l'assembleur de 0x601d90 à 0x601dc1) :
		// avec un piéton (+8 du contexte), le groupe +4 jugé dans le contexte du piéton (son emplacement +0x40) ; sans, vrai si le
		// contexte a un objet (+0xc).
		static const uint32 kDrapeauObjet = ActionHash("PropFlags"), kObjetUtilise = ActionHash("PropPedIsUsingMe"),
		                    kModeleObjet = ActionHash("PropModelID"), kGroupePieton = ActionHash("PedConditionGroup");
		if(type == kDrapeauObjet) return objetsAnimes.Drapeau(ObjetDuContexte(), (int)mots[0]);
		if(type == kObjetUtilise){ const CObjetAnime *o = objetsAnimes.Objet(ObjetDuContexte()); return o && o->utilisateur >= 0; }
		if(type == kModeleObjet){
			const CObjetAnime *o = objetsAnimes.Objet(ObjetDuContexte());
			return o && (ActionHash(NomDeModele(o->modele).c_str()) & 0x7fffffff) == (mots[0] & 0x7fffffff);
		}
		if(type == kGroupePieton){
			if(ped < 0) return objet >= 0;
			std::vector<int32> groupe;
			if(!f.GroupeDe(d, groupe)) return 0;
			return arbres.JugerListe(f, groupe, CJugeOutil(ped, t)) == 1;
		}
		// TargetProp (ConditionTargetProp 0x936690, FUN_0065c250 ; créateur 0x644f40, 0x2c octets) : +4 la portée, +8 une hauteur
		// (pour les ramassages), les octets +0xc (les ramassages, avec le +0x149c du piéton : non suivi) et +0xd, +0x10 le nom du
		// modèle (FUN_0051acd0 : faux s'il est inconnu), +0x14 le point de synchronisation, +0x1c le genre de l'objet, l'octet +0x20
		// et +0x24 passés au relevé, les octets +0x28 (l'objet déjà visé suffit) et +0x29 (il devient aussi la cible du contexte,
		// FUN_00606b40, et l'objet +0x544 : non suivi). Un point de synchronisation hors des cinq (FUN_00520230), nommé, mène à
		// l'objet le plus proche du modèle (FUN_00460650, FUN_00467c30, FUN_005eb6f0 : non suivi, faux). Sinon, hors du joueur, le
		// relevé (FUN_004991e0 : rien quand le piéton se sert déjà d'un objet, +0x548 ; la liste +0x4c8 vidée ; FUN_004989f0), puis
		// FUN_004979b0(genre, portée, modèle, 1, 0, 0) : l'objet visé (+0x438) et son point (+0x43c).
		static const uint32 kObjetCible = ActionHash("TargetProp"), kObjetVise = ActionHash("PropTargetInteractive");
		if(type == kObjetCible){
			CScriptPed *q = pietons.Acceder(ped);
			if(q == nil) return 0;
			uint32 hm = f.ChampCondition(d, 0x10) & 0x7fffffff, b28 = f.ChampCondition(d, 0x28);
			int32 modele = -1;
			if(hm){
				for(const auto &e : modeles.parNom) if((ActionHash(e.first.c_str()) & 0x7fffffff) == hm){ modele = e.second; break; }
				if(modele == -1) return 0;
			}
			if((b28 & 0xff) && objetsAnimes.Objet(q->objetVise) && (hm == 0 || objetsAnimes.Objet(q->objetVise)->modele == modele)) return 1;
			uint32 sync = f.ChampCondition(d, 0x14) & 0x7fffffff;
			bool connu = sync == 0;
			for(const char *n : kPointsDeSynchro) connu |= (ActionHash(n) & 0x7fffffff) == sync;
			if(!connu) return 0;
			float portee; memcpy(&portee, &mots[0], 4);
			if(q->type != SCRIPT_PED_TYPE_JOUEUR) ReleverObjetsProches(q, portee);
			return ViserObjetChoisi(q, ped, f.ChampCondition(d, 0x1c), portee, modele, "TargetProp", t);
		}
		// PropTargetInteractive (ConditionPropTargetInteractive 0x936838, FUN_00655110, lue à l'assembleur de 0x655110 à 0x65519d ;
		// créateur 0x645470, 0x18 octets, FUN_00654e30 : +4 la portée, 2 par défaut (_DAT_00900bfc), +8 le genre de l'objet (un
		// HashString, FUN_0066bd40), l'octet +0xc, +0x10 un flottant (la même valeur par défaut) et les octets +0x14 à +0x17, à 0) : sans
		// piéton (+8 du contexte), faux ; le relevé FUN_004991e0(portée, +0xc, +0x10, +0x14) sur le composant +0x34c du piéton, sauf pour
		// le joueur (+0x1310 à 0xd) quand +0xc est nul et la portée nulle ; un partenaire de prise (+0x155c) sans l'octet +0x17 rend
		// faux ; sinon FUN_004979b0(genre, portée, -1, 0, +0x15, +0x16) : l'objet repéré ou l'un des proches, à moins de la portée, de
		// n'importe quel modèle, et animé (le quatrième argument nul ne laisse passer que les entités de type 6, les 3 bits bas de
		// +0x108), devient l'objet visé. L'outil ne relève les objets que pour les piétons de l'IA (la branche du joueur, FUN_00497ce0,
		// n'est pas suivie) ; ses objets sont tous animés ; +0x15 (le genre DAMAGEABLE) et +0x16 (l'objet verrouillé au joueur) ne
		// sont pas suivis.
		if(type == kObjetVise){
			CScriptPed *q = pietons.Acceder(ped);
			if(q == nil) return 0;
			float portee; memcpy(&portee, &mots[0], 4);
			bool sansReleve = q->type == SCRIPT_PED_TYPE_JOUEUR && (f.ChampCondition(d, 0xc) & 0xff) == 0 && portee == 0.0f;
			if(!sansReleve && q->type != SCRIPT_PED_TYPE_JOUEUR) ReleverObjetsProches(q, portee);
			if(q->cibleDePrise >= 0 && ((f.ChampCondition(d, 0x14) >> 24) & 0xff) == 0) return 0;
			return ViserObjetChoisi(q, ped, mots[1], portee, -1, "PropTargetInteractive", t);
		}
		const CScriptPed *moi = pietons.Acceder(ped);
		// WeaponEquiped (ConditionWeaponEquiped 0x9365a4, FUN_0065bd80, lu à l'assembleur de 0x65bde9 à 0x65be1e) : sans piéton,
		// faux ; l'arme en passage (+0x1574) avec l'octet +9, sinon l'arme en main (+0x10e). Sans arme : vrai si le nom +4 manque
		// ou vaut « unarmed » (_stricmp). Avec une arme : un nom manquant rend la négation de l'octet +8 ; « any » (0x93c2e8) est
		// vrai ; sinon le HashString du nom doit être le nom du modèle (+4 du modelinfo, FUN_0050e7f0).
		if(type == kArme){
			if(moi == nil) return 0;
			std::string nom = f.ChaineCitee((uint32)d + 8);
			int32 a = ((mots[1] >> 8) & 0xff) && moi->armePassage != -1 ? moi->armePassage : moi->armeTenue;
			const std::string *m = a == -1 ? nil : NomDArme(a);
			if(m == nil) return nom.empty() || strcasecmp(nom.c_str(), "unarmed") == 0;
			if(nom.empty()) return (mots[1] & 0xff) == 0;
			if(strcasecmp(nom.c_str(), "any") == 0) return 1;
			return ActionHash(nom.c_str()) == ActionHash(m->c_str());
		}
		float seuil; memcpy(&seuil, &mots[0], 4);
		// IsPlayer (FUN_005ffe40) : le type du piéton (+0x1310) vaut 13 (PLAYER1).
		if(type == kJoueur) return moi && moi->type == SCRIPT_PED_TYPE_JOUEUR;
		// IsFemale (FUN_005ffb50) : l'octet +0x50 du modelinfo du piéton (PedIsFemale), la colonne FEMALE de son entrée peds.
		// IsAuthority (FUN_005ffb20) : son type (+0x1310) est PREFECT (0), COP (7) ou TEACHER (8).
		// PedType (FUN_005ffac0) : son type est +4.
		static const uint32 kFemme = ActionHash("IsFemale"), kAutorite = ActionHash("IsAuthority"), kTypePieton = ActionHash("PedType");
		if(type == kFemme){
			if(moi == nil) return 0;
			auto it = modeles.femme.find(moi->modele);
			return it != modeles.femme.end() && it->second;
		}
		if(type == kAutorite) return moi && (moi->type == 0 || moi->type == 7 || moi->type == 8);
		if(type == kTypePieton) return moi && moi->type == (int32)mots[0];
		// PedModelID (FUN_00602050) : le modelinfo du piéton (FUN_00477c70), dont le nom haché (+4) est celui de la chaîne
		// +4 (FUN_0050e7f0 : HashString). IsElderly (FUN_006069a0 → FUN_0044e010) : l'entrée de pedstats.dat de son modelinfo
		// (+0x54, son nom haché en +0x18) est STAT_CV_MALE_OLD ou STAT_CV_FEMALE_OLD. HasGrappleTarget (FUN_00606490) : un
		// partenaire de prise (+0x155c). IsStealthMissionChar (FUN_00602cf0) : +0x1398 non nul.
		static const uint32 kModele = ActionHash("PedModelID"), kVieux = ActionHash("IsElderly"),
		                    kPrise = ActionHash("HasGrappleTarget"), kDiscret = ActionHash("IsStealthMissionChar");
		if(type == kModele){
			if(moi == nil) return 0;
			auto it = modeles.nomDuModele.find(moi->modele);
			return it != modeles.nomDuModele.end() && ActionHash(it->second.c_str()) == ActionHash(f.ChaineCitee((uint32)d + 8).c_str());
		}
		if(type == kVieux){
			if(moi == nil) return 0;
			auto it = modeles.statDuPieton.find(moi->modele);
			return it != modeles.statDuPieton.end() && (it->second == "STAT_CV_MALE_OLD" || it->second == "STAT_CV_FEMALE_OLD");
		}
		if(type == kPrise) return moi && moi->cibleDePrise >= 0;
		// TargetRelativeOrientation (FUN_005ff420, lue à l'assembleur) : l'autre est le partenaire de prise (+0x155c) si
		// l'octet +4, sinon +0x434 du piéton s'il est posé (l'outil ne le pose pas), sinon la cible du composant de visée
		// (+0x1c8, FUN_004d7630 : la cible verrouillée d'abord) ; vrai si 2 × |acos(devant · devant de l'autre) - (+8)|,
		// ramené dans [-π, π] (FUN_00432d90), ne dépasse pas +0xc.
		static const uint32 kOrientation = ActionHash("TargetRelativeOrientation");
		if(type == kOrientation){
			if(moi == nil) return 0;
			const CScriptPed *autre = pietons.Acceder((mots[0] & 0xff) ? moi->cibleDePrise : CibleDe(ped));
			if(autre == nil || autre == moi) return 0;
			float a = moi->cap * 3.14159265f / 180.0f, b = autre->cap * 3.14159265f / 180.0f;
			float produit = cosf(a) * cosf(b) + sinf(a) * sinf(b);
			float angle = produit >= 1.0f ? 0.0f : produit <= -1.0f ? 3.14159265f : acosf(produit);   // FUN_00413c10
			float vise, tolerance; memcpy(&vise, &mots[1], 4); memcpy(&tolerance, &mots[2], 4);
			float e = fabsf(angle - vise);
			while(e > 3.14159265f) e -= 2.0f * 3.14159265f;
			return e + e <= tolerance;
		}
		// Sprint (FUN_005fda70) : la réserve de sprint (+0x18d0) comparée à +4 par +8 (FUN_005fb750). IsPlayerTired
		// (FUN_005fc3b0) : l'état physique du joueur (DAT_00c1af4c, que PlayerGetPhysicalState rend) vaut 2 ; l'outil le
		// laisse à 0.
		static const uint32 kSprint = ActionHash("Sprint"), kFatigue = ActionHash("IsPlayerTired");
		if(type == kSprint) return moi ? ComparerCondition(moi->sprint, seuil, mots[1]) : -1;
		if(type == kFatigue) return etatPhysiqueJoueur == 2;
		// WeaponDesired (ConditionWeaponDesired, FUN_0065b4b0) : sans arme voulue (+0x18 du composant +0x2e0 à 0), la négation de
		// l'octet +8 ; avec un nom +4 qui n'est pas « any », son arme (FUN_0051ad50, gardée en +0xc) doit être la voulue, sinon la
		// négation de +8 ; puis l'octet +8. WeaponIsInventory (FUN_0065b7e0 → FUN_0055ba80) : une arme en main dont le type a +0x64.
		static const uint32 kArmeVoulue = ActionHash("WeaponDesired"), kArmeInventaire = ActionHash("WeaponIsInventory");
		if(type == kArmeVoulue){
			bool o = (mots[1] & 0xff) != 0;
			if(moi == nil || moi->armeVoulue == 0) return !o;
			std::string nom = f.ChaineCitee((uint32)d + 8);
			if(!nom.empty() && strcasecmp(nom.c_str(), "any") != 0 && moi->armeVoulue != ArmeParNom(nom)) return !o;
			return o;
		}
		if(type == kArmeInventaire) return moi && moi->armeTenue != -1 && armes.Type(moi->armeTenue).inventaire != 0;
		// WeaponNeedInit (ConditionWeaponNeedInit 0x9366a4, FUN_0065b7b0) : le +0x120 de l'objet arme (+0x1d0, un CWeapon) vaut
		// -1 (_DAT_009009ac). Le constructeur de CWeapon (FUN_004da610, 0x4da696) et ses remises (FUN_004d92c0, FUN_004d9940,
		// FUN_004d9fa0) le posent à -1, le lancement d'une piste SetWeaponInfo (FUN_00660f50) à son champ +0x2c (7 à mains
		// nues dans Weapons.cat) : la banque NeedInit ne passe qu'une fois par arme.
		// ConversationActive (CConversationActiveCond 0x937318, FUN_00685940, lue à l'assembleur de 0x685940 à 0x685953) :
		// setne / sub 1 / and : l'octet de DAT_00b9db1c (une conversation en cours) seulement quand le contexte n'a pas de
		// piéton (+8) ; avec un piéton, faux.
		static const uint32 kArmeInit = ActionHash("WeaponNeedInit"), kConversation = ActionHash("ConversationActive"),
		                    kSurMission = ActionHash("OnMission");
		if(type == kSurMission) return SurMission(mots[0]);
		static const uint32 kTenue = ActionHash("PedIsInOutfit");
		if(type == kTenue) return ConditionTenue(mots[0]);
		static const uint32 kCondMission = ActionHash("MissionCond"), kDebloquee = ActionHash("MissionUnlocked");
		if(type == kCondMission || type == kDebloquee) return ConditionMission(type, mots);
		if(type == kArmeInit) return moi && moi->armeInit == -1.0f;
		if(type == kConversation) return 0;
		// WeaponDesiredIsEquiped (FUN_0065b5b0) : sans arme voulue, vrai ; sinon l'objet arme (+0x1d0) doit en avoir le
		// modèle (+0x10e), et une arme voulue -1 (les mains nues, déjà nues) est alors remise à 0. IsWeaponFiredWhenDropped
		// (FUN_0065bbe0) : l'octet +0x2b du type de l'arme tenue (WeaponGetType), faux sans arme.
		static const uint32 kArmeTenue = ActionHash("WeaponDesiredIsEquiped"), kArmeLachee = ActionHash("IsWeaponFiredWhenDropped");
		if(type == kArmeTenue){
			CScriptPed *q = pietons.Acceder(ped);
			if(q == nil || q->armeVoulue == 0) return 1;
			if(q->armeVoulue != q->armeTenue) return 0;
			if(q->armeVoulue == -1) q->armeVoulue = 0;
			return 1;
		}
		// ParametricTime (ConditionParametricTime 0x937ad8, FUN_005fbed0 ; créateur 0x649580) : le flottant que pointe le
		// +0x38 du contexte (+0x157c du piéton, que seul le vélo pose) comparé à +4 par +8 (FUN_005fb750).
		static const uint32 kParametrique = ActionHash("ParametricTime");
		if(type == kParametrique) return moi ? ComparerCondition(moi->parametrique, seuil, mots[1]) : -1;
		// WeaponAmmo (FUN_0065b290) : les munitions (FUN_00486130 : 0 pour une arme sous 1) de l'arme tenue (FUN_00474aa0 :
		// le modèle +0x10e de l'objet arme +0x1d0, sinon 0), ou avec l'octet +0xc celles de l'arme du HUD (HUDWeapons, le
		// composant 20, +0xc0) pour le joueur seulement, comparées à +4 par +8 (FUN_005fb750). Sans arme : 0.
		static const uint32 kMunitions = ActionHash("WeaponAmmo");
		if(type == kMunitions){
			if(moi == nil) return 0;
			if((mots[2] & 0xff) && moi->type != SCRIPT_PED_TYPE_JOUEUR) return 0;
			int32 a = (mots[2] & 0xff) ? armeHud : moi->armeTenue;
			return ComparerCondition(a == -1 && !(mots[2] & 0xff) ? 0.0f : (float)MunitionsDe(ped, a), seuil, mots[1]);
		}
		if(type == kArmeLachee) return moi && moi->armeTenue != -1 && armes.Type(moi->armeTenue).tireLachee;
		// WeaponHudLocked (0x9366d4, FUN_0065aec0) : l'octet +0x13c du HUD des armes. WeaponModelRequest (0x93661c, FUN_0065b6f0) :
		// l'arme du nom +4 (FUN_0051ad50), ou la voulue, demandée (WeaponRequestModel, la priorité 0xc avec l'octet +8) : l'outil n'a
		// pas de diffusion, chargée. WeaponDesiredIsFocusWeapon (0x936590, FUN_0065b5f0) : faux hors du joueur ; vrai sans arme au
		// HUD (+0xc0 à -1) ; sinon l'arme voulue est celle du HUD, et avec l'octet +4 une autre la remplace.
		static const uint32 kHudVerrou = ActionHash("WeaponHudLocked"), kModeleDemande = ActionHash("WeaponModelRequest"),
		                    kArmeHud = ActionHash("WeaponDesiredIsFocusWeapon");
		if(type == kHudVerrou) return hudArmesVerrouille;
		// WeaponFlags (0x937c84, FUN_0065bb20) : sans arme en main, faux ; le drapeau +8 de l'entrée d'inventaire de l'arme (FUN_0045cb70,
		// FUN_0045cb40 : les mots +4 + 4n), sinon de l'objet arme (+0x26c + 4n). Les pistes SetWeaponFlags qui les posent ne sont pas
		// jouées : 0, donc faux.
		static const uint32 kDrapeauxArme = ActionHash("WeaponFlags");
		if(type == kDrapeauxArme) return 0;
		if(type == kModeleDemande) return 1;
		if(type == kArmeHud){
			CScriptPed *q = pietons.Acceder(ped);
			if(q == nil || q->type != SCRIPT_PED_TYPE_JOUEUR) return 0;
			if(armeHud == -1) return 1;
			bool egal = q->armeVoulue == armeHud;
			if(!egal && (mots[0] & 0xff)) q->armeVoulue = armeHud;
			return egal;
		}
		// CollisionWeaponType (FUN_0065aa30, lu dans l'index) : sur le relevé du dernier coup reçu (le +0x10 du contexte, +0x408 du
		// composant +0x2e0), l'arme du coup (+0x5e, un mot : +0x466 ; -1 à mains nues) et son genre (+0x60 : +0x468), que la méthode
		// du coup pose (FUN_0047b810, ses 5e et 19e arguments). Avec l'octet +8, l'arme du nom +0xc (FUN_0051ad50 : -1 pour
		// « unarmed » ou un nom inconnu, gardée en +0x10) doit être celle du coup ; sans lui, une arme est requise, puis le genre +4
		// juge son type et le genre du coup : 0, +0x68 et le genre 2 ou 3 ; 1, +0x68, +0x69 et le genre 2 ; 2, +0x69 et le genre 1 ;
		// 3, le genre 4 ; un autre, faux.
		static const uint32 kArmeDuCoup = ActionHash("CollisionWeaponType");
		if(type == kArmeDuCoup){
			auto r = coups.find(ped);
			int32 arme = r == coups.end() ? -1 : r->second.arme, genre = r == coups.end() ? 0 : r->second.genre;
			if(mots[1] & 0xff) return ArmeParNom(f.ChaineCitee((uint32)d + 16)) == arme;
			if(arme < 0) return 0;
			const CTypeArme &t = armes.Type(arme);
			switch(mots[0]){
			case 0: return t.tir68 && (genre == 2 || genre == 3);
			case 1: return t.tir68 && t.tir69 && genre == 2;
			case 2: return t.tir69 && genre == 1;
			case 3: return genre == 4;
			default: return 0;
			}
		}
		// CollisionStart (FUN_00602260) : l'octet +0x4e5 du piéton, que le lancement d'une piste Collision pose et que rien
		// ne remet à 0. AttackPhase (FUN_00601fc0) : la phase +0x71c est +4, et, si +8 est positif, l'échéance +0x720
		// tombe plus de +8 secondes après maintenant (src/core/ActionController).
		static const uint32 kCollisionStart = ActionHash("CollisionStart"), kPhase = ActionHash("AttackPhase");
		if(type == kCollisionStart || type == kPhase){
			auto it = controleurs.find(ped);
			if(it == controleurs.end()) return 0;
			if(type == kCollisionStart) return ConditionCollisionStart(it->second);
			float delai; memcpy(&delai, &mots[1], 4);
			return ConditionAttackPhase(it->second, (int32)mots[0], delai, (uint32)lrintf(t * 1000.0f));
		}
		// IsSocialStateWait (FUN_00603b80) et IsSocializing (FUN_006039d0, l'octet +4 : pour le joueur, un autre en échange
		// avec lui) : l'état social (+0xbb0) et le partenaire (+0xbb4) du piéton.
		static const uint32 kAttenteSociale = ActionHash("IsSocialStateWait"), kSocial = ActionHash("IsSocializing");
		if(type == kAttenteSociale) return pietons.AttenteSociale(ped);
		if(type == kSocial) return pietons.Socialise(ped, (mots[0] & 0xff) != 0);
		// Controller (FUN_006006f0) : le piéton mené par une manette (l'octet +5 : l'une des deux ; sinon la 1 si l'octet +4,
		// la 0 sinon). La manette 0 mène le joueur (FUN_0049b1b0) ; PedSetControllerID les change.
		static const uint32 kManette = ActionHash("Controller");
		if(type == kManette) return pietons.MeneParLaManette(ped, (mots[0] & 0xff) != 0, ((mots[0] >> 8) & 0xff) != 0);
		// CameraActive (FUN_0065aea0) : le mode courant du gestionnaire de caméras (+0x90) est +4. CameraInWidescreen
		// (FUN_0065afc0) : l'écran large (DAT_00c3cd0c).
		static const uint32 kCamera = ActionHash("CameraActive"), kLargeEcran = ActionHash("CameraInWidescreen");
		if(type == kCamera) return CameraActive(cameras, (int32)mots[0]);
		if(type == kLargeEcran) return CameraInWidescreen(cameras);
		// AttackTimer (ConditionAttackTimerPlayTime, FUN_006000c0) : le temps depuis le dernier coup du piéton (+0x550, posé à
		// l'arrêt de ses pistes Collision), comparé à +4 s par +0xc ; l'octet +8 relance la minuterie.
		// Attacked (FUN_00606b80) : le temps depuis le coup reçu (+0x2c du relevé, comme HitTime) au plus +4 s ; alors, pour un
		// piéton qui n'est pas le joueur, l'attaquant du relevé devient sa cible, la cible de son objectif et son agresseur.
		static const uint32 kMinuterie = ActionHash("AttackTimer"), kAttaque = ActionHash("Attacked");
		if(type == kMinuterie){
			auto it = controleurs.find(ped);
			if(it == controleurs.end()) return 0;
			return ConditionAttackTimer(it->second, seuil, (mots[1] & 0xff) != 0, mots[2], (uint32)lrintf(t * 1000.0f));
		}
		// BlockTimer (ConditionBlockTimerPlayTime 0x936a08, FUN_00600160) : maintenant (DAT_00c1a9b4) comparé par +0xc à
		// l'instant de la parade (+0x4d0, le dernier appui du bouton 12 par une piste ActionRequest, 0 sans parade) + 1000 ×
		// (+4) ; si elle passe, l'octet +8 relance la minuterie. Seul AI_2_02 s'en sert.
		static const uint32 kParade = ActionHash("BlockTimer");
		if(type == kParade){
			uint32 &inst = instantsParade[ped];
			int r = ComparerCondition((float)maintenant, (float)inst + seuil * 1000.0f, mots[2]);
			if(r == 1 && (mots[1] & 0xff)) inst = maintenant;
			return r;
		}
		if(type == kAttaque){
			auto it = coups.find(ped);
			float depuis = it != coups.end() && it->second.instant >= 0.0f ? t - it->second.instant : t;
			if(!(depuis <= seuil)) return 0;
			if(it != coups.end() && it->second.attaquant >= 0) pietons.PrendreAttaquant(ped, it->second.attaquant);
			return 1;
		}
		// FightClass (FUN_00601440) : la classe de combat du piéton (+0x714, la colonne S de pedstats.dat) est +4.
		// RangeToFocus (FUN_006008e0, lu à l'assembleur de 0x6008e0 à 0x60096c) : sans focus (+0x53c), faux ; sinon la
		// distance à plat (x, y) entre les deux, comparée à +4 par +8 (FUN_005fb750).
		static const uint32 kClasse = ActionHash("FightClass"), kFocus = ActionHash("RangeToFocus");
		if(type == kClasse) return moi && moi->classeCombat == (int32)mots[0];
		if(type == kFocus){
			const CScriptPed *f = moi ? pietons.Acceder(moi->focus) : nil;
			if(f == nil) return 0;
			float dx = f->pos.x - moi->pos.x, dy = f->pos.y - moi->pos.y;
			return ComparerCondition(sqrtf(fabsf(dx * dx + dy * dy)), seuil, mots[1]);
		}
		// HudComponentVisible (FUN_0065aee0) : l'emplacement 4 du composant +4 de la table du HUD (DAT_00c674a0), l'octet
		// +8 pour tous ceux que les arbres relevés testent (14, 18, 19, 23 à 37 ; HUDMoney et HUDClockWarnings ont le leur).
		static const uint32 kHud = ActionHash("HudComponentVisible");
		if(type == kHud) return HudComponentVisible(hud, (int32)mots[0]);
		// Season (FUN_005fc8d0) : avec l'octet +4, la zone visible doit être dehors (FUN_004301d0) ; puis la saison du
		// chapitre en cours est +8.
		static const uint32 kSaison = ActionHash("Season");
		if(type == kSaison){
			if((mots[0] & 0xff) && !ZoneDehors(zones.zoneVisible)) return 0;
			return chapitre < 7 && kSaisonDuChapitre[chapitre] == (int32)mots[1];
		}
		// MotionIntentionSpeed (FUN_005fd810) : la vitesse de l'intention de mouvement (+0x1c du composant +0x2e0) comparée
		// au seuil +4 par +8 (FUN_005fb750). MotionIntention (ConditionMotionIntentionDirection, FUN_005fd760) : le devant
		// du piéton, à plat, tourné de +4 radians (FUN_005fbe50), et la direction de l'intention (+0, +4, +8) ont un produit
		// scalaire d'au moins cos(+8) (vérifié à l'assembleur, 0x5fd7ef : vrai si cos(+8) ≤ le produit).
		static const uint32 kVitesseIntention = ActionHash("MotionIntentionSpeed"), kDirectionIntention = ActionHash("MotionIntention");
		if(type == kVitesseIntention) return moi ? ComparerCondition(moi->intentionVitesse, seuil, mots[1]) : -1;
		if(type == kDirectionIntention){
			if(moi == nil) return -1;
			float tour, ouverture; memcpy(&tour, &mots[0], 4); memcpy(&ouverture, &mots[1], 4);
			float a = moi->cap * 3.14159265f / 180.0f + tour;
			float produit = cosf(a) * moi->intention.x + sinf(a) * moi->intention.y;
			return cosf(ouverture) <= produit;
		}
		if(type == kDiscret) return moi && moi->discretion;
		// IsMissionSpecific (FUN_005ffb70) : l'octet +0x1570 vaut 2, ce que le constructeur (FUN_0047ffe0) pose pour un
		// piéton créé par un script (FUN_005cf6b0 → FUN_0046dea0(modèle, 2)) ; PedMakeAmbient le change (FUN_004770a0).
		// Dans l'outil : un piéton noté à un objet script.
		if(type == kMission) return moi && ped != SCRIPT_PED_JOUEUR && moi->script >= 0;
		// PedFlags (FUN_005fd700) : le drapeau +4 du piéton (+0x1314 + 4n) non nul. Dans la charge d'une prise, « Not PedFlags »
		// (le drapeau 70, posé par la fin de la charge) mène à un nœud dont l'Execute vise GrappleMoves/ExecuteNodes, qui
		// n'existe pas (ExecuteNodes est sous GrappleStrikes) : le nœud finit tout de suite et passe à ../Give.
		// Le jugement lit sans borne : le « drapeau » 138 (0x8a, la sélection d'une arme du joueur, Weapons/SelectActions/WeaponSelect)
		// est le mot +0x153c, au-delà des 0x89 drapeaux, que la remise FUN_00485820 met à 0 et FUN_00475b10 à 1 pour deux piétons
		// (non suivi) : 0.
		if(type == kDrapeaux) return moi && mots[0] < SCRIPT_PED_DRAPEAUX ? moi->drapeaux[mots[0]] != 0 : moi && mots[0] == 0x8a ? 0 : -1;
		// ActionNodePlayTimePast (FUN_005fbfc0) : le temps du nœud joué a atteint le seuil +0x50 de l'objet +0x10 du contexte,
		// -1 par défaut (0x9009ac, FUN_005f3ab0), que seule une piste Damage à +0x1c positif pose (FUN_00609400). L'outil
		// ne lit pas les pistes Damage (aucune des arbres relevés n'a de seuil positif) : le seuil reste -1, c'est vrai.
		if(type == kTempsPasse){
			bool ok; float v = TempsDuNoeud(&ok);
			return ok ? v >= -1.0f : -1;
		}
		// AnimationGroupLoaded (FUN_006024d0) : +4 le nom du groupe (cité par la première table), +8 un octet. Faux sans
		// piéton ou pour un groupe inconnu (FUN_0042e200) ; vrai s'il est prêt ; sinon, l'octet posé, le groupe est
		// demandé (FUN_00531ed0) et la condition reste fausse jusqu'à ce qu'il soit chargé.
		if(type == kGroupeAnim){
			std::string nom = f.ChaineCitee((uint32)d + 8);
			if(moi == nil || nom.empty() || !groupesAnim.Existe(nom.c_str())) return 0;
			if(groupesAnim.EstCharge(nom.c_str())) return 1;
			if((mots[1] & 0xff) != 0 && groupesAnim.Demander(nom.c_str(), imageCourante))
				printf("%6.2f s  le piéton %d demande le groupe d'animations %s\n", t, ped, nom.c_str());
			return 0;
		}
		// IsSpeechPreloaded (FUN_00602620) : +4 l'événement, +8 la ligne (-1 : toutes), l'octet +0xc : pour ce piéton
		// seulement. Vrai si l'une des deux places de préchargement du son (0xcf458c, FUN_005a0cc0) tient cette ligne,
		// prête : dans l'outil, une ligne retenue par une piste SpeechTrack à une image antérieure.
		if(type == kParolePrete){
			for(const ParoleTenue &r : parolesTenues)
				if(r.evenement == (int)mots[0] && (mots[1] == 0xffffffffu || r.ligne == (int)mots[1])
				   && ((mots[2] & 0xff) == 0 || r.ped == ped) && r.image < imageCourante) return 1;
			return 0;
		}
		// TargetRelativePosition (FUN_005feed0) : la cible de genre +0x14 (FUN_00477ab0 : 0 la cible verrouillée, 1 le
		// partenaire de prise +0x155c ; les autres genres ne sont pas suivis) à au plus +4 mètres, et dans la direction
		// +8 (radians, depuis l'avant du piéton, vers sa droite) à +0xc près ; l'octet +0x10 en fait aussi des partenaires
		// de prise s'ils n'en ont pas (PedSetGrappleTarget). Dans l'outil, l'avant est (cos cap, sin cap).
		if(type == kPositionCible){
			if(moi == nil) return 0;
			int genre = (int)mots[4], h = genre == 0 ? moi->verrou : genre == 1 ? moi->cibleDePrise : -2;
			if(h == -2) return -1;
			const CScriptPed *c = pietons.Acceder(h);
			if(c == nil) return 0;
			float dx = c->pos.x - moi->pos.x, dy = c->pos.y - moi->pos.y, dz = c->pos.z - moi->pos.z;
			float dist = sqrtf(dx * dx + dy * dy + dz * dz), portee, angle, tolerance;
			memcpy(&portee, &mots[0], 4); memcpy(&angle, &mots[1], 4); memcpy(&tolerance, &mots[2], 4);
			if(dist > portee) return 0;
			float a = moi->cap * 3.14159265f / 180.0f, fx = cosf(a), fy = sinf(a);
			float n = dist > 0.0f ? dist : 1.0f, ux = dx / n, uy = dy / n, uz = dz / n;
			float vu = fx * ux + fy * uy;
			if(vu > 1.0f) vu = 1.0f; else if(vu < -1.0f) vu = -1.0f;
			float vers = acosf(vu);
			if(fy * ux - fx * uy + 0.0f * uz < 0.0f) vers = 6.2831855f - vers;   // à gauche : 2π moins l'angle
			float ecart = remainderf(vers - angle, 6.2831855f);
			if(fabsf(ecart) > tolerance) return 0;
			if((mots[3] & 0xff) == 0) return 1;
			CScriptPed *mp = pietons.Acceder(ped), *mc = pietons.Acceder(h);
			if(mp->cibleDePrise < 0 && mc->cibleDePrise < 0){ mp->cibleDePrise = h; mc->cibleDePrise = ped; return 1; }
			return mp->cibleDePrise == h;
		}
		// HumiliationGroupIs (FUN_006039b0) : le groupe d'humiliation du piéton (+0xd44, posé par FUN_0048f5d0) égal à +4.
		if(type == kGroupeHumiliation) return moi ? moi->groupeHumiliation == (int32)mots[0] : -1;
		// CharacterSize (FUN_00601460) : la taille du modèle (+0x74 du modelinfo, colonne SIZE de l'IDE) égale à +4.
		if(type == kTaille){
			if(moi == nil) return -1;
			auto it = modeles.tailleDuPieton.find(moi->modele);
			return it != modeles.tailleDuPieton.end() && it->second == (int32)mots[0];
		}
		// PedFightingSystem (FUN_005fd740) : +4 égal au système de combat du piéton (+0x16dc, PedSetFightingSystem).
		if(type == kSysteme) return moi && (int32)mots[0] == moi->systemeCombat;
		// Health (FUN_005fd8f0) : la santé (+0x1cb8) comparée à +4 par le comparateur +8 (FUN_005fb750) ;
		// PercentageHealth (FUN_005fd930) : la santé sur la santé maximale (+0x1ca4).
		if(type == kSante) return moi ? ComparerCondition(moi->sante, seuil, mots[1]) : -1;
		if(type == kPourcent) return moi && moi->santeMax > 0 ? ComparerCondition(moi->sante / moi->santeMax, seuil, mots[1]) : -1;
		// IsHealthAtHumiliationLevel (FUN_00601a90) : un piéton qui n'est pas une fille (PedIsFemale +0x50, non suivi) et
		// dont la colonne BM (0x3f) est positive, si sa santé sur son maximum est sous BM / 100 ; la seconde voie (la
		// peur envers le joueur, FUN_00496840 < 5 et l'émotion 4) n'est pas suivie.
		if(type == kHumiliation){
			if(moi == nil || moi->stats[0x3f] <= 0 || moi->santeMax <= 0) return 0;
			return moi->sante / moi->santeMax < moi->stats[0x3f] / 100.0f;
		}
		// stat (0x5ffff0, hors de l'index, lue à l'assembleur de 0x5ffff0 à 0x6000b3) : si l'octet +9, faux quand le
		// contrôleur de l'IA du piéton (+0x290 du composant +0x2e0) joue le nœud de DAT_01b65288 (FUN_005f46a0 ; non suivi :
		// l'octet est nul dans les arbres relevés) ; puis la colonne +0xc (FUN_00476f10, GameGetPedStat), un entier passé en
		// flottant. L'octet +8 nul : vrai si elle vaut au moins +4. L'octet +8 posé, un tirage : avec l'octet +0x10, le
		// dé +0x14 du contexte (+0x28 + 4 × (+0x14)) × 100 ; sinon rand() du CRT (0x85c6ca) / 32767 × 100 (les
		// doubles 0x900ce8 et 0x900130), et vrai si la colonne le dépasse strictement (fcompp, test ah, 0x41) : la colonne
		// est une chance en pour cent.
		if(type == kStat){
			if((mots[1] & 0xff00) != 0 || moi == nil || mots[2] >= 64) return -1;
			float v = (float)(int32)moi->stats[mots[2]];
			if((mots[1] & 0xff) == 0) return v >= seuil;
			float x = (mots[3] & 0xff) ? De(mots[4]) * 100.0f : (float)((double)RandCrt(graineIA) / 32767.0 * 100.0);
			return v > x;
		}
		// HitTime (FUN_005fc400) : le temps depuis le coup (+0x434), ou l'intervalle d'avant (+0x438) si l'octet +0xc,
		// comparé à +4 par +8. Sans coup reçu, +0x434 et +0x438 valent -1 : le constructeur du relevé (FUN_005f3ab0, ecx =
		// le composant + 0x408 en 0x484b5f) les pose à _DAT_009009ac, et FUN_004850b0 ne fait pas courir un -1.
		// DamagePending (FUN_005fbf00) : les dégâts du relevé (+0x440) comparés à +4 par +8.
		if(type == kTempsCoup || type == kEnAttente){
			auto it = coups.find(ped);
			float v = type == kTempsCoup ? -1.0f : 0.0f;   // sans coup : -1, qui ne court pas
			if(it != coups.end()){
				if(type == kEnAttente) v = it->second.degats;
				else v = (mots[2] & 0xff) ? it->second.precedent : t - it->second.instant;
			}
			return ComparerCondition(v, seuil, mots[1]);
		}
		if(type == kMoi) return arbres.JoueHachage(NoeudJoue(ped), mots[0] & 0x7fffffff);
		if(type == kDrapeau || type == kZone){
			auto it = coups.find(ped);
			bool enCours = it != coups.end() && t <= it->second.jusqua;
			if(type == kZone) return enCours;
			return enCours && it->second.drapeau == (mots[0] & 0x7fffffff);
		}
		if(type == kDemande){
			// ActionRequest (ConditionActionIntentionActionRequest 0x9368d4, FUN_005fd570, vérifié au registre : la chaîne
			// 0x939b90, le créateur 0x6456d0) lit le bouton brut +4 ; ActionRequestButton (0x9368fc, FUN_005fd650) passe
			// par ActionMapButes (DAT_020c88d0 +0x18 + 4 × n). Pour un autre que le joueur, ses boutons appuyés par l'IA.
			if(ped != SCRIPT_PED_JOUEUR){
				auto it = boutonsPietons.find(ped);
				unsigned char etat = it != boutonsPietons.end() && mots[0] < BOUTONS_PIETON ? it->second[mots[0]] : 0;
				return EtatBoutonCondition(etat, mots[1]);
			}
			auto it = demandes.find(mots[0]);
			// La manette de l'outil : une demande en attente pose le bit 0 du bouton ; la durée tenue n'est pas suivie.
			unsigned char etat = it != demandes.end() && maintenant <= it->second ? 1 : 0;
			return EtatBoutonCondition(etat, mots[1]);
		}
		if(type == kTemps){
			bool ok; float v = TempsDuNoeud(&ok);
			if(!ok) return -1;
			return ComparerCondition(v, seuil, mots[1]);
		}
		if(type == kCible){
			const CScriptPed *p = pietons.Acceder(ped);
			if(p == nil || p->verrou < 0) return 0;
			return arbres.JoueHachage(NoeudJoue(p->verrou), mots[0] & 0x7fffffff);
		}
		if(type == kLua){
			// L'enregistrement : +0xc (le mot +0x10) le script, +0x10 (le mot +0x14) la fonction (« mission », « F_CanBlock »).
			return AppelLuaCond(f.ChaineCitee((uint32)d + 0x10), f.ChaineCitee((uint32)d + 0x14));
		}
		return JugerIA(f, d, type, mots, seuil);
	}
	// Les conditions des arbres de l'IA (AI_FightTutorial et son CombatActions ; table d'enregistrement FUN_0064b090, la
	// fonction de jugement à l'emplacement 1 de chaque table virtuelle).
	int JugerIA(const CActionTreeFile &f, int32 d, uint32 type, const uint32 mots[6], float seuil) const {
		static const uint32 kObjectif = ActionHash("Objective"), kPoserDrapeau = ActionHash("SetPedFlags"),
		                    kInterdites = ActionHash("AttacksDisabled"), kPrint = ActionHash("Print"),
		                    kAcquerir = ActionHash("AcquireTarget"), kJoindre = ActionHash("JoinCombatZone"),
		                    kHorsPortee = ActionHash("IsTargetOutOfReach"), kIntervalle = ActionHash("Interval"),
		                    kHasard = ActionHash("Random"), kPondere = ActionHash("WeightedRandom"),
		                    kDistance = ActionHash("RangeToTarget"), kDansZone = ActionHash("InCombatZone"),
		                    kIntention = ActionHash("ActionNodeRequest"), kBranche = ActionHash("OpeningBranchChildHasChild"),
		                    kGroupeCible = ActionHash("TargetConditionGroup"), kCone = ActionHash("InAttackCone"),
		                    kHauteur = ActionHash("TargetRelativeHeight");
		CScriptPed *moi = pietons.Acceder(ped);
		// Objective (FUN_00606c00) : l'objectif courant (FUN_004701f0) est du type +4 ; un objectif d'attaque (7 ou 16)
		// qui a une cible la pose en +0x540 (FUN_0048c330 sur le composant de visée +0x3bc). Le cas du type 8
		// (FUN_0077e470, FUN_00606b40) n'est pas suivi.
		if(type == kObjectif){
			const CObjectifPieton *o = pietons.ObjectifCourant(ped);
			if(o == nil || o->type != (int32)mots[0]) return 0;
			if((o->type == OBJECTIF_ATTAQUER_PIETON || o->type == OBJECTIF_ATTAQUER_16) && pietons.Acceder(o->cible)) moi->cibleObjectif = o->cible;
			return 1;
		}
		// SetPedFlags, en condition (FUN_005fd720) : le drapeau +8 du piéton reçoit l'octet +4 ; toujours vraie. La banque
		// Combat d'AI_FightTutorial lève ainsi le drapeau 54.
		if(type == kPoserDrapeau){
			if(moi == nil || mots[1] >= SCRIPT_PED_DRAPEAUX) return -1;
			moi->drapeaux[mots[1]] = (int32)(mots[0] & 0xff);
			return 1;
		}
		if(type == kInterdites) return 0;   // AttacksDisabled (FUN_006094f0) rend toujours faux
		if(type == kPrint) return 1;        // Print (FUN_00609b30) : un message de mise au point, vraie
		// AcquireTarget (FUN_00601760 → FUN_00499320) : si l'octet +0x1de4 le demande, la cible est choisie de nouveau
		// (FUN_004992a0, CScriptPedPool::ChoisirCible) ; vraie si le piéton a une cible (PedGetTargetPed). FUN_00499340 lève cet
		// octet puis le rabat en choisissant, à chaque image de chaque piéton de l'IA (CScriptPedPool::SuivreCibles) : il ne reste
		// levé que pour un piéton qui n'a pas encore eu son image, celui qu'on vient de créer.
		if(type == kAcquerir){
			if(moi == nil) return 0;
			if(!choisis.count(ped)){
				CScriptPedPool::ButesCibles b;
				b.nonVuMax = ButeIA(100, 6.0f); b.portee = ButeIA(104, 30.0f); b.punitionMin = (int32)butesIA.Mot(108);
				b.marge = ButeIA(112, 1.0f); b.margeCible = ButeIA(116, 2.0f);
				pietons.ChoisirCible(ped, maintenant, b, TirerVue, nil);
			}
			return CibleDe(ped) >= 0;
		}
		// JoinCombatZone (FUN_00601550) : la cible +0x538, un piéton ; la zone +8 (0 à 2, sinon la première des trois qui
		// passe), en essai seulement (PedJoinCombatZone avec l'octet 1).
		if(type == kJoindre){
			int c = CibleDe(ped);
			if(c < 0) return 0;
			if(mots[1] < 3) return pietons.JoindreZoneCombat(ped, c, (int)mots[1], true);
			for(int z = 0; z < 3; z++) if(pietons.JoindreZoneCombat(ped, c, z, true)) return 1;
			return 0;
		}
		// IsTargetOutOfReach (FUN_00602c40 → FUN_00487990 / FUN_00487560) : la cible (et celle que FUN_00496c30 lui
		// associe) hors d'atteinte : plus haute de +8 mètres (2) en grimpant, ou hors du maillage de navigation
		// (FUN_005ead00). L'outil n'a ni maillage ni escalade : supposé à portée, faux.
		if(type == kHorsPortee) return 0;
		// Interval (FUN_006033b0) : avec un piéton, le tirage de FUN_00465910 à la période +4 (son mot +0x10c, ici son
		// numéro, et le compteur d'images) ; sans piéton, une image sur +4.
		if(type == kIntervalle) return TirageOpportunite(mots[0], (uint16)ped, imageCourante);
		// Random (ConditionRandom, FUN_00600a50) : l'octet +0xd relance le dé +0x10 du contexte (rand() / 32767) ; la valeur
		// est ce dé avec l'octet +0xe, sinon le tirage de l'image du piéton (+0x1748) ; comparée à +4 par +8 (FUN_005fb750).
		// La garde de l'octet +0xc (faux quand le contrôleur de l'IA joue le nœud de DAT_01b65288) n'est pas suivie : nulle
		// dans les arbres relevés.
		if(type == kHasard){
			uint32 i = mots[3];
			if((mots[2] >> 8) & 0xff) De(i) = (float)RandCrt(graineIA) / 32767.0f;
			float v = ((mots[2] >> 16) & 0xff) ? De(i) : moi ? moi->hasardImage : 0.0f;
			return ComparerCondition(v, seuil, mots[1]);
		}
		// WeightedRandom (FUN_00600b00) : faux si le seuil +8 est sous le tirage du contexte (+0x24, 0 au départ) ; sinon le
		// tirage est renouvelé (rand() / 32767) et la condition vraie. Les gardes des octets +0xc (le nœud de DAT_01b65288)
		// et +0xd (FUN_00486760 sur la cible) ne sont pas suivies.
		if(type == kPondere){
			float s8; memcpy(&s8, &mots[1], 4);
			if(s8 < Tirage()) return 0;
			Tirage() = (float)RandCrt(graineIA) / 32767.0f;
			return 1;
		}
		// RangeToTarget (FUN_00600780) : la distance à la cible (+0x434, sinon PedGetTargetPed) comparée à +4 par +8.
		if(type == kDistance){
			const CScriptPed *c = pietons.Acceder(CibleDe(ped));
			if(moi == nil || c == nil) return 0;
			return ComparerCondition(DistancePietons(moi, c), seuil, mots[1]);
		}
		// InCombatZone (FUN_006015e0) : la distance à la cible dans la zone +4, élargie de +8 des deux côtés ; la zone 0
		// va de 0 à AIButes +0x2c, la 1 de +0x2c à +0x30, la 2 de +0x30 à 100 m (0x900490).
		if(type == kDansZone){
			const CScriptPed *c = pietons.Acceder(CibleDe(ped));
			if(moi == nil || c == nil) return 0;
			float dedans = 0.0f, dehors = 0.0f, tol; memcpy(&tol, &mots[1], 4);
			if(mots[0] == 0){ dedans = 0.0f; dehors = ButeIA(44, 1.5f); }
			else if(mots[0] == 1){ dedans = ButeIA(44, 1.5f); dehors = ButeIA(48, 3.0f); }
			else if(mots[0] == 2){ dedans = ButeIA(48, 3.0f); dehors = 100.0f; }
			float dist = DistancePietons(moi, c);
			return dedans - tol <= dist && dist <= dehors + tol;
		}
		// ActionNodeRequest (ConditionIntentionActionNodeRequest, FUN_005fd620) : l'intention du piéton (+0x254) est le
		// nom du nœud jugé (le +0x34 du contexte).
		if(type == kIntention){
			auto it = intentionsPietons.find(ped);
			return noeud && it != intentionsPietons.end() && it->second == (noeud->hachage & 0x7fffffff);
		}
		// OpeningBranchChildHasChild (FUN_00602710) : la racine de l'arbre d'actions du piéton (le +0x40 du contexte du
		// contrôleur d'actions) a un enfant nommé +4, qui a lui-même un enfant nommé +8.
		if(type == kBranche){
			auto it = controleurs.find(ped);
			if(it == controleurs.end() || it->second.arbre == nil) return 0;
			for(const CActionNode &e : CActionBank::Enfants(*it->second.arbre))
				if((CActionBank::Nom(e) & 0x7fffffff) == (mots[0] & 0x7fffffff)){
					for(const CActionNode &g : CActionBank::Enfants(e)) if((CActionBank::Nom(g) & 0x7fffffff) == (mots[1] & 0x7fffffff)) return 1;
					return 0;
				}
			return 0;
		}
		// TargetConditionGroup (FUN_00601cc0) : le groupe de conditions (+4, par la seconde table de renvois) jugé pour la
		// cible : le partenaire de prise (+0x155c) si l'octet +8, sinon PedGetTargetPed ; faux sans cible.
		if(type == kGroupeCible){
			int c = (mots[1] & 0xff) ? (moi ? moi->cibleDePrise : -1) : CibleDe(ped);
			std::vector<int32> groupe;
			if(pietons.Acceder(c) == nil || !f.GroupeDe(d, groupe)) return 0;
			return arbres.JugerListe(f, groupe, CJugeOutil(c, t)) == 1;
		}
		// AttackerConditionGroup (ConditionAttackerConditionGroup 0x9370a0, FUN_00601e00) : le groupe +4 jugé pour l'attaquant du
		// dernier coup reçu (le +4 du relevé, PedGetLastHitWeapon : +0x40c du composant +0x2e0), dans son propre contexte
		// (l'emplacement +0x40 de sa table) ; faux sans piéton ni attaquant. Le véhicule du contexte (+0xc, son conducteur
		// +0x1c0) n'est pas suivi.
		static const uint32 kGroupeAttaquant = ActionHash("AttackerConditionGroup");
		if(type == kGroupeAttaquant){
			auto it = coups.find(ped);
			int a = it != coups.end() ? it->second.attaquant : -1;
			std::vector<int32> groupe;
			if(moi == nil || pietons.Acceder(a) == nil || !f.GroupeDe(d, groupe)) return 0;
			return arbres.JugerListe(f, groupe, CJugeOutil(a, t)) == 1;
		}
		// Le groupe des alliés (src/peds/ScriptPedPool : le meneur +0x2f4, le suiveur +0x2f8). IsAllyFollower (ConditionIsAllyFollower
		// 0x93762c, FUN_006037b0 → FUN_004851d0) : un meneur ; IsAllyLeader (0x93761c, FUN_00603790 → FUN_004851e0) : un suiveur ;
		// IsTargetAlly (0x93763c, FUN_006037d0) : la cible (PedGetTargetPed, un piéton : les 3 bits bas de +0x108 à 3) est dans la
		// chaîne de ses suiveurs (FUN_004853b0). HasAllyState (ConditionAllyState 0x937d4c, FUN_00606450) et HasAllyMorale
		// (ConditionAllyMorale 0x937d60, FUN_00606470) : +0x318 et +0x31c égaux au mot +4. AllyLeaderConditionGroup (0x93791c,
		// FUN_00604610) : le groupe +4 jugé pour le premier meneur de la chaîne (FUN_00485410), dans son propre contexte
		// (l'emplacement 16 de sa table) ; AllyFollowerConditionGroup (0x93793c, FUN_00604660) : pour le suiveur (+0x2f8). Faux
		// sans eux.
		static const uint32 kEstSuiveur = ActionHash("IsAllyFollower"), kEstMeneur = ActionHash("IsAllyLeader"),
		                    kCibleAlliee = ActionHash("IsTargetAlly"), kEtatAllie = ActionHash("HasAllyState"),
		                    kMoralAllie = ActionHash("HasAllyMorale"), kGroupeMeneur = ActionHash("AllyLeaderConditionGroup"),
		                    kGroupeSuiveur = ActionHash("AllyFollowerConditionGroup");
		if(type == kEstSuiveur) return moi && moi->meneur >= 0;
		if(type == kEstMeneur) return moi && moi->suiveur >= 0;
		if(type == kCibleAlliee) return moi && pietons.DansSesSuiveurs(ped, CibleDe(ped));
		if(type == kEtatAllie) return moi && moi->etatAllie == (int32)mots[0];
		if(type == kMoralAllie) return moi && moi->moralAllie == (int32)mots[0];
		// AllyIsInCombat (ConditionAllyIsInCombat 0x937d38, FUN_00606360) : un tirage de période 4 (FUN_00465910), un meneur et
		// l'octet +0x304 ; parmi ses piétons proches (+0x3c0, la liste +0x3c4) vivants, le premier dont la cible (PedGetTargetPed,
		// un piéton) est dans son groupe (FUN_00485440 : lui, ses meneurs, ses suiveurs) et dont l'objectif courant (FUN_004701f0)
		// attaque (7 ou 16) devient la cible de son objectif (FUN_0048c330) et sa cible (FUN_004989c0 → FUN_00497d20(q, 0)) ; la
		// cible de son arme (FUN_004d7890) n'est pas suivie. Vrai alors.
		static const uint32 kAllieEnCombat = ActionHash("AllyIsInCombat");
		if(type == kAllieEnCombat){
			if(moi == nil || !TirageOpportunite(4, (uint16)ped, imageCourante) || moi->meneur < 0 || !moi->groupe304) return 0;
			for(int q : moi->proches){
				const CScriptPed *a = pietons.Acceder(q);
				if(a == nil || a->mort || !pietons.DansSonGroupe(ped, CibleDe(q))) continue;
				const CObjectifPieton *o = pietons.ObjectifCourant(q);
				if(o == nil || (o->type != OBJECTIF_ATTAQUER_PIETON && o->type != OBJECTIF_ATTAQUER_16)) continue;
				moi->cibleObjectif = q;
				pietons.Verrouiller(ped, q, 0);
				return 1;
			}
			return 0;
		}
		if(type == kGroupeMeneur || type == kGroupeSuiveur){
			int a = moi == nil ? -1 : type == kGroupeMeneur ? pietons.PremierMeneur(ped) : moi->suiveur;
			std::vector<int32> groupe;
			if(pietons.Acceder(a) == nil || !f.GroupeDe(d, groupe)) return 0;
			return arbres.JugerListe(f, groupe, CJugeOutil(a, t)) == 1;
		}
		// InAttackCone (FUN_00602200 → FUN_004865a0) : la cible à moins d'AIButes +0x54 (1,5 m) et devant le piéton (le
		// cosinus de l'angle au moins +0x58, 0,76). Le dernier test (FUN_0048d320, l'un voit l'autre) est supposé vrai.
		if(type == kCone){
			const CScriptPed *c = pietons.Acceder(CibleDe(ped));
			if(moi == nil || c == nil) return 0;
			float dx = c->pos.x - moi->pos.x, dy = c->pos.y - moi->pos.y, n = sqrtf(dx * dx + dy * dy);
			if(sqrtf(dx * dx + dy * dy) > ButeIA(84, 1.5f)) return 0;
			if(n <= 0.0f) return 1;
			float a = moi->cap * 3.14159265f / 180.0f;
			return (cosf(a) * dx + sinf(a) * dy) / n >= ButeIA(88, 0.76f);
		}
		// TargetRelativeHeight (FUN_00603170) : la tranche +8 … +0xc de la hauteur du piéton (sa boîte de collision, +0x18
		// et +0x28 du modèle) recouvre la hauteur de la cible. L'outil n'a pas les boîtes : 1,8 m depuis les pieds, supposé.
		if(type == kHauteur){
			const CScriptPed *c = pietons.Acceder((mots[0] & 0xff) ? (moi ? moi->cibleDePrise : -1) : CibleDe(ped));
			if(moi == nil || c == nil) return 0;
			float bas, haut; memcpy(&bas, &mots[1], 4); memcpy(&haut, &mots[2], 4);
			const float h = 1.8f;
			return moi->pos.z + bas * h <= c->pos.z + h && c->pos.z <= moi->pos.z + haut * h;
		}
		// Le véhicule du piéton (+0x1554, le CVehicle où il est ; l'octet +0x1558 : il y est assis) et son genre (+0x46c du
		// véhicule : 0 une voiture, 1 un vélo). FUN_004ce540 (ecx = le piéton) rend l'octet +0x1558, faux sans véhicule.
		// OnBike (ConditionOnBike 0x936ec0, FUN_00600600) : FUN_004ce540 et le genre 1 ; InCar (ConditionInCar 0x936eb0,
		// FUN_006005c0) : FUN_004ce540 et le genre 0. L'outil n'a pas de véhicules : les deux sont fausses.
		// TargetVehicleCar (0x93725c, FUN_005fe260) et TargetVehicleBike (0x937270, FUN_005fe4d0) : pour le joueur (type 13), le
		// véhicule le plus proche à moins de +4 m (FUN_004d32a0 ; un genre 0, puis 1) devient le véhicule visé (+0x430), faux s'il
		// n'y en a pas ; pour un autre, le véhicule visé déjà retenu (+0x430) s'il est du bon genre, sinon, au tirage de l'image
		// (FUN_00465910(8), sans effet), le plus proche (FUN_004d32a0) ; sans véhicule, +0x430 est vidé et la condition fausse,
		// avant tout calcul de distance (FUN_00414a20, FUN_0085c4d0 sous +4). Sans véhicules dans l'outil : fausses.
		// IsTargetAttackRightsTaken (0x936da4, FUN_005ffc70, lue à l'assembleur de 0x5ffc70 à 0x5ffc9e) : la cible du piéton
		// (PedGetTargetPed, FUN_00496fb0) est un piéton et l'octet +0x344 de son composant de combat (+0x668, FUN_004861b0) est
		// posé : ses droits d'attaque sont pris, par n'importe quel attaquant (CScriptPedPool::DroitsPrisSurCible).
		// FightToken (ConditionFightToken 0x936b30, FUN_00601f70, lue à l'assembleur de 0x601f70 à 0x601fb8) : la cible
		// (PedGetTargetPed) est un piéton et FUN_00486760 (ecx = son composant de combat +0x668) trouve au piéton un jeton non
		// coupé (l'octet +0xc de l'entrée) ou la place du cône (+0x324, non coupée +0x330) ; vrai pour une cible qui n'est
		// pas un piéton, faux sans cible (CScriptPedPool::TientJeton).
		// Les dernières classes de AI.act jugées dans 1_01 à 1_03 (lues le 03.11.2026) :
		// - DetectProjectile (0x9378ec, FUN_00604410 → FUN_0048a300) et DetectGrenade (0x9378dc, FUN_00604380 → FUN_0048a130) :
		//   les projectiles en vol de la table 0xc3a068 (l'octet actif en -0x1c de chaque entrée, FUN_004d3c80), un qui menace
		//   le piéton (sa trajectoire, FUN_0041a380 sous +4 m) ; rien sans projectile, et rien pour un piéton à +0x1360. L'outil
		//   n'a pas de projectiles : fausses.
		// - LiquidLevel (0x92e5e0, FUN_005ff7f0) : l'octet +0x16cc (dans l'eau), puis la hauteur de la surface (+0x16d0) sous
		//   le piéton comparée à +4 par +8 ; hors de l'eau, faux. L'outil n'a pas d'eau.
		// - IsTandemHelper (0x936b0c, FUN_00606f00) : le complice +0x9b4 (vidé s'il ne le désigne plus, +0x9b8) ; les tandems ne
		//   sont appelés par aucun script de 1_01 à 1_05 : fausse. (IsAllyFollower et IsAllyLeader : plus haut, le groupe des alliés.)
		// - Stimulus (0x9370c0, FUN_00600340) et IsStimulusPriority (0x93782c, FUN_00603fe0) : le stimulus courant du piéton
		//   (+0x1d30), son genre +0 comparé à +4, sa priorité (FUN_004a5ba0) comparée à +4 par +8 ; sans stimulus, fausses.
		//   L'outil n'a pas de stimuli (les pistes CreateStimulus de AI.act ne sont pas jouées).
		// - WeaponEquipedType (0x9365b8, FUN_0065be50) : le type de l'arme tenue (WeaponGetType, +0x68 / +0x69) ; sans arme
		//   (FUN_00474aa0 faux) ni arme voulue (+0x1574 à -1), faux.
		// - PedIsTrigger (0x937344, FUN_00602350, lue à l'assembleur : ecx = 0xbcd098, puis FUN_00424490) : le piéton est l'un
		//   des déclencheurs de personnage (+0x1804 leur nombre, un handle tous les 0x100 octets, FUN_005c7220(h, 2)) que pose
		//   PedCharacterTrigger (0x5c6660), qu'aucun script de 1_01 à 1_03 n'appelle : faux.
		// - CheckActionTimer (0x937500, FUN_006033e0 → FUN_00474870, lue à l'assembleur : sete) : la minuterie d'action
		//   (+0x1e0c) ne dépasse pas maintenant × 0,001.
		// - IsSpeechBusy (0x92e754, FUN_006025e0 → FUN_005a27b0) : avec l'octet +4, le piéton parle (la ligne en cours ou la
		//   précédente, DAT_00cf458c + 0x5c × (DAT_00cf4644 & 1), son locuteur +0x10), sinon une ligne quelconque ; la file
		//   (FUN_0059f050) et FUN_005a0bd0 ne sont pas suivies : la parole de l'outil, une ligne à la fois.
		// - SocialAnimIs (0x93767c, FUN_00603940) : dans les états sociaux 1 et 3, l'animation sociale (+0xc24) vaut +4 ; sinon
		//   +4 vaut 25. IsWantingToSocialize (0x9376cc, FUN_00603ab0) : le bit 4 de l'octet +0xd68 et le partenaire voulu
		//   (+0xbb8). L'outil ne joue pas le système social (l'état reste 0, aucun partenaire voulu).
		static const uint32 kProjectile = ActionHash("DetectProjectile"), kGrenade = ActionHash("DetectGrenade"),
		                    kEau = ActionHash("LiquidLevel"), kTandem = ActionHash("IsTandemHelper"),
		                    kStimulus = ActionHash("Stimulus"), kPrioriteStimulus = ActionHash("IsStimulusPriority"),
		                    kTypeArme = ActionHash("WeaponEquipedType"), kDeclencheurPerso = ActionHash("PedIsTrigger"),
		                    kMinuterieAction = ActionHash("CheckActionTimer"), kParoleOccupee = ActionHash("IsSpeechBusy"),
		                    kAnimSociale = ActionHash("SocialAnimIs"), kVeutSocialiser = ActionHash("IsWantingToSocialize");
		if(type == kProjectile || type == kGrenade || type == kEau || type == kTandem ||
		   type == kStimulus || type == kPrioriteStimulus || type == kDeclencheurPerso || type == kVeutSocialiser)
			return 0;
		// WeaponEquipedType (FUN_0065be50) : l'arme en passage avec l'octet +8 (sinon l'arme en main) ; sans elle, faux ; le genre +4 :
		// 0 l'octet +0x68 du type, 1 +0x68 et +0x69, 2 +0x69 ; un autre, faux.
		if(type == kTypeArme){
			if(moi == nil) return 0;
			int32 a = (mots[1] & 0xff) && moi->armePassage != -1 ? moi->armePassage : moi->armeTenue;
			if(a == -1) return 0;
			const CTypeArme &t = armes.Type(a);
			return mots[0] == 0 ? t.tir68 : mots[0] == 1 ? t.tir68 && t.tir69 : mots[0] == 2 ? t.tir69 : 0;
		}
		if(type == kMinuterieAction) return moi ? !(moi->minuterieAction > (float)maintenant * 0.001f) : -1;
		if(type == kParoleOccupee) return ParoleEnCours((mots[0] & 0xff) ? ped : -1);
		if(type == kAnimSociale){
			if(moi && (moi->etatSocial == 1 || moi->etatSocial == 3)) return -1;   // +0xc24 non suivi
			return mots[0] == 25;
		}
		// IsSocializingWithPlayer (ConditionIsSocializingWithPlayer 0x9376bc, FUN_00603a70) : le piéton a un état social (+0xbb0)
		// et son partenaire (+0xbb4) est le joueur (DAT_00c1aea8). TargetedByPlayer (0x9370f4, FUN_00601be0) : faux sans joueur ;
		// vrai si le piéton est la cible du joueur (PedGetTargetPed, un piéton : l'octet +0x108 & 7 vaut 3) ou celle de son
		// composant de visée (+0xa0, FUN_004d5980), que l'outil ne distingue pas de sa cible (--viser, FUN_00497d20).
		static const uint32 kSocialJoueur = ActionHash("IsSocializingWithPlayer"), kViseParJoueur = ActionHash("TargetedByPlayer");
		if(type == kSocialJoueur) return moi && moi->etatSocial != 0 && moi->partenaireSocial == SCRIPT_PED_JOUEUR;
		if(type == kViseParJoueur){
			const CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
			return j && j->verrou >= 0 && j->verrou == ped;
		}
		static const uint32 kJeton = ActionHash("FightToken");
		if(type == kJeton) return pietons.TientJeton(ped);
		static const uint32 kDroitsPris = ActionHash("IsTargetAttackRightsTaken");
		if(type == kDroitsPris) return pietons.DroitsPrisSurCible(ped);
		static const uint32 kVelo = ActionHash("OnBike"), kVoiture = ActionHash("InCar"),
		                    kVoitureVisee = ActionHash("TargetVehicleCar"), kVeloVise = ActionHash("TargetVehicleBike");
		if(type == kVelo || type == kVoiture || type == kVoitureVisee || type == kVeloVise) return 0;
		// Le POI d'un piéton (+0x1d24 : le point d'intérêt qu'il occupe, posé par FUN_004730b0, vidé par la remise FUN_00474f50,
		// FUN_00479440 et PedClearPOIForAllPeds ; son genre en +0x70). IsHangingOut (ConditionIsHangingOut 0x936d4c,
		// FUN_005ffb90) : un POI du genre 2 ; HavePOI (0x936b44, FUN_005ffcd0, lue à l'assembleur de 0x5ffcd0 à 0x5ffcf0) : un POI
		// occupé ou demandé (+0x1d20) ; HaveDynamicPOI (0x93747c, FUN_005ffd30) : un POI mobile libre du gestionnaire des POI
		// mobiles (FUN_00448310 : ses places, leur nombre en +0xa0, FUN_004481b0) ; IsScriptedAmbient (0x936d94, FUN_005ffc40) : un POI du genre 3 à 15 ;
		// IsScenarioPed (0x937b28, FUN_005ffbf0 → FUN_004730d0, ecx = le piéton) : un POI du genre 3, ou le scénario +0x14cc
		// (posé par le système des scénarios, FUN_00716410 et ses voisines). NearbyDockLocation (0x936e70, FUN_00601150) : vrai si
		// le piéton a déjà une place d'arrimage (+0x1d1c) ; sinon le gestionnaire des places (FUN_0046e8c0) en tire une au rand()
		// du CRT parmi les siennes (+0xa0), 0 sans place et sans tirage, et la garde si un second rand() tombe sous son octet
		// +0x100. IsGuarding (0x9373fc, FUN_00602bf0) : l'entité gardée (+0x1e08, la référence que le constructeur de CPed vide,
		// FUN_0047ffe0) ; IsTethered (0x93744c, FUN_00603040) : la laisse (+0x360, le déclencheur de FUN_00484ab0) ; IsPatrolPed
		// (0x937a9c, FUN_005ffe60) : l'octet +0x1570, qui dit qui a créé le piéton (FUN_004770a0), vaut 4. NeedsToDive (0x937550,
		// FUN_005ffe80) : l'octet +0x16d4 positif et l'entité à éviter +0x16d8, puis FUN_004777e0 (devant lui ou non) comparé à
		// l'octet +4 ; faux sinon. L'outil n'a ni POI, ni scénarios, ni places d'arrimage (AreaAddDockLocation est un bouchon),
		// ni gardes, ni patrouilles, ni entités qui foncent sur un piéton : neuf sont fausses ; IsTethered lit la laisse que pose
		// PedSetTetherToPoint (le genre +0x360).
		static const uint32 kTraine = ActionHash("IsHangingOut"), kAmbiantScripte = ActionHash("IsScriptedAmbient"),
		                    kScenario = ActionHash("IsScenarioPed"), kArrimage = ActionHash("NearbyDockLocation"),
		                    kGarde = ActionHash("IsGuarding"), kLaisse = ActionHash("IsTethered"),
		                    kPatrouille = ActionHash("IsPatrolPed"), kPlonger = ActionHash("NeedsToDive"),
		                    kAvoirPoi = ActionHash("HavePOI"), kPoiMobile = ActionHash("HaveDynamicPOI");
		if(type == kLaisse){ const CScriptPed *p = pietons.Acceder(ped); return p && p->laisse != 0; }   // PedSetTetherToPoint
		if(type == kAvoirPoi || type == kPoiMobile || type == kTraine || type == kAmbiantScripte || type == kScenario || type == kArrimage || type == kGarde ||
		   type == kPatrouille || type == kPlonger) return 0;
		// VisibleArea (ConditionVisibleArea 0x936d14, FUN_005fc7c0) : la zone visible (DAT_00bd1008, AreaGetVisible) est +4.
		// IsNISorCutsceneRunning (0x937e84, FUN_005fd2c0) : avec l'octet +4, un NIS en cours (FUN_005bb220 : l'octet
		// DAT_00cfb340, qu'EnterNIS pose et ExitNIS efface) ; avec l'octet +5, une cinématique en cours (DAT_020c5be4,
		// GetCutsceneRunning). IsRumbleModeAmbient et IsRumbleModeMission (ConditionIsRumbleMode_Ambient 0x937b00, FUN_005fd0f0 ;
		// _Mission 0x937b14, FUN_005fd100) : les octets DAT_00c2afa0 et DAT_00c2afa1, que PedSetGlobalAttitude_Rumble pose et
		// que l'initialisation des attitudes (FUN_0046f660) et leur remise (0x46f350) effacent. PlayerIsInStealthProp (0x937ab0,
		// FUN_005fd000) : l'octet +0x1e91 du joueur (DAT_00c1aea8), que pose l'entrée dans une cachette (casier, poubelle) :
		// l'outil n'en a pas, faux. TimeCond (CTimeCond 0x9372d4, FUN_006a7380 → FUN_00425cf0) : la même classe que dans
		// Missions.cat, l'heure du jeu dans [+4 h +0xc min, +8 h +0x10 min[ (CMissionBank::Evaluer).
		static const uint32 kZoneVisible = ActionHash("VisibleArea"), kNis = ActionHash("IsNISorCutsceneRunning"),
		                    kRumbleAmbiant = ActionHash("IsRumbleModeAmbient"), kRumbleMission = ActionHash("IsRumbleModeMission"),
		                    kCachette = ActionHash("PlayerIsInStealthProp"), kHeure = ActionHash("TimeCond");
		if(type == kZoneVisible) return zones.zoneVisible == (int32)mots[0];
		if(type == kNis) return ((mots[0] & 0xff) && nisEnCours) || (((mots[0] >> 8) & 0xff) && CinematiqueEnCours());
		if(type == kRumbleAmbiant) return rumbleAmbiant;
		if(type == kRumbleMission) return rumbleMission;
		if(type == kCachette) return 0;
		if(type == kHeure) return ConditionMission(type, mots);
		nonLues[type]++;
		return -1;
	}
};
// PlayerSetControl(0 / 1) (0x5d12f0) : le joueur rendu au script ou au joueur ; aucun script ne le relit.
static int controleJoueur = 1;
// La parole scriptée (src/audio/SpeechEvents.h, docs/son.md) : une seule ligne à la fois, comme le moteur
// du son la décrit (FUN_005a27b0 : le locuteur, l'événement sur 9 bits, le numéro de ligne sur 11). La
// durée d'une ligne est celle de son échantillon dans Speech.bin (src/audio/SpeechBank.h) : le premier que
// FUN_0059ee10 retiendrait pour le modèle du locuteur et la ligne (le moteur tire au hasard parmi les prises
// pas encore jouées) ; sans échantillon, 2,5 s.
static const float PAROLE_MS = 2500;
static CSpeechBank banqueParole;
static bool banqueParoleLue = false;

static uint32
DureeParole(int ped, int evenement, int ligne)
{
	const CScriptPed *p = ped >= 0 ? pietons.Acceder(ped) : nil;
	if(p == nil || !banqueParoleLue) return (uint32)PAROLE_MS;
	std::vector<int> c = banqueParole.Candidats(evenement, p->modele, ligne);
	int ms = c.empty() ? -1 : banqueParole.DureeMs(c[0]);
	if(ms < 0){
		printf("        parole : aucun échantillon de %s ligne %d pour le modèle %d, 2,5 s\n", kSpeechEvents[evenement], ligne, p->modele);
		return (uint32)PAROLE_MS;
	}
	return (uint32)ms;
}
static struct { bool enCours = false; int ped = -1, evenement = -1, ligne = -1, id = 0; uint32 fin = 0; } parole;
static bool ParoleEnCours(int ped){ return parole.enCours && (ped < 0 || parole.ped == ped); }
// Le préchargement d'une ligne (SoundPreloadSpeech, docs/son.md) : l'outil n'a pas de diffusion, la ligne est
// prête à l'image qui suit, et le rappel FUN_005d49d0 est joué à chaque image tant qu'elle attend.
static CSpeechPreload prechargement;
// CCutsceneMgr réduit à ses drapeaux (docs/cinematiques.md) : la cinématique chargée (DAT_020c5be2), en cours
// (DAT_020c5be4), son temps (DAT_020c5b1c), sur les fichiers de Cuts/Cuts.img (src/cutscene/CutsceneFiles.h).
// La fin, dans le moteur, vient de l'arbre d'actions /Global/CutScenes (FUN_006c3a70) ; ici, faute de le
// jouer, c'est la dernière clé de la piste caméra. --sans-cinematiques : IsCutsceneLoaded répond faux et
// SCutscenes.lua passe directement à StopCutscene, comme quand le chargement échoue.
static struct {
	CCutsceneFiles fichiers;
	bool jouer = true;              // --sans-cinematiques l'éteint
	bool chargee = false;           // DAT_020c5be2
	bool enCours = false;           // DAT_020c5be4
	float temps = 0;                // DAT_020c5b1c, en secondes
	float duree = 0;                // la dernière clé de la piste caméra
	bool son = false;               // DAT_020c5be7 (LoadCutsceneSound)
	size_t sousTitre = 0;           // le prochain sous-titre à afficher
	std::string nomTable; int tailleTable = 0;   // CutSetCutsceneName (DAT_00bceb08), CutSetCutsceneTableSize (DAT_00bceb00)
} cinematique;
static bool CinematiqueEnCours(void){ return cinematique.enCours; }
static struct {
	bool enCours = false, versNoir = false, noir = false; uint32 fin = 0;
	int Etat(void) const { return enCours ? FONDU_EN_COURS : noir ? FONDU_NOIR : FONDU_RENDU; }   // FUN_004f11a0
} fondu;

// La mission en cours et sa machine à états, réduite de FUN_006ad3e0 :
// 1 MissionSetup tourne, 2 main tourne, 3 MissionCleanup tourne.
enum EtatMission { AUCUNE = 0, PREPARATION = 1, DEROULEMENT = 2, NETTOYAGE = 3 };
static struct {
	int etat = AUCUNE;
	int script = -1;
	int indice = -1;                     // dans banque.missions (+0x4bc)
	std::string nom;                     // le nom demandé par le script, ou le titre quand le gestionnaire la lance
	bool reussie = false;
} mission;

// FUN_005fc940 : +4 nul, MissionActive ; sinon l'indice de la mission de ce nom (FUN_006a9e70, -1 sans elle) doit être celui
// de l'un des deux emplacements du gestionnaire (+0x4bc, puis +0x5a0 : FUN_006aa520, FUN_006aa530), dont l'état (+8) n'est
// ni 0 (aucune), ni 1 (demandée), ni 11 (finie) (FUN_006aa290, FUN_006aa2b0). Les états de FUN_006ad3e0 : 2 MissionSetup,
// 4 main, 12, 8 et 9 la fin de main, 10 MissionCleanup, 11 finie, puis 0 : ceux de l'outil (PREPARATION, DEROULEMENT,
// NETTOYAGE) sont tous « en mission ». L'outil n'a qu'un emplacement.
static int
SurMission(uint32 hachage)
{
	if(hachage == 0) return mission.etat != AUCUNE;
	if(mission.etat == AUCUNE || mission.indice < 0 || mission.indice >= (int)banque.missions.size()) return 0;
	return (banque.missions[mission.indice].hachage & 0x7fffffff) == (hachage & 0x7fffffff);
}

// FUN_006a74d0 : le script « mission » est celui de la mission en cours (FUN_006aa630, MissionGetName +0x1c ; aucune :
// faux) ; un autre nom est cherché parmi les objets script sans la casse, « \ » lu « / » (FUN_005dbb60), sinon le premier
// (FUN_005db770(0), main.lua). FUN_005d80a0 en fait le script courant (FUN_005dbc40) et appelle, par FUN_005d8000,
// NSCall(fichier, fonction, false) de util.lua : la fonction de cet espace de noms (CreateNameSpace : les noms des scripts
// de mission n'y sont pas dans _G), 0.0 si elle n'existe pas (le troisième argument faux). Le résultat est lu par
// lua_tonumber (0x73b0d0 : un nombre, ou une chaîne convertible, sinon 0), tronqué en entier (0x85c6f0), et la condition
// est vraie s'il est positif : F_CanBullyAttack et F_CanBlock rendent 1.0 ou 0.0, et un booléen vaut faux.
// La piste Script (FUN_006851a0) : le même choix de l'espace de noms ; la fonction est appelée par NSCall avec, si la piste le
// demande, la poignée du piéton et le nombre de la piste (FUN_005d8460, FUN_005d8480). La réponse n'est pas lue.
static void
AppelScript(const std::string &script, const std::string &fonction, int ped, float nombre, bool avecNombre = false, int declencheur = -2)
{
	int s = -1;
	if(script == "mission"){
		if(mission.etat == AUCUNE || mission.script < 0) return;
		s = mission.script;
	}else{
		std::string voulu = script;
		for(char &c : voulu) if(c == '\\') c = '/';
		for(size_t i = 0; i < scripts.size() && s < 0; i++){
			std::string nom = scripts[i].fichier;
			for(char &c : nom) if(c == '\\') c = '/';
			if(strcasecmp(nom.c_str(), voulu.c_str()) == 0) s = (int)i;
		}
		if(s < 0) s = 0;
	}
	lua_State *L = etatLua;
	int avant = scriptCourant;
	scriptCourant = s;
	lua_getglobal(L, "NSCall");
	lua_pushstring(L, scripts[s].fichier.c_str());
	lua_pushstring(L, fonction.c_str());
	lua_pushboolean(L, 0);
	int n = 3;
	// FUN_006851a0 : la poignée du piéton (l'octet +0x20) ou celle du déclencheur (+0x21, -1 sans), puis le nombre (FUN_005d8480).
	if(ped >= 0){ lua_pushnumber(L, ped); lua_pushnumber(L, nombre); n = 5; }
	else if(declencheur != -2){ lua_pushnumber(L, declencheur); lua_pushnumber(L, nombre); n = 5; }
	else if(avecNombre){ lua_pushnumber(L, nombre); n = 4; }
	if(lua_pcall(L, n, 0, 0) != 0){ printf("        piste Script : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
	scriptCourant = avant;
}

static int
AppelLuaCond(const std::string &script, const std::string &fonction)
{
	if(etatLua == nil || fonction.empty() || scripts.empty()) return -1;
	int s = -1;
	if(script == "mission"){
		if(mission.etat == AUCUNE || mission.script < 0) return 0;
		s = mission.script;
	}else{
		std::string voulu = script;
		for(char &c : voulu) if(c == '\\') c = '/';
		for(size_t i = 0; i < scripts.size() && s < 0; i++){
			std::string nom = scripts[i].fichier;
			for(char &c : nom) if(c == '\\') c = '/';
			if(strcasecmp(nom.c_str(), voulu.c_str()) == 0) s = (int)i;
		}
		if(s < 0) s = 0;
	}
	lua_State *L = etatLua;
	int avant = scriptCourant;
	scriptCourant = s;
	lua_getglobal(L, "NSCall");
	lua_pushstring(L, scripts[s].fichier.c_str());
	lua_pushstring(L, fonction.c_str());
	lua_pushboolean(L, 0);
	int r = -1;
	if(lua_pcall(L, 3, 1, 0) == 0) r = (int)lua_tonumber(L, -1) > 0;
	lua_pop(L, 1);
	scriptCourant = avant;
	return r;
}
// Le gestionnaire de missions (objet 0x020c3ca0, FUN_006ad130 à chaque image), réduit : une mission
// dont les conditions sont vraies est « activée » (son corona existe) ; activée, elle démarre seule
// si son champ 51 le dit, ou, une fois le gestionnaire autorisé (+0x77d, posé par gamemain avec
// SystemAllowMissionManagerToForceRunMissions), quand le joueur est dans le pavé de son corona
// (FUN_006a8650 : demi-côté = champ 28 de MissionButtonTrigger, 2 m pour 1_01, 1 m ailleurs ; le moteur
// affiche ensuite l'invite BUT_LAUNCH et lit la manette avant FUN_006acfa0, l'outil lance tout de
// suite). Les conditions sont évaluées par CMissionBank::Activable (MissionCond, TimeCond, ConditionFalse) ;
// une mission dont une condition est d'une classe pas encore lue reste inactive.
static struct {
	bool actif = false;                  // --nouvelle-partie
	bool autorise = false;               // DAT_020c441d
	std::vector<bool> activees;          // bit 1 de l'octet +4 de l'entrée de 6 octets (+0x688)
	CEtatMissions etat;                  // le mot +2 de chaque entrée (réussites) et l'heure, que les conditions lisent
	bool deplacerJoueur = true;          // SystemShouldMovePlayer : vrai une fois à la nouvelle partie
} gestionnaire;

static int
ConditionMission(uint32 type, const uint32 mots[6])
{
	CMissionCondition c;
	c.type = type & 0x7fffffff;
	for(int i = 0; i < 6; i++) c.mots[i] = mots[i];
	if(gestionnaire.etat.reussites.size() != banque.missions.size()) gestionnaire.etat.reussites.assign(banque.missions.size(), 0);
	return banque.Evaluer(c, gestionnaire.etat);
}

// Le nom d'une mission pour le journal : en clair quand le fichier le donne, sinon son titre.
static const char *
NomMission(const CMissionData &m){ return m.nom.empty() ? m.titre.c_str() : m.nom.c_str(); }

static int prochainId = 1;
static bool tousLesControleurs;                        // LANCER_CONTROLEURS : les passages de tous les contrôleurs
static CLuaApi api;                                   // la frontière moteur / scripts (LANCER_API, sinon docs/api-lua-tables.tsv)
static std::map<std::string, int> inconnues;          // globales lues avant d'être posées : nil, comme dans le jeu
static std::map<std::string, int> appels;
static std::vector<std::string> derniers;   // les derniers appels, affichés en cas d'erreur

static uint8 *
Lire(const CDirectoryEntry *d, uint32 *octets)
{
	*octets = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(*octets);
	int32 fd = CFileMgr::OpenFile(kImg, "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, *octets);
	CFileMgr::CloseFile(fd);
	if(!ok){ free(buf); return nil; }
	return buf;
}

static const char *
NomThreadCourant()
{
	if(scriptCourant < 0) return "(premier niveau)";
	const Script &sc = scripts[scriptCourant];
	if(sc.courant < 0 || sc.courant >= (int)sc.threads.size()) return "(premier niveau)";
	return sc.threads[sc.courant].fonction.c_str();
}

static void
Journal(const char *quoi, lua_State *L, int premierArg)
{
	appels[quoi]++;
	std::string args;
	int n = lua_gettop(L);
	for(int i = premierArg; i <= n && i < premierArg + 4; i++){
		if(!args.empty()) args += ", ";
		switch(lua_type(L, i)){
		case LUA_TSTRING: args += "\""; args += lua_tostring(L, i); args += "\""; break;
		case LUA_TNUMBER: { char b[32]; snprintf(b, sizeof(b), "%g", lua_tonumber(L, i)); args += b; break; }
		case LUA_TBOOLEAN: args += lua_toboolean(L, i) ? "true" : "false"; break;
		case LUA_TNIL: args += "nil"; break;
		default: args += lua_typename(L, lua_type(L, i)); break;
		}
	}
	char ligne[512];
	snprintf(ligne, sizeof(ligne), "%6.2f s  %-22s %s(%s)", maintenant / 1000.0, NomThreadCourant(), quoi, args.c_str());
	derniers.push_back(ligne);
	if(derniers.size() > 4) derniers.erase(derniers.begin());
	if(journalRestant <= 0) return;
	journalRestant--;
	printf("%s\n", ligne);
}

static void
AfficherDerniers()
{
	for(const std::string &d : derniers) printf("        juste avant : %s\n", d.c_str());
}

// --- Bouchons : un nom du moteur absent est une table qu'on peut appeler et indexer. ---
static void PousserBouchon(lua_State *L, const char *nom);

// Réponse neutre d'une nouvelle partie, d'après le nom (provisoire, en attendant le vrai moteur) :
// une question (Is…, Has…, Should…, …IsReady) répond false, un compte ou une lecture (Get…, …Count) 0.
static int
ReponseNeutre(lua_State *L, const char *nom)
{
	const char *point = strrchr(nom, '.');
	const char *n = point ? point + 1 : nom;
	// Le joueur a choisi « Histoire » au menu principal.
	if(strcmp(n, "HasStoryModeBeenSelected") == 0){ lua_pushboolean(L, 1); return 1; }
	static const char *questions[] = { "Is", "Has", "Should", "Can", "Are", "Was", "Did", "Does" };
	for(const char *q : questions)
		if(strncmp(n, q, strlen(q)) == 0 && isupper((unsigned char)n[strlen(q)])){ lua_pushboolean(L, 0); return 1; }
	if(strstr(n, "Is") && isupper((unsigned char)strstr(n, "Is")[2])){ lua_pushboolean(L, 0); return 1; }
	if(strstr(n, "Get") || strstr(n, "Count")){ lua_pushnumber(L, 0); return 1; }
	return 0;
}

static int
AppelBouchon(lua_State *L)
{
	const char *nom = lua_tostring(L, lua_upvalueindex(1));
	Journal(nom, L, 2);
	return ReponseNeutre(L, nom);
}

static int
IndexBouchon(lua_State *L)
{
	std::string nom = lua_tostring(L, lua_upvalueindex(1));
	const char *cle = lua_tostring(L, 2);
	nom += ".";
	nom += cle ? cle : "?";
	PousserBouchon(L, nom.c_str());
	return 1;
}

static void
PousserBouchon(lua_State *L, const char *nom)
{
	lua_newtable(L);
	lua_newtable(L);
	lua_pushstring(L, "__call");
	lua_pushstring(L, nom);
	lua_pushcclosure(L, AppelBouchon, 1);
	lua_settable(L, -3);
	lua_pushstring(L, "__index");
	lua_pushstring(L, nom);
	lua_pushcclosure(L, IndexBouchon, 1);
	lua_settable(L, -3);
	lua_setmetatable(L, -2);
}

// Une globale absente : une fonction du moteur devient un bouchon ; tout le reste est nil, comme
// dans le jeu, où seules les tables de CreateLuaState et util.lua précèdent les scripts.
static int
IndexGlobal(lua_State *L)
{
	const char *nom = lua_tostring(L, 2);
	if(nom == nil){ lua_pushnil(L); return 1; }
	if(api.Nombre() == 0 || api.EstFonction(nom)){ PousserBouchon(L, nom); return 1; }
	inconnues[nom]++;
	lua_pushnil(L);
	return 1;
}

// docs/api-lua-tables.tsv : LANCER_API, sinon depuis la racine du dépôt, sinon à côté de cette source.
static void
ChargerApi(void)
{
	const char *env = getenv("LANCER_API");
	if(env && api.Charger(env)) return;
	if(api.Charger("docs/api-lua-tables.tsv")) return;
	std::string ici = __FILE__;
	size_t barre = ici.find_last_of("/\\");
	if(barre != std::string::npos && api.Charger((ici.substr(0, barre) + "/../docs/api-lua-tables.tsv").c_str())) return;
	printf("docs/api-lua-tables.tsv introuvable (LANCER_API ?) : toute globale inconnue sera un bouchon\n");
}

// --- Les fonctions du moteur recréées. ---

// ImportScript (0x5be5f0) : compile LibTable.lur et renvoie la fonction, sans l'exécuter.
static int
ImportScript(lua_State *L)
{
	std::string chemin = luaL_checkstring(L, 1);
	size_t barre = chemin.find_last_of("/\\");
	std::string nom = barre == std::string::npos ? chemin : chemin.substr(barre + 1);
	size_t point = nom.rfind('.');
	if(point != std::string::npos) nom.resize(point);
	nom += ".lur";
	const CDirectoryEntry *d = CdStream::ms_images[image].Find(nom.c_str());
	if(d == nil) return luaL_error(L, "ImportScript : %s absent de Scripts.img", nom.c_str());
	uint32 octets;
	uint8 *buf = Lire(d, &octets);
	if(buf == nil) return luaL_error(L, "ImportScript : lecture de %s impossible", nom.c_str());
	int err = luaL_loadbuffer(L, (const char*)buf, octets, nom.c_str());
	free(buf);
	if(err != 0) return lua_error(L);
	return 1;
}

// Wait(ms) (0x5bfa70) : heure de réveil du thread courant, puis lua_yield.
static int
Wait(lua_State *L)
{
	int ms = (int)luaL_optnumber(L, 1, 0);
	if(scriptCourant >= 0 && scripts[scriptCourant].courant >= 0){
		Thread &t = scripts[scriptCourant].threads[scripts[scriptCourant].courant];
		if(t.reveil != -1) t.reveil = maintenant + ms;
	}
	return lua_yield(L, 0);
}

// Le moteur « prêt » : gamemain attend SystemIsReady() avant de continuer.
static int
SystemIsReady(lua_State *L)
{
	appels["SystemIsReady"]++;
	lua_pushboolean(L, 1);
	return 1;
}

// ClockGet() → heure, minute ; ClockSet(h, m) ; ClockSetTickRate(secondes de jeu par seconde réelle, …).
static int
ClockGet(lua_State *L)
{
	appels["ClockGet"]++;
	lua_pushnumber(L, horloge.Heure());
	lua_pushnumber(L, horloge.Minute());
	return 2;
}

static int
ClockSet(lua_State *L)
{
	Journal("ClockSet", L, 1);
	horloge.Regler((int32)luaL_optnumber(L, 1, 8), (int32)luaL_optnumber(L, 2, 0));
	return 0;
}

static int
ClockSetTickRate(lua_State *L)
{
	Journal("ClockSetTickRate", L, 1);
	horloge.minutesParSeconde = (float)luaL_optnumber(L, 1, 60) / 60.0f;
	return 0;
}

static int
GetTimer(lua_State *L)
{
	lua_pushnumber(L, (lua_Number)maintenant);
	return 1;
}

static int
SurErreur(lua_State *L)
{
	printf("%6.2f s  %-22s ERREUR %s\n", maintenant / 1000.0, NomThreadCourant(),
	       lua_tostring(L, 1) ? lua_tostring(L, 1) : "?");
	AfficherDerniers();
	return 1;
}

// Appel protégé d'une fonction de util.lua qui renvoie une coroutine (0x5d8d40, 0x5d8e40).
static lua_State *
AppelUtil(lua_State *L, const char *fonctionUtil, const char *fichier, const char *fonction, int *ref)
{
	lua_pushcfunction(L, SurErreur);
	int errfunc = lua_gettop(L);
	lua_pushstring(L, fonctionUtil);
	lua_rawget(L, LUA_GLOBALSINDEX);
	lua_pushstring(L, fichier);
	int nargs = 1;
	if(fonction){ lua_pushstring(L, fonction); nargs = 2; }
	if(lua_pcall(L, nargs, 1, errfunc) != 0){
		lua_settop(L, errfunc - 1);
		return nil;
	}
	lua_State *co = lua_type(L, -1) == LUA_TTHREAD ? lua_tothread(L, -1) : nil;
	if(co) *ref = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_settop(L, errfunc - 1);
	return co;
}

// ThreadNameSpace (0x5d8e40) : un thread de plus dans l'objet script n° s.
static int
NouveauThread(lua_State *L, int s, const char *fonction)
{
	Thread t;
	t.co = AppelUtil(L, "ThreadNameSpace", scripts[s].fichier.c_str(), fonction, &t.ref);
	if(t.co == nil) return 0;
	t.fonction = fonction;
	t.id = prochainId++;
	scripts[s].threads.push_back(t);
	return t.id;
}

// FUN_005dbf90(fichier, 0) : l'objet script ; CreateNameSpace exécute son premier niveau.
static int
NouveauScript(lua_State *L, const char *fichier)
{
	Script sc;
	sc.fichier = fichier;
	Thread premier;
	premier.co = AppelUtil(L, "CreateNameSpace", fichier, nil, &premier.ref);
	premier.fonction = "(premier niveau)";
	if(premier.co == nil) return -1;
	sc.threads.push_back(premier);
	scripts.push_back(sc);
	return (int)scripts.size() - 1;
}

// FUN_005d8db0 : un thread de cette fonction existe dans le script et n'est pas fini (état != 4).
static bool
ThreadEnCours(int s, const char *fonction)
{
	if(s < 0) return false;
	for(size_t i = 1; i < scripts[s].threads.size(); i++)
		if(scripts[s].threads[i].fonction == fonction && scripts[s].threads[i].etat < TUE) return true;
	return false;
}

// CreateThread("F_Nom") (0x5bf2f0) : un thread de plus dans le script courant ; renvoie son numéro.
static int
CreateThread(lua_State *L)
{
	const char *f = luaL_checkstring(L, 1);
	Journal("CreateThread", L, 1);
	int id = scriptCourant >= 0 ? NouveauThread(L, scriptCourant, f) : 0;
	lua_pushnumber(L, id);
	return 1;
}

static int
TerminateThread(lua_State *L)
{
	Journal("TerminateThread", L, 1);
	int id = (int)luaL_optnumber(L, 1, 0);
	for(Script &sc : scripts)
		for(Thread &t : sc.threads)
			if(t.id == id && t.etat < ARRET_DEMANDE) t.etat = ARRET_DEMANDE;
	return 0;
}

// LuaScript_Update (0x5da960), réduit à ce qui compte sans le reste du moteur.
static void
MettreAJourScript(lua_State *L, int s)
{
	for(size_t i = 1; i < scripts[s].threads.size(); i++){
		Thread &t = scripts[s].threads[i];
		if(t.etat >= TUE) continue;
		if(t.reveil < 0 && t.etat < ARRET_DEMANDE) t.etat = ARRET_DEMANDE;
		if(!((uint32)t.reveil < maintenant || t.etat == ARRET_DEMANDE)) continue;
		if(t.etat == NOUVEAU) t.etat = EN_COURS;
		lua_pushstring(L, "Alive");
		lua_pushboolean(L, t.etat != ARRET_DEMANDE);
		lua_rawset(L, LUA_GLOBALSINDEX);
		scriptCourant = s;
		scripts[s].courant = (int)i;
		lua_State *co = t.co;
		int r = lua_resume(co, 0);
		scripts[s].courant = -1;
		scriptCourant = -1;
		Thread &u = scripts[s].threads[i];      // CreateThread a pu agrandir le tableau pendant la reprise
		lua_Debug ar;
		if(r != 0){
			printf("%6.2f s  %-22s ERREUR %s\n", maintenant / 1000.0, u.fonction.c_str(),
			       lua_tostring(u.co, -1) ? lua_tostring(u.co, -1) : "?");
			AfficherDerniers();
			u.etat = FINI;
		}else if(lua_getstack(u.co, 0, &ar) == 0 && lua_gettop(u.co) == 0){
			printf("%6.2f s  %-22s fini\n", maintenant / 1000.0, u.fonction.c_str());
			u.etat = FINI;
		}else if(u.etat == ARRET_DEMANDE && u.fonction != "MissionCleanup"){
			u.etat = TUE;
		}
		if(u.etat >= TUE){ luaL_unref(L, LUA_REGISTRYINDEX, u.ref); u.ref = LUA_NOREF; }
	}
}

// La machine à états de la mission en cours (FUN_006ad3e0, cas 2, 9 et 10).
static void
AvancerMission(lua_State *L)
{
	switch(mission.etat){
	case PREPARATION:
		// FUN_006abf20 : MissionSetup fini, le thread main démarre.
		if(!ThreadEnCours(mission.script, "MissionSetup")){
			printf("%6.2f s  mission %s : MissionSetup fini, main démarre\n", maintenant / 1000.0, mission.nom.c_str());
			NouveauThread(L, mission.script, "main");
			mission.etat = DEROULEMENT;
			// FUN_006abf20, après le thread main : FUN_006ab510(« Run ») joue le nœud Run de la mission (src/core/MissionBank) ;
			// ses pistes Script se lancent à cette première image (FUN_005f49a0).
			for(const CMissionData::Appel &a : banque.missions[mission.indice].run){
				printf("%6.2f s  mission %s : le nœud Run appelle %s dans %s (piste Script)\n", maintenant / 1000.0, mission.nom.c_str(),
				       a.fonction.c_str(), a.espace.c_str());
				AppelScript(a.espace, a.fonction, -1, a.nombre, a.avecNombre, a.avecDeclencheur ? -1 : -2);
			}
		}
		break;
	case DEROULEMENT:
		// FUN_006abed0 : main fini (ou arrêté par MissionSucceed / MissionFail), MissionCleanup.
		if(!ThreadEnCours(mission.script, "main")){
			printf("%6.2f s  mission %s : main fini, MissionCleanup\n", maintenant / 1000.0, mission.nom.c_str());
			scripts[mission.script].fin = true;
			NouveauThread(L, mission.script, "MissionCleanup");
			mission.etat = NETTOYAGE;
		}
		break;
	case NETTOYAGE:
		if(!ThreadEnCours(mission.script, "MissionCleanup")){
			printf("%6.2f s  mission %s terminée (%s)\n", maintenant / 1000.0, mission.nom.c_str(),
			       mission.reussie ? "réussie" : "échouée ou arrêtée");
			// FUN_006ac910 : la réussite incrémente le mot +2 de l'entrée de la mission.
			if(mission.reussie && mission.indice >= 0){
				if(gestionnaire.etat.reussites.size() != banque.missions.size()) gestionnaire.etat.reussites.assign(banque.missions.size(), 0);
				gestionnaire.etat.reussites[mission.indice]++;
			}
			{
				std::vector<int> partis;
				for(int i = 1; i < SCRIPT_PED_POOL_SIZE; i++) if(const CScriptPed *p = pietons.Acceder(i)) if(p->script == mission.script) partis.push_back(i);
				for(int i = 0; i < CRamassages::kTaille; i++)
					if(ramassages.entrees[i].Pris() && ramassages.entrees[i].script == mission.script) ramassages.Retirer(i);   // FUN_005d9760
				for(auto it = marqueursDuScript.begin(); it != marqueursDuScript.end(); )
					if(it->second == mission.script){ marqueurs.Retirer(it->first); it = marqueursDuScript.erase(it); } else ++it;
				int n = pietons.SupprimerDuScript(mission.script);
				for(int h : partis) OublierControleurs(h);
				if(n) printf("        %d piéton(s) laissé(s) par la mission, libéré(s)\n", n);
			}
			for(Thread &t : scripts[mission.script].threads)
				if(t.etat < TUE){ t.etat = TUE; luaL_unref(L, LUA_REGISTRYINDEX, t.ref); t.ref = LUA_NOREF; }
			// FUN_005b2680 (depuis FUN_005d9d30, la fin du thread 0 de l'objet script) → FUN_005b5ae0 : les gestionnaires des
			// déclencheurs de la mission sont lâchés. Supposé : la fin de l'objet script de la mission, après MissionCleanup.
			for(int r : declencheursEv.OublierScript(mission.script)) luaL_unref(L, LUA_REGISTRYINDEX, r);
			mission.etat = AUCUNE;
			mission.script = -1;
		}
		break;
	}
}

// LuaScript_Update (0x5da960), réduit à ce qui compte sans le reste du moteur.
static void
MettreAJour(lua_State *L)
{
	for(size_t s = 0; s < scripts.size(); s++) MettreAJourScript(L, (int)s);
	AvancerMission(L);
}

// RequestModel(id) (0x5c61e0), WeaponRequestModel (0x5c6340), VehicleRequestModel (0x5d5c60) :
// demandent le modèle et renvoient vrai quand il est chargé (état de diffusion 1). PedRequestModel
// (0x5c6320) ne renvoie rien et reste en bouchon. Les scripts bouclent dessus (« while not RequestModel(id) do Wait(0) end »).
// Sans diffusion dans cet outil, un modèle est chargé dès qu'on le demande : provisoire.
static int
RequestModel(lua_State *L)
{
	appels["RequestModel"]++;
	lua_pushboolean(L, 1);
	return 1;
}

// --- Les données de mission : DATLoad et les tables POINTLIST, PATH, TRIGGER ---

// DATLoad(fichier, emplacement) (0x5bf390) : « 1_01.DAT » de DAT\Trigger.img dans les pools, sous un
// emplacement (le monde à 1, portes et tags à 5, la mission à 2 : lu dans les appels de main.lua et
// de 1_01.lua, le code n'est pas dans l'export). DATInit() suit chaque chargement ; DATUnload("2")
// libère l'emplacement, à la fin de la mission.
static int
DATLoad(lua_State *L)
{
	const char *fichier = luaL_checkstring(L, 1);
	int emplacement = (int)luaL_optnumber(L, 2, 1);
	Journal("DATLoad", L, 1);
	int avant = donnees.NombreTrajets();
	if(!donnees.Charger(fichier, emplacement)) printf("        DATLoad : %s introuvable dans DAT\\Trigger.img\n", fichier);
	// FUN_006d3b20 → FUN_005ecf00 : chaque trajet lu passe à FUN_004720e0 avec l'objet script qui charge (FUN_005db800) ;
	// ceux dont PATROLTYPE n'est pas -1 deviennent des rondes, arrêtées (src/peds/Patrouilles). Les cases neuves sont en fin
	// de pool, ou des cases libérées plus tôt : toutes sont revues, Ajouter écarte un haché déjà tenu.
	for(int h = 1; h < donnees.NombreTrajets(); h++){
		if(h < avant && donnees.EmplacementTrajet(h) != emplacement) continue;
		const CTriggerPath *t = donnees.TrajetParHandle(h);
		if(t && patrouilles.Ajouter(h, donnees.HachageTrajet(h), t->patrouille, scriptCourant))
			printf("%6.2f s  rondes : %s (PATROLTYPE %d) devient une ronde, arrêtée\n", maintenant / 1000.0, t->nom.c_str(), t->patrouille);
	}
	return 0;
}

// DATUnload(emplacement) : FUN_005ed010 retire d'abord la ronde de chaque trajet de l'emplacement (FUN_00472750), puis le
// trajet est libéré.
static int
DATUnload(lua_State *L)
{
	Journal("DATUnload", L, 1);
	int emplacement = (int)luaL_checknumber(L, 1);
	for(int h = 1; h < donnees.NombreTrajets(); h++)
		if(donnees.EmplacementTrajet(h) == emplacement) patrouilles.Retirer(donnees.HachageTrajet(h));
	// Un déclencheur libéré emporte son objet animé (supposé : la libération du déclencheur, FUN_006d6920 en sens inverse, n'est
	// pas lue).
	for(int h = 0; h < donnees.NombreZones(); h++)
		if(donnees.EmplacementZone(h) == emplacement) objetsAnimes.RetirerDuDeclencheur(h);
	donnees.Decharger(emplacement);
	return 0;
}

// AreaDisablePatrolPath(trajet) (0x5b6050) : le trajet du pool (FUN_005ed240) ; s'il existe, sa ronde s'arrête (FUN_00472250
// sur DAT_00c2b034). Rend vrai si le trajet existe, qu'il ait une ronde ou non : 1_05 boucle dessus (F_SetupWorld,
// « while not AreaDisablePatrolPath(PATH._SGD_PREFECT2) do Wait(0) end ») et le bouchon, qui ne rendait rien, l'y gardait.
static int
AreaDisablePatrolPath(lua_State *L)
{
	Journal("AreaDisablePatrolPath", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	const CTriggerPath *t = h != -1 ? donnees.TrajetParHandle(h) : nil;
	if(t) patrouilles.Arreter(donnees.HachageTrajet(h));
	lua_pushboolean(L, t != nil);
	return 1;
}

// AreaEnablePatrolPath(trajet) (0x5b60a0) : FUN_00471f80, la ronde repart ; rien n'est rendu.
static int
AreaEnablePatrolPath(lua_State *L)
{
	Journal("AreaEnablePatrolPath", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	if(donnees.TrajetParHandle(h)) patrouilles.Reprendre(donnees.HachageTrajet(h));
	return 0;
}

// AreaInitPatrolPath(trajet, a, b, c[, discret = vrai]) (0x5b6150 → FUN_00471fd0) : la ronde reçoit ses réglages et repart.
static int
AreaInitPatrolPath(lua_State *L)
{
	Journal("AreaInitPatrolPath", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	if(donnees.TrajetParHandle(h) == nil) return 0;
	patrouilles.Initialiser(donnees.HachageTrajet(h), (int32)luaL_optnumber(L, 2, 0), (int32)luaL_optnumber(L, 3, 0),
	                        (int32)luaL_optnumber(L, 4, 0), lua_isnone(L, 5) ? true : lua_toboolean(L, 5) != 0);
	return 0;
}

// AreaEnableAllPatrolPaths() (0x5b61e0 → FUN_004721a0), AreaDisableAllPatrolPaths() (0x5b61d0 → FUN_00472710).
static int
AreaEnableAllPatrolPaths(lua_State *L)
{
	Journal("AreaEnableAllPatrolPaths", L, 1);
	patrouilles.ReprendreToutes();
	return 0;
}

static int
AreaDisableAllPatrolPaths(lua_State *L)
{
	Journal("AreaDisableAllPatrolPaths", L, 1);
	patrouilles.ArreterToutes();
	return 0;
}

// AreaClearPatrolPaths() (0x5b60d0) : l'objet script en cours (FUN_005db7e0 sur 0xd02850) retire ses rondes (FUN_00472880) ;
// hors d'un script, toutes (FUN_004729a0).
static int
AreaClearPatrolPaths(lua_State *L)
{
	Journal("AreaClearPatrolPaths", L, 1);
	if(scriptCourant >= 0) patrouilles.RetirerDe(scriptCourant);
	else patrouilles.Vider();
	return 0;
}

// DATInit (0x5bf410 → FUN_006d2200) : FUN_00416000 refait la table des portes du gestionnaire de zones
// (chaque liste SV_ ou DT_ avec le déclencheur de son nom), puis FUN_004154e0 crée les objets à ramasser
// des listes dont l'AREACODE est la zone visible (pas joué ici : pas d'objets).
static int
DATInit(lua_State *L)
{
	Journal("DATInit", L, 1);
	size_t avant = zones.portes.size();
	zones.ReconstruirePortes(donnees);
	if(zones.portes.size() != avant && journalRestant > 0){
		int sv = 0; for(const CAreaPorte &p : zones.portes) sv += !p.parObjet;
		journalRestant--; printf("%6.2f s  zones : %zu portes dans la table (%d SV_ en entrant, %zu DT_ par leur objet)\n", maintenant / 1000.0, zones.portes.size(), sv, zones.portes.size() - sv);
	}
	return 0;
}

// POINTLIST.nom, PATH.nom, TRIGGER.nom (0x5db130, 0x5db220, 0x5db0f0) : le handle, ou -1 s'il n'est
// pas chargé. Le nom arrive avec son « _ » de tête, que la recherche ignore.
static int
IndexPointList(lua_State *L){ const char *n = luaL_checkstring(L, 2); appels["POINTLIST"]++; lua_pushnumber(L, donnees.ListePoints(n)); return 1; }
static int
IndexPath(lua_State *L){ const char *n = luaL_checkstring(L, 2); appels["PATH"]++; lua_pushnumber(L, donnees.Trajet(n)); return 1; }
static int
IndexTrigger(lua_State *L){ const char *n = luaL_checkstring(L, 2); appels["TRIGGER"]++; lua_pushnumber(L, donnees.Declencheur(n)); return 1; }
// MODELENUM.x (0x5db1d0) : le nom sans son premier caractère, toujours (pas seulement « _ »), puis
// FUN_0051c1e0 : l'indice du modèle de ce nom, ou -1.
static int
IndexModelEnum(lua_State *L)
{
	const char *n = luaL_checkstring(L, 2);
	appels["MODELENUM"]++;
	int32 id = modeles.Index(n[0] ? n + 1 : n);
	if(id < 0 && journalRestant > 0){ journalRestant--; printf("%6.2f s  %-22s MODELENUM.%s : modèle inconnu\n", maintenant / 1000.0, NomThreadCourant(), n); }
	lua_pushnumber(L, id);
	return 1;
}
// POI.x (0x5db170, nom en 0x901d18) : le nom sans son premier caractère (toujours, comme MODELENUM), puis
// FUN_00448e30 : un groupe de ce nom rend son premier point, sinon le premier point de ce nom ; -1 sinon.
static int
IndexPoi(lua_State *L)
{
	const char *n = luaL_checkstring(L, 2);
	appels["POI"]++;
	int h = donnees.Poi(n[0] ? n + 1 : n);
	if(h < 0 && journalRestant > 0){ journalRestant--; printf("%6.2f s  %-22s POI.%s : aucun point d'intérêt de ce nom chargé\n", maintenant / 1000.0, NomThreadCourant(), n); }
	lua_pushnumber(L, h);
	return 1;
}

static void
PoserEnumeration(lua_State *L, const char *nom, lua_CFunction index)
{
	lua_newtable(L);
	lua_newtable(L);
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, index);
	lua_settable(L, -3);
	lua_setmetatable(L, -2);
	lua_setglobal(L, nom);
}

// Le n-ième point d'une liste (1 par défaut), comme FUN_005bc790 : nil et un message si la liste
// n'est pas chargée (le moteur renvoie alors six zéros).
static const CTriggerPointPoint *
PointArg(lua_State *L, const char *quoi, int argListe, int argN)
{
	int h = (int)luaL_checknumber(L, argListe);
	int n = lua_isnumber(L, argN) ? (int)lua_tonumber(L, argN) : 1;
	const CTriggerPointPoint *pt = donnees.Point(h, n);
	if(pt == nil) printf("        %s : la liste %d n'est pas chargée\n", quoi, h);
	return pt;
}

// GetPointFromPointList(liste, n) : x, y, z, lacet, 0, 0.
static int
GetPointFromPointList(lua_State *L)
{
	Journal("GetPointFromPointList", L, 1);
	const CTriggerPointPoint *pt = PointArg(L, "GetPointFromPointList", 1, 2);
	for(int i = 0; i < 6; i++) lua_pushnumber(L, pt == nil ? 0 : i == 0 ? pt->pos.x : i == 1 ? pt->pos.y : i == 2 ? pt->pos.z : i == 3 ? pt->lacetTangageRoulis[0] : 0);
	return 6;
}

// GetPointList(liste) : six nombres par point de la chaîne.
static int
GetPointList(lua_State *L)
{
	Journal("GetPointList", L, 1);
	const CTriggerPointList *l = donnees.Liste((int)luaL_checknumber(L, 1));
	if(l == nil) return 0;
	for(const CTriggerPointPoint &pt : l->points){
		lua_pushnumber(L, pt.pos.x); lua_pushnumber(L, pt.pos.y); lua_pushnumber(L, pt.pos.z);
		lua_pushnumber(L, pt.lacetTangageRoulis[0]); lua_pushnumber(L, 0); lua_pushnumber(L, 0);
	}
	return 6 * (int)l->points.size();
}

// Le joueur posé à un point : position, cap, et une zone. FUN_005d2310 lit l'octet +0xc de
// l'entrée de la liste, qui est l'AREACODE du bloc POINT (FUN_006d59a0 l'y écrit), ou garde
// DAT_00bd1008 s'il vaut -1 ; c'est la zone de destination d'une téléportation, pas la zone où le
// joueur est (1_01_EndPoint dit 5, et le script y attend AreaGetVisible() == 2),
// d'où --zone pour jouer les portes.
// Toutes les fonctions Lua qui déplacent le joueur posent l'octet +0x8dc (fondu immédiat) puis appellent
// FUN_00415640 : une demande au gestionnaire de zones, qui pose le joueur à l'arrivée (états 9 et 10 de
// FUN_00416e30) ; AreaIsLoading répond vrai entre-temps, et c'est ce qu'attendent AreaTransitionXYZ et
// AreaTransitionPoint, qui sont des fonctions Lua de SGlFunc.lua (PlayerSetPos…Area, puis
// « while AreaIsLoading() do Wait(0) end », puis Wait(100)).
static void
DemanderZone(const char *quoi, const CVector &pos, float cap, int zone)
{
	if(zone < 0) zone = zones.zoneVisible;        // FUN_005d2310 : -1 garde DAT_00bd1008
	if(!zones.Demander(pos, cap, zone, nil, true)) printf("        %s : un changement de zone est déjà en cours, demande ignorée\n", quoi);
}

static void
PoserJoueur(const CTriggerPointPoint *pt, int zone)
{
	DemanderZone("PlayerSetPosPoint", pt->pos, pt->lacetTangageRoulis[0], zone);
}

// PlayerSetPosPoint(liste[, n]) (0x5d2310) : le joueur au point, tourné selon son lacet, dans la zone de
// l'AREACODE de la liste (l'octet +0xc de l'entrée), ou la zone visible si l'AREACODE vaut -1.
static int
PlayerSetPosPoint(lua_State *L)
{
	Journal("PlayerSetPosPoint", L, 1);
	const CTriggerPointPoint *pt = PointArg(L, "PlayerSetPosPoint", 1, 2);
	const CTriggerPointList *liste = donnees.Liste((int)luaL_checknumber(L, 1));
	if(pt) PoserJoueur(pt, liste ? liste->zone : -1);
	return 0;
}

// AreaGetVisible() (0x5b7300) : la zone visible, DAT_00bd1008.
static int
AreaGetVisible(lua_State *L)
{
	appels["AreaGetVisible"]++;
	lua_pushnumber(L, zones.zoneVisible);
	return 1;
}

// AreaIsLoading() (0x5b6f50 → 0x415230) : un changement de zone est demandé ou en cours.
static int
AreaIsLoading(lua_State *L)
{
	appels["AreaIsLoading"]++;
	lua_pushboolean(L, zones.EnChargement());
	return 1;
}

// AreaSignalAreaTransitionReadyToLoad() (0x5b6f40) : le main du script de zone dit qu'il a fini de préparer la
// zone (DAT_00bccdf0) ; l'état 8 de la machine attend ce signal (+0x870) avant de poser la zone et le joueur.
static int
AreaSignalAreaTransitionReadyToLoad(lua_State *L)
{
	Journal("AreaSignalAreaTransitionReadyToLoad", L, 1);
	zones.SignalerPret();
	return 0;
}

// AreaRegisterAreaScript(code, script) (0x5b7530 → 0x414f10) : le script d'une zone, dans sa case de la table.
static int
AreaRegisterAreaScript(lua_State *L)
{
	Journal("AreaRegisterAreaScript", L, 1);
	zones.EnregistrerScript((int32)luaL_checknumber(L, 1), luaL_checkstring(L, 2));
	return 0;
}

// PlayerSetPosSimple(x, y, z) (0x5d2bd0, lue à l'assembleur de 0x5d2bd0 à 0x5d2c5a) : les trois flottants (FUN_005d82a0) ; le joueur
// (DAT_00c1aea8) sort du monde (la méthode +0xc de sa vftable), reçoit la position (FUN_0041c390, sans toucher au cap), ses objets de
// rendu suivent (FUN_00443950, FUN_00465610 : le cadre de son modèle) et il rentre dans le monde (la méthode +8). Ni la zone ni le
// gestionnaire de zones ne changent, contrairement à PlayerSetPosXYZ. Ne rend rien.
static int
PlayerSetPosSimple(lua_State *L)
{
	Journal("PlayerSetPosSimple", L, 1);
	CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	j->pos = CVector((float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0));
	if(journalRestant > 0) printf("%6.2f s  le joueur est posé en (%.2f, %.2f, %.2f) (PlayerSetPosSimple)\n", maintenant / 1000.0, j->pos.x, j->pos.y, j->pos.z);
	return 0;
}

// PlayerSetPosXYZ(x, y, z) (0x5d1280 → 0x415fb0) : même chemin, avec le cap du joueur et la zone visible.
static int
PlayerSetPosXYZ(lua_State *L)
{
	Journal("PlayerSetPosXYZ", L, 1);
	const CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	DemanderZone("PlayerSetPosXYZ", CVector((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)), j->cap, zones.zoneVisible);
	return 0;
}

// CameraFade(ms, sens) (0x5b8ea0 → CameraFade 0x4f1980) : sens 0, l'écran va au noir ; 1, il revient ; une
// durée négative prend celle du moteur. Ici le fondu n'est qu'une durée qui court : la machine des zones
// attend sa fin (FUN_004f11a0), et les scripts font Wait(ms) de leur côté.
static void
LancerFondu(float ms, bool versNoir)
{
	fondu.enCours = true; fondu.versNoir = versNoir; fondu.fin = maintenant + (uint32)ms;
}

static int
CameraFade(lua_State *L)
{
	Journal("CameraFade", L, 1);
	float ms = (float)luaL_optnumber(L, 1, -1);
	int sens = (int)luaL_optnumber(L, 2, 0);
	if(sens != 0 && sens != 1) return 0;
	if(ms < 0) ms = sens == 0 ? fonduNoirMs : fonduRetourMs;
	LancerFondu(ms, sens == 0);
	return 0;
}

static int
PlayerGetPosXYZ(lua_State *L)
{
	appels["PlayerGetPosXYZ"]++;
	const CVector &p = pietons.Acceder(SCRIPT_PED_JOUEUR)->pos;
	lua_pushnumber(L, p.x); lua_pushnumber(L, p.y); lua_pushnumber(L, p.z);
	return 3;
}

// PlayerSetPosPointArea(zone, liste[, n]) (0x5d2410) : comme PlayerSetPosPoint, avec la zone donnée par le script.
static int
PlayerSetPosPointArea(lua_State *L)
{
	Journal("PlayerSetPosPointArea", L, 1);
	const CTriggerPointPoint *pt = PointArg(L, "PlayerSetPosPointArea", 2, 3);
	if(pt) DemanderZone("PlayerSetPosPointArea", pt->pos, pt->lacetTangageRoulis[0], (int)luaL_checknumber(L, 1));
	return 0;
}

// PlayerIsInTrigger(déclencheur) (0x5d2a30) et PedIsInTrigger(ped, déclencheur) (0x5c9930) : la
// position du piéton est dans la zone du déclencheur (FUN_006d6e50).
static int
PlayerIsInTrigger(lua_State *L)
{
	appels["PlayerIsInTrigger"]++;
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	lua_pushboolean(L, h >= 0 && donnees.DansDeclencheur(h, pietons.Acceder(SCRIPT_PED_JOUEUR)->pos));
	return 1;
}

static int
PedIsInTrigger(lua_State *L)
{
	appels["PedIsInTrigger"]++;
	const CScriptPed *p = lua_isnumber(L, 1) ? pietons.Acceder((int)lua_tonumber(L, 1)) : nil;
	int h = lua_isnumber(L, 2) ? (int)lua_tonumber(L, 2) : -1;
	lua_pushboolean(L, p != nil && h >= 0 && donnees.DansDeclencheur(h, p->pos));
	return 1;
}

// PlayerIsInAreaXYZ(x, y, z, rayon[, marqueur, angle]) (0x5d13f0) et PedIsInAreaXYZ(ped, x, y, z, rayon[, marqueur,
// angle]) (0x5c84a0) : FUN_005c2910(1, entité, …) teste le pavé fermé de demi-côté « rayon » sur les trois axes
// (FUN_0046d960, PaveContient) ; un marqueur non nul fait aussi dessiner un marqueur au point (FUN_0052a4b0, pas
// ici). PedIsInAreaXYZ répond faux pour un piéton qui n'existe pas.
static int
PaveArg(lua_State *L, const CScriptPed *p, int premier)
{
	if(p == nil){ lua_pushboolean(L, 0); return 1; }
	CVector c((float)luaL_checknumber(L, premier), (float)luaL_checknumber(L, premier + 1), (float)luaL_checknumber(L, premier + 2));
	float r = (float)luaL_optnumber(L, premier + 3, 1);
	lua_pushboolean(L, PaveContient(p->pos, c, r, r, r, true));
	return 1;
}

static int
PlayerIsInAreaXYZ(lua_State *L)
{
	appels["PlayerIsInAreaXYZ"]++;
	return PaveArg(L, pietons.Acceder(SCRIPT_PED_JOUEUR), 1);
}

static int
PedIsInAreaXYZ(lua_State *L)
{
	appels["PedIsInAreaXYZ"]++;
	return PaveArg(L, lua_isnumber(L, 1) ? pietons.Acceder((int)lua_tonumber(L, 1)) : nil, 2);
}

// PlayerIsInAreaObject(id, genre, rayon, marqueur[, …]) (0x5d14f0) : LuaGetEntity(id, genre) puis FUN_005c2810 prend
// la position de l'entité (genre 2 : un piéton de script, FUN_005c7220 ; 3 : le joueur ; 0 et 1 : un objet, un
// véhicule, pas encore dans l'outil : faux), et FUN_005c2910(1, …) teste le joueur dans le pavé fermé de demi-côté
// « rayon » autour d'elle, comme PlayerIsInAreaXYZ. Faux si l'entité n'existe pas.
static int
PlayerIsInAreaObject(lua_State *L)
{
	Journal("PlayerIsInAreaObject", L, 1);
	int id = (int)luaL_optnumber(L, 1, -1), genre = (int)luaL_optnumber(L, 2, -1);
	float r = (float)luaL_optnumber(L, 3, 1);
	const CScriptPed *e = genre == 2 ? pietons.Acceder(id) : genre == 3 ? pietons.Acceder(SCRIPT_PED_JOUEUR) : nil;
	const CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	lua_pushboolean(L, e != nil && j != nil && PaveContient(j->pos, e->pos, r, r, r, true));
	return 1;
}

// PedLockTarget(ped, cible, mode) (0x5cb060) : le verrou de cible du piéton (CScriptPedPool::Verrouiller).
static int
PedLockTarget(lua_State *L)
{
	Journal("PedLockTarget", L, 1);
	// Le troisième argument : un nombre, ou un booléen que FUN_004969a0 convertit (FUN_005d81c0 choisit).
	int mode = lua_isnumber(L, 3) ? (int)lua_tonumber(L, 3) : lua_toboolean(L, 3);
	pietons.Verrouiller((int)luaL_optnumber(L, 1, -1), (int)luaL_optnumber(L, 2, -1), mode);
	return 0;
}

// --aller NOM[,n]@s ou --aller x,y,z@s : à s secondes, le joueur est posé au n-ième point de la
// liste NOM (1 par défaut), ou aux coordonnées, comme s'il y avait marché.
static void
AllerAuPoint(const char *cible)
{
	CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	float x, y, z;
	if(sscanf(cible, "%f,%f,%f", &x, &y, &z) == 3){
		j->pos = CVector(x, y, z);
		printf("%6.2f s  le joueur va en (%.1f, %.1f, %.1f)\n", maintenant / 1000.0, x, y, z);
		return;
	}
	std::string nom = cible; int n = 1;
	size_t virgule = nom.find(',');
	if(virgule != std::string::npos){ n = atoi(nom.c_str() + virgule + 1); nom.erase(virgule); }
	int h = donnees.ListePoints(nom.c_str());
	const CTriggerPointPoint *pt = donnees.Point(h, n);
	if(pt == nil){ printf("%6.2f s  --aller : la liste %s n'est pas chargée\n", maintenant / 1000.0, nom.c_str()); return; }
	// La zone ne change pas : c'est une porte qui la change (--zone), pas une marche.
	j->pos = pt->pos; j->cap = pt->lacetTangageRoulis[0];
	printf("%6.2f s  le joueur va au point %d de %s (%d) en (%.1f, %.1f, %.1f), toujours en zone %d\n", maintenant / 1000.0, n, nom.c_str(), h, pt->pos.x, pt->pos.y, pt->pos.z, zones.zoneVisible);
}

// --suivre PIETON[,d]@s : à partir de s secondes, la manette du joueur le mène au sprint vers le piéton PIETON tant qu'il en
// est à plus de d mètres (2 par défaut) ; la dernière poursuite commencée l'emporte, elle cesse quand le piéton n'existe plus.
// Comme le stick dans le jeu, elle ne fait rien quand le script a repris le contrôle (PlayerSetControl(0)) ni quand le joueur
// suit lui-même un trajet (l'objectif FollowPath de sa file passe devant).
// --courir x,y,z@s : la même manette, vers un point fixe (le joueur s'arrête à 0,3 m) ; un allié qui le suit (TetherLoco) le voit
// bouger, ce que --aller, qui le pose, ne fait pas.
struct Poursuite { uint32 debut; int ped; float distance; bool annoncee; std::vector<CVector> trace; CVector dernier; bool fixe = false; CVector point; };
static std::vector<Poursuite> poursuites;

static void
PoursuivrePieton(void)
{
	Poursuite *active = nil;
	for(Poursuite &q : poursuites) if(maintenant >= q.debut) active = &q;
	if(active == nil) return;
	CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	const CScriptPed *c = active->fixe ? nil : pietons.Acceder(active->ped);
	if(!active->fixe && (c == nil || c->mort)) return;
	const CVector cible = active->fixe ? active->point : c->pos;
	// La trace du piéton poursuivi : ses positions, une tous les 0,5 m ; le joueur court dessus, il passe où il est passé.
	auto ecart = [](const CVector &a, const CVector &b){ return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); };
	if(active->trace.empty() || ecart(active->trace.back(), cible) > 0.5f) active->trace.push_back(cible);
	if(controleJoueur != 1) return;
	const CObjectifPieton *o = pietons.ObjectifCourant(SCRIPT_PED_JOUEUR);
	if(o && o->type == OBJECTIF_SUIVRE_TRAJET) return;
	if(!active->annoncee){
		active->annoncee = true;
		active->trace.clear(); active->trace.push_back(cible);
		if(active->fixe) printf("%6.2f s  le joueur court vers (%.1f, %.1f, %.1f)\n", maintenant / 1000.0, cible.x, cible.y, cible.z);
		else printf("%6.2f s  le joueur poursuit le piéton %d (à %.1f m)\n", maintenant / 1000.0, active->ped, active->distance);
	}
	else if(ecart(j->pos, active->dernier) > 0.01f){ active->trace.clear(); active->trace.push_back(cible); }   // posé ailleurs (--entrer, un script)
	float reste = ScriptPedVitesse(ALLURE_SPRINT) * 0.033f;
	j->intentionVitesse = 0;
	while(reste > 0 && sqrtf((cible.x - j->pos.x) * (cible.x - j->pos.x) + (cible.y - j->pos.y) * (cible.y - j->pos.y)) > active->distance && !active->trace.empty()){
		CVector v(active->trace.front().x - j->pos.x, active->trace.front().y - j->pos.y, active->trace.front().z - j->pos.z);
		float d = ecart(active->trace.front(), j->pos);
		if(d <= reste){ j->pos = active->trace.front(); reste -= d; active->trace.erase(active->trace.begin()); continue; }
		j->pos = CVector(j->pos.x + v.x * reste / d, j->pos.y + v.y * reste / d, j->pos.z + v.z * reste / d);
		j->cap = atan2f(v.y, v.x) * 180.0f / 3.14159265f;
		j->intention = CVector(v.x / d, v.y / d, 0);
		j->intentionVitesse = ScriptPedIntentionVitesse(ALLURE_SPRINT, 1.5f);
		reste = 0;
	}
	active->dernier = j->pos;
}

// --entrer NOM@s : à s secondes, le joueur est posé dans le déclencheur NOM (à la position du
// déclencheur), comme s'il y avait marché. C'est l'outil qui joue le joueur, pas le moteur.
static void
EntrerDansDeclencheur(const char *nom)
{
	int h = donnees.Declencheur(nom);
	const CTriggerZone *z = donnees.ZoneParHandle(h);
	if(z == nil){ printf("%6.2f s  --entrer : le déclencheur %s n'est pas chargé\n", maintenant / 1000.0, nom); return; }
	CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	j->pos = CVector(z->pos.x, z->pos.y, z->pos.z + 0.5f);
	// La position d'un déclencheur n'est pas toujours dans sa zone (_1_05_TRIGGERJOCKSNEARLIBRARY) : le centre de son périmètre.
	CVector centre;
	bool recentre = !donnees.DansDeclencheur(h, j->pos) && donnees.CentreDeclencheur(h, centre) && donnees.DansDeclencheur(h, centre);
	if(recentre) j->pos = centre;
	printf("%6.2f s  le joueur entre dans %s (%d) en (%.1f, %.1f, %.1f) : %s\n", maintenant / 1000.0, nom, h, j->pos.x, j->pos.y, j->pos.z,
	       recentre ? "dedans, au centre de son périmètre" : donnees.DansDeclencheur(h, j->pos) ? "dedans" : "la position du déclencheur est hors de sa propre zone");
}

// --- Les piétons : le pool des scripts, PedCreatePoint & co. ---

// Le handle d'un piéton en argument : le piéton, ou nil (le moteur répond alors 0 / faux / rien).
static CScriptPed *
PedArg(lua_State *L, int arg)
{
	return lua_isnumber(L, arg) ? pietons.Acceder((int)lua_tonumber(L, arg)) : nil;
}

// pedstats.dat (docs/pedstats.md) : les entrées lues par CPedStats::ParseLine (0x499d80), chacune sous son nom.
// Le moteur les lit par CPedStats::Initialise (0x49a0a0) et FUN_00478e30 copie dans le piéton créé celle que le
// modelinfo de son modèle garde en +0x54 (la colonne STAT de son entrée peds, CModelEnum::statDuPieton).
static std::map<std::string, CPedStat> statsParNom;
// Ce que ParseLine demande pour ses colonnes textuelles : le vrai index de modèle (FUN_0051c1e0) ; les classes, armes
// et missions ne servent pas à l'outil, un haché tient leur place.
uint32 HashString(const char *s){ return ActionHash(s); }
int32 GetModelIndexByName(const char *n){ return strcmp(n, "none") == 0 ? -1 : modeles.Index(n); }
int32 GetCharacterClassId(const char *n){ return (int32)(ActionHash(n) & 0xff); }
int32 GetWeaponIdByName(const char *n){ return strcmp(n, "unarmed") == 0 ? 0 : (int32)(ActionHash(n) & 0xff); }
int32 GetMissionIdByName(const char *n){ return (int32)(ActionHash(n) & 0xffff); }
// CPedStats::Initialise n'est pas appelé (l'outil lit le fichier lui-même, ChargerPedStats) : ses appuis restent vides.
uint8 CFileLoader::ms_lineBuffer[0x15e];
char *CFileLoader::LoadLine(int32){ return nil; }
void CMemoryHeap::Push(int){}
void CMemoryHeap::Pop(void){}

static void
ChargerPedStats(void)
{
	char chemin[512];
	CheminHote("Config/Dat/PEDSTATS.DAT", chemin, sizeof(chemin));
	FILE *f = fopen(chemin, "rb");
	if(f == nil){ printf("pedstats.dat introuvable : les piétons gardent 100 points de vie\n"); return; }
	char ligne[0x15e];
	while(fgets(ligne, sizeof(ligne), f)){
		// FUN_00429ac0 (LoadLine) : fin de ligne coupée, contrôles et virgules changés en espaces, blancs de tête sautés.
		for(char *c = ligne; *c; c++){ if(*c == '\n'){ *c = 0; break; } if(*c < ' ' || *c == ',') *c = ' '; }
		char *l = ligne;
		while(*l && *l < '!') l++;
		if(*l == 0 || *l == '#') continue;
		CPedStat e;
		memset(&e, 0, sizeof(e));
		CPedStats::ParseLine(&e, l);
		statsParNom[e.m_name] = e;
	}
	fclose(f);
	printf("%zu entrées de pedstats.dat\n", statsParNom.size());
}

// L'initialisation du composant d'actions (FUN_00484d10, ecx = le composant +0x2e0 ; appelée par CPed::SetModelIndex,
// FUN_004788d0, depuis le constructeur de CPed FUN_0047ffe0) : les trois contrôleurs arrêtés (FUN_005f3f40), les entrées
// vidées (FUN_005f38e0), puis les arbres. L'arbre de l'IA (+0x4b4) : les noms gardés par PedSetAITree (+0x540, +0x580)
// s'ils sont posés, sinon la racine et le fichier du modelinfo (+0x6c, +0x70 : la colonne AITREE de l'entrée peds) ;
// l'arbre trouvé dans l'arbre global (FUN_005f59b0), sinon son fichier chargé s'il ne l'est pas déjà (FUN_00484130 :
// FUN_005dbe60, FUN_005f6c50) et l'arbre recherché, nul si le fichier l'était. L'arbre d'actions (+0x4b0) de même, avec
// les noms +0x4c0, +0x500 et le modelinfo +0x64, +0x68 (ACTIONTREE). Chacun devient la racine de son contexte
// (FUN_005f36d0) ; à chaque image, faute de nœud joué, son contrôleur y redescend (FUN_00484040 pour les actions,
// FUN_00483ec0 dans FUN_00484490 pour l'IA). Le contrôleur du visage (+0x4bc : /Global/PlayerFacial pour le type 13,
// sinon DAT_01b649bc hors de quatre modèles) n'est pas suivi.
static void
PoserArbresDuModele(int h, int32 modele)
{
	auto it = modeles.arbresDuPieton.find(modele);
	CScriptPed *p = pietons.Acceder(h);
	if(it == modeles.arbresDuPieton.end() || p == nil) return;
	const CModelEnum::Arbres &a = it->second;
	float t = maintenant / 1000.0f;
	const CActionNode *ia = arbres.TrouverOuCharger(a.ia.c_str(), a.fichierIA.c_str()) ? arbres.Trouver(a.ia.c_str()) : nil;
	if(ia){
		CActionController &c = controleursIA[h];
		c = CActionController();
		c.sousControleurs = true;
		c.arbre = ia;
		c.Racine(arbres, ia);
		p->arbreIA = true;
	}
	const CActionNode *ac = arbres.TrouverOuCharger(a.actions.c_str(), a.fichierActions.c_str()) ? arbres.Trouver(a.actions.c_str()) : nil;
	if(ac){
		CActionController &c = controleurs[h];
		c = CActionController();
		c.arbre = ac;
		c.Racine(arbres, ac);
	}
	printf("%6.2f s  piéton %d (modèle %d) : arbre d'actions %s%s, arbre de l'IA %s%s\n", t, h, modele, a.actions.c_str(),
	       ac ? "" : " (absent)", a.ia.c_str(), ia ? "" : " (absent)");
}

// FUN_00486cd0 : les jetons de combat d'un piéton, autant par catégorie qu'AIButes +0x3c, +0x40, +0x44, +0x48 (2, 1, 1, 1).
static void
PoserJetonsDe(int h)
{
	int32 n[4];
	for(int k = 0; k < 4; k++) n[k] = butesIA.Champ((uint16)(60 + 4 * k)) ? (int32)butesIA.Mot((uint16)(60 + 4 * k)) : (k == 0 ? 2 : 1);
	pietons.PoserJetons(h, n);
}

// FUN_005cf6b0 : la création commune, notée à l'objet script courant ; le piéton reçoit l'entrée de pedstats.dat
// de son modèle (FUN_00478e30) et les arbres de son modèle (FUN_00484d10).
static int
CreerPed(lua_State *L, const char *quoi, int32 modele, const CVector &pos, float cap)
{
	int h = pietons.Creer(modele, pos, cap, scriptCourant);
	if(h < 0) printf("        %s : le pool des piétons est plein (%d)\n", quoi, SCRIPT_PED_POOL_SIZE);
	else{
		PoserArbresDuModele(h, modele);
		PoserJetonsDe(h);
		auto ty = modeles.typeDuPieton.find(modele);   // FUN_0047ffe0 : le type du modelinfo (+0x4c) en +0x1310
		if(ty != modeles.typeDuPieton.end() && ty->second >= 0) pietons.Acceder(h)->type = ty->second;
		auto st = modeles.statDuPieton.find(modele);
		auto e = st == modeles.statDuPieton.end() ? statsParNom.end() : statsParNom.find(st->second);
		if(e != statsParNom.end()){
			pietons.PoserStats(h, e->second.m_fields);
			const CScriptPed *p = pietons.Acceder(h);
			printf("%6.2f s  piéton %d (modèle %d) : %s, %.0f points de vie, régénération %.0f, seuil d'humiliation %d\n",
			       maintenant / 1000.0, h, modele, e->first.c_str(), p->santeMax, p->regeneration, p->stats[63]);
		}
	}
	lua_pushnumber(L, h);
	return 1;
}

// --- Les armes : leurs types et l'inventaire (src/peds/Armes) ---

// Une fiche ObjectButes lue en CTypeArme : +0x18 (24) le genre, +0x1c (28) l'arbre, +0x25 (37) le ramassage au contact, +0x34 (52)
// le maximum, +0x44 (68) le décalage en hauteur des ramassages, +0x54 (84) le nom des munitions, +0x64 (100), les octets +0x68 (104)
// et +0x69 (105).
static CTypeArme
LireTypeArme(const CActionTrack &t, const std::map<uint16, std::string> &chaines)
{
	CTypeArme a;
	a.lu = true;
	a.genre = (int32)t.Mot(24);
	a.maximum = (int32)t.Mot(52);
	a.inventaire = (int32)t.Mot(100);
	const CActionAttribut *b = t.Champ(104), *c = t.Champ(105);
	a.tir68 = b && b->valeur[0]; a.tir69 = c && c->valeur[0];
	const CActionAttribut *l = t.Champ(43); a.tireLachee = l && l->valeur[0];
	const CActionAttribut *ac = t.Champ(37); a.auContact = ac && ac->valeur[0];
	if(const CActionAttribut *dz = t.Champ(68)) if(dz->taille >= 4) memcpy(&a.decalageRamassage, dz->valeur, 4);
	auto it = chaines.find(28); if(it != chaines.end()) a.arbre = it->second;
	it = chaines.find(84); if(it != chaines.end()) a.nomMunition = it->second;
	it = chaines.find(80); if(it != chaines.end()) a.nomProjectile = it->second;
	return a;
}

// FUN_0051ad50 : l'indice de l'arme de ce nom parmi les modèles weap (ms_weaponModelStore, DAT_00c735e8), -1 pour « unarmed »
// ou un nom inconnu.
static int32
ArmeParNom(const std::string &nom)
{
	if(strcasecmp(nom.c_str(), "unarmed") == 0) return -1;
	uint32 h = ActionHash(nom.c_str());
	for(const auto &a : modeles.armes) if(ActionHash(a.second.c_str()) == h) return a.first;
	return -1;
}

// CWeapon::RegisterCB (FUN_004da330 → FUN_004da2d0) : pour chaque modèle weap puis item, FUN_0050eaa0 cherche l'ObjectButes de
// Weapons/Info/<nom> (FUN_005fa760(3, …)), puis Items/<nom>, puis OtherPickups/<nom> (FUN_005fa760(2, …)) depuis l'arbre des
// réglages (FUN_0072ad50) ; l'outil lit Weapons/Info dans Global_Weapons.cat et les deux autres sous GlobalsBank/Globals de
// Globals.cat. Le modèle des munitions (+0x84) est résolu par FUN_00728150 à la première demande : ici au chargement, le
// même. La fiche par défaut est l'ObjectButes de Globals (FUN_0072ae00, DAT_020c88d8). Les trois armes aux munitions sans fin
// de FUN_0045cdf0 sont « marble », « supermarble » et « RubBand » (la table des noms 0xa136e8).
static void
ChargerArmes(void)
{
	CActionTrack t;
	std::map<uint16, std::string> ch;
	if(CActionBank::ButesChemin("Globals.cat", {ActionHash("GlobalsBank"), ActionHash("Globals")}, ActionHash("ObjectButes"), t, &ch))
		armes.defaut = LireTypeArme(t, ch), armes.defaut.lu = false;
	int lues = 0;
	auto charger = [&](int32 id, const std::string &nom){
		uint32 h = ActionHash(nom.c_str());
		if(CActionBank::ButesChemin("Global_Weapons.cat", {ActionHash("Weapons"), ActionHash("Info"), h}, ActionHash("ObjectButes"), t, &ch) ||
		   CActionBank::ButesChemin("Globals.cat", {ActionHash("GlobalsBank"), ActionHash("Globals"), ActionHash("Items"), h}, ActionHash("ObjectButes"), t, &ch) ||
		   CActionBank::ButesChemin("Globals.cat", {ActionHash("GlobalsBank"), ActionHash("Globals"), ActionHash("OtherPickups"), h}, ActionHash("ObjectButes"), t, &ch)){
			armes.types[id] = LireTypeArme(t, ch);
			lues++;
		}
	};
	for(const auto &a : modeles.armes) charger(a.first, a.second);
	for(const auto &a : modeles.objets) charger(a.first, a.second);
	for(auto &a : armes.types) a.second.munition = a.second.nomMunition.empty() ? -1 : ArmeParNom(a.second.nomMunition);
	// FUN_00728150 : la fiche de projectile de chaque type qui nomme la sienne (+0x50), Weapons/Projectiles/<nom> ; DAT_020c88c8 sinon.
	if(CActionBank::ButesChemin("Globals.cat", {ActionHash("GlobalsBank"), ActionHash("Globals")}, ActionHash("ProjectileButes"), t, &ch))
		projectileDefaut = LireTypeProjectile(t, ch), projectileDefaut.lu = false;
	for(const auto &a : armes.types)
		if(!a.second.nomProjectile.empty() &&
		   CActionBank::ButesChemin("Global_Weapons.cat", {ActionHash("Weapons"), ActionHash("Projectiles"), ActionHash(a.second.nomProjectile.c_str())},
		                            ActionHash("ProjectileButes"), t, &ch))
			typesProjectiles[a.first] = LireTypeProjectile(t, ch);
	armes.sansFin[0] = modeles.Index("marble"); armes.sansFin[1] = modeles.Index("supermarble"); armes.sansFin[2] = modeles.Index("RubBand");
	// DAT_00a136b8 et les quatre modèles que FUN_00541ae0 traite à part (InitModelIndices, la table des noms 0xa136e8).
	if(!modeles.objets.empty()) objetsJoueur.premier = modeles.objets.begin()->first;
	objetsJoueur.chocolats = modeles.Index("ChocBox"); objetsJoueur.fleurs = modeles.Index("FlowerGift");
	objetsJoueur.ticket = modeles.Index("ticket"); objetsJoueur.bigGulp = modeles.Index("BigGulp");
	printf("%d fiches d'armes et d'objets (ObjectButes) pour %zu armes et %zu objets, %zu fiches de projectiles\n", lues, modeles.armes.size(),
	       modeles.objets.size(), typesProjectiles.size());
}

// FUN_00486130 (ecx = le piéton) : 0 pour une arme sous 1 ; les munitions de ses munitions (+0x84 > 0) s'il en a ; sinon les siennes,
// et 1 si elle n'en a pas mais qu'il la tient en main.
static int32
MunitionsDe(int ped, int32 arme)
{
	if(arme < 1) return 0;
	const CTypeArme &t = armes.Type(arme);
	if(t.munition > 0) return inventaire.Munitions(armes, ped, t.munition);
	int32 n = inventaire.Munitions(armes, ped, arme);
	const CScriptPed *p = pietons.Acceder(ped);
	return n == 0 && p && p->armeTenue == arme ? 1 : n;
}

// L'hôte de CObjectData::Initialise (src/objects) : le modèle par son nom (FUN_0051c040), le reste par des hachés (les effets, les sons
// et les butes de ramassage ne servent pas à l'outil).
int32 FindModelIndexByName(const char *name){ return modeles.Index(name); }
uint16 GetEffectIdByName(const char *name){ return (uint16)(ActionHash(name) & 0x7fff); }
uint16 GetSoundBankIdByName(const char *name){ return (uint16)(ActionHash(name) & 0x7fff); }
uint32 GetSoundIdByName(const char *name){ return ActionHash(name); }
uint32 GetStreamIdByName(const char *name, int32){ return ActionHash(name); }
uint32 GetPickupButesByName(const char *name){ return ActionHash(name); }
void ObjectDataPostInit(void) {}

// La ligne de object.dat d'un modèle (CObjectData : la table 0xc35200, puis les alias 0xc34f58), nil sinon.
static const CObjectInfo *
InfoObjet(int32 modele)
{
	for(int i = 0; i < CObjectData::ms_numObjectInfos; i++) if(CObjectData::ms_aObjectInfo[i].modelIndex == modele) return &CObjectData::ms_aObjectInfo[i];
	for(int i = 0; i < CObjectData::ms_numAliases; i++)
		if(CObjectData::ms_aAliases[i].modelIndex == modele && CObjectData::ms_aAliases[i].target >= 0)
			return &CObjectData::ms_aObjectInfo[CObjectData::ms_aAliases[i].target];
	return nil;
}

// Les objets cassables : object.dat (CObjectData::Initialise, 0x4d0c90), puis les entrées spec des .ipb de Stream/World.img dont le
// modèle a une ligne.
static int32
NoterObjet(const CIplSpec &e)
{
	const CObjectInfo *inf = InfoObjet(e.inst.modelId);
	if(inf == nil) return 0;
	CObjetOutil o;
	o.modele = e.inst.modelId; o.nom = e.nom & 0x7fffffff; o.pos = e.inst.pos;
	o.sante = inf->hitPointsAndLock & 0x7fff;
	o.verrouillable = (inf->hitPointsAndLock & 0x8000) != 0;
	o.parArmeSeule = (inf->destroyByWeaponOnly & 1) != 0;
	objets.push_back(o);
	return 0;
}

static int32
NoterInstance(const CIplInst &e)
{
	if(collectibles.Indice(e.unk116) < 0) return 0;
	CCollectiblePose c;
	c.modele = e.modelId; c.pos = e.pos; c.zone = (int8)lroundf(e.unk68); c.hache = e.unk116; c.placement = placementCourant;
	collectiblesPoses.push_back(c);
	return 0;
}

static void
ChargerObjets(void)
{
	CObjectData::Initialise("Config\\Dat\\OBJECT.DAT");
	int32 monde = CdStream::AddImage("Stream\\World.img");
	if(monde < 0) return;
	const CdImage &img = CdStream::ms_images[monde];
	CIplFile::ms_specHandler = NoterObjet;
	CIplFile::ms_instHandler = NoterInstance;
	int fichiers = 0;
	for(int32 i = 0; i < img.m_numEntries; i++){
		const CDirectoryEntry &d = img.m_entries[i];
		size_t n = strlen(d.name);
		if(n < 4 || strcasecmp(d.name + n - 4, ".ipb") != 0) continue;
		std::vector<uint8> buf(d.size * CDSTREAM_SECTOR_SIZE);
		int32 fd = CFileMgr::OpenFile("Stream\\World.img", "rb", 1);
		CFileMgr::Seek(fd, d.offset * CDSTREAM_SECTOR_SIZE, 0);
		bool ok = CFileMgr::ReadExact(fd, buf.data(), (uint32)buf.size());
		CFileMgr::CloseFile(fd);
		if(ok){ placementCourant = (uint8)fichiers; CIplFile::Load(buf.data(), (uint32)buf.size()); fichiers++; }
	}
	CIplFile::ms_specHandler = nil;
	CIplFile::ms_instHandler = nil;
	printf("%d lignes de object.dat ; %zu objets cassables dans les entrées spec de %d .ipb\n", CObjectData::ms_numObjectInfos, objets.size(), fichiers);
	printf("%zu collectibles posés par les entrées inst des .ipb\n", collectiblesPoses.size());
}

// ObjectNameToHashID(nom) (0x5c2390) : le HashString du nom, rendu en pointeur léger (thunk_FUN_0073b520 : le type 2).
static int
ObjectNameToHashID(lua_State *L)
{
	Journal("ObjectNameToHashID", L, 1);
	lua_pushlightuserdata(L, (void *)(uintptr_t)(lua_isstring(L, 1) ? ActionHash(lua_tostring(L, 1)) : 0));
	return 1;
}

// RegisterHashEventHandler(haché, événement, fonction) (0x5bbd80, lu à l'assembleur de 0x5bbd80 à 0x5bbde2) : le haché (FUN_005d8580,
// le pointeur léger), l'événement (FUN_005d8200) ; une fonction (FUN_005d84f0) : l'entrée (FUN_005b3ee0) et la case (FUN_005b3830,
// FUN_005d9f00) ; sinon la case vidée (FUN_005b3b80).
static int
RegisterHashEventHandler(lua_State *L)
{
	Journal("RegisterHashEventHandler", L, 1);
	uint32 h = (uint32)(uintptr_t)lua_touserdata(L, 1) & 0x7fffffff;
	int e = (int)luaL_optnumber(L, 2, 0);
	if(e < 0 || e > 7) return 0;
	CGestionnaireHache *g = nil;
	for(CGestionnaireHache &x : gestionnairesHaches) if(x.nom == h) g = &x;
	if(lua_isfunction(L, 3)){
		if(g == nil){
			if(gestionnairesHaches.size() >= capaciteHaches) return 0;
			gestionnairesHaches.push_back({h, {LUA_NOREF, LUA_NOREF, LUA_NOREF, LUA_NOREF, LUA_NOREF, LUA_NOREF, LUA_NOREF, LUA_NOREF}});
			g = &gestionnairesHaches.back();
		}
		if(g->rappels[e] != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, g->rappels[e]);
		lua_pushvalue(L, 3);
		g->rappels[e] = luaL_ref(L, LUA_REGISTRYINDEX);
	}else if(g && g->rappels[e] != LUA_NOREF){
		luaL_unref(L, LUA_REGISTRYINDEX, g->rappels[e]);
		g->rappels[e] = LUA_NOREF;
	}
	return 0;
}

// SetNumberOfHandledHashEventObjects(n) (0x5bbe70 → FUN_005b3f90, le script appelant en propriétaire) : au-delà des dix entrées en
// place, le tas en prend n − 10 (FUN_005b3a40).
static int
SetNumberOfHandledHashEventObjects(lua_State *L)
{
	Journal("SetNumberOfHandledHashEventObjects", L, 1);
	int n = (int)luaL_optnumber(L, 1, 0);
	if(n > 10) capaciteHaches = (size_t)n;
	return 0;
}

// ObjectToggleVulnerability(modèle, b) (0x5c2e20) : pour chaque CObject de ce modèle (+0x10e), le bit 6 de l'octet +0x209 reçoit b.
static int
ObjectToggleVulnerability(lua_State *L)
{
	Journal("ObjectToggleVulnerability", L, 1);
	int32 m = (int32)luaL_optnumber(L, 1, -1);
	bool b = lua_toboolean(L, 2) != 0;
	for(CObjetOutil &o : objets) if(o.modele == m) o.invulnerable = b;
	return 0;
}

// ObjectGetModelIndex(nom) (0x5c22e0) : FUN_0051c1e0, le modèle de ce nom.
static int
ObjectGetModelIndex(lua_State *L)
{
	Journal("ObjectGetModelIndex", L, 1);
	lua_pushnumber(L, lua_isstring(L, 1) ? modeles.Index(lua_tostring(L, 1)) : -1);
	return 1;
}

// PAnimDestroyedByPed(id[, genre]) (0x5c3950, lu à l'assembleur de 0x5c3950 à 0x5c3a01) : un argument, l'objet animé du déclencheur
// (FUN_006d79c0, son +0x238) ; le genre 0, le CObject de la poignée (FUN_0044a460) et son +0x224 ; le genre 5, l'objet animé du pool
// (FUN_0044a490, +0x238). Sinon 0. L'outil n'a que les CObject.
static int
PAnimDestroyedByPed(lua_State *L)
{
	Journal("PAnimDestroyedByPed", L, 1);
	int r = 0;
	if(lua_gettop(L) >= 2 && (int)luaL_optnumber(L, 2, -1) == 0)
		if(const CObjetOutil *o = ObjetParPoignee((int)luaL_optnumber(L, 1, -1))) r = o->destructeur;
	lua_pushnumber(L, r);
	return 1;
}

int
CJugeOutil::ObjetDuContexte(void) const
{
	if(ped < 0) return objet;
	const CScriptPed *p = pietons.Acceder(ped);
	if(p == nil) return -1;
	if(p->objetUtilise >= 0) return p->objetUtilise;
	if(objetsAnimes.Objet(p->objetVise)) return p->objetVise;
	return objetsAnimes.Objet(p->objetRepere) ? p->objetRepere : -1;
}

// FUN_005f46a0(&DAT_01b652c8, 1, 1) sur le contrôleur de l'objet (+0x128, FUN_004ed7f0) : il joue un nœud nommé NotUseable (le
// HashString 0x44482976), ou un nœud dessous.
static bool
ObjetNonUtilisable(int i)
{
	auto it = controleursObjets.find(i);
	return it != controleursObjets.end() && it->second.joue && arbres.JoueHachage(it->second.joue, ActionHash("NotUseable") & 0x7fffffff);
}

// --- Les objets animés des déclencheurs (src/objects/ObjetsAnimes) ---
static std::map<uint32, CFicheObjetAnime> fichesAnimees;   // par HashString du nom du modèle

// FUN_00520460 : la fiche PropButes du modèle, cherchée en profondeur sous Props/Info de PropGlbl.cat (FUN_0072bd60), sinon
// Props/Info/default. Lue une fois par modèle.
static const CFicheObjetAnime *
FicheAnimee(const std::string &modele)
{
	uint32 h = ActionHash(modele.c_str()) & 0x7fffffff;
	auto it = fichesAnimees.find(h);
	if(it != fichesAnimees.end()) return &it->second;
	CActionTrack t;
	std::map<uint16, std::string> ch;
	static const uint32 kProps = ActionHash("Props"), kInfo = ActionHash("Info"), kButes = ActionHash("PropButes"),
	                    kDefaut = ActionHash("default");
	CFicheObjetAnime f;
	if(CActionBank::ButesCherche("PropGlbl.cat", {kProps, kInfo}, h, kButes, t, &ch) ||
	   CActionBank::ButesChemin("PropGlbl.cat", {kProps, kInfo, kDefaut}, kButes, t, &ch))
		f = CFicheObjetAnime::Lire(t, ch);
	return &(fichesAnimees[h] = f);
}

// FUN_006d6650 : le nom commence par « DT_ » ou « _DT_ » (plus de trois caractères) : le bit 3 de +0x3f du déclencheur.
static bool
NomDePorteDT(const std::string &n)
{
	return n.size() > 3 && (n.compare(0, 3, "DT_") == 0 || n.compare(0, 4, "_DT_") == 0);
}

// Le pool des déclencheurs tel que FUN_00416540 et FUN_00416490 le lisent : le modèle du PLACEMENT de leur périmètre
// (FUN_006d5450 → FUN_0051c240 : -1 pour un nom vide), l'AREACODE, l'emplacement de DATLoad, ISMISSIONSPECIFIC.
static std::vector<CObjetsAnimes::Declencheur>
DeclencheursDuPool(void)
{
	std::vector<CObjetsAnimes::Declencheur> ds;
	for(int h = 0; h < donnees.NombreZones(); h++){
		const CTriggerZone *z = donnees.ZoneParHandle(h);
		if(z == nil) continue;
		CObjetsAnimes::Declencheur d;
		d.handle = h; d.mission = z->mission; d.zone = z->zone; d.emplacement = donnees.EmplacementZone(h);
		d.porte = NomDePorteDT(z->nom); d.pos = z->pos; d.lacet = z->lacet;
		const CTriggerPerimeter *pe = donnees.PerimetreDe(h);
		if(pe && !pe->placement.empty()){
			d.modele = modeles.Index(pe->placement.c_str());
			if(d.modele != -1) d.fiche = FicheAnimee(pe->placement);
		}
		ds.push_back(d);
	}
	return ds;
}

// FUN_00416ae0, à l'arrivée d'un changement de zone : FUN_00416490 retire les objets animés des autres zones, puis
// CStreaming::LoadScene (FUN_005343c0) → FUN_00416c70 → FUN_00416540 crée ceux de la zone visible. La saison
// (DAT_00a147ec[DAT_00a147e0]) n'est pas lue : 0.
static void
PoserObjetsAnimes(int zone)
{
	if(!objetsAnimes.fiche) objetsAnimes.fiche = [](int32 m){ std::string n = NomDeModele(m); return n.empty() ? (const CFicheObjetAnime *)nil : FicheAnimee(n); };
	std::vector<CObjetsAnimes::Declencheur> ds = DeclencheursDuPool();
	std::vector<int> partis = objetsAnimes.RetirerHorsZone(zone, ds);
	if(!partis.empty()) printf("%6.2f s  objets animés : %zu retirés (FUN_00416490), hors de la zone %d\n", maintenant / 1000.0, partis.size(), zone);
	std::vector<int> crees = objetsAnimes.CreerPourZone(zone, ds, 0);
	if(crees.empty()) return;
	std::string liste;
	for(int i : crees){
		const CObjetAnime *o = objetsAnimes.Objet(i);
		const CTriggerZone *z = o ? donnees.ZoneParHandle(o->declencheur) : nil;
		if(z) liste += (liste.empty() ? "" : ", ") + z->nom;
	}
	printf("%6.2f s  objets animés : %zu créés pour la zone %d (FUN_00416540) : %s\n", maintenant / 1000.0, crees.size(), zone, liste.c_str());
}

// PAnimCreate(déclencheur[, x, b]) (0x5c4680, CObjetsAnimes::CreerParScript) : l'objet animé du déclencheur, même propre à une
// mission ; b (le troisième argument, FUN_005d8f20 s'il y en a trois ou plus) va à l'octet +0x3a8, 1 par défaut. Ne rend rien.
static int
PAnimCreate(lua_State *L)
{
	Journal("PAnimCreate", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	uint8 b = lua_gettop(L) >= 3 ? (lua_toboolean(L, 3) ? 1 : 0) : 1;
	if(h < 0 || donnees.ZoneParHandle(h) == nil) return 0;
	if(!objetsAnimes.fiche) objetsAnimes.fiche = [](int32 m){ std::string n = NomDeModele(m); return n.empty() ? (const CFicheObjetAnime *)nil : FicheAnimee(n); };
	for(const CObjetsAnimes::Declencheur &d : DeclencheursDuPool()){
		if(d.handle != h) continue;
		bool avait = objetsAnimes.ObjetDe(h) >= 0;
		int i = objetsAnimes.CreerParScript(d, b);
		const CTriggerZone *z = donnees.ZoneParHandle(h);
		if(i >= 0 && !avait)
			printf("%6.2f s  objets animés : PAnimCreate crée l'objet %d (modèle %d, %s) du déclencheur %s\n", maintenant / 1000.0, i, d.modele,
			       NomDeModele(d.modele).c_str(), z ? z->nom.c_str() : "?");
		break;
	}
	return 0;
}

// CreatePersistentEntity(modèle, x, y, z, cap, zone[, ?]) (0x5bd090, lu dans l'index) : le modèle par son nom (FUN_0051c1e0) ou son
// indice ; inconnu (-1), deux -1 sont rendus. Un modèle animé (le type 10 du modelinfo : les entrées panm) devient un objet animé
// sans déclencheur (FUN_0044cd20, CObjetsAnimes::CreerPersistant) ; un autre, un CObject (0x120, FUN_004ceb60, sans le +0x12 du
// modelinfo ; sinon un CDummyObject changé en CObject de 0x22c, FUN_004d0490) à la position, au cap (+0x10, FUN_00412ad0), la zone
// en +0x111 ; la collision, les effets et le monde (FUN_0045d560) ne sont pas suivis. Rend la poignée de l'entité dans son pool
// (l'indice × 0x100 + l'octet d'état) et son genre : 0 pour un CObject (le pool DAT_00c0f614), 5 pour un objet animé (DAT_00c0f608),
// 4 pour le pool DAT_00c0f5f8. L'outil range ces CObject à part des objets cassables (l'octet 2 de leurs poignées), que ses
// projectiles (des sphères de 0,5 m) ne touchent pas (supposé : gamemain en pose une trentaine dès 1,12 s, dont la porte du bus).
static std::vector<CObjetOutil> persistants;
static int
CreatePersistentEntity(lua_State *L)
{
	Journal("CreatePersistentEntity", L, 1);
	int32 m = lua_type(L, 1) == LUA_TSTRING ? modeles.Index(lua_tostring(L, 1)) : (int32)luaL_optnumber(L, 1, -1);
	if(m < 0){ lua_pushnumber(L, -1); lua_pushnumber(L, -1); return 2; }
	CVector pos((float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0), (float)luaL_optnumber(L, 4, 0));
	float cap = (float)luaL_optnumber(L, 5, 0);
	int8 zone = (int8)luaL_optnumber(L, 6, 0);
	if(modeles.animes.count(m)){
		if(!objetsAnimes.fiche) objetsAnimes.fiche = [](int32 mm){ std::string n = NomDeModele(mm); return n.empty() ? (const CFicheObjetAnime *)nil : FicheAnimee(n); };
		int i = objetsAnimes.CreerPersistant(m, pos, cap, zone);
		const CObjetAnime *o = objetsAnimes.Objet(i);
		printf("%6.2f s  entité persistante : l'objet animé %d (modèle %d, %s)\n", maintenant / 1000.0, i, m, NomDeModele(m).c_str());
		lua_pushnumber(L, i * 0x100 + (o ? (o->generation & 0x7f) : 0));
		lua_pushnumber(L, 5);
		return 2;
	}
	CObjetOutil o;
	o.modele = m; o.pos = pos;
	persistants.push_back(o);
	printf("%6.2f s  entité persistante : le CObject %zu (modèle %d, %s)\n", maintenant / 1000.0, persistants.size() - 1, m, NomDeModele(m).c_str());
	lua_pushnumber(L, (double)((persistants.size() - 1) * 0x100 + 2));
	lua_pushnumber(L, 0);
	return 2;
}

// DeletePersistentEntity(poignée, genre) (0x5bca50, lu dans l'index : FUN_0044a510 retrouve l'entité, FUN_0045dc20 et FUN_0045e540 la
// retirent du monde, son destructeur la détruit) : l'objet animé rendu au pool, ou le CObject persistant de l'outil tenu pour retiré.
static int
DeletePersistentEntity(lua_State *L)
{
	Journal("DeletePersistentEntity", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1, g = (int)luaL_optnumber(L, 2, -1);
	if(h < 0) return 0;
	if(g == 5) objetsAnimes.Liberer(h >> 8);
	else if(g == 0 && (h & 0xff) == 2 && (size_t)(h >> 8) < persistants.size()) persistants[(size_t)(h >> 8)].detruit = true;
	return 0;
}

// --- Les marqueurs du radar (src/hud/Marqueurs : les 300 traces de CRadar, DAT_00c674cc + 0x210) ---
// FUN_005d9220 note chaque marqueur à l'objet script qui le pose (avec ses trois derniers arguments) : l'outil le retire avec la
// mission, comme ses ramassages (supposé : le même retrait que FUN_005d9760).
// (marqueurs et marqueursDuScript : déclarés avec les ramassages.)

// AddBlipForChar(ped, _, icône[, couleur, a, f, g]) (0x5b8bb0, lu à l'assembleur de 0x5b8bb0 à 0x5b8d39) : le piéton (FUN_005c7220),
// sinon -1 ; le deuxième argument n'est pas lu ; l'icône au troisième (FUN_005d8200), la couleur au quatrième (5 par défaut), puis
// un entier (-1) et deux flottants (0 et 2) que FUN_005d9220 garde avec le marqueur. Les traces de ce piéton aux genres 6, 2 et 0xd
// sont rendues (FUN_0054a740), puis FUN_0054a460(l'icône 0x11 : le genre 0xd, sinon 2 ; la poignée du piéton, une couleur RVBA
// nulle, la couleur, l'icône, 0) et l'affichage à 5 (FUN_00548aa0). Rend la poignée du marqueur.
static int
AddBlipForChar(lua_State *L)
{
	Journal("AddBlipForChar", L, 1);
	int ped = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	if(pietons.Acceder(ped) == nil){ lua_pushnumber(L, -1); return 1; }
	int32 icone = (int32)luaL_optnumber(L, 3, 0), couleur = lua_gettop(L) >= 4 ? (int32)luaL_optnumber(L, 4, 5) : 5;
	marqueurs.RetirerEntite(6, ped); marqueurs.RetirerEntite(2, ped); marqueurs.RetirerEntite(0xd, ped);
	int32 h = marqueurs.AjouterEntite(icone == 0x11 ? 0xd : 2, ped, couleur, icone, false, (uint32)maintenant);
	marqueurs.PoserAffichage(h, 5);
	if(h != -1) marqueursDuScript.push_back({h, scriptCourant});
	lua_pushnumber(L, h);
	return 1;
}

// BlipAddPoint(liste, icône[, n, couleur, a, f, g]) (0x5b8690, lu de 0x5b8690 à 0x5b884c) : la liste de points (FUN_006d6300), son
// n-ième point (FUN_006d5860, FUN_006d58a0 ; le premier sans n), l'icône au deuxième argument, la couleur au quatrième (1 par
// défaut), un entier (-1) et deux flottants (0 et 2) pour FUN_005d9220 ; la position notée en DAT_00bf4508 … DAT_00bf4510 et
// DAT_00bf4520 à 1 ; FUN_0054a2c0(4, la position, une couleur RVBA (0, 0x7f, 0, 0xff), la couleur, l'icône, 0), l'affichage à 5.
static int
BlipAddPoint(lua_State *L)
{
	Journal("BlipAddPoint", L, 1);
	int l = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	int n = lua_gettop(L) >= 3 ? (int)luaL_optnumber(L, 3, 1) : 1;
	const CTriggerPointPoint *pt = donnees.Point(l, n < 1 ? 1 : n);
	if(pt == nil) pt = donnees.Point(l, 1);
	CVector pos = pt ? pt->pos : CVector(0, 0, 0);
	int32 icone = (int32)luaL_optnumber(L, 2, 0), couleur = lua_gettop(L) >= 4 ? (int32)luaL_optnumber(L, 4, 1) : 1;
	int32 h = marqueurs.AjouterPoint(4, pos, couleur, icone, false, (uint32)maintenant);
	marqueurs.PoserAffichage(h, 5);
	if(h != -1) marqueursDuScript.push_back({h, scriptCourant});
	lua_pushnumber(L, h);
	return 1;
}

// BlipAddXYZ(x, y, z, icône[, couleur, a, f, g]) (0x5b8400, lu à l'assembleur de 0x5b8400 à 0x5b85c7) : la position (FUN_005d82a0 ×
// 3), l'icône au quatrième argument (FUN_005d8200), la couleur au cinquième (1 par défaut), un entier au sixième (-1) et deux flottants
// (0 et 2) pour FUN_005d9220 ; DAT_00bf4520 à 1 et la position en DAT_00bf4508 … DAT_00bf4510 ; FUN_0054a2c0(4, la position, la
// couleur RVBA (0, 0x7f, 0, 0xff), la couleur, l'icône, 0), l'affichage à 5 (FUN_00548aa0). Rend la poignée du marqueur.
static int
BlipAddXYZ(lua_State *L)
{
	Journal("BlipAddXYZ", L, 1);
	CVector pos((float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0));
	int32 icone = (int32)luaL_optnumber(L, 4, 0), couleur = lua_gettop(L) >= 5 ? (int32)luaL_optnumber(L, 5, 1) : 1;
	int32 h = marqueurs.AjouterPoint(4, pos, couleur, icone, false, (uint32)maintenant);
	marqueurs.PoserAffichage(h, 5);
	if(h != -1) marqueursDuScript.push_back({h, scriptCourant});
	lua_pushnumber(L, h);
	return 1;
}

// BlipRemove(poignée) (0x5b8170, lu de 0x5b8170 à 0x5b81a5) : avec un nombre (FUN_005d81c0), DAT_00bf4520 à 0 et FUN_0054a7f0.
static int
BlipRemove(lua_State *L)
{
	Journal("BlipRemove", L, 1);
	if(!lua_isnumber(L, 1)) return 0;
	marqueurs.Retirer((int32)lua_tonumber(L, 1));
	return 0;
}

// PAnimExists(déclencheur) (0x5c48c0, lu à l'assembleur de 0x5c48c0 à 0x5c4927) : le déclencheur (FUN_005d8200, rien en dessous
// de 0), sa case du pool (DAT_020c7c48 : son octet d'état sans le bit 0x80), son objet (+0x1c), le +0x114 de l'objet (sa
// géométrie) et le +0x94 de celle-ci non nuls ; un booléen (FUN_005d8450). L'outil n'a pas la géométrie : l'objet suffit.
static int
PAnimExists(lua_State *L)
{
	Journal("PAnimExists", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	lua_pushboolean(L, h >= 0 && donnees.ZoneParHandle(h) && objetsAnimes.ObjetDe(h) >= 0);
	return 1;
}

// FUN_005c4410(L, n, &lus) : l'objet animé des premiers arguments, n arguments après eux. Un de plus : un déclencheur (un nombre,
// FUN_006d79c0 : son +0x1c) ou un pointeur léger (FUN_00436ac0, non suivi) ; deux de plus : une case du pool et son genre
// (FUN_0044a510, non suivi) ; sinon un modèle et une position (FUN_00460650, non suivi). -1 sans objet.
static int
ObjetAnimeArg(lua_State *L, int n)
{
	if(lua_gettop(L) == n + 1 && lua_isnumber(L, 1)) return objetsAnimes.ObjetDe((int)lua_tonumber(L, 1));
	return -1;
}

// PAnimSetActionNode(objet, nœud, fichier) (0x5c52a0, lu à l'assembleur de 0x5c52a0 à 0x5c5557) : avec trois arguments, un
// déclencheur (FUN_00415e80 sur le pool 0x20c7e98, son objet +0x1c) ou un pointeur léger (FUN_00436ac0) ; avec sept, un modèle
// et une position (non suivi). Le nœud doit commencer par « \ » ou « / » ; FUN_00520810 donne l'indice du fichier parmi les
// arbres diffusés, FUN_005f59b0 cherche le nœud, et s'il manque le fichier est demandé (FUN_00531ed0(0x5d41 + indice, 0x13)),
// chargé (FUN_00533d90) puis le nœud cherché de nouveau ; trouvé, le fichier est noté à l'objet script (FUN_005db7e0,
// FUN_005d9990) et FUN_004ef1f0(nœud, indice) le pose (lu dans l'index) : avec le fichier de l'arbre de l'objet (+0x22c) et une
// fiche qui en a un (+0x6c), FUN_004edec0 joue le nœud tel quel (+0x188 à 0, FUN_005f48f0, puis une image du contrôleur,
// FUN_005f49a0) ; sinon le nœud devient la racine (+0x228) et le fichier +0x22c, l'action des piétons (+0x230) revient à 0, et la
// descente (+0x4c) dans le contexte de l'objet (DAT_00a453f8, sans piéton) choisit ce que FUN_004edec0 joue. Rien n'est rendu.
static int
PAnimSetActionNode(lua_State *L)
{
	Journal("PAnimSetActionNode", L, 1);
	int i = lua_gettop(L) == 3 && lua_isnumber(L, 1) ? objetsAnimes.ObjetDe((int)lua_tonumber(L, 1)) : -1;
	CObjetAnime *o = objetsAnimes.Objet(i);
	const char *noeud = luaL_optstring(L, 2, ""), *fichier = luaL_optstring(L, 3, "");
	if(o == nil || (noeud[0] != '/' && noeud[0] != '\\')) return 0;
	if(!arbres.TrouverOuCharger(noeud, fichier)){
		printf("        PAnimSetActionNode : %s absent de %s\n", noeud, fichier);
		return 0;
	}
	const CActionNode *n = arbres.Trouver(noeud);
	CActionController &c = controleursObjets[i];
	c.fige = true;
	float t = maintenant / 1000.0f;
	bool meme = !o->fichier.empty() && ActionHash(CActionBank::NomCat(o->fichier.c_str()).c_str()) == ActionHash(CActionBank::NomCat(fichier).c_str());
	if(meme) c.Jouer(arbres, n, t);
	else {
		o->fichier = fichier;
		o->actionPieton = nil;
		c.arbre = n;
		c.Racine(arbres, n);
		if(!c.Entrer(arbres, n, CJugeOutil::DeLObjet(i, t), t)) c.joue = nil;
	}
	if(journalRestant > 0) printf("%6.2f s  objet animé du déclencheur %d : %s%s\n", t, o->declencheur, c.joue ? arbres.CheminDe(c.joue).c_str() : "rien",
	                              meme ? "" : " (par la descente)");
	return 0;
}

// PAnimSetPropFlag(objet, n, valeur) (0x5c55b0, lu à l'assembleur de 0x5c55b0 à 0x5c560b) : l'objet de FUN_005c4410(L, 2), le
// nombre n (FUN_005d8200) et le booléen (FUN_005d8f20) passés à FUN_004ea300. PAnimGetPropFlag(objet, n) (0x5c5610, lu de
// 0x5c5610 à 0x5c5673) : FUN_005c4410(L, 1), le mot +0x160 + 4n non nul ; faux sans objet.
static int
PAnimSetPropFlag(lua_State *L)
{
	Journal("PAnimSetPropFlag", L, 1);
	int i = ObjetAnimeArg(L, 2);
	if(i >= 0) objetsAnimes.PoserDrapeau(i, (int)lua_tonumber(L, 2), lua_toboolean(L, 3) != 0);
	return 0;
}

static int
PAnimGetPropFlag(lua_State *L)
{
	Journal("PAnimGetPropFlag", L, 1);
	int i = ObjetAnimeArg(L, 1);
	lua_pushboolean(L, i >= 0 && objetsAnimes.Drapeau(i, lua_isnumber(L, 2) ? (int)lua_tonumber(L, 2) : -1));
	return 1;
}

// Une image des contrôleurs des objets animés (le contrôleur +0x1bc de chacun) : les pistes PropFlags (FUN_00655b50) et
// PropSetPedAction (FUN_006561a0) de leur nœud, lancées à leur entrée, posent les drapeaux de l'objet et ce qu'il offre aux
// piétons (+0x230).
static void
AvancerObjets(void)
{
	float t = maintenant / 1000.0f;
	for(auto it = controleursObjets.begin(); it != controleursObjets.end(); ){
		CObjetAnime *o = objetsAnimes.Objet(it->first);
		if(o == nil){ it = controleursObjets.erase(it); continue; }
		CActionController &c = it->second;
		c.Avancer(arbres, nil, CJugeOutil::DeLObjet(it->first, t), t);
		for(const CActionController::DrapeauObjet &d : c.drapeauxObjetPoses) objetsAnimes.PoserDrapeau(it->first, d.numero, d.valeur);
		c.drapeauxObjetPoses.clear();
		for(const CActionController::ActionObjet &a : c.actionsObjetPosees){
			o->actionPieton = a.noeud;
			if(journalRestant > 0) printf("%6.2f s  objet animé du déclencheur %d : il offre %s aux piétons\n", t, o->declencheur,
			                              a.noeud ? arbres.CheminDe(a.noeud).c_str() : "rien");
		}
		c.actionsObjetPosees.clear();
		++it;
	}
}

// PropInteract (PropInteractTrack 0x935440 ; lancement FUN_00656120, lu dans l'index) : l'objet visé du piéton (+0x438), s'il est
// animé (les 3 bits bas de +0x108 à 6) et offre une action (+0x230) : son drapeau 10 à 0 (FUN_004ea300), l'action descendue dans le
// contexte du piéton (son emplacement +0x4c, jugée pour lui) et jouée par son contrôleur (FUN_005f4490), puis FUN_004ed6e0 (lu dans
// l'index) : l'objet prend le piéton (+0x234), le piéton l'objet (+0x548) et +0x1520 à 1 ; FUN_004eac70(piéton, 3 pour le joueur,
// 2 sinon, 1) et FUN_0048be60 (le modèle noté au composant +0x3bc) ne sont pas suivis.
static void
Interagir(int ped, CActionController &c)
{
	CScriptPed *p = pietons.Acceder(ped);
	CObjetAnime *o = p ? objetsAnimes.Objet(p->objetVise) : nil;
	if(o == nil || o->actionPieton == nil) return;
	float t = maintenant / 1000.0f;
	objetsAnimes.PoserDrapeau(p->objetVise, 10, false);
	CJugeOutil juge(ped, t);
	const CActionNode *n = arbres.Choisir(o->actionPieton, juge);
	if(n) c.Jouer(arbres, n, t, &juge);
	o->utilisateur = ped;
	p->objetUtilise = p->objetVise;
	p->utiliseObjet = true;
	printf("%6.2f s  PropInteract : le piéton %d se sert de l'objet animé du déclencheur %d : %s\n", t, ped, o->declencheur,
	       n ? arbres.CheminDe(n).c_str() : "rien n'aboutit");
}

// L'écran de fin (src/hud/EcranDeFin.h). MinigameSetCompletion(texte, réussi[, argent[, sous-texte]]) (0x708cc0, lue à l'assembleur
// de 0x708cc0 à 0x708d42) : la chaîne (FUN_005d82e0), le booléen (FUN_005d8f20), avec trois arguments l'argent (FUN_005d8200,
// un entier fild), avec quatre le sous-texte, puis FUN_00707390. MinigameAddCompletionMsg(texte[, n]) (0x708de0 → FUN_00705360).
// MinigameIsShowingCompletion() (0x708f90).
static CEcranDeFin ecranDeFin;

static int
MinigameSetCompletion(lua_State *L)
{
	Journal("MinigameSetCompletion", L, 1);
	int n = lua_gettop(L);
	ecranDeFin.Poser(luaL_optstring(L, 1, ""), lua_toboolean(L, 2) != 0, n >= 3 ? (float)(int)luaL_optnumber(L, 3, 0) : 0.0f,
	                 n >= 4 ? luaL_optstring(L, 4, "") : nil);
	printf("%6.2f s  écran de fin : %s%s, %.0f\n", maintenant / 1000.0, ecranDeFin.texte.c_str(), ecranDeFin.reussi ? " (réussi)" : "", ecranDeFin.argent);
	return 0;
}

static int
MinigameAddCompletionMsg(lua_State *L)
{
	Journal("MinigameAddCompletionMsg", L, 1);
	ecranDeFin.Ajouter(luaL_optstring(L, 1, ""), lua_gettop(L) >= 2 ? (int32)luaL_optnumber(L, 2, 0) : 0);
	return 0;
}

static int
MinigameIsShowingCompletion(lua_State *L)
{
	Journal("MinigameIsShowingCompletion", L, 1);
	lua_pushboolean(L, ecranDeFin.Montre());
	return 1;
}

// Les objectifs de mission (src/hud/ObjectifsMission.h, l'objet 0x20c4420). MissionObjectiveAdd(texte[, octet[, rappel]])
// (0x5c1de0, lue à l'assembleur de 0x5c1de0 à 0x5c1e5b) : la chaîne (FUN_005d82e0), avec deux arguments l'octet (FUN_005d8200),
// FUN_006ae110 ; avec trois, le délai du rappel (FUN_005d82a0 → FUN_006ae420, non suivi) ; rend l'indice de l'objectif
// (FUN_005d8460 ; -1 quand l'écran de fin se montre ou que dix sont pris). MissionObjectiveComplete(i) (0x5c1f30 → FUN_006ae2f0(i, 1))
// et MissionObjectiveRemove(i) (0x5c1f60 → FUN_006ae320(i)). Le bouchon ne rendait rien : 1_06_01 passe ce résultat à table.insert.
static CObjectifsMission objectifsMission;

static int
MissionObjectiveAdd(lua_State *L)
{
	Journal("MissionObjectiveAdd", L, 1);
	uint8 octet = lua_gettop(L) >= 2 ? (uint8)(int)luaL_optnumber(L, 2, 0) : 0;
	int i = objectifsMission.Ajouter(lua_isstring(L, 1) ? lua_tostring(L, 1) : nil, octet, ecranDeFin.Montre(), (uint32)maintenant);
	lua_pushnumber(L, i);
	return 1;
}

static int
MissionObjectiveComplete(lua_State *L)
{
	Journal("MissionObjectiveComplete", L, 1);
	if(lua_isnumber(L, 1)) objectifsMission.Terminer((uint8)(int)lua_tonumber(L, 1), true, (uint32)maintenant);
	return 0;
}

static int
MissionObjectiveRemove(lua_State *L)
{
	Journal("MissionObjectiveRemove", L, 1);
	if(lua_isnumber(L, 1)) objectifsMission.Retirer((uint8)(int)lua_tonumber(L, 1), (uint32)maintenant);
	return 0;
}

// PAnimRequest(déclencheur) (0x5c4590, lue à l'assembleur de 0x5c4590 à 0x5c4673) : la case du pool des déclencheurs (0x20c7c48 ;
// nulle pour un indice négatif ou une case libre), son périmètre (+0x20) et le modèle de son PLACEMENT (+0x10) ; sans modèle (-1),
// vrai. Sinon le modèle doit être chargé (FUN_0052dbe0 : l'octet +4 de son entrée de diffusion à 1 ; sinon FUN_00531ed0(modèle, 0x1e)
// le demande et la réponse est fausse), puis sa collision (le mot +0x30 du +0xc du modelinfo : l'entrée 0x56b8 + n ; sinon
// FUN_00531ed0(…, 0x16)) ; le modèle est noté à l'objet script courant (FUN_005db7e0, FUN_005d92f0 : relâché à la fin de la mission).
// L'outil n'a pas de diffusion : un modèle est chargé dès qu'on le demande (comme pour RequestModel), la réponse est vraie.
static int
PAnimRequest(lua_State *L)
{
	Journal("PAnimRequest", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	const CTriggerPerimeter *pe = h >= 0 ? donnees.PerimetreDe(h) : nil;
	int32 m = pe && !pe->placement.empty() ? modeles.Index(pe->placement.c_str()) : -1;
	if(m != -1 && journalRestant > 0) printf("%6.2f s  PAnimRequest : le modèle %d (%s) du déclencheur %d est demandé, chargé (supposé)\n",
	                                        maintenant / 1000.0, m, pe->placement.c_str(), h);
	lua_pushboolean(L, 1);
	return 1;
}

// AreaRegisterAnimProps() (0x5b6d60 → 0x438700) : la table DAT_00bd3e78 jusqu'à son compte DAT_00bd5c54, que rien n'augmente
// (vérifié à l'assembleur : écrit seulement en 0x416b07, remis à 0) ; elle ne crée rien.
static int
AreaRegisterAnimProps(lua_State *L)
{
	Journal("AreaRegisterAnimProps", L, 1);
	return 0;
}

// GetPointListSize(liste) (0x5bc740) : la liste du pool (CPointPool__GetSlot), 0 sans elle ; sinon FUN_006d5800 compte les points de
// sa chaîne (le mot +4, puis les suivants jusqu'à 0xfffe ; 0 sans premier point).
static int
GetPointListSize(lua_State *L)
{
	Journal("GetPointListSize", L, 1);
	const CTriggerPointList *l = lua_isnumber(L, 1) ? donnees.Liste((int)lua_tonumber(L, 1)) : nil;
	lua_pushnumber(L, l ? (int)l->points.size() : 0);
	return 1;
}

// PickupCreateXYZ(modèle, x, y, z[, fiche]) (0x5d0b90) : avec cinq arguments, la fiche de ramassage du nom (FUN_005fa7b0, FUN_005d0b20 :
// PickupData/<nom> dans l'arbre des réglages) ; FUN_00443e90(x, y, z, modèle, 1, fiche, 0, 0) → FUN_00443af0(x, y, z, modèle, fiche, 0,
// 0, 0, 0) (sans fiche, celle du type de l'arme, +0x88, ou la fiche par défaut DAT_00c0f560), puis le bit 2 de +0x46 au cinquième
// argument (1 : ramassé à la main) ; la poignée notée à l'objet script courant (FUN_005db7e0, FUN_005d9760) et rendue. L'outil garde
// le nom de la fiche, sans la lire (son octet +0x18, le bit 0, est supposé nul).
static int
PickupCreateXYZ(lua_State *L)
{
	Journal("PickupCreateXYZ", L, 1);
	int32 m = (int32)luaL_optnumber(L, 1, -1);
	CVector pos((float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0), (float)luaL_optnumber(L, 4, 0));
	std::string fiche = lua_gettop(L) == 5 && lua_isstring(L, 5) ? lua_tostring(L, 5) : "";
	int32 h = ramassages.Creer(pos, m, fiche, false, false, 0, 0, false, armes.Type(m).decalageRamassage);
	if(CRamassage *r = ramassages.Entree(h)){ r->drapeaux |= RAMASSAGE_A_LA_MAIN; r->script = scriptCourant; }
	lua_pushnumber(L, h);
	return 1;
}

// PickupCreateFromPed(modèle, ped[, fiche]) (0x5d0cc0, lue à l'assembleur de 0x5d0cc0 à 0x5d0d5e) : le modèle (FUN_005d8200), le
// piéton (FUN_005c7220, le genre 2 de l'objet script) ; avec exactement trois arguments (FUN_0073ad50), la fiche de ramassage du nom
// (FUN_005fa7b0, FUN_005d0b20 : PickupData/<nom>), sinon aucune ; FUN_00443d90(piéton, modèle, fiche) (CRamassages::CreerDuPieton) ;
// la poignée notée à l'objet script courant (FUN_005db7e0, FUN_005d9760 : retirée avec la mission) et rendue (FUN_005d8460). Le jeu
// ne vérifie pas le piéton (FUN_00443d90 lit sa matrice) : sans lui, l'outil ne crée rien et ne rend rien.
static int
PickupCreateFromPed(lua_State *L)
{
	Journal("PickupCreateFromPed", L, 1);
	int32 m = (int32)luaL_optnumber(L, 1, -1);
	const CScriptPed *p = lua_isnumber(L, 2) ? pietons.Acceder((int)lua_tonumber(L, 2)) : nil;
	if(p == nil) return 0;
	std::string fiche = lua_gettop(L) == 3 && lua_isstring(L, 3) ? lua_tostring(L, 3) : "";
	int32 h = ramassages.CreerDuPieton(p->pos, m, fiche, armes.Type(m).decalageRamassage);
	if(CRamassage *r = ramassages.Entree(h)){
		r->script = scriptCourant;
		if(journalRestant > 0) printf("%6.2f s  ramassage %d (modèle %d, %s) laissé par le piéton %d en (%.2f, %.2f, %.2f)\n", maintenant / 1000.0,
		                              h, m, fiche.c_str(), (int)lua_tonumber(L, 2), r->pos.x, r->pos.y, r->pos.z);
	}
	lua_pushnumber(L, h);
	return 1;
}

// PickupDelete(poignée) (0x5d0d60, hors de l'index, non lu) : l'outil libère l'entrée.
static int
PickupDelete(lua_State *L)
{
	Journal("PickupDelete", L, 1);
	ramassages.Retirer(ramassages.Indice(lua_isnumber(L, 1) ? (int32)lua_tonumber(L, 1) : -1));
	return 0;
}

// Le joueur ramasse le ramassage le plus proche, à moins de 2 m (supposé) : dans le moteur, la banque Weapons/PickUpActions (une
// ActionRequest, TargetProp) joue l'animation de prise puis PropPickup (l'objet visé, son arme en passage, +0x1574) et PropAttach (l'arme
// en main ; avec l'octet +0x22, l'arbre de l'arme : /Global/Thrown pour la brique) ; l'entrée du ramassage est retirée. L'outil fait
// ces trois pas d'un coup ; la banque NeedInit de l'arbre de l'arme pose ensuite les réglages (SetWeaponInfo).
static void
Ramasser(void)
{
	CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	int mieux = -1; float d2 = 4.0f;
	for(int i = 0; i < CRamassages::kTaille; i++){
		const CRamassage &q = ramassages.entrees[i];
		if(!q.Pris() || !(q.drapeaux & RAMASSAGE_A_LA_MAIN)) continue;
		float dx = q.pos.x - j->pos.x, dy = q.pos.y - j->pos.y, dz = q.pos.z - j->pos.z;
		float e = dx * dx + dy * dy + dz * dz;
		if(e < d2){ d2 = e; mieux = i; }
	}
	if(mieux < 0){ printf("%6.2f s  le joueur ne trouve rien à ramasser à moins de 2 m\n", maintenant / 1000.0); return; }
	const CRamassage r = ramassages.entrees[mieux];
	ramassages.Retirer(mieux);
	printf("%6.2f s  le joueur ramasse %d (modèle %d, %s) en (%.2f, %.2f, %.2f)\n", maintenant / 1000.0, mieux | (int)r.generation << 16, r.modele,
	       NomDArme(r.modele) ? NomDArme(r.modele)->c_str() : "?", r.pos.x, r.pos.y, r.pos.z);
	if(armes.Type(r.modele).inventaire != 0) inventaire.Donner(armes, SCRIPT_PED_JOUEUR, r.modele);   // une arme qui se range
	if(j->armeTenue != -1){ j->armePassage = j->armeTenue; LacherArme(SCRIPT_PED_JOUEUR); }   // PropDetach de l'arme d'avant
	j->armePassage = r.modele;                                                                  // PropPickup
	CActionController::PisteArme d{0.0f, CActionController::ARME_ATTACHER, 0, true, true};      // PropAttach, l'octet +0x22
	CActionController &c = controleurs[SCRIPT_PED_JOUEUR];
	c.pistesArmeLancees.push_back(d);
	AppliquerArmes(SCRIPT_PED_JOUEUR, c);
}

// --- Les collectibles (src/objects/Collectibles : CCollectiblesMgr, l'objet 0xbceb50, déclaré avec les ramassages) ---
static int rappelCollectibles = LUA_NOREF;   // +0x1e8c (0xbd09dc) : CollectiblesOnCollectedCallback

// CGame::Initialise (FUN_0042ee90, après CPickups::Init) → FUN_00428bf0 → FUN_004287a0 : config\dat\colctbls.dat lu en entier
// (FUN_004264c0 : ouvert, sa taille par FUN_0042d1b0(f, 0, 2) et FUN_0042d2e0, un bloc de GameMalloc), puis ligne à ligne.
static void
ChargerCollectibles(void)
{
	static uint8 texte[0x4000];
	int32 n = CFileMgr::LoadFile("config\\dat\\colctbls.dat", texte, sizeof(texte), "rb");
	collectibles.Initialiser((const char *)texte, n > 0 ? (size_t)n : 0);
	printf("%d collectibles dans colctbls.dat :", collectibles.nombre);
	static const char *noms[NUM_COLLECTIBLE_TYPES] = { "nains", "transistors", "élastiques", "cartes", "citrouilles", "tombes", "artefacts" };
	for(int t = 0; t < NUM_COLLECTIBLE_TYPES; t++) printf(" %d %s%s", collectibles.NombreDuType(t), noms[t], t + 1 < NUM_COLLECTIBLE_TYPES ? "," : "\n");
}

// Le rappel des scripts de FUN_004288c0 (FUN_005d7f80 : il y en a un ; FUN_005d9ac0, FUN_005d8110, FUN_005d8510 : sa fonction
// poussée ; six nombres par FUN_005d8460 ; FUN_005d8140(6, 0) l'appelle) : le type, les pris du type, leur nombre, le haché, la
// poignée du ramassage et l'indice. Le jeu l'appelle dans le script et le thread qui l'ont posé (FUN_005d8830) ; l'outil, dans
// l'état qu'on lui donne.
static void
AppelerRappelCollectible(lua_State *L, const CRappelCollectible &r)
{
	if(rappelCollectibles == LUA_NOREF || L == nil) return;
	printf("%6.2f s  collectibles : le rappel des scripts est appelé (%d, %d, %d, %#x, %d, %d)\n", maintenant / 1000.0, r.type, r.pris,
	       r.nombre, r.hache, r.ramassage, r.indice);
	lua_rawgeti(L, LUA_REGISTRYINDEX, rappelCollectibles);
	lua_pushnumber(L, r.type); lua_pushnumber(L, r.pris); lua_pushnumber(L, r.nombre);
	lua_pushnumber(L, (double)r.hache); lua_pushnumber(L, r.ramassage); lua_pushnumber(L, r.indice);
	if(lua_pcall(L, 6, 0, 0) != 0){ printf("        rappel des collectibles : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
}

// La population (FUN_004a0a10 → FUN_004a0130, lu dans l'index et à l'assembleur de 0x4a0130 à 0x4a0200) : à chaque image, un
// trente-deuxième du pool des objets fictifs (DAT_00c0f600, par le compteur d'images DAT_00c1a99c & 0x1f) ; un objet fictif de la zone
// visible (+0x111 = DAT_00bd1008), de la zone 1, ou de la zone DAT_00bd09eb quand DAT_00bd09ec est posé (non suivi), à moins de 80 m
// au plan (_DAT_009051d8) du joueur (FUN_00446890(0) : sa position, ou celle de son véhicule) passe à FUN_0049f2f0 (lu dans l'index) :
// FUN_0049e390 (non suivi, supposé vrai) ; un collectible (FUN_00426270 sur +0x124) qui n'est ni un nain, ni une citrouille, ni une
// tombe (FUN_00426210, FUN_00426230, FUN_00426250) devient un ramassage (FUN_00445640), dont la poignée est notée au collectible
// (FUN_004262c0), et l'objet fictif est retiré (FUN_0045dc20, son destructeur) ; un nain, une citrouille ou une tombe pas pris deviennent
// un CObject de 0x22c octets (FUN_004d0490 : des objets à casser, non suivis). L'outil passe tous les collectibles posés à chaque image
// (jusqu'à 31 images plus tôt que le jeu) ; une entrée changée en ramassage n'a plus d'objet fictif, et l'outil, qui ne
// rediffuse pas les placements, ne la repose plus (un ramassage retiré, ObjectRemovePickupsInTrigger, ne revient pas).
static void
PoserLesCollectibles(void)
{
	const CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	if(j == nil || !joueurPose) return;   // supposé : le joueur de l'outil n'est nulle part avant d'être posé
	for(CCollectiblePose &c : collectiblesPoses){
		int i = collectibles.Indice(c.hache);
		if(c.converti || i < 0 || collectibles.Pris(i) || !collectibles.Disponible(i) || collectibles.AuSol(i)) continue;
		if(c.zone != zones.zoneVisible && c.zone != 1) continue;
		float dx = c.pos.x - j->pos.x, dy = c.pos.y - j->pos.y;
		if(dx * dx + dy * dy >= 80.0f * 80.0f) continue;
		int32 h = ramassages.CreerDuCollectible(c.pos, c.modele, (int16)i, c.hache, c.placement, armes.Type(c.modele).decalageRamassage);
		collectibles.PoserRamassage(i, h);
		c.converti = true;   // FUN_0045dc20 et le destructeur : l'objet fictif n'est plus
		const std::string *nom = NomDArme(c.modele);
		printf("%6.2f s  collectibles : l'entrée %d (modèle %d, %s) devient le ramassage %#x en (%.2f, %.2f, %.2f), à %.1f m du joueur\n",
		       maintenant / 1000.0, i, c.modele, nom ? nom->c_str() : "?", h, c.pos.x, c.pos.y, c.pos.z, sqrtf(dx * dx + dy * dy));
	}
}

// CPickups::Update (FUN_00445970, lu à l'assembleur de 0x445970 à 0x44598f) : FUN_00445710 pour chaque entrée prise du pool, de
// 0xc0a100 à 0xc0f560. La prise au contact (CRamassages::AuContact) mène à FUN_00444ac0(joueur, 1) ; l'outil en suit les objets (les
// genres 2 à 4 du type, CRamassages::PrendreObjet) et appelle le rappel des collectibles après le compte (le jeu l'appelle depuis
// FUN_004288c0, juste avant FUN_00541ae0).
static void
AvancerRamassages(void)
{
	const CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	if(j == nil || !joueurPose) return;
	for(int i = 0; i < CRamassages::kTaille; i++){
		const CRamassage &r = ramassages.entrees[i];
		if(!r.Pris()) continue;
		const CTypeArme &t = armes.Type(r.modele);
		if(!ramassages.AuContact(i, j->pos, j->sante, t.auContact)) continue;
		const CRamassage avant = r;
		CRappelCollectible rappel;
		bool aRappel = false;
		int pris = ramassages.PrendreObjet(i, t.genre, armes, objetsJoueur, collectibles, &rappel, &aRappel);
		const std::string *nom = NomDArme(avant.modele);
		if(pris) printf("%6.2f s  le joueur prend au contact le ramassage %#x (modèle %d, %s%s) en (%.2f, %.2f, %.2f) : il en a %d\n",
		                maintenant / 1000.0, ramassages.Poignee(i), avant.modele, nom ? nom->c_str() : "?",
		                avant.drapeaux & RAMASSAGE_COLLECTIBLE ? ", un collectible" : "", avant.pos.x, avant.pos.y, avant.pos.z,
		                objetsJoueur.Nombre(avant.modele));
		if(aRappel) AppelerRappelCollectible(etatLua, rappel);
	}
}

// ObjectRemovePickupsInTrigger(déclencheur[, modèle]) (0x5c31b0, hors de l'index ; lue à l'assembleur de 0x5c31b0 à 0x5c3338) : le
// déclencheur (FUN_005d8200, la case du pool 0x20c7c48, nulle pour un indice négatif ou une case libre), le modèle au second argument
// s'il y en a deux (-1 sinon) ; les ramassages de ce modèle dans le déclencheur sont retirés (CRamassages::RetirerSi) ; puis les
// CObject du pool DAT_00c0f618 de ce modèle (+0x10e) dans le déclencheur, au plus 256, sont retirés du monde (FUN_0045dc20,
// FUN_0045e540) et détruits : l'outil n'a que les objets cassables des entrées spec, non suivis ici.
static int
ObjectRemovePickupsInTrigger(lua_State *L)
{
	Journal("ObjectRemovePickupsInTrigger", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	int32 m = lua_gettop(L) > 1 ? (int32)luaL_optnumber(L, 2, -1) : -1;
	if(h < 0) return 0;
	int n = ramassages.RetirerSi(m, [&](const CVector &p){ return donnees.DansDeclencheur(h, p); });
	if(n) printf("%6.2f s  %d ramassage(s) retiré(s) du déclencheur %d\n", maintenant / 1000.0, n, h);
	return 0;
}

// CollectiblesSetTypeAvailable(type[, b]) (0x5bafa0, lu à l'assembleur de 0x5bafa0 à 0x5bafe7) : b vrai par défaut, le second
// argument s'il y en a deux (FUN_005d8f20) ; le type (FUN_005d8200) ; FUN_00426340(type, b) : l'octet « disponible » (+5 + 2i) de
// chaque entrée du type.
static int
CollectiblesSetTypeAvailable(lua_State *L)
{
	Journal("CollectiblesSetTypeAvailable", L, 1);
	bool b = lua_gettop(L) >= 2 ? lua_toboolean(L, 2) != 0 : true;
	collectibles.PoserDisponible((int)luaL_optnumber(L, 1, -1), b);
	return 0;
}

// CollectiblesSetAllAsCollected(type[, b]) (0x5bb010, lu de 0x5bb010 à 0x5bb058) : le type, b vrai par défaut ; FUN_00428ba0 passe
// chaque entrée du type à FUN_004288c0(i, b), qui appelle le rappel des scripts pour chacune quand b est vrai.
static int
CollectiblesSetAllAsCollected(lua_State *L)
{
	Journal("CollectiblesSetAllAsCollected", L, 1);
	bool b = lua_gettop(L) >= 2 ? lua_toboolean(L, 2) != 0 : true;
	CRappelCollectible r[CCollectibles::kTaille];
	int n = collectibles.ToutMarquer((int)luaL_optnumber(L, 1, -1), b, r, CCollectibles::kTaille);
	for(int i = 0; i < n; i++) AppelerRappelCollectible(L, r[i]);
	return 0;
}

// CollectiblesGetNumCollectable(type) (0x5bb060) : FUN_004262e0, le mot +0x1454 + 2 × type ; CollectiblesGetNumCollected(type)
// (0x5bb090) : FUN_004262f0, les entrées du type à l'octet « pris » posé (lues à l'assembleur de 0x5bb060 à 0x5bb0be).
static int
CollectiblesGetNumCollectable(lua_State *L)
{
	Journal("CollectiblesGetNumCollectable", L, 1);
	lua_pushnumber(L, collectibles.NombreDuType((int)luaL_optnumber(L, 1, -1)));
	return 1;
}

static int
CollectiblesGetNumCollected(lua_State *L)
{
	Journal("CollectiblesGetNumCollected", L, 1);
	lua_pushnumber(L, collectibles.PrisDuType((int)luaL_optnumber(L, 1, -1)));
	return 1;
}

// CollectiblesOnCollectedCallback(fonction) (0x5baff0 → FUN_005d9f00(L, 0, 0xbd09dc)) : l'argument mis au registre (luaL_ref,
// FUN_0073c360), l'ancien rappel lâché (FUN_005d9a60) ; le jeu note le script et le thread courants (FUN_005dbbf0, +0x114c).
static int
CollectiblesOnCollectedCallback(lua_State *L)
{
	Journal("CollectiblesOnCollectedCallback", L, 1);
	if(lua_gettop(L) < 1) return 0;
	if(rappelCollectibles != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, rappelCollectibles);
	lua_pushvalue(L, 1);
	rappelCollectibles = luaL_ref(L, LUA_REGISTRYINDEX);
	return 0;
}

// --- Les objets du joueur (src/peds/Armes : CObjetsJoueur, les 80 compteurs de DAT_00cf0060, déclarés avec les ramassages) ---

// Les plages des modèles weap (0xa136b0 … 0xa136b4) et item (0xa136b8 … 0xa136bc) : les entrées des sections weap et item des
// .idb (src/core/ModelEnum), de la première à la dernière.
static bool
DansLaPlage(const std::map<int32, std::string> &m, int32 id)
{
	return !m.empty() && id >= m.begin()->first && id <= m.rbegin()->first;
}

// ItemGetCurrentNum(modèle) (0x5be2d0, lu à l'assembleur de 0x5be2d0 à 0x5be33f) : le modèle (FUN_005d8200) ; une arme, ses
// munitions dans l'inventaire du joueur (FUN_0045cdf0 sur DAT_00c1aea8 + 0x1c4) ; un item, son compteur (FUN_00541cb0) ; 0 sinon.
static int
ItemGetCurrentNum(lua_State *L)
{
	Journal("ItemGetCurrentNum", L, 1);
	int32 m = (int32)luaL_optnumber(L, 1, -1), n = 0;
	if(DansLaPlage(modeles.armes, m)) n = inventaire.Munitions(armes, SCRIPT_PED_JOUEUR, m);
	else if(DansLaPlage(modeles.objets, m)) n = objetsJoueur.Nombre(m);
	lua_pushnumber(L, n);
	return 1;
}

// ItemSetCurrentNum(modèle, n) (0x5be340, lu de 0x5be340 à 0x5be3a2) : une arme, FUN_0045d0f0(modèle, n, 1) sur l'inventaire du
// joueur (posée si elle y est) ; un item, FUN_00541c10(modèle, n).
static int
ItemSetCurrentNum(lua_State *L)
{
	Journal("ItemSetCurrentNum", L, 1);
	int32 m = (int32)luaL_optnumber(L, 1, -1), n = (int32)luaL_optnumber(L, 2, 0);
	if(DansLaPlage(modeles.armes, m)) inventaire.PoserMunitions(armes, SCRIPT_PED_JOUEUR, m, n);
	else if(DansLaPlage(modeles.objets, m)) objetsJoueur.Poser(armes, m, n);
	return 0;
}

// GiveWeaponToPlayer(arme[, b]) (0x5be100, lu à l'assembleur de 0x5be100 à 0x5be151) : l'arme (FUN_005d8200), b vrai par défaut
// (FUN_005d8f20 avec deux arguments ou plus), puis FUN_0045ccb0(arme, b, 0) sur l'inventaire du joueur (DAT_00c1aea8 + 0x1c4).
static int
GiveWeaponToPlayer(lua_State *L)
{
	Journal("GiveWeaponToPlayer", L, 1);
	inventaire.Donner(armes, SCRIPT_PED_JOUEUR, (int32)luaL_optnumber(L, 1, -1), false);
	return 0;
}

// GiveAmmoToPlayer(arme, n[, b]) (0x5be160, lu à l'assembleur de 0x5be160 à 0x5be1be) : FUN_0045cd80(arme, n, b, 0) sur
// l'inventaire du joueur.
static int
GiveAmmoToPlayer(lua_State *L)
{
	Journal("GiveAmmoToPlayer", L, 1);
	inventaire.AjouterMunitions(armes, SCRIPT_PED_JOUEUR, (int32)luaL_optnumber(L, 1, -1), (int32)luaL_optnumber(L, 2, 0));
	return 0;
}

// PedGetAmmoCount(ped, arme) (0x5ceca0, lu à l'assembleur de 0x5ceca0 à 0x5cecf6) : FUN_0045cdf0(arme) sur l'inventaire du
// piéton (+0x1c4), 0 sans piéton.
static int
PedGetAmmoCount(lua_State *L)
{
	Journal("PedGetAmmoCount", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	lua_pushnumber(L, pietons.Acceder(h) ? inventaire.Munitions(armes, h, (int32)luaL_optnumber(L, 2, -1)) : 0);
	return 1;
}

// WeaponGetType(arme) (0x5c63d0, lu à l'assembleur de 0x5c63d0 à 0x5c644f) : les octets +0x69 et +0x68 du type : les deux, 1 ;
// +0x69 seul, 2 ; +0x68 seul, 0 ; aucun, le mot de pile que la fonction a poussé en entrant (ecx), indéterminé : l'outil rend 3.
static int
WeaponGetType(lua_State *L)
{
	Journal("WeaponGetType", L, 1);
	const CTypeArme &t = armes.Type((int32)luaL_optnumber(L, 1, -1));
	lua_pushnumber(L, t.tir69 ? (t.tir68 ? 1 : 2) : t.tir68 ? 0 : 3);
	return 1;
}

// IsItemAWeapon(id) (0x5be3b0) : l'identifiant dans la plage des armes (0xa136b0 … 0xa136b4).
static int
IsItemAWeapon(lua_State *L)
{
	Journal("IsItemAWeapon", L, 1);
	int32 id = (int32)luaL_optnumber(L, 1, -1);
	lua_pushboolean(L, modeles.armes.count(id) != 0);
	return 1;
}

// PedSetWeapon(ped, arme, munitions[, b]) (0x5c9390) : le piéton (0 le joueur, DAT_00c1aea8 ; sinon FUN_005c7220), rien sans lui ; b
// vrai par défaut (FUN_005d8f20 avec quatre arguments). -1 : l'arme voulue (+0x18 du composant +0x2e0) passe à -1, et pour le
// joueur au HUD (FUN_0055c6f0). Sinon, avec un type (WeaponGetType) : ses munitions (+0x84, FUN_00728150) ; si elles sont une autre
// arme, l'inventaire reçoit l'arme avec 1 s'il n'en a pas (FUN_0045cee0), puis c'est elles qui prennent le compte ; ce qu'il en
// avait est retiré (FUN_0045cf70 de FUN_0045cdf0), puis le compte donné ajouté (FUN_0045cee0) ; l'arme voulue devient l'arme (et
// pour le joueur, le HUD). L'arme passe en main par l'arbre (Weapons/SelectActions : PropGetDesired, PropAttach).
static void
PoserArmeVoulue(int h, int32 arme, int32 n)
{
	CScriptPed *p = pietons.Acceder(h);
	if(p == nil) return;
	if(arme == -1){
		p->armeVoulue = -1;
		if(p->type == SCRIPT_PED_TYPE_JOUEUR) PoserArmeHud(-1);
		return;
	}
	int32 munition = armes.Type(arme).munition, compte = arme;
	if(munition != -1){
		compte = munition;
		if(munition != arme && inventaire.Munitions(armes, h, arme) == 0) inventaire.Changer(armes, h, arme, 1);
	}
	inventaire.Retirer(armes, h, compte, inventaire.Munitions(armes, h, compte));
	inventaire.Changer(armes, h, compte, (int16)n);
	p->armeVoulue = arme;
	if(p->type == SCRIPT_PED_TYPE_JOUEUR) PoserArmeHud(arme);
}

static int
PedSetWeapon(lua_State *L)
{
	Journal("PedSetWeapon", L, 1);
	PoserArmeVoulue(lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1, (int32)luaL_optnumber(L, 2, -1), (int32)luaL_optnumber(L, 3, 0));
	return 0;
}

// PedSetWeaponNow(ped, arme, munitions[, b]) (0x5c9500) : le drapeau 29 du piéton (+0x1388) à 1, PedSetWeapon, puis le nœud
// /Global/Weapons/SelectActions (FUN_005f59b0) joué tout de suite par son contrôleur d'actions (l'emplacement +0x4c, la descente,
// sinon FUN_005f4fc0 ; FUN_005f4490 puis FUN_005f49a0) : la banque ForceDesiredWeapon (si PedFlags 29) range l'arme tenue et prend
// la voulue (PropDetach, PropGetDesired, PropAttach, SetPedFlags 29 à 0).
static int
PedSetWeaponNow(lua_State *L)
{
	Journal("PedSetWeaponNow", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	CScriptPed *p = pietons.Acceder(h);
	if(p == nil) return 0;
	p->drapeaux[29] = 1;
	PoserArmeVoulue(h, (int32)luaL_optnumber(L, 2, -1), (int32)luaL_optnumber(L, 3, 0));
	const CActionNode *n = arbres.Trouver("/Global/Weapons/SelectActions");
	if(n){
		CActionController &c = controleurs[h];
		float t = maintenant / 1000.0f;
		if(!c.Entrer(arbres, n, CJugeOutil(h, t), t)) c.Jouer(arbres, n, t);
		AppliquerArmes(h, c);
	}
	return 0;
}

// PlayerSetWeapon(arme, munitions[, b]) (0x5d18c0) : sans munitions de l'arme (FUN_0045cdf0), FUN_0045cee0 les ajoute ; sinon
// FUN_0045d0f0 les pose (Poser, l'entrée libérée sous 1) ; l'arme voulue du joueur et le HUD prennent l'arme.
static int
PlayerSetWeapon(lua_State *L)
{
	Journal("PlayerSetWeapon", L, 1);
	int32 a = (int32)luaL_optnumber(L, 1, -1), n = (int16)luaL_optnumber(L, 2, 0);
	if(inventaire.Munitions(armes, SCRIPT_PED_JOUEUR, a) == 0) inventaire.Changer(armes, SCRIPT_PED_JOUEUR, a, n);
	else{
		int i = inventaire.Trouver(SCRIPT_PED_JOUEUR, a);
		if(i != -1){ inventaire.Poser(armes, i, n); if(inventaire.entrees[i].munitions < 1) inventaire.Liberer(i); }
	}
	pietons.Acceder(SCRIPT_PED_JOUEUR)->armeVoulue = a;
	PoserArmeHud(a);
	return 0;
}

// PedClearWeapon(ped, arme) (0x5c95c0) : FUN_0045cf70(arme, 1000) ; si le piéton la tient, son arme voulue passe à -1.
static int
PedClearWeapon(lua_State *L)
{
	Journal("PedClearWeapon", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	CScriptPed *p = pietons.Acceder(h);
	if(p == nil) return 0;
	int32 a = (int32)luaL_optnumber(L, 2, -1);
	inventaire.Retirer(armes, h, a, 1000, p->type == SCRIPT_PED_TYPE_JOUEUR, modeles.Index("RBandBall"));
	if(p->armeTenue == a) p->armeVoulue = -1;
	return 0;
}

// L'objet arme remis (FUN_004d92c0 : plus de modèle) et l'arbre du piéton rendu si l'arme l'avait changé (+0x33c ; FUN_005f3f40
// puis FUN_005f36d0 sur +0x4b0).
static void
LacherArme(int h)
{
	CScriptPed *p = pietons.Acceder(h);
	if(p == nil) return;
	p->armeTenue = -1;
	p->armeInit = -1.0f;
	RendreArbreAvantArme(h);
}

// PedClearAllWeapons(ped) (0x5c9720) : 0 le joueur (FUN_005c7220(h, 3 − (h ≠ 0))) ; l'objet arme remis, l'arbre rendu, l'inventaire
// vidé (FUN_0045d040), l'arme voulue et l'arme en passage à -1.
static int
PedClearAllWeapons(lua_State *L)
{
	Journal("PedClearAllWeapons", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	CScriptPed *p = pietons.Acceder(h);
	if(p == nil) return 0;
	LacherArme(h);
	inventaire.Vider(h);
	p->armeVoulue = -1; p->armePassage = -1;
	return 0;
}

// PedDestroyWeapon(ped, arme) (0x5c9630) : FUN_0045cf70(arme, 1000) ; si le piéton la tient, l'objet arme remis, l'arbre rendu,
// l'arme en passage et l'arme voulue à -1 ; pour le joueur dont le HUD la montre, le HUD à -1 (et PlayerUnequip, non suivi).
static int
PedDestroyWeapon(lua_State *L)
{
	Journal("PedDestroyWeapon", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	CScriptPed *p = pietons.Acceder(h);
	if(p == nil) return 0;
	int32 a = (int32)luaL_optnumber(L, 2, -1);
	inventaire.Retirer(armes, h, a, 1000, p->type == SCRIPT_PED_TYPE_JOUEUR, modeles.Index("RBandBall"));
	if(p->armeTenue == a){ LacherArme(h); p->armePassage = -1; p->armeVoulue = -1; }
	if(p->type == SCRIPT_PED_TYPE_JOUEUR && armeHud == a) PoserArmeHud(-1);
	return 0;
}

// PedGetWeapon(ped) (0x5ce070) : 0 le joueur ; l'arme en main (+0x10e de +0x1d0), -1 sans piéton.
static int
PedGetWeapon(lua_State *L)
{
	Journal("PedGetWeapon", L, 1);
	const CScriptPed *p = PedArg(L, 1);
	lua_pushnumber(L, p ? p->armeTenue : -1);
	return 1;
}

// PedHasWeapon(ped, arme) (0x5cd160) : le piéton tient cette arme. PlayerHasWeapon(arme) (0x5d1d70) : le joueur la tient.
// WeaponEquipped([arme]) (0x5be410) : avec un argument, le joueur la tient ; sans, il tient une arme (+0x10e ≠ -1).
static int
PedHasWeapon(lua_State *L)
{
	appels["PedHasWeapon"]++;
	const CScriptPed *p = PedArg(L, 1);
	lua_pushboolean(L, p && p->armeTenue == (int32)luaL_optnumber(L, 2, -2));
	return 1;
}

// PlayerUnequip() (0x5be400 → FUN_0055c780 sur le HUD des armes, DAT_00c674f0 ; lu à l'assembleur de 0x55c780 à 0x55c7db) : l'arme
// du HUD (+0xc0) à -1 ; FUN_0045cdf0(-1) sur l'inventaire du joueur (+0x1c4 ; son résultat n'est pas lu) ; si le joueur tient une
// arme (le modèle +0x10e de son objet arme +0x1d0 n'est pas -1), l'arme voulue (+0x18 du composant +0x2e0) passe à -1, les mains
// nues ; FUN_0055c6f0(-1) (PoserArmeHud : l'arme du HUD, +0xc4 à -1, la recherche de l'arme dans la liste du HUD) ; l'échéance de
// l'affichage du HUD (+0x140) à l'horloge (DAT_00c1a9b4) plus 4 s, que l'outil ne dessine pas. Ne rend rien. C'est le contrôleur
// du joueur qui range ensuite l'arme (Weapons/SelectActions, la condition WeaponDesired).
static int
PlayerUnequip(lua_State *L)
{
	Journal("PlayerUnequip", L, 1);
	CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	armeHud = -1;
	if(j && j->armeTenue != -1) j->armeVoulue = -1;
	PoserArmeHud(-1);
	return 0;
}

static int
PlayerHasWeapon(lua_State *L)
{
	appels["PlayerHasWeapon"]++;
	lua_pushboolean(L, pietons.Acceder(SCRIPT_PED_JOUEUR)->armeTenue == (int32)luaL_optnumber(L, 1, -2));
	return 1;
}

static int
WeaponEquipped(lua_State *L)
{
	appels["WeaponEquipped"]++;
	int32 a = pietons.Acceder(SCRIPT_PED_JOUEUR)->armeTenue;
	lua_pushboolean(L, lua_gettop(L) == 1 ? a == (int32)luaL_optnumber(L, 1, -2) : a != -1);
	return 1;
}

// ObjectNumProjectileImpacts(déclencheur, arme) (0x5c2ed0) : le déclencheur du pool (DAT_020c7c48, nul s'il est négatif ou libre) ; la
// fiche de projectile de l'arme (son type +0x80, FUN_00728150) et ses deux marques (+0x150 et +0x152, FUN_007282e0 : celles des noms
// +0x54 et +0x58) ; pour chacune, FUN_00560f10 compte les places de la table des marques de ce genre dont la position est dans le
// déclencheur (FUN_006d6e50, le test de PedIsInTrigger). Sans fiche, 0.
static int
ObjectNumProjectileImpacts(lua_State *L)
{
	appels["ObjectNumProjectileImpacts"]++;
	int h = (int)luaL_optnumber(L, 1, -1);
	int32 a = (int32)luaL_optnumber(L, 2, -1);
	int n = 0;
	auto it = typesProjectiles.find(a);
	if(it != typesProjectiles.end() && h >= 0)
		for(const CMarque &m : marques) if(m.nom == it->second.marque && donnees.DansDeclencheur(h, m.pos)) n++;
	lua_pushnumber(L, n);
	return 1;
}

// AreaClearAllProjectiles([genre]) (0x5b7100) : sans argument, les 32 projectiles détruits (FUN_004d4940 → FUN_004d3dc0) ; avec
// un, FUN_004d48a0 (non lu) : l'outil les détruit tous aussi.
static int
AreaClearAllProjectiles(lua_State *L)
{
	Journal("AreaClearAllProjectiles", L, 1);
	projectiles.clear();
	return 0;
}

// PedGetLastHitWeapon(ped) (0x5cd500 → PedGetLastHitWeapon 0x474a90, le relevé +0x408 du composant +0x2e0) : l'arme du dernier coup
// reçu (+0x5e) ; 0 est le joueur (FUN_005c7220(h, 3 − (h ≠ 0))). -1 sans coup (supposé : la remise du relevé n'est pas lue).
static int
PedGetLastHitWeapon(lua_State *L)
{
	appels["PedGetLastHitWeapon"]++;
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	auto r = coups.find(h);
	lua_pushnumber(L, r == coups.end() ? -1 : r->second.arme);
	return 1;
}

// PedGetWhoHitMeLast(ped) (0x5c8c60) : l'attaquant du relevé du composant d'actions (l'emplacement +0x40, son +0x10, +4) ; s'il en a un,
// sa poignée (l'emplacement +0xac) ; sans piéton ou sans attaquant, rien n'est rendu (nil).
static int
PedGetWhoHitMeLast(lua_State *L)
{
	appels["PedGetWhoHitMeLast"]++;
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	auto r = coups.find(h);
	if(pietons.Acceder(h) == nil || r == coups.end() || r->second.attaquant < 0) return 0;
	lua_pushnumber(L, r->second.attaquant);
	return 1;
}

// PedIsHit(ped, genre, ms) (0x5c8a40, lu à l'assembleur de 0x5c8a40 à 0x5c8ae4) : rien de vrai sans piéton (FUN_005c7220) ;
// le deuxième argument est lu (FUN_005d8200) et jeté ; le troisième, un mot (movzx), divisé par 1000 (le double 0x900138) ;
// l'emplacement 16 de CPed (FUN_00474a80 : le composant d'actions +0x2e0, +0x300), son relevé +0x10 et le temps depuis le
// dernier coup (+0x2c, -1 sans coup : le flottant 0x9009ac) : vrai s'il ne vaut pas -1 et ne dépasse pas les secondes données.
// Rend toujours un booléen (FUN_005d8450). 1_05 le juge à chaque image sur ses bullies (F_HitByPlayer, la fonction 51 de
// 1_05.lur, puis PedGetWhoHitMeLast).
static int
PedIsHit(lua_State *L)
{
	Journal("PedIsHit", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	float limite = (float)((uint16)(int)luaL_optnumber(L, 3, 0)) / 1000.0f;
	auto r = coups.find(h);
	bool touche = false;
	if(pietons.Acceder(h) && r != coups.end() && r->second.instant >= 0.0f){
		float depuis = maintenant / 1000.0f - r->second.instant;
		touche = depuis <= limite;
	}
	lua_pushboolean(L, touche);
	return 1;
}

// PedRecruitAlly(meneur, allié[, b]) (0x5cd550, lu à l'assembleur de 0x5cd550 à 0x5cd5cb) : les deux piétons (FUN_005c7220 ;
// le meneur peut manquer), FUN_00485fa0(meneur + 0x2f0, allié, 1) (CScriptPedPool::RecruterAllie) ; avec un troisième argument
// nombre (lua_type 3, FUN_0073ad50), son booléen (FUN_005d8f20) dans +0x14ec des deux. Ne rend rien.
// PedHasAlly(ped) (0x5cd730) et PedIsModel(ped, modèle) (0x5c92a0) : CScriptPedPool::AUnAllie et EstDuModele ; un booléen, faux sans
// piéton.
static int
PedHasAlly(lua_State *L)
{
	Journal("PedHasAlly", L, 1);
	lua_pushboolean(L, lua_isnumber(L, 1) && pietons.AUnAllie((int)lua_tonumber(L, 1)));
	return 1;
}

static int
PedIsModel(lua_State *L)
{
	Journal("PedIsModel", L, 1);
	lua_pushboolean(L, lua_isnumber(L, 1) && pietons.EstDuModele((int)lua_tonumber(L, 1), (int32)luaL_optnumber(L, 2, 0)));
	return 1;
}

static int
PedRecruitAlly(lua_State *L)
{
	Journal("PedRecruitAlly", L, 1);
	CScriptPed *m = PedArg(L, 1), *a = PedArg(L, 2);
	if(m == nil) return 0;
	pietons.RecruterAllie((int)lua_tonumber(L, 1), a ? (int)lua_tonumber(L, 2) : -1, true);
	if(lua_type(L, 3) == LUA_TNUMBER && a){ int32 b = (int)lua_tonumber(L, 3) != 0; m->groupe14ec = b; a->groupe14ec = b; }   // FUN_005d8f20
	return 0;
}

// PedDismissAlly(meneur, allié[, b]) (0x5cd5d0, lu à l'assembleur de 0x5cd5d0 à 0x5cd670) : les deux piétons, et s'ils sont là
// FUN_00485880(meneur + 0x2f0, allié) (CScriptPedPool::RenvoyerAllie) ; puis +0x14ec de chacun des deux qui existe : le booléen
// du troisième argument s'il est un nombre (FUN_005d8f20), 0 sinon.
static int
PedDismissAlly(lua_State *L)
{
	Journal("PedDismissAlly", L, 1);
	CScriptPed *m = PedArg(L, 1), *a = PedArg(L, 2);
	if(m && a) pietons.RenvoyerAllie((int)lua_tonumber(L, 1), (int)lua_tonumber(L, 2));
	int32 b = lua_type(L, 3) == LUA_TNUMBER && (int)lua_tonumber(L, 3) != 0;   // FUN_005d8f20 : un nombre tronqué non nul
	if(m) m->groupe14ec = b;
	if(a) a->groupe14ec = b;
	return 0;
}

// PedSetMissionCritical(ped, b[, rappel[, b2]]) (0x5cca10, lu à l'assembleur de 0x5cca10 à 0x5cca85) : rien sans piéton
// (FUN_005c7220) ; l'octet +0x141c reçoit le booléen du deuxième argument (FUN_005d8f20), le rappel du troisième est rangé en
// +0x18e8 (FUN_005d9f00, une fonction ou rien), +0x1420 le quatrième (FUN_005d8fa0, faux par défaut) ; non critique, le rappel est
// lâché (FUN_005d9af0). Ne rend rien.
static std::map<int, int> rappelsCritiques;   // le +0x18e8 de chaque piéton : une référence du registre Lua
static int
PedSetMissionCritical(lua_State *L)
{
	Journal("PedSetMissionCritical", L, 1);
	CScriptPed *p = PedArg(L, 1);
	if(p == nil) return 0;
	int h = (int)lua_tonumber(L, 1);
	p->critique = BooleenArg(L, 2);
	int &ref = rappelsCritiques.emplace(h, LUA_NOREF).first->second;
	Oublier(L, ref);
	if(lua_isfunction(L, 3)){ lua_pushvalue(L, 3); ref = luaL_ref(L, LUA_REGISTRYINDEX); }
	p->rappelCritique = ref != LUA_NOREF;
	p->critique1420 = lua_isnone(L, 4) || lua_isnil(L, 4) ? false : BooleenArg(L, 4);
	if(!p->critique){ Oublier(L, ref); p->rappelCritique = false; }
	return 0;
}

// Les rappels des piétons critiques tombés à 0 (CScriptPedPool::critiquesAZero, FUN_004782a0) : appelés avec la poignée du
// piéton, d'un bloc (lua_pcall) dans le script qui les a posés, puis lâchés.
static void
AppelerCritiques(lua_State *L)
{
	std::vector<int> v;
	v.swap(pietons.critiquesAZero);
	for(int h : v){
		auto it = rappelsCritiques.find(h);
		if(it == rappelsCritiques.end() || it->second == LUA_NOREF) continue;
		printf("%6.2f s  le piéton critique %d tombe à 0 point : son rappel est appelé\n", maintenant / 1000.0, h);
		lua_rawgeti(L, LUA_REGISTRYINDEX, it->second);
		lua_pushnumber(L, h);
		if(lua_pcall(L, 1, 0, 0) != 0){ printf("        rappel du piéton critique : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
		Oublier(L, it->second);
	}
}

// PedFindInAreaXYZ(x, y, z, r) (0x5d0960, lu à l'assembleur de 0x5d0960 à 0x5d0a2b) : FUN_005cf850(liste, x, y, z, r, r, r) parcourt
// le pool des piétons dans l'ordre de ses cases ; un piéton sans l'octet +0x78 ni +0x1338 (non suivis : supposés nuls) dont la
// position est dans le carré [x − r, x + r] × [y − r, y + r] (FUN_0046d880, supposé sur la position) est gardé ; le test de la
// hauteur est écrit « z − r <= pz ou pz <= z + r », toujours vrai. Rend le booléen « au moins un », puis la poignée de chacun
// (l'emplacement 43, +0xac) ; la liste est libérée (FUN_005ee940). T_BathroomAntics (la fonction 63 de 1_05.lur) en tire
// bBathroomClear : personne d'autre que le joueur, Algie ou un mort.
static int
PedFindInAreaXYZ(lua_State *L)
{
	Journal("PedFindInAreaXYZ", L, 1);
	float x = (float)luaL_optnumber(L, 1, 0), y = (float)luaL_optnumber(L, 2, 0), r = (float)luaL_optnumber(L, 4, 0);
	std::vector<int> trouves;
	for(int h = 0; h < SCRIPT_PED_POOL_SIZE; h++){
		const CScriptPed *p = pietons.Acceder(h);
		if(p && fabsf(p->pos.x - x) <= r && fabsf(p->pos.y - y) <= r) trouves.push_back(h);
	}
	lua_pushboolean(L, !trouves.empty());
	for(int h : trouves) lua_pushnumber(L, h);
	return (int)trouves.size() + 1;
}

// PedFindInAreaObject(ped, r) (0x5d0a30, lue à l'assembleur de 0x5d0a30 à 0x5d0b1e) : le piéton (FUN_005c7220, le genre 2) ; sans
// lui, rien n'est rendu ; sinon sa position (FUN_005c2810(&pos, 2, poignée)) et la même recherche que PedFindInAreaXYZ
// (FUN_005cf850(liste, x, y, z, r, r, r)) : le booléen « au moins un », puis la poignée de chacun, le piéton lui-même compris.
static int
PedFindInAreaObject(lua_State *L)
{
	Journal("PedFindInAreaObject", L, 1);
	const CScriptPed *c = lua_isnumber(L, 1) ? pietons.Acceder((int)lua_tonumber(L, 1)) : nil;
	if(c == nil) return 0;
	float r = (float)luaL_optnumber(L, 2, 0);
	std::vector<int> trouves;
	for(int h = 0; h < SCRIPT_PED_POOL_SIZE; h++){
		const CScriptPed *p = pietons.Acceder(h);
		if(p && fabsf(p->pos.x - c->pos.x) <= r && fabsf(p->pos.y - c->pos.y) <= r) trouves.push_back(h);
	}
	lua_pushboolean(L, !trouves.empty());
	for(int h : trouves) lua_pushnumber(L, h);
	return (int)trouves.size() + 1;
}

// PlayerWeaponHudLock(b) (0x5d1b80 → PlayerWeaponHudLock 0x55bb20) : l'octet +0x13c du HUD des armes, que lit WeaponHudLocked.
static int
PlayerWeaponHudLock(lua_State *L)
{
	Journal("PlayerWeaponHudLock", L, 1);
	hudArmesVerrouille = lua_toboolean(L, 1) != 0;
	return 0;
}

// PlayerGetPhysicalState() (0x5d1ee0) : DAT_00c1af4c.
static int
PlayerGetPhysicalState(lua_State *L)
{
	lua_pushnumber(L, etatPhysiqueJoueur);
	return 1;
}

// ChapterSet(n) (0x5c0b70 → FUN_00454470) : le chapitre ; si la saison change, le moteur change les modèles de saison
// (PedSetSeasonalModels) et le temps (FUN_00534780), que l'outil n'a pas. ChapterGet() (0x5c0ba0) le rend.
static int
ChapterSet(lua_State *L)
{
	Journal("ChapterSet", L, 1);
	chapitre = (uint8)luaL_checknumber(L, 1);
	return 0;
}

static int
ChapterGet(lua_State *L)
{
	lua_pushnumber(L, chapitre);
	return 1;
}

// Les composants du HUD (src/hud/HudComponents.h). ToggleHUDComponentVisibility(n, oui) (0x5bd500 : l'emplacement 5 du
// composant n), ToggleHUDComponentLocked(n, oui) (0x5bd560 : l'octet +0x15), HUDComponentGetVisibility(n) (0x5bd590 :
// l'emplacement 4), HUDComponentGetLocked (0x5bd5f0), HUDComponentGetFlashing (0x5bd5c0), HUDClearAllElements
// (0x5bdb80), HUDSaveVisibility (0x5bdb70) et HUDRestoreVisibility (0x5bdba0). Le booléen des scripts passe par
// FUN_005d8f20 : un nombre tronqué non nul, sinon lua_toboolean.
static bool
BooleenArg(lua_State *L, int i)
{
	return lua_isnumber(L, i) ? (int)lua_tonumber(L, i) != 0 : lua_toboolean(L, i) != 0;
}

static int
ToggleHUDComponentVisibility(lua_State *L)
{
	Journal("ToggleHUDComponentVisibility", L, 1);
	hud.Basculer((int)luaL_checknumber(L, 1), BooleenArg(L, 2));
	return 0;
}

static int
ToggleHUDComponentLocked(lua_State *L)
{
	hud.Verrouiller((int)luaL_checknumber(L, 1), BooleenArg(L, 2));
	return 0;
}

static int
HUDComponentGetVisibility(lua_State *L)
{
	lua_pushboolean(L, hud.Visible((int)luaL_checknumber(L, 1)));
	return 1;
}

static int
HUDComponentGetLocked(lua_State *L)
{
	lua_pushboolean(L, hud.Verrouille((int)luaL_checknumber(L, 1)));
	return 1;
}

static int
HUDComponentGetFlashing(lua_State *L)
{
	lua_pushboolean(L, hud.Clignote((int)luaL_checknumber(L, 1)));
	return 1;
}

static int
HUDClearAllElements(lua_State *L)
{
	hud.ToutCacher();
	return 0;
}

static int
HUDSaveVisibility(lua_State *L)
{
	hud.Sauver();
	return 0;
}

static int
HUDRestoreVisibility(lua_State *L)
{
	hud.Restaurer();
	return 0;
}

// Le gestionnaire de caméras (src/render/CameraMgr.h). CameraGetActive() (0x5b9bb0) : le mode courant. CameraSetActive(mode,
// durée, octet) (0x5b9b30) : une demande (FUN_004f2ad0), que seules les pistes de caméra appliquent (FUN_004f1740) ;
// l'octet vaut 1 sans troisième argument. CameraReturnToPlayer(octet) (0x5b8fd0) : la demande du mode 1 (l'octet vrai par
// défaut ; avec un second argument vrai et le point de reprise, le moteur replace aussi la caméra, non suivi).
// CameraSetWidescreen(oui, progressif) (0x5b8f20).
static int
CameraGetActive(lua_State *L)
{
	lua_pushnumber(L, cameras.courant);
	return 1;
}

static int
CameraSetActive(lua_State *L)
{
	Journal("CameraSetActive", L, 1);
	cameras.Demander((int32)luaL_checknumber(L, 1), 0, lua_gettop(L) > 2 ? BooleenArg(L, 3) : true);
	return 0;
}

static int
CameraReturnToPlayer(lua_State *L)
{
	Journal("CameraReturnToPlayer", L, 1);
	cameras.Demander(CAMERA_JOUEUR, 0, lua_gettop(L) > 0 ? BooleenArg(L, 1) : true);
	return 0;
}

static int
CameraSetWidescreen(lua_State *L)
{
	Journal("CameraSetWidescreen", L, 1);
	cameras.LargeEcran(BooleenArg(L, 1), lua_gettop(L) == 2 && !BooleenArg(L, 2));
	return 0;
}

// PedSetControllerID(ped, n) (FUN_005c9fc0) : le piéton mené par la manette n (0 : DAT_00c2b120, 1 : DAT_00c2b124).
static int
PedSetControllerID(lua_State *L)
{
	Journal("PedSetControllerID", L, 1);
	pietons.PoserManette((int)luaL_checknumber(L, 1), (int)luaL_checknumber(L, 2));
	return 0;
}

// La garde-robe du joueur (src/peds/Clothing, 0x20c5f70). ClothingOverlay(oui) (0x5baf70) l'ouvre ou la ferme ;
// ClothingDefineOutfit(nom, second, six pièces, b, b) (0x5ba900 → FUN_006cbf70) y définit une tenue ;
// ClothingSetPlayerOutfit(nom ou nombre[, pièce à pièce]) (0x5baa40 → FUN_006cb5c0) la met ; ClothingIsWearingOutfit(nom)
// (0x5bac40 → FUN_006ca890) et ClothingIsWearingAnyOutfit() (0x5bac80) le disent. Les noms passent par HashString.
static CClothingManager gardeRobe;
static int
ClothingOverlay(lua_State *L)
{
	Journal("ClothingOverlay", L, 1);
	gardeRobe.Ouvrir(BooleenArg(L, 1));
	return 0;
}

static int
ClothingDefineOutfit(lua_State *L)
{
	Journal("ClothingDefineOutfit", L, 1);
	auto nom = [&](int i){ return lua_isstring(L, i) ? ActionHash(lua_tostring(L, i)) : 0u; };
	uint32 pieces[GARDE_ROBE_PIECES];
	for(int k = 0; k < GARDE_ROBE_PIECES; k++) pieces[k] = nom(3 + k);
	gardeRobe.Definir(nom(1), nom(2), pieces, BooleenArg(L, 10));
	return 0;
}

static int
ClothingSetPlayerOutfit(lua_State *L)
{
	Journal("ClothingSetPlayerOutfit", L, 1);
	uint32 h = lua_type(L, 1) == LUA_TNUMBER ? (uint32)lua_tonumber(L, 1) : lua_isstring(L, 1) ? ActionHash(lua_tostring(L, 1)) : 0u;
	gardeRobe.Mettre(h, lua_gettop(L) >= 2 && BooleenArg(L, 2));
	return 0;
}

static int
ClothingIsWearingOutfit(lua_State *L)
{
	appels["ClothingIsWearingOutfit"]++;
	lua_pushboolean(L, lua_isstring(L, 1) && gardeRobe.Porte(ActionHash(lua_tostring(L, 1))));
	return 1;
}

static int
ClothingIsWearingAnyOutfit(lua_State *L)
{
	appels["ClothingIsWearingAnyOutfit"]++;
	lua_pushboolean(L, gardeRobe.PorteUneTenue());
	return 1;
}

static int ConditionTenue(uint32 nom){ return gardeRobe.Porte(nom); }

// PedSetAsleep(ped, oui) (0x5caa30) : +0x1358 du piéton ; PedSetGlobalSleep(oui) (0x5c6e40) : DAT_00c2b108. L'un ou
// l'autre arrête l'arbre de l'IA (FUN_00484490).
static int
PedSetAsleep(lua_State *L)
{
	Journal("PedSetAsleep", L, 1);
	if(CScriptPed *p = pietons.Acceder((int)luaL_checknumber(L, 1))) p->endormi = BooleenArg(L, 2);
	return 0;
}

static int
PedSetGlobalSleep(lua_State *L)
{
	Journal("PedSetGlobalSleep", L, 1);
	pietons.sommeilGeneral = BooleenArg(L, 1);
	return 0;
}

// PedSetFaction(ped, type) (0x5cb570) : le type du piéton (+0x1310), sauf 13 (PLAYER1), que le moteur refuse.
static int
PedSetFaction(lua_State *L)
{
	Journal("PedSetFaction", L, 1);
	CScriptPed *p = pietons.Acceder((int)luaL_checknumber(L, 1));
	int32 t = (int32)luaL_checknumber(L, 2);
	if(p && t != SCRIPT_PED_TYPE_JOUEUR) p->type = t;
	return 0;
}

// PedSetIsStealthMissionPed(ped, oui) (0x5d0110 → FUN_005d0080) : +0x1398 ; le moteur prévient aussi la mémoire des
// stimuli (FUN_0054a740, FUN_0054a460), que l'outil n'a pas.
static int
PedSetIsStealthMissionPed(lua_State *L)
{
	Journal("PedSetIsStealthMissionPed", L, 1);
	CScriptPed *p = pietons.Acceder((int)luaL_checknumber(L, 1));
	// FUN_005d8fa0(L, 1, 1) : un booléen ou un nombre, sinon vrai par défaut.
	if(p) p->discretion = lua_isboolean(L, 2) || lua_isnumber(L, 2) ? (lua_isboolean(L, 2) ? lua_toboolean(L, 2) != 0 : lua_tonumber(L, 2) != 0) : true;
	return 0;
}

// PedGetFaction(ped) (0x5cb5c0) : son type, -1 pour un handle sans piéton.
static int
PedGetFaction(lua_State *L)
{
	const CScriptPed *p = pietons.Acceder((int)luaL_checknumber(L, 1));
	lua_pushnumber(L, p ? p->type : -1);
	return 1;
}

// PedCreatePoint(modèle, liste, n) (0x5cf9d0) : au n-ième point de la liste, tourné selon son lacet.
static int
PedCreatePoint(lua_State *L)
{
	Journal("PedCreatePoint", L, 1);
	int32 modele = (int32)luaL_checknumber(L, 1);
	const CTriggerPointPoint *pt = PointArg(L, "PedCreatePoint", 2, 3);
	if(pt == nil){ lua_pushnumber(L, -1); return 1; }
	return CreerPed(L, "PedCreatePoint", modele, pt->pos, pt->lacetTangageRoulis[0]);
}

// PedCreateXYZ(modèle, x, y, z) (0x5cfb50).
static int
PedCreateXYZ(lua_State *L)
{
	Journal("PedCreateXYZ", L, 1);
	CVector pos((float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
	return CreerPed(L, "PedCreateXYZ", (int32)luaL_checknumber(L, 1), pos, 0);
}

static int
PedDelete(lua_State *L)
{
	Journal("PedDelete", L, 1);
	if(lua_isnumber(L, 1) && pietons.Supprimer((int)lua_tonumber(L, 1))) OublierControleurs((int)lua_tonumber(L, 1));
	return 0;
}

static int
PedIsValid(lua_State *L)
{
	appels["PedIsValid"]++;
	lua_pushboolean(L, lua_isnumber(L, 1) && pietons.Valide((int)lua_tonumber(L, 1)));
	return 1;
}

static int
PedIsPlayer(lua_State *L)
{
	appels["PedIsPlayer"]++;
	lua_pushboolean(L, lua_isnumber(L, 1) && (int)lua_tonumber(L, 1) == SCRIPT_PED_JOUEUR);
	return 1;
}

static int
PlayerGetPedIndex(lua_State *L)
{
	appels["PlayerGetPedIndex"]++;
	lua_pushnumber(L, SCRIPT_PED_JOUEUR);
	return 1;
}

static int
PedGetPosXYZ(lua_State *L)
{
	appels["PedGetPosXYZ"]++;
	const CScriptPed *p = PedArg(L, 1);
	lua_pushnumber(L, p ? p->pos.x : 0); lua_pushnumber(L, p ? p->pos.y : 0); lua_pushnumber(L, p ? p->pos.z : 0);
	return 3;
}

static int
PedSetPosXYZ(lua_State *L)
{
	Journal("PedSetPosXYZ", L, 1);
	CScriptPed *p = PedArg(L, 1);
	if(p) p->pos = CVector((float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
	return 0;
}

// PedGetOffsetInWorldCoords(ped, dx, dy, dz) (0x5ce500, lu à l'assembleur de 0x5ce500 à 0x5ce60e) : sans nombre en premier
// argument (FUN_005d81c0) ou sans piéton (FUN_005c7220), (0, 0, 0) ; sinon le décalage (FUN_005d82a0 × 3) tourné par la matrice
// du piéton (FUN_008b5f90 : son +0x14 ; FUN_00414560 : dx × la droite + dy × l'avant + dz × le haut, les colonnes +0, +0x10,
// +0x20) plus sa position (+0x30 de la matrice, +4 sans elle). Trois nombres. Dans l'outil, le piéton n'a qu'un cap : l'avant
// (cos cap, sin cap, 0), la droite (sin cap, −cos cap, 0), le haut (0, 0, 1).
static int
PedGetOffsetInWorldCoords(lua_State *L)
{
	Journal("PedGetOffsetInWorldCoords", L, 1);
	const CScriptPed *p = PedArg(L, 1);
	float x = 0, y = 0, z = 0;
	if(p){
		float dx = (float)luaL_optnumber(L, 2, 0), dy = (float)luaL_optnumber(L, 3, 0), dz = (float)luaL_optnumber(L, 4, 0);
		float a = p->cap * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a);
		x = p->pos.x + dx * s + dy * c; y = p->pos.y - dx * c + dy * s; z = p->pos.z + dz;
	}
	lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
	return 3;
}

// PedSetStationary(ped, b) (0x5c7950) : l'octet +0x4e4 du piéton (FUN_005d8f20), rien sans piéton. L'outil le passe à la vitesse
// de la navigation dans CombatLoco (ScriptPedVitesseDuTrajet) ; ses gestes (PedMoveToPoint…) ne le lisent pas.
static int
PedSetStationary(lua_State *L)
{
	Journal("PedSetStationary", L, 1);
	if(CScriptPed *p = PedArg(L, 1)) p->immobile = lua_toboolean(L, 2) != 0;
	return 0;
}

// CPed::SetHeading (FUN_004755e0) : le cap visé (+0x1700) et celui de l'entité (+0x10, et sa matrice), puis la direction de
// l'intention de mouvement (+0, +0xc du composant +0x2e0) tournée vers l'avant (FUN_00415350). L'outil garde le cap en degrés
// depuis +x : le moteur prend le lacet d'un point moins 90 degrés, en radians (0 vers +y), la même direction.
static void
PoserCap(CScriptPed *p, float capDegres)
{
	p->cap = capDegres;
	float a = capDegres * 3.14159265f / 180.0f;
	p->intention = CVector(cosf(a), sinf(a), 0);
}

// FUN_005c73c0 (et FUN_00470b40) : la direction à plat vers l'entité, normée (FUN_00414cc0), son angle (FUN_00432e90) moins π/2
// (_DAT_00900960), ramené dans [0, 2π[ (_DAT_009009b0), au piéton par FUN_004755e0. Rien sur place.
static void
TournerVers(CScriptPed *p, const CScriptPed *e)
{
	float dx = e->pos.x - p->pos.x, dy = e->pos.y - p->pos.y;
	if(dx * dx + dy * dy > 0.0f) PoserCap(p, atan2f(dy, dx) * 180.0f / 3.14159265f);
}

// PedSetPosPoint(ped, liste[, n]) (0x5c82c0) : le n-ième point de la liste (le premier, puis FUN_006d58a0 tant qu'il y en a :
// au-delà, le dernier), posé par FUN_005c74a0, puis le cap du lacet du point (+0x1c, (lacet − 90) × π / 180, FUN_004755e0).
// FUN_005c74a0 : sous z = -20 (_DAT_00a14a8c), z vient du sol (FUN_00463350, non suivi : pas de monde) ; le joueur (type 13)
// passe par une demande de zone dans la zone visible (FUN_00415730, DAT_00bd1008) ; un autre piéton est posé (son véhicule s'il
// en a un, +0x1554), z plus la distance du centre à la base du modèle (FUN_00465fc0 : −min.z de la boîte du modèle de
// collision, +0xc du modelinfo). Pour un piéton, c'est 0 : CTempColModels::Initialise (FUN_005712f0) bâtit le modèle des
// piétons (DAT_00cf312c, que le chargeur des .idb leur donne, FUN_0042bfd0 → FUN_0050e9e0) de trois sphères de 0,35 m
// (_DAT_0090d0a8) à 0,35, 0,95 et 1,55 m (_DAT_0091c100, _DAT_0091c0fc) et d'une boîte de z = 0 à 1,9 : l'origine d'un
// piéton est à ses pieds.
static int
PedSetPosPoint(lua_State *L)
{
	Journal("PedSetPosPoint", L, 1);
	CScriptPed *p = PedArg(L, 1);
	if(p == nil || !lua_isnumber(L, 2)) return 0;
	int h = (int)lua_tonumber(L, 2), n = lua_gettop(L) == 3 && lua_isnumber(L, 3) ? (int)lua_tonumber(L, 3) : 1;
	const CTriggerPointPoint *pt = nil;
	for(int k = n < 1 ? 1 : n; k >= 1 && pt == nil; k--) pt = donnees.Point(h, k);
	if(pt == nil) return 0;
	if(p == pietons.Acceder(SCRIPT_PED_JOUEUR)) DemanderZone("PedSetPosPoint", pt->pos, pt->lacetTangageRoulis[0], -1);
	else p->pos = pt->pos;
	PoserCap(p, pt->lacetTangageRoulis[0]);
	return 0;
}

// Les portes nommées par les scripts (CEtatPortes, l'objet 0xc3b808). AreaSetDoorLocked (0x5b7760), AreaSetDoorLockedToPeds
// (0x5b79a0), AreaSetDoorOpen (0x5b7be0), AreaIsDoorLockedToPeds (0x5b7880) et AreaIsDoorOpen (0x5b7ac0) prennent la porte de
// trois façons : une chaîne (le type 4, FUN_005d81e0) est cherchée parmi les déclencheurs (FUN_006d78c0 : HashString du nom
// contre +0x2c), sinon son HashString va droit à la table ; un pointeur léger (le type 2, FUN_005d8560 → FUN_006d7870), que
// l'outil ne suit pas ; un nombre, la poignée d'un déclencheur. Pour un déclencheur, FUN_006d6420, FUN_006d6480, FUN_006d6500,
// FUN_006d6450 et FUN_006d64d0 passent par son objet animé (+0x1c : +0x180 verrouillée aux piétons, +0x184 ouverte, l'ouverture
// FUN_004eebc0 / FUN_004eed70, le chemin des piétons FUN_005eaf00) ; sans objet, par la table sous le nom haché du déclencheur
// (+0x2c). L'outil ne crée pas les objets animés (PAnimCreate 0x5c4680 n'est pas lu) : un déclencheur passe toujours par la
// table, ce que le jeu ne fait que pour un déclencheur sans objet. AreaSetDoorPathableToPeds (0x5b7d00) ne touche que l'objet
// (FUN_004ea270, FUN_005eaf00) : rien dans l'outil.
static CEtatPortes etatPortes;

static bool
NomDePorte(lua_State *L, uint32 &nom)
{
	if(lua_type(L, 1) == LUA_TSTRING){
		// Le déclencheur trouvé par son nom a ce même nom haché en +0x2c : sans objet animé, les deux chemins mènent à la même paire.
		nom = ActionHash(lua_tostring(L, 1)) & 0x7fffffff;
		return true;
	}
	if(lua_type(L, 1) != LUA_TNUMBER) return false;
	int t = (int)lua_tonumber(L, 1);
	if(t < 0 || donnees.ZoneParHandle(t) == nil) return false;
	nom = donnees.HachageZone(t);
	return true;
}

// La porte animée i ouverte ou fermée sans piéton (CObjetsAnimes::OuvrirPorte, FermerPorte : FUN_004eebc0, FUN_004eed70) : le nœud de
// l'arbre des portes (Act/Props/Door.act, diffusé avec l'objet : supposé chargé), joué par le contrôleur de l'objet (FUN_004edec0).
static bool
PorteAnimee(int i, bool ouvrir, bool immediat)
{
	CObjetAnime *o = objetsAnimes.Objet(i);
	if(o == nil) return false;
	const char *chemin = !ouvrir ? "/Global/Door/DoorFunctions/Closing" : immediat ? "/Global/Door/NotUseable/Opened/Immediate" : "/Global/Door/DoorFunctions/Opening";
	const CActionNode *n = arbres.TrouverOuCharger(chemin, "Act/Props/Door.act") ? arbres.Trouver(chemin) : nil;
	auto it = controleursObjets.find(i);
	bool deja = it != controleursObjets.end() && it->second.joue && arbres.JoueHachage(it->second.joue, ActionHash(ouvrir ? "Opening" : "Closing") & 0x7fffffff);
	bool r = ouvrir ? objetsAnimes.OuvrirPorte(i, 0, n != nil, deja) : objetsAnimes.FermerPorte(i, n != nil, deja);
	if(r && n && !deja){
		CActionController &c = controleursObjets[i];
		c.fige = true;
		c.Jouer(arbres, n, maintenant / 1000.0f);
		if(journalRestant > 0) printf("%6.2f s  objet animé du déclencheur %d : %s (%s)\n", maintenant / 1000.0, o->declencheur, ouvrir ? "ouvert" : "fermé", chemin);
	}
	return r;
}

// Le déclencheur d'une porte nommée : une chaîne cherchée parmi les déclencheurs (FUN_006d78c0), ou un nombre ; -1 sinon.
static int
DeclencheurDePorte(lua_State *L)
{
	if(lua_type(L, 1) == LUA_TSTRING) return donnees.Declencheur(lua_tostring(L, 1));
	if(lua_type(L, 1) == LUA_TNUMBER){ int t = (int)lua_tonumber(L, 1); return t >= 0 && donnees.ZoneParHandle(t) ? t : -1; }
	return -1;
}

// Un déclencheur qui a un objet animé passe par lui (FUN_006d6420 : le drapeau 7, +0x17c, par FUN_004ea260 ; FUN_006d6480 : le
// drapeau 8, +0x180, et le chemin des piétons FUN_005eaf00, non suivi ; FUN_006d6500 : FUN_004eebc0(0, 0, 0) ou FUN_004eed70(0) ;
// FUN_006d6450 : le drapeau 8 ; FUN_006d64d0 : le drapeau 9, +0x184) ; sans objet, par la table de CEtatPortes.
static int
PoserPorte(lua_State *L, const char *quoi, int bit)
{
	Journal(quoi, L, 1);
	bool v = lua_toboolean(L, 2) != 0;   // FUN_005d8f20, lu avant la porte
	int i = objetsAnimes.ObjetDe(DeclencheurDePorte(L));
	if(i >= 0){
		if(bit == CEtatPortes::VERROUILLEE) objetsAnimes.PoserDrapeau(i, 7, v);
		else if(bit == CEtatPortes::VERROUILLEE_PIETONS) objetsAnimes.PoserDrapeau(i, 8, v);
		else PorteAnimee(i, v, false);
		return 0;
	}
	uint32 nom;
	if(NomDePorte(L, nom)) etatPortes.Poser(nom, bit, v);
	return 0;
}

static int
LirePorte(lua_State *L, const char *quoi, int bit)
{
	appels[quoi]++;
	int i = objetsAnimes.ObjetDe(DeclencheurDePorte(L));
	if(i >= 0){
		lua_pushboolean(L, objetsAnimes.Drapeau(i, bit == CEtatPortes::OUVERTE ? 9 : 8));
		return 1;
	}
	uint32 nom;
	lua_pushboolean(L, NomDePorte(L, nom) && etatPortes.Lire(nom, bit));
	return 1;
}

// PAnimOpenDoor(objet) (0x5c4b20 : FUN_005c4410(L, 0), puis FUN_004eebc0(0, 0, 0)), PAnimOpenDoorImmediate (0x5c4b50 :
// FUN_004eebc0(0, 0, 1)), PAnimCloseDoor (0x5c4b80 : FUN_004eed70(0)), PAnimDoorStayOpen (0x5c4bb0 : FUN_004ea300(0x10, 1)), lues à
// l'assembleur de 0x5c4b20 à 0x5c4bd6 ; PAnimIsOpen(déclencheur) (0x5c38b0, lue de 0x5c38b0 à 0x5c38fc : FUN_006d79c0, le +0x184
// de l'objet non nul). Rien n'est rendu, sauf par PAnimIsOpen.
static int PAnimOpenDoor(lua_State *L){ Journal("PAnimOpenDoor", L, 1); PorteAnimee(ObjetAnimeArg(L, 0), true, false); return 0; }
static int PAnimOpenDoorImmediate(lua_State *L){ Journal("PAnimOpenDoorImmediate", L, 1); PorteAnimee(ObjetAnimeArg(L, 0), true, true); return 0; }
static int PAnimCloseDoor(lua_State *L){ Journal("PAnimCloseDoor", L, 1); PorteAnimee(ObjetAnimeArg(L, 0), false, false); return 0; }
static int PAnimDoorStayOpen(lua_State *L){ Journal("PAnimDoorStayOpen", L, 1); objetsAnimes.PoserDrapeau(ObjetAnimeArg(L, 0), 16, true); return 0; }
static int
PAnimIsOpen(lua_State *L)
{
	Journal("PAnimIsOpen", L, 1);
	int i = lua_isnumber(L, 1) ? objetsAnimes.ObjetDe((int)lua_tonumber(L, 1)) : -1;
	lua_pushboolean(L, objetsAnimes.Drapeau(i, 9));
	return 1;
}

static int AreaSetDoorLocked(lua_State *L){ return PoserPorte(L, "AreaSetDoorLocked", CEtatPortes::VERROUILLEE); }
static int AreaSetDoorLockedToPeds(lua_State *L){ return PoserPorte(L, "AreaSetDoorLockedToPeds", CEtatPortes::VERROUILLEE_PIETONS); }
static int AreaSetDoorOpen(lua_State *L){ return PoserPorte(L, "AreaSetDoorOpen", CEtatPortes::OUVERTE); }
static int AreaIsDoorLockedToPeds(lua_State *L){ return LirePorte(L, "AreaIsDoorLockedToPeds", CEtatPortes::VERROUILLEE_PIETONS); }
static int AreaIsDoorOpen(lua_State *L){ return LirePorte(L, "AreaIsDoorOpen", CEtatPortes::OUVERTE); }

// PedIgnoreStimuli(ped, b) (0x5ca370, lua_PedIgnoreStimuli) : le mot +0x1370 du piéton (le drapeau 23, +0x1314 + 4 × 23) prend
// le booléen (FUN_005d8f20) ; rien s'il n'existe pas (FUN_005c7220). SetAmbientPedsIgnoreStimuli(b) (0x5bb2b0, hors de
// l'index, lu à l'assembleur de 0x5bb2b0 à 0x5bb307) le pose à tous les piétons du pool (DAT_00c0f5f0, du dernier au premier)
// que FUN_00476470 dit ambiants : pas en véhicule (+0x1554), pas le joueur, et l'octet de création +0x1570 ni 2 (un script)
// ni 3 ; dans l'outil, les piétons qu'aucun script ne tient. PlayerSetControl(1) remet celui du joueur à 0. Le mot est lu par
// FUN_004909a0 (le piéton ignore les stimuli : il vaut 1), que l'outil ne joue pas, et par PedFlags (le drapeau 23).
static int
PedIgnoreStimuli(lua_State *L)
{
	Journal("PedIgnoreStimuli", L, 1);
	if(CScriptPed *p = PedArg(L, 1)) p->drapeaux[23] = lua_toboolean(L, 2) ? 1 : 0;
	return 0;
}

static int
SetAmbientPedsIgnoreStimuli(lua_State *L)
{
	Journal("SetAmbientPedsIgnoreStimuli", L, 1);
	int v = lua_toboolean(L, 1) ? 1 : 0;
	for(int i = SCRIPT_PED_POOL_SIZE - 1; i >= 0; i--)
		if(CScriptPed *p = pietons.Acceder(i)) if(i != SCRIPT_PED_JOUEUR && p->script < 0) p->drapeaux[23] = v;
	return 0;
}

// PedSetTetherToPoint(ped, liste[, n], rayon) (0x5cc370) : rien si le piéton n'existe pas (FUN_005c7220) ou si la liste
// n'a pas de point (CPointPool__GetSlot) ; avec quatre arguments, le n-ième point, le dernier s'il en manque (FUN_006d58a0) ;
// le rayon est le dernier argument (FUN_005d82a0). PedSetTetherToXYZ (FUN_00499910, ecx = le piéton + 0x358, lu à
// l'assembleur en 0x5cc468) pose le genre 1 (+0x360), le rayon (+0x364), le point (+0x368 … +0x370), les octets +0x3b4 (1)
// et +0x3b5 (0), puis deux fois FUN_00499840, qui ne refait le point que pour les genres 2 et 3. PedClearTether (0x5c9a30 →
// FUN_004998f0) remet le genre à 0. Le piéton n'est pas encore tiré vers son point : l'outil n'a pas la locomotion de laisse
// (TetherLoco dans AI.act), seulement la condition IsTethered.
static int
PedSetTetherToPoint(lua_State *L)
{
	Journal("PedSetTetherToPoint", L, 1);
	CScriptPed *p = PedArg(L, 1);
	if(p == nil || !lua_isnumber(L, 2)) return 0;
	int h = (int)lua_tonumber(L, 2), arguments = lua_gettop(L), n = arguments == 4 ? (int)luaL_optnumber(L, 3, 1) : 1;
	const CTriggerPointPoint *pt = donnees.Point(h, 1);
	if(pt == nil) return 0;
	for(int k = 2; k <= n; k++){ const CTriggerPointPoint *q = donnees.Point(h, k); if(q == nil) break; pt = q; }
	p->laisse = 1;
	p->laisseRayon = (float)luaL_optnumber(L, arguments == 4 ? 4 : 3, 0);
	p->laissePoint = pt->pos;
	return 0;
}

// PedRestrictToTrigger(ped, déclencheur) (0x5c99e0, lu à l'assembleur de 0x5c99e0 à 0x5c9a2b) : rien sans piéton ; le déclencheur
// est un mot (movzx) ; FUN_004999e0 (ecx = le piéton + 0x358) pose le rayon +0x364 à 0, le genre 3 (+0x360), le déclencheur
// (+0x384), les octets +0x3b4 à 1 et +0x3b5 à 0, puis deux fois FUN_00499840 (le genre 3 : le point devient la position du
// piéton, ramenée dans le déclencheur par FUN_004995c0, non suivi). Dans la laisse, FUN_00499530 juge alors la position dans la zone
// du déclencheur (FUN_006d6ed0) ; l'outil ne tire pas le piéton vers elle (TetherLoco n'est pas sur ces piétons).
static int
PedRestrictToTrigger(lua_State *L)
{
	Journal("PedRestrictToTrigger", L, 1);
	CScriptPed *p = PedArg(L, 1);
	if(p == nil) return 0;
	p->laisse = 3; p->laisseRayon = 0; p->laisseDeclencheur = (uint16)(int)luaL_optnumber(L, 2, 0);
	p->laissePieton = -1; p->laisse3b4 = true; p->laisse3b5 = false; p->laissePoint = p->pos;
	return 0;
}

// PedSetInfiniteSprint(ped, b) (0x5cde60, lu à l'assembleur de 0x5cde60 à 0x5cdea1) : +0x13c8 au booléen (FUN_005d8f20) ;
// PedCanTeleportOnAreaTransition(ped, b) (0x5cd080) : l'octet +0x1c99, que FUN_0048b970 lit pour les poursuivants du joueur.
static int
PedSetInfiniteSprint(lua_State *L)
{
	Journal("PedSetInfiniteSprint", L, 1);
	if(CScriptPed *p = PedArg(L, 1)) p->sprintSansFin = BooleenArg(L, 2);
	return 0;
}

static int
PedCanTeleportOnAreaTransition(lua_State *L)
{
	Journal("PedCanTeleportOnAreaTransition", L, 1);
	if(CScriptPed *p = PedArg(L, 1)) p->teleportable = BooleenArg(L, 2);
	return 0;
}

static int
PedClearTether(lua_State *L)
{
	Journal("PedClearTether", L, 1);
	if(CScriptPed *p = PedArg(L, 1)) p->laisse = 0;
	return 0;
}

// PedFaceObject(ped, id, genre, mode[, b]) (0x5c77b0) : l'entité de LuaGetEntity(id, genre) (2 un piéton, 3 le joueur ; les
// objets et les véhicules ne sont pas suivis). Mode 0 : FUN_005c73c0 tourne le piéton tout de suite vers elle (la direction à
// plat, normée, son angle moins π/2, ramené dans [0, 2π[ : FUN_004755e0). Sinon un FaceEntityObjective (type 4, FUN_00470ac0 :
// l'entité en +0x14, l'octet b en +0x18, vrai par défaut) va au bout de la file ; l'outil ne joue pas cet objectif.
static int
PedFaceObject(lua_State *L)
{
	Journal("PedFaceObject", L, 1);
	CScriptPed *p = PedArg(L, 1);
	if(p == nil) return 0;
	int id = (int)luaL_optnumber(L, 2, -1), genre = (int)luaL_optnumber(L, 3, -1), mode = (int)luaL_optnumber(L, 4, 0);
	const CScriptPed *e = genre == 2 ? pietons.Acceder(id) : genre == 3 ? pietons.Acceder(SCRIPT_PED_JOUEUR) : nil;
	if(e == nil || mode != 0) return 0;
	TournerVers(p, e);
	return 0;
}

// PedFaceHeading(ped, degrés, mode) (0x5c76b0) : le cap en radians (× π / 180, les doubles 0x900160 et 0x900158) à
// FUN_005c72e0 : mode 0, CPed::SetHeading (FUN_004755e0) puis la matrice et le cadre de rendu (FUN_00443950, FUN_006c9990) ;
// sinon un FaceHeadingObjective (FUN_004709d0 : le cap en +0x14, le genre 2) poussé dans la file (CPed::PushObjective), que
// l'outil ne joue pas. Le cap du moteur part de +y : celui de l'outil (depuis +x) vaut 90 degrés de plus.
static int
PedFaceHeading(lua_State *L)
{
	Journal("PedFaceHeading", L, 1);
	CScriptPed *p = PedArg(L, 1);
	if(p && (int)luaL_optnumber(L, 3, 0) == 0) PoserCap(p, (float)luaL_optnumber(L, 2, 0) + 90.0f);
	return 0;
}

// PlayerFaceHeadingNow(degrés) (0x5c7360, lue à l'assembleur de 0x5c7360 à 0x5c73b2) : le cap en radians (× π / 180, les doubles
// 0x900160 et 0x900158) posé au joueur (DAT_00c1aea8) par FUN_004755e0 (CPed::SetHeading : le cap visé +0x1700, la matrice par
// FUN_00412ad0, +0x10, le vecteur avant aux +0 et +0xc du composant +0x2e0), puis la matrice et le cadre de rendu (FUN_00443950,
// FUN_006c9990 sur le +0xc de +0x114) : le chemin du mode 0 de PedFaceHeading, sans file d'objectifs. Ne rend rien.
static int
PlayerFaceHeadingNow(lua_State *L)
{
	Journal("PlayerFaceHeadingNow", L, 1);
	PoserCap(pietons.Acceder(SCRIPT_PED_JOUEUR), (float)luaL_optnumber(L, 1, 0) + 90.0f);
	return 0;
}

// PedFaceObjectNow(ped, id, genre) (0x5c78b0) : un FaceEntityObjective sur la pile (FUN_00470ac0), son cap (FUN_00470b40 : vers
// l'entité comme FUN_005c73c0, sinon le cap visé +0x1700), posé par FUN_004755e0, puis la matrice et le cadre de rendu mis à
// jour (FUN_00443950, FUN_006c9990) et l'objectif détruit (FUN_00470b10) : le piéton est tourné à l'image même.
static int
PedFaceObjectNow(lua_State *L)
{
	Journal("PedFaceObjectNow", L, 1);
	CScriptPed *p = PedArg(L, 1);
	if(p == nil) return 0;
	int id = (int)luaL_optnumber(L, 2, -1), genre = (int)luaL_optnumber(L, 3, -1);
	const CScriptPed *e = genre == 2 ? pietons.Acceder(id) : genre == 3 ? pietons.Acceder(SCRIPT_PED_JOUEUR) : nil;
	if(e) TournerVers(p, e);
	return 0;
}

static int
PedGetHeading(lua_State *L)
{
	appels["PedGetHeading"]++;
	const CScriptPed *p = PedArg(L, 1);
	lua_pushnumber(L, p ? p->cap : 0);
	return 1;
}

// PedFaceXYZ(ped, x, y, z) : le cap vers ce point, en degrés depuis +x.
static int
PedFaceXYZ(lua_State *L)
{
	Journal("PedFaceXYZ", L, 1);
	CScriptPed *p = PedArg(L, 1);
	if(p){
		float dx = (float)luaL_checknumber(L, 2) - p->pos.x, dy = (float)luaL_checknumber(L, 3) - p->pos.y;
		p->cap = atan2f(dy, dx) * 180.0f / 3.14159265f;
	}
	return 0;
}

static int
PedIsDead(lua_State *L)   // 0x5c8f30 : +0x1c98, vrai sans piéton
{
	appels["PedIsDead"]++;
	const CScriptPed *p = PedArg(L, 1);
	lua_pushboolean(L, p ? p->mort : 1);
	return 1;
}

static int
PedGetHealth(lua_State *L)
{
	appels["PedGetHealth"]++;
	const CScriptPed *p = PedArg(L, 1);
	lua_pushnumber(L, p ? p->sante : 0);
	return 1;
}

// PedSetHealth(ped, pv) (0x5c8e80) : FUN_00478190 (la santé, pas sous la santé minimale ; nulle, le piéton ne meurt
// pas), et le maximum (+0x1ca4) relevé s'il est dépassé.
static int
PedSetHealth(lua_State *L)
{
	Journal("PedSetHealth", L, 1);
	if(!lua_isnumber(L, 1)) return 0;
	int h = (int)lua_tonumber(L, 1);
	float v = (float)luaL_checknumber(L, 2);
	pietons.Sante(h, v);
	if(CScriptPed *p = pietons.Acceder(h)) if(p->santeMax < v) p->santeMax = v;
	return 0;
}

// PedSetImmortalFlag(ped, oui) (0x5ce6e0) et PedGetImmortalFlag(ped) (0x5ce730, faux sans piéton) : +0x13fc, qui
// empêche la mort par la piste Death (FUN_00479110).
static int
PedSetImmortalFlag(lua_State *L)
{
	Journal("PedSetImmortalFlag", L, 1);
	if(CScriptPed *p = lua_isnumber(L, 1) ? pietons.Acceder((int)lua_tonumber(L, 1)) : nil) p->immortel = lua_toboolean(L, 2) != 0;
	return 0;
}

static int
PedGetImmortalFlag(lua_State *L)
{
	appels["PedGetImmortalFlag"]++;
	const CScriptPed *p = lua_isnumber(L, 1) ? pietons.Acceder((int)lua_tonumber(L, 1)) : nil;
	lua_pushboolean(L, p && p->immortel);
	return 1;
}

// PedSetInvulnerable(ped, oui) (0x5c9160) : +0x138c ; PedSetInvulnerableToPlayer(ped, oui) (0x5c9200) : +0x1364 ;
// PlayerSetInvulnerable(oui) (0x5d1810) : +0x138c du joueur ; PedSetMinHealth(ped, pv) (0x5c8ef0) : +0x1ca8.
// FUN_0047b6b0 et FUN_004782a0 les lisent quand un coup porte (CScriptPedPool::Blesser).
static CScriptPed *
PietonArg(lua_State *L, int i){ return lua_isnumber(L, i) ? pietons.Acceder((int)lua_tonumber(L, i)) : nil; }

static int
PedSetInvulnerable(lua_State *L)
{
	Journal("PedSetInvulnerable", L, 1);
	if(CScriptPed *p = PietonArg(L, 1)) p->drapeaux[DRAPEAU_INVULNERABLE] = lua_toboolean(L, 2) != 0;
	return 0;
}

static int
PedSetInvulnerableToPlayer(lua_State *L)
{
	Journal("PedSetInvulnerableToPlayer", L, 1);
	if(CScriptPed *p = PietonArg(L, 1)) p->drapeaux[DRAPEAU_INVULNERABLE_JOUEUR] = lua_toboolean(L, 2) != 0;
	return 0;
}

static int
PlayerSetInvulnerable(lua_State *L)
{
	Journal("PlayerSetInvulnerable", L, 1);
	pietons.Acceder(SCRIPT_PED_JOUEUR)->drapeaux[DRAPEAU_INVULNERABLE] = lua_toboolean(L, 1) != 0;
	return 0;
}

static int
PedSetMinHealth(lua_State *L)
{
	Journal("PedSetMinHealth", L, 1);
	if(CScriptPed *p = PietonArg(L, 1)) p->santeMin = (float)luaL_checknumber(L, 2);
	return 0;
}

// PedGetMaxHealth(ped) (0x5c8d80) et PedSetMaxHealth(ped, pv) (0x5c8de0) : +0x1ca4 ; GameGetPedStat(ped, n)
// (0x5d4ca0, FUN_00476f10) et GameSetPedStat(ped, n, v) (0x5d4d10, FUN_00476fa0) : la colonne n en +0x1900.
static int
PedGetMaxHealth(lua_State *L)
{
	appels["PedGetMaxHealth"]++;
	const CScriptPed *p = PedArg(L, 1);
	lua_pushnumber(L, p ? p->santeMax : 0);
	return 1;
}

static int
PedSetMaxHealth(lua_State *L)
{
	Journal("PedSetMaxHealth", L, 1);
	if(CScriptPed *p = PietonArg(L, 1)) p->santeMax = (float)luaL_checknumber(L, 2);
	return 0;
}

// PedSetFightingSystem(ped, n) (0x5ca590) : +0x16dc, que la condition PedFightingSystem compare.
static int
PedSetFightingSystem(lua_State *L)
{
	Journal("PedSetFightingSystem", L, 1);
	if(CScriptPed *p = PietonArg(L, 1)) p->systemeCombat = (int32)luaL_checknumber(L, 2);
	return 0;
}

static int
GameGetPedStat(lua_State *L)
{
	appels["GameGetPedStat"]++;
	lua_pushnumber(L, lua_isnumber(L, 1) ? pietons.Stat((int)lua_tonumber(L, 1), (int)luaL_checknumber(L, 2)) : 0);
	return 1;
}

static int
GameSetPedStat(lua_State *L)
{
	Journal("GameSetPedStat", L, 1);
	if(lua_isnumber(L, 1)) pietons.PoserStat((int)lua_tonumber(L, 1), (int)luaL_checknumber(L, 2), (int32)luaL_checknumber(L, 3));
	return 0;
}

// Les attitudes (CScriptPedPool::attitudesTypes, CAttitudeSet du piéton). PedSetDefaultTypeToTypeAttitude(de, vers, v)
// (0x5c6b30 → 0x46f7a0), PedSetTypeToTypeAttitude (0x5c6af0 → 0x46f760), PedGetTypeToTypeAttitude(de, vers) (0x5c6b80 →
// 0x46f6e0) ; PedSetPedToTypeAttitude(ped, type, v) (0x5cb480 → 0x473080), PedGetPedToTypeAttitude(ped, type) (0x5cb4d0 →
// 0x475230, 2 sans piéton).
static int
PedSetDefaultTypeToTypeAttitude(lua_State *L)
{
	Journal("PedSetDefaultTypeToTypeAttitude", L, 1);
	pietons.attitudesTypes.PoserDefaut((int)luaL_optnumber(L, 1, -1), (int)luaL_optnumber(L, 2, -1), (int32)luaL_optnumber(L, 3, 2));
	return 0;
}
static int
PedSetTypeToTypeAttitude(lua_State *L)
{
	Journal("PedSetTypeToTypeAttitude", L, 1);
	pietons.attitudesTypes.PoserTypeVersType((int)luaL_optnumber(L, 1, -1), (int)luaL_optnumber(L, 2, -1), (int32)luaL_optnumber(L, 3, 2));
	return 0;
}
static int
PedGetTypeToTypeAttitude(lua_State *L)
{
	appels["PedGetTypeToTypeAttitude"]++;
	lua_pushnumber(L, pietons.attitudesTypes.TypeVersType((int)luaL_optnumber(L, 1, -1), (int)luaL_optnumber(L, 2, -1)));
	return 1;
}
static int
PedSetPedToTypeAttitude(lua_State *L)
{
	Journal("PedSetPedToTypeAttitude", L, 1);
	if(CScriptPed *p = PedArg(L, 1)) p->attitudes.Ecrire((int)luaL_optnumber(L, 2, -1), (int32)luaL_optnumber(L, 3, 2));
	return 0;
}
static int
PedGetPedToTypeAttitude(lua_State *L)
{
	appels["PedGetPedToTypeAttitude"]++;
	lua_pushnumber(L, PedArg(L, 1) ? pietons.Attitude((int)lua_tonumber(L, 1), (int)luaL_optnumber(L, 2, -1)) : 2);
	return 1;
}

// SetFactionRespect(faction, respect) (0x5c1130, lu à l'assembleur) : le respect, en flottant, ramené à 0 s'il est négatif et à
// 100 au-delà (_DAT_00900130, 0x900490), tronqué (_ftol) et rangé par FUN_00446610 en +0x1b0 + 4 × faction des données du
// joueur (0xc1aea8) ; GetFactionRespect(faction) (0x5c11b0 → FUN_00446630) le rend.
static int
SetFactionRespect(lua_State *L)
{
	Journal("SetFactionRespect", L, 1);
	int f = (int)luaL_optnumber(L, 1, -1);
	float r = (float)luaL_optnumber(L, 2, 0);
	if(!(r > 0.0f)) r = 0.0f; else if(r > 100.0f) r = 100.0f;
	if(f >= 0 && f < 16) pietons.respect[f] = (int32)r;
	return 0;
}
static int
GetFactionRespect(lua_State *L)
{
	appels["GetFactionRespect"]++;
	int f = (int)luaL_optnumber(L, 1, -1);
	lua_pushnumber(L, f >= 0 && f < 16 ? pietons.respect[f] : 0);
	return 1;
}

// PedOverrideStat(ped, n, valeur) (0x5c9070) : la colonne n de ses stats par FUN_00476fa0, comme GameSetPedStat.
static int
PedOverrideStat(lua_State *L)
{
	Journal("PedOverrideStat", L, 1);
	if(lua_isnumber(L, 1)) pietons.PoserStat((int)lua_tonumber(L, 1), (int)luaL_checknumber(L, 2), (int32)luaL_checknumber(L, 3));
	return 0;
}

// PedSetFlag(ped, n, b) (lua_PedSetFlag 0x5cc0b0) : le mot +0x1314 + 4n reçoit l'octet de b (FUN_005d8f20), sauf le drapeau 12
// (+0x1344, que la méthode du coup lit : un script ne peut pas le poser) et au-delà de 0x88. PedGetFlag(ped, n) (0x5cc110) le
// rend, vrai s'il n'est pas nul (sans borne sur n dans le moteur ; l'outil rend faux au-delà).
static int
PedSetFlag(lua_State *L)
{
	Journal("PedSetFlag", L, 1);
	CScriptPed *p = PedArg(L, 1);
	if(p == nil) return 0;
	uint32 n = (uint32)(int32)luaL_optnumber(L, 2, -1);
	if(n != DRAPEAU_INTOUCHABLE && n < SCRIPT_PED_DRAPEAUX) p->drapeaux[n] = lua_toboolean(L, 3) ? 1 : 0;
	return 0;
}

static int
PedGetFlag(lua_State *L)
{
	appels["PedGetFlag"]++;
	const CScriptPed *p = PedArg(L, 1);
	if(p == nil) return 0;
	uint32 n = (uint32)(int32)luaL_optnumber(L, 2, -1);
	lua_pushboolean(L, n < SCRIPT_PED_DRAPEAUX && p->drapeaux[n] != 0);
	return 1;
}

// --- Les gestes des piétons : marche, course, arrêt, bagarre (src/peds/ScriptPedPool.h). ---

// PedMoveToPoint(ped, allure, liste, n) (0x5c7df0) : vers le n-ième point de la liste.
static int
PedMoveToPoint(lua_State *L)
{
	Journal("PedMoveToPoint", L, 1);
	const CTriggerPointPoint *pt = PointArg(L, "PedMoveToPoint", 3, 4);
	if(pt && lua_isnumber(L, 1)){
		pietons.Aller((int)lua_tonumber(L, 1), pt->pos, (int)luaL_optnumber(L, 2, ALLURE_MARCHE));
		// FUN_005c6180 : la vitesse d'intention de l'allure, 0,7 sans argument.
		if(CScriptPed *p = pietons.Acceder((int)lua_tonumber(L, 1))) p->intentionVitesse = ScriptPedIntentionVitesse(lua_isnumber(L, 2) ? (int)lua_tonumber(L, 2) : -1);
	}
	return 0;
}

// La piste FollowPath (FollowPathTrack 0x93800c) d'un piéton dont l'objectif courant est un FollowPath, jouée par l'outil
// comme un geste (le moteur la lance depuis l'arbre de l'IA, GeneralObjectives/FollowPath) : son état (CSuiviTrajet), le
// nœud atteint en dernier, l'échéance d'une attente (+0x1c) et les deux rappels Lua de l'objectif.
struct SuiviPieton {
	CSuiviTrajet s;
	bool lance = false;
	uint32 echeance = 0;
	int rappelAtteint = LUA_NOREF, rappelQuitte = LUA_NOREF, rappelAction = LUA_NOREF;
	bool enActions = false;      // +0x64 de la piste : les actions du nœud en cours (FUN_0062eb70)
	int32 action = 0;            // +0x2c : l'action suivante
};
static std::map<int, SuiviPieton> suivis;
static void Oublier(lua_State *L, int &ref);

static void
OublierSuivi(int h)
{
	auto it = suivis.find(h);
	if(it == suivis.end()) return;
	if(etatLua){ Oublier(etatLua, it->second.rappelAtteint); Oublier(etatLua, it->second.rappelQuitte); Oublier(etatLua, it->second.rappelAction); }
	suivis.erase(it);
}

static std::vector<CNoeudTrajet>
NoeudsDuTrajet(int trajet)
{
	std::vector<CNoeudTrajet> n;
	if(const CTriggerPath *t = donnees.TrajetParHandle(trajet))
		for(const CTriggerPathPoint &p : t->points){
			CNoeudTrajet x; x.pos = p.pos; x.attente = p.attente; x.actions = !p.actions.empty();
			for(const CTriggerAction &a : p.actions){ CActionNoeud c; c.type = a.type; c.lacet = a.orientation[0]; c.attente = a.attente; x.listeActions.push_back(c); }
			n.push_back(x);
		}
	return n;
}

// FUN_0062e640 et FUN_0062df60 : le rappel est appelé avec (piéton, trajet, nœud).
static void
RappelerSuivi(lua_State *L, int ref, int ped, int trajet, int noeud, const char *quoi)
{
	printf("%6.2f s  piéton %d, trajet %d : nœud %d %s\n", maintenant / 1000.0, ped, trajet, noeud, quoi);
	if(ref == LUA_NOREF) return;
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	lua_pushnumber(L, ped); lua_pushnumber(L, trajet); lua_pushnumber(L, noeud);
	if(lua_pcall(L, 3, 0, 0) != 0){ printf("        rappel du trajet : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
}

static void
AvancerSuivis(lua_State *L)
{
	for(auto it = suivis.begin(); it != suivis.end(); ){
		int ped = it->first;
		SuiviPieton &v = it->second;
		CScriptPed *p = pietons.Acceder(ped);
		const CObjectifPieton *o = p ? pietons.ObjectifCourant(ped) : nil;
		if(p == nil || p->mort){ Oublier(L, v.rappelAtteint); Oublier(L, v.rappelQuitte); Oublier(L, v.rappelAction); it = suivis.erase(it); continue; }
		// Pas l'objectif courant : la piste s'arrête et rend son état à l'objectif (FUN_0062c230 : le nœud, le mode, le sens) ;
		// relancée, elle repart de ce nœud sans précédent ni plan (FUN_0062c290).
		if(o == nil || o->type != OBJECTIF_SUIVRE_TRAJET){ v.s.precedent = -1; v.s.plan = -1; ++it; continue; }
		int trajet = o->trajet;
		std::vector<CNoeudTrajet> noeuds = NoeudsDuTrajet(trajet);
		int nombre = (int)noeuds.size();
		CSuiviTrajet &s = v.s;
		if(!v.lance){   // FUN_0062c290, FUN_0062c1e0
			s = CSuiviTrajet();
			s.noeud = o->noeudDepart; s.mode = o->mode; s.sens = 1;
			v.lance = true; v.echeance = 0;
		}
		if(nombre == 0 || s.noeud < 0 || s.noeud >= nombre){ ++it; continue; }
		// FUN_0062e5f0 : le nœud quitté, puis le suivant ; sans suivant, la fin retire l'objectif (FUN_00621c80).
		bool fini = false;
		auto suivant = [&](void){
			int32 n = s.noeud;
			RappelerSuivi(L, v.rappelQuitte, ped, trajet, n, "quitté");
			s.precedent = n;
			if(ScriptPedSuiviSuivant(s, nombre, s.noeud)){ ScriptPedSuiviPoserPlan(s, noeuds, n, s.noeud); return; }
			s.noeud = 0;
			pietons.RetirerObjectifs(ped, OBJECTIF_SUIVRE_TRAJET);
			fini = true;
		};
		// FUN_0062e780 : l'action suivante du nœud (un virage ou un arrêt, puis son attente), le rappel +0x34 (FUN_0062e040, avec le
		// piéton, le trajet, le nœud et le rang de l'action) ; la liste finie, le nœud suivant (FUN_0062e5f0), sans pas à cette image.
		auto jouerAction = [&](void){
			CActionNoeud a;
			if(ScriptPedSuiviAction(noeuds[s.noeud], v.action, maintenant, v.echeance, a)){
				if(a.type == 0){ PoserCap(p, a.lacet); }   // FUN_00621c00 : la direction de l'intention au lacet du virage
				p->intentionVitesse = 0;
				printf("%6.2f s  piéton %d, trajet %d : nœud %d, action %d (type %d, %.2f s)\n", maintenant / 1000.0, ped, trajet, s.noeud, v.action, a.type, a.attente);
				if(v.rappelAction != LUA_NOREF){
					lua_rawgeti(L, LUA_REGISTRYINDEX, v.rappelAction);
					lua_pushnumber(L, ped); lua_pushnumber(L, trajet); lua_pushnumber(L, s.noeud); lua_pushnumber(L, v.action);
					if(lua_pcall(L, 4, 0, 0) != 0){ printf("        rappel de l'action : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
				}
				return;
			}
			v.enActions = false;
			suivant();
			p->intentionVitesse = 0;
		};
		if(v.echeance != 0){   // FUN_0062eb70 : l'attente d'un nœud
			if(maintenant <= v.echeance){ p->intentionVitesse = 0; ++it; continue; }
			v.echeance = 0;
			if(v.enActions){ jouerAction(); ++it; continue; }
			suivant();
			if(fini){ p->intentionVitesse = 0; ++it; continue; }
		}
		// L'arrivée au nœud visé, sous le rayon du piéton (+0x51c) : les plans en attente, puis le rappel (FUN_0062e640).
		if(ScriptPedSuiviAtteint(s, nombre, p->pos, noeuds[s.noeud].pos, p->rayonNoeud, false)){
			while(s.plan != -1 && s.plan != s.noeud){
				int32 q = s.plan, r = q;
				RappelerSuivi(L, v.rappelAtteint, ped, trajet, q, "atteint");
				RappelerSuivi(L, v.rappelQuitte, ped, trajet, q, "quitté");
				if(ScriptPedSuiviSuivant(s, nombre, r)) ScriptPedSuiviPoserPlan(s, noeuds, q, r); else s.plan = -1;
			}
			RappelerSuivi(L, v.rappelAtteint, ped, trajet, s.noeud, "atteint");
			if(noeuds[s.noeud].actions){ v.enActions = true; v.action = 0; jouerAction(); ++it; continue; }
			if(noeuds[s.noeud].attente > 0.0f){
				v.echeance = ScriptPedSuiviEcheance(noeuds[s.noeud].attente, maintenant);
				p->cap = ScriptPedSuiviCap(donnees.TrajetParHandle(trajet)->points[s.noeud].lacetTangageRoulis[0], false);   // FUN_0062c300
				p->intentionVitesse = 0;
				++it; continue;
			}
			suivant();
			if(fini){ p->intentionVitesse = 0; ++it; continue; }
		}
		// Le pas (FUN_0062ee20) : le but lissé (FUN_00621a30 → FUN_005e7560), puis le passage d'un plan (FUN_0062eaa0).
		float vitesseIntention = o->vitesse < 0.0f ? 1.1f : o->vitesse;   // FUN_00478030 : à pied, -1 → _DAT_0090cec8
		if(s.precedent == -1){
			if(ScriptPedSuiviArret(s, noeuds, s.noeud)){ s.precedent = -2; }
			else{ ScriptPedSuiviPoserPlan(s, noeuds, -1, s.noeud); s.precedent = s.noeud; ScriptPedSuiviSuivant(s, nombre, s.noeud); }
		}
		CVector but = noeuds[s.noeud].pos;
		if(s.precedent >= 0 && s.precedent < nombre){
			float t = 0;
			but = ScriptPedSuiviBut(noeuds[s.precedent].pos, noeuds[s.noeud].pos, p->pos, 3.0f * vitesseIntention + 3.0f, t);
			if(t > 1.0f && !ScriptPedSuiviArret(s, noeuds, s.noeud)){ s.precedent = s.noeud; ScriptPedSuiviSuivant(s, nombre, s.noeud); }
		}
		if(ScriptPedSuiviPlanPasse(s, p->pos)){
			int32 q = s.plan, r = q;
			RappelerSuivi(L, v.rappelAtteint, ped, trajet, q, "atteint");
			RappelerSuivi(L, v.rappelQuitte, ped, trajet, q, "quitté");
			if(ScriptPedSuiviSuivant(s, nombre, r)) ScriptPedSuiviPoserPlan(s, noeuds, q, r); else s.plan = -1;
		}
		float dx = but.x - p->pos.x, dy = but.y - p->pos.y, dz = but.z - p->pos.z, d = sqrtf(dx * dx + dy * dy);
		float pas = ScriptPedVitesse(o->allure < 0 ? ALLURE_MARCHE : o->allure) * 0.033f;
		p->enMarche = false;
		p->intentionVitesse = vitesseIntention;
		if(d > 1e-3f){
			float k = d < pas ? 1.0f : pas / d;
			p->pos.x += dx * k; p->pos.y += dy * k; p->pos.z += dz * k;
			p->cap = atan2f(dy, dx) * 180.0f / 3.14159265f;
			p->intention = CVector(dx / d, dy / d, 0);
		}
		++it;
	}
}

// RegisterPedEventHandler(ped, événement, fonction) (0x5bbdf0, lu à l'assembleur : Ghidra ne l'a pas en fonction) : le piéton
// (FUN_005c7220), puis, si le troisième argument est une fonction (FUN_005d84f0), la paire du piéton (FUN_005b4600) et sa case
// (FUN_005b4030) reçoivent le rappel (FUN_005d9f00) ; sinon la case est vidée et la paire retirée (FUN_005b42d0). Seul
// l'événement 0 a sa propre case (CGestionnairesPietons) : l'outil ignore les autres.
static CGestionnairesPietons gestionnaires;

static int
RegisterPedEventHandler(lua_State *L)
{
	Journal("RegisterPedEventHandler", L, 1);
	CScriptPed *p = PedArg(L, 1);
	int ped = p ? (int)lua_tonumber(L, 1) : -1, evenement = (int)luaL_optnumber(L, 2, 0);
	if(p == nil || evenement != 0) return 0;
	if(lua_isfunction(L, 3)){
		gestionnaires.Ajouter(ped);
		int *c = gestionnaires.Case(ped);
		if(c == nil) return 0;
		if(*c != CGestionnairesPietons::SANS_RAPPEL) luaL_unref(L, LUA_REGISTRYINDEX, *c);
		lua_pushvalue(L, 3);
		*c = luaL_ref(L, LUA_REGISTRYINDEX);
	}
	else{
		int r = gestionnaires.Retirer(ped);
		if(r != CGestionnairesPietons::SANS_RAPPEL) luaL_unref(L, LUA_REGISTRYINDEX, r);
	}
	return 0;
}

// SetNumberOfHandledTriggerEventObjects(n[, table]) (FUN_005bbeb0) : n (arrondi par FUN_005d8200) et le script qui appelle
// (FUN_005db7e0, son +0x44) passent à FUN_005b5e50, pour la table 1 avec un seul argument, pour la table du second sinon ;
// rien pour n nul ou négatif. LibTriggerNew (L_MonitorTriggers, L_RegisterNumberOfObjectEvents) l'appelle une fois par
// mission, avant ses RegisterTriggerEventHandler.
static int
SetNumberOfHandledTriggerEventObjects(lua_State *L)
{
	Journal("SetNumberOfHandledTriggerEventObjects", L, 1);
	int arguments = lua_gettop(L), n = (int)luaL_optnumber(L, 1, 0);
	if(n <= 0) return 0;
	if(arguments == 1) declencheursEv.Dimensionner(1, n, scriptCourant);
	else if(arguments == 2) declencheursEv.Dimensionner((int)luaL_optnumber(L, 2, 0), n, scriptCourant);
	return 0;
}

// RegisterTriggerEventHandler(déclencheur, événement, fonction[, table]) (0x5bbfa0, lu à l'assembleur de 0x5bbfa0 à
// 0x5bc09e) : le nombre d'arguments (FUN_005d8170), le déclencheur et l'événement (FUN_005d8200) ; rien si le déclencheur
// est négatif ou libre dans son pool (DAT_020c7c48, l'octet 0x80). Si le troisième est une fonction (FUN_005d84f0) : avec
// trois arguments, l'entrée du déclencheur dans la table 1 (FUN_005b5d10(·, 1)) et le rappel dans sa case (FUN_005b4ff0,
// FUN_005d9f00) ; avec quatre, dans la table du quatrième. Sinon, la case est vidée (FUN_005b56f0) : dans la table 1 avec
// trois arguments, dans la table 0 avec quatre (le moteur pousse 0, quel que soit le quatrième). Le rappel sera appelé avec
// (déclencheur, piéton).
static int
RegisterTriggerEventHandler(lua_State *L)
{
	Journal("RegisterTriggerEventHandler", L, 1);
	int arguments = lua_gettop(L), t = (int)luaL_optnumber(L, 1, -1), e = (int)luaL_optnumber(L, 2, 0);
	if(t < 0 || donnees.ZoneParHandle(t) == nil) return 0;
	if(lua_isfunction(L, 3)){
		int table = arguments == 3 ? 1 : arguments == 4 ? (int)luaL_optnumber(L, 4, 0) : -1;
		if(table < 0) return 0;
		declencheursEv.Ajouter(table, t);
		int *c = declencheursEv.Case(table, t, e);
		if(c == nil) return 0;
		if(*c != CGestionnairesDeclencheurs::SANS_RAPPEL) luaL_unref(L, LUA_REGISTRYINDEX, *c);
		lua_pushvalue(L, 3);
		*c = luaL_ref(L, LUA_REGISTRYINDEX);
		for(CGestionnairesDeclencheurs::Entree &n : declencheursEv.tables[table].entrees)
			if(n.declencheur == t && e >= 0 && e < CGestionnairesDeclencheurs::EVENEMENTS) n.scripts[e] = scriptCourant;
	}
	else if(arguments == 3 || arguments == 4){
		int r = declencheursEv.Retirer(arguments == 3 ? 1 : 0, t, e);
		if(r != CGestionnairesDeclencheurs::SANS_RAPPEL) luaL_unref(L, LUA_REGISTRYINDEX, r);
	}
	return 0;
}

// AreaSetTriggerMonitoringRules(déclencheur, joueurSeul) (0x5b7f60, lu à l'assembleur de 0x5b7f74 à 0x5b7fa2) : le bit 2 de
// l'octet +0x3f du déclencheur prend le booléen (FUN_005d8f20) ; FUN_006d6f60 ne suit alors que le joueur dans ce déclencheur.
static int
AreaSetTriggerMonitoringRules(lua_State *L)
{
	Journal("AreaSetTriggerMonitoringRules", L, 1);
	int t = (int)luaL_optnumber(L, 1, -1);
	if(t < 0 || donnees.ZoneParHandle(t) == nil) return 0;
	if(lua_toboolean(L, 2)) declencheursJoueurSeul.insert(t); else declencheursJoueurSeul.erase(t);
	return 0;
}

// Les gestionnaires globaux des événements (RegisterGlobalEventHandler 0x5bbcf0, lu à l'assembleur de 0x5bbcf0 à 0x5bbd71) : deux
// tables (FUN_005b1c70(&événement, table) rend la case), la table 0 avec deux arguments, sinon celle que choisit le troisième (0 ou
// non) ; sans fonction (FUN_005d84f0), la case de la table 0 est vidée (FUN_005b1ca0). FUN_005b1ce0 dit si une case a un rappel.
// Lancés par l'outil : l'événement 7 (un piéton frappé, FUN_005b2190) et l'événement 1 (un CObject détruit, FUN_005b3530).
static int gestionnairesGlobaux[2][32];
static bool gestionnairesGlobauxPrets = false;
static int *
CaseGlobale(int e, int table)
{
	if(!gestionnairesGlobauxPrets){ for(auto &t : gestionnairesGlobaux) for(int &c : t) c = LUA_NOREF; gestionnairesGlobauxPrets = true; }
	return e >= 0 && e < 32 ? &gestionnairesGlobaux[table ? 1 : 0][e] : nil;
}

static int
RegisterGlobalEventHandler(lua_State *L)
{
	Journal("RegisterGlobalEventHandler", L, 1);
	int n = lua_gettop(L), e = (int)luaL_optnumber(L, 1, -1);
	int table = n == 2 ? 0 : (int)luaL_optnumber(L, 3, 0) != 0;
	if(!lua_isfunction(L, 2)) table = 0;
	int *c = CaseGlobale(e, table);
	if(c == nil) return 0;
	if(*c != LUA_NOREF){ luaL_unref(L, LUA_REGISTRYINDEX, *c); *c = LUA_NOREF; }
	if(lua_isfunction(L, 2)){ lua_pushvalue(L, 2); *c = luaL_ref(L, LUA_REGISTRYINDEX); }
	return 0;
}

// Le rappel d'une case globale, avec deux arguments déjà poussés par `pousser` ; rien sans rappel.
static void
AppelerGlobal(int e, int table, const char *quoi, void (*pousser)(lua_State *, const void *), const void *donnees)
{
	int *c = CaseGlobale(e, table);
	if(c == nil || *c == LUA_NOREF || etatLua == nil) return;
	lua_State *L = etatLua;
	printf("%6.2f s  le gestionnaire global de l'événement %d (table %d) est appelé : %s\n", maintenant / 1000.0, e, table, quoi);
	lua_rawgeti(L, LUA_REGISTRYINDEX, *c);
	pousser(L, donnees);
	if(lua_pcall(L, 2, 0, 0) != 0){ printf("        gestionnaire global : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
}

// FUN_005b2190, depuis la méthode du coup (FUN_0047b810) : les gestionnaires globaux de l'événement 7 (tables 0 puis 1, FUN_005b1ce0,
// FUN_005b1c70), appelés avec les poignées du frappé et de l'attaquant (l'emplacement +0xac de chacun, -1 sans lui) ; puis le rappel de
// l'événement 0 du piéton frappé (FUN_005b40b0, FUN_005b4030), avec (frappé, attaquant).
static void
PrevenirCoupRecu(int cible, int attaquant)
{
	struct P { int a, b; } paire{cible, attaquant};
	auto pousser = [](lua_State *L, const void *d){ const P *p = (const P *)d; lua_pushnumber(L, p->a); lua_pushnumber(L, p->b); };
	AppelerGlobal(7, 0, "un piéton frappé", pousser, &paire);
	AppelerGlobal(7, 1, "un piéton frappé", pousser, &paire);
	if(etatLua == nil || !gestionnaires.Pret(cible)) return;
	lua_State *L = etatLua;
	printf("%6.2f s  le rappel du coup reçu de %d est appelé (%d, %d)\n", maintenant / 1000.0, cible, cible, attaquant);
	lua_rawgeti(L, LUA_REGISTRYINDEX, *gestionnaires.Case(cible));
	lua_pushnumber(L, cible); lua_pushnumber(L, attaquant);
	gestionnaires.enAppel = true;
	if(lua_pcall(L, 2, 0, 0) != 0){ printf("        rappel du coup : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
	gestionnaires.enAppel = false;
}

// PedFollowPath(ped, trajet, mode, allure, rappel, départ, rappel2, rappel3) (0x5cfbf0) : rien sans piéton, sans trajet
// (FUN_005ed240), pour un trajet vide, ou d'un seul nœud hors du mode 3. Avec plus de cinq arguments, ou si l'objectif
// FollowPath de sa file (FUN_005cf670 : le premier de type 12) suit un autre trajet : le contrôleur de l'IA s'arrête
// (FUN_005f3f40 sur +0x290), la file se vide (FUN_00470200) et un objectif FollowPath neuf (FUN_0046fc20) va au bout
// (CPed::PushObjective), son nœud de départ pris au sixième argument (0 sinon). L'objectif passe ensuite devant (FUN_00471be0),
// reçoit le mode (+0x24) et la vitesse de l'allure (+0x3c, FUN_005c6180, 0,3 par défaut, _DAT_00911e30). Les trois rappels
// (+0x2c au nœud atteint, FUN_0062e640 ; +0x30 au nœud quitté, FUN_0062df60 ; +0x34) ne sont pas encore gardés : la piste
// FollowPath qui avance le piéton n'est pas jouée (docs/missions.md, « 1_03 »).
static int
PedFollowPath(lua_State *L)
{
	Journal("PedFollowPath", L, 1);
	CScriptPed *p = lua_isnumber(L, 1) ? pietons.Acceder((int)lua_tonumber(L, 1)) : nil;
	if(p == nil || !lua_isnumber(L, 2)) return 0;
	int ped = (int)lua_tonumber(L, 1), trajet = (int)lua_tonumber(L, 2), mode = (int)luaL_optnumber(L, 3, 0);
	const CTriggerPath *t = donnees.TrajetParHandle(trajet);
	if(t == nil || t->points.empty() || (t->points.size() == 1 && mode != 3)) return 0;
	int arguments = lua_gettop(L), courant = -1;
	for(int i = 0; i < p->nombreObjectifs; i++) if(p->objectifs[i].type == OBJECTIF_SUIVRE_TRAJET){ courant = i; break; }
	if(arguments > 5 || courant < 0 || p->objectifs[courant].trajet != trajet){
		if(controleursIA.count(ped)) controleursIA[ped].Jouer(arbres, nil, maintenant / 1000.0f);   // FUN_005f3f40
		p->nombreObjectifs = 0;                                                                     // FUN_00470200
		CObjectifPieton o;
		o.type = OBJECTIF_SUIVRE_TRAJET; o.trajet = trajet;
		o.noeudDepart = arguments > 5 ? (int)luaL_optnumber(L, 6, 0) : 0;
		pietons.PousserObjectif(ped, o, false);
		courant = p->nombreObjectifs - 1;
		suivis[ped].lance = false;   // une piste neuve partira de ce départ
	}
	CObjectifPieton o = p->objectifs[courant];   // FUN_00471be0 : devant
	for(int i = courant; i > 0; i--) p->objectifs[i] = p->objectifs[i - 1];
	p->objectifs[0] = o;
	p->objectifs[0].mode = mode;
	p->objectifs[0].allure = lua_isnumber(L, 4) ? (int)lua_tonumber(L, 4) : -1;
	p->objectifs[0].vitesse = ScriptPedIntentionVitesse(p->objectifs[0].allure, 0.3f);
	// FUN_005d9f00 : les rappels du cinquième (+0x2c, nœud atteint) et du septième argument (+0x30, nœud quitté).
	SuiviPieton &v = suivis[ped];
	Oublier(L, v.rappelAtteint); Oublier(L, v.rappelQuitte);
	if(lua_isfunction(L, 5)){ lua_pushvalue(L, 5); v.rappelAtteint = luaL_ref(L, LUA_REGISTRYINDEX); }
	if(lua_isfunction(L, 7)){ lua_pushvalue(L, 7); v.rappelQuitte = luaL_ref(L, LUA_REGISTRYINDEX); }
	Oublier(L, v.rappelAction);   // le huitième (+0x34) : le rappel des actions des nœuds (FUN_0062e040, lu en 0x62e076)
	if(lua_isfunction(L, 8)){ lua_pushvalue(L, 8); v.rappelAction = luaL_ref(L, LUA_REGISTRYINDEX); }
	return 0;
}

// DistanceBetweenCoords3d(x1, y1, z1, x2, y2, z2) (0x5bfac0) et DistanceBetweenCoords2d(x1, y1, x2, y2) (0x5bfc10) : la racine
// (FUN_0085c4d0) du carré de la différence (FUN_00414a20), rendue en flottant (FUN_005d8480).
static int
DistanceBetweenCoords3d(lua_State *L)
{
	appels["DistanceBetweenCoords3d"]++;
	float x = (float)luaL_optnumber(L, 1, 0) - (float)luaL_optnumber(L, 4, 0), y = (float)luaL_optnumber(L, 2, 0) - (float)luaL_optnumber(L, 5, 0),
	      z = (float)luaL_optnumber(L, 3, 0) - (float)luaL_optnumber(L, 6, 0);
	lua_pushnumber(L, sqrtf(x * x + y * y + z * z));
	return 1;
}

static int
DistanceBetweenCoords2d(lua_State *L)
{
	appels["DistanceBetweenCoords2d"]++;
	float x = (float)luaL_optnumber(L, 1, 0) - (float)luaL_optnumber(L, 3, 0), y = (float)luaL_optnumber(L, 2, 0) - (float)luaL_optnumber(L, 4, 0);
	lua_pushnumber(L, sqrtf(x * x + y * y));
	return 1;
}

// PedPathNodeReachedDistance(ped, rayon) (0x5cd930) : le rayon d'arrivée aux nœuds de trajet (+0x51c) et l'octet +0x520.
static int
PedPathNodeReachedDistance(lua_State *L)
{
	Journal("PedPathNodeReachedDistance", L, 1);
	if(CScriptPed *p = lua_isnumber(L, 1) ? pietons.Acceder((int)lua_tonumber(L, 1)) : nil){
		p->rayonNoeud = (float)luaL_optnumber(L, 2, 0);
		p->rayonNoeudPose = true;
	}
	return 0;
}

// PathGetLastNode(trajet) (0x5bc600) : le dernier nœud (le nombre moins 1, FUN_005ec510), nil sans trajet ou pour un trajet vide.
static int
PathGetLastNode(lua_State *L)
{
	appels["PathGetLastNode"]++;
	const CTriggerPath *t = lua_isnumber(L, 1) ? donnees.TrajetParHandle((int)lua_tonumber(L, 1)) : nil;
	if(t && !t->points.empty()) lua_pushnumber(L, (lua_Number)(t->points.size() - 1));
	else lua_pushnil(L);
	return 1;
}

// PedMoveToXYZ(ped, allure, x, y, z) (0x5c7c90).
static int
PedMoveToXYZ(lua_State *L)
{
	Journal("PedMoveToXYZ", L, 1);
	CVector cible((float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5));
	if(lua_isnumber(L, 1)){
		pietons.Aller((int)lua_tonumber(L, 1), cible, (int)luaL_optnumber(L, 2, ALLURE_MARCHE));
		if(CScriptPed *p = pietons.Acceder((int)lua_tonumber(L, 1))) p->intentionVitesse = ScriptPedIntentionVitesse(lua_isnumber(L, 2) ? (int)lua_tonumber(L, 2) : -1);
	}
	return 0;
}

// PedStop(ped) (0x5c7630).
static int
PedStop(lua_State *L)
{
	Journal("PedStop", L, 1);
	if(lua_isnumber(L, 1)) pietons.Arreter((int)lua_tonumber(L, 1));
	return 0;
}

// PedAttack(attaquant, cible, style, devant, …) (0x5c9ca0) : un AttackPedObjective (FUN_00470c10 : type 7, la cible, le
// style) dans la file des objectifs de l'attaquant. Un troisième argument nombre est le style (3 dans 1_01 et 1_02) et le
// quatrième, un booléen (faux par défaut), le met devant (FUN_00471320) plutôt qu'au bout (FUN_00471240) ; sinon les
// troisième et quatrième, deux booléens, donnent le style par FUN_004969a0 (faux, vrai : 0 ; faux, faux : 1 ; vrai, vrai : 2 ;
// vrai, faux : 3). Le cinquième (un booléen) va en +0x1404, non suivi.
static int
PedAttack(lua_State *L)
{
	Journal("PedAttack", L, 1);
	if(!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) return 0;
	int32 style = 0; bool devant = false;
	if(lua_isnumber(L, 3)){ style = (int32)lua_tonumber(L, 3); devant = lua_toboolean(L, 4) != 0; }
	else {
		// FUN_005d8fa0(L, n, 1) : un booléen (ou un nombre) est lu, autre chose (nil compris) vaut le défaut, vrai.
		bool a = lua_isboolean(L, 3) ? lua_toboolean(L, 3) != 0 : true, b = lua_isboolean(L, 4) || lua_isnumber(L, 4) ? lua_toboolean(L, 4) != 0 : true;
		style = !a ? (b ? 0 : 1) : (b ? 2 : 3);
	}
	pietons.Attaquer((int)lua_tonumber(L, 1), (int)lua_tonumber(L, 2), style, devant);
	return 0;
}

// PedAttackPlayer(ped[, style[, devant]]) (0x5d2620) : un AttackPedObjective (FUN_00470c10) contre le joueur (FUN_005c7220(0, 3)),
// comme PedAttack : un deuxième argument nombre est le style et le troisième (faux par défaut) le met devant (FUN_00471320) ;
// sinon les deuxième et troisième, deux booléens (vrais par défaut, FUN_005d8fa0), donnent le style par FUN_004969a0.
static int
PedAttackPlayer(lua_State *L)
{
	Journal("PedAttackPlayer", L, 1);
	if(!lua_isnumber(L, 1)) return 0;
	int32 style = 0; bool devant = false;
	if(lua_isnumber(L, 2)){ style = (int32)lua_tonumber(L, 2); devant = lua_toboolean(L, 3) != 0; }
	else {
		bool a = lua_isboolean(L, 2) || lua_isnumber(L, 2) ? lua_toboolean(L, 2) != 0 : true, b = lua_isboolean(L, 3) || lua_isnumber(L, 3) ? lua_toboolean(L, 3) != 0 : true;
		style = !a ? (b ? 0 : 1) : (b ? 2 : 3);
	}
	pietons.Attaquer((int)lua_tonumber(L, 1), SCRIPT_PED_JOUEUR, style, devant);
	return 0;
}

// PedClearObjectives(ped) (0x5ca8e0) : CScriptPedPool::ViderObjectifs, puis le contrôleur de l'IA arrêté (FUN_005f3f40) ; la
// piste FollowPath, qui n'a plus d'objectif, s'arrête (FUN_0062c230).
static int
PedClearObjectives(lua_State *L)
{
	Journal("PedClearObjectives", L, 1);
	int ped = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	if(!pietons.ViderObjectifs(ped)) return 0;
	if(controleursIA.count(ped)) controleursIA[ped].Jouer(arbres, nil, maintenant / 1000.0f);
	return 0;
}

// PedMakeTargetable(ped, b) (0x5ca980) : +0xc0 du piéton reçoit la négation de b (FUN_005d8f20).
static int
PedMakeTargetable(lua_State *L)
{
	Journal("PedMakeTargetable", L, 1);
	if(CScriptPed *p = PedArg(L, 1)) p->pasCiblable = !lua_toboolean(L, 2);
	return 0;
}

// PedMakeAmbient(ped) (0x5ca850) : rendu à la population ; le script ne le ramasse plus.
static int
PedMakeAmbient(lua_State *L)
{
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	const CScriptPed *p = pietons.Acceder(h);
	if(p && p->script != -1) Journal("PedMakeAmbient", L, 1); else appels["PedMakeAmbient"]++;
	pietons.RendreAmbiant(h);
	return 0;
}

// La taille d'un piéton (CharacterSize : modelinfo +0x74, la colonne SIZE de l'IDE), -1 si inconnue.
static int
TailleDuPieton(const CScriptPed *p)
{
	if(p == nil) return -1;
	auto it = modeles.tailleDuPieton.find(p->modele);
	return it == modeles.tailleDuPieton.end() ? -1 : it->second;
}
static uint32 graineHumiliation = 1;   // le rand() du CRT que FUN_004925d0 tire (l'outil a sa propre graine)

// Les pistes des objectifs lancées à cette image : ClearCurrentObjective (FUN_0062a180 → FUN_00470110) et FleeObjective
// (FUN_00628ce0 : la cible fuie est +0x538, ou +0x544 si l'octet +0x19, non suivi ; rien sans cible).
static void
AppliquerObjectifs(int ped, CActionController &c)
{
	for(const CActionController::Effacement &e : c.effacementsLances){
		int n = pietons.RetirerObjectifs(ped, e.type);
		if(n > 0) printf("%6.2f s  le piéton %d perd %d objectif(s) de type %d (ClearCurrentObjective)\n", maintenant / 1000.0, ped, n, e.type);
	}
	for(const CActionController::Fuite &f : c.fuitesLancees){
		const CScriptPed *fuyard = pietons.Acceder(ped);
		int cible = f.autreCible ? (fuyard ? fuyard->agresseur : -1) : CibleDe(ped);
		if(cible < 0) continue;
		CObjectifPieton o; o.type = OBJECTIF_FUIR; o.cible = cible;
		if(pietons.PousserObjectif(ped, o, !f.auBout))
			printf("%6.2f s  le piéton %d fuit le piéton %d (FleeObjective, %s de la file)\n", maintenant / 1000.0, ped, cible, f.auBout ? "au bout" : "devant");
	}
	c.effacementsLances.clear(); c.fuitesLancees.clear();
}

// La mise à jour d'une piste ManageTargets (FUN_00627420), à chaque image où elle vit : l'octet +0x18 fait du joueur la
// cible (FUN_004989c0) ; si la cible (+0x538, FUN_0048bee0) est le joueur, un tirage de période 8 (FUN_00465910) mène à
// FUN_0048d6a0 (sur le composant social +0xb44, non suivi) et FUN_00492950 donne au piéton, sous la moitié de sa santé,
// son groupe d'humiliation. La suite (les types de piétons 0, 7, 8 et le +0x1d40 de la cible) n'est pas suivie.
static void
GererCibles(int ped, bool joueur)
{
	CScriptPed *p = pietons.Acceder(ped);
	if(p == nil) return;
	if(joueur) pietons.Verrouiller(ped, SCRIPT_PED_JOUEUR, 0);
	if(CibleDe(ped) != SCRIPT_PED_JOUEUR) return;
	if(p->groupeHumiliation != -1 && p->groupeHumiliation <= 12) return;
	// Le rand() de FUN_004925d0 n'est tiré que quand FUN_00492950 choisit, sous la moitié de la santé.
	if(!(p->santeMax > 0 && (double)(p->sante / p->santeMax) < 0.5)) return;
	float tirage = (float)RandCrt(graineHumiliation) / 32767.0f;
	int g = pietons.SurveillerHumiliation(ped, TailleDuPieton(p), TailleDuPieton(pietons.Acceder(SCRIPT_PED_JOUEUR)), tirage);
	if(g >= 0) printf("%6.2f s  le piéton %d (%.0f / %.0f points) reçoit le groupe d'humiliation %d (%s), par ManageTargets\n",
	                  maintenant / 1000.0, ped, p->sante, p->santeMax, g, ScriptPedGroupeHumiliation(g));
}

// La mise à jour d'une piste CombatLoco (FUN_0062ea40) : le tirage de période 8 (FUN_00465910, +0x78), forcé par +0x79 ;
// la cible (FUN_00625270 : le joueur si l'octet +0x18, sinon +0x538) ; aux images de tirage, ou à la première, le piéton
// prend une zone de combat autour d'elle (FUN_006252d0 : PedJoinCombatZone, la zone 0, sinon 1, sinon 2) ; les rayons de
// sa zone (FUN_006253e0 : +0x66c, AIButes +0x28 / +0x2c pour la zone 0, +0x2c / +0x30, +0x30 / +0x34, sinon +0x34 / +0x38,
// rangés en +0x6c et +0x70 ; un demi-mètre de plus si son émotion envers la cible est 4 à 6, non suivie) ; le choix du
// point (FUN_0062e120, ScriptPedCombatLocoChoisir) ; le pas (FUN_00625970) : la vitesse du mode (ScriptPedVitesseDuPas),
// le point qui suit la cible aux images de tirage (cible + le décalage +0x38, s'il est à plus de 0,5 m du piéton,
// _DAT_009055a0), et, sauf aux modes 3, 7 et 12, le cap voulu vers la cible ; la navigation (FUN_004a4eb0, le rayon
// d'arrivée 0,2 m, _DAT_00900484) en fait la vitesse de l'intention (FUN_005e4a60). La cible vue : FUN_0048d320
// (CScriptPedPool::Voit, sans monde à traverser). Supposé : le point toujours valable (FUN_006254c0), tout droit (sans la recherche de chemin, FUN_005e9df0), à
// l'allure de course de l'outil réduite dans le rapport de la vitesse de l'intention à 1,5.
static std::map<int, CCombatLoco> etatsCombat;
static uint32 graineCombat = 1;   // le rand() du CRT que la piste tire (l'outil a sa propre graine)
static int TirerCombat(void *){ return RandCrt(graineCombat); }
static uint32 graineVue = 7;      // le rand() du cache de la vue (FUN_0048d320)
static int TirerVue(void *){ return RandCrt(graineVue); }

// PedCanSeeObject(ped, id, genre) (0x5cafe0) : sans piéton, rien n'est rendu ; sinon l'entité de LuaGetEntity(id, genre) (2 un piéton, 3 le
// joueur ; les objets et les véhicules ne sont pas suivis : faux) passée au cache de la vue du piéton (FUN_0048d320,
// CScriptPedPool::Voit : la portée et le cône de sa ligne de pedstats.dat).
static int
PedCanSeeObject(lua_State *L)
{
	Journal("PedCanSeeObject", L, 1);
	int h = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	if(pietons.Acceder(h) == nil) return 0;
	int id = (int)luaL_optnumber(L, 2, -1), genre = (int)luaL_optnumber(L, 3, -1);
	int c = genre == 2 ? id : genre == 3 ? SCRIPT_PED_JOUEUR : -1;
	lua_pushboolean(L, pietons.Acceder(c) != nil && c != h && pietons.Voit(h, c, TirerVue, nil));
	return 1;
}

// La minuterie de mission (le composant du HUD qui l'affiche : +0xc la durée en ms, +0x10 son départ, +0x14 le temps écoulé à l'arrêt,
// l'octet +0x19 en marche, +0x1a en pause ; l'horloge DAT_00c1a9b4) :
// - MissionTimerStart(s) (0x5c1fb0) : FUN_0043f600(1) (en marche, plus en pause) puis FUN_0043f540 (la durée s × 1000, le départ
//   maintenant) ;
// - MissionTimerStop() (0x5c20c0) : FUN_0043f600(0) : en marche et pas en pause, le temps écoulé gardé ; arrêtée ;
// - le reste (FUN_0043f5a0) : en marche, la durée moins le temps écoulé (0 au-delà) ; sinon la durée moins le temps gardé ; en secondes
//   (× 0,001, _DAT_00900fa0) ; MissionTimerHasFinished (0x5c20d0) : le reste n'est pas positif ; MissionTimerGetTimeRemaining
//   (0x5c2120) : le reste arrondi. MissionTimerStartFixed (une heure de l'horloge du jeu) et MissionTimerPause ne sont pas suivies.
struct CMinuterieMission { uint32 duree = 0, depart = 0, ecoule = 0; bool enMarche = false, enPause = false; };
static CMinuterieMission minuterieMission;
static float
ResteMinuterie(void)
{
	const CMinuterieMission &m = minuterieMission;
	int64_t r;
	if(!m.enPause && m.enMarche){
		uint32 e = maintenant - m.depart;
		if(m.duree < e) return 0.0f;
		r = (int64_t)m.duree - e;
	}else r = (int64_t)m.duree - m.ecoule;
	return (float)r * 0.001f;
}

static int
MissionTimerStart(lua_State *L)
{
	Journal("MissionTimerStart", L, 1);
	minuterieMission.enPause = false; minuterieMission.enMarche = true;
	minuterieMission.duree = (uint32)((int16)luaL_optnumber(L, 1, 0) * 1000); minuterieMission.depart = maintenant;
	return 0;
}

static int
MissionTimerStop(lua_State *L)
{
	Journal("MissionTimerStop", L, 1);
	CMinuterieMission &m = minuterieMission;
	if(!m.enPause && m.enMarche) m.ecoule = maintenant - m.depart;
	m.enPause = false; m.enMarche = false;
	return 0;
}

static int
MissionTimerHasFinished(lua_State *L)
{
	appels["MissionTimerHasFinished"]++;
	lua_pushboolean(L, ResteMinuterie() <= 0.0f);
	return 1;
}

static int
MissionTimerGetTimeRemaining(lua_State *L)
{
	appels["MissionTimerGetTimeRemaining"]++;
	lua_pushnumber(L, (int)lrintf(ResteMinuterie()));
	return 1;
}

static void
CombatLoco(int ped, bool joueur, bool premiere)
{
	CScriptPed *p = pietons.Acceder(ped);
	if(p == nil || p->mort) return;
	if(joueur) pietons.Verrouiller(ped, SCRIPT_PED_JOUEUR, 0);
	int c = CibleDe(ped);
	const CScriptPed *q = pietons.Acceder(c);
	if(q == nil) return;
	CCombatLoco &e = etatsCombat[ped];
	if(premiere) e = CCombatLoco();
	e.tirage = TirageOpportunite(8, (uint16)ped, imageCourante) || e.forcer || premiere;
	e.forcer = false;
	if(e.tirage)
		for(int z = 0; z < 3; z++) if(pietons.JoindreZoneCombat(ped, c, z, false)) break;
	static const uint16 kPetits[5] = {40, 44, 48, 52, 52}, kGrands[5] = {44, 48, 52, 56, 56};
	int z = p->zoneCombat < 0 || p->zoneCombat > 3 ? 3 : p->zoneCombat;
	float petit = ButeIA(kPetits[z], 0.9f), grand = ButeIA(kGrands[z], 1.5f);
	int32 avant = e.sousEtat;
	ScriptPedCombatLocoChoisir(e, p->pos, q->pos, petit, grand, 33, pietons.Voit(ped, c, TirerVue, nil), TirerCombat, nil);   // FUN_0048d320
	if(e.sousEtat != avant && journalIA > 0){
		journalIA--;
		printf("%6.2f s  CombatLoco de %d : sous-état %d → %d, mode %d vers (%.2f, %.2f)\n", maintenant / 1000.0, ped, avant, e.sousEtat, e.mode, e.point.x, e.point.y);
	}
	// FUN_00625970
	float d2Cible = (q->pos.x - p->pos.x) * (q->pos.x - p->pos.x) + (q->pos.y - p->pos.y) * (q->pos.y - p->pos.y);
	if(e.tirage){
		CVector suivi(q->pos.x + e.decalage.x, q->pos.y + e.decalage.y, q->pos.z + e.decalage.z);
		float sx = suivi.x - p->pos.x, sy = suivi.y - p->pos.y;
		if(sx * sx + sy * sy - 0.25f > 0.0f) e.point = suivi;
	}
	float dx = e.point.x - p->pos.x, dy = e.point.y - p->pos.y, d = sqrtf(dx * dx + dy * dy);
	float vitesse = ScriptPedVitesseDuPas(e.mode, d2Cible, d * d, ButeIA(44, 1.5f));
	float ecart = remainderf(atan2f(dy, dx) - p->cap * 3.14159265f / 180.0f, 2.0f * 3.14159265f);
	p->intentionVitesse = ScriptPedVitesseDuTrajet(vitesse, d * d, ecart, 0.2f, p->immobile);
	if(d > 0 && p->intentionVitesse > 0) p->intention = CVector(dx / d, dy / d, 0);
	if(p->intentionVitesse > 0){
		float pas = ScriptPedVitesse(ALLURE_COURSE) * 0.033f * p->intentionVitesse / 1.5f;
		float k = d - 0.2f < pas ? (d - 0.2f) / d : pas / d;
		p->pos.x += dx * k; p->pos.y += dy * k;
	}
	if(e.mode == 3 || e.mode == 7 || e.mode == 12){ if(d > 0 && p->intentionVitesse > 0) p->cap = atan2f(dy, dx) * 180.0f / 3.14159265f; }
	else if(d2Cible > 0) p->cap = atan2f(q->pos.y - p->pos.y, q->pos.x - p->pos.x) * 180.0f / 3.14159265f;
}

// La laisse vers un piéton (le genre 2) : FUN_00499840 refait son point depuis le piéton tenu (+0x374) et le décalage +0x378
// (FUN_004994e0 : dans le repère du piéton tenu, la droite (sin cap, −cos cap) puis l'avant (cos cap, sin cap), supposé) ; sans
// piéton, la laisse tombe (genre 0). Le moteur l'appelle à chaque image depuis la mise à jour du piéton (l'appelant n'est pas
// lu) ; l'outil, depuis TetherLoco.
static void
RefaireLaisse(CScriptPed &p)
{
	if(p.laisse == 3){ p.laissePoint = p.pos; return; }   // le genre 3 : la position du piéton (FUN_004995c0 non suivi)
	if(p.laisse != 2) return;
	const CScriptPed *q = pietons.Acceder(p.laissePieton);
	if(q == nil){ p.laisse = 0; return; }
	float c = cosf(q->cap * 3.14159265f / 180.0f), s = sinf(q->cap * 3.14159265f / 180.0f);
	p.laissePoint = CVector(q->pos.x + p.laisseDecalage.x * s + p.laisseDecalage.y * c,
	                        q->pos.y - p.laisseDecalage.x * c + p.laisseDecalage.y * s, q->pos.z + p.laisseDecalage.z);
}

// FUN_00485490 (ecx = l'allié + 0x2f0, lu à l'assembleur de 0x485490 à 0x4857a0) : la place de l'allié à côté du premier meneur
// de sa chaîne ; rien avec +0x14ec. Le côté : le produit scalaire de la droite du premier meneur (FUN_004735a0, la première
// colonne de sa matrice, à plat, normée) et de la direction de son meneur direct vers lui (FUN_00414a20, normée), changé de
// signe : négatif, à droite de 1 + 0,35 m (le rayon des piétons, DAT_00cf311c, que 0x571500 pose depuis 0x90d0a8), sinon à
// gauche ; un allié tenu en laisse qui en est sorti (FUN_004997a0 : la distance au point de la laisse au-delà de son rayon)
// ajoute 1 m devant (DAT_00c2b1d8 … : (0, 1, 0)). Le point, dans le repère du premier meneur (FUN_0046dbd0), doit être sur
// le maillage de navigation (FUN_00485190, FUN_005e9df0), sinon la position du premier meneur : l'outil n'a pas le
// maillage, la place est gardée (supposé).
static bool
PlaceAupresDuMeneur(int ped, CVector &place)
{
	const CScriptPed *p = pietons.Acceder(ped);
	int r = pietons.PremierMeneur(ped);
	const CScriptPed *racine = pietons.Acceder(r), *direct = p ? pietons.Acceder(p->meneur) : nil;
	if(p == nil || racine == nil || direct == nil || p->groupe14ec != 0) return false;
	float c = cosf(racine->cap * 3.14159265f / 180.0f), s = sinf(racine->cap * 3.14159265f / 180.0f);
	float dx = racine->pos.x - direct->pos.x, dy = racine->pos.y - direct->pos.y, n = sqrtf(dx * dx + dy * dy);
	float produit = n > 0 ? -(s * dx / n - c * dy / n) : -0.0f;
	float cote = produit < 0.0f ? 1.35f : -1.35f, avant = 0;
	if(p->laisse != 0){
		float ex = p->pos.x - p->laissePoint.x, ey = p->pos.y - p->laissePoint.y, ez = p->pos.z - p->laissePoint.z;
		if(ex * ex + ey * ey + ez * ez > p->laisseRayon * p->laisseRayon) avant = 1.0f;
	}
	place = CVector(racine->pos.x + cote * s + avant * c, racine->pos.y - cote * c + avant * s, racine->pos.z);
	return true;
}

// La mise à jour d'une piste TetherLoco (FUN_00627fc0, lue à l'assembleur ; le lancement FUN_00627f10, lu de 0x627f10 à
// 0x627fb2) : sans laisse, rien (la piste rend faux). Le point visé : la cible si l'octet +0x18 et l'octet +0x3b4 du piéton,
// sinon le point de la laisse (FUN_00499460) ou, si le piéton tenu est le premier meneur de sa chaîne, la place à côté de lui
// (FUN_00485490). La vitesse : la distance au-delà de l'arrivée (+0x28) sur le rayon de la laisse (+0x364), plus, avec l'octet
// +0x2f et au-delà du seuil +0x34, si le piéton va vers le point (le produit scalaire de son avant et de l'écart au-dessus de
// 0,45, le double 0x900e48), le gain +0x30 fois le chemin du point depuis l'image d'avant sur 0,02 × le pas de l'image
// (0xb0c0b0 fois CTimer::ms_fTimeStep, DAT_00c1a9a4, en cinquantièmes de seconde : 33 / 20 à 30 images par seconde, soit le pas en
// secondes) ; bornée à 1,5 (0x90fb9c) ; moyennée sur les trente dernières images positives (+0x44 …
// +0xb8) sauf avec l'octet +0x2e, tant que le piéton tenu bouge (FUN_00424560, sa vitesse : l'intention de l'outil) ; la
// moyenne est vidée sinon. La navigation (FUN_004a4eb0, l'arrivée +0x28 ; les drapeaux 0x20014, 0x1c avec +0x14ec) le mène
// (+0x40) ; sous 0,1 (0x900128) il s'arrête. Arrêté, avec l'octet +0x2c, il se tourne vers le premier meneur (FUN_004a4750).
// L'arrêt de la piste sur un meneur occupé (FUN_00487990(meneur, 5, 1), ou +0x548 du meneur dans la laisse) n'est pas lu :
// supposé faux. Le maillage de navigation, la recherche de chemin et l'octet +0x38 ne sont pas suivis.
static bool
TetherLoco(int ped, CActionController::Laisse &l, bool premiere)
{
	CScriptPed *p = pietons.Acceder(ped);
	if(p == nil || p->mort) return false;
	if(premiere){
		l.enMarche = false;
		if(p->laisse != 0){
			p->laisseDecalage = CVector(l.dx, l.dy, 0);
			if(l.rayon > 0){ l.rayonAvant = p->laisseRayon; p->laisseRayon = l.rayon; }
			RefaireLaisse(*p);
			l.px = p->laissePoint.x; l.py = p->laissePoint.y; l.pz = p->laissePoint.z;
		}
	}
	RefaireLaisse(*p);
	if(p->laisse == 0) return false;
	const CScriptPed *tenu = p->laisse == 2 ? pietons.Acceder(p->laissePieton) : nil;
	CVector vise = p->laissePoint;
	const CScriptPed *cible = l.versCible && p->laisse3b4 ? pietons.Acceder(CibleDe(ped)) : nil;
	if(cible) vise = cible->pos;
	else if(tenu && p->laissePieton == pietons.PremierMeneur(ped)) PlaceAupresDuMeneur(ped, vise);
	float dx = vise.x - p->pos.x, dy = vise.y - p->pos.y, d = sqrtf(dx * dx + dy * dy);
	float exces = d - l.arrivee > 0 ? d - l.arrivee : 0;
	float vit = p->laisseRayon > 0 ? exces / p->laisseRayon : 0;
	if(l.suitVitesse && d > l.seuil){
		float c = cosf(p->cap * 3.14159265f / 180.0f), s = sinf(p->cap * 3.14159265f / 180.0f);
		if(c * dx + s * dy > 0.45f){
			float mx = vise.x - l.px, my = vise.y - l.py, mz = vise.z - l.pz;
			vit += l.gain * sqrtf(mx * mx + my * my + mz * mz) / (0.02f * (33.0f / 20.0f));
		}
	}
	if(vit > 1.5f) vit = 1.5f;
	bool bouge = tenu == nil || tenu->intentionVitesse > 0;
	if(vit > 0 && bouge && !l.sansMoyenne){
		l.moyenne[l.indice] = vit;
		if(++l.indice > 29) l.indice = 0;
		int n = 0; float somme = 0;
		for(float m : l.moyenne) if(m > 0){ n++; somme += m; }
		if(n > 0) vit = somme / n;
	}else if(!(vit > 0 && bouge)){
		for(float &m : l.moyenne) m = 0;
		l.indice = 0;
	}
	float ecart = remainderf(atan2f(dy, dx) - p->cap * 3.14159265f / 180.0f, 2.0f * 3.14159265f);
	float v = ScriptPedVitesseDuTrajet(vit, d * d, ecart, l.arrivee, p->immobile);   // FUN_004a4eb0 → FUN_005e4a60
	bool avant = l.enMarche;
	l.enMarche = v > 0 && vit >= 0.1f;
	static int journalLaisses = 400;   // les départs et arrêts journalisés (les premiers)
	if(l.enMarche != avant && journalLaisses > 0){
		journalLaisses--;
		printf("%6.2f s  TetherLoco de %d, en (%.2f, %.2f, %.2f) : %s (%.2f, %.2f, %.2f), à %.2f m, vitesse %.2f\n", maintenant / 1000.0,
		       ped, p->pos.x, p->pos.y, p->pos.z, l.enMarche ? "en marche vers" : "arrêté près de", vise.x, vise.y, vise.z, d, v);
	}
	p->intentionVitesse = l.enMarche ? v : 0;
	if(l.enMarche){
		p->intention = CVector(dx / d, dy / d, 0);
		float pas = ScriptPedVitesse(ALLURE_COURSE) * 0.033f * v / 1.5f;
		float k = d - l.arrivee < pas ? (d - l.arrivee) / d : pas / d;
		// L'outil n'a ni sol ni escaliers : la hauteur suit celle du point visé au même pas (supposé ; dans le moteur, la
		// navigation et la physique le portent sur le sol, ce qui fait monter Algie à l'étage derrière le joueur).
		p->pos.x += dx * k; p->pos.y += dy * k; p->pos.z += (vise.z - p->pos.z) * k;
		p->cap = atan2f(dy, dx) * 180.0f / 3.14159265f;
	}else if(l.b2c && tenu && p->laissePieton == pietons.PremierMeneur(ped)){
		float fx = tenu->pos.x - p->pos.x, fy = tenu->pos.y - p->pos.y;
		if(fx * fx + fy * fy > 0) p->cap = atan2f(fy, fx) * 180.0f / 3.14159265f;
	}
	l.px = vise.x; l.py = vise.y; l.pz = vise.z;
	return true;
}

// Les pistes Death qui se lancent (FUN_0060fdc0 → l'emplacement 42 de CPed, FUN_00479110) : le piéton meurt
// (CScriptPedPool::Mourir), sauf déjà mort ou immortel.
static void
AppliquerMorts(int ped, CActionController &c)
{
	for(const CActionController::Mort &d : c.mortsLancees)
		if(pietons.Mourir(ped, maintenant))
			printf("%6.2f s  piéton %d : piste Death (%d), il meurt, %.0f points de vie\n", maintenant / 1000.0, ped, d.octet,
			       pietons.Acceder(ped)->sante);
	c.mortsLancees.clear();
}

// Les pistes DamageTarget (FUN_0060fad0, FUN_0060fbe0) : le partenaire de prise du piéton (FUN_0060fc60, +0x155c) subit les
// dégâts de l'image, portés par le piéton (FUN_0047b7f0 → FUN_0047b6b0 : CScriptPedPool::Blesser, sans relevé de coup).
static void
AppliquerDegatsCible(int ped, CActionController &c)
{
	const CScriptPed *p = pietons.Acceder(ped);
	for(float v : c.coupsCible){
		const CScriptPed *q = p ? pietons.Acceder(p->cibleDePrise) : nil;
		if(q == nil || v == 0.0f) continue;
		float avant = q->sante;
		bool porte = pietons.Blesser(p->cibleDePrise, v, ped);
		printf("%6.2f s  DamageTarget de %d sur %d : %.2f, %s, %.2f → %.2f points de vie\n", maintenant / 1000.0, ped, p->cibleDePrise, v,
		       porte ? "porté" : "rien", avant, q->sante);
	}
	c.coupsCible.clear();
}

// SetWeaponInfo (FUN_00660f50) : sur l'objet arme, +0x120 = le champ +0x2c (44), +0x124 = +0x40 (64), +0x12c = l'arme du nom +0x44
// (68, FUN_0051ad50) ou, sans nom, le modèle de l'arme.
static void
PoserInfoArme(CScriptPed &p, const CActionController::InfoArme &d)
{
	p.armeInit = d.init;
	p.armePortee = d.champ40;
	p.armeMunition = d.munition.empty() ? p.armeTenue : ArmeParNom(d.munition);
}

// FUN_00664710, réduit : le départ, la cible et la vitesse, puis le projectile (FUN_004d4c40).
// - le départ (FUN_0065e880) : la douille « Muzzle » de l'objet arme (FUN_00467c30), ou pour une arme lancée l'os qu'elle tient
//   (+0x138, FUN_006c1030) ; sans eux, (0, 0, 1) par la matrice du piéton (FUN_0046dbd0, FUN_0045d3b0) ; puis le décalage de la
//   piste WeaponFireOffset (DAT_01b65c30). L'outil n'a ni douilles ni os : 1 m au-dessus des pieds du tireur ;
// - la cible (FUN_00662a40) : la cible visée du composant de visée (FUN_004d5980, FUN_004d7630, FUN_0048bdb0) ; sans elle, devant
//   le piéton à la portée +0x124 + charge × (+0x120 − +0x124), au moins 0,01 (_DAT_0090054c). L'outil prend sa cible verrouillée,
//   à 1 m au-dessus de ses pieds ;
// - la vitesse (FUN_0065f050, la première passe) : la vitesse de la fiche, +0x60 + charge × (+0x64 − +0x60) (0,4, _DAT_0090048c,
//   sans fiche ; 0,01 au moins), la durée du vol T = la distance / elle (0,01 au moins), la gravité g = +0x80 × −0,008
//   (_DAT_00912870), et v = (cible − départ − (0, 0, ½ g T²)) / T, en mètres par pas ; les bornes +0x68 … +0x74 (-1 pour la bille)
//   ne sont pas suivies ;
// - les dégâts : +0x4c + charge × (+0x50 − +0x4c), × la statistique 31 / _DAT_00900130 (non suivi).
static void
Tirer(int ped, float charge)
{
	CScriptPed *p = pietons.Acceder(ped);
	if(p == nil) return;
	int32 munition = p->armeMunition;
	if(munition == -1) return;   // FUN_0065d160 : pas de munition, pas de tir
	const CTypeProjectile &tp = TypeProjectile(munition);
	CVector depart(p->pos.x, p->pos.y, p->pos.z + 1.0f), cible;
	const CScriptPed *c = pietons.Acceder(p->verrou);
	const CObjetOutil *o = ped == SCRIPT_PED_JOUEUR ? ObjetParPoignee(objetVise) : nil;
	if(o && !o->detruit) cible = o->pos;
	else if(c && c != p) cible = CVector(c->pos.x, c->pos.y, c->pos.z + 1.0f);
	else{
		float portee = p->armePortee + charge * (p->armeInit - p->armePortee);
		if(portee < 0.01f) portee = 0.01f;
		float a = p->cap * 3.14159265f / 180.0f;
		cible = CVector(depart.x + cosf(a) * portee, depart.y + sinf(a) * portee, depart.z);
	}
	float vitesse = tp.vitesseMin + charge * (tp.vitesseMax - tp.vitesseMin);
	if(vitesse < 0.01f) vitesse = 0.01f;
	float dx = cible.x - depart.x, dy = cible.y - depart.y, dz = cible.z - depart.z;
	float T = sqrtf(dx * dx + dy * dy + dz * dz) / vitesse;
	if(T <= 0.01f) T = 0.01f;
	float g = tp.gravite * -0.008f;
	CProjectileOutil q;
	q.actif = true; q.modele = munition; q.tireur = ped; q.pos = depart; q.gravite = g;
	q.vitesse = CVector(dx / T, dy / T, (dz - 0.5f * g * T * T) / T);
	q.degats = tp.degatsMin + charge * (tp.degatsMax - tp.degatsMin);
	q.naissance = maintenant; q.vie = tp.vie > 0 ? (uint32)tp.vie : 5000;
	projectiles.push_back(q);
	printf("%6.2f s  le piéton %d tire : projectile %d (%s) de (%.2f, %.2f, %.2f) vers (%.2f, %.2f, %.2f), %.2f m par pas, %.0f points\n",
	       maintenant / 1000.0, ped, munition, NomDArme(munition) ? NomDArme(munition)->c_str() : "?", depart.x, depart.y, depart.z,
	       cible.x, cible.y, cible.z, vitesse, q.degats);
}

// WeaponFire (FUN_00661f10) lancée à cette image.
static void
AppliquerTirs(int ped, CActionController &c)
{
	CScriptPed *p = pietons.Acceder(ped);
	for(const CActionController::Tir &d : c.tirsLances){
		if(p == nil || p->armeTenue == -1) continue;   // FUN_0065d160 sans arme en main
		float charge = 0.0f;   // l'objet arme +0x128 (WeaponSetCharge, WeaponIncrementCharge, non jouées) vaut -100 (_DAT_00906520) : 0
		(void)d;
		Tirer(ped, charge);
		if(p->drapeaux[24] == 0) inventaire.Retirer(armes, ped, p->armeMunition, 1, p->type == SCRIPT_PED_TYPE_JOUEUR, modeles.Index("RBandBall"));
		if(d.lacher && p->armeTenue == p->armeMunition && inventaire.Munitions(armes, ped, p->armeMunition) == 0) LacherArme(ped);
	}
	c.tirsLances.clear();
}

// Le segment a → b passe-t-il dans la sphère (c, r) ? La fraction du segment au point le plus proche, dans `u`.
static bool
SegmentDansSphere(const CVector &a, const CVector &b, const CVector &c, float r, float *u)
{
	float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z, l2 = dx * dx + dy * dy + dz * dz;
	float t = l2 > 0.0f ? ((c.x - a.x) * dx + (c.y - a.y) * dy + (c.z - a.z) * dz) / l2 : 0.0f;
	t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	float px = a.x + dx * t - c.x, py = a.y + dy * t - c.y, pz = a.z + dz * t - c.z;
	*u = t;
	return px * px + py * py + pz * pz <= r * r;
}

// Un projectile qui touche un piéton : la méthode du coup du piéton (l'emplacement 15 de CPed, FUN_0047b810), comme un coup porté,
// avec l'arme du projectile (sans le facteur du joueur ni les arrondis, réservés aux coups sans arme) ; le relevé du coup reçu, la
// descente dans l'arbre des coups, dont les pistes DamageHandle ôtent les dégâts, et l'événement 0 des scripts (FUN_005b2190).
static void
RecevoirProjectile(int cible, int tireur, float degats, int32 arme)
{
	CoupRecu &r = coups[cible];
	float t = maintenant / 1000.0f;
	r.precedent = r.instant >= 0.0f ? t - r.instant : -1.0f;
	r.instant = t;
	r.drapeau = 0; r.jusqua = t + 1.0f; r.degats = degats; r.attaquant = tireur; r.arme = arme; r.genre = 0;
	CActionController &v = controleurs[cible];
	const CActionNode *arbreCoups = arbres.Trouver("/Global/HitTree");
	bool pris = arbreCoups && v.Entrer(arbres, arbreCoups, CJugeOutil(cible, t), t);
	printf("%6.2f s  le projectile de %d touche %d (%.0f points)%s%s\n", maintenant / 1000.0, tireur, cible, degats,
	       pris ? " : il joue " : "", pris ? CheminVu(v, v.joue).c_str() : "");
	if(pietons.CoupRecevable(cible, tireur)) PrevenirCoupRecu(cible, tireur);
}

// FUN_005b3530 (depuis FUN_004d0300, la destruction d'un CObject) : le gestionnaire global de l'événement 1 de la table 0 (FUN_005b1ce0,
// FUN_005b1c70), puis le rappel de l'événement 3 du haché de l'objet (+0x228 ; FUN_005b38b0, FUN_005b3830), appelé dans
// un thread neuf de son objet script (FUN_005d9ac0) avec le haché (un pointeur léger, thunk_FUN_0073b520) et la poignée de l'objet
// (FUN_005d8460) ; l'outil l'appelle d'un bloc (lua_pcall) dans le script qui l'a posé.
static void AppelerGlobal(int e, int table, const char *quoi, void (*pousser)(lua_State *, const void *), const void *donnees);
static void
PrevenirObjetDetruit(size_t i)
{
	const CObjetOutil &o = objets[i];
	struct P { uint32 h; int p; } paire{o.nom, PoigneeObjet(i)};
	AppelerGlobal(1, 0, "un objet détruit", [](lua_State *L, const void *d){ const P *p = (const P *)d;
	              lua_pushlightuserdata(L, (void *)(uintptr_t)p->h); lua_pushnumber(L, p->p); }, &paire);
	for(const CGestionnaireHache &g : gestionnairesHaches){
		if(g.nom != o.nom || g.rappels[3] == LUA_NOREF || etatLua == nil) continue;
		lua_State *L = etatLua;
		printf("%6.2f s  le rappel de l'événement 3 du haché %08x est appelé (objet %d)\n", maintenant / 1000.0, o.nom, PoigneeObjet(i));
		lua_rawgeti(L, LUA_REGISTRYINDEX, g.rappels[3]);
		lua_pushlightuserdata(L, (void *)(uintptr_t)o.nom);
		lua_pushnumber(L, PoigneeObjet(i));
		if(lua_pcall(L, 2, 0, 0) != 0){ printf("        rappel de l'objet : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
		break;
	}
}

// Un coup sur un CObject : ses points de vie baissent des dégâts, l'auteur gardé en +0x224 ; à 0, FUN_004d0300 le détruit (rien si
// le bit 6 de +0x209 est posé : ObjectToggleVulnerability ; ses effets, ses sons et son remplaçant ne sont pas suivis), puis
// FUN_005b3530 prévient les scripts. DestroyByWeaponOnly (+2 de la ligne) : un projectile est une arme.
static void
FrapperObjet(size_t i, int auteur, float degats)
{
	CObjetOutil &o = objets[i];
	if(o.detruit) return;
	if(o.invulnerable){
		printf("%6.2f s  le projectile de %d touche l'objet %d (modèle %d, %08x) : invulnérable\n", maintenant / 1000.0, auteur, PoigneeObjet(i), o.modele, o.nom);
		return;
	}
	o.sante -= (int32)ceilf(degats);
	o.destructeur = auteur;
	printf("%6.2f s  le projectile de %d touche l'objet %d (modèle %d, %08x) : %d points de vie\n", maintenant / 1000.0, auteur, PoigneeObjet(i),
	       o.modele, o.nom, o.sante > 0 ? o.sante : 0);
	if(o.sante > 0) return;
	o.detruit = true;
	printf("%6.2f s  l'objet %d (modèle %d, %08x) est détruit par %d\n", maintenant / 1000.0, PoigneeObjet(i), o.modele, o.nom, auteur);
	PrevenirObjetDetruit(i);
}

// Une image des projectiles (CProjectileInfo, à chaque pas) : le point avance de sa vitesse, la gravité s'y ajoute ; au bout de sa vie,
// il disparaît. Les contacts : le corps des piétons, le modèle de collision que CTempColModels::Initialise (FUN_005712f0) leur bâtit,
// trois sphères de 0,35 m à 0,35, 0,95 et 1,55 m au-dessus des pieds (le tireur exclu) ; le premier touché le long du pas.
static void
AvancerProjectiles(float pas)
{
	for(CProjectileOutil &q : projectiles){
		if(!q.actif) continue;
		if(maintenant - q.naissance > q.vie){ q.actif = false; continue; }
		CVector avant = q.pos;
		q.pos.x += q.vitesse.x * pas; q.pos.y += q.vitesse.y * pas; q.pos.z += q.vitesse.z * pas;
		q.vitesse.z += q.gravite * pas;
		int touche = -1; float mieux = 2.0f;
		for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++){
			const CScriptPed *p = pietons.Acceder(i);
			if(p == nil || i == q.tireur || p->mort) continue;
			for(float h : {0.35f, 0.95f, 1.55f}){
				float u;
				if(SegmentDansSphere(avant, q.pos, CVector(p->pos.x, p->pos.y, p->pos.z + h), 0.35f, &u) && u < mieux){ mieux = u; touche = i; }
			}
		}
		// Les objets cassables : une sphère de 0,5 m autour de leur position (supposé : l'outil ne lit pas leur modèle de collision).
		int objet = -1;
		for(size_t i = 0; i < objets.size(); i++){
			float u;
			if(!objets[i].detruit && SegmentDansSphere(avant, q.pos, objets[i].pos, 0.5f, &u) && u < mieux){ mieux = u; objet = (int)i; touche = -1; }
		}
		if(objet >= 0){
			q.actif = false;
			CVector a(avant.x + (q.pos.x - avant.x) * mieux, avant.y + (q.pos.y - avant.y) * mieux, avant.z + (q.pos.z - avant.z) * mieux);
			PoserMarque(TypeProjectile(q.modele).marque, a);   // la marque de l'impact (FUN_00560f10 la comptera)
			FrapperObjet((size_t)objet, q.tireur, q.degats);
		}else if(touche >= 0){
			q.actif = false;
			RecevoirProjectile(touche, q.tireur, q.degats, q.modele);
		}
	}
	size_t n = 0;
	for(const CProjectileOutil &q : projectiles) if(q.actif) projectiles[n++] = q;
	projectiles.resize(n);
}

// L'arbre du piéton (+0x4b0 du composant +0x2e0) que PropDetach, PedClearAllWeapons et PedDestroyWeapon rendent au contexte
// quand l'arme l'avait changé (l'octet +0x33c) : l'outil garde l'arbre posé avant l'arme.
static std::map<int, const CActionNode *> arbresAvantArme;
static void
RendreArbreAvantArme(int ped)
{
	auto it = arbresAvantArme.find(ped);
	if(it == arbresAvantArme.end()) return;
	CActionController &c = controleurs[ped];
	const CActionNode *arme = c.arbre;
	c.arbre = it->second;
	c.Racine(arbres, it->second);
	arbresAvantArme.erase(it);
	float t = maintenant / 1000.0f;   // comme à la prise : revenu dans l'arbre de l'arme, il redescend depuis celui du piéton
	if(arme && c.joue && c.arbre && arbres.Sous(arme, c.joue) && c.Poser(arbres, c.arbre, CJugeOutil(ped, t), t))
		printf("%6.2f s  contrôleur de %d : %s\n", maintenant / 1000.0, ped, arbres.CheminDe(c.joue).c_str());
}

// FUN_0051adb0 : l'arme d'un nom haché parmi les armes (ms_weaponModelStore, le +4 de chacune) ; -1 pour 0, « unarmed » ou un
// nom inconnu.
static int32
ArmeParHache(uint32 h)
{
	h &= 0x7fffffff;
	if(h == 0 || h == ActionHash("unarmed")) return -1;
	for(const auto &a : modeles.armes) if(ActionHash(a.second.c_str()) == h) return a.first;
	return -1;
}

// Le HUD des armes (HUDWeapons, le composant 20, DAT_00c674f0) : son arme (+0xc0), posée par FUN_0055c6f0 : -1 si le joueur n'a pas
// de munitions de cette arme (FUN_0045cdf0), sauf RBandBall (DAT_00bf432c). WeaponAmmo (avec l'octet +0xc) et
// WeaponDesiredIsFocusWeapon la lisent ; l'octet +0x13c (PlayerWeaponHudLock) verrouille le choix.
static void
PoserArmeHud(int32 a)
{
	armeHud = a;
	if(a != -1 && inventaire.Munitions(armes, SCRIPT_PED_JOUEUR, a) == 0 && a != modeles.Index("RBandBall")) armeHud = -1;
}

// Les pistes de l'arme en main lancées à cette image (CActionController::pistesArmeLancees).
static void
AppliquerArmes(int ped, CActionController &c)
{
	CScriptPed *p = pietons.Acceder(ped);
	if(p == nil){ c.pistesArmeLancees.clear(); return; }
	for(const CActionController::PisteArme &d : c.pistesArmeLancees){
		if(d.genre == CActionController::ARME_VOULUE_POSER){   // FUN_00660770
			p->armeVoulue = (d.nom & 0x7fffffff) != ActionHash("NoWeapon") ? ArmeParHache(d.nom) : -1;
			if(d.octet) PoserArmeHud(p->armeVoulue);
		}else if(d.genre == CActionController::ARME_PASSAGE){   // FUN_00660840
			p->armePassage = ArmeParHache(d.nom);
		}else if(d.genre == CActionController::ARME_VOULUE_PRENDRE){   // FUN_00660870
			p->armePassage = p->armeVoulue;
			p->armeVoulue = 0;
		}else if(d.genre == CActionController::ARME_ATTACHER){   // FUN_0065fad0
			if(p->armePassage == -1 || p->armePassage == 0) continue;
			p->armeTenue = p->armePassage;   // FUN_004d92c0 puis FUN_004da6f0 : les modèles d'armes sont toujours là dans l'outil
			p->armeInit = -1.0f;             // FUN_004d92c0 : +0x120 et +0x124 à -1
			const CTypeArme &t = armes.Type(p->armeTenue);
			printf("%6.2f s  le piéton %d prend en main l'arme %d (%s)\n", maintenant / 1000.0, ped, p->armeTenue,
			       modeles.armes.count(p->armeTenue) ? modeles.armes[p->armeTenue].c_str() : "?");
			if(d.octet && !t.arbre.empty()){
				const CActionNode *n = arbres.Trouver(t.arbre.c_str());
				if(n){
					CActionController &k = controleurs[ped];
					const CActionNode *avant = k.arbre;
					if(!arbresAvantArme.count(ped)) arbresAvantArme[ped] = avant;
					k.arbre = n;
					k.Racine(arbres, n);
					printf("%6.2f s  contexte des actions de %d : %s\n", maintenant / 1000.0, ped, t.arbre.c_str());
					// L'outil lance les pistes d'un nœud à l'image qui suit son entrée : un nœud court (Weapons/SelectActions/
					// WeaponSelect/select/default, 0,067 s) a déjà rendu la main à l'arbre d'avant, d'où le moteur le ferait
					// redescendre depuis l'arbre de l'arme. Le contrôleur redescend donc depuis lui s'il est revenu dans l'ancien.
					float tt = maintenant / 1000.0f;
					if(avant && k.joue && arbres.Sous(avant, k.joue) && k.Poser(arbres, n, CJugeOutil(ped, tt), tt))
						printf("%6.2f s  contrôleur de %d : %s\n", maintenant / 1000.0, ped, arbres.CheminDe(k.joue).c_str());
				}
			}
		}else if(d.genre == CActionController::ARME_DETACHER){   // FUN_00660640
			if(p->drapeaux[21] != 0){
				p->armePassage = p->armeTenue;
				if(p->armeTenue != -1) printf("%6.2f s  le piéton %d range l'arme %d\n", maintenant / 1000.0, ped, p->armeTenue);
				p->armeTenue = -1;
				p->armeInit = -1.0f;
				if(d.octet) RendreArbreAvantArme(ped);
			}else p->armePassage = -1;
		}
	}
	c.pistesArmeLancees.clear();
}

// Les pistes TargetAttackRightsAcquire et TargetAttackRightsRelease lancées à cette image (FUN_0062a1a0, FUN_0062a1e0).
static void
AppliquerDroits(int ped, CActionController &c)
{
	for(bool prendre : c.droitsLances){
		const CScriptPed *p = pietons.Acceder(ped);
		int cible = p ? p->verrou : -1;
		bool fait = prendre ? pietons.PrendreDroits(ped, maintenant) : pietons.RendreDroits(ped);
		if(fait) printf("%6.2f s  le piéton %d %s les droits d'attaque sur %d\n", maintenant / 1000.0, ped, prendre ? "prend" : "rend", cible);
	}
	c.droitsLances.clear();
	for(uint32 i : c.desLances) c.des[i & 3] = (float)RandCrt(graineIA) / 32767.0f;   // FUN_00632810
	c.desLances.clear();
	if(CScriptPed *p = pietons.Acceder(ped))   // FUN_00474840 : DAT_00c1a9b4 × 0,001 (le flottant 0x900fa0) + la durée
		for(float d : c.minuteriesLancees) p->minuterieAction = (float)maintenant * 0.001f + d;
	c.minuteriesLancees.clear();
	// Script (FUN_006851a0) : la fonction dans son espace de noms (AppelScript, comme FUN_005d80a0) ; sans mission en cours,
	// « mission » ne mène à rien (FUN_006aa630 rend -1, le lancement s'arrête).
	std::vector<CActionController::AppelScript> appels;
	appels.swap(c.appelsLances);
	for(const CActionController::AppelScript &a : appels){
		printf("%6.2f s  le piéton %d appelle %s dans %s (piste Script)\n", maintenant / 1000.0, ped, a.fonction.c_str(), a.script.c_str());
		int declencheur = -2;
		if(a.avecDeclencheur){
			const CObjetAnime *o = objetsAnimes.Objet(CJugeOutil(ped, maintenant / 1000.0f).ObjetDuContexte());
			declencheur = o ? o->declencheur : -1;
		}
		AppelScript(a.script, a.fonction, a.avecPieton ? ped : -1, a.nombre, a.avecNombre, declencheur);
	}
}

// Une image du contrôleur de l'IA d'un piéton (FUN_00484490), puis ce que font ses pistes et celles des contrôleurs de
// ses pistes spawn : l'intention (ActionNodeRequest), ManageTargets, CombatLoco, FaceTarget, SetPedFlags.
static void
AvancerIA(int ped, CActionController &c, uint32 images)
{
	CScriptPed *p = pietons.Acceder(ped);
	if(p == nil || c.arbre == nil) return;
	// Mort, endormi, ou tous endormis : l'IA ne tourne pas et les entrées du piéton sont vidées (FUN_005f38e0) ; mort, son
	// contrôleur s'arrête (FUN_005f3f40). Sinon la vitesse de l'intention de mouvement reste celle que la dernière piste
	// de navigation a posée : aucune remise à 0 à chaque image (les 35 écritures de +0x1c relevées à l'assembleur, et le
	// +0x1c de FUN_005f38e0, sont toutes dans des pistes, la navigation ou ces cas-là).
	if(!pietons.IAAvance(ped)){ pietons.ViderEntrees(ped); boutonsPietons.erase(ped); if(p->mort) c.Jouer(arbres, nil, maintenant / 1000.0f); return; }
	float t = maintenant / 1000.0f;
	c.image = images; c.graine = (uint16)ped;
	const CActionNode *avant = c.joue;
	if(c.joue == nil) c.Poser(arbres, c.arbre, CJugeOutil(ped, t), t);   // FUN_00483ec0
	else c.Avancer(arbres, nil, CJugeOutil(ped, t), t);
	if(c.joue != avant && journalIA > 0){ journalIA--; printf("%6.2f s  IA de %d : %s\n", t, ped, CheminVu(c, c.joue).c_str()); }
	std::vector<CActionController *> tous;
	c.Recueillir(tous);
	static std::map<const CActionController *, const CActionNode *> vus;   // LANCER_IA : les passages des contrôleurs des spawn
	if(getenv("LANCER_IA"))
		for(size_t i = 1; i < tous.size(); i++)
			if(vus[tous[i]] != tous[i]->joue){ vus[tous[i]] = tous[i]->joue; printf("%6.2f s  IA de %d, spawn : %s\n", t, ped, CheminVu(*tous[i], tous[i]->joue).c_str()); }
	for(CActionController *k : tous){
		if(k->reposLances > 0){ pietons.ViderEntrees(ped); intentionsPietons.erase(ped); boutonsPietons.erase(ped); }   // IDLE : FUN_0061d8e0
		for(const auto &a : k->appuis){   // FUN_006260d0
			AppuyerBouton(ped, a.first, a.second);
			if(a.first == 12) instantsParade[ped] = maintenant;
		}
		for(int32 b : k->laches) AppuyerBouton(ped, b, 3);   // FUN_00626160
		k->laches.clear();
		for(uint32 n : k->intentions) intentionsPietons[ped] = n;
		for(const CActionController::Gestion &g : k->gestionsActives) GererCibles(ped, g.joueur);
		for(const CActionController::Gestion &g : k->combatsActifs) CombatLoco(ped, g.joueur, k->Temps(t) <= g.entree + 0.034f);
		for(CActionController::Laisse &l : k->laisses){   // FUN_00627fc0 à chaque image
			if(k->Temps(t) < l.entree || l.vie == -2.0f) continue;
			if(!TetherLoco(ped, l, !l.lancee)) l.vie = -2.0f;
			l.lancee = true;
		}
		for(const CActionController::Regard &r : k->regardsActifs){   // FUN_00620dc0
			const CScriptPed *q = pietons.Acceder(r.genre == 0 ? CibleDe(ped) : r.genre == 1 ? p->cibleDePrise : -1);
			if(q && q != p) p->cap = atan2f(q->pos.y - p->pos.y, q->pos.x - p->pos.x) * 180.0f / 3.14159265f + r.decalage;
		}
		for(const CActionController::Drapeau &d : k->drapeauxPoses) if(d.numero < SCRIPT_PED_DRAPEAUX) p->drapeaux[d.numero] = d.valeur;
		k->drapeauxPoses.clear();
		AppliquerMorts(ped, *k);
		AppliquerDegatsCible(ped, *k);
		AppliquerDroits(ped, *k);
		AppliquerArmes(ped, *k);   // avant SetWeaponInfo : PropAttach remet +0x120, la banque NeedInit qui suit le pose
		for(const CActionController::InfoArme &d : k->infosArmeLancees) PoserInfoArme(*p, d);   // FUN_00660f50
		AppliquerTirs(ped, *k);
		AppliquerObjectifs(ped, *k);
	}
}

// FUN_006d6f60 pour chaque piéton du pool (le joueur compris), puis les rappels des événements rendus : FUN_005b1ec0 pour
// chaque table où l'événement a un rappel ; FUN_005b1db0 l'appelle dans un thread neuf de l'objet script du rappel
// (FUN_005d9ac0, FUN_005d8140) avec la poignée du déclencheur (FUN_006d7910) et celle du piéton (l'emplacement 43 de CPed,
// +0xac). L'outil l'appelle d'un bloc (lua_pcall), dans le script qui l'a posé : un rappel qui attendrait (Wait) n'est pas
// suivi. Les paires d'un piéton supprimé sont oubliées (supposé : le destructeur de CPed n'est pas lu).
static const char *const kEvenementsDeclencheur[] = {"un piéton entre", "le joueur entre", "un projectile", "un piéton sort",
                                                     "le joueur sort", "dedans"};

// (joueurPose : déclaré avec les ramassages.)

static void
SuivreDeclencheurs(void)
{
	auto &d = declencheursEv.dedans;
	for(size_t j = 0; j < d.size(); ) if(pietons.Acceder(d[j].ped) == nil) d.erase(d.begin() + j); else j++;
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE && etatLua; i++){
		const CScriptPed *p = pietons.Acceder(i);
		if(p == nil || (i == SCRIPT_PED_JOUEUR && !joueurPose)) continue;
		CVector pos = p->pos;
		std::vector<CGestionnairesDeclencheurs::Appel> appels = declencheursEv.Suivre(i, i == SCRIPT_PED_JOUEUR,
			[&](int t){ return donnees.DansDeclencheur(t, pos); },
			[&](int t){ return declencheursJoueurSeul.count(t) != 0; });
		for(const CGestionnairesDeclencheurs::Appel &a : appels){
			int *c = declencheursEv.Case(a.table, a.declencheur, a.evenement);
			if(c == nil || *c == CGestionnairesDeclencheurs::SANS_RAPPEL) continue;
			int script = -1;
			for(const CGestionnairesDeclencheurs::Entree &n : declencheursEv.tables[a.table].entrees)
				if(n.declencheur == a.declencheur) script = n.scripts[a.evenement];
			if(a.evenement != CGestionnairesDeclencheurs::DEDANS){
				const CTriggerZone *z = donnees.ZoneParHandle(a.declencheur);
				printf("%6.2f s  déclencheur %d (%s) : %s (%d), le rappel de la table %d est appelé\n", maintenant / 1000.0,
				       a.declencheur, z ? z->nom.c_str() : "?", kEvenementsDeclencheur[a.evenement], i, a.table);
			}
			lua_State *L = etatLua;
			int avant = scriptCourant;
			if(script >= 0) scriptCourant = script;
			lua_rawgeti(L, LUA_REGISTRYINDEX, *c);
			lua_pushnumber(L, a.declencheur); lua_pushnumber(L, i);
			if(lua_pcall(L, 2, 0, 0) != 0){ printf("        rappel du déclencheur : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
			scriptCourant = avant;
		}
	}
}

// Une image des piétons : les gestes avancent, et ce qui aboutit se journalise.
static int TirerVue(void *);
static void
AvancerPietons(void)
{
	pietons.Avancer(0.033f);
	if(etatLua) AvancerSuivis(etatLua);   // les pistes FollowPath
	// CPed (FUN_004812d0), pour chaque piéton : les proches (FUN_0047d1d0, refaits au tirage de l'image FUN_00465910(16)), puis
	// le composant de combat (FUN_00487780) et ses jetons (FUN_00486e40), avec le dernier coup du piéton (+0x550).
	{
		CScriptPedPool::ButesJetons bj;
		bj.echeance = ButeIA(76, 10.0f); bj.echeanceVelo = ButeIA(80, 30.0f); bj.porteeCone = ButeIA(84, 1.5f); bj.cosCone = ButeIA(88, 0.76f);
		for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++){
			CScriptPed *q = pietons.Acceder(i);
			if(q == nil) continue;
			auto it = controleurs.find(i);
			q->dernierCoup = it != controleurs.end() ? it->second.minuterieAttaque : 0;
			pietons.ReleverProches(i, TirageOpportunite(16, (uint16)i, imageCourante));
			std::vector<int> avant;
			for(int k = 0; k < 4; k++) for(int j = 0; j < q->nombreJetons[k]; j++) avant.push_back(q->jetons[k][j].detenteur);
			avant.push_back(q->jetonCone.detenteur);
			pietons.DistribuerJetons(i, maintenant, bj, TirerCrt, nil);
			size_t n = 0;
			for(int k = 0; k < 4; k++) for(int j = 0; j < q->nombreJetons[k]; j++, n++)
				if(q->jetons[k][j].detenteur != avant[n] && journalRestant > 0)
					printf("%6.2f s  jeton %d.%d de %d : %d → %d\n", maintenant / 1000.0, k, j, i, avant[n], q->jetons[k][j].detenteur);
			if(q->jetonCone.detenteur != avant[n])
				printf("%6.2f s  place du cône de %d : %d → %d\n", maintenant / 1000.0, i, avant[n], q->jetonCone.detenteur);
		}
	}
	pietons.AvancerVue(0.033f);   // CPed (FUN_004812d0) : FUN_0048ccf0, FUN_00499340
	pietons.AvancerDroits(maintenant);   // le composant de combat (FUN_00487780) : les droits d'attaque échus
	// FUN_00499340, la suite : la cible des piétons de l'IA (CScriptPedPool::SuivreCibles), avec les AIButes de la visée.
	CScriptPedPool::ButesCibles b;
	b.nonVuMax = ButeIA(100, 6.0f); b.portee = ButeIA(104, 30.0f); b.punitionMin = (int32)butesIA.Mot(108);
	b.marge = ButeIA(112, 1.0f); b.margeCible = ButeIA(116, 2.0f);
	int avant[SCRIPT_PED_POOL_SIZE];
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++){ const CScriptPed *p = pietons.Acceder(i); avant[i] = p ? p->verrou : -1; }
	pietons.SuivreCibles(0.033f, maintenant, b, TirerVue, nil);
	// FUN_004812d0, 0x4818f8 : après FUN_00499340, le tirage de l'image de chaque piéton (+0x1748).
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++)
		if(CScriptPed *q = pietons.Acceder(i)) q->hasardImage = (float)RandCrt(graineIA) / 32767.0f;
	choisis.clear();
	for(int i = 1; i < SCRIPT_PED_POOL_SIZE; i++) if(pietons.Acceder(i)) choisis.insert(i);
	for(int i = 1; i < SCRIPT_PED_POOL_SIZE; i++){
		const CScriptPed *p = pietons.Acceder(i);
		if(p && p->verrou != avant[i])
			printf("%6.2f s  piéton %d : cible %d → %d (style %d)\n", maintenant / 1000.0, i, avant[i], p->verrou, p->verrouMode);
	}
	AvancerProjectiles(33.0f / 20.0f);   // le pas du moteur (ms_fTimeStep) compte des cinquantièmes de seconde
	if(etatLua) AppelerCritiques(etatLua);
	// FUN_004812d0, 0x481c45 : les déclencheurs suivis (FUN_006d6f60), piéton par piéton, après le reste de son image.
	SuivreDeclencheurs();
	for(int i = 0; i < SCRIPT_PED_POOL_SIZE; i++){
		const CScriptPed *p = pietons.Acceder(i);
		if(p == nil || journalRestant <= 0) continue;
		if(p->arrive){ journalRestant--; printf("%6.2f s  piéton %d (modèle %d) arrivé en (%.1f, %.1f, %.1f)\n", maintenant / 1000.0, i, p->modele, p->pos.x, p->pos.y, p->pos.z); }
		if(p->aFrappe){ journalRestant--; printf("%6.2f s  piéton %d frappe le piéton %d (%.0f pv)\n", maintenant / 1000.0, i, p->dernierFrappe, pietons.Acceder(p->dernierFrappe) ? pietons.Acceder(p->dernierFrappe)->sante : 0); }
		if(p->vientDeMourir){ journalRestant--; printf("%6.2f s  piéton %d (modèle %d) est mort\n", maintenant / 1000.0, i, p->modele); }
	}
}

// LoadAnimationGroup(nom) (0x5c08a0) : le groupe demandé puis chargé tout de suite (LoadAllRequestedModels) ;
// HasAnimGroupLoaded(nom) (0x5bf6e0) : l'octet +4 de son entrée de streaming ; UnLoadAnimationGroup(nom) (0x5c0920).
static int
LoadAnimationGroup(lua_State *L)
{
	Journal("LoadAnimationGroup", L, 1);
	if(lua_isstring(L, 1)) groupesAnim.Charger(lua_tostring(L, 1));
	return 0;
}

static int
HasAnimGroupLoaded(lua_State *L)
{
	appels["HasAnimGroupLoaded"]++;
	lua_pushboolean(L, lua_isstring(L, 1) && groupesAnim.EstCharge(lua_tostring(L, 1)));
	return 1;
}

static int
UnLoadAnimationGroup(lua_State *L)
{
	Journal("UnLoadAnimationGroup", L, 1);
	if(lua_isstring(L, 1)) groupesAnim.Decharger(lua_tostring(L, 1));
	return 0;
}

static int
PlayerGetHealth(lua_State *L)
{
	appels["PlayerGetHealth"]++;
	lua_pushnumber(L, pietons.Acceder(SCRIPT_PED_JOUEUR)->sante);
	return 1;
}

// PedSetActionNode(ped, nœud, fichier) (0x5ca0d0) : faux si le piéton n'existe pas ; sinon FUN_00473fa0 → FUN_004849a0 :
// le nœud trouvé dans l'arbre global, ou le fichier chargé puis le nœud retrouvé ; vrai s'il existe. Le moteur
// note aussi le fichier à l'objet script courant (FUN_005dc040) pour le décharger à la fin.
static int
PedSetActionNode(lua_State *L)
{
	Journal("PedSetActionNode", L, 1);
	const CScriptPed *p = PedArg(L, 1);
	if(p == nil){ lua_pushboolean(L, 0); return 1; }
	const char *noeud = luaL_optstring(L, 2, ""), *fichier = luaL_optstring(L, 3, "");
	bool ok = arbres.TrouverOuCharger(noeud, fichier);
	// Le nœud devient celui que le piéton joue (+0x10 de son contrôleur) ; le moteur l'y met quand le
	// contrôleur le prend, l'outil tout de suite.
	if(ok){
		int ped = (int)lua_tonumber(L, 1);
		CActionController &c = controleurs[ped];
		// Une banque descend ; si rien n'y aboutit (conditions pas lues), le piéton la « joue » quand même, comme avant.
		if(!c.Entrer(arbres, arbres.Trouver(noeud), CJugeOutil(ped, maintenant / 1000.0f), maintenant / 1000.0f))
			c.Jouer(arbres, arbres.Trouver(noeud), maintenant / 1000.0f);
	}
	if(!ok) printf("        PedSetActionNode : %s absent de %s%s\n", noeud, *fichier ? fichier : "l'arbre global", arbres.EstCharge(fichier) ? "" : " (fichier introuvable)");
	lua_pushboolean(L, ok);
	return 1;
}

// PedSetActionTree(ped, arbre, fichier) (0x5cac20) : FUN_00473f80 charge le fichier, puis FUN_005dc040 pose l'arbre au
// contrôleur du piéton, qui y descend (CActionBank::Choisir, FUN_005f5490 / FUN_005f5500) : le premier nœud qui
// aboutit devient le nœud joué. Les conditions des nœuds ne sont pas encore jugées ici : une branche qui en porte
// n'est pas prise, et le contrôleur ne redescend pas aux images suivantes.
static int
PedSetActionTree(lua_State *L)
{
	Journal("PedSetActionTree", L, 1);
	const CScriptPed *p = PedArg(L, 1);
	const char *arbre = luaL_optstring(L, 2, ""), *fichier = luaL_optstring(L, 3, "");
	if(p == nil || *arbre == 0 || *fichier == 0) return 0;
	if(!arbres.TrouverOuCharger(arbre, fichier)){
		printf("        PedSetActionTree : %s absent de %s\n", arbre, fichier);
		return 0;
	}
	int ped = (int)lua_tonumber(L, 1);
	CActionController &c = controleurs[ped];
	if(!c.Poser(arbres, arbres.Trouver(arbre), CJugeOutil(ped, maintenant / 1000.0f), maintenant / 1000.0f))
		printf("        PedSetActionTree : rien n'aboutit dans %s (conditions jugées)\n", arbre);
	return 0;
}

// PedSetAITree(ped, arbre, fichier) (lua_PedSetAITree 0x5caac0) : FUN_00473f70 → FUN_00484290 (ecx = le composant +0x2e0
// du piéton) remet à zéro le contrôleur de l'IA (+0x290, FUN_005f3f40), garde les deux noms (+0x540, +0x580), trouve
// l'arbre (FUN_005f59b0, le fichier chargé s'il manque : FUN_005dbe60, FUN_005f6c50) en +0x4b4 et en fait la racine du
// contexte de l'IA (+0x358, FUN_005f36d0). Chaque image, FUN_00484490 fait avancer ce contrôleur (FUN_005f49a0) après
// l'avoir fait redescendre depuis cette racine s'il ne joue rien (FUN_00483ec0). FUN_005dc040 note le fichier à
// l'objet script, comme pour PedSetActionTree.
static int
PedSetAITree(lua_State *L)
{
	Journal("PedSetAITree", L, 1);
	CScriptPed *p = pietons.Acceder(lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1);
	const char *arbre = luaL_optstring(L, 2, ""), *fichier = luaL_optstring(L, 3, "");
	if(p == nil || *arbre == 0) return 0;
	if(!arbres.TrouverOuCharger(arbre, fichier)){
		printf("        PedSetAITree : %s absent de %s\n", arbre, fichier);
		return 0;
	}
	int ped = (int)lua_tonumber(L, 1);
	CActionController &c = controleursIA[ped];
	c = CActionController();
	c.sousControleurs = true;
	p->arbreIA = true;
	c.arbre = arbres.Trouver(arbre);
	c.Racine(arbres, c.arbre);   // FUN_00484290 → FUN_005f36d0 : la racine du contexte et sa clé
	return 0;
}

// PedSetCombatZoneMask(ped, z0, z1, z2) (0x5cd0c0 → FUN_00486110) : les zones de combat que le piéton peut prendre.
static int
PedSetCombatZoneMask(lua_State *L)
{
	Journal("PedSetCombatZoneMask", L, 1);
	CScriptPed *p = pietons.Acceder(lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1);
	if(p) for(int z = 0; z < 3; z++) p->masqueZones[z] = lua_toboolean(L, 2 + z) != 0;
	return 0;
}

// PedIsPlaying(ped, chemin[, sous]) (0x5cb830 → FUN_005f3bf0) : le piéton joue ce nœud, ou, avec `sous`, un
// nœud placé dessous.
// GetCurrentDay([modulo]) (0x5c0c00) : le jour du calendrier (DAT_020c3640), ou ce jour modulo 5.
static int
GetCurrentDay(lua_State *L)
{
	appels["GetCurrentDay"]++;
	int j = gestionnaire.etat.jour;
	lua_pushnumber(L, lua_toboolean(L, 1) ? j % 5 : j);   // 5 écrit en dur dans 0x5c0c00 (DayCond, lui, lit TimeCycleButes)
	return 1;
}

static int
PedIsPlaying(lua_State *L)
{
	Journal("PedIsPlaying", L, 1);
	const CActionNode *n = lua_isnumber(L, 1) ? NoeudJoue((int)lua_tonumber(L, 1)) : nil;
	lua_pushboolean(L, n && arbres.Joue(n, luaL_optstring(L, 2, ""), lua_toboolean(L, 3) != 0));
	return 1;
}

// PedMePlaying(ped, nom[, sous]) (0x5cb910 → FUN_005f46a0) : le nœud joué, ou un de ses parents, porte ce nom.
static int
PedMePlaying(lua_State *L)
{
	Journal("PedMePlaying", L, 1);
	const CActionNode *n = lua_isnumber(L, 1) ? NoeudJoue((int)lua_tonumber(L, 1)) : nil;
	lua_pushboolean(L, n && arbres.JoueNom(n, luaL_optstring(L, 2, "")));
	return 1;
}

// L'historique des boutons (src/hud/ButtonHistory : HUDButtonHistory, l'objet DAT_00c674f4) : les leçons de 1_02 et de 1_06 posent
// une séquence (ButtonHistoryAdd…) et trois fonctions du script (FUN_005d9f00 sur +0x70, +0x74 et +0x78 : FUN_008b52e0, FUN_0053aca0
// et FUN_0053acb0 rendent ces adresses), appelées quand le joueur réussit la séquence, la rate, ou tape le bon bouton. Dans le mode de
// l'arbre (ButtonHistoryEnableActionTreeInput), ce sont les pistes ActionRegisterButtonHistory des nœuds que le joueur joue qui
// rapportent les boutons. --sequence@s fait réussir le joueur d'un coup (l'outil pose tous les pas faits : un raccourci de l'outil,
// qui ne tape pas les boutons).
static CButtonHistory historiqueBoutons;
static int rappelReussi = LUA_NOREF, rappelRate = LUA_NOREF, rappelBonBouton = LUA_NOREF;

static void
Oublier(lua_State *L, int &ref)
{
	if(ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, ref);
	ref = LUA_NOREF;
}

// FUN_005d9f00(L, 0, case) : la fonction du premier argument mise au registre, l'ancienne lâchée (sans fonction, la case reste vide).
static void
PoserRappelBoutons(lua_State *L, int &ref)
{
	Oublier(L, ref);
	if(lua_isfunction(L, 1)){ lua_pushvalue(L, 1); ref = luaL_ref(L, LUA_REGISTRYINDEX); }
}

static int
ButtonHistorySetCallbackPassed(lua_State *L)
{
	Journal("ButtonHistorySetCallbackPassed", L, 1);
	PoserRappelBoutons(L, rappelReussi);
	return 0;
}

static int
ButtonHistorySetCallbackFailed(lua_State *L)
{
	Journal("ButtonHistorySetCallbackFailed", L, 1);
	PoserRappelBoutons(L, rappelRate);
	return 0;
}

static int
ButtonHistorySetCallbackCorrectButton(lua_State *L)
{
	Journal("ButtonHistorySetCallbackCorrectButton", L, 1);
	PoserRappelBoutons(L, rappelBonBouton);
	return 0;
}

// ButtonHistoryClearSequence() (0x5bd810 → FUN_0053abb0) : la séquence vidée ; les rappels restent.
static int
ButtonHistoryClearSequence(lua_State *L)
{
	Journal("ButtonHistoryClearSequence", L, 1);
	historiqueBoutons.Effacer();
	return 0;
}

// ButtonHistoryAddSequence(bouton, tenir, …) (0x5bd850, lu à l'assembleur de 0x5bd850 à 0x5bd8b7) : par paires, FUN_0053ad50(bouton,
// tenir, 750, -1, -1) (0x2ee : le délai du pas).
static int
ButtonHistoryAddSequence(lua_State *L)
{
	Journal("ButtonHistoryAddSequence", L, 1);
	int n = lua_gettop(L);
	for(int i = 0; i < n; i += 2) historiqueBoutons.Ajouter((int32)luaL_optnumber(L, i + 1, 0), lua_toboolean(L, i + 2) != 0, 0x2ee);
	return 0;
}

// Les secondes des scripts en millisecondes : × 1000 (le double 0x900138), puis l'arrondi 0x85c6f0 (tronqué).
static int32
EnMs(lua_State *L, int i)
{
	return (int32)((double)luaL_optnumber(L, i, 0) * 1000.0);
}

// ButtonHistoryAddSequenceTimeInterval(bouton, tenir, secondes, …) (0x5bd8f0, lu de 0x5bd8f0 à 0x5bd96d) : par triplets,
// FUN_0053ad50(bouton, tenir, secondes × 1000, -1, -1).
static int
ButtonHistoryAddSequenceTimeInterval(lua_State *L)
{
	Journal("ButtonHistoryAddSequenceTimeInterval", L, 1);
	int n = lua_gettop(L);
	for(int i = 0; i < n; i += 3)
		historiqueBoutons.Ajouter((int32)luaL_optnumber(L, i + 1, 0), lua_toboolean(L, i + 2) != 0, EnMs(L, i + 3));
	return 0;
}

// ButtonHistoryAddSequenceTimeIntervalCharge(bouton, s1, s2, s3, …) (0x5bd970, lu de 0x5bd970 à 0x5bda19) : par quadruplets,
// FUN_0053ad50(bouton, 0, s1 × 1000, s2 × 1000, s3 × 1000).
static int
ButtonHistoryAddSequenceTimeIntervalCharge(lua_State *L)
{
	Journal("ButtonHistoryAddSequenceTimeIntervalCharge", L, 1);
	int n = lua_gettop(L);
	for(int i = 0; i < n; i += 4)
		historiqueBoutons.Ajouter((int32)luaL_optnumber(L, i + 1, 0), false, EnMs(L, i + 2), EnMs(L, i + 3), EnMs(L, i + 4));
	return 0;
}

// ButtonHistorySetSequenceTime(secondes) (0x5bd820 → FUN_0053ac70(secondes × 1000)).
static int
ButtonHistorySetSequenceTime(lua_State *L)
{
	Journal("ButtonHistorySetSequenceTime", L, 1);
	historiqueBoutons.PoserDuree(EnMs(L, 1));
	return 0;
}

// ButtonHistoryIgnoreController(b) (0x5bd720 → FUN_0053b2e0, le booléen par FUN_005d8f20).
static int
ButtonHistoryIgnoreController(lua_State *L)
{
	Journal("ButtonHistoryIgnoreController", L, 1);
	historiqueBoutons.IgnorerManette(lua_toboolean(L, 1) != 0, (uint32)maintenant);
	return 0;
}

// ButtonHistoryIgnoreSequence(bouton, …) (0x5bda20 → FUN_0053ade0 pour chaque argument).
static int
ButtonHistoryIgnoreSequence(lua_State *L)
{
	Journal("ButtonHistoryIgnoreSequence", L, 1);
	int n = lua_gettop(L);
	for(int i = 1; i <= n; i++) historiqueBoutons.Ignorer((int32)luaL_optnumber(L, i, 0));
	return 0;
}

// ButtonHistoryEnableActionTreeInput(b) (0x5bda60 → FUN_0053af90 : l'octet +0xb0).
static int
ButtonHistoryEnableActionTreeInput(lua_State *L)
{
	Journal("ButtonHistoryEnableActionTreeInput", L, 1);
	historiqueBoutons.parArbre = lua_toboolean(L, 1) != 0;
	return 0;
}

// ButtonHistoryAddSequenceLocalText(texte) (0x5bda90 → FUN_0053ad20 : 31 caractères en +0xcc).
static int
ButtonHistoryAddSequenceLocalText(lua_State *L)
{
	Journal("ButtonHistoryAddSequenceLocalText", L, 1);
	historiqueBoutons.texte = std::string(luaL_optstring(L, 1, "")).substr(0, 31);
	return 0;
}

// Un rappel de l'historique, avec ses arguments (FUN_005d9ac0, FUN_005d8110, FUN_005d8510, FUN_005d8460, FUN_005d8450, FUN_005d8140).
static void
AppelerRappelBoutons(lua_State *L, int ref, const char *quoi, int nargs, int32 bouton, bool expiree)
{
	if(ref == LUA_NOREF || L == nil) return;
	printf("%6.2f s  historique des boutons : le rappel « %s » est appelé\n", maintenant / 1000.0, quoi);
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	if(nargs >= 1) lua_pushnumber(L, bouton);
	if(nargs >= 2) lua_pushboolean(L, expiree);
	if(lua_pcall(L, nargs, 0, 0) != 0){ printf("        rappel de l'historique des boutons : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
}

// FUN_0053c250 (la méthode 8 de HUDButtonHistory) : à chaque image où le composant 21 est montré (supposé : le gestionnaire du HUD
// passe les composants visibles), l'historique avance et appelle ses rappels.
static void
AvancerHistoriqueBoutons(lua_State *L)
{
	if(!hud.Visible(21)) return;
	CButtonHistory::Evenements e = historiqueBoutons.Avancer((uint32)maintenant, rappelReussi != LUA_NOREF, rappelRate != LUA_NOREF,
	                                                          rappelBonBouton != LUA_NOREF);
	if(e.reussie) AppelerRappelBoutons(L, rappelReussi, "réussie", 0, 0, false);
	if(e.bonBouton) AppelerRappelBoutons(L, rappelBonBouton, "bon bouton", 1, e.bouton, false);
	if(e.ratee) AppelerRappelBoutons(L, rappelRate, "ratée", 2, e.bouton, e.expiree);
}

// --sequence@s : le joueur réussit la séquence posée : l'outil la tient pour tapée (tous ses pas faits) ; l'image suivante appelle le
// rappel « réussie » comme FUN_0053c250.
static void
ReussirSequence(lua_State *L)
{
	CButtonHistory &h = historiqueBoutons;
	if(h.nombre < 1 || h.manetteIgnoree){ printf("%6.2f s  le joueur tape une séquence : aucune n'est posée\n", maintenant / 1000.0); return; }
	for(int i = 0; i < h.nombre; i++) h.pas[i].fait = true;
	h.courant = h.nombre; h.echouee = false; h.evenement = true;
	printf("%6.2f s  le joueur réussit la séquence de boutons\n", maintenant / 1000.0);
}

static int
EnterNIS(lua_State *L)
{
	Journal("EnterNIS", L, 1);
	nisEnCours = true;
	return 0;
}

static int
ExitNIS(lua_State *L)
{
	Journal("ExitNIS", L, 1);
	nisEnCours = false;
	return 0;
}

static int
PedSetGlobalAttitude_Rumble(lua_State *L)
{
	Journal("PedSetGlobalAttitude_Rumble", L, 1);
	auto lire = [&](int n){ return lua_isboolean(L, n) ? lua_toboolean(L, n) != 0 : lua_isnumber(L, n) ? lua_tonumber(L, n) != 0 : false; };
	rumbleAmbiant = lire(1);
	rumbleMission = lire(2);
	return 0;
}

// PlayerSetControl(c) (0x5d12f0, lua_PlayerSetControl). 0 : FUN_00445c50(1) coupe la manette (FUN_0050de50(0), l'octet +0x1c
// de la manette) et rabat l'octet +0x1e90 du joueur (FUN_00497080 : une autorité ne le prend plus pour cible) ; les entrées du
// joueur sont vidées (FUN_005f38e0) et FUN_00474420 pose son intention de mouvement (+0x2e0 : +0 … +8) sur son avant, à plat
// (FUN_00415350, FUN_00414cc0), recopiée en +0xc … +0x14. 1 : le drapeau 23 du joueur (+0x1370) à 0, FUN_00445c50(0) rend la
// manette et +0x1e90, FUN_0050de80(1) (l'octet +0x1f). Les autres valeurs ne font rien. Manette coupée, ce que l'outil fait
// faire à la manette (--jouer, --viser, --demander, --sequence, --suivre) est perdu : un bouton pressé pendant une cinématique
// ne fait rien.
static int
PlayerSetControl(lua_State *L)
{
	Journal("PlayerSetControl", L, 1);
	int c = (int)luaL_optnumber(L, 1, 1);
	CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	if(c == 0){
		controleJoueur = 0;
		if(j){
			j->controlable = false;
			pietons.ViderEntrees(SCRIPT_PED_JOUEUR);
			float a = j->cap * 3.14159265f / 180.0f;
			j->intention = CVector(cosf(a), sinf(a), 0.0f);
		}
	}else if(c == 1){
		controleJoueur = 1;
		if(j){ j->drapeaux[23] = 0; j->controlable = true; }
	}
	return 0;
}

// SoundPlayScriptedSpeechEvent(ped, événement, ligne[, banque, b, b]) (0x5d37b0) : l'événement cherché dans la table
// sans la casse ; absent, la fonction sort sans rien rendre ; sinon FUN_005a4980 joue la ligne et le script reçoit
// l'identifiant (DAT_00cf4e00, -2 si le modèle du piéton n'a pas cet événement : « ATTENTION SCRIPTER »).
static int
SoundPlayScriptedSpeechEvent(lua_State *L)
{
	Journal("SoundPlayScriptedSpeechEvent", L, 1);
	const char *nom = luaL_optstring(L, 2, "");
	int index = SpeechEventIndex(nom);
	if(index < 0){ printf("        SoundPlayScriptedSpeechEvent : « %s » n'est pas dans la table des %d événements de parole\n", nom, NUM_SPEECH_EVENTS); return 0; }
	parole.enCours = true;
	parole.ped = lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1;
	parole.evenement = index;
	parole.ligne = (int)luaL_optnumber(L, 3, 0);
	parole.id++;
	parole.fin = maintenant + DureeParole(parole.ped, parole.evenement, parole.ligne);
	lua_pushnumber(L, parole.id);
	return 1;
}

// SoundPreloadSpeech(ped, événement, ligne[, taille[, b, b]]) (0x5d4a20) : le nom cherché dans la table comme
// SoundPlayScriptedSpeechEvent (absent : rien), puis le bloc DAT_00cfb3b4 remis à zéro, l'échéance à 30 s.
static int
SoundPreloadSpeech(lua_State *L)
{
	Journal("SoundPreloadSpeech", L, 1);
	const char *nom = luaL_checkstring(L, 2);
	int index = SpeechEventIndex(nom);
	if(index < 0){ printf("        SoundPreloadSpeech : « %s » n'est pas dans la table des %d événements de parole\n", nom, NUM_SPEECH_EVENTS); return 0; }
	prechargement.Precharger(maintenant, lua_isnumber(L, 1) ? (int)lua_tonumber(L, 1) : -1, index, (int)luaL_optnumber(L, 3, 0));
	lua_pushnumber(L, ++parole.id);
	return 1;
}

// SoundIsSpeechPreloaded() (0x5d39e0) : l'octet +9 du bloc, posé par le rappel quand la ligne est prête.
static int
SoundIsSpeechPreloaded(lua_State *L)
{
	appels["SoundIsSpeechPreloaded"]++;
	lua_pushboolean(L, prechargement.prechargee);
	return 1;
}

// SoundPlayPreloadedSpeech() (0x5d39d0) : l'octet +8, « jouer » ; le rappel suivant la lance.
static int
SoundPlayPreloadedSpeech(lua_State *L)
{
	Journal("SoundPlayPreloadedSpeech", L, 1);
	prechargement.jouer = true;
	return 0;
}

// SoundStopCurrentSpeechEvent([ped]) (0x5d3a00, lu à l'assembleur de 0x5d3a00 à 0x5d3a48) : avec un argument (lua_gettop positif),
// le piéton (FUN_005c7220) et FUN_005a5880(DAT_00cf4378, piéton, 0, 0x19) : sa ligne en cours s'arrête ; sans argument,
// FUN_005a5c60 arrête toutes les lignes. L'outil n'a qu'une ligne à la fois : elle finit à cette image. F_Stage1Objectives
// (la fonction 15 de 1_05.lur) coupe ainsi Algie quand une bande est battue.
static int
SoundStopCurrentSpeechEvent(lua_State *L)
{
	Journal("SoundStopCurrentSpeechEvent", L, 1);
	if(lua_gettop(L) > 0){
		CScriptPed *p = PedArg(L, 1);
		if(p == nil || !parole.enCours || parole.ped != (int)lua_tonumber(L, 1)) return 0;
	}
	if(parole.enCours) parole.fin = maintenant;
	return 0;
}

// SoundSpeechPlaying([ped[, événement[, ligne]]]) (0x5d3a50 → FUN_005a27b0) : sans piéton, quelqu'un parle ; avec, ce
// piéton parle, et de cet événement et de cette ligne si le script les donne.
static int
SoundSpeechPlaying(lua_State *L)
{
	appels["SoundSpeechPlaying"]++;
	bool oui = parole.enCours;
	if(oui && lua_isnumber(L, 1) && (int)lua_tonumber(L, 1) != -1){
		oui = parole.ped == (int)lua_tonumber(L, 1);
		if(oui && lua_isstring(L, 2) && !lua_isnumber(L, 2)) oui = SpeechEventIndex(lua_tostring(L, 2)) == parole.evenement;
		if(oui && lua_isnumber(L, 3) && (int)lua_tonumber(L, 3) != -1) oui = parole.ligne == (int)lua_tonumber(L, 3);
	}
	lua_pushboolean(L, oui);
	return 1;
}

// --- Les cinématiques (docs/cinematiques.md) ---

// LoadCutscene(nom, drapeau) (0x5bb100 → CCutsceneMgr::LoadCutsceneData, FUN_006c4480) : les fichiers de la
// cinématique ; elle n'est « chargée » (DAT_020c5be2) que si son .INF donne au moins un objet et que tous
// leurs modèles sont là : ici, qu'il y ait des objets.
static int
LoadCutscene(lua_State *L)
{
	Journal("LoadCutscene", L, 1);
	const char *nom = luaL_checkstring(L, 1);
	cinematique.chargee = cinematique.enCours = false;
	cinematique.temps = cinematique.duree = 0; cinematique.sousTitre = 0;
	if(!cinematique.jouer){ printf("        cinématique %s : --sans-cinematiques, elle ne sera pas chargée\n", nom); return 0; }
	if(!cinematique.fichiers.Charger(nom)){ printf("        Cuts\\Cuts.img introuvable : %s ne sera pas chargée\n", nom); return 0; }
	const CCutsceneFiles &f = cinematique.fichiers;
	cinematique.chargee = f.aObjets && !f.inf.objets.empty();
	cinematique.duree = f.aCamera ? f.camera.Duree() : 0;
	printf("        cinématique %s : %zu objets (%zu faciaux), %zu sous-titres, caméra %s (%.2f s), %zu fichiers d'animation%s\n", nom,
	       f.inf.objets.size(), f.inf.faciaux.size(), f.sub.lignes.size(), f.aCamera ? "lue" : "absente", cinematique.duree, f.animations.size(),
	       cinematique.chargee ? "" : " : pas d'objet, elle ne sera pas jouée");
	return 0;
}

// LoadCutsceneSound(nom) (0x5bb130 → FUN_006c3750) : la banque de sons du nom ; DAT_020c5be7.
static int
LoadCutsceneSound(lua_State *L){ Journal("LoadCutsceneSound", L, 1); cinematique.son = true; return 0; }

// IsCutsceneLoaded() (0x5bb240) : DAT_020c5be2.
static int
IsCutsceneLoaded(lua_State *L){ appels["IsCutsceneLoaded"]++; lua_pushboolean(L, cinematique.chargee); return 1; }

// StartCutscene() (0x5bb150 → FUN_006c6210) : le temps à zéro, l'arbre d'actions lancé, DAT_020c5be4 = 1.
static int
StartCutscene(lua_State *L)
{
	Journal("StartCutscene", L, 1);
	if(!cinematique.chargee) return 0;
	cinematique.enCours = true; cinematique.temps = 0; cinematique.sousTitre = 0;
	printf("%6.2f s  cinématique %s : elle commence, %.2f s à jouer\n", maintenant / 1000.0, cinematique.fichiers.nom.c_str(), cinematique.duree);
	return 0;
}

// StopCutscene() (0x5bb160 → CCutsceneMgr::DeleteCutsceneData, FUN_006c3ed0) : tout est libéré.
static int
StopCutscene(lua_State *L)
{
	Journal("StopCutscene", L, 1);
	if(cinematique.chargee) printf("%6.2f s  cinématique %s : arrêtée à %.2f s\n", maintenant / 1000.0, cinematique.fichiers.nom.c_str(), cinematique.temps);
	cinematique.chargee = cinematique.enCours = cinematique.son = false;
	return 0;
}

// GetCutsceneRunning() (0x5bb190) : l'octet DAT_020c5be4, en nombre ; GetCutsceneTime() (0x5bb170) : le temps en ms.
static int
GetCutsceneRunning(lua_State *L){ appels["GetCutsceneRunning"]++; lua_pushnumber(L, cinematique.enCours ? 1 : 0); return 1; }
static int
GetCutsceneTime(lua_State *L){ appels["GetCutsceneTime"]++; lua_pushnumber(L, (lua_Number)(long)(cinematique.temps * 1000 + 0.5f)); return 1; }

// CutSetCutsceneTableSize(n) (0x5bb1e0), CutSetCutsceneName(nom) (0x5bb200) : SCutscenes.lua les pose au chargement.
static int
CutSetCutsceneTableSize(lua_State *L){ Journal("CutSetCutsceneTableSize", L, 1); cinematique.tailleTable = (int)luaL_optnumber(L, 1, 0); return 0; }
static int
CutSetCutsceneName(lua_State *L){ Journal("CutSetCutsceneName", L, 1); cinematique.nomTable = luaL_optstring(L, 1, ""); if(cinematique.nomTable.size() > 16) cinematique.nomTable.resize(16); return 0; }

// Une image de FUN_006c4350 : le temps avance tant que la cinématique tourne ; les sous-titres passent au
// journal à leur image de début ; à la dernière clé de la caméra, la cinématique est finie.
static void
AvancerCinematique(void)
{
	if(!cinematique.enCours) return;
	cinematique.temps += 0.033f;
	const CCutsceneSub &sub = cinematique.fichiers.sub;
	if(cinematique.fichiers.aSousTitres && sub.imagesParSeconde > 0)
		while(cinematique.sousTitre < sub.lignes.size() && sub.lignes[cinematique.sousTitre].debut <= cinematique.temps * sub.imagesParSeconde){
			if(journalRestant > 0){ journalRestant--; printf("%6.2f s  cinématique %s : sous-titre %s (%.1f à %.1f s)\n", maintenant / 1000.0, cinematique.fichiers.nom.c_str(), sub.lignes[cinematique.sousTitre].cle.c_str(), sub.lignes[cinematique.sousTitre].debut / sub.imagesParSeconde, sub.lignes[cinematique.sousTitre].fin / sub.imagesParSeconde); }
			cinematique.sousTitre++;
		}
	if(cinematique.temps >= cinematique.duree){
		cinematique.enCours = false;
		printf("%6.2f s  cinématique %s : finie (%.2f s)\n", maintenant / 1000.0, cinematique.fichiers.nom.c_str(), cinematique.temps);
	}
}

// --- Les missions ---

// FUN_006ac5d0 : la banque des missions, Missions.cat et les sous-arbres qu'elle cite.
static void
ChargerMissions(void)
{
	if(!banque.Charger()){ printf("Act\\Act.img ou Missions.cat introuvable : pas de table des missions\n"); return; }
	banqueParoleLue = banqueParole.Charger();
	printf("parole : %s\n", banqueParoleLue ? "SpeechDB.bin et Speech.bin lus, durées réelles" : "SpeechDB.bin ou Speech.bin illisible, 2,5 s par ligne");
	int jouables = 0;
	for(const CMissionData &m : banque.missions) jouables += m.Jouable();
	printf("%zu missions dans la banque (%zu fichiers), %d avec un script\n", banque.missions.size(), banque.banques.size(), jouables);
	{
		// FUN_005f6f20 : les arbres globaux sont chargés avant tout script.
		printf("%d arbres d'actions globaux chargés sous /Global\n", arbres.ChargerGlobaux());
		// Les durées des animations des piétons (MAINPED.HXD, docs/hxd.md) : une piste Animation en mode 0 finit avec
		// son animation (FUN_0060ae00), ce qui finit le nœud et fait jouer ses Sequence.
		{
			const int32 MAX = 4 << 20;
			uint8 *h = (uint8*)malloc(MAX);
			int32 k = CFileMgr::LoadFile("Anim\\MAINPED.HXD", h, MAX, "rb");
			CHxdFile mainped;
			if(k > 0 && mainped.Load(h, (uint32)k))
				for(const CHxdAnim &a : mainped.anims) arbres.dureesAnimations[a.hachage & 0x7fffffff] = a.duree;
			printf("%d durées d'animations lues dans MAINPED.HXD\n", (int)arbres.dureesAnimations.size());
			free(h);
		}
		CActionTrack pb;  // FUN_0072ae00 : PlayerButes → DAT_020c88b8 ; +0xac, +0xb0, +0xb4 : le facteur des coups du joueur
		if(CActionBank::Butes("PlayerButes", pb)) facteurCoupsJoueur = CActionBank::ButesReel(pb, 172);
		CActionBank::Butes("AIButes", butesIA);   // FUN_0072ae00 : AIButes → DAT_020c889c
		PoserJetonsDe(SCRIPT_PED_JOUEUR);   // le composant de combat du joueur (FUN_00486cd0)
		CActionTrack t;   // FUN_0072ae00 : AreaTransitionButes → DAT_020c8910
		if(CActionBank::Butes("AreaTransitionButes", t)){
			fonduNoirMs = CActionBank::ButesReel(t, 24) * 1000;
			fonduRetourMs = CActionBank::ButesReel(t, 28) * 1000;
		}
		// FUN_0072ae00 : TimeCycleButes → DAT_020c88ac ; +0x18 = jours d'un cycle, le modulo de DayCond et de
		// GetCurrentDay(true).
		if(CActionBank::Butes("TimeCycleButes", t) && t.Mot(24) > 0) gestionnaire.etat.joursParCycle = (int)t.Mot(24);
	}
}

// FUN_006acba0 : le script de la mission puis MissionSetup ; par indice dans la banque.
static bool
LancerMission(lua_State *L, int indice, const char *nom)
{
	if(mission.etat != AUCUNE){ printf("        %s refusée : %s est en cours\n", nom, mission.nom.c_str()); return false; }
	if(indice < 0){ printf("        %s absente de la banque\n", nom); return false; }
	const CMissionData &m = banque.missions[indice];
	if(!m.Jouable()){ printf("        %s n'a pas de script (%s) : rien à lancer\n", nom, m.script.empty() ? "champ 28 vide" : m.script.c_str()); return false; }
	int s = NouveauScript(L, m.script.c_str());
	if(s < 0){ printf("        %s : CreateNameSpace(%s) a échoué\n", nom, m.script.c_str()); return false; }
	mission.nom = nom;
	mission.indice = indice;
	mission.script = s;
	mission.reussie = false;
	mission.etat = PREPARATION;
	printf("%6.2f s  mission %s : %s chargé, MissionSetup démarre\n", maintenant / 1000.0, nom, m.script.c_str());
	NouveauThread(L, s, "MissionSetup");
	return true;
}

// ForceStartMission(nom) : FUN_006acba0 par nom.
static int
ForceStartMission(lua_State *L)
{
	const char *nom = luaL_checkstring(L, 1);
	Journal("ForceStartMission", L, 1);
	LancerMission(L, banque.Index(nom), nom);
	return 0;
}

// MissionSucceed / MissionFail : la mission s'arrête ; main est arrêté, MissionCleanup suivra.
static void
ArreterMain(void)
{
	if(mission.script < 0) return;
	for(Thread &t : scripts[mission.script].threads)
		if(t.fonction == "main" && t.etat < ARRET_DEMANDE) t.etat = ARRET_DEMANDE;
}

static int
MissionSucceed(lua_State *L){ Journal("MissionSucceed", L, 1); mission.reussie = true; ArreterMain(); return 0; }

static int
MissionFail(lua_State *L){ Journal("MissionFail", L, 1); mission.reussie = false; ArreterMain(); return 0; }

static int
MissionActive(lua_State *L)
{
	appels["MissionActive"]++;
	lua_pushboolean(L, mission.etat != AUCUNE);
	return 1;
}

static int
MissionActiveSpecific(lua_State *L)
{
	appels["MissionActiveSpecific"]++;
	const char *nom = luaL_checkstring(L, 1);
	lua_pushboolean(L, mission.etat != AUCUNE && strcasecmp(mission.nom.c_str(), nom) == 0);
	return 1;
}

static int
MissionGetCurrentName(lua_State *L)
{
	appels["MissionGetCurrentName"]++;
	if(mission.etat == AUCUNE) lua_pushnil(L);
	else lua_pushstring(L, mission.nom.c_str());
	return 1;
}

// IsMissionCompleated(nom) (0x5c0580) : réussie au moins une fois dans cette partie.
static int
IsMissionCompleated(lua_State *L)
{
	appels["IsMissionCompleated"]++;
	int i = banque.Index(luaL_checkstring(L, 1));
	lua_pushboolean(L, i >= 0 && i < (int)gestionnaire.etat.reussites.size() && gestionnaire.etat.reussites[i] != 0);
	return 1;
}

// MissionForceCompleted(nom) (0x5c0740) : le bit 8 de l'octet +4 de l'entrée de la mission (FUN_006a9e70, FUN_006aa660),
// que MissionUnlocked lit ; IsMissionCompleated ne lit que les réussites (le mot +2).
static int
MissionForceCompleted(lua_State *L)
{
	Journal("MissionForceCompleted", L, 1);
	int i = banque.Index(luaL_checkstring(L, 1));
	if(i < 0) return 0;
	if(gestionnaire.etat.drapeaux.size() != banque.missions.size()) gestionnaire.etat.drapeaux.resize(banque.missions.size(), 0);
	gestionnaire.etat.drapeaux[i] |= 8;
	return 0;
}

// SystemShouldMovePlayer() → (déplacer ?, mode) : à la nouvelle partie, vrai et 0 (gamemain pose alors
// le joueur à la grille de l'école, PlayerSetPosXYZArea(272.68, -73.13, 6.97, 0)) jusqu'à
// SystemResetShouldMovePlayer() ; le mode 1 serait une sauvegarde à reprendre.
static int
SystemShouldMovePlayer(lua_State *L)
{
	appels["SystemShouldMovePlayer"]++;
	lua_pushboolean(L, gestionnaire.actif && gestionnaire.deplacerJoueur);
	lua_pushnumber(L, 0);
	return 2;
}

static int
SystemResetShouldMovePlayer(lua_State *L)
{
	Journal("SystemResetShouldMovePlayer", L, 1);
	gestionnaire.deplacerJoueur = false;
	return 0;
}

// PlayerSetPosXYZArea(x, y, z, zone) : le joueur demandé là, dans cette zone ; c'est par là que passe
// AreaTransitionXYZ (SGlFunc.lua), donc gamemain à la nouvelle partie (272.68, -73.13, 6.97, 0).
static int
PlayerSetPosXYZArea(lua_State *L)
{
	Journal("PlayerSetPosXYZArea", L, 1);
	const CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
	DemanderZone("PlayerSetPosXYZArea", CVector((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)), j->cap, (int)luaL_optnumber(L, 4, zones.zoneVisible));
	return 0;
}

// MissionGetIndex(nom) (0x5bfe20) : l'indice dans le tableau +0x4b8, ou -1.
static int
MissionGetIndex(lua_State *L)
{
	appels["MissionGetIndex"]++;
	lua_pushnumber(L, banque.Index(luaL_checkstring(L, 1)));
	return 1;
}

// SystemAllowMissionManagerToForceRunMissions() (0x5d5110) : pose l'octet 0x020c441d, +0x77d du gestionnaire.
static int
SystemAllowMissionManagerToForceRunMissions(lua_State *L)
{
	Journal("SystemAllowMissionManagerToForceRunMissions", L, 1);
	gestionnaire.autorise = true;
	return 0;
}

// AreaTransitionXYZ et AreaTransitionPoint ne sont pas des fonctions du moteur : SGlFunc.lua les définit
// (GlobalImportScript, donc dans _G) autour de PlayerSetPosXYZArea / PlayerSetPosPointArea et d'une attente
// de AreaIsLoading. Une version C ici serait écrasée par la leur au chargement de main.lua.

// FUN_004176d0 : une image du gestionnaire de zones, et ce qu'il demande au monde.
static int imagesEnAttenteDuSignal = 0;
static void
AvancerZones(lua_State *L)
{
	bool scriptsDeZone = false, scriptDemande = false;
	const char *attendu = zones.Script(zones.demande.zone);
	for(const Script &sc : scripts)
		if(sc.zone && !sc.fin){ scriptsDeZone = true; if(strcasecmp(sc.fichier.c_str(), attendu) == 0) scriptDemande = true; }
	int avant = zones.zoneVisible;
	// FUN_00416e30, état 0 : les portes SV_ (le joueur dans leur déclencheur) et DT_ (leur objet poussé)
	// envoient le joueur vers leur point.
	{
		const CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
		int porte = zones.PasserUnePorte(donnees, j->pos, j->cap);
		if(porte >= 0){
			const CTriggerZone *dz = donnees.ZoneParHandle(zones.portes[porte].declencheur);
			printf("%6.2f s  zones : le joueur passe la porte %s, vers la zone %d%s\n", maintenant / 1000.0, dz ? dz->nom.c_str() : "?", zones.demande.zone,
			       zones.retourEnReserve ? " (retour gardé)" : "");
		}
	}
	zones.AvancerImage(fondu.Etat(), !scriptsDeZone, scriptDemande);
	const CAreaManager::Evenements &e = zones.evenements;
	if(e.fonduNoir) LancerFondu(fonduNoirMs, true);
	if(e.decharger){
		// FUN_005dbb20 : les scripts de zone sont retirés (leurs threads avec).
		for(Script &sc : scripts){
			if(!sc.zone || sc.fin) continue;
			sc.fin = true;
			for(Thread &t : sc.threads) if(t.etat < TUE){ t.etat = TUE; luaL_unref(L, LUA_REGISTRYINDEX, t.ref); t.ref = LUA_NOREF; }
			printf("%6.2f s  zones : script de zone %s retiré\n", maintenant / 1000.0, sc.fichier.c_str());
		}
	}
	if(e.charger){
		// FUN_005dc020(script, 1) : l'objet script de la zone, son premier niveau puis son main ; marqué « zone ».
		if(*e.charger){
			int s = NouveauScript(L, e.charger);
			if(s >= 0){ scripts[s].zone = true; NouveauThread(L, s, "main"); printf("%6.2f s  zones : script de zone %s chargé, main démarre\n", maintenant / 1000.0, e.charger); }
			else printf("%6.2f s  zones : CreateNameSpace(%s) a échoué\n", maintenant / 1000.0, e.charger);
		}else{ printf("%6.2f s  zones : aucun script enregistré pour la zone %d, rien n'attendra son signal\n", maintenant / 1000.0, zones.demande.zone); zones.SignalerPret(); }
		imagesEnAttenteDuSignal = 0;
	}
	// Le main du script de zone doit appeler AreaSignalAreaTransitionReadyToLoad ; s'il ne le fait pas (un
	// bouchon qui le bloque avant), on le dit une fois au bout de cinq secondes.
	if(zones.etat == AREA_PRET && ++imagesEnAttenteDuSignal == 150) printf("%6.2f s  zones : %s n'a pas encore appelé AreaSignalAreaTransitionReadyToLoad, la zone %d attend\n", maintenant / 1000.0, attendu, zones.demande.zone);
	if(e.zoneChangee) printf("%6.2f s  zones : la zone visible passe de %d à %d\n", maintenant / 1000.0, avant, zones.zoneVisible);
	if(e.joueurPose){
		PoserObjetsAnimes(zones.zoneVisible);   // FUN_00416ae0, avant FUN_004162f0
		CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
		j->pos = zones.demande.pos; j->cap = zones.demande.cap;
		joueurPose = true;
		printf("%6.2f s  zones : le joueur est posé en (%.1f, %.1f, %.1f), cap %.0f, zone %d\n", maintenant / 1000.0, j->pos.x, j->pos.y, j->pos.z, j->cap, zones.zoneVisible);
		AmenerLesAllies(zones.zoneVisible, j->pos);
	}
	if(e.fonduRetour) LancerFondu(fonduRetourMs, false);
}

// FUN_004162f0 (le joueur posé au bout d'un changement de zone) → FUN_0048b970 (lu à l'assembleur), pour chaque piéton du pool
// du dernier au premier : hors d'une mission de classe (MissionInClass), un allié du joueur (FUN_004853e0 : lui-même n'est pas
// le joueur, et un de ses meneurs l'est) passe avec lui si la zone d'arrivée (DAT_00bd1008) est 0, 2, 9 ou 13, s'il a un rappel
// (+0x141c), ou si FUN_0044e2b0 et l'octet +0x50 du sexe sont faux : FUN_0048af70 le pose, son partenaire de prise lâché
// (FUN_00477320), FUN_00473d10, sa file d'objectifs vidée (FUN_00470200) et son état d'allié à 1 (+0x318). FUN_0048af70 (lu de
// 0x48af70 à 0x48b2b8) : avec +0x153c et le joueur pour premier meneur, 1,5 m à la droite du joueur (0x8ffa08), sinon jusqu'à
// deux cents tirages autour du point d'arrivée (+0x8c4 de 0x9e4f4c), x et y décalés de rand() / 32767 × 6 − 3 (0x900978,
// 0x900988), gardés s'ils sont au sol (FUN_00463560), à vue (FUN_0049e170), hors des zones interdites (FUN_0046ee60) et sur le
// maillage (FUN_005eb670) ; l'outil n'a ni sol ni maillage : le premier tirage est gardé, à la hauteur du joueur (supposé).
// Les autres piétons (les poursuivants, ceux qu'on efface) ne sont pas suivis.
static void
AmenerLesAllies(int zone, const CVector &arrivee)
{
	bool zoneSure = zone == 0 || zone == 2 || zone == 9 || zone == 13;
	for(int h = SCRIPT_PED_POOL_SIZE - 1; h > SCRIPT_PED_JOUEUR; h--){
		CScriptPed *p = pietons.Acceder(h);
		if(p == nil || p->type == SCRIPT_PED_TYPE_JOUEUR || pietons.PremierMeneur(h) != SCRIPT_PED_JOUEUR) continue;
		if(!zoneSure) continue;
		float dx = (float)RandCrt(graineIA) / 32767.0f * 6.0f - 3.0f, dy = (float)RandCrt(graineIA) / 32767.0f * 6.0f - 3.0f;
		p->pos = CVector(arrivee.x + dx, arrivee.y + dy, arrivee.z);
		p->cibleDePrise = -1;
		p->nombreObjectifs = 0;
		p->etatAllie = 1;
		printf("%6.2f s  zones : l'allié %d passe avec le joueur, posé en (%.1f, %.1f, %.1f)\n", maintenant / 1000.0, h, p->pos.x, p->pos.y, p->pos.z);
	}
}

// FUN_006ad130, réduit : à chaque image, ce que le gestionnaire fait des missions de la banque.
static void
AvancerGestionnaire(lua_State *L)
{
	static std::set<std::string> coronasAbsents;
	if(!gestionnaire.actif) return;
	if(gestionnaire.activees.size() != banque.missions.size()) gestionnaire.activees.assign(banque.missions.size(), false);
	if(gestionnaire.etat.reussites.size() != banque.missions.size()) gestionnaire.etat.reussites.assign(banque.missions.size(), 0);
	gestionnaire.etat.heure = horloge.Heure(); gestionnaire.etat.minute = horloge.Minute();
	gestionnaire.etat.courante = mission.etat != AUCUNE ? mission.indice : -1;
	gestionnaire.etat.coursCourant = mission.etat != AUCUNE && mission.indice >= 0 && banque.missions[mission.indice].type == 1 ? mission.indice : -1;
	for(size_t i = 0; i < banque.missions.size(); i++){
		const CMissionData &m = banque.missions[i];
		// FUN_006ad130 : une mission déjà réussie n'est réactivée que si son champ 44 (+0x2c) le dit.
		if(!m.Jouable() || (gestionnaire.etat.reussites[i] && !m.rejouable)) continue;
		if(banque.Activable((int)i, gestionnaire.etat) != 1) continue;
		bool libre = mission.etat == AUCUNE;
		if(!gestionnaire.activees[i]){
			gestionnaire.activees[i] = true;
			printf("%6.2f s  gestionnaire : %s activée (%s%s%s)\n", maintenant / 1000.0, NomMission(m),
			       m.lancerSeul ? "se lance seule" : m.corona.empty() ? "sans corona" : "corona ", m.corona.c_str(),
			       m.corona.empty() ? "" : m.coronaDemiCote == 2 ? ", pavé de 2 m" : ", pavé de 1 m");
			if(m.lancerSeul && libre){ LancerMission(L, (int)i, NomMission(m)); continue; }
		}
		if(!libre || !gestionnaire.autorise || m.corona.empty()) continue;
		// Le corona est une liste de points de MisTrig.dat, que gamemain charge (DATLoad("MisTrig.DAT", 1))
		// avant d'autoriser le gestionnaire ; 1_01_Corona est à 24,8 m de la grille. FUN_006a8650 teste le
		// joueur dans le pavé de demi-côté +0x1c (champ 28) autour du point, par FUN_005c2910(1, 0, 3, …), le
		// même pavé fermé que PlayerIsInAreaXYZ.
		int h = donnees.ListePoints(m.corona.c_str());
		const CTriggerPointPoint *pt = donnees.Point(h, 1);
		if(pt == nil){
			if(!coronasAbsents.count(m.corona)){ coronasAbsents.insert(m.corona); printf("%6.2f s  gestionnaire : le corona %s n'est dans aucun DAT chargé\n", maintenant / 1000.0, m.corona.c_str()); }
			continue;
		}
		const CVector &j = pietons.Acceder(SCRIPT_PED_JOUEUR)->pos;
		float r = m.coronaDemiCote;
		if(!PaveContient(j, pt->pos, r, r, r, true)) continue;
		printf("%6.2f s  gestionnaire : le joueur est dans le pavé de %.0f m du corona %s (%d), %s démarre\n", maintenant / 1000.0, r, m.corona.c_str(), h, NomMission(m));
		LancerMission(L, (int)i, NomMission(m));
	}
}

int
main(int argc, char **argv)
{
	const char *aLancer = nil;           // --mission NOM : ForceStartMission(NOM) après une seconde
	std::vector<std::pair<uint32, std::string>> entrees;   // --entrer NOM@s : le joueur entre dans NOM à s secondes
	std::vector<std::pair<uint32, std::string>> allers;    // --aller NOM[,n]@s ou x,y,z@s : le joueur y est posé à s secondes
	std::vector<std::pair<uint32, int>> portes;            // --zone N@s : le joueur passe dans la zone N (une porte) à s secondes
	std::vector<std::pair<uint32, std::string>> poussees;  // --porte NOM@s : le joueur pousse la porte du déclencheur NOM (DT_…) à s secondes
	std::vector<uint32> sequences;                         // --sequence @s : le joueur réussit la séquence de boutons à s secondes
	std::vector<std::pair<uint32, uint32>> attentes;      // --demander N@s : une ActionRequest n° N du joueur à s secondes (tenue 0,25 s)
	std::vector<std::pair<uint32, std::string>> gestes;    // --jouer CHEMIN@s : le joueur joue ce nœud d'action à s secondes (la manette : parer, frapper)
	std::vector<std::pair<uint32, int>> visees;           // --viser PIETON@s : le joueur verrouille ce piéton à s secondes
	std::vector<std::pair<uint32, int>> choixArmes;       // --arme ARME@s : le joueur choisit cette arme au HUD à s secondes
	std::vector<std::pair<uint32, std::string>> objetsVises;   // --viser-objet NOM@s : le joueur vise cet objet (le nom de l'instance)
	std::vector<uint32> tirsManette;                       // --tirer @s1,s2… : le joueur tire avec l'arme en main
	std::vector<uint32> prises;                            // --ramasser @s1,s2… : le joueur ramasse ce qui est à ses pieds
	std::vector<const char*> pos;
	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "--mission") == 0 && i + 1 < argc) aLancer = argv[++i];
		else if(strcmp(argv[i], "--nouvelle-partie") == 0) gestionnaire.actif = true;
		else if(strcmp(argv[i], "--sans-cinematiques") == 0) cinematique.jouer = false;
		else if(strcmp(argv[i], "--entrer") == 0 && i + 1 < argc){
			std::string e = argv[++i]; size_t at = e.find('@');
			entrees.push_back({(uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000), e.substr(0, at)});
		}
		else if(strcmp(argv[i], "--zone") == 0 && i + 1 < argc){
			std::string e = argv[++i]; size_t at = e.find('@');
			portes.push_back({(uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000), atoi(e.c_str())});
		}
		else if(strcmp(argv[i], "--jouer") == 0 && i + 1 < argc){
			// --jouer CHEMIN@s1,s2,… : le même geste à plusieurs instants (une série de coups).
			std::string e = argv[++i];
			size_t at = e.rfind('@');
			if(at == std::string::npos) gestes.push_back({2000, e});
			else for(const char *q = e.c_str() + at + 1; *q; ){
				gestes.push_back({(uint32)(atof(q) * 1000), e.substr(0, at)});
				const char *v = strchr(q, ',');
				if(v == nil) break;
				q = v + 1;
			}
		}
		else if(strcmp(argv[i], "--sequence") == 0 && i + 1 < argc){
			std::string e = argv[++i]; size_t at = e.find('@');
			sequences.push_back((uint32)(atof(e.c_str() + (at == std::string::npos ? 0 : at + 1)) * 1000));
		}
		else if(strcmp(argv[i], "--demander") == 0 && i + 1 < argc){
			std::string e = argv[++i]; size_t at = e.find('@');
			attentes.push_back({(uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000), (uint32)strtoul(e.c_str(), nil, 0)});
		}
		else if(strcmp(argv[i], "--porte") == 0 && i + 1 < argc){
			std::string e = argv[++i]; size_t at = e.find('@');
			poussees.push_back({(uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000), e.substr(0, at)});
		}
		else if(strcmp(argv[i], "--viser") == 0 && i + 1 < argc){
			// --viser PIETON@s : le bouton de verrouillage de la manette, le joueur prend le piéton pour cible (FUN_00497d20).
			std::string e = argv[++i]; size_t at = e.find('@');
			visees.push_back({(uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000), atoi(e.c_str())});
		}
		else if(strcmp(argv[i], "--tirer") == 0 && i + 1 < argc){
			// --tirer @s1,s2,… : la gâchette, le tir du haut du corps (les pistes spawn du nœud de l'arme : Release/Fire/SLINGSHOT et sa
			// piste WeaponFire) sans changer le nœud joué ; l'outil n'a pas de contrôleur à part pour le haut du corps.
			std::string e = argv[++i]; size_t at = e.find('@');
			for(const char *q = e.c_str() + (at == std::string::npos ? 0 : at + 1); *q; ){
				tirsManette.push_back((uint32)(atof(q) * 1000));
				const char *v = strchr(q, ',');
				if(v == nil) break;
				q = v + 1;
			}
		}
		else if(strcmp(argv[i], "--ramasser") == 0 && i + 1 < argc){
			// --ramasser @s1,s2,… : le bouton d'action près d'un ramassage (une brique, un couvercle) : le joueur le prend en main.
			std::string e = argv[++i]; size_t at = e.find('@');
			for(const char *q = e.c_str() + (at == std::string::npos ? 0 : at + 1); *q; ){
				prises.push_back((uint32)(atof(q) * 1000));
				const char *v = strchr(q, ',');
				if(v == nil) break;
				q = v + 1;
			}
		}
		else if(strcmp(argv[i], "--viser-objet") == 0 && i + 1 < argc){
			// --viser-objet NOM@s : le verrouillage sur un objet (ManualLockTargetable, le bit 15 de sa ligne de object.dat), par le nom
			// de son instance (« tschool_BusWindow01 ») ; « - » le lâche. Les tirs du joueur partent vers lui.
			std::string e = argv[++i]; size_t at = e.find('@');
			objetsVises.push_back({(uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000), e.substr(0, at)});
		}
		else if(strcmp(argv[i], "--arme") == 0 && i + 1 < argc){
			// --arme ARME@s : la croix de la manette, le joueur choisit une arme de son inventaire au HUD (FUN_0055c6f0 : +0xc0 du
			// composant HUDWeapons) ; la condition WeaponDesiredIsFocusWeapon en fait l'arme voulue, Weapons/SelectActions la prend.
			std::string e = argv[++i]; size_t at = e.find('@');
			choixArmes.push_back({(uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000), atoi(e.c_str())});
		}
		else if(strcmp(argv[i], "--suivre") == 0 && i + 1 < argc){
			std::string e = argv[++i]; size_t at = e.find('@'), v = e.find(',');
			Poursuite q;
			q.debut = (uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000);
			q.ped = atoi(e.c_str());
			q.distance = v != std::string::npos && (at == std::string::npos || v < at) ? (float)atof(e.c_str() + v + 1) : 2.0f;
			q.annoncee = false;
			poursuites.push_back(q);
		}
		else if(strcmp(argv[i], "--courir") == 0 && i + 1 < argc){
			std::string e = argv[++i]; size_t at = e.find('@');
			Poursuite q;
			q.debut = (uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000);
			q.ped = -1; q.distance = 0.3f; q.annoncee = false; q.fixe = true;
			if(sscanf(e.c_str(), "%f,%f,%f", &q.point.x, &q.point.y, &q.point.z) == 3) poursuites.push_back(q);
		}
		else if(strcmp(argv[i], "--aller") == 0 && i + 1 < argc){
			std::string e = argv[++i]; size_t at = e.find('@');
			allers.push_back({(uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000), e.substr(0, at)});
		}
		else pos.push_back(argv[i]);
	}
	if(getenv("LANCER_JOURNAL")) journalRestant = atoi(getenv("LANCER_JOURNAL"));   // lignes de journal (400)
	if(getenv("LANCER_JOURNAL_IA")) journalIA = atoi(getenv("LANCER_JOURNAL_IA"));   // passages de l'IA journalisés (80)
	// LANCER_CONTROLEURS=1 : journalise aussi les passages des contrôleurs des piétons de script (le bully, Russell).
	tousLesControleurs = getenv("LANCER_CONTROLEURS") != nil;
	double secondes = pos.size() > 0 ? atof(pos[0]) : 10.0;
	const char *fichier = pos.size() > 1 ? pos[1] : "main.lua";
	const char *fonction = pos.size() > 2 ? pos[2] : "gamemain";
	image = CdStream::AddImage(kImg);
	if(image < 0){ printf("%s introuvable (BULLY_DATA ?)\n", kImg); return 1; }
	{
		int monde = CdStream::AddImage("Stream\\World.img");
		if(monde >= 0) printf("%d groupes d'animations dans Stream/World.img\n", groupesAnim.Lire(CdStream::ms_images[monde]));
	}

	// CreateLuaState (0x5db260) : bibliothèques, API, puis util.lua dans _G.
	lua_State *L = lua_open();
	etatLua = L;
	luaopen_base(L); luaopen_table(L); luaopen_math(L); luaopen_debug(L);
	lua_settop(L, 0);
	lua_register(L, "ImportScript", ImportScript);
	lua_register(L, "Wait", Wait);
	lua_register(L, "GetTimer", GetTimer);
	lua_register(L, "SystemIsReady", SystemIsReady);
	lua_register(L, "ClockGet", ClockGet);
	lua_register(L, "ClockSet", ClockSet);
	lua_register(L, "ClockSetTickRate", ClockSetTickRate);
	lua_register(L, "CreateThread", CreateThread);
	lua_register(L, "TerminateThread", TerminateThread);
	lua_register(L, "__onerror", SurErreur);
	lua_register(L, "RequestModel", RequestModel);
	lua_register(L, "WeaponRequestModel", RequestModel);
	lua_register(L, "VehicleRequestModel", RequestModel);
	lua_register(L, "ForceStartMission", ForceStartMission);
	lua_register(L, "MissionSucceed", MissionSucceed);
	lua_register(L, "MissionFail", MissionFail);
	lua_register(L, "MissionActive", MissionActive);
	lua_register(L, "MissionActiveSpecific", MissionActiveSpecific);
	lua_register(L, "MissionGetCurrentName", MissionGetCurrentName);
	lua_register(L, "IsMissionCompleated", IsMissionCompleated);
	lua_register(L, "MissionGetIndex", MissionGetIndex);
	lua_register(L, "SystemAllowMissionManagerToForceRunMissions", SystemAllowMissionManagerToForceRunMissions);
	lua_register(L, "AreaIsLoading", AreaIsLoading);
	lua_register(L, "AreaSignalAreaTransitionReadyToLoad", AreaSignalAreaTransitionReadyToLoad);
	lua_register(L, "AreaRegisterAreaScript", AreaRegisterAreaScript);
	lua_register(L, "CameraFade", CameraFade);
	lua_register(L, "PedSetActionNode", PedSetActionNode);
	lua_register(L, "ButtonHistorySetCallbackPassed", ButtonHistorySetCallbackPassed);
	lua_register(L, "ButtonHistorySetCallbackFailed", ButtonHistorySetCallbackFailed);
	lua_register(L, "ButtonHistoryClearSequence", ButtonHistoryClearSequence);
	lua_register(L, "ButtonHistorySetCallbackCorrectButton", ButtonHistorySetCallbackCorrectButton);
	lua_register(L, "ButtonHistoryAddSequence", ButtonHistoryAddSequence);
	lua_register(L, "ButtonHistoryAddSequenceTimeInterval", ButtonHistoryAddSequenceTimeInterval);
	lua_register(L, "ButtonHistoryAddSequenceTimeIntervalCharge", ButtonHistoryAddSequenceTimeIntervalCharge);
	lua_register(L, "ButtonHistorySetSequenceTime", ButtonHistorySetSequenceTime);
	lua_register(L, "ButtonHistoryIgnoreController", ButtonHistoryIgnoreController);
	lua_register(L, "ButtonHistoryIgnoreSequence", ButtonHistoryIgnoreSequence);
	lua_register(L, "ButtonHistoryEnableActionTreeInput", ButtonHistoryEnableActionTreeInput);
	lua_register(L, "ButtonHistoryAddSequenceLocalText", ButtonHistoryAddSequenceLocalText);
	lua_register(L, "PedIsPlaying", PedIsPlaying);
	lua_register(L, "PedMePlaying", PedMePlaying);
	lua_register(L, "PlayerIsInAreaObject", PlayerIsInAreaObject);
	lua_register(L, "PedLockTarget", PedLockTarget);
	lua_register(L, "PedSetActionTree", PedSetActionTree);
	lua_register(L, "GetCurrentDay", GetCurrentDay);
	lua_register(L, "SoundPlayScriptedSpeechEvent", SoundPlayScriptedSpeechEvent);
	lua_register(L, "SoundSpeechPlaying", SoundSpeechPlaying);
	lua_register(L, "SoundPreloadSpeech", SoundPreloadSpeech);
	lua_register(L, "SoundIsSpeechPreloaded", SoundIsSpeechPreloaded);
	lua_register(L, "SoundPlayPreloadedSpeech", SoundPlayPreloadedSpeech);
	lua_register(L, "PlayerSetControl", PlayerSetControl);
	lua_register(L, "LoadCutscene", LoadCutscene);
	lua_register(L, "LoadCutsceneSound", LoadCutsceneSound);
	lua_register(L, "IsCutsceneLoaded", IsCutsceneLoaded);
	lua_register(L, "StartCutscene", StartCutscene);
	lua_register(L, "StopCutscene", StopCutscene);
	lua_register(L, "GetCutsceneRunning", GetCutsceneRunning);
	lua_register(L, "GetCutsceneTime", GetCutsceneTime);
	lua_register(L, "CutSetCutsceneTableSize", CutSetCutsceneTableSize);
	lua_register(L, "CutSetCutsceneName", CutSetCutsceneName);
	lua_register(L, "SystemShouldMovePlayer", SystemShouldMovePlayer);
	lua_register(L, "SystemResetShouldMovePlayer", SystemResetShouldMovePlayer);
	lua_register(L, "PlayerSetPosXYZArea", PlayerSetPosXYZArea);
	lua_register(L, "DATLoad", DATLoad);
	lua_register(L, "DATUnload", DATUnload);
	lua_register(L, "DATInit", DATInit);
	lua_register(L, "GetPointFromPointList", GetPointFromPointList);
	lua_register(L, "GetPointList", GetPointList);
	lua_register(L, "PlayerSetPosPoint", PlayerSetPosPoint);
	lua_register(L, "PlayerSetPosXYZ", PlayerSetPosXYZ);
	lua_register(L, "PlayerSetPosPointArea", PlayerSetPosPointArea);
	lua_register(L, "PlayerIsInTrigger", PlayerIsInTrigger);
	lua_register(L, "PedIsInTrigger", PedIsInTrigger);
	lua_register(L, "PlayerIsInAreaXYZ", PlayerIsInAreaXYZ);
	lua_register(L, "PedIsInAreaXYZ", PedIsInAreaXYZ);
	lua_register(L, "AreaGetVisible", AreaGetVisible);
	lua_register(L, "PlayerGetPosXYZ", PlayerGetPosXYZ);
	lua_register(L, "PedCreatePoint", PedCreatePoint);
	lua_register(L, "AreaDisablePatrolPath", AreaDisablePatrolPath);
	lua_register(L, "AreaEnablePatrolPath", AreaEnablePatrolPath);
	lua_register(L, "AreaInitPatrolPath", AreaInitPatrolPath);
	lua_register(L, "AreaEnableAllPatrolPaths", AreaEnableAllPatrolPaths);
	lua_register(L, "AreaDisableAllPatrolPaths", AreaDisableAllPatrolPaths);
	lua_register(L, "AreaClearPatrolPaths", AreaClearPatrolPaths);
	lua_register(L, "PedSetInvulnerable", PedSetInvulnerable);
	lua_register(L, "PedSetInvulnerableToPlayer", PedSetInvulnerableToPlayer);
	lua_register(L, "PlayerSetInvulnerable", PlayerSetInvulnerable);
	lua_register(L, "PedSetMinHealth", PedSetMinHealth);
	lua_register(L, "PedGetMaxHealth", PedGetMaxHealth);
	lua_register(L, "LoadAnimationGroup", LoadAnimationGroup);
	lua_register(L, "HasAnimGroupLoaded", HasAnimGroupLoaded);
	lua_register(L, "UnLoadAnimationGroup", UnLoadAnimationGroup);
	lua_register(L, "PedSetMaxHealth", PedSetMaxHealth);
	lua_register(L, "GameGetPedStat", GameGetPedStat);
	lua_register(L, "PedSetFightingSystem", PedSetFightingSystem);
	lua_register(L, "GameSetPedStat", GameSetPedStat);
	lua_register(L, "PedOverrideStat", PedOverrideStat);
	lua_register(L, "PedSetDefaultTypeToTypeAttitude", PedSetDefaultTypeToTypeAttitude);
	lua_register(L, "PedSetTypeToTypeAttitude", PedSetTypeToTypeAttitude);
	lua_register(L, "PedGetTypeToTypeAttitude", PedGetTypeToTypeAttitude);
	lua_register(L, "PedSetPedToTypeAttitude", PedSetPedToTypeAttitude);
	lua_register(L, "PedGetPedToTypeAttitude", PedGetPedToTypeAttitude);
	lua_register(L, "SetFactionRespect", SetFactionRespect);
	lua_register(L, "GetFactionRespect", GetFactionRespect);
	lua_register(L, "PedSetFlag", PedSetFlag);
	lua_register(L, "PedGetFlag", PedGetFlag);
	lua_register(L, "PedCreateXYZ", PedCreateXYZ);
	lua_register(L, "PedDelete", PedDelete);
	lua_register(L, "PedIsValid", PedIsValid);
	lua_register(L, "PedIsPlayer", PedIsPlayer);
	lua_register(L, "PlayerGetPedIndex", PlayerGetPedIndex);
	lua_register(L, "PedGetPosXYZ", PedGetPosXYZ);
	lua_register(L, "PedSetPosXYZ", PedSetPosXYZ);
	lua_register(L, "PedSetPosPoint", PedSetPosPoint);
	lua_register(L, "PedFaceObject", PedFaceObject);
	lua_register(L, "PedFaceObjectNow", PedFaceObjectNow);
	lua_register(L, "PedFaceHeading", PedFaceHeading);
	lua_register(L, "GiveWeaponToPlayer", GiveWeaponToPlayer);
	lua_register(L, "GiveAmmoToPlayer", GiveAmmoToPlayer);
	lua_register(L, "PedGetAmmoCount", PedGetAmmoCount);
	lua_register(L, "WeaponGetType", WeaponGetType);
	lua_register(L, "IsItemAWeapon", IsItemAWeapon);
	lua_register(L, "PedSetWeapon", PedSetWeapon);
	lua_register(L, "PedSetWeaponNow", PedSetWeaponNow);
	lua_register(L, "PlayerSetWeapon", PlayerSetWeapon);
	lua_register(L, "PedClearWeapon", PedClearWeapon);
	lua_register(L, "PedClearAllWeapons", PedClearAllWeapons);
	lua_register(L, "PedDestroyWeapon", PedDestroyWeapon);
	lua_register(L, "PedGetWeapon", PedGetWeapon);
	lua_register(L, "PedHasWeapon", PedHasWeapon);
	lua_register(L, "PlayerHasWeapon", PlayerHasWeapon);
	lua_register(L, "WeaponEquipped", WeaponEquipped);
	lua_register(L, "PlayerUnequip", PlayerUnequip);
	lua_register(L, "PlayerSetPosSimple", PlayerSetPosSimple);
	lua_register(L, "PlayerFaceHeadingNow", PlayerFaceHeadingNow);
	lua_register(L, "PlayerWeaponHudLock", PlayerWeaponHudLock);
	lua_register(L, "ObjectNumProjectileImpacts", ObjectNumProjectileImpacts);
	lua_register(L, "AreaClearAllProjectiles", AreaClearAllProjectiles);
	lua_register(L, "ObjectNameToHashID", ObjectNameToHashID);
	lua_register(L, "RegisterGlobalEventHandler", RegisterGlobalEventHandler);
	lua_register(L, "PedGetLastHitWeapon", PedGetLastHitWeapon);
	lua_register(L, "PedGetWhoHitMeLast", PedGetWhoHitMeLast);
	lua_register(L, "PedIsHit", PedIsHit);
	lua_register(L, "SoundStopCurrentSpeechEvent", SoundStopCurrentSpeechEvent);
	lua_register(L, "PedRestrictToTrigger", PedRestrictToTrigger);
	lua_register(L, "PedSetInfiniteSprint", PedSetInfiniteSprint);
	lua_register(L, "PedCanTeleportOnAreaTransition", PedCanTeleportOnAreaTransition);
	lua_register(L, "PedFindInAreaXYZ", PedFindInAreaXYZ);
	lua_register(L, "PedFindInAreaObject", PedFindInAreaObject);
	lua_register(L, "PedSetMissionCritical", PedSetMissionCritical);
	lua_register(L, "PedRecruitAlly", PedRecruitAlly);
	lua_register(L, "PedHasAlly", PedHasAlly);
	lua_register(L, "PedIsModel", PedIsModel);
	lua_register(L, "PedDismissAlly", PedDismissAlly);
	lua_register(L, "PedCanSeeObject", PedCanSeeObject);
	lua_register(L, "MissionTimerStart", MissionTimerStart);
	lua_register(L, "MissionTimerStop", MissionTimerStop);
	lua_register(L, "MissionTimerHasFinished", MissionTimerHasFinished);
	lua_register(L, "MissionTimerGetTimeRemaining", MissionTimerGetTimeRemaining);
	lua_register(L, "GetPointListSize", GetPointListSize);
	lua_register(L, "PickupCreateXYZ", PickupCreateXYZ);
	lua_register(L, "PickupDelete", PickupDelete);
	lua_register(L, "PickupCreateFromPed", PickupCreateFromPed);
	lua_register(L, "PAnimRequest", PAnimRequest);
	lua_register(L, "PAnimCreate", PAnimCreate);
	lua_register(L, "AddBlipForChar", AddBlipForChar);
	lua_register(L, "BlipAddPoint", BlipAddPoint);
	lua_register(L, "BlipRemove", BlipRemove);
	lua_register(L, "BlipAddXYZ", BlipAddXYZ);
	lua_register(L, "CreatePersistentEntity", CreatePersistentEntity);
	lua_register(L, "DeletePersistentEntity", DeletePersistentEntity);
	lua_register(L, "ObjectRemovePickupsInTrigger", ObjectRemovePickupsInTrigger);
	lua_register(L, "ItemGetCurrentNum", ItemGetCurrentNum);
	lua_register(L, "ItemSetCurrentNum", ItemSetCurrentNum);
	lua_register(L, "CollectiblesSetTypeAvailable", CollectiblesSetTypeAvailable);
	lua_register(L, "CollectiblesSetAllAsCollected", CollectiblesSetAllAsCollected);
	lua_register(L, "CollectiblesGetNumCollectable", CollectiblesGetNumCollectable);
	lua_register(L, "CollectiblesGetNumCollected", CollectiblesGetNumCollected);
	lua_register(L, "CollectiblesOnCollectedCallback", CollectiblesOnCollectedCallback);
	lua_register(L, "RegisterHashEventHandler", RegisterHashEventHandler);
	lua_register(L, "SetNumberOfHandledHashEventObjects", SetNumberOfHandledHashEventObjects);
	lua_register(L, "ObjectToggleVulnerability", ObjectToggleVulnerability);
	lua_register(L, "ObjectGetModelIndex", ObjectGetModelIndex);
	lua_register(L, "PAnimDestroyedByPed", PAnimDestroyedByPed);
	lua_register(L, "PAnimExists", PAnimExists);
	lua_register(L, "PAnimOpenDoor", PAnimOpenDoor);
	lua_register(L, "PAnimOpenDoorImmediate", PAnimOpenDoorImmediate);
	lua_register(L, "PAnimCloseDoor", PAnimCloseDoor);
	lua_register(L, "PAnimDoorStayOpen", PAnimDoorStayOpen);
	lua_register(L, "PAnimIsOpen", PAnimIsOpen);
	lua_register(L, "MissionObjectiveAdd", MissionObjectiveAdd);
	lua_register(L, "MissionObjectiveComplete", MissionObjectiveComplete);
	lua_register(L, "MissionObjectiveRemove", MissionObjectiveRemove);
	lua_register(L, "MinigameSetCompletion", MinigameSetCompletion);
	lua_register(L, "MinigameAddCompletionMsg", MinigameAddCompletionMsg);
	lua_register(L, "MinigameIsShowingCompletion", MinigameIsShowingCompletion);
	lua_register(L, "PAnimSetActionNode", PAnimSetActionNode);
	lua_register(L, "PAnimSetPropFlag", PAnimSetPropFlag);
	lua_register(L, "PAnimGetPropFlag", PAnimGetPropFlag);
	lua_register(L, "AreaRegisterAnimProps", AreaRegisterAnimProps);
	lua_register(L, "PedGetOffsetInWorldCoords", PedGetOffsetInWorldCoords);
	lua_register(L, "PedSetStationary", PedSetStationary);
	lua_register(L, "RegisterPedEventHandler", RegisterPedEventHandler);
	lua_register(L, "RegisterTriggerEventHandler", RegisterTriggerEventHandler);
	lua_register(L, "SetNumberOfHandledTriggerEventObjects", SetNumberOfHandledTriggerEventObjects);
	lua_register(L, "AreaSetTriggerMonitoringRules", AreaSetTriggerMonitoringRules);
	lua_register(L, "PedAttackPlayer", PedAttackPlayer);
	lua_register(L, "PedClearObjectives", PedClearObjectives);
	lua_register(L, "PedMakeTargetable", PedMakeTargetable);
	lua_register(L, "PedGetHeading", PedGetHeading);
	lua_register(L, "PedFaceXYZ", PedFaceXYZ);
	lua_register(L, "PedIsDead", PedIsDead);
	lua_register(L, "PedGetHealth", PedGetHealth);
	lua_register(L, "PedSetHealth", PedSetHealth);
	lua_register(L, "PedFollowPath", PedFollowPath);
	lua_register(L, "PathGetLastNode", PathGetLastNode);
	lua_register(L, "PedPathNodeReachedDistance", PedPathNodeReachedDistance);
	lua_register(L, "DistanceBetweenCoords3d", DistanceBetweenCoords3d);
	lua_register(L, "DistanceBetweenCoords2d", DistanceBetweenCoords2d);
	lua_register(L, "MissionForceCompleted", MissionForceCompleted);
	lua_register(L, "PedSetImmortalFlag", PedSetImmortalFlag);
	lua_register(L, "PedGetImmortalFlag", PedGetImmortalFlag);
	lua_register(L, "PlayerGetHealth", PlayerGetHealth);
	lua_register(L, "PedMoveToPoint", PedMoveToPoint);
	lua_register(L, "PedMoveToXYZ", PedMoveToXYZ);
	lua_register(L, "PedStop", PedStop);
	lua_register(L, "PedAttack", PedAttack);
	lua_register(L, "PedSetAITree", PedSetAITree);
	lua_register(L, "PedSetTetherToPoint", PedSetTetherToPoint);
	lua_register(L, "PedIgnoreStimuli", PedIgnoreStimuli);
	lua_register(L, "AreaSetDoorLocked", AreaSetDoorLocked);
	lua_register(L, "AreaSetDoorLockedToPeds", AreaSetDoorLockedToPeds);
	lua_register(L, "AreaSetDoorOpen", AreaSetDoorOpen);
	lua_register(L, "AreaIsDoorLockedToPeds", AreaIsDoorLockedToPeds);
	lua_register(L, "AreaIsDoorOpen", AreaIsDoorOpen);
	lua_register(L, "SetAmbientPedsIgnoreStimuli", SetAmbientPedsIgnoreStimuli);
	lua_register(L, "PedClearTether", PedClearTether);
	lua_register(L, "EnterNIS", EnterNIS);
	lua_register(L, "ExitNIS", ExitNIS);
	lua_register(L, "PedSetGlobalAttitude_Rumble", PedSetGlobalAttitude_Rumble);
	lua_register(L, "PedSetFaction", PedSetFaction);
	lua_register(L, "ChapterSet", ChapterSet);
	lua_register(L, "PlayerGetPhysicalState", PlayerGetPhysicalState);
	lua_register(L, "ChapterGet", ChapterGet);
	lua_register(L, "PedSetControllerID", PedSetControllerID);
	lua_register(L, "PedSetAsleep", PedSetAsleep);
	lua_register(L, "ClothingOverlay", ClothingOverlay);
	lua_register(L, "ClothingDefineOutfit", ClothingDefineOutfit);
	lua_register(L, "ClothingSetPlayerOutfit", ClothingSetPlayerOutfit);
	lua_register(L, "ClothingIsWearingOutfit", ClothingIsWearingOutfit);
	lua_register(L, "ClothingIsWearingAnyOutfit", ClothingIsWearingAnyOutfit);
	lua_register(L, "PedSetGlobalSleep", PedSetGlobalSleep);
	lua_register(L, "CameraGetActive", CameraGetActive);
	lua_register(L, "CameraSetActive", CameraSetActive);
	lua_register(L, "CameraReturnToPlayer", CameraReturnToPlayer);
	lua_register(L, "CameraSetWidescreen", CameraSetWidescreen);
	lua_register(L, "ToggleHUDComponentVisibility", ToggleHUDComponentVisibility);
	lua_register(L, "ToggleHUDComponentLocked", ToggleHUDComponentLocked);
	lua_register(L, "HUDComponentGetVisibility", HUDComponentGetVisibility);
	lua_register(L, "HUDComponentGetLocked", HUDComponentGetLocked);
	lua_register(L, "HUDComponentGetFlashing", HUDComponentGetFlashing);
	lua_register(L, "HUDClearAllElements", HUDClearAllElements);
	lua_register(L, "HUDSaveVisibility", HUDSaveVisibility);
	lua_register(L, "HUDRestoreVisibility", HUDRestoreVisibility);
	lua_register(L, "PedSetIsStealthMissionPed", PedSetIsStealthMissionPed);
	lua_register(L, "PedGetFaction", PedGetFaction);
	lua_register(L, "PedSetCombatZoneMask", PedSetCombatZoneMask);
	lua_register(L, "PedMakeAmbient", PedMakeAmbient);
	// Les énumérations posées par 0x5db040 : POINTLIST, PATH et TRIGGER cherchent dans les pools.
	PoserEnumeration(L, "POINTLIST", IndexPointList);
	PoserEnumeration(L, "PATH", IndexPath);
	PoserEnumeration(L, "TRIGGER", IndexTrigger);
	PoserEnumeration(L, "MODELENUM", IndexModelEnum);
	PoserEnumeration(L, "POI", IndexPoi);
	if(modeles.Charger()) printf("%zu noms de modèles dans Objects/ide.img\n", modeles.Nombre());
	else printf("Objects\\ide.img introuvable : MODELENUM répondra -1\n");
	ChargerArmes();
	ChargerCollectibles();   // CGame::Initialise : les collectibles avant les placements
	ChargerObjets();
	ChargerPedStats();
	ChargerMissions();
	// Le joueur, créé avec le monde (CPlayerPed, modèle 0) : FUN_00484d10 lui pose aussi les arbres de son modèle,
	// /Global/Player (Act/Player.act) et /Global/PlayerAI.
	PoserArbresDuModele(SCRIPT_PED_JOUEUR, 0);
	ChargerApi();
	lua_newtable(L);
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, IndexGlobal);
	lua_settable(L, -3);
	lua_setmetatable(L, LUA_GLOBALSINDEX);

	lua_pushcfunction(L, ImportScript);
	lua_pushstring(L, "util.lua");
	if(lua_pcall(L, 1, 1, 0) != 0 || lua_pcall(L, 0, 0, 0) != 0){
		printf("util.lua : %s\n", lua_tostring(L, -1));
		return 1;
	}

	if(NouveauScript(L, fichier) < 0){ printf("CreateNameSpace(%s) a échoué\n", fichier); return 1; }
	printf("%s chargé dans son espace de noms ; lancement de %s\n", fichier, fonction);
	if(NouveauThread(L, 0, fonction) == 0){ printf("ThreadNameSpace(%s, %s) a échoué\n", fichier, fonction); return 1; }

	// La boucle de jeu : une image toutes les 33 ms.
	int images = 0;
	for(maintenant = 0; maintenant < (uint32)(secondes * 1000); maintenant += 33, images++){
		horloge.Avancer(0.033f);
		imageCourante = (uint32)images;
		groupesAnim.Avancer(imageCourante);
		if(aLancer && maintenant >= 1000){
			lua_pushcfunction(L, ForceStartMission);
			lua_pushstring(L, aLancer);
			lua_call(L, 1, 0);
			aLancer = nil;
		}
		for(auto &e : entrees) if(!e.second.empty() && maintenant >= e.first){ EntrerDansDeclencheur(e.second.c_str()); e.second.clear(); }
		for(auto &e : allers) if(!e.second.empty() && maintenant >= e.first){ AllerAuPoint(e.second.c_str()); e.second.clear(); }
		for(auto &e : visees) if(e.second >= 0 && maintenant >= e.first && controleJoueur != 1){
			printf("%6.2f s  la manette est coupée : le joueur ne vise pas le piéton %d\n", maintenant / 1000.0, e.second);
			e.second = -1;
		}
		for(uint32 &e : prises) if(e != 0xffffffffu && maintenant >= e){
			if(controleJoueur != 1) printf("%6.2f s  la manette est coupée : le joueur ne ramasse rien\n", maintenant / 1000.0);
			else Ramasser();
			e = 0xffffffffu;
		}
		for(uint32 &e : tirsManette) if(e != 0xffffffffu && maintenant >= e){
			CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
			if(controleJoueur != 1) printf("%6.2f s  la manette est coupée : le joueur ne tire pas\n", maintenant / 1000.0);
			else if(j->armeTenue == -1) printf("%6.2f s  le joueur n'a pas d'arme en main : il ne tire pas\n", maintenant / 1000.0);
			else{   // la piste WeaponFire de Fire/SLINGSHOT (FUN_00661f10) : le tir, puis une munition retirée sans le drapeau 24
				Tirer(SCRIPT_PED_JOUEUR, 0.0f);
				if(j->drapeaux[24] == 0) inventaire.Retirer(armes, SCRIPT_PED_JOUEUR, j->armeMunition, 1, true, modeles.Index("RBandBall"));
				// l'octet +0x32 des pistes WeaponFire du lance-pierre et des armes lancées : l'arme lâchée quand il n'en reste plus
				if(j->armeTenue == j->armeMunition && inventaire.Munitions(armes, SCRIPT_PED_JOUEUR, j->armeMunition) == 0) LacherArme(SCRIPT_PED_JOUEUR);
			}
			e = 0xffffffffu;
		}
		for(auto &e : objetsVises) if(!e.second.empty() && maintenant >= e.first){
			objetVise = -1;
			uint32 h = ActionHash(e.second.c_str());
			for(size_t k = 0; k < objets.size(); k++) if(objets[k].nom == h && objets[k].verrouillable && !objets[k].detruit) objetVise = PoigneeObjet(k);
			if(e.second == "-") printf("%6.2f s  le joueur ne vise plus d'objet\n", maintenant / 1000.0);
			else if(objetVise < 0) printf("%6.2f s  le joueur ne trouve pas d'objet à viser nommé %s\n", maintenant / 1000.0, e.second.c_str());
			else printf("%6.2f s  le joueur vise l'objet %d (%s, modèle %d)\n", maintenant / 1000.0, objetVise, e.second.c_str(), ObjetParPoignee(objetVise)->modele);
			e.second.clear();
		}
		for(auto &e : choixArmes) if(e.second >= -1 && maintenant >= e.first){
			if(controleJoueur != 1) printf("%6.2f s  la manette est coupée : le joueur ne choisit pas l'arme %d\n", maintenant / 1000.0, e.second);
			else if(hudArmesVerrouille) printf("%6.2f s  le HUD des armes est verrouillé : le joueur ne choisit pas l'arme %d\n", maintenant / 1000.0, e.second);
			else{
				PoserArmeHud(e.second);
				printf("%6.2f s  le joueur choisit l'arme %d au HUD : %d\n", maintenant / 1000.0, e.second, armeHud);
			}
			e.second = -2;
		}
		for(auto &e : visees) if(e.second >= 0 && maintenant >= e.first){
			bool pris = pietons.Verrouiller(SCRIPT_PED_JOUEUR, e.second, 0);
			printf("%6.2f s  le joueur vise le piéton %d%s\n", maintenant / 1000.0, e.second, pris ? "" : " : refusé");
			e.second = -1;
		}
		// Une porte DT_ poussée : FUN_00415160 pose l'objet de porte en cours (+0x394) ; le moteur le fait depuis
		// l'animation de la porte (FUN_00610ed0, non jouée), l'outil le fait au nom du déclencheur.
		for(auto &e : gestes) if(!e.second.empty() && maintenant >= e.first && controleJoueur != 1){
			printf("%6.2f s  la manette est coupée : le joueur ne joue pas %s\n", maintenant / 1000.0, e.second.c_str());
			e.second.clear();
		}
		for(auto &e : gestes) if(!e.second.empty() && maintenant >= e.first){
			// Ce que la manette ferait faire au contrôleur du joueur (une parade, un coup) : le nœud devient le
			// nœud joué (+0x10), que PedIsPlaying compare.
			// CHEMIN:FICHIER.act : le fichier est chargé s'il manque (FUN_004849a0), pour un arbre que le joueur
			// n'atteint que par une référence (GlobalLoco depuis Player).
			std::string chemin = e.second, act;
			size_t dp = chemin.find(':');
			if(dp != std::string::npos){ act = chemin.substr(dp + 1); chemin = chemin.substr(0, dp); }
			arbres.TrouverOuCharger(chemin.c_str(), act.c_str());
			const CActionNode *n = arbres.Trouver(chemin.c_str());
			if(n == nil) printf("%6.2f s  le joueur joue %s : nœud absent de l'arbre global\n", maintenant / 1000.0, e.second.c_str());
			else {
				CActionController &c = controleurs[SCRIPT_PED_JOUEUR];
				c.parLaManette = true;   // le nœud tient lieu d'un bouton tenu (src/core/ActionController.cpp, Jouer)
				if(!c.Entrer(arbres, n, CJugeOutil(SCRIPT_PED_JOUEUR, maintenant / 1000.0f), maintenant / 1000.0f)){
					// Une banque dont la descente jugée n'aboutit pas (conditions de visée, de bouton) : la manette de l'outil
					// prend le premier nœud jouable sans condition parmi ses enfants (HeavyAttacks → son coup de pied).
					const CActionNode *d = nil;
					if(n->genre == 'b') for(const CActionNode &e : n->enfants) if((d = arbres.Choisir(&e)) != nil) break;
					c.Jouer(arbres, d ? d : n, maintenant / 1000.0f);
				}
				c.parLaManette = false;
				printf("%6.2f s  le joueur joue %s%s\n", maintenant / 1000.0, chemin.c_str(), c.joue != n ? (" → " + CheminVu(c, c.joue)).c_str() : "");
			}
			e.second.clear();
		}
		for(uint32 &q : sequences) if(q != 0xffffffffu && maintenant >= q){ if(controleJoueur == 1) ReussirSequence(L); q = 0xffffffffu; }
		// La manette : une ActionRequest en attente un quart de seconde, que le contrôleur du joueur juge.
		bool demande = false;
		for(auto &e : attentes) if(e.first != 0xffffffffu && maintenant >= e.first){
			if(controleJoueur != 1){ e.first = 0xffffffffu; continue; }
			demandes[e.second] = maintenant + 250;
			printf("%6.2f s  le joueur demande l'action %u\n", maintenant / 1000.0, e.second);
			e.first = 0xffffffffu; demande = true;
		}
		// Les contrôleurs de l'IA d'abord (FUN_00484490) : l'intention qu'ils posent se lit dans l'arbre d'actions à la
		// même image.
		for(auto &c : controleursIA) AvancerIA(c.first, c.second, (uint32)images);
		// Les contrôleurs d'actions avancent (src/core/ActionController.h) ; on journalise ceux du joueur.
		for(auto &c : controleurs){
			const CActionNode *avant = c.second.joue;
			// Le tirage des Opportunity à période (FUN_00465910) : le compteur d'images du jeu (DAT_00c1a99c) et le
			// mot +0x10c du piéton, que l'outil n'a pas : son numéro le remplace.
			c.second.image = (uint32)images;
			c.second.graine = (uint16)c.first;
			bool d = c.first == SCRIPT_PED_JOUEUR && (demande || !demandes.empty());
			// FUN_00484040 : sans nœud joué (+0x268), le contrôleur redescend depuis la racine de son contexte (+0x340),
			// s'il a un arbre (+0x4b0), avant d'avancer.
			if(c.second.joue == nil && c.second.arbre){
				c.second.Poser(arbres, c.second.arbre, CJugeOutil(c.first, maintenant / 1000.0f), maintenant / 1000.0f);
				if(c.second.joue && (c.first == SCRIPT_PED_JOUEUR || tousLesControleurs))
					printf("%6.2f s  contrôleur de %d : %s\n", maintenant / 1000.0, c.first, CheminVu(c.second, c.second.joue).c_str());
			}
			if(c.second.Avancer(arbres, nil, CJugeOutil(c.first, maintenant / 1000.0f), maintenant / 1000.0f, d) && c.second.joue != avant){
				if(c.first == SCRIPT_PED_JOUEUR)
					printf("%6.2f s  contrôleur du joueur : %s\n", maintenant / 1000.0, CheminVu(c.second, c.second.joue).c_str());
				else if(tousLesControleurs)
					printf("%6.2f s  contrôleur de %d : %s\n", maintenant / 1000.0, c.first, CheminVu(c.second, c.second.joue).c_str());
			}
			// Les coups qui portent (pistes Collision, FUN_005f9e70) : l'outil n'a pas les sphères des os, le coup touche la
			// cible verrouillée. La cible reçoit le coup (slot 15 de CPed, FUN_0047b810) : son drapeau tient une seconde
			// (ConditionFlag, HitLocation) et, sans nœud de coup donné, son contrôleur descend dans l'arbre des coups
			// (+0x4b8 du contrôleur ; dans l'outil, /Global/HitTree). L'attaquant joue le nœud +0x68 de la piste
			// (FUN_005f4490), s'il en a un.
			for(const CActionController::Contact &k : c.second.touches){
				const CScriptPed *p = pietons.Acceder(c.first);
				if(p == nil || p->verrou < 0) continue;
				int cible = p->verrou;
				// Supposé : le corps projeté par une prise (PushFwd/RCV, « FRIENDLYHIT ») ne touche pas celui qui le pousse ;
				// le moteur l'écarte par la géométrie, l'outil par le partenaire du PlayOnTarget (deux secondes).
				auto pt = partenaires.find(c.first);
				if(pt != partenaires.end() && pt->second.first == cible && maintenant <= pt->second.second) continue;
				// Supposé aussi : faute des sphères des os (FUN_005f9e70 cherche le contact des sphères de l'attaquant avec les
				// corps proches), un coup ne touche qu'une cible devant l'attaquant (à moins de 90 degrés de son cap). Le
				// bully qui fuit après son humiliation (HumiliateFlee/…/RunStart : FaceTarget décalé de 180 degrés) tombe en
				// avant (KnockOuts/Rear/Rear) : sa collision FRIENDLYHIT ne touche plus le joueur resté dans son dos.
				if(const CScriptPed *q = pietons.Acceder(cible)){
					float dx = q->pos.x - p->pos.x, dy = q->pos.y - p->pos.y, a = p->cap * 3.14159265f / 180.0f;
					float n = sqrtf(dx * dx + dy * dy), cosinus = n > 0.0f ? (cosf(a) * dx + sinf(a) * dy) / n : 1.0f;
					if(cosinus <= 0.0f){
						printf("%6.2f s  le coup de %d (%s) manque %d, dans son dos\n", maintenant / 1000.0, c.first, CheminVu(c.second, k.porteur).c_str(), cible);
						continue;
					}
				}
				// FUN_0047b810 : les dégâts de la piste (+0x34) ; un coup du joueur (type 13) sans arme (-1) est multiplié par
				// PlayerButes +0xac / +0xb0 / +0xb4 selon l'état physique du joueur (DAT_00c1af4c, PlayerGetPhysicalState ;
				// l'outil : 0) ; puis, sans arme, arrondi au-dessus après le facteur +0x1cd4 de l'attaquant (FUN_0085dbc0)
				// et au-dessous après le facteur +0x1cfc de la cible (FUN_0085d540), tous deux à 1 depuis le constructeur
				// (FUN_0047ffe0 ; FUN_00426560, qui les change, n'est pas suivi).
				float degats = k.degats * (c.first == SCRIPT_PED_JOUEUR ? facteurCoupsJoueur : 1.0f);
				degats = floorf(ceilf(degats * 1.0f) * 1.0f);
				CoupRecu &r = coups[cible];
				float t = maintenant / 1000.0f;
				r.precedent = r.instant >= 0.0f ? t - r.instant : -1.0f;  // FUN_00472b70 : +0x434 passe en +0x438 (-1 sans coup)
				r.instant = t;
				r.drapeau = k.drapeau & 0x7fffffff; r.jusqua = t + 1.0f; r.degats = degats; r.attaquant = c.first;
				r.arme = p->armeTenue; r.genre = 0;
				if(k.agresseur) pietons.Agresse(cible, c.first);   // FUN_0047b810 → FUN_004973d0 (le 15e argument, +0x214)
				CActionController &v = controleurs[cible];
				// Un piéton à terre (son nœud joué s'appelle On_Ground, ou un parent) : dans le moteur, la réaction passe par
				// HitTree/BASE/HitTreeBase/…/On_Ground (« si MePlaying… », joué par un Execute des nœuds de coup), dont
				// l'Opportunity mène à GroundAndWallHits/On_Ground. L'outil ne joue pas les Execute en contrôleur
				// secondaire : il y entre directement (supposé). Sinon, la racine de HitTree.
				bool aTerre = arbres.JoueHachage(NoeudJoue(cible), ActionHash("On_Ground"));
				const CActionNode *arbreCoups = arbres.Trouver(aTerre ? "/Global/HitTree/GroundAndWallHits/On_Ground" : "/Global/HitTree");
				bool pris = arbreCoups && v.Entrer(arbres, arbreCoups, CJugeOutil(cible, maintenant / 1000.0f), maintenant / 1000.0f);
				printf("%6.2f s  le coup de %d porte sur %d (%s)%s%s\n", maintenant / 1000.0, c.first, cible, CheminVu(c.second, k.porteur).c_str(),
				       pris ? " : il joue " : "", pris ? CheminVu(v, v.joue).c_str() : "");
				if(pietons.CoupRecevable(cible, c.first)) PrevenirCoupRecu(cible, c.first);   // FUN_0047b810 → FUN_005b2190
				if(!k.surCoup.empty())
					if(const CActionNode *n = arbres.Relatif(k.porteur, k.surCoup.c_str()))
						c.second.Entrer(arbres, n, CJugeOutil(c.first, maintenant / 1000.0f), maintenant / 1000.0f);
			}
			// Les pistes DamageHandle qui se lancent (FUN_0060fc70) : les dégâts du coup reçu (+0x440) multipliés par la
			// piste, portés par l'attaquant du relevé (+0x40c), par FUN_0047b7f0 → FUN_0047b6b0 → FUN_004782a0
			// (CScriptPedPool::Blesser : invulnérabilités, santé minimale). Le cas de FUN_004255b0 (la cible tombe
			// d'un coup du joueur) n'est pas suivi.
			for(const CActionController::Blessure &b : c.second.blessures){
				auto r = coups.find(c.first);
				if(r == coups.end() || b.facteur == 0.0f) continue;
				const CScriptPed *p = pietons.Acceder(c.first);
				float avant = p ? p->sante : 0.0f;
				bool porte = pietons.Blesser(c.first, r->second.degats * b.facteur, r->second.attaquant);
				printf("%6.2f s  %d subit %.0f × %g (%s) : %s, %.0f → %.0f points de vie\n", maintenant / 1000.0, c.first,
				       r->second.degats, b.facteur, arbres.CheminDe(b.porteur).c_str(), porte ? "porté" : "rien", avant, p ? p->sante : 0.0f);
			}
			// SetPedFlags (FUN_0060f8e0) : le drapeau du piéton (+0x1314 + 4n).
			if(CScriptPed *p = pietons.Acceder(c.first))
				for(const CActionController::Drapeau &d : c.second.drapeauxPoses) if(d.numero < SCRIPT_PED_DRAPEAUX) p->drapeaux[d.numero] = d.valeur;
			c.second.drapeauxPoses.clear();
			// PropFlags (FUN_00655b50) : l'objet du contexte du piéton (FUN_004eaa60). PropSetPedAction (FUN_006561a0) écrit dans le
			// +0xc du contexte, qu'un contexte de piéton n'a pas : rien. PropInteract (FUN_00656120) : Interagir.
			{
				int o = CJugeOutil(c.first, maintenant / 1000.0f).ObjetDuContexte();
				for(const CActionController::DrapeauObjet &d : c.second.drapeauxObjetPoses) objetsAnimes.PoserDrapeau(o, d.numero, d.valeur);
				c.second.drapeauxObjetPoses.clear();
				c.second.actionsObjetPosees.clear();
				for(; c.second.interactionsLancees > 0; c.second.interactionsLancees--) Interagir(c.first, c.second);
			}
			AppliquerMorts(c.first, c.second);
			AppliquerDegatsCible(c.first, c.second);
			AppliquerDroits(c.first, c.second);
			// SetWeaponInfo (FUN_00660f50) : le +0x120 de l'objet arme du piéton.
			AppliquerArmes(c.first, c.second);   // avant SetWeaponInfo : PropAttach remet +0x120, la banque NeedInit qui suit le pose
			if(CScriptPed *p = pietons.Acceder(c.first))
				for(const CActionController::InfoArme &d : c.second.infosArmeLancees) PoserInfoArme(*p, d);
			AppliquerTirs(c.first, c.second);
			// SpeechTrack (FUN_006195a0) : une ligne à tenir est retenue ; les autres ne sont pas jouées par l'outil (une
			// seule ligne à la fois, que les scripts attendent). SpeechStartTrack (FUN_00619950) lâche les lignes du piéton.
			for(const CActionController::Parole &e : c.second.parolesLancees){
				if(e.tenue == 0 || e.evenement < 0 || e.evenement >= NUM_SPEECH_EVENTS) continue;
				for(auto it = parolesTenues.begin(); it != parolesTenues.end(); )
					it = it->ped == c.first ? parolesTenues.erase(it) : it + 1;
				parolesTenues.push_back({c.first, e.evenement, e.ligne, e.tenue, imageCourante, maintenant + 12000});
				printf("%6.2f s  le piéton %d retient %s ligne %d\n", maintenant / 1000.0, c.first, kSpeechEvents[e.evenement], e.ligne);
			}
			c.second.parolesLancees.clear();
			// ActionRegisterButtonHistory (FUN_0061bab0 → FUN_0053b340) : le bouton et l'état rapportés à l'historique des boutons.
			for(const CActionController::BoutonHistorique &b : c.second.boutonsLances){
				historiqueBoutons.Rapporter(b.bouton, b.etat, (uint32)maintenant);
				if(historiqueBoutons.parArbre)
					printf("%6.2f s  le piéton %d rapporte le bouton %d (état %d) à l'historique des boutons\n", maintenant / 1000.0, c.first, b.bouton, b.etat);
			}
			c.second.boutonsLances.clear();
			for(; c.second.departsLances > 0; c.second.departsLances--)
				for(auto it = parolesTenues.begin(); it != parolesTenues.end(); ){
					if(it->ped != c.first){ ++it; continue; }
					parole.enCours = true; parole.ped = it->ped; parole.evenement = it->evenement;
					parole.ligne = it->ligne < 0 ? 0 : it->ligne; parole.id++;
					parole.fin = maintenant + DureeParole(parole.ped, parole.evenement, parole.ligne);
					printf("%6.2f s  parole : le piéton %d dit %s ligne %d (lâchée par SpeechStartTrack)\n", maintenant / 1000.0, it->ped, kSpeechEvents[it->evenement], it->ligne);
					it = parolesTenues.erase(it);
				}
			// FaceTarget (FUN_00620dc0) et SeekEntityPosition (FUN_0062a940) : le piéton se tourne vers sa cible, ou y va (à
			// la course de l'outil) jusqu'à la distance d'arrivée. L'outil n'a ni chemins ni virages progressifs.
			if(CScriptPed *p = pietons.Acceder(c.first)){
				auto cibleDe = [&](int genre){ return genre == 0 ? p->verrou : genre == 1 ? p->cibleDePrise : -1; };
				for(const CActionController::Poursuite &r : c.second.poursuitesActives){
					const CScriptPed *q = pietons.Acceder(cibleDe(r.genre));
					if(q == nil || q == p) continue;
					float dx = q->pos.x - p->pos.x, dy = q->pos.y - p->pos.y, d = sqrtf(dx * dx + dy * dy);
					if(d <= r.arrivee + 1e-3f) continue;   // arrivé (au millimètre près)
					float pas = ScriptPedVitesse(ALLURE_COURSE) * 0.033f, k = d - r.arrivee < pas ? (d - r.arrivee) / d : pas / d;
					p->pos.x += dx * k; p->pos.y += dy * k;
					p->cap = atan2f(dy, dx) * 180.0f / 3.14159265f;
					if(d - r.arrivee < pas) printf("%6.2f s  le piéton %d arrive à %.2f m du piéton %d (SeekEntityPosition)\n", maintenant / 1000.0, c.first, r.arrivee, cibleDe(r.genre));
				}
				for(const CActionController::Regard &r : c.second.regardsActifs){
					const CScriptPed *q = pietons.Acceder(cibleDe(r.genre));
					if(q == nil || q == p) continue;
					p->cap = atan2f(q->pos.y - p->pos.y, q->pos.x - p->pos.x) * 180.0f / 3.14159265f + r.decalage;
				}
			}
			AppliquerObjectifs(c.first, c.second);
			// SetHealth (FUN_00627500) : poser (FUN_00478190, pas sous la santé minimale), rendre (FUN_00474b70, jusqu'au
			// maximum), ôter (FUN_004782a0, sans les invulnérabilités, ni sous 0 ni sous la santé minimale).
			for(const CActionController::Sante &e : c.second.santesLancees){
				CScriptPed *p = pietons.Acceder(c.first);
				if(p == nil) continue;
				float avant = p->sante, v = p->sante;
				if(e.mode == 0) v = e.valeur < p->santeMin ? p->santeMin : e.valeur;
				else if(e.mode == 1){ if(p->sante <= p->santeMax){ v = p->sante + e.valeur; if(v > p->santeMax) v = p->santeMax; } }
				else { v = p->sante - e.valeur; if(v < 0) v = 0; if(v < p->santeMin) v = p->santeMin; }
				pietons.Sante(c.first, v);
				printf("%6.2f s  SetHealth sur %d (%s %.0f) : %.0f → %.0f points de vie\n", maintenant / 1000.0, c.first,
				       e.mode == 0 ? "poser" : e.mode == 1 ? "rendre" : "ôter", e.valeur, avant, v);
			}
			// SetTargetAsGrappleTarget (FUN_0062a580) : la cible (celle de PedLockTarget) et le piéton deviennent partenaires
			// et descendent chacun dans leur nœud ; l'exécution sans jouer (+0x19 / +0x1a) n'est pas suivie.
			for(const CActionController::Prise &e : c.second.prises){
				const CScriptPed *p = pietons.Acceder(c.first);
				if(p == nil || p->verrou < 0) continue;
				int cible = p->verrou;
				// PedSetGrappleTarget (0x474360) des deux côtés : chacun est le partenaire de prise (+0x155c) de l'autre.
				if(CScriptPed *mp = pietons.Acceder(c.first)) mp->cibleDePrise = cible;
				if(CScriptPed *mc = pietons.Acceder(cible)) mc->cibleDePrise = c.first;
				bool a = e.soi && !e.executerSoi && c.second.Entrer(arbres, e.soi, CJugeOutil(c.first, maintenant / 1000.0f), maintenant / 1000.0f);
				bool b = e.cible && !e.executerCible && controleurs[cible].Entrer(arbres, e.cible, CJugeOutil(cible, maintenant / 1000.0f), maintenant / 1000.0f);
				partenaires[cible] = {c.first, maintenant + 2000};
				printf("%6.2f s  prise de %d sur %d : %s %s, %s %s\n", maintenant / 1000.0, c.first, cible,
				       a ? "il joue" : "rien pour lui", a ? CheminVu(c.second, c.second.joue).c_str() : "",
				       b ? "la cible joue" : "rien pour la cible", b ? CheminVu(controleurs[cible], controleurs[cible].joue).c_str() : (e.cible ? arbres.CheminDe(e.cible).c_str() : ""));
			}
			// PlayOnTarget : la cible du piéton (FUN_004d5980 ; dans l'outil, celle de PedLockTarget) joue le nœud. Sans elle, la piste
			// attend (FUN_006615f0 rend vrai) ; l'outil n'a pas le composant de visée des piétons de l'IA (le +0xa0 que posent leurs
			// pistes SetTarget, TargetLock, GiveTargetToFocus et leurs conditions par FUN_004d7890) : à défaut de verrou, il prend le
			// partenaire de prise (+0x155c), supposé.
			std::vector<const CActionNode *> pourCible;
			pourCible.swap(c.second.pourCible);
			for(const CActionNode *n : pourCible){
				const CScriptPed *p = pietons.Acceder(c.first);
				int qui = p == nil ? -1 : p->verrou >= 0 ? p->verrou : p->cibleDePrise;
				if(pietons.Acceder(qui) == nil){ c.second.SansCible(n); continue; }
				CActionController &cible = controleurs[qui];
				if(!cible.Entrer(arbres, n, CJugeOutil(qui, maintenant / 1000.0f), maintenant / 1000.0f)) cible.Jouer(arbres, n, maintenant / 1000.0f);
				partenaires[qui] = {c.first, maintenant + 2000};
				printf("%6.2f s  la cible de %d (%d%s) joue %s\n", maintenant / 1000.0, c.first, qui, p->verrou >= 0 ? "" : ", son partenaire de prise",
				       CheminVu(cible, cible.joue).c_str());
			}
		}
		AvancerHistoriqueBoutons(L);
		intentionsPietons.clear();   // FUN_005f3720 : l'intention ne vaut que pour l'image
		VieillirBoutons();
		for(auto it = demandes.begin(); it != demandes.end(); ) it = maintenant > it->second ? demandes.erase(it) : ++it;
		for(auto &e : poussees) if(!e.second.empty() && maintenant >= e.first){
			int h = donnees.Declencheur(e.second.c_str());
			if(h < 0) printf("%6.2f s  le joueur pousse la porte %s : aucun déclencheur de ce nom n'est chargé\n", maintenant / 1000.0, e.second.c_str());
			else { zones.PousserPorte(h); printf("%6.2f s  le joueur pousse la porte %s (+0x394)\n", maintenant / 1000.0, e.second.c_str()); }
			e.second.clear();
		}
		for(auto &e : portes) if(e.second >= 0 && maintenant >= e.first){
			// Une porte : la demande de FUN_00416e30 (état 0) pour le point d'arrivée, sans l'octet +0x8dc.
			const CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
			printf("%6.2f s  le joueur passe une porte vers la zone %d%s\n", maintenant / 1000.0, e.second, zones.Demander(j->pos, j->cap, e.second, nil, false) ? "" : " : refusé, un changement est en cours");
			e.second = -1;
		}
		// L'échéance d'une ligne retenue (le rappel 0x619550) : la tenue 1 la joue, la tenue 2 l'annule.
		for(auto it = parolesTenues.begin(); it != parolesTenues.end(); ){
			if(maintenant < it->echeance){ ++it; continue; }
			printf("%6.2f s  parole : %s ligne %d retenue par le piéton %d, échue : %s\n", maintenant / 1000.0, kSpeechEvents[it->evenement],
			       it->ligne, it->ped, it->tenue == 2 ? "annulée" : "jouée");
			if(it->tenue != 2){
				parole.enCours = true; parole.ped = it->ped; parole.evenement = it->evenement; parole.ligne = it->ligne < 0 ? 0 : it->ligne;
				parole.id++; parole.fin = maintenant + DureeParole(parole.ped, parole.evenement, parole.ligne);
			}
			it = parolesTenues.erase(it);
		}
		if(prechargement.attente){
			bool annulee = false;
			if(prechargement.Rappel(true, maintenant, &annulee)){
				parole.enCours = true; parole.ped = prechargement.ped; parole.evenement = prechargement.evenement;
				parole.ligne = prechargement.ligne; parole.fin = maintenant + DureeParole(parole.ped, parole.evenement, parole.ligne);
				if(journalRestant > 0){ journalRestant--; printf("%6.2f s  parole : le piéton %d dit %s ligne %d (préchargée)\n", maintenant / 1000.0, parole.ped, kSpeechEvents[parole.evenement], parole.ligne); }
			}
			else if(annulee) printf("%6.2f s  parole : %s ligne %d préchargée et jamais demandée, annulée après 30 s\n", maintenant / 1000.0, kSpeechEvents[prechargement.evenement], prechargement.ligne);
		}
		if(parole.enCours && maintenant >= parole.fin){ parole.enCours = false; if(journalRestant > 0){ journalRestant--; printf("%6.2f s  parole : le piéton %d a fini %s ligne %d\n", maintenant / 1000.0, parole.ped, kSpeechEvents[parole.evenement], parole.ligne); } }
		if(fondu.enCours && maintenant >= fondu.fin){ fondu.enCours = false; fondu.noir = fondu.versNoir; if(journalRestant > 0){ journalRestant--; printf("%6.2f s  fondu : l'écran est %s\n", maintenant / 1000.0, fondu.versNoir ? "noir" : "rendu"); } }
		MettreAJour(L);
		AvancerPietons();
		AvancerObjets();
		PoserLesCollectibles();
		AvancerRamassages();
		ecranDeFin.Avancer(33.0f / 20.0f);   // FUN_007073c0 : le pas du moteur (ms_fTimeStep)
		PoursuivrePieton();   // --suivre : la manette du joueur
		AvancerCinematique();
		AvancerZones(L);
		AvancerGestionnaire(L);
	}

	size_t crees = 0;
	int vivants = 0;
	for(const Script &sc : scripts)
		for(size_t i = 1; i < sc.threads.size(); i++){ crees++; vivants += sc.threads[i].etat < TUE; }
	printf("\n%d images (%.1f s) : %zu threads créés, %d encore vivants\n", images, secondes, crees, vivants);
	for(const Script &sc : scripts)
		for(size_t i = 1; i < sc.threads.size(); i++)
			printf("  %-14s %-28s %s\n", sc.fichier.c_str(), sc.threads[i].fonction.c_str(),
			       sc.threads[i].etat < TUE ? "vivant" : sc.threads[i].etat == TUE ? "tué" : "fini");
	std::vector<std::pair<int, std::string>> tri;
	for(auto &a : appels) tri.push_back({-a.second, a.first});
	std::sort(tri.begin(), tri.end());
	printf("appels au moteur les plus fréquents :");
	for(size_t i = 0; i < tri.size() && i < 25; i++) printf(" %s (%d)", tri[i].second.c_str(), -tri[i].first);
	printf("\n");
	if(!inconnues.empty()){
		std::vector<std::pair<int, std::string>> tri2;
		for(auto &a : inconnues) tri2.push_back({-a.second, a.first});
		std::sort(tri2.begin(), tri2.end());
	if(!nonLues.empty()){
		std::vector<std::pair<int, uint32>> v;
		for(auto &e : nonLues) v.push_back({-e.second, e.first});
		std::sort(v.begin(), v.end());
		printf("conditions pas encore lues (jugées -1) :");
		for(size_t i = 0; i < v.size() && i < 40; i++) printf(" #%08x (%d)", v[i].second, -v[i].first);
		printf("\n");
	}
		printf("globales de script lues avant d'être posées (nil) :");
		for(size_t i = 0; i < tri2.size() && i < 12; i++) printf(" %s (%d)", tri2[i].second.c_str(), -tri2[i].first);
		printf("\n");
	}
	lua_close(L);
	return 0;
}
