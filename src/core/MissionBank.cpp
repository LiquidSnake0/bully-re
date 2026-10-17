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
