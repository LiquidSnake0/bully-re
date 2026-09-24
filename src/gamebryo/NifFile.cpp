#include "NifFile.h"
#include <cstring>
#include <cstdlib>

// Lecteur borné : toute lecture hors du bloc met m_ok à faux, le bloc est
// alors abandonné et compté comme non décodé.
class NifReader
{
public:
	const uint8 *p, *end; bool ok, be;
	NifReader(const uint8 *d, uint32 n, bool bigEndian = false) : p(d), end(d + n), ok(true), be(bigEndian) {}
	bool Need(uint32 n) { if(p + n > end){ ok = false; return false; } return true; }
	uint8 U8(void) { if(!Need(1)) return 0; return *p++; }
	uint16 U16(void) { if(!Need(2)) return 0; uint16 v = be ? (uint16)(p[0] << 8 | p[1]) : (uint16)(p[1] << 8 | p[0]); p += 2; return v; }
	uint32 U32(void) { if(!Need(4)) return 0; uint32 v = be ? ((uint32)p[0] << 24 | (uint32)p[1] << 16 | (uint32)p[2] << 8 | p[3]) : ((uint32)p[3] << 24 | (uint32)p[2] << 16 | (uint32)p[1] << 8 | p[0]); p += 4; return v; }
	int32 I32(void) { return (int32)U32(); }
	float F32(void) { uint32 u = U32(); float v; memcpy(&v, &u, 4); return v; }
	CVector Vec(void) { CVector v; v.x = F32(); v.y = F32(); v.z = F32(); return v; }
	int32 *Refs(int32 n) {
		if(n < 0 || n > 100000 || !Need(n * 4)){ ok = false; return nil; }
		int32 *r = (int32*)malloc((n ? n : 1) * sizeof(int32));
		for(int32 i = 0; i < n; i++) r[i] = I32();
		return r;
	}
	bool Done(void) const { return ok && p == end; }
};

eNifBlock
CNifFile::KindOf(const char *t)
{
	if(strcmp(t, "NiNode") == 0) return NIF_NODE;
	if(strcmp(t, "NiTriShape") == 0) return NIF_TRISHAPE;
	if(strcmp(t, "NiTriStrips") == 0) return NIF_TRISTRIPS;
	if(strcmp(t, "NiTriShapeData") == 0) return NIF_TRISHAPEDATA;
	if(strcmp(t, "NiTriStripsData") == 0) return NIF_TRISTRIPSDATA;
	if(strcmp(t, "NiSourceTexture") == 0) return NIF_SOURCETEXTURE;
	// NiSourceCubeMap dérive de NiSourceTexture et se lit pareil.
	if(strcmp(t, "NiSourceCubeMap") == 0) return NIF_SOURCETEXTURE;
	if(strcmp(t, "NiPixelData") == 0) return NIF_PIXELDATA;
	if(strcmp(t, "NiPalette") == 0) return NIF_PALETTE;
	if(strcmp(t, "NiStringExtraData") == 0) return NIF_STRINGEXTRADATA;
	if(strcmp(t, "NiIntegerExtraData") == 0) return NIF_INTEGEREXTRADATA;
	if(strcmp(t, "NiMaterialProperty") == 0) return NIF_MATERIALPROPERTY;
	if(strcmp(t, "NiTexturingProperty") == 0) return NIF_TEXTURINGPROPERTY;
	return NIF_INCONNU;
}

static void
LireAVObject(NifReader &r, NifAVObject &o)
{
	o.name = r.I32();
	o.numExtra = r.I32(); o.extra = r.Refs(o.numExtra);
	o.controller = r.I32();
	o.flags = r.U16();
	o.translation = r.Vec();
	for(int i = 0; i < 3; i++) for(int j = 0; j < 3; j++) o.rotation.m[i][j] = r.F32();
	o.scale = r.F32();
	o.numProperties = r.I32(); o.properties = r.Refs(o.numProperties);
	o.collision = r.I32();
}

