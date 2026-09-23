// Types de base, dans l'esprit de re3. Rien ici ne vient du binaire.
#pragma once
#include <cstdint>
#include <cstddef>

typedef int8_t   int8;
typedef uint8_t  uint8;
typedef int16_t  int16;
typedef uint16_t uint16;
typedef int32_t  int32;
typedef uint32_t uint32;
typedef bool     bool8;

#define nil nullptr

struct CVector {
	float x, y, z;
	CVector(void) : x(0.0f), y(0.0f), z(0.0f) {}
	CVector(float x, float y, float z) : x(x), y(y), z(z) {}
};

// Même disposition que reVC (core/Rect.h) et que bully.exe (CPhysical::
// GetBoundRect 0x468fe0 écrit [0] = x − r, [1] = y + r, [2] = x + r,
// [3] = y − r) : « bottom » est le y maximal et « top » le y minimal.
struct CRect {
	float left;     // x min
	float bottom;   // y max
	float right;    // x max
	float top;      // y min
	CRect(void) : left(0.0f), bottom(0.0f), right(0.0f), top(0.0f) {}
	CRect(float l, float t, float r, float b) : left(l), bottom(b), right(r), top(t) {}
};

// Objet graphique Gamebryo (NiAVObject / NiNode). Recréé plus tard ; pour
// l'instant un type opaque dont on ne lit que le premier octet (le type,
// comme rwOBJECTTYPE dans RenderWare).
struct RwObject { uint8 type; };

inline float sq(float x) { return x*x; }
#define PI 3.14159265358979f
#define DEGTORAD(x) ((x) * PI / 180.0f)
#define RADTODEG(x) ((x) * 180.0f / PI)
