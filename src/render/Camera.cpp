#include "Camera.h"
#include <cmath>

CVector
Camera::Forward(void) const
{
	return CVector(cosf(pitch) * cosf(yaw), cosf(pitch) * sinf(yaw), sinf(pitch));
}

CVector
Camera::Right(void) const
{
	return CVector(sinf(yaw), -cosf(yaw), 0.0f);
}

CVector
Camera::Up(void) const
{
	// Right × Forward, pour un repère direct avec Z du monde vers le haut.
	CVector r = Right(), f = Forward();
	return CVector(r.y * f.z - r.z * f.y, r.z * f.x - r.x * f.z, r.x * f.y - r.y * f.x);
}

CVector
Camera::ToView(const CVector &p) const
{
	CVector d(p.x - pos.x, p.y - pos.y, p.z - pos.z);
	CVector r = Right(), u = Up(), f = Forward();
	return CVector(d.x * r.x + d.y * r.y + d.z * r.z,
	               d.x * u.x + d.y * u.y + d.z * u.z,
	               d.x * f.x + d.y * f.y + d.z * f.z);
}

RasterPVertex
Camera::Project(const CVector &v, float u, float vv) const
{
	float foc = (h * 0.5f) / tanf(fovY * 0.5f);
	RasterPVertex o;
	o.invW = 1.0f / v.z;
	o.x = w * 0.5f + foc * v.x * o.invW;
	o.y = h * 0.5f - foc * v.y * o.invW;
	o.uw = u * o.invW; o.vw = vv * o.invW;
	return o;
}

void
Camera::Move(float avant, float droite, float haut)
{
	pos.x += cosf(yaw) * avant + sinf(yaw) * droite;
	pos.y += sinf(yaw) * avant - cosf(yaw) * droite;
	pos.z += haut;
}

int32
CameraClipProject(const Camera &cam, const CVector view[3], const float uv[6], RasterPVertex out[6])
{
	// Sutherland-Hodgman sur un seul plan : un triangle en donne au plus un
	// quadrilatère, découpé ensuite en éventail.
	CVector p[4]; float t[8]; int32 n = 0;
	const float zn = cam.near_;
	for(int32 i = 0; i < 3; i++){
		int32 j = (i + 1) % 3;
		const CVector &a = view[i], &b = view[j];
		bool ina = a.z >= zn, inb = b.z >= zn;
		if(ina){ p[n] = a; t[n*2] = uv[i*2]; t[n*2+1] = uv[i*2+1]; n++; }
		if(ina != inb){
			float k = (zn - a.z) / (b.z - a.z);
			p[n] = CVector(a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k, zn);
			t[n*2]   = uv[i*2]   + (uv[j*2]   - uv[i*2])   * k;
			t[n*2+1] = uv[i*2+1] + (uv[j*2+1] - uv[i*2+1]) * k;
			n++;
		}
	}
	if(n < 3) return 0;
	RasterPVertex s[4];
	for(int32 i = 0; i < n; i++) s[i] = cam.Project(p[i], t[i*2], t[i*2+1]);
	out[0] = s[0]; out[1] = s[1]; out[2] = s[2];
	if(n == 3) return 1;
	out[3] = s[0]; out[4] = s[2]; out[5] = s[3];
	return 2;
}
