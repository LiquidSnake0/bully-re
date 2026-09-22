#include "ColModel.h"
#include <cstring>
#include <cstdlib>

extern void *GameMalloc(size_t size);

void
CColSphere::Set(float r, const CVector &c, uint8 surf, uint8 p)
{
	radius = r; center = c; surface = surf; piece = p;
}

void
CColBox::Set(const CVector &mn, const CVector &mx, uint8 surf, uint8 p)
{
	min = mn; max = mx; surface = surf; piece = p;
}

void
CColModel::Init(void)
{
	memset(this, 0, sizeof(*this));
}

void
CColModel::RemoveCollisionVolumes(void)
{
	if(pColData){
		free(pColData->spheres); free(pColData->boxes); free(pColData->vertices);
		free(pColData->triangles); free(pColData->kdTree); free(pColData->triLinks);
		free(pColData);
		pColData = nil;
	}
	hasGeometry = false;
}

void (*CColLoader::ms_handler)(int32 id, const char *name, CColModel *model, uint8 colSlot) = nil;

static inline int32 RdInt(const uint8 *&p) { int32 v; memcpy(&v, p, 4); p += 4; return v; }
static inline float RdFloat(const uint8 *&p) { float v; memcpy(&v, p, 4); p += 4; return v; }
static inline CVector RdVec(const uint8 *&p) { CVector v; v.x = RdFloat(p); v.y = RdFloat(p); v.z = RdFloat(p); return v; }

// 0x0042bb60
bool
CColLoader::LoadCollisionModel(const uint8 *buf, uint32 size, CColModel &model, uint16 version, uint16 flags)
{
	const uint8 *p = buf;
	const uint8 *end = buf + size;

	// 0x42a970 : bornes, les deux bourrages de la boîte ne sont pas copiés
	model.boundingSphere.center = RdVec(p);
	model.boundingSphere.radius = RdFloat(p);
	model.boundingBox.min = RdVec(p); p += 4;
	model.boundingBox.max = RdVec(p); p += 4;

	model.RemoveCollisionVolumes();
	CCollisionData *cd = (CCollisionData*)calloc(1, sizeof(CCollisionData));
	model.pColData = cd;

	int32 n = (int8)RdInt(p);                       // le binaire ne lit que l'octet bas
	if(n > 0){
		cd->numSpheres = n;
		cd->spheres = (CColSphere*)calloc(n, sizeof(CColSphere));
		for(int32 i = 0; i < n; i++){
			CVector c = RdVec(p); float r = RdFloat(p);
			cd->spheres[i].Set(r, c, p[0], p[1]); p += 4;
		}
	}
	n = (int8)RdInt(p);                             // lignes : sautées
	if(n > 0) p += n * 0x20;
	n = (int8)RdInt(p);
	if(n > 0){
		cd->numBoxes = n;
		cd->boxes = (CColBox*)calloc(n, sizeof(CColBox));
		for(int32 i = 0; i < n; i++){
			CVector mn = RdVec(p); p += 4;
			CVector mx = RdVec(p); p += 4;
			cd->boxes[i].Set(mn, mx, p[0], p[1]); p += 4;
		}
	}
	n = RdInt(p);
	if(n > 0){
		cd->numVertices = n;
		uint32 bytes = n * 6;
		cd->vertices = (CompressedVector*)calloc(n, sizeof(CompressedVector));
		if(version < 1){                        // COLL : quatre flottants (le dernier nul) convertis en int16
			for(int32 i = 0; i < n; i++){
				cd->vertices[i].x = (int16)(RdFloat(p) * 128.0f);
				cd->vertices[i].y = (int16)(RdFloat(p) * 128.0f);
				cd->vertices[i].z = (int16)(RdFloat(p) * 128.0f);
				p += 4;
			}
		}else{
			memcpy(cd->vertices, p, bytes);
			if(bytes & 3) bytes = (bytes & ~3) + 4;
			p += bytes;
		}
	}
	int16 nt = (int16)RdInt(p);
	if(nt > 0){
		cd->numTriangles = nt;
		cd->triangles = (CColTriangle*)calloc(nt, sizeof(CColTriangle));
		// COLL / COL2 : quatre dwords par triangle, cinq pour COL2 ou si le
		// premier triangle porte le marqueur (octets 0x12 et 0x13 à 0xff et
		// octet 0xf non nul)
		bool cinq = false;
		for(int32 i = 0; i < nt; i++){
			CColTriangle &t = cd->triangles[i];
			if(version < 3){
				if(i == 0) cinq = version >= 1 || (p[0x12] == 0xff && p[0x13] == 0xff && p[0xf] != 0);
				t.a = (uint16)RdInt(p); t.b = (uint16)RdInt(p); t.c = (uint16)RdInt(p);
				t.surface = (uint8)RdInt(p);
				if(cinq) p += 4;
			}else if(version == 3 && flags == 3){
				memcpy(&t.a, p, 6); t.surface = p[6]; p += 12;
			}else{
				memcpy(&t.a, p, 6); t.surface = p[6]; p += 8;
			}
			t.flag = 1;
		}
	}
	model.hasGeometry = cd->numSpheres + cd->numBoxes + cd->numTriangles != 0;

	if(p != end){
		uint32 tag; memcpy(&tag, p, 4);
		if(tag == COL_KDTREE_MAGIC){
			int32 nodes; memcpy(&nodes, p + 36, 4);
			cd->numKdNodes = nodes;
			uint32 bytes = 40 + nodes * 12;
			cd->kdTree = (uint8*)malloc(bytes);
			memcpy(cd->kdTree, p, bytes);
			p += bytes;
		}
		if(p != end){
			memcpy(&tag, p, 4);
			if(tag == COL_LIMK){
				p += 4;
				model.numLinks = (int16)RdInt(p);
				cd->triLinks = (int32(*)[3])calloc(nt > 0 ? nt : 1, 12);
				for(int32 i = 0; i < nt; i++){
					cd->triLinks[i][0] = RdInt(p); cd->triLinks[i][1] = RdInt(p); cd->triLinks[i][2] = RdInt(p);
				}
			}
		}
	}
	// 0x42a8e0(model) suit dans le binaire : plans des triangles, non recréé
	return p == end;
}