static void *
LireNode(NifReader &r)
{
	NifNode *n = (NifNode*)calloc(1, sizeof(NifNode));
	LireAVObject(r, *n);
	n->numChildren = r.I32(); n->children = r.Refs(n->numChildren);
	n->numEffects = r.I32(); n->effects = r.Refs(n->numEffects);
	return n;
}

// NiGeometry en 20.3.0.9 : données, peau, matériaux (compte, noms, extra),
// matériau actif, drapeau « dirty »
static void *
LireGeometry(NifReader &r)
{
	NifGeometry *g = (NifGeometry*)calloc(1, sizeof(NifGeometry));
	LireAVObject(r, *g);
	g->data = r.I32();
	g->skin = r.I32();
	g->numMaterials = r.I32();
	for(int32 i = 0; i < g->numMaterials && r.ok; i++){ r.I32(); r.I32(); }   // nom, extra data
	g->activeMaterial = r.I32();
	g->dirty = r.U8();
	return g;
}

static void
LireGeometryData(NifReader &r, NifGeometryData &d)
{
	d.groupId = r.I32();
	d.numVertices = r.U16();
	d.keepFlags = r.U8(); d.compressFlags = r.U8();
	int32 nv = d.numVertices;
	if(r.U8()){ d.vertices = (CVector*)malloc(nv * sizeof(CVector) + 1); for(int32 i = 0; i < nv && r.ok; i++) d.vertices[i] = r.Vec(); }
	d.dataFlags = r.U16();
	d.numUVSets = d.dataFlags & 0x3f;
	if(r.U8()){ d.normals = (CVector*)malloc(nv * sizeof(CVector) + 1); for(int32 i = 0; i < nv && r.ok; i++) d.normals[i] = r.Vec(); }
	if(d.dataFlags & 0x1000){                        // tangentes et binormales
		for(int32 i = 0; i < 2 * nv && r.ok; i++) r.Vec();
	}
	d.center = r.Vec(); d.radius = r.F32();
	if(r.U8()){ d.colors = (float(*)[4])malloc(nv * 16 + 1); for(int32 i = 0; i < nv && r.ok; i++) for(int k = 0; k < 4; k++) d.colors[i][k] = r.F32(); }
	if(d.numUVSets > 0){
		d.uv = (float(*)[2])malloc(d.numUVSets * nv * 8 + 1);
		for(int32 i = 0; i < d.numUVSets * nv && r.ok; i++){ d.uv[i][0] = r.F32(); d.uv[i][1] = r.F32(); }
	}
	d.consistency = r.U16();
	d.additionalData = r.I32();
}

static void *
LireTriShapeData(NifReader &r)
{
	NifGeometryData *d = (NifGeometryData*)calloc(1, sizeof(NifGeometryData));
	LireGeometryData(r, *d);
	d->numTriangles = r.U16();
	r.U32();                                        // nombre de points = 3 × triangles
	if(r.U8()){
		d->triangles = (uint16(*)[3])malloc(d->numTriangles * 6 + 1);
		for(int32 i = 0; i < d->numTriangles && r.ok; i++) for(int k = 0; k < 3; k++) d->triangles[i][k] = r.U16();
	}
	uint16 groups = r.U16();                        // groupes d'appariement, ignorés
	for(uint16 g = 0; g < groups && r.ok; g++){ uint16 n = r.U16(); for(uint16 i = 0; i < n && r.ok; i++) r.U16(); }
	return d;
}

