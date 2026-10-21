#include "MissionBank.h"
#include "ActionTree.h"
#include "CdStream.h"
#include "FileMgr.h"
#include <cstdlib>
#include <cstring>
#include <strings.h>

// FUN_006ab7e0 : chaque nœud qui porte une piste MissionData devient une entrée, pistes comprises
// quand un nœud en porte deux (Minigames : ArcadeRace3D en a deux).
static void
Parcourir(const CActionTreeFile &f, const CActionNode &n, const char *banque, std::vector<CMissionData> &dest,
          std::vector<std::string> &references)
{
	static const uint32 kMissionData = ActionHash("MissionData") & 0x7fffffff;
	static const uint32 kBouton = ActionHash("MissionButtonTrigger") & 0x7fffffff;
	if(n.genre == 'r' || n.genre == 'i'){
		// « chap1.act » dans le fichier : le sous-arbre est chap1.cat dans Act.img.
		std::string nom = n.nom;
		size_t pt = nom.rfind('.');
		if(pt != std::string::npos) nom.erase(pt);
		references.push_back(nom + ".cat");
		return;
	}
	for(int32 d : n.pistes){
		CActionTrack t;
		if(!f.Piste(d, t) || t.type != kMissionData) continue;
		CMissionData m;
		m.banque = banque;
		m.nom = n.nom;
		m.hachage = n.hachage & 0x7fffffff;
		const CActionAttribut *a;
		if((a = t.Champ(24)) != nil) m.titre = f.ChaineCitee(a->decalage);
		if((a = t.Champ(28)) != nil) m.script = f.ChaineCitee(a->decalage);
		if((a = t.Champ(32)) != nil) m.cle = f.ChaineCitee(a->decalage);
		if((a = t.Champ(36)) != nil) m.afficherTitre = a->valeur[0] != 0;
		m.type = (int)t.Mot(40);
		if((a = t.Champ(51)) != nil) m.lancerSeul = a->valeur[0] != 0;
		m.conditions = (int)n.conditions.size();
		for(int32 dc : n.conditions){
			CMissionCondition c;
			if(f.Condition(dc, c.type, c.mots)) m.conds.push_back(c);
		}
		// L'enfant « Active » porte la piste MissionButtonTrigger : le corona. Les positions des attributs sont
		// les décalages dans l'objet que FUN_006a8650 lit à chaque image : +0x1c le demi-côté du pavé autour du
		// point, +0x20 le marqueur, +0x24 et +0x28 sa taille, +0x30 la clé de l'invite.
		for(const CActionNode &e : n.enfants)
			for(int32 de : e.pistes){
				CActionTrack b;
				if(!f.Piste(de, b) || b.type != kBouton) continue;
				if((a = b.Champ(24)) != nil) m.corona = f.ChaineCitee(a->decalage);
				if((a = b.Champ(28)) != nil && a->taille >= 4) memcpy(&m.coronaDemiCote, a->valeur, 4);
				m.coronaMarqueur = (int)b.Mot(32);
				if((a = b.Champ(36)) != nil) m.coronaMarqueurVisible = a->valeur[0] != 0;
				m.coronaTaille = (int)b.Mot(40);
				if((a = b.Champ(48)) != nil) m.coronaInvite = f.ChaineCitee(a->decalage);
			}
		dest.push_back(m);
	}
	for(const CActionNode &e : n.enfants) Parcourir(f, e, banque, dest, references);
}

bool
CMissionBank::LireFichier(int32 image, const char *fichier)
{
	const CDirectoryEntry *d = CdStream::ms_images[image].Find(fichier);
	if(d == nil) return false;
	uint32 octets = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(octets);
	int32 fd = CFileMgr::OpenFile("Act\\Act.img", "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, octets);
	CFileMgr::CloseFile(fd);
	CActionTreeFile f;
	std::vector<std::string> references;
	if(ok && (ok = f.Load(buf, octets))){
		banques.push_back(fichier);
		Parcourir(f, f.racine, fichier, missions, references);
	}
	free(buf);
	// Les références d'un fichier, dans son ordre, après son propre contenu.
	for(const std::string &r : references) LireFichier(image, r.c_str());
	return ok;
}

bool
CMissionBank::Charger(void)
{
	missions.clear();
	banques.clear();
	int32 act = CdStream::AddImage("Act\\Act.img");
	if(act < 0) return false;
	return LireFichier(act, "Missions.cat");
}

int
CMissionBank::Index(const char *nom) const
{
	uint32 h = ActionHash(nom) & 0x7fffffff;
	for(size_t i = 0; i < missions.size(); i++){
		const CMissionData &m = missions[i];
		if(!m.nom.empty() && strcasecmp(m.nom.c_str(), nom) == 0) return (int)i;
		if(m.hachage == h) return (int)i;
	}
	return -1;
}

const CMissionData *
CMissionBank::Trouver(const char *nom) const
{
	int i = Index(nom);
	return i < 0 ? nil : &missions[i];
}

static const uint32 kMissionCond = ActionHash("MissionCond") & 0x7fffffff;
static const uint32 kTimeCond = ActionHash("TimeCond") & 0x7fffffff;
static const uint32 kFalse = ActionHash("False") & 0x7fffffff;              // ConditionFalse, enregistrée sous « False »
static const uint32 kCurrentMission = ActionHash("CurrentMission") & 0x7fffffff;
static const uint32 kCurrentClassMission = ActionHash("CurrentClassMission") & 0x7fffffff;

// FUN_00425cf0(h1, h2, m1, m2) : l'heure en minutes depuis minuit dans [h1:m1, h2:m2[, en passant minuit
// quand la fin précède le début.
static bool
DansLaPlage(int maintenant, int debut, int fin)
{
	if(fin < debut) return debut <= maintenant || maintenant < fin;
	return debut <= maintenant && maintenant < fin;
}

int
CMissionBank::Evaluer(const CMissionCondition &c, const CEtatMissions &etat, const char **classe) const
{
	if(classe) *classe = nil;
	if(c.type == kMissionCond){
		if(classe) *classe = "MissionCond";
		uint32 nom = c.mots[0] & 0x7fffffff;
		// CurrentMission et CurrentClassMission passent par la mission en cours (FUN_006a9ed0,
		// FUN_006aa630) : pas encore lues.
		if(nom == kCurrentMission || nom == kCurrentClassMission) return -1;
		// FUN_006a9e70 : la première entrée de ce nom ; -1 rend faux.
		for(size_t i = 0; i < missions.size(); i++)
			if(missions[i].hachage == nom) return i < etat.reussites.size() && etat.reussites[i] != 0;
		return 0;
	}
	if(c.type == kTimeCond){
		if(classe) *classe = "TimeCond";
		int debut = (int)c.mots[0] * 60 + (int)c.mots[2], fin = (int)c.mots[1] * 60 + (int)c.mots[3];
		return DansLaPlage(etat.heure * 60 + etat.minute, debut, fin);
	}
	if(c.type == kFalse){
		if(classe) *classe = "ConditionFalse";
		return 0;
	}
	return -1;
}

int
CMissionBank::Activable(int index, const CEtatMissions &etat, const char **classe) const
{
	if(index < 0 || index >= (int)missions.size()) return 0;
	const CMissionData &m = missions[index];
	if((int)m.conds.size() != m.conditions) return -1;
	int r = 1;
	for(const CMissionCondition &c : m.conds){
		int v = Evaluer(c, etat, classe);
		if(v < 0) return -1;
		if(v == 0) r = 0;
	}
	return r;
}
