// Sens du quaternion des placements « Ipl$ », tranché par les modèles eux-mêmes.
//
// Pour une partie des placements, le nœud du modèle (fille de « Scene Root »
// qui porte le nom du modèle) est resté à la position du placement et porte
// une rotation nette : l'artiste a exporté l'objet déjà posé. Ce nœud est
// ignoré au rendu (espace entité), mais sa rotation, dans la convention
// v' = R·v de NifTransform, doit être celle que le placement encode. On
// compare donc R(nœud) à la matrice du quaternion et à celle de son conjugué.
//   BULLY_DATA=<racine du jeu> build/tests/test_placement
#include "../outils/commun.h"
#include "../src/gamebryo/NifTransform.h"
#include "../src/core/IplFile.h"
#include <cmath>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC ligne %d : %s\n", __LINE__, #cond); echecs++; } }while(0)

static std::vector<CIplInst> g_inst;
static int32 GarderInst(const CIplInst &e){ g_inst.push_back(e); return 0; }

// Formule usuelle, non transposée : ce que NifPlacementRotation n'est pas.
static void
Usuelle(const float q[4], float r[3][3])
{
	float x = q[0], y = q[1], z = q[2], w = q[3];
	r[0][0] = 1 - 2*(y*y + z*z); r[0][1] = 2*(x*y - z*w);     r[0][2] = 2*(x*z + y*w);
	r[1][0] = 2*(x*y + z*w);     r[1][1] = 1 - 2*(x*x + z*z); r[1][2] = 2*(y*z - x*w);
	r[2][0] = 2*(x*z - y*w);     r[2][1] = 2*(y*z + x*w);     r[2][2] = 1 - 2*(x*x + y*y);
}

static float Ecart(const float a[3][3], const NifMatrix33 &b){ float d = 0; for(int i = 0; i < 3; i++) for(int j = 0; j < 3; j++) d = fmaxf(d, fabsf(a[i][j] - b.m[i][j])); return d; }

// Le nœud du modèle : fille directe de la racine qui porte le nom du modèle.
static const NifAVObject *
NoeudDuModele(const CNifFile &f, const char *nom)
{
	if(f.numBlocks == 0 || f.blocks[0].kind != NIF_NODE) return nil;
	const NifNode *root = (const NifNode*)f.blocks[0].data;
	for(int32 i = 0; i < root->numChildren; i++){
		int32 c = root->children[i];
		if(c < 0 || c >= f.numBlocks || f.blocks[c].data == nil) continue;
		const NifBlock &b = f.blocks[c];
		if(b.kind != NIF_NODE && b.kind != NIF_TRISHAPE && b.kind != NIF_TRISTRIPS) continue;
		const NifAVObject *o = (const NifAVObject*)b.data;
		if(strcasecmp(f.String(o->name), nom) == 0) return o;
	}
	return nil;
}

int
main(void)
{
	// Unitaire : 90° autour de Z, quaternion (0, 0, sin 45°, cos 45°). Dans la
	// convention des placements, X du modèle part vers −Y du monde.
	{
		float q[4] = { 0, 0, 0.70710678f, 0.70710678f };
		CVector un(1, 1, 1);
		NifTransform p = NifFromPlacement(CVector(10, 0, 0), un, q);
		CVector v = NifApply(p, CVector(1, 0, 0));
		VERIF(fabsf(v.x - 10) < 1e-5f && fabsf(v.y + 1) < 1e-5f && fabsf(v.z) < 1e-5f);
		// échelle par axe avant rotation : (2, 3, 1) étire X du modèle par 2
		NifTransform s = NifFromPlacement(CVector(0, 0, 0), CVector(2, 3, 1), q);
		v = NifApply(s, CVector(1, 0, 0));
		VERIF(fabsf(v.x) < 1e-5f && fabsf(v.y + 2) < 1e-5f);
	}

	outil::Archives a;
	if(!outil::Ouvrir(a)){ printf("archives introuvables (BULLY_DATA ?)\n"); return 1; }
	const CdImage &img = CdStream::ms_images[a.monde];
	int cas = 0, transposee = 0, usuelle = 0, aucune = 0;
	for(int k = 0; k < img.m_numEntries; k++){
		const CDirectoryEntry *d = &img.m_entries[k];
		size_t L = strlen(d->name);
		if(L < 4 || strcasecmp(d->name + L - 4, ".ipb")) continue;
		uint32 nb; uint8 *buf = outil::LireMonde(a, d->name, &nb);
		if(buf == nil) continue;
		g_inst.clear(); CIplFile::ms_instHandler = GarderInst; CIplFile::Load(buf, nb); free(buf);
		for(const CIplInst &e : g_inst){
			if(e.name[0] == 0) continue;
			uint32 n; uint8 *b = outil::LireMonde(a, std::string(e.name) + ".nif", &n);
			if(b == nil) continue;
			CNifFile f;
			if(f.Load(b, n)){
				const NifAVObject *o = NoeudDuModele(f, e.name);
				if(o){
					float horsIdentite = 0;
					for(int i = 0; i < 3; i++) for(int j = 0; j < 3; j++) horsIdentite = fmaxf(horsIdentite, fabsf(o->rotation.m[i][j] - (i == j)));
					float dp = fabsf(o->translation.x - e.pos.x) + fabsf(o->translation.y - e.pos.y) + fabsf(o->translation.z - e.pos.z);
					if(horsIdentite > 0.1f && dp < 0.5f){
						cas++;
						float rt[3][3], ru[3][3];
						NifPlacementRotation(e.rot, rt); Usuelle(e.rot, ru);
						float et = Ecart(rt, o->rotation), eu = Ecart(ru, o->rotation);
						if(et < 1e-3f) transposee++; else if(eu < 1e-3f) usuelle++; else aucune++;
						printf("  %-20s transposée %.6f  usuelle %.6f\n", e.name, et, eu);
					}
				}
				f.Free();
			}
			free(b);
		}
	}
	printf("%d nœuds de modèle tournés à la position de leur placement : %d suivent la formule transposée, %d l'usuelle, %d aucune\n", cas, transposee, usuelle, aucune);
	VERIF(transposee >= 5);
	VERIF(usuelle == 0);
	printf(echecs ? "%d échec(s)\n" : "OK\n", echecs);
	return echecs ? 1 : 0;
}
