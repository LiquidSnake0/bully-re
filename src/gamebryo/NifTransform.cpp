#include "NifTransform.h"

NifTransform
NifIdentity(void)
{
	NifTransform x;
	for(int i = 0; i < 3; i++)
		for(int j = 0; j < 3; j++)
			x.r[i][j] = i == j ? 1.0f : 0.0f;
	x.s = 1.0f;
	x.t = CVector(0.0f, 0.0f, 0.0f);
	return x;
}

CVector
NifRotate(const float r[3][3], const CVector &v)
{
	return CVector(r[0][0]*v.x + r[0][1]*v.y + r[0][2]*v.z,
	               r[1][0]*v.x + r[1][1]*v.y + r[1][2]*v.z,
	               r[2][0]*v.x + r[2][1]*v.y + r[2][2]*v.z);
}

// monde(enfant) = monde(parent) ∘ local(enfant)
NifTransform
NifCompose(const NifTransform &p, const NifAVObject &o)
{
	NifTransform c;
	for(int i = 0; i < 3; i++)
		for(int j = 0; j < 3; j++){
			c.r[i][j] = 0.0f;
			for(int k = 0; k < 3; k++)
				c.r[i][j] += p.r[i][k] * o.rotation.m[k][j];
		}
	c.s = p.s * o.scale;
	CVector tt = NifRotate(p.r, CVector(o.translation.x * p.s, o.translation.y * p.s, o.translation.z * p.s));
	c.t = CVector(p.t.x + tt.x, p.t.y + tt.y, p.t.z + tt.z);
	return c;
}

CVector
NifApply(const NifTransform &x, const CVector &v)
{
	CVector r = NifRotate(x.r, CVector(v.x * x.s, v.y * x.s, v.z * x.s));
	return CVector(r.x + x.t.x, r.y + x.t.y, r.z + x.t.z);
}

static void
Parcourir(const CNifFile &f, int32 bloc, const NifTransform &parent, int profondeur, NifShapeFn fn, void *ctx)
{
	if(bloc < 0 || bloc >= f.numBlocks || profondeur > 64 || f.blocks[bloc].data == nil) return;
	const NifBlock &b = f.blocks[bloc];
	if(b.kind == NIF_NODE){
		const NifNode *n = (const NifNode*)b.data;
		NifTransform t = NifCompose(parent, *n);
		for(int32 i = 0; i < n->numChildren; i++)
			Parcourir(f, n->children[i], t, profondeur + 1, fn, ctx);
		return;
	}
	if(b.kind != NIF_TRISHAPE && b.kind != NIF_TRISTRIPS) return;
	const NifGeometry *g = (const NifGeometry*)b.data;
	if(g->data < 0 || g->data >= f.numBlocks || f.blocks[g->data].data == nil) return;
	const NifGeometryData *d = (const NifGeometryData*)f.blocks[g->data].data;
	fn(f, bloc, *g, *d, NifCompose(parent, *g), ctx);
}

void
NifWalkShapes(const CNifFile &f, NifShapeFn fn, void *ctx)
{
	Parcourir(f, 0, NifIdentity(), 0, fn, ctx);
}
