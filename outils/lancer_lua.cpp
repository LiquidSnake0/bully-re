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
//   BULLY_DATA=<racine du jeu> [LANCER_JOURNAL=lignes] [LANCER_CONTROLEURS=1] build/outils/lancer_lua [secondes] [script.lua [fonction]] [--mission NOM | --nouvelle-partie] [--sans-cinematiques] [--entrer DECLENCHEUR@s]... [--aller LISTE[,n]@s | --aller x,y,z@s]... [--zone N@s]... [--porte DT_NOM@s]... [--jouer /Global/CHEMIN[:Act/FICHIER.act]@s[,s2…]]... [--demander N@s]... [--sequence @s]...
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
#include "../src/anim/Hxd.h"
#include "../src/anim/AnimGroups.h"
#include "../src/core/ConditionListe.h"
#include "../src/audio/SpeechEvents.h"
#include "../src/audio/SpeechBank.h"
#include "../src/cutscene/CutsceneFiles.h"
#include "../src/peds/ScriptPedPool.h"
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
// Les piétons des scripts (src/peds/ScriptPedPool.h) : le joueur en 0, PedCreatePoint & co.
static CScriptPedPool pietons;
// Les modèles par nom (src/core/ModelEnum.h) : MODELENUM._Taxicab → 286, ou -1.
static CModelEnum modeles;
// Le gestionnaire de zones (src/core/AreaManager.h, docs/zones.md) : la zone visible (DAT_00bd1008, ce que
// AreaGetVisible répond), les scripts de zone (AreaRegisterAreaScript) et le changement de zone du joueur,
// demandé par PlayerSetPos… et joué image par image comme FUN_00416e30 : fondu au noir, scripts de zone
// retirés puis celui de la nouvelle zone chargé avec son main, zone visible et joueur posés à l'arrivée.
static CAreaManager zones;
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
static std::map<int, CActionController> controleursIA;   // le contrôleur de l'IA (+0x290 du composant +0x2e0), PedSetAITree
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
static lua_State *etatLua = nil;        // pour LuaCond : la fonction du script de mission appelée par le contrôleur
// --demander N@s : une ActionRequest du joueur (la manette) en attente jusqu'à cet instant (ms).
static std::map<uint32, uint32> demandes;

// Les conditions des nœuds d'action pour un piéton (FUN_005fb9b0 les juge une à une ; la banque fait NOT, OR, False
// et ConditionGroup). Champs relevés dans les enregistrements (mots[i] = champ +4 * (i + 1) de l'objet) :
//   WeaponEquiped        +4 : le nom de l'arme (« unarmed », « any », « bat »…), cité par la première table ;
//   ActionRequest        (ConditionActionRequestButton, FUN_005fd650) +4 : le numéro de l'action, +8 : le mode
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
struct CoupRecu { uint32 drapeau = 0; float jusqua = -1.0f; float degats = 0.0f; int attaquant = -1; float instant = -1.0f, precedent = 0.0f; };
static std::map<int, CoupRecu> coups;
// Un piéton qui disparaît emporte ses contrôleurs (le composant +0x2e0, libéré avec le CPed) : une entrée du pool reprise
// plus tard (PedCreatePoint) part de contrôleurs neufs.
static void
OublierControleurs(int h)
{
	controleurs.erase(h);
	controleursIA.erase(h);
	coups.erase(h);
}

