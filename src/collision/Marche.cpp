#include "Marche.h"
#include <cmath>

static inline CVector Sub(const CVector &a, const CVector &b){ return CVector(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline CVector Add(const CVector &a, const CVector &b){ return CVector(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline CVector Mul(const CVector &a, float k){ return CVector(a.x * k, a.y * k, a.z * k); }
static inline float Dot(const CVector &a, const CVector &b){ return a.x * b.x + a.y * b.y + a.z * b.z; }

void
CMondeCollision::AjouterTriangle(const CVector &a, const CVector &b, const CVector &c)
{
	tri.push_back(a); tri.push_back(b); tri.push_back(c);
}

void
CMondeCollision::AjouterBoite(const CVector k[8])
{
	// faces : -x, +x, -y, +y, -z, +z (indices = bits x | y<<1 | z<<2)
	static const int f[6][4] = { {0,2,6,4}, {1,5,7,3}, {0,4,5,1}, {2,3,7,6}, {0,1,3,2}, {4,6,7,5} };
	for(int i = 0; i < 6; i++){
		AjouterTriangle(k[f[i][0]], k[f[i][1]], k[f[i][2]]);
		AjouterTriangle(k[f[i][0]], k[f[i][2]], k[f[i][3]]);
	}
}

void
CMondeCollision::AjouterSphere(const CVector &c, float r)
{
	sphCentre.push_back(c); sphRayon.push_back(r);
}

bool
CMondeCollision::Sol(float x, float y, float zHaut, float profondeur, float *z) const
{
	bool trouve = false; float meilleur = zHaut - profondeur;
	for(size_t i = 0; i < tri.size(); i += 3){
		const CVector &a = tri[i], &b = tri[i+1], &c = tri[i+2];
		// Projection sur le plan horizontal : (x, y) dans le triangle ?
		float d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
		if(fabsf(d) < 1e-9f) continue;                   // triangle vertical
		float l0 = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / d;
		float l1 = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / d;
		float l2 = 1.0f - l0 - l1;
		if(l0 < 0 || l1 < 0 || l2 < 0) continue;
		float zt = l0 * a.z + l1 * b.z + l2 * c.z;
		if(zt <= zHaut && zt >= meilleur){ meilleur = zt; trouve = true; }
	}
	for(size_t i = 0; i < sphCentre.size(); i++){
		float dx = x - sphCentre[i].x, dy = y - sphCentre[i].y, r = sphRayon[i];
		float h2 = r * r - dx * dx - dy * dy;
		if(h2 < 0) continue;
		float zt = sphCentre[i].z + sqrtf(h2);
		if(zt <= zHaut && zt >= meilleur){ meilleur = zt; trouve = true; }
	}
	if(trouve) *z = meilleur;
	return trouve;
}

// Point du triangle le plus proche de p (Ericson, Real-Time Collision Detection, 5.1.5).
static CVector
PlusProche(const CVector &p, const CVector &a, const CVector &b, const CVector &c)
{
	CVector ab = Sub(b, a), ac = Sub(c, a), ap = Sub(p, a);
	float d1 = Dot(ab, ap), d2 = Dot(ac, ap);
	if(d1 <= 0 && d2 <= 0) return a;
	CVector bp = Sub(p, b); float d3 = Dot(ab, bp), d4 = Dot(ac, bp);
	if(d3 >= 0 && d4 <= d3) return b;
	float vc = d1 * d4 - d3 * d2;
	if(vc <= 0 && d1 >= 0 && d3 <= 0) return Add(a, Mul(ab, d1 / (d1 - d3)));
	CVector cp = Sub(p, c); float d5 = Dot(ab, cp), d6 = Dot(ac, cp);
	if(d6 >= 0 && d5 <= d6) return c;
	float vb = d5 * d2 - d1 * d6;
	if(vb <= 0 && d2 >= 0 && d6 <= 0) return Add(a, Mul(ac, d2 / (d2 - d6)));
	float va = d3 * d6 - d5 * d4;
	if(va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) return Add(b, Mul(Sub(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
	float den = 1.0f / (va + vb + vc);
	return Add(a, Add(Mul(ab, vb * den), Mul(ac, vc * den)));
}

int32
CMondeCollision::Repousser(CVector &centre, float rayon) const
{
	int32 contacts = 0;
	for(size_t i = 0; i < tri.size(); i += 3){
		const CVector &a = tri[i], &b = tri[i+1], &c = tri[i+2];
		// rejet rapide par la boîte du triangle
		if(centre.x + rayon < fminf(fminf(a.x, b.x), c.x) || centre.x - rayon > fmaxf(fmaxf(a.x, b.x), c.x)) continue;
		if(centre.y + rayon < fminf(fminf(a.y, b.y), c.y) || centre.y - rayon > fmaxf(fmaxf(a.y, b.y), c.y)) continue;
		if(centre.z + rayon < fminf(fminf(a.z, b.z), c.z) || centre.z - rayon > fmaxf(fmaxf(a.z, b.z), c.z)) continue;
		CVector q = PlusProche(centre, a, b, c);
		float dx = centre.x - q.x, dy = centre.y - q.y, dz = centre.z - q.z;
		float d2 = dx * dx + dy * dy + dz * dz;
		if(d2 >= rayon * rayon) continue;
		float dh = sqrtf(dx * dx + dy * dy);
		if(dh < 1e-5f) continue;                         // contact par-dessus ou par-dessous : c'est le sol, pas un mur
		// On pousse à l'horizontale de ce qu'il faut pour sortir : la
		// distance horizontale doit devenir √(r² − dz²), dz ne changeant pas.
		float pousse = sqrtf(rayon * rayon - dz * dz) - dh;
		centre.x += dx / dh * pousse;
		centre.y += dy / dh * pousse;
		contacts++;
	}
	for(size_t i = 0; i < sphCentre.size(); i++){
		float dx = centre.x - sphCentre[i].x, dy = centre.y - sphCentre[i].y, dz = centre.z - sphCentre[i].z;
		float r = rayon + sphRayon[i], d2 = dx * dx + dy * dy + dz * dz;
		if(d2 >= r * r) continue;
		float dh = sqrtf(dx * dx + dy * dy);
		if(dh < 1e-5f) continue;
		float pousse = r - sqrtf(d2);
		centre.x += dx / dh * pousse; centre.y += dy / dh * pousse;
		contacts++;
	}
	return contacts;
}

void
CMarcheur::Avancer(const CMondeCollision &m, float dx, float dy, float dt)
{
	// Les murs : deux sphères le long du corps, au-dessus de la hauteur de
	// marche pour ne pas buter sur une marche qu'on doit monter.
	pos.x += dx; pos.y += dy;
	for(int passe = 0; passe < 3; passe++){
		int32 n = 0;
		for(float h : { marche + rayon + 0.05f, hauteurYeux - 0.2f }){
			CVector c(pos.x, pos.y, pos.z + h);
			n += m.Repousser(c, rayon);
			pos.x = c.x; pos.y = c.y;
		}
		if(n == 0) break;
	}
	// Le sol : cherché depuis la hauteur d'une marche au-dessus des pieds.
	float z;
	bool sol = m.Sol(pos.x, pos.y, pos.z + marche, 50.0f, &z);
	vz -= 9.81f * dt;
	float zSuivant = pos.z + vz * dt;
	if(sol && zSuivant <= z){ pos.z = z; vz = 0; auSol = true; }
	else { pos.z = zSuivant; auSol = false; }
}
