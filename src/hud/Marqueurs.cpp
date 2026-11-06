#include "Marqueurs.h"

static int
Libre(const CTraceRadar *t)
{
	for(int i = 0; i < CMarqueurs::kTaille; i++) if(!t[i].Prise()) return i;
	return -1;
}

static int32
Monter(CTraceRadar &t, int i)
{
	t.generation = t.generation < 0xfffe ? t.generation + 1 : 1;
	return t.generation << 12 | i;
}

int32
CMarqueurs::AjouterEntite(int32 genre, int32 entite, int32 couleur, int32 icone, bool b, uint32 maintenant)
{
	bool pieton = genre == 2 || genre == 0xd || genre == 6;
	if((icone == 2 || icone == 0x11 || icone == 0x1b) && !pieton) return -1;
	int i = Libre(traces);
	if(i < 0) return -1;
	CTraceRadar &t = traces[i];
	t.genre = genre; t.entite = entite; t.couleur = couleur; t.icone = icone;
	t.drapeaux = (uint8)((t.drapeaux & 0xe3) | (b ? 4 : 0) | 3);
	t.affichage = 1;
	t.echeance = maintenant + 0x9c4;
	return Monter(t, i);
}

int32
CMarqueurs::AjouterPoint(int32 genre, const CVector &pos, int32 couleur, int32 icone, bool b, uint32 maintenant)
{
	if(pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f) return -1;
	int i = Libre(traces);
	if(i < 0) return -1;
	CTraceRadar &t = traces[i];
	t.genre = genre; t.pos = pos; t.entite = 0; t.couleur = couleur; t.icone = icone;
	t.drapeaux = (uint8)((((t.drapeaux & ~4) | (b ? 4 : 0)) | 3) & 0xf7) | 0x10;
	t.affichage = 1;
	t.echeance = maintenant + 0x9c4;
	return Monter(t, i);
}

int
CMarqueurs::Indice(int32 h) const
{
	if(h == -1) return -1;
	uint32 i = (uint32)h & 0xf0000fffu;
	if(i >= (uint32)kTaille) return -1;
	return (traces[i].generation << 12 | (int32)i) == h ? (int)i : -1;
}

static void
Rendre(CTraceRadar &t)
{
	t.drapeaux &= 0xe5;
	t.genre = 0; t.couleur = 0; t.icone = -1;
}

void
CMarqueurs::Retirer(int32 h)
{
	int i = Indice(h);
	if(i >= 0) Rendre(traces[i]);
}

int
CMarqueurs::RetirerEntite(int32 genre, int32 entite)
{
	int n = 0;
	for(CTraceRadar &t : traces)
		if(t.genre == genre && t.entite == entite){ Rendre(t); n++; }
	return n;
}
