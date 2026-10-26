#include "ActionBank.h"
#include "ConditionListe.h"
#include "CdStream.h"
#include "FileMgr.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

CActionBank::CActionBank(void)
{
	racine.genre = 'b';
	CActionNode global;
	global.genre = 'b';
	global.nom = "Global";
	global.hachage = ActionHash("Global");
	racine.enfants.push_back(global);
	// Les arbres de tête restent à leur place : les arbres montés gardent l'adresse du nœud qui porte leur référence et de
	// leur banque (la table de FUN_005f6800). 68 fichiers globaux, les missions et les banques des références y tiennent.
	Global()->enfants.reserve(1024);
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


static void
Marquer(CActionNode &n, int16 source)
{
	n.source = source;
	for(CActionNode &e : n.enfants) Marquer(e, source);
}

// Une inclusion ('i') insère à sa place l'arbre d'un autre fichier : « actions » de GlobalActions inclut Grapples.act,
// RisingAttacks.act…, et les scripts désignent ensuite /Global/Actions/Grapples/Front/… . Le nœud 'i' porte le nom du
// fichier et son dossier ; sa banque de tête (« Grapples ») prend la place, avec ses données gardées comme les autres.
// Une référence ('r') reste un lien vers un autre arbre (GlobalLoco), que la descente ne suit pas.
bool
CActionBank::Inclure(CActionNode &n, int prof)
{
	if(prof > 8) return false;
	for(CActionNode &e : n.enfants){
		if(e.genre == 'i'){
			int32 image = ImageAct();
			const CDirectoryEntry *d = image < 0 ? nil : CdStream::ms_images[image].Find(NomCat(e.nom.c_str()).c_str());
			std::shared_ptr<Source> src = std::make_shared<Source>();
			if(d == nil || !LireCat(d, src->donnees) || !src->fichier.Load(src->donnees.data(), (uint32)src->donnees.size())) continue;
			Marquer(src->fichier.racine, (int16)sources.size());
			sources.push_back(src);
			e = std::move(src->fichier.racine);
			Inclure(e, prof + 1);
		}else Inclure(e, prof);
	}
	return true;
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
	std::shared_ptr<Source> src = std::make_shared<Source>();
	CActionTreeFile &f = src->fichier;
	if(!LireCat(d, src->donnees) || !f.Load(src->donnees.data(), (uint32)src->donnees.size())) return false;
	int16 sourceTete = (int16)sources.size();
	Marquer(f.racine, sourceTete);
	sources.push_back(src);
	Inclure(f.racine);
	// La banque de tête du fichier (« 1_01 », « Player ») devient un enfant de /Global ; une banque du
	// même nom déjà là reçoit ses enfants (vtable +0x18 de la racine).
	CActionNode *cible = nil;
	for(CActionNode &e : Global()->enfants) if(e.hachage == f.racine.hachage){ cible = &e; break; }
	uint32 tete = f.racine.hachage;
	if(cible == nil){ Global()->enfants.push_back(std::move(f.racine)); cible = &Global()->enfants.back(); }
	else for(CActionNode &e : f.racine.enfants) cible->enfants.push_back(std::move(e));
	fichiers.push_back(cat);
	hachages.push_back(ActionHash(cat.substr(0, cat.size() - 4).c_str()));
	// FUN_005f6c50 (greffé sous Global) : la clé devient le nom de l'arbre (FUN_005f4e40), puis FUN_005f6bb0 résout ses
	// références.
	cle = tete;
	Monter(*cible, tete, sourceTete);
	return true;
}

void
CActionBank::PoserParent(CActionNode &m, const CActionNode *p, uint32 k)
{
	if(k == 0xffffffffu){ m.parentDefaut = p; return; }
	for(auto &e : m.parents){
		if(e.first == k){ e.second = p; return; }
		if(e.second == p){ e.first = k; return; }
	}
	m.parents.push_back({k, p});
}

const CActionNode *
CActionBank::ParentMonte(const CActionNode &m, uint32 k) const
{
	if(k == 0xffffffffu){ k = cle; if(k == 0xffffffffu) return m.parentDefaut; }
	for(const auto &e : m.parents) if(e.first == k) return e.second;
	return m.parentDefaut;
}

// FUN_005f6bb0 : chaque référence de fichier pas encore résolue (+0xc nul) l'est, puis ses enfants sont parcourus. Une
// référence 'r' (l'octet +0x28 à 0, FUN_005fafb0) passe par FUN_005f6960 ; les inclusions 'i' (FUN_005f6d90) sont déjà
// remplacées par leur arbre (Inclure). La clé est celle de l'arbre de tête (FUN_005f4e40 depuis la référence) ; dans un
// arbre inclus ou monté, résolu avant d'avoir un parent, FUN_005f4e40 tombe sur un parent nul et rend -1.
void
CActionBank::Monter(CActionNode &n, uint32 cleArbre, int16 sourceTete, int prof)
{
	if(prof > 16) return;
	for(CActionNode &e : n.enfants){
		if(e.genre == 'r'){
			if(e.monte) continue;
			uint32 k = e.source == sourceTete ? cleArbre : 0xffffffffu;
			// FUN_005f6960 : la banque /Global/<nom> et son ActionNodeMultiParentReference (le premier enfant, vtable +4).
			uint32 hb = ActionHash(e.nom.c_str());
			CActionNode *banque = nil, *m = nil;
			for(CActionNode &g : Global()->enfants) if(g.hachage == hb){ banque = &g; break; }
			if(banque && !banque->enfants.empty() && banque->enfants[0].multiParent) m = &banque->enfants[0];
			if(m == nil){
				int32 image = ImageAct();
				const CDirectoryEntry *d = image < 0 ? nil : CdStream::ms_images[image].Find(NomCat(e.nom.c_str()).c_str());
				std::shared_ptr<Source> src = std::make_shared<Source>();
				if(d == nil || !LireCat(d, src->donnees) || !src->fichier.Load(src->donnees.data(), (uint32)src->donnees.size()))
					continue;                                  // le fichier manque : la référence reste non résolue
				Marquer(src->fichier.racine, (int16)sources.size());
				sources.push_back(src);
				Inclure(src->fichier.racine);
				if(banque == nil){                            // FUN_005f5e10, greffée sous Global (vtable +0x18)
					CActionNode b;
					b.genre = 'b'; b.nom = e.nom; b.hachage = hb;
					Global()->enfants.push_back(std::move(b));
					banque = &Global()->enfants.back();
				}
				banque->enfants.push_back(std::move(src->fichier.racine));
				m = &banque->enfants.back();
				m->multiParent = true;
				m->parentDefaut = banque;                     // FUN_005f6650 : l'ajout pose le parent (+4)
				PoserParent(*m, banque, banque->hachage);
				Monter(*m, 0xffffffffu, -1, prof + 1);        // ses propres références, à la clé -1
			}
			PoserParent(*m, &n, k);                           // le nœud qui porte la référence, pour sa clé
			e.monte = m;
		}else Monter(e, cleArbre, sourceTete, prof + 1);
	}
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
		for(const CActionNode &x : Enfants(*courant)) if(Nom(x) == h){ suivant = &x; break; }
		if(suivant == nil) return nil;
		if(suivant->nom.empty()) suivant->nom = segment;   // un nom haché, appris du chemin qui le désigne (journaux)
		pile.push_back(suivant);
	}
	return pile.back();
}

