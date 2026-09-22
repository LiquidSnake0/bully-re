#include "IplFile.h"
#include <cstring>
#include <cstdlib>

int32 (*CIplFile::ms_instHandler)(const CIplInst &e) = nil;
int32 (*CIplFile::ms_railHandler)(const CIplRail &e) = nil;
int32 (*CIplFile::ms_occlHandler)(const CIplOccl &e) = nil;
int32 (*CIplFile::ms_propHandler)(const char *name) = nil;
int32 (*CIplFile::ms_pontHandler)(const CIplPont &e) = nil;
int32 (*CIplFile::ms_poisHandler)(const CIplPois &g, const CIplPoiPoint &p) = nil;
int32 CIplFile::ms_numInst, CIplFile::ms_numRail, CIplFile::ms_numSpec, CIplFile::ms_numProj,
      CIplFile::ms_numOccl, CIplFile::ms_numProp, CIplFile::ms_numPerm, CIplFile::ms_numPont, CIplFile::ms_numPois, CIplFile::ms_numTrig, CIplFile::ms_numPthx;
int32 CIplFile::ms_lastTag;

static inline int32 RdInt(const uint8 *&p) { int32 v; memcpy(&v, p, 4); p += 4; return v; }
// chaîne à longueur préfixée sur un octet
static void RdName(const uint8 *&p, char *dest, int32 size)
{
	int32 n = *p++;
	int32 k = n < size - 1 ? n : size - 1;
	memcpy(dest, p, k); dest[k] = '\0';
	p += n;
}

// 0x00435780 puis 0x00435140 par section
bool
CIplFile::Load(const uint8 *data, uint32 size)
{
	if(size < 23 || memcmp(data, "Ipl$", 4) != 0) return false;
	const uint8 *p = data + 19;
	const uint8 *end = data + size;
	int32 numSections = RdInt(p);
	for(int32 s = 0; s < numSections && p + 8 <= end; s++){
		int32 tag = RdInt(p);
		int32 count = RdInt(p);
		ms_lastTag = tag;
		switch(tag){
		case IPL_INST:
			for(int32 i = 0; i < count; i++){
				CIplInst e; memcpy(&e, p, sizeof(e)); p += 120;
				ms_numInst++;
				if(ms_instHandler) ms_instHandler(e);
			}
			break;
		case IPL_SPEC: p += count * 128; ms_numSpec += count; break;
		case IPL_PROJ: p += count * 108; ms_numProj += count; break;
		case IPL_OCCL:
			for(int32 i = 0; i < count; i++){
				CIplOccl e; memcpy(&e, p, 28); p += 28;
				ms_numOccl++;
				if(ms_occlHandler) ms_occlHandler(e);
			}
			break;
		case IPL_PROP:
			for(int32 i = 0; i < count; i++){
				char name[256]; RdName(p, name, sizeof(name));
				ms_numProp++;
				if(ms_propHandler) ms_propHandler(name);
			}
			break;
		case IPL_RAIL:
			for(int32 i = 0; i < count; i++){
				CIplRail e; memset(&e, 0, sizeof(e));
				RdName(p, e.name, sizeof(e.name));
				e.unk = RdInt(p);
				e.numPoints = RdInt(p);
				e.points = (int32(*)[3])p;
				p += e.numPoints * 12;
				ms_numRail++;
				if(ms_railHandler) ms_railHandler(e);
			}
			break;
		case IPL_PERM:
			for(int32 i = 0; i < count; i++){
				char name[256]; RdName(p, name, sizeof(name));
				RdInt(p); RdInt(p); RdInt(p); int32 n = RdInt(p);   // dword, deux flottants, compte
				p += n * 4;
				ms_numPerm++;
			}
			break;
		case IPL_PONT:
			for(int32 i = 0; i < count; i++){
				CIplPont e; memset(&e, 0, sizeof(e));
				RdName(p, e.name, sizeof(e.name));
				e.d0 = RdInt(p);
				for(int k = 0; k < 3; k++) e.a[k] = RdInt(p);
				for(int k = 0; k < 3; k++) e.b[k] = RdInt(p);
				e.d1 = RdInt(p);
				RdName(p, e.s0, sizeof(e.s0)); RdName(p, e.s1, sizeof(e.s1)); RdName(p, e.s2, sizeof(e.s2));
				for(int k = 0; k < 6; k++) e.c[k] = RdInt(p);
				RdName(p, e.s3, sizeof(e.s3)); RdName(p, e.s4, sizeof(e.s4));
				ms_numPont++;
				if(ms_pontHandler) ms_pontHandler(e);
			}
			break;
		case IPL_POIS:
			for(int32 i = 0; i < count; i++){
				CIplPois g; memset(&g, 0, sizeof(g));
				RdName(p, g.name, sizeof(g.name));
				g.unk = RdInt(p);
				g.numPoints = RdInt(p);
				for(int32 k = 0; k < g.numPoints; k++){
					CIplPoiPoint pt; memset(&pt, 0, sizeof(pt));
					for(int j = 0; j < 4; j++) RdName(p, pt.s[j], sizeof(pt.s[j]));
					for(int j = 0; j < 21; j++) pt.d[j] = RdInt(p);
					ms_numPois++;
					if(ms_poisHandler) ms_poisHandler(g, pt);
				}
			}
			break;
		case IPL_TRIG:
			for(int32 i = 0; i < count; i++){
				char name[256]; RdName(p, name, sizeof(name));
				p += 19 * 4;
				ms_numTrig++;
			}
			break;
		case IPL_PTHX:
			for(int32 i = 0; i < count; i++){
				char name[256]; RdName(p, name, sizeof(name));
				RdInt(p); RdInt(p); int32 n = RdInt(p);
				p += n * 36;
				ms_numPthx++;
			}
			break;
		default:
			return false;
		}
		if(p > end) return false;
	}
	// le fichier est complété par des zéros jusqu'au secteur
	for(; p < end; p++) if(*p) return false;
	return true;
}
