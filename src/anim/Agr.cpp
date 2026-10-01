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

static uint16 Lire16(const uint8 *p){ uint16 v; memcpy(&v, p, 2); return v; }
static int16 LireI16(const uint8 *p){ int16 v; memcpy(&v, p, 2); return v; }
static float LireF(const uint8 *p){ float v; memcpy(&v, p, 4); return v; }

// Une image clé de n'importe quel type : index de la précédente du même os,
// instant en fraction de la durée, quaternion (w, x, y, z), et translation pour
// les types qui la portent. Décodeurs de bully.exe : 1002 0x6b1710, 1004 idem +
// 0x6b1870, 1001 0x6b4970, 1003 idem + 0x6b1830, 1000 et 999 0x6b5440.
static void
DecoderCleType(int32 type, const uint8 *k, uint32 *prec, float *t, float q[4], CVector *tr)
{
	if(type == 1002 || type == 1004){
		uint32 w0 = Lire32(k), w1 = Lire32(k + 4);
		*prec = w0 & 0x7ff;
		*t = ((w0 >> 11) & 0x1ff) / 511.0f;
		DecoderCle(w0, w1, q);
		if(tr){
			// 3 composantes signées en centimètres : x 10 bits (signe bit 10),
			// y 10 bits (signe bit 21), z 9 bits (signe bit 31). Échelle 0,01.
			uint32 v = Lire32(k + 8);
			tr->x = (v & 0x3ff) * 0.01f * ((v >> 10 & 1) ? -1.0f : 1.0f);
			tr->y = ((v >> 11) & 0x3ff) * 0.01f * ((v >> 21 & 1) ? -1.0f : 1.0f);
			tr->z = ((v >> 22) & 0x1ff) * 0.01f * ((v >> 31 & 1) ? -1.0f : 1.0f);
		}
		return;
	}
	*prec = Lire16(k);
	*t = Lire16(k + 2) / 65535.0f;                            // constante 0x941f28
	if(type == 1001 || type == 1003){
		// Quatre i16 en 32767e, rangés x, y, z, w (w à +10).
		const float e = 1.0f / 32767;
		q[1] = LireI16(k + 4) * e; q[2] = LireI16(k + 6) * e; q[3] = LireI16(k + 8) * e; q[0] = LireI16(k + 10) * e;
		if(tr) *tr = CVector(LireI16(k + 12) * 0.001f, LireI16(k + 14) * 0.001f, LireI16(k + 16) * 0.001f);   // mm, +18 bourrage
	}else{
		// 1000 et 999 : quatre floats rangés w, x, y, z ; 999 ajoute trois floats en mètres.
		q[0] = LireF(k + 4); q[1] = LireF(k + 8); q[2] = LireF(k + 12); q[3] = LireF(k + 16);
		if(tr) *tr = CVector(LireF(k + 20), LireF(k + 24), LireF(k + 28));
	}
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
	const uint8 *p = buf, *finReelle = buf + taille, *fin = finReelle;
	// Les secteurs sont complétés de zéros. On les retire pour trouver la fin
	// des positions, mais pas pour les images clés : une clé 1003 finit par
	// deux octets de bourrage nuls, souvent précédés d'une translation nulle.
	while(fin > buf && fin[-1] == 0) fin--;
	while(p + 20 <= fin && EnTete(p, fin)){
		AgrAnim a;
		a.type = (int32)Lire32(p + 4); a.numCles = (int32)Lire32(p + 8); memcpy(&a.duree, p + 16, 4);
		const uint8 *cles = p + 20;
		const uint8 *apres = cles + (size_t)a.numCles * kTailleCle[a.type - 999];
		if(apres > finReelle) return false;
		// La fin de la section de positions : l'en-tête suivant, cherché de 4 en 4.
		const uint8 *suivant = apres;
		while(suivant + 20 <= fin && !EnTete(suivant, fin)) suivant += 4;
		if(suivant + 20 > fin) suivant = fin > apres ? fin : apres;
		if(a.numCles > 0){
			const int32 tc = kTailleCle[a.type - 999];
			std::vector<int32> os(a.numCles, -1);
			std::vector<float> t(a.numCles);
			std::vector<std::array<float, 4>> q(a.numCles);
			std::vector<uint32> prec(a.numCles);
			std::vector<CVector> tr(a.numCles);
			const bool translation = a.type == 999 || a.type == 1003 || a.type == 1004;
			for(int32 i = 0; i < a.numCles; i++){
				const uint8 *k = cles + (size_t)tc * i;
				DecoderCleType(a.type, k, &prec[i], &t[i], q[i].data(), translation ? &tr[i] : nullptr);
				t[i] *= a.duree;
			}
			// Les têtes de piste ouvrent l'animation : précédente 0 et instant nul.
			// « Précédente 0 » veut aussi dire « la clé 0 » : la deuxième clé de
			// l'os 0 a le même champ, mais un instant non nul. 36 os pour un
			// piéton, de 1 à 33 pour les objets (2 pour le ballon, 15 pour le vélo).
			int32 tetes = 0;
			while(tetes < a.numCles && tetes < AGR_OS && prec[tetes] == 0 && t[tetes] == 0) tetes++;
			a.numOs = tetes;
			// Chaque autre clé désigne la précédente du même os, dans l'ordre du
			// temps : un seul passage suffit pour propager l'os.
			for(int32 i = 0; i < tetes; i++) os[i] = i;
			bool ok = tetes > 0;
			for(int32 i = tetes; i < a.numCles && ok; i++){
				if((int32)prec[i] >= i || os[prec[i]] < 0){ ok = false; break; }
				os[i] = os[prec[i]];
			}
			if(ok){
				for(int32 i = 0; i < a.numCles; i++){
					// Une clé nulle (norme 0, sur les bras de 63 animations 1002) veut
					// dire « pas de donnée pour cet os à cet instant » : on la saute.
					float n2 = q[i][0]*q[i][0] + q[i][1]*q[i][1] + q[i][2]*q[i][2] + q[i][3]*q[i][3];
					if(n2 < 0.25f) continue;
					AgrCle c; c.t = t[i]; memcpy(c.q, q[i].data(), sizeof c.q);
					a.pistes[os[i]].push_back(c);
					if(translation){ AgrPosition ps; ps.t = t[i]; ps.p = tr[i]; a.positions[os[i]].push_back(ps); }
				}
				for(int32 k = 0; k < AGR_OS; k++)
					for(size_t j = 1; j < a.pistes[k].size(); j++) if(a.pistes[k][j].t < a.pistes[k][j-1].t) ok = false;
				a.decodee = ok;
				// Positions à part (types sans translation dans la clé) : index
				// d'image clé → instant et os de cette clé.
				if(!translation)
				for(const uint8 *r = apres; r + 8 <= suivant; r += 8){
					uint16 k; int16 x, y, z;
					memcpy(&k, r, 2); memcpy(&x, r + 2, 2); memcpy(&y, r + 4, 2); memcpy(&z, r + 6, 2);
					if(k >= a.numCles) break;
					AgrPosition ps; ps.t = t[k]; ps.p = CVector(x / 1000.0f, y / 1000.0f, z / 1000.0f);
					a.positions[os[k]].push_back(ps);
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

bool
AgrPositionOs(const AgrAnim &a, int32 os, float t, CVector *p)
{
	if(!a.decodee || os < 0 || os >= AGR_OS || a.positions[os].empty()) return false;
	const std::vector<AgrPosition> &v = a.positions[os];
	if(a.duree > 0){ t = fmodf(t, a.duree); if(t < 0) t += a.duree; }
	if(t <= v.front().t){ *p = v.front().p; return true; }
	if(t >= v.back().t){ *p = v.back().p; return true; }
	size_t j = 1; while(j < v.size() && v[j].t < t) j++;
	float dt = v[j].t - v[j-1].t, u = dt > 1e-6f ? (t - v[j-1].t) / dt : 0;
	p->x = v[j-1].p.x + u * (v[j].p.x - v[j-1].p.x);
	p->y = v[j-1].p.y + u * (v[j].p.y - v[j-1].p.y);
	p->z = v[j-1].p.z + u * (v[j].p.z - v[j-1].p.z);
	return true;
}

CVector
AgrDeplacement(const AgrAnim &a)
{
	const std::vector<AgrPosition> &v = a.positions[AGR_OS - 1];
	if(!a.decodee || v.size() < 2) return CVector(0, 0, 0);
	return CVector(v.back().p.x - v.front().p.x, v.back().p.y - v.front().p.y, v.back().p.z - v.front().p.z);
}

void
AgrMatrice(const float q[4], float m[3][3])
{
	float w = q[0], x = q[1], y = q[2], z = q[3];
	m[0][0] = 1 - 2*(y*y + z*z); m[0][1] = 2*(x*y - z*w);     m[0][2] = 2*(x*z + y*w);
	m[1][0] = 2*(x*y + z*w);     m[1][1] = 1 - 2*(x*x + z*z); m[1][2] = 2*(y*z - x*w);
	m[2][0] = 2*(x*z - y*w);     m[2][1] = 2*(y*z + x*w);     m[2][2] = 1 - 2*(x*x + y*y);
}