// 0x0042c790
bool
CColLoader::LoadCollisionFile(const uint8 *buf, uint32 size, uint8 colSlot)
{
	while(size > 8){
		ColHeader h; memcpy(&h, buf, sizeof(h));
		if(h.ident == COL_IDENT_PATH){
			buf += 8 + h.size; size -= 8 + h.size;
			continue;
		}
		if(h.ident != COL_IDENT_COLL && h.ident != COL_IDENT_COL2 && h.ident != COL_IDENT_COL3)
			return size - 8 < 2048;
		uint32 hdr = h.ident == COL_IDENT_COLL ? 32 : 36;
		uint16 version = 0, flags = 1;
		if(h.ident == COL_IDENT_COLL){
			memcpy(h.name, buf + 8, 20); memcpy(&h.modelId, buf + 28, 4);
		}else if(h.ident == COL_IDENT_COL2){
			version = (uint16)h.version;     // le binaire garde le dword entier
		}else{
			version = h.version; flags = h.flags;
		}
		char name[21]; memcpy(name, h.name, 20); name[20] = '\0';
		int32 id = name[0] ? -1 : h.modelId;
		CColModel *model = (CColModel*)calloc(1, sizeof(CColModel));
		model->colSlot = colSlot;
		uint32 body = h.size - (hdr - 8);
		bool ok = LoadCollisionModel(buf + hdr, body, *model, version, flags);
		if(ms_handler) ms_handler(id, name, model, colSlot);
		else { model->RemoveCollisionVolumes(); free(model); }
		if(!ok) return false;
		buf += 8 + h.size; size -= 8 + h.size;
	}
	return true;
}
