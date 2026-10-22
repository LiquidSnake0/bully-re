#include "ActionBank.h"
#include "CdStream.h"
#include "FileMgr.h"
#include <cstdlib>
#include <cstring>

CActionBank::CActionBank(void)
{
	racine.genre = 'b';
	CActionNode global;
	global.genre = 'b';
	global.nom = "Global";
	global.hachage = ActionHash("Global");
	racine.enfants.push_back(global);
}

// FUN_005f6c50 : un « / » de tête retiré, « .cat » écrit à la place des quatre derniers caractères ;
// les fichiers de Act.img n'ont pas de dossier, on ne garde que le nom.
std::string
CActionBank::NomCat(const char *act)
{
	std::string s = act ? act : "";
	size_t barre = s.find_last_of("/\\");
	if(barre != std::string::npos) s = s.substr(barre + 1);
	size_t point = s.rfind('.');
	if(point != std::string::npos) s.resize(point);
	return s + ".cat";
}

bool
CActionBank::EstCharge(const char *act) const
{
	std::string cat = NomCat(act);
	uint32 h = ActionHash(cat.substr(0, cat.size() - 4).c_str());
	for(uint32 x : hachages) if(x == h) return true;
	return false;
}

// Act/Act.img, ajoutée une seule fois aux images.
static int32
ImageAct(void)
{
	static int32 image = -1;
	if(image < 0) image = CdStream::AddImage("Act\\Act.img");
	return image;
}

// Le contenu d'un .cat de Act/Act.img.
static bool
LireCat(const CDirectoryEntry *d, std::vector<uint8> &buf)
{
	buf.resize(d->size * CDSTREAM_SECTOR_SIZE);
	int32 fd = CFileMgr::OpenFile("Act\\Act.img", "rb", 1);
	if(fd == 0) return false;
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf.data(), (uint32)buf.size());
	CFileMgr::CloseFile(fd);
	return ok;
}

static bool
ChercherButes(const CActionTreeFile &f, const CActionNode &n, uint32 type, CActionTrack &piste)
{
	for(int32 p : n.pistes){
		CActionTrack t;
		if(f.Piste(p, t) && t.type == type){ piste = t; return true; }
	}
	for(const CActionNode &e : n.enfants) if(ChercherButes(f, e, type, piste)) return true;
	return false;
}

bool
CActionBank::Butes(const char *nom, CActionTrack &piste, const char *cat)
{
	int32 image = ImageAct();
	if(image < 0) return false;
	const CDirectoryEntry *d = CdStream::ms_images[image].Find(cat);
	std::vector<uint8> buf;
	CActionTreeFile f;
	if(d == nil || !LireCat(d, buf) || !f.Load(buf.data(), (uint32)buf.size())) return false;
	return ChercherButes(f, f.racine, ActionHash(nom), piste);
}

float
CActionBank::ButesReel(const CActionTrack &piste, uint16 position)
{
	uint32 m = piste.Mot(position);
	float r;
	memcpy(&r, &m, 4);
	return r;
}

bool
CActionBank::Charger(const char *act)
{
	if(EstCharge(act)) return true;
	int32 image = ImageAct();
	if(image < 0) return false;
	std::string cat = NomCat(act);
	const CDirectoryEntry *d = CdStream::ms_images[image].Find(cat.c_str());
	if(d == nil) return false;
	std::vector<uint8> buf;
	CActionTreeFile f;
	if(!LireCat(d, buf) || !f.Load(buf.data(), (uint32)buf.size())) return false;
	// La banque de tête du fichier (« 1_01 », « Player ») devient un enfant de /Global ; une banque du
	// même nom déjà là reçoit ses enfants (vtable +0x18 de la racine).
	CActionNode *cible = nil;
	for(CActionNode &e : Global()->enfants) if(e.hachage == f.racine.hachage){ cible = &e; break; }
	if(cible == nil) Global()->enfants.push_back(std::move(f.racine));
	else for(CActionNode &e : f.racine.enfants) cible->enfants.push_back(std::move(e));
	fichiers.push_back(cat);
	hachages.push_back(ActionHash(cat.substr(0, cat.size() - 4).c_str()));
	return true;
}

// FUN_005f59b0 : la pile des nœuds traversés tient lieu de pointeur de parent pour « .. ».
const CActionNode *
CActionBank::Trouver(const char *chemin) const
{
	if(chemin == nil || (*chemin != '/' && *chemin != '\\')) return nil;
	std::vector<const CActionNode*> pile(1, &racine);
	const char *p = chemin + 1;
	while(*p){
		const char *e = p;
		while(*e && *e != '/' && *e != '\\') e++;
		std::string segment(p, e);
		p = *e ? e + 1 : e;
		if(segment.empty()) continue;
		if(segment == ".."){ if(pile.size() > 1) pile.pop_back(); continue; }
		uint32 h = ActionHash(segment.c_str());
		const CActionNode *courant = pile.back(), *suivant = nil;
		for(const CActionNode &x : courant->enfants) if(x.hachage == h){ suivant = &x; break; }
		if(suivant == nil) return nil;
		pile.push_back(suivant);
	}
	return pile.back();
}

static bool
Contient(const CActionNode &a, const CActionNode *n)
{
	for(const CActionNode &e : a.enfants) if(&e == n || Contient(e, n)) return true;
	return false;
}

