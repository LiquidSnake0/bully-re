#include "Agr.h"
#include <cmath>
#include <cstring>

static const int32 kTailleCle[6] = { 32, 20, 12, 8, 20, 12 };   // types 999 à 1004 (0x6b2ca0)

static uint32 Lire32(const uint8 *p){ uint32 v; memcpy(&v, p, 4); return v; }

static void
DecoderCle(uint32 w0, uint32 w1, float q[4])
{
	const float e = 1.0f / 1023;
	q[1] = ((w0 >> 21) & 0x3ff) * e * ((w0 >> 20 & 1) ? -1.0f : 1.0f);
	q[2] = (w1 & 0x3ff) * e * ((w0 >> 31 & 1) ? -1.0f : 1.0f);
	q[3] = ((w1 >> 11) & 0x3ff) * e * ((w1 >> 10 & 1) ? -1.0f : 1.0f);
	q[0] = (w1 >> 22) * e * ((w1 >> 21 & 1) ? -1.0f : 1.0f);
}

static bool
EnTete(const uint8 *p, const uint8 *fin)
{
	if(p + 20 > fin) return false;
	uint32 type = Lire32(p + 4), n = Lire32(p + 8);
	float d; memcpy(&d, p + 16, 4);
	return Lire32(p) == 0x100 && type >= 999 && type <= 1004 && Lire32(p + 12) == 0 && n > 0 && n < 20000 && d > 0 && d < 1000;
}

bool
AgrLireGroupe(const uint8 *buf, uint32 taille, std::vector<AgrAnim> &out)
{
	const uint8 *p = buf, *fin = buf + taille;
	while(fin > buf && fin[-1] == 0) fin--;                  // les secteurs sont complétés de zéros
	while(p + 20 <= fin && EnTete(p, fin)){
		AgrAnim a;
		a.type = (int32)Lire32(p + 4); a.numCles = (int32)Lire32(p + 8); memcpy(&a.duree, p + 16, 4);
		const uint8 *cles = p + 20;
		const uint8 *apres = cles + (size_t)a.numCles * kTailleCle[a.type - 999];
		if(apres > fin) return false;
		// La fin de la section de positions : l'en-tête suivant, cherché de 4 en 4.
		const uint8 *suivant = apres;
		while(suivant + 20 <= fin && !EnTete(suivant, fin)) suivant += 4;
		if(suivant + 20 > fin) suivant = fin;
		if(a.type == 1002 && a.numCles >= AGR_OS){
			std::vector<int32> os(a.numCles, -1);
			std::vector<float> t(a.numCles);
			std::vector<std::array<float, 4>> q(a.numCles);
			for(int32 i = 0; i < a.numCles; i++){
				uint32 w0 = Lire32(cles + 8 * i), w1 = Lire32(cles + 8 * i + 4);
				t[i] = ((w0 >> 11) & 0x1ff) / 511.0f * a.duree;
				DecoderCle(w0, w1, q[i].data());
			}
			// Chaque clé désigne la précédente du même os ; les 36 premières sont
			// les têtes. Les clés sont rangées dans l'ordre du temps : un seul
			// passage suffit pour propager l'os.
			for(int32 i = 0; i < AGR_OS; i++) os[i] = i;
			bool ok = true;
			for(int32 i = AGR_OS; i < a.numCles; i++){
				uint32 prec = Lire32(cles + 8 * i) & 0x7ff;
				if((int32)prec >= i || os[prec] < 0){ ok = false; break; }
				os[i] = os[prec];
			}
			if(ok){
				for(int32 i = 0; i < a.numCles; i++){
					// Une clé nulle (norme 0, sur les bras de 63 animations) veut dire
					// « pas de donnée pour cet os à cet instant » : on la saute.
					float n2 = q[i][0]*q[i][0] + q[i][1]*q[i][1] + q[i][2]*q[i][2] + q[i][3]*q[i][3];
					if(n2 < 0.25f) continue;
					AgrCle c; c.t = t[i]; memcpy(c.q, q[i].data(), sizeof c.q);
					a.pistes[os[i]].push_back(c);
				}
				for(int32 k = 0; k < AGR_OS; k++)
					for(size_t j = 1; j < a.pistes[k].size(); j++) if(a.pistes[k][j].t < a.pistes[k][j-1].t) ok = false;
				a.decodee = ok;
				// Positions : index d'image clé → instant de cette clé.
				for(const uint8 *r = apres; r + 8 <= suivant; r += 8){
					uint16 k; int16 x, y, z;
					memcpy(&k, r, 2); memcpy(&x, r + 2, 2); memcpy(&y, r + 4, 2); memcpy(&z, r + 6, 2);
					if(k >= a.numCles) break;
					AgrPosition ps; ps.t = t[k]; ps.p = CVector(x / 1000.0f, y / 1000.0f, z / 1000.0f);
					a.positions.push_back(ps);
				}
			}
		}
		out.push_back(a);
		p = suivant;
	}
	return !out.empty();
}

static void
Slerp(const float a[4], const float b0[4], float u, float out[4])
{
	float b[4] = { b0[0], b0[1], b0[2], b0[3] };
	float c = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
	if(c < 0){ for(float &x : b) x = -x; c = -c; }
	float ka, kb;
	if(c > 0.9995f){ ka = 1 - u; kb = u; }
	else { float th = acosf(c), s = sinf(th); ka = sinf((1 - u) * th) / s; kb = sinf(u * th) / s; }
	float n = 0;
	for(int i = 0; i < 4; i++){ out[i] = ka * a[i] + kb * b[i]; n += out[i] * out[i]; }
	n = sqrtf(n); if(n > 1e-6f) for(int i = 0; i < 4; i++) out[i] /= n;
}

bool
AgrRotation(const AgrAnim &a, int32 os, float t, float q[4])
{
	if(!a.decodee || os < 0 || os >= AGR_OS || a.pistes[os].empty()) return false;
	const std::vector<AgrCle> &p = a.pistes[os];
	if(a.duree > 0){ t = fmodf(t, a.duree); if(t < 0) t += a.duree; }
	if(t <= p.front().t){ memcpy(q, p.front().q, 16); return true; }
	if(t >= p.back().t){ memcpy(q, p.back().q, 16); return true; }
	size_t j = 1; while(j < p.size() && p[j].t < t) j++;
	float dt = p[j].t - p[j-1].t;
	Slerp(p[j-1].q, p[j].q, dt > 1e-6f ? (t - p[j-1].t) / dt : 0, q);
	return true;
}

void
AgrMatrice(const float q[4], float m[3][3])
{
	float w = q[0], x = q[1], y = q[2], z = q[3];
	m[0][0] = 1 - 2*(y*y + z*z); m[0][1] = 2*(x*y - z*w);     m[0][2] = 2*(x*z + y*w);
	m[1][0] = 2*(x*y + z*w);     m[1][1] = 1 - 2*(x*x + z*z); m[1][2] = 2*(y*z - x*w);
	m[2][0] = 2*(x*z - y*w);     m[2][1] = 2*(y*z + x*w);     m[2][2] = 1 - 2*(x*x + y*y);
}
