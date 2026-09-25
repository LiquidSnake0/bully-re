#include "SoftRaster.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

RasterImage
RasterCreate(int32 w, int32 h, uint8 r, uint8 g, uint8 b)
{
	RasterImage img;
	img.w = w; img.h = h;
	img.rgb = (uint8*)malloc((size_t)w * h * 3);
	img.depth = (float*)malloc((size_t)w * h * sizeof(float));
	for(int32 i = 0; i < w * h; i++){
		img.rgb[i*3] = r; img.rgb[i*3+1] = g; img.rgb[i*3+2] = b;
		img.depth[i] = 1e30f;
	}
	return img;
}

void
RasterFree(RasterImage &img)
{
	free(img.rgb); free(img.depth);
	img.rgb = nil; img.depth = nil;
}

static inline float
Aire(const RasterVertex &a, const RasterVertex &b, const RasterVertex &c)
{
	return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

void
RasterTriangle(RasterImage &img, const RasterVertex v[3], const RasterTexture *tex, float shade)
{
	float aire = Aire(v[0], v[1], v[2]);
	if(fabsf(aire) < 1e-6f) return;
	int32 x0 = (int32)floorf(fminf(fminf(v[0].x, v[1].x), v[2].x)), x1 = (int32)ceilf(fmaxf(fmaxf(v[0].x, v[1].x), v[2].x));
	int32 y0 = (int32)floorf(fminf(fminf(v[0].y, v[1].y), v[2].y)), y1 = (int32)ceilf(fmaxf(fmaxf(v[0].y, v[1].y), v[2].y));
	if(x0 < 0) x0 = 0; if(y0 < 0) y0 = 0;
	if(x1 >= img.w) x1 = img.w - 1; if(y1 >= img.h) y1 = img.h - 1;
	float inv = 1.0f / aire;
	for(int32 y = y0; y <= y1; y++)
		for(int32 x = x0; x <= x1; x++){
			RasterVertex p; p.x = x + 0.5f; p.y = y + 0.5f;
			// coordonnées barycentriques, même signe que l'aire = à l'intérieur
			float w0 = Aire(v[1], v[2], p) * inv, w1 = Aire(v[2], v[0], p) * inv, w2 = Aire(v[0], v[1], p) * inv;
			if(w0 < 0 || w1 < 0 || w2 < 0) continue;
			float z = w0 * v[0].z + w1 * v[1].z + w2 * v[2].z;
			int32 i = y * img.w + x;
			if(z >= img.depth[i]) continue;
			img.depth[i] = z;
			float cr = 200, cg = 200, cb = 205;
			if(tex && tex->rgba && tex->w && tex->h){
				float u = w0 * v[0].u + w1 * v[1].u + w2 * v[2].u;
				float vv = w0 * v[0].v + w1 * v[1].v + w2 * v[2].v;
				u -= floorf(u); vv -= floorf(vv);               // répétition
				uint32 tx = (uint32)(u * (tex->w - 1) + 0.5f), ty = (uint32)(vv * (tex->h - 1) + 0.5f);
				const uint8 *t = tex->rgba + (ty * tex->w + tx) * 4;
				cr = t[0]; cg = t[1]; cb = t[2];
			}
			img.rgb[i*3]   = (uint8)fminf(255.0f, cr * shade);
			img.rgb[i*3+1] = (uint8)fminf(255.0f, cg * shade);
			img.rgb[i*3+2] = (uint8)fminf(255.0f, cb * shade);
		}
}

bool
RasterWritePPM(const RasterImage &img, const char *path)
{
	FILE *f = fopen(path, "wb");
	if(f == nil) return false;
	fprintf(f, "P6\n%d %d\n255\n", img.w, img.h);
	fwrite(img.rgb, 1, (size_t)img.w * img.h * 3, f);
	fclose(f);
	return true;
}