bool
CActionBank::Joue(const CActionNode *courant, const char *chemin, bool sous) const
{
	const CActionNode *n = Trouver(chemin);
	if(n == nil || courant == nil) return false;
	if(courant == n) return true;
	return sous && Sous(n, courant);   // FUN_005f4f80 : remonter les parents de `courant` (ceux de la clé courante)
}

bool
CActionBank::Sous(const CActionNode *a, const CActionNode *n) const
{
	if(a == nil || n == nil) return false;
	if(a == n) return true;
	// Une référence montée vaut sa cible : ses descendants sont ceux de l'arbre monté.
	const CActionNode *cible = a->monte ? a->monte : a;
	for(const CActionNode *x : Lignee(n)) if(x == a || x == cible) return true;
	return false;
}

uint32
CActionBank::CleDe(const CActionNode *n) const
{
	std::vector<const CActionNode *> l = Lignee(n);
	return l.size() > 2 ? l[2]->hachage : 0xffffffffu;   // racine, Global, puis l'arbre de tête
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
	if(depuis->monte) return Choisir(depuis->monte, conditions, contexte);   // FUN_005f5470 : la descente de la cible
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
	return JoueHachage(courant, ActionHash(nom));
}

bool
CActionBank::JoueHachage(const CActionNode *courant, uint32 h) const
{
	if(courant == nil) return false;
	std::vector<const CActionNode *> pile = Lignee(courant);
	if(pile.empty()) return courant->hachage == h;
	for(const CActionNode *n : pile) if(n->hachage == h) return true;
	return false;
}

static const uint32 kNot = ActionHash("NOT");                         // ConditionNOT (FUN_00639470)
static const uint32 kOr = ActionHash("OR");                           // ConditionOR (FUN_00639490)
static const uint32 kFalse = ActionHash("False");                     // ConditionFalse
static const uint32 kGroupe = ActionHash("ConditionGroup");

