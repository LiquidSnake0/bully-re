#include "DatPools.h"
#include "ActionTree.h"
#include "CdStream.h"
#include "FileMgr.h"
#include <cstdlib>
#include <cstring>

static uint32
HachageNom(const char *nom)
{
	if(nom && *nom == '_') nom++;        // FUN_005db130 & co : le « _ » de POINTLIST._NOM n'est pas dans le fichier
	return ActionHash(nom) & 0x7fffffff;
}

template<class T> int
CDatPools::Placer(std::vector<Entree<T>> &pool, const std::string &fichier, int emplacement, uint32 h, T &&d, int premier)
{
	while((int)pool.size() < premier) pool.emplace_back();
	for(size_t i = premier; i < pool.size(); i++)
		if(pool[i].libre){ pool[i] = Entree<T>{fichier, emplacement, h, false, std::move(d)}; return (int)i; }
	pool.push_back(Entree<T>{fichier, emplacement, h, false, std::move(d)});
	return (int)pool.size() - 1;
}

template<class T> int
CDatPools::Chercher(const std::vector<Entree<T>> &pool, uint32 h, int premier)
{
	for(size_t i = premier; i < pool.size(); i++)
		if(!pool[i].libre && pool[i].hachage == h) return (int)i;
	return -1;
}

template<class T> void
CDatPools::Liberer(std::vector<Entree<T>> &pool, int emplacement)
{
	for(Entree<T> &e : pool)
		if(!e.libre && e.emplacement == emplacement){ e.libre = true; e.donnee = T(); }
}

bool
CDatPools::Charger(const char *fichier, int emplacement)
{
	// L'image est ouverte une fois : le moteur n'a qu'un nombre fixe d'images, et DATLoad revient souvent.
	static int32 img = -1;
	if(img < 0) img = CdStream::AddImage("DAT\\Trigger.img");
	if(img < 0) return false;
	const CDirectoryEntry *d = CdStream::ms_images[img].Find(fichier);
	if(d == nil) return false;
	uint32 octets = d->size * CDSTREAM_SECTOR_SIZE;
	uint8 *buf = (uint8*)malloc(octets);
	int32 fd = CFileMgr::OpenFile("DAT\\Trigger.img", "rb", 1);
	CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
	bool ok = CFileMgr::ReadExact(fd, buf, octets);
	CFileMgr::CloseFile(fd);
	CTriggerFile f;
	if(ok) ok = f.Load(buf, octets);
	free(buf);
	if(!ok) return false;
	std::string nom = fichier;
	for(CTriggerPointList &pl : f.pointLists){ uint32 h = HachageNom(pl.nom.c_str()); Placer(listes, nom, emplacement, h, std::move(pl), 1); }
	for(CTriggerPath &p : f.paths){ uint32 h = HachageNom(p.nom.c_str()); Placer(trajets, nom, emplacement, h, std::move(p), 1); }
	// Les zones gardent l'index de leur périmètre dans le fichier : on le décale dans notre table.
	int32 base = (int32)perimetres.size();
	for(CTriggerPerimeter &pe : f.perimetres) perimetres.push_back(std::move(pe));
	for(CTriggerZone &z : f.zones){ if(z.perimetre >= 0) z.perimetre += base; uint32 h = HachageNom(z.nom.c_str()); Placer(zones, nom, emplacement, h, std::move(z), 0); }
	fichiersCharges++;
	return true;
}

void
CDatPools::Decharger(int emplacement)
{
	Liberer(listes, emplacement); Liberer(trajets, emplacement); Liberer(zones, emplacement);
	fichiersCharges = 0;
	for(const Entree<CTriggerPointList> &e : listes) if(!e.libre) fichiersCharges++;   // approximatif : compte les listes
}

void
CDatPools::Vider(void)
{
	listes.clear(); trajets.clear(); zones.clear(); perimetres.clear();
	fichiersCharges = 0;
}

int CDatPools::ListePoints(const char *nom) const { return Chercher(listes, HachageNom(nom), 1); }
int CDatPools::Trajet(const char *nom) const { return Chercher(trajets, HachageNom(nom), 1); }
int CDatPools::Declencheur(const char *nom) const { return Chercher(zones, HachageNom(nom), 0); }

const CTriggerPointList *
CDatPools::Liste(int h) const
{
	if(h < 1 || h >= (int)listes.size() || listes[h].libre) return nil;
	return &listes[h].donnee;
}

const CTriggerPath *
CDatPools::TrajetParHandle(int h) const
{
	if(h < 1 || h >= (int)trajets.size() || trajets[h].libre) return nil;
	return &trajets[h].donnee;
}

const CTriggerZone *
CDatPools::ZoneParHandle(int h) const
{
	if(h < 0 || h >= (int)zones.size() || zones[h].libre) return nil;
	return &zones[h].donnee;
}

bool
CDatPools::DansDeclencheur(int h, const CVector &pos) const
{
	const CTriggerZone *z = ZoneParHandle(h);
	return z != nil && z->Contient(perimetres, pos.x, pos.y, pos.z);
}

const CTriggerPointPoint *
CDatPools::Point(int h, int n) const
{
	const CTriggerPointList *l = Liste(h);
	if(l == nil || l->points.empty()) return nil;
	// FUN_006d58a0 : au-delà du dernier point de la chaîne, le moteur garde le dernier.
	if(n < 1) n = 1;
	if(n > (int)l->points.size()) n = (int)l->points.size();
	return &l->points[n - 1];
}