// Les bandes sont converties en liste de triangles, orientation alternée,
// les triangles dégénérés (deux indices égaux) écartés.
static void *
LireTriStripsData(NifReader &r)
{
	NifGeometryData *d = (NifGeometryData*)calloc(1, sizeof(NifGeometryData));
	LireGeometryData(r, *d);
	uint16 declared = r.U16();
	d->numStrips = r.U16();
	uint16 *len = (uint16*)malloc((d->numStrips + 1) * 2);
	uint32 total = 0;
	for(uint16 s = 0; s < d->numStrips; s++){ len[s] = r.U16(); total += len[s]; }
	if(r.U8() && r.ok){
		d->triangles = (uint16(*)[3])malloc((total + 1) * 6);
		int32 nt = 0;
		for(uint16 s = 0; s < d->numStrips && r.ok; s++){
			uint16 a = 0, b = 0;
			for(uint16 i = 0; i < len[s] && r.ok; i++){
				uint16 c = r.U16();
				if(i >= 2 && a != b && b != c && a != c){
					if(i & 1){ d->triangles[nt][0] = b; d->triangles[nt][1] = a; d->triangles[nt][2] = c; }
					else     { d->triangles[nt][0] = a; d->triangles[nt][1] = b; d->triangles[nt][2] = c; }
					nt++;
				}
				a = b; b = c;
			}
		}
		d->numTriangles = (uint16)nt;
	}
	(void)declared;
	free(len);
	return d;
}

static void *
LireSourceTexture(NifReader &r)
{
	NifSourceTexture *t = (NifSourceTexture*)calloc(1, sizeof(NifSourceTexture));
	t->name = r.I32();
	int32 ne = r.I32(); free(r.Refs(ne));
	r.I32();                                        // contrôleur
	t->external = r.U8();
	// Deux mots dans les deux cas, le bloc fait la même taille. Quand la
	// texture est interne (les .nft), le second est la référence au
	// NiPixelData ; quand elle est externe (les NiSourceCubeMap des .nif,
	// 36 octets, qui nomment un .nft), il vaut -1.
	t->fileName = r.I32();
	t->pixelData = r.I32();
	t->pixelLayout = r.U32(); t->useMipmaps = r.U32(); t->alphaFormat = r.U32();
	t->isStatic = r.U8(); t->directRender = r.U8(); t->persistRenderData = r.U8();
	return t;
}

// NiPixelData. Taille = 79 + 12 × mipmaps + numPixels × numFaces, vérifiée
// à l'octet près sur 35 642 blocs (voir docs/nft.md).
static void *
LirePixelData(NifReader &r)
{
	NifPixelData *d = (NifPixelData*)calloc(1, sizeof(NifPixelData));
	d->pixelFormat = r.U32();
	d->bitsPerPixel = r.U8();
	d->rendererHint = r.I32();
	d->extraDataValue = r.U32();
	d->flags = r.U8();
	d->tiling = r.U32();
	d->srgb = r.U8();
	for(int k = 0; k < 4; k++){
		d->channels[k].type = r.U32();
		d->channels[k].convention = r.U32();
		d->channels[k].bitsPerChannel = r.U8();
		d->channels[k].isSigned = r.U8();
	}
	d->palette = r.I32();
	d->numMipmaps = r.U32();
	d->bytesPerPixel = r.U32();
	if(d->numMipmaps > 32){ r.ok = false; return d; }
	if(d->numMipmaps){
		d->mipmaps = (NifMipmap*)malloc(d->numMipmaps * sizeof(NifMipmap));
		for(uint32 k = 0; k < d->numMipmaps; k++){
			d->mipmaps[k].width = r.U32();
			d->mipmaps[k].height = r.U32();
			d->mipmaps[k].offset = r.U32();
		}
	}
	d->numPixels = r.U32();
	d->numFaces = r.U32();
	uint32 total = d->numPixels * d->numFaces;
	if(!r.Need(total)) return d;
	d->pixels = r.p;                // pas de copie : on pointe dans le tampon
	r.p += total;
	return d;
}

// NiPalette : 1 + 4 + 4 × numEntries octets.
static void *
LirePalette(NifReader &r)
{
	NifPalette *p = (NifPalette*)calloc(1, sizeof(NifPalette));
	p->hasAlpha = r.U8();
	p->numEntries = r.U32();
	if(p->numEntries > 65536 || !r.Need(p->numEntries * 4)){ r.ok = false; return p; }
	p->entries = r.p;
	r.p += p->numEntries * 4;
	return p;
}