static float facteurCoupsJoueur = 1.0f;   // PlayerButes +0xac (172) pour l'état physique 0 (FUN_0047b810), lu au démarrage
static std::map<int, std::pair<int, uint32>> partenaires;   // le piéton qui nous a fait jouer un nœud (PlayOnTarget), jusqu'à quand (ms)
// L'intention de chaque piéton pour l'image (+0x254 du composant +0x2e0) : le nom qu'une piste ActionNodeRequest de son
// arbre de l'IA y pose (FUN_005f35e0), lu par la condition ActionNodeRequest de son arbre d'actions, effacé en fin d'image
// avec l'état des boutons (FUN_005f3720).
static std::map<int, uint32> intentionsPietons;
// AIButes (Globals.cat, DAT_020c889c, FUN_0072ae00) : +0x18 à +0x20 la capacité des trois zones de combat ; +0x28 à
// +0x38 les rayons des zones (0,9 / 1,5 / 3 / 6 / 8 m) ; +0x54 la portée et +0x58 le cosinus du cône d'attaque.
static CActionTrack butesIA;
static float
ButeIA(uint16 position, float defaut)
{
	return butesIA.Champ(position) ? CActionBank::ButesReel(butesIA, position) : defaut;
}
static uint32 graineIA = 7;   // le rand() du CRT des conditions Random et WeightedRandom (l'outil a sa propre graine)
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
struct CJugeOutil : CJugeAction {
	int ped; float t;
	// Le tirage du contexte (+0x24) que WeightedRandom compare puis renouvelle (FUN_00600b00) : tiré à chaque nouveau
	// contexte, donc à chaque descente (supposé : le contexte le tire à sa remise à zéro). Les seuils de branches
	// sœurs étant croissants (0,2 puis 1), la première qui passe l'emporte avec la probabilité de son écart.
	mutable float tirage;
	CJugeOutil(int p, float t_) : ped(p), t(t_) { tirage = (float)RandCrt(graineIA) / 32767.0f; }
	int JoueNom(uint32 nom) const override { return arbres.JoueHachage(NoeudJoue(ped), nom & 0x7fffffff); }
	bool ACible(void) const override { return CibleDe(ped) >= 0; }
	// Le temps du nœud joué par le contrôleur qui juge (le +0x10 du contexte), sinon celui du contrôleur d'actions.
	float TempsDuNoeud(bool *ok) const {
		if(controleur){ *ok = true; return controleur->Temps(t); }
		auto it = controleurs.find(ped);
		*ok = it != controleurs.end();
		return *ok ? it->second.Temps(t) : 0.0f;
	}
	int Juger(const CActionTreeFile &f, int32 d, uint32 type, const uint32 mots[6]) const override {
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
		if(type == kArme) return f.ChaineCitee((uint32)d + 8) == "unarmed";
		const CScriptPed *moi = pietons.Acceder(ped);
		float seuil; memcpy(&seuil, &mots[0], 4);
		// IsPlayer (FUN_005ffe40) : le type du piéton (+0x1310) vaut 13 ; dans l'outil, l'entrée 0 du pool.
		if(type == kJoueur) return ped == SCRIPT_PED_JOUEUR;
		// IsMissionSpecific (FUN_005ffb70) : l'octet +0x1570 vaut 2, ce que le constructeur (FUN_0047ffe0) pose pour un
		// piéton créé par un script (FUN_005cf6b0 → FUN_0046dea0(modèle, 2)) ; PedMakeAmbient le change (FUN_004770a0).
		// Dans l'outil : un piéton noté à un objet script.
		if(type == kMission) return moi && ped != SCRIPT_PED_JOUEUR && moi->script >= 0;
		// PedFlags (FUN_005fd700) : le drapeau +4 du piéton (+0x1314 + 4n) non nul. Dans la charge d'une prise, « Not PedFlags »
		// (le drapeau 70, posé par la fin de la charge) mène à un nœud dont l'Execute vise GrappleMoves/ExecuteNodes, qui
		// n'existe pas (ExecuteNodes est sous GrappleStrikes) : le nœud finit tout de suite et passe à ../Give.
		if(type == kDrapeaux) return moi && mots[0] < SCRIPT_PED_DRAPEAUX ? moi->drapeaux[mots[0]] != 0 : -1;
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
		// stat (0x5ffff0, hors de l'index) : si l'octet +9, faux quand le piéton joue le nœud de DAT_01b65288 (non
		// suivi : l'octet est nul dans les arbres relevés) ; la colonne +0xc (GameGetPedStat) au moins égale à +4 quand
		// l'octet +8 est nul (sinon un tirage, non suivi).
		if(type == kStat){
			if((mots[1] & 0xff00) != 0 || (mots[1] & 0xff) != 0 || moi == nil || mots[2] >= 64) return -1;
			return (float)moi->stats[mots[2]] >= seuil;
		}
		// HitTime (FUN_005fc400) : le temps depuis le coup (+0x434), ou l'intervalle d'avant (+0x438) si l'octet +0xc,
		// comparé à +4 par +8. Sans coup reçu, +0x434 part de 0 à la création et court : l'outil prend le temps écoulé.
		// DamagePending (FUN_005fbf00) : les dégâts du relevé (+0x440) comparés à +4 par +8.
		if(type == kTempsCoup || type == kEnAttente){
			auto it = coups.find(ped);
			float v = type == kTempsCoup ? t : 0.0f;   // sans coup : +0x434 a couru depuis la création (à 0)
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
			if(ped != SCRIPT_PED_JOUEUR) return 0;
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
			std::string fonction = f.ChaineCitee((uint32)d + 0x14);
			if(etatLua == nil || fonction.empty()) return -1;
			lua_getglobal(etatLua, fonction.c_str());
			if(!lua_isfunction(etatLua, -1)){ lua_pop(etatLua, 1); return 0; }
			if(lua_pcall(etatLua, 0, 1, 0) != 0){ lua_pop(etatLua, 1); return -1; }
			// FUN_006a74d0 → FUN_005d80a0 : le résultat est lu par lua_tonumber (0x73b0d0 : un nombre, ou une chaîne
			// convertible, sinon 0), tronqué en entier (0x85c6f0), et la condition est vraie s'il est positif. Les
			// fonctions des scripts rendent 1.0 ou 0.0 (F_CanBullyAttack, F_PlayerGroundKicks…) : 0.0 est faux, et un
			// booléen, faute d'être un nombre, l'est aussi.
			int r = (int)lua_tonumber(etatLua, -1) > 0;
			lua_pop(etatLua, 1);
			return r;
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
		// (FUN_004992a0, parmi les objectifs : FUN_004988b0, FUN_00496b60 ; non lu en détail) ; vraie si le piéton a une
		// cible (PedGetTargetPed). Supposé : sans cible, il prend celle de son objectif d'attaque (+0x540).
		if(type == kAcquerir){
			if(moi == nil) return 0;
			if(CibleDe(ped) < 0 && pietons.Acceder(moi->cibleObjectif)) pietons.Verrouiller(ped, moi->cibleObjectif, 0);
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
		// Random (FUN_00600a50) : une valeur tirée (l'octet +0xd la tire dans le contexte, sinon la valeur +0x1748 du
		// piéton) comparée à +4 par +8. Supposé : un tirage neuf ; la garde de l'octet +0xc (un nœud joué) n'est pas suivie.
		if(type == kHasard) return ComparerCondition((float)RandCrt(graineIA) / 32767.0f, seuil, mots[1]);
		// WeightedRandom (FUN_00600b00) : faux si le seuil +8 est sous le tirage du contexte (+0x24) ; sinon le tirage est
		// renouvelé et la condition vraie. Les gardes des octets +0xc et +0xd ne sont pas suivies.
		if(type == kPondere){
			float s8; memcpy(&s8, &mots[1], 4);
			if(s8 < tirage) return 0;
			tirage = (float)RandCrt(graineIA) / 32767.0f;
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
			for(const CActionNode &e : it->second.arbre->enfants)
				if((e.hachage & 0x7fffffff) == (mots[0] & 0x7fffffff)){
					for(const CActionNode &g : e.enfants) if((g.hachage & 0x7fffffff) == (mots[1] & 0x7fffffff)) return 1;
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

// Le nom d'une mission pour le journal : en clair quand le fichier le donne, sinon son titre.
static const char *
NomMission(const CMissionData &m){ return m.nom.empty() ? m.titre.c_str() : m.nom.c_str(); }

static int prochainId = 1;
static bool tousLesControleurs;                        // LANCER_CONTROLEURS : les passages de tous les contrôleurs
static CLuaApi api;                                   // la frontière moteur / scripts (LANCER_API, sinon docs/api-lua-tables.tsv)
static std::map<std::string, int> inconnues;          // globales lues avant d'être posées : nil, comme dans le jeu
static std::map<std::string, int> appels;
static int journalRestant = 400;
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
				int n = pietons.SupprimerDuScript(mission.script);
				for(int h : partis) OublierControleurs(h);
				if(n) printf("        %d piéton(s) laissé(s) par la mission, libéré(s)\n", n);
			}
			for(Thread &t : scripts[mission.script].threads)
				if(t.etat < TUE){ t.etat = TUE; luaL_unref(L, LUA_REGISTRYINDEX, t.ref); t.ref = LUA_NOREF; }
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
	if(!donnees.Charger(fichier, emplacement)) printf("        DATLoad : %s introuvable dans DAT\\Trigger.img\n", fichier);
	return 0;
}

static int
DATUnload(lua_State *L)
{
	Journal("DATUnload", L, 1);
	donnees.Decharger((int)luaL_checknumber(L, 1));
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
	printf("%6.2f s  le joueur entre dans %s (%d) en (%.1f, %.1f, %.1f) : %s\n", maintenant / 1000.0, nom, h, j->pos.x, j->pos.y, j->pos.z,
	       donnees.DansDeclencheur(h, j->pos) ? "dedans" : "la position du déclencheur est hors de sa propre zone");
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

// FUN_005cf6b0 : la création commune, notée à l'objet script courant ; le piéton reçoit l'entrée de pedstats.dat
// de son modèle (FUN_00478e30).
static int
CreerPed(lua_State *L, const char *quoi, int32 modele, const CVector &pos, float cap)
{
	int h = pietons.Creer(modele, pos, cap, scriptCourant);
	if(h < 0) printf("        %s : le pool des piétons est plein (%d)\n", quoi, SCRIPT_PED_POOL_SIZE);
	else{
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
PedIsDead(lua_State *L)
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

static int
PedSetHealth(lua_State *L)
{
	Journal("PedSetHealth", L, 1);
	if(lua_isnumber(L, 1)) pietons.Sante((int)lua_tonumber(L, 1), (float)luaL_checknumber(L, 2));
	return 0;
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

// --- Les gestes des piétons : marche, course, arrêt, bagarre (src/peds/ScriptPedPool.h). ---

// PedMoveToPoint(ped, allure, liste, n) (0x5c7df0) : vers le n-ième point de la liste.
static int
PedMoveToPoint(lua_State *L)
{
	Journal("PedMoveToPoint", L, 1);
	const CTriggerPointPoint *pt = PointArg(L, "PedMoveToPoint", 3, 4);
	if(pt && lua_isnumber(L, 1)) pietons.Aller((int)lua_tonumber(L, 1), pt->pos, (int)luaL_optnumber(L, 2, ALLURE_MARCHE));
	return 0;
}

// PedMoveToXYZ(ped, allure, x, y, z) (0x5c7c90).
static int
PedMoveToXYZ(lua_State *L)
{
	Journal("PedMoveToXYZ", L, 1);
	CVector cible((float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5));
	if(lua_isnumber(L, 1)) pietons.Aller((int)lua_tonumber(L, 1), cible, (int)luaL_optnumber(L, 2, ALLURE_MARCHE));
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
		int cible = f.autreCible ? -1 : CibleDe(ped);
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

// La mise à jour d'une piste CombatLoco (FUN_0062ea40) : la cible (FUN_00625270 : le joueur si l'octet +0x18, sinon
// +0x538) ; une image sur le tirage de période 8, ou à la première, le piéton prend une zone de combat autour d'elle
// (FUN_006252d0 : PedJoinCombatZone, la zone 0, sinon 1, sinon 2) ; les rayons de sa zone (FUN_006253e0 : +0x66c, AIButes
// +0x28 / +0x2c pour la zone 0, +0x2c / +0x30, +0x30 / +0x34, sinon +0x34 / +0x38 ; un demi-mètre de plus si son émotion
// envers la cible est 4 à 6, non suivie) ; puis l'intention de mouvement (FUN_0062e120, FUN_0061b430) : le piéton va
// vers sa cible tant qu'il est plus loin que le grand rayon. Supposé : à l'allure de course de l'outil, tout droit (le
// moteur passe par la recherche de chemin, FUN_005e9df0) ; le petit rayon (reculer) n'est pas suivi.
static void
CombatLoco(int ped, bool joueur, bool premiere)
{
	CScriptPed *p = pietons.Acceder(ped);
	if(p == nil || p->mort) return;
	if(joueur) pietons.Verrouiller(ped, SCRIPT_PED_JOUEUR, 0);
	int c = CibleDe(ped);
	const CScriptPed *q = pietons.Acceder(c);
	if(q == nil) return;
	if(premiere || TirageOpportunite(8, (uint16)ped, imageCourante))
		for(int z = 0; z < 3; z++) if(pietons.JoindreZoneCombat(ped, c, z, false)) break;
	static const uint16 kGrands[5] = {44, 48, 52, 56, 56};
	int z = p->zoneCombat < 0 || p->zoneCombat > 3 ? 3 : p->zoneCombat;
	float rayon = ButeIA(kGrands[z], 1.5f);
	float dx = q->pos.x - p->pos.x, dy = q->pos.y - p->pos.y, d = sqrtf(dx * dx + dy * dy);
	if(d <= rayon) return;
	float pas = ScriptPedVitesse(ALLURE_COURSE) * 0.033f, k = d - rayon < pas ? (d - rayon) / d : pas / d;
	p->pos.x += dx * k; p->pos.y += dy * k;
	p->cap = atan2f(dy, dx) * 180.0f / 3.14159265f;
}

// Une image du contrôleur de l'IA d'un piéton (FUN_00484490), puis ce que font ses pistes et celles des contrôleurs de
// ses pistes spawn : l'intention (ActionNodeRequest), ManageTargets, CombatLoco, FaceTarget, SetPedFlags.
static void
AvancerIA(int ped, CActionController &c, uint32 images)
{
	CScriptPed *p = pietons.Acceder(ped);
	if(p == nil || p->mort || c.arbre == nil) return;
	float t = maintenant / 1000.0f;
	c.image = images; c.graine = (uint16)ped;
	const CActionNode *avant = c.joue;
	if(c.joue == nil) c.Poser(arbres, c.arbre, CJugeOutil(ped, t), t);   // FUN_00483ec0
	else c.Avancer(arbres, nil, CJugeOutil(ped, t), t);
	if(c.joue != avant && journalIA > 0){ journalIA--; printf("%6.2f s  IA de %d : %s\n", t, ped, arbres.CheminDe(c.joue).c_str()); }
	std::vector<CActionController *> tous;
	c.Recueillir(tous);
	static std::map<const CActionController *, const CActionNode *> vus;   // LANCER_IA : les passages des contrôleurs des spawn
	if(getenv("LANCER_IA"))
		for(size_t i = 1; i < tous.size(); i++)
			if(vus[tous[i]] != tous[i]->joue){ vus[tous[i]] = tous[i]->joue; printf("%6.2f s  IA de %d, spawn : %s\n", t, ped, arbres.CheminDe(tous[i]->joue).c_str()); }
	for(CActionController *k : tous){
		for(uint32 n : k->intentions) intentionsPietons[ped] = n;
		for(const CActionController::Gestion &g : k->gestionsActives) GererCibles(ped, g.joueur);
		for(const CActionController::Gestion &g : k->combatsActifs) CombatLoco(ped, g.joueur, k->Temps(t) <= g.entree + 0.034f);
		for(const CActionController::Regard &r : k->regardsActifs){   // FUN_00620dc0
			const CScriptPed *q = pietons.Acceder(r.genre == 0 ? CibleDe(ped) : r.genre == 1 ? p->cibleDePrise : -1);
			if(q && q != p) p->cap = atan2f(q->pos.y - p->pos.y, q->pos.x - p->pos.x) * 180.0f / 3.14159265f + r.decalage;
		}
		for(const CActionController::Drapeau &d : k->drapeauxPoses) if(d.numero < SCRIPT_PED_DRAPEAUX) p->drapeaux[d.numero] = d.valeur;
		k->drapeauxPoses.clear();
		AppliquerObjectifs(ped, *k);
	}
}

// Une image des piétons : les gestes avancent, et ce qui aboutit se journalise.
static void
AvancerPietons(void)
{
	pietons.Avancer(0.033f);
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

// L'historique des boutons (HUDButtonHistory) : la leçon de combo de 1_02 pose une séquence (ButtonHistoryAdd…),
// puis deux fonctions du script, appelées quand le joueur réussit ou rate la séquence. L'outil garde ces deux
// fonctions ; --sequence@s fait réussir le joueur (la fonction « réussie » est appelée), rien ne la rate de soi-même.
static int rappelReussi = LUA_NOREF, rappelRate = LUA_NOREF;

static void
Oublier(lua_State *L, int &ref)
{
	if(ref != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, ref);
	ref = LUA_NOREF;
}

static int
ButtonHistorySetCallbackPassed(lua_State *L)
{
	Journal("ButtonHistorySetCallbackPassed", L, 1);
	Oublier(L, rappelReussi);
	if(lua_isfunction(L, 1)){ lua_pushvalue(L, 1); rappelReussi = luaL_ref(L, LUA_REGISTRYINDEX); }
	return 0;
}

static int
ButtonHistorySetCallbackFailed(lua_State *L)
{
	Journal("ButtonHistorySetCallbackFailed", L, 1);
	Oublier(L, rappelRate);
	if(lua_isfunction(L, 1)){ lua_pushvalue(L, 1); rappelRate = luaL_ref(L, LUA_REGISTRYINDEX); }
	return 0;
}

static int
ButtonHistoryClearSequence(lua_State *L)
{
	Journal("ButtonHistoryClearSequence", L, 1);
	Oublier(L, rappelReussi);
	Oublier(L, rappelRate);
	return 0;
}

// --sequence@s : le joueur tape la séquence demandée ; la fonction « réussie » du script est appelée.
static void
ReussirSequence(lua_State *L)
{
	if(rappelReussi == LUA_NOREF){ printf("%6.2f s  le joueur tape une séquence : aucune n'est posée\n", maintenant / 1000.0); return; }
	int ref = rappelReussi;
	rappelReussi = LUA_NOREF;
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	luaL_unref(L, LUA_REGISTRYINDEX, ref);
	printf("%6.2f s  le joueur réussit la séquence de boutons\n", maintenant / 1000.0);
	if(lua_pcall(L, 0, 0, 0) != 0){ printf("        rappel de la séquence : %s\n", lua_tostring(L, -1)); lua_pop(L, 1); }
}

static int
PlayerSetControl(lua_State *L)
{
	Journal("PlayerSetControl", L, 1);
	int c = (int)luaL_optnumber(L, 1, 1);
	if(c == 0 || c == 1) controleJoueur = c;
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
		CScriptPed *j = pietons.Acceder(SCRIPT_PED_JOUEUR);
		j->pos = zones.demande.pos; j->cap = zones.demande.cap;
		printf("%6.2f s  zones : le joueur est posé en (%.1f, %.1f, %.1f), cap %.0f, zone %d\n", maintenant / 1000.0, j->pos.x, j->pos.y, j->pos.z, j->cap, zones.zoneVisible);
	}
	if(e.fonduRetour) LancerFondu(fonduRetourMs, false);
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
		else if(strcmp(argv[i], "--aller") == 0 && i + 1 < argc){
			std::string e = argv[++i]; size_t at = e.find('@');
			allers.push_back({(uint32)(at == std::string::npos ? 2000 : atof(e.c_str() + at + 1) * 1000), e.substr(0, at)});
		}
		else pos.push_back(argv[i]);
	}
	if(getenv("LANCER_JOURNAL")) journalRestant = atoi(getenv("LANCER_JOURNAL"));   // lignes de journal (400)
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
	lua_register(L, "PedCreateXYZ", PedCreateXYZ);
	lua_register(L, "PedDelete", PedDelete);
	lua_register(L, "PedIsValid", PedIsValid);
	lua_register(L, "PedIsPlayer", PedIsPlayer);
	lua_register(L, "PlayerGetPedIndex", PlayerGetPedIndex);
	lua_register(L, "PedGetPosXYZ", PedGetPosXYZ);
	lua_register(L, "PedSetPosXYZ", PedSetPosXYZ);
	lua_register(L, "PedGetHeading", PedGetHeading);
	lua_register(L, "PedFaceXYZ", PedFaceXYZ);
	lua_register(L, "PedIsDead", PedIsDead);
	lua_register(L, "PedGetHealth", PedGetHealth);
	lua_register(L, "PedSetHealth", PedSetHealth);
	lua_register(L, "PlayerGetHealth", PlayerGetHealth);
	lua_register(L, "PedMoveToPoint", PedMoveToPoint);
	lua_register(L, "PedMoveToXYZ", PedMoveToXYZ);
	lua_register(L, "PedStop", PedStop);
	lua_register(L, "PedAttack", PedAttack);
	lua_register(L, "PedSetAITree", PedSetAITree);
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
	ChargerPedStats();
	ChargerMissions();
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
		// Une porte DT_ poussée : FUN_00415160 pose l'objet de porte en cours (+0x394) ; le moteur le fait depuis
		// l'animation de la porte (FUN_00610ed0, non jouée), l'outil le fait au nom du déclencheur.
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
				if(!c.Entrer(arbres, n, CJugeOutil(SCRIPT_PED_JOUEUR, maintenant / 1000.0f), maintenant / 1000.0f)){
					// Une banque dont la descente jugée n'aboutit pas (conditions de visée, de bouton) : la manette de l'outil
					// prend le premier nœud jouable sans condition parmi ses enfants (HeavyAttacks → son coup de pied).
					const CActionNode *d = nil;
					if(n->genre == 'b') for(const CActionNode &e : n->enfants) if((d = arbres.Choisir(&e)) != nil) break;
					c.Jouer(arbres, d ? d : n, maintenant / 1000.0f);
				}
				printf("%6.2f s  le joueur joue %s%s\n", maintenant / 1000.0, chemin.c_str(), c.joue != n ? (" → " + arbres.CheminDe(c.joue)).c_str() : "");
			}
			e.second.clear();
		}
		for(uint32 &q : sequences) if(q != 0xffffffffu && maintenant >= q){ ReussirSequence(L); q = 0xffffffffu; }
		// La manette : une ActionRequest en attente un quart de seconde, que le contrôleur du joueur juge.
		bool demande = false;
		for(auto &e : attentes) if(e.first != 0xffffffffu && maintenant >= e.first){
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
			if(c.second.Avancer(arbres, nil, CJugeOutil(c.first, maintenant / 1000.0f), maintenant / 1000.0f, d) && c.second.joue != avant){
				if(c.first == SCRIPT_PED_JOUEUR)
					printf("%6.2f s  contrôleur du joueur : %s\n", maintenant / 1000.0, arbres.CheminDe(c.second.joue).c_str());
				else if(tousLesControleurs)
					printf("%6.2f s  contrôleur de %d : %s\n", maintenant / 1000.0, c.first, arbres.CheminDe(c.second.joue).c_str());
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
				// FUN_0047b810 : les dégâts de la piste (+0x34) ; un coup du joueur (type 13) sans arme (-1) est multiplié par
				// PlayerButes +0xac / +0xb0 / +0xb4 selon l'état physique du joueur (DAT_00c1af4c, PlayerGetPhysicalState ;
				// l'outil : 0) ; puis, sans arme, arrondi au-dessus après le facteur +0x1cd4 de l'attaquant (FUN_0085dbc0)
				// et au-dessous après le facteur +0x1cfc de la cible (FUN_0085d540), tous deux à 1 depuis le constructeur
				// (FUN_0047ffe0 ; FUN_00426560, qui les change, n'est pas suivi).
				float degats = k.degats * (c.first == SCRIPT_PED_JOUEUR ? facteurCoupsJoueur : 1.0f);
				degats = floorf(ceilf(degats * 1.0f) * 1.0f);
				CoupRecu &r = coups[cible];
				float t = maintenant / 1000.0f;
				r.precedent = r.instant >= 0.0f ? t - r.instant : 0.0f;   // FUN_00472b70 : +0x434 passe en +0x438
				r.instant = t;
				r.drapeau = k.drapeau & 0x7fffffff; r.jusqua = t + 1.0f; r.degats = degats; r.attaquant = c.first;
				CActionController &v = controleurs[cible];
				// Un piéton à terre (son nœud joué s'appelle On_Ground, ou un parent) : dans le moteur, la réaction passe par
				// HitTree/BASE/HitTreeBase/…/On_Ground (« si MePlaying… », joué par un Execute des nœuds de coup), dont
				// l'Opportunity mène à GroundAndWallHits/On_Ground. L'outil ne joue pas les Execute en contrôleur
				// secondaire : il y entre directement (supposé). Sinon, la racine de HitTree.
				bool aTerre = arbres.JoueHachage(NoeudJoue(cible), ActionHash("On_Ground"));
				const CActionNode *arbreCoups = arbres.Trouver(aTerre ? "/Global/HitTree/GroundAndWallHits/On_Ground" : "/Global/HitTree");
				bool pris = arbreCoups && v.Entrer(arbres, arbreCoups, CJugeOutil(cible, maintenant / 1000.0f), maintenant / 1000.0f);
				printf("%6.2f s  le coup de %d porte sur %d (%s)%s%s\n", maintenant / 1000.0, c.first, cible, arbres.CheminDe(k.porteur).c_str(),
				       pris ? " : il joue " : "", pris ? arbres.CheminDe(v.joue).c_str() : "");
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
				       a ? "il joue" : "rien pour lui", a ? arbres.CheminDe(c.second.joue).c_str() : "",
				       b ? "la cible joue" : "rien pour la cible", b ? arbres.CheminDe(controleurs[cible].joue).c_str() : (e.cible ? arbres.CheminDe(e.cible).c_str() : ""));
			}
			// PlayOnTarget : la cible du piéton (FUN_004d5980 ; dans l'outil, celle de PedLockTarget) joue le nœud.
			for(const CActionNode *n : c.second.pourCible){
				const CScriptPed *p = pietons.Acceder(c.first);
				if(p == nil || p->verrou < 0) continue;
				CActionController &cible = controleurs[p->verrou];
				if(!cible.Entrer(arbres, n, CJugeOutil(p->verrou, maintenant / 1000.0f), maintenant / 1000.0f)) cible.Jouer(arbres, n, maintenant / 1000.0f);
				partenaires[p->verrou] = {c.first, maintenant + 2000};
				printf("%6.2f s  la cible de %d (%d) joue %s\n", maintenant / 1000.0, c.first, p->verrou, arbres.CheminDe(cible.joue).c_str());
			}
		}
		intentionsPietons.clear();   // FUN_005f3720 : l'intention ne vaut que pour l'image
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
		printf("globales de script lues avant d'être posées (nil) :");
		for(size_t i = 0; i < tri2.size() && i < 12; i++) printf(" %s (%d)", tri2[i].second.c_str(), -tri2[i].first);
		printf("\n");
	}
	lua_close(L);
	return 0;
}
