// Découpeur de ligne propre à Bully, utilisé par les chargeurs récents
// (object.dat…). bully.exe : Init 0x61a310, Next 0x61a370, Copy 0x61a250,
// AsLong 0x61a430, AsFloat 0x61a460. Huit délimiteurs au plus, copiés dans
// l'objet ; object.dat en passe quatre : espace, tabulation, CR, LF (0x913710).
#pragma once
#include "../common.h"

class CTokenizer
{
public:
	const char *m_cur;       // +0x00 : début du jeton courant
	char m_delims[8];        // +0x04
	const char *m_start;     // +0x0c
	int32 m_unused;          // +0x10

	void Init(const char *text, const char *delims);       // 0x61a310 : saute les délimiteurs de tête
	void Next(void);                                       // 0x61a370 : passe au jeton suivant
	char *Copy(char *dest, uint32 size, char stop, bool useDelims);   // 0x61a250 : copie le jeton courant
	int32 AsLong(void);                                    // 0x61a430
	float AsFloat(void);                                   // 0x61a460
	bool AtEnd(void) const { return *m_cur == '\0'; }

	bool IsDelim(char c) const;
};