// NiStringExtraData et NiIntegerExtraData : nom puis valeur, huit octets.
static void *
LireExtraData(NifReader &r)
{
	NifExtraData *e = (NifExtraData*)calloc(1, sizeof(NifExtraData));
	e->name = r.I32();
	e->value = r.I32();
	return e;
}

static void *
LireMaterial(NifReader &r)
{
	NifMaterialProperty *m = (NifMaterialProperty*)calloc(1, sizeof(NifMaterialProperty));
	m->name = r.I32();
	int32 ne = r.I32(); free(r.Refs(ne));
	r.I32();
	for(int k = 0; k < 3; k++) m->ambient[k] = r.F32();
	for(int k = 0; k < 3; k++) m->diffuse[k] = r.F32();
	for(int k = 0; k < 3; k++) m->specular[k] = r.F32();
	for(int k = 0; k < 3; k++) m->emissive[k] = r.F32();
	m->glossiness = r.F32(); m->alpha = r.F32();
	return m;
}

// NiTexturingProperty en 20.3.0.9 : drapeaux u16, nombre d'emplacements
// u32, puis pour chaque emplacement un booléen « présent » suivi d'un
// TexDesc { source i32, drapeaux u16 (jeu d'UV dans les bits bas, filtrage,
// bornage), transformation u8 (+ 32 octets si vraie) }. L'emplacement 5
// (relief) porte en plus 6 flottants, le 7 (parallaxe) un flottant. Après
// les emplacements : nombre de textures de shader u32 (+ 4+1 octets chacune).
// Seule la texture de base est retenue.
static void
LireTexDesc(NifReader &r, int32 *source, uint16 *flags, uint16 *uvSet)
{
	int32 s = r.I32(); uint16 f = r.U16();
	if(r.U8()){ for(int k = 0; k < 5; k++) r.F32(); r.U32(); r.F32(); r.F32(); }   // translation (2), échelle (2), rotation, méthode, centre (2)
	if(source){ *source = s; *flags = f; *uvSet = f & 0xff; }
}

static void *
LireTexturing(NifReader &r)
{
	NifTexturingProperty *t = (NifTexturingProperty*)calloc(1, sizeof(NifTexturingProperty));
	t->name = r.I32();
	int32 ne = r.I32(); free(r.Refs(ne));
	r.I32();
	t->flags = r.U16();
	t->textureCount = r.U32();
	t->baseTexture = -1;
	for(uint32 i = 0; i < t->textureCount && i < 12 && r.ok; i++){
		if(r.U8()){
			if(i == 0) LireTexDesc(r, &t->baseTexture, &t->baseFlags, &t->baseUVSet);
			else LireTexDesc(r, nil, nil, nil);
			if(i == 5){ for(int k = 0; k < 6; k++) r.F32(); }   // relief : luminance, décalage, matrice 2×2
			if(i == 7) r.F32();                                   // parallaxe : décalage
		}
	}
	uint32 ns = r.U32();                                          // textures de shader
	for(uint32 i = 0; i < ns && i < 32 && r.ok; i++){ if(r.U8()){ LireTexDesc(r, nil, nil, nil); r.U32(); } }
	return t;
}