int
CActionBank::JugerListe(const CActionTreeFile &f, const std::vector<int32> &liste, const CJugeAction &juge, int prof) const
{
	if(prof > 8) return -1;
	auto genre = [&](int i){ uint32 t = f.TypeCondition(liste[i]); return t == kNot ? 1 : t == kOr ? 2 : 0; };
	return EvaluerListe((int)liste.size(), [&](int i){
		uint32 type, mots[6];
		if(!f.Condition(liste[i], type, mots)) return -1;
		if(type == kNot || type == kOr) return 1;
		if(type == kFalse) return 0;
		if(type == kGroupe){
			std::vector<int32> groupe;
			return f.GroupeDe(liste[i], groupe) ? JugerListe(f, groupe, juge, prof + 1) : -1;
		}
		return juge.Juger(f, liste[i], type, mots);
	}, genre);
}

int
CActionBank::Juger(const CActionNode &n, const CJugeAction &juge) const
{
	if(n.conditions.empty()) return 1;
	const CActionTreeFile *f = Fichier(n);
	const CActionNode *avant = juge.noeud;
	juge.noeud = &n;                     // le contexte +0x34 : le nœud jugé
	int r = f ? JugerListe(*f, n.conditions, juge) : -1;
	juge.noeud = avant;
	return r;
}

const CActionNode *
CActionBank::Choisir(const CActionNode *depuis, const CJugeAction &juge) const
{
	if(depuis && depuis->monte) return Choisir(depuis->monte, juge);   // FUN_005f5470 : la descente de la cible
	if(depuis == nil || Juger(*depuis, juge) != 1) return nil;
	switch(depuis->genre){
	case 'n': case 'l':
		return depuis;                       // FUN_005f5500
	case 'b':
		for(const CActionNode &e : depuis->enfants){   // FUN_005f5490 : le premier enfant qui aboutit
			const CActionNode *r = Choisir(&e, juge);
			if(r) return r;
		}
		return nil;
	default:
		return nil;
	}
}

std::string
CActionBank::CheminDe(const CActionNode *n) const
{
	std::vector<const CActionNode *> pile = Lignee(n);
	if(pile.empty()) return std::string();
	std::string s;
	for(size_t i = 1; i < pile.size(); i++){
		char h[12];
		snprintf(h, sizeof(h), "#%08x", pile[i]->hachage);
		s += "/" + (pile[i]->nom.empty() ? std::string(h) : pile[i]->nom);
	}
	return s;
}

// Le chemin rangé (Chemin ne traverse pas les références : leurs arbres sont rangés sous /Global/<nom>), puis, si le nœud est
// dans un arbre monté, le parent de la clé courante à la place de sa banque (FUN_005f6290) : la lignée par où « .. »
// remonte.
std::vector<const CActionNode *>
CActionBank::Lignee(const CActionNode *n, int prof) const
{
	std::vector<const CActionNode *> pile;
	if(n == nil || !Chemin(racine, n, pile)){ pile.clear(); return pile; }
	for(size_t i = pile.size(); i-- > 1;){
		if(!pile[i]->multiParent) continue;
		const CActionNode *p = ParentMonte(*pile[i]);
		if(p == nil || p == pile[i - 1] || prof > 8) break;
		std::vector<const CActionNode *> haut = Lignee(p, prof + 1);
		if(haut.empty()) break;
		haut.insert(haut.end(), pile.begin() + (long)i, pile.end());
		return haut;
	}
	return pile;
}

const CActionNode *
CActionBank::Relatif(const CActionNode *depuis, const char *chemin) const
{
	if(chemin == nil) return nil;
	if(*chemin == '/' || *chemin == '\\') return Trouver(chemin);
	std::vector<const CActionNode *> pile = Lignee(depuis);
	if(pile.empty()) return nil;
	const char *p = chemin;
	while(*p){
		const char *e = p;
		while(*e && *e != '/' && *e != '\\') e++;
		std::string segment(p, e);
		p = *e ? e + 1 : e;
		if(segment.empty() || segment == ".") continue;
		if(segment == ".."){ if(pile.size() > 1) pile.pop_back(); continue; }
		uint32 h = ActionHash(segment.c_str());
		const CActionNode *suivant = nil;
		for(const CActionNode &x : Enfants(*pile.back())) if(Nom(x) == h){ suivant = &x; break; }
		if(suivant == nil) return nil;
		if(suivant->nom.empty()) suivant->nom = segment;
		pile.push_back(suivant);
	}
	return pile.back();
}