bool
CActionBank::Joue(const CActionNode *courant, const char *chemin, bool sous) const
{
	const CActionNode *n = Trouver(chemin);
	if(n == nil || courant == nil) return false;
	if(courant == n) return true;
	return sous && Contient(*n, courant);   // remonter les parents de `courant` jusqu'à `n` revient à le chercher sous `n`
}

bool
CActionBank::TrouverOuCharger(const char *chemin, const char *act)
{
	if(Trouver(chemin)) return true;
	if(act == nil || *act == 0 || EstCharge(act)) return false;
	if(!Charger(act)) return false;
	return Trouver(chemin) != nil;
}

const CActionNode *
CActionBank::Choisir(const CActionNode *depuis, Conditions conditions, void *contexte) const
{
	if(depuis == nil) return nil;
	if(!depuis->conditions.empty() && (conditions == nil || !conditions(*depuis, contexte))) return nil;
	switch(depuis->genre){
	case 'n': case 'l':
		return depuis;                       // FUN_005f5500
	case 'b':
		for(const CActionNode &e : depuis->enfants){   // FUN_005f5490 : le premier enfant qui aboutit
			const CActionNode *r = Choisir(&e, conditions, contexte);
			if(r) return r;
		}
		return nil;
	default:
		return nil;
	}
}

// PTR_s_Act_Globals_act_00b0a4f0 : la liste que FUN_005f6f20 charge au démarrage, dans cet ordre.
const char *const CActionBank::globaux[68] = {
	"Act/Globals.act", "Act/Missions.act", "Act/CameraActions.act", "Act/Player.act", "Act/PlayerAI.act",
	"Act/Anim/GS_Female_A.act", "Act/AI/AI.act", "Act/Anim/N_Striker_B.act", "Act/Anim/N_Striker_A.act",
	"Act/Anim/N_Ranged_A.act", "Act/Anim/J_Striker_A.act", "Act/Anim/J_Grappler_A.act",
	"Act/Anim/J_Melee_A.act", "Act/Anim/G_Striker_A.act", "Act/Anim/G_Grappler_A.act", "Act/Anim/G_Melee_A.act",
	"Act/Anim/P_Striker_A.act", "Act/Anim/P_Grappler_A.act", "Act/Anim/P_Striker_B.act",
	"Act/Anim/DO_Grappler_A.act", "Act/Anim/DO_Striker_A.act", "Act/Anim/Authority.act",
	"Act/Anim/LE_Orderly_A.act", "Act/Anim/TE_Female_A.act", "Act/Anim/CV_Male_A.act", "Act/Anim/GS_Male_A.act",
	"Act/Anim/GS_Fat_A.act", "Act/Anim/B_Striker_A.act", "Act/Anim/CV_Female_A.act", "Act/Anim/J_Mascot.act",
	"Act/Anim/CV_OLD.act", "Act/Anim/AN_RAT.act", "Act/AI/AI_Rat.act", "Act/Anim/AN_DOG.act",
	"Act/AI/AI_Dog.act", "Act/Anim/G_Johnny.act", "Act/Globals/GlobalActions.act", "Act/Globals/SimpleLoco.act",
	"Act/HitTree.act", "Act/Weapons.act", "Act/Vehicles.act", "Act/WProps.act", "Act/Anim/Ambient.act",
	"Act/Globals/Cameras.act", "Act/Globals/PlayerFacial.act", "Act/Globals/NPCFacial.act",
	"Act/Globals/CutScene.act", "Act/Weapons/Basketball.act", "Act/Weapons/Bat.act", "Act/Weapons/BBGun.act",
	"Act/Weapons/Books.act", "Act/Weapons/FlashLight.act", "Act/Weapons/Gun.act", "Act/Weapons/KickMeSign.act",
	"Act/Weapons/JBroom.act", "Act/Weapons/NonWeapon.act", "Act/Weapons/Shields.act", "Act/Weapons/Slasher.act",
	"Act/Weapons/SledgeHammer.act", "Act/Weapons/SlingShot.act", "Act/Weapons/Spraycan.act",
	"Act/Weapons/SnowShovel.act", "Act/Weapons/Thrown.act", "Act/Weapons/Umbrella.act",
	"Act/Weapons/WCamera.act", "Act/Weapons/WFireExt.act", "Act/Weapons/WaterPipe.act",
	"Act/Weapons/EdgarShield.act"
};

int
CActionBank::ChargerGlobaux(void)
{
	int n = 0;
	for(const char *act : globaux) n += Charger(act);
	return n;
}

// Le chemin de la racine jusqu'à `cible` (compris), dans `pile` ; faux si `cible` n'est pas sous `n`.
static bool
Chemin(const CActionNode &n, const CActionNode *cible, std::vector<const CActionNode *> &pile)
{
	pile.push_back(&n);
	if(&n == cible) return true;
	for(const CActionNode &e : n.enfants) if(Chemin(e, cible, pile)) return true;
	pile.pop_back();
	return false;
}

bool
CActionBank::JoueNom(const CActionNode *courant, const char *nom) const
{
	if(courant == nil || nom == nil) return false;
	uint32 h = ActionHash(nom);
	std::vector<const CActionNode *> pile;
	if(!Chemin(racine, courant, pile)) return courant->hachage == h;
	for(const CActionNode *n : pile) if(n->hachage == h) return true;
	return false;
}
