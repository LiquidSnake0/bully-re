#include "CameraMgr.h"

void
CCameraMgr::Demander(int32 mode, int32 duree, bool octet)
{
	demande.mode = mode;
	demande.duree = duree;
	demande.octet = octet;
}

void
CCameraMgr::DemanderSiAutre(int32 mode)
{
	if(courant != mode) Demander(mode, 0, true);
}

void
CCameraMgr::Appliquer(void)
{
	if(demande.mode != courant){
		precedent = courant;
		courant = demande.mode;
	}
	demande.mode = 0;
}

void
CCameraMgr::LargeEcran(bool oui, bool immediat)
{
	largeEcran = oui;
	transition = 0.0f;
	if(immediat) largeEcranAtteint = oui;
}

bool
CameraActive(const CCameraMgr &c, int32 mode)
{
	return c.courant == mode;
}

bool
CameraInWidescreen(const CCameraMgr &c)
{
	return c.largeEcran;
}