bool
CNifFile::Load(const uint8 *data, uint32 size)
{
	memset(this, 0, sizeof(*this));
	m_data = data; m_size = size;
	const uint8 *nl = (const uint8*)memchr(data, '\n', size < 64 ? size : 64);
	if(nl == nil || memcmp(data, "Gamebryo File Format", 20) != 0) return false;
	NifReader r(nl + 1, size - (uint32)(nl + 1 - data));
	version = r.U32(); endian = r.U8(); userVersion = r.U32();
	numBlocks = r.I32();
	// 247 fichiers (cinématiques CS_*) sont grand-boutistes : le boutisme
	// s'applique à partir du nombre de types, le nombre de blocs reste petit-boutiste
	r.be = endian == 0;
	numTypes = r.U16();
	if(!r.ok || numBlocks < 0 || numBlocks > 200000 || numTypes < 0 || numTypes > 1000) return false;
	types = (char**)calloc(numTypes, sizeof(char*));
	for(int32 i = 0; i < numTypes; i++){
		uint32 l = r.U32(); if(!r.Need(l)) return false;
		types[i] = (char*)malloc(l + 1); memcpy(types[i], r.p, l); types[i][l] = '\0'; r.p += l;
	}
	blocks = (NifBlock*)calloc(numBlocks, sizeof(NifBlock));
	for(int32 i = 0; i < numBlocks; i++){ blocks[i].typeIndex = r.U16(); if(blocks[i].typeIndex >= numTypes) return false; }
	for(int32 i = 0; i < numBlocks; i++) blocks[i].size = r.U32();
	numStrings = r.I32(); r.U32();
	if(!r.ok || numStrings < 0 || numStrings > 200000) return false;
	strings = (char**)calloc(numStrings, sizeof(char*));
	for(int32 i = 0; i < numStrings; i++){
		uint32 l = r.U32(); if(!r.Need(l)) return false;
		strings[i] = (char*)malloc(l + 1); memcpy(strings[i], r.p, l); strings[i][l] = '\0'; r.p += l;
	}
	numGroups = r.I32();
	for(int32 i = 0; i < numGroups && r.ok; i++) r.U32();
	if(!r.ok) return false;
	uint32 off = (uint32)(r.p - data);
	for(int32 i = 0; i < numBlocks; i++){
		NifBlock &b = blocks[i];
		b.offset = off; off += b.size;
		if(off > size) return false;
		b.kind = KindOf(types[b.typeIndex]);
		NifReader br(data + b.offset, b.size, endian == 0);
		switch(b.kind){
		case NIF_NODE: b.data = LireNode(br); break;
		case NIF_TRISHAPE: case NIF_TRISTRIPS: b.data = LireGeometry(br); break;
		case NIF_TRISHAPEDATA: b.data = LireTriShapeData(br); break;
		case NIF_TRISTRIPSDATA: b.data = LireTriStripsData(br); break;
		case NIF_SOURCETEXTURE: b.data = LireSourceTexture(br); break;
		case NIF_PIXELDATA: b.data = LirePixelData(br); break;
		case NIF_PALETTE: b.data = LirePalette(br); break;
		case NIF_STRINGEXTRADATA:
		case NIF_INTEGEREXTRADATA: b.data = LireExtraData(br); break;
		case NIF_MATERIALPROPERTY: b.data = LireMaterial(br); break;
		case NIF_TEXTURINGPROPERTY: b.data = LireTexturing(br); break;
		default: break;
		}
		if(b.kind != NIF_INCONNU){
			if(br.Done()) numDecoded++;
			else { b.kind = NIF_INCONNU; }          // taille incohérente : on garde le bloc mais pas ses données
		}
	}
	return true;
}

void
CNifFile::Free(void)
{
	if(types) for(int32 i = 0; i < numTypes; i++) free(types[i]);
	if(strings) for(int32 i = 0; i < numStrings; i++) free(strings[i]);
	for(int32 i = 0; blocks && i < numBlocks; i++){
		NifBlock &b = blocks[i];
		if(b.data == nil) continue;
		switch(b.kind){
		case NIF_NODE: { NifNode *n = (NifNode*)b.data; free(n->extra); free(n->properties); free(n->children); free(n->effects); break; }
		case NIF_TRISHAPE: case NIF_TRISTRIPS: { NifGeometry *g = (NifGeometry*)b.data; free(g->extra); free(g->properties); break; }
		case NIF_TRISHAPEDATA: case NIF_TRISTRIPSDATA: { NifGeometryData *d = (NifGeometryData*)b.data; free(d->vertices); free(d->normals); free(d->colors); free(d->uv); free(d->triangles); break; }
		// Les pixels et les entrées de palette pointent dans le tampon source, rien à libérer.
		case NIF_PIXELDATA: free(((NifPixelData*)b.data)->mipmaps); break;
		default: break;
		}
		free(b.data);
	}
	free(types); free(strings); free(blocks);
	memset(this, 0, sizeof(*this));
}
