#include "NifSkin.h"
#include <cmath>
#include <cstring>

static void
Parcourir(const CNifFile &f, int32 bloc, const NifTransform &parent, int profondeur, NifTransform *out, const NifMatrix33 *pose, bool espaceEntite)
{
	if(bloc < 0 || bloc >= f.numBlocks || profondeur > 64 || f.blocks[bloc].data == nil) return;
	const NifBlock &b = f.blocks[bloc];
	if(b.kind != NIF_NODE && b.kind != NIF_TRISHAPE && b.kind != NIF_TRISTRIPS) return;
	const NifAVObject &o = *(const NifAVObject*)b.data;
	NifTransform t = (espaceEntite && profondeur <= 1) ? parent : NifCompose(parent, o);
	if(pose && b.kind == NIF_NODE){
		// la rotation de pose s'ajoute dans le repère local du nœud
		NifAVObject p; memset(&p, 0, sizeof(p)); p.rotation = pose[bloc]; p.scale = 1;
		t = NifCompose(t, p);
	}
	out[bloc] = t;
	if(b.kind == NIF_NODE){
		const NifNode *n = (const NifNode*)b.data;
		for(int32 i = 0; i < n->numChildren; i++) Parcourir(f, n->children[i], t, profondeur + 1, out, pose, espaceEntite);
	}
}

void
NifWorldTransforms(const CNifFile &f, NifTransform *out, const NifMatrix33 *pose, bool espaceEntite)
{
	for(int32 i = 0; i < f.numBlocks; i++) out[i] = NifIdentity();
	Parcourir(f, 0, NifIdentity(), 0, out, pose, espaceEntite);
}

static NifTransform
DeLiaison(const NifMatrix33 &m, const CVector &t, float s)
{
	NifAVObject o; memset(&o, 0, sizeof(o));
	o.rotation = m; o.translation = t; o.scale = s;
	return NifCompose(NifIdentity(), o);
}

static NifTransform
Composer(const NifTransform &a, const NifTransform &b)
{
	NifAVObject o; memset(&o, 0, sizeof(o));
	for(int i = 0; i < 3; i++) for(int j = 0; j < 3; j++) o.rotation.m[i][j] = b.r[i][j];
	o.translation = b.t; o.scale = b.s;
	return NifCompose(a, o);
}

bool
NifSkinVertices(const CNifFile &f, int32 bloc, const NifTransform *mondes, CVector *out)
{
	if(bloc < 0 || bloc >= f.numBlocks) return false;
	const NifBlock &b = f.blocks[bloc];
	if((b.kind != NIF_TRISHAPE && b.kind != NIF_TRISTRIPS) || !b.data) return false;
	const NifGeometry &g = *(const NifGeometry*)b.data;
	if(g.skin < 0 || g.skin >= f.numBlocks || f.blocks[g.skin].kind != NIF_SKININSTANCE || !f.blocks[g.skin].data) return false;
	if(g.data < 0 || g.data >= f.numBlocks || !f.blocks[g.data].data) return false;
	const NifSkinInstance &si = *(const NifSkinInstance*)f.blocks[g.skin].data;
	if(si.data < 0 || si.data >= f.numBlocks || f.blocks[si.data].kind != NIF_SKINDATA || !f.blocks[si.data].data) return false;
	const NifSkinData &sd = *(const NifSkinData*)f.blocks[si.data].data;
	const NifGeometryData &d = *(const NifGeometryData*)f.blocks[g.data].data;
	if(!d.vertices || !sd.hasWeights || sd.numBones != si.numBones) return false;

	int32 n = d.numVertices;
	float *poids = (float*)calloc(n ? n : 1, sizeof(float));
	for(int32 i = 0; i < n; i++) out[i] = CVector(0, 0, 0);
	for(int32 k = 0; k < sd.numBones; k++){
		int32 os = si.bones[k];
		if(os < 0 || os >= f.numBlocks) continue;
		const NifSkinBone &sb = sd.bones[k];
		NifTransform m = Composer(mondes[os], DeLiaison(sb.rotation, sb.translation, sb.scale));
		for(int32 i = 0; i < sb.numVertices; i++){
			int32 v = sb.indices[i]; float w = sb.weights[i];
			if(v >= n) continue;
			CVector p = NifApply(m, d.vertices[v]);
			out[v].x += w * p.x; out[v].y += w * p.y; out[v].z += w * p.z; poids[v] += w;
		}
	}
	for(int32 i = 0; i < n; i++){
		if(poids[i] < 1e-3f){ out[i] = NifApply(mondes[bloc], d.vertices[i]); continue; }
		if(fabsf(poids[i] - 1) > 1e-4f){ out[i].x /= poids[i]; out[i].y /= poids[i]; out[i].z /= poids[i]; }
	}
	free(poids);
	return true;
}

int32
NifFindNode(const CNifFile &f, const char *nom)
{
	for(int32 i = 0; i < f.numBlocks; i++){
		if(f.blocks[i].kind != NIF_NODE || !f.blocks[i].data) continue;
		const char *s = f.String(((const NifAVObject*)f.blocks[i].data)->name);
		if(s && strcmp(s, nom) == 0) return i;
	}
	return -1;
}

NifMatrix33
NifAxisRotation(int32 axe, float angle)
{
	NifMatrix33 m; memset(&m, 0, sizeof(m));
	float c = cosf(angle), s = sinf(angle);
	int32 a = (axe + 1) % 3, b = (axe + 2) % 3;
	m.m[axe][axe] = 1;
	m.m[a][a] = c; m.m[a][b] = -s;
	m.m[b][a] = s; m.m[b][b] = c;
	return m;
}
