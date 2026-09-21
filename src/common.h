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

struct CRect {
	float left, bottom, right, top;   // même ordre que reVC : x1, y1, x2, y2
	CRect(void) : left(0.0f), bottom(0.0f), right(0.0f), top(0.0f) {}
	CRect(float l, float b, float r, float t) : left(l), bottom(b), right(r), top(t) {}
};

// Objet graphique Gamebryo (NiAVObject / NiNode). Recréé plus tard ; pour
// l'instant un type opaque dont on ne lit que le premier octet (le type,
// comme rwOBJECTTYPE dans RenderWare).
struct RwObject { uint8 type; };
