#include "Tokenizer.h"
#include <cstdlib>
#include <cstring>

bool
CTokenizer::IsDelim(char c) const
{
	for(int i = 0; i < 8; i++)
		if(c == m_delims[i]) return true;
	return false;
}

// 0x0061a310
void
CTokenizer::Init(const char *text, const char *delims)
{
	m_cur = text;
	m_start = text;
	m_unused = 0;
	memset(m_delims, 0, sizeof(m_delims));
	strncpy(m_delims, delims, sizeof(m_delims));
	while(*m_cur != '\0' && IsDelim(*m_cur)) m_cur++;
}

// 0x0061a370 : si on est sur un délimiteur on les saute ; sinon on avance
// jusqu'au prochain délimiteur, puis on saute les délimiteurs.
void
CTokenizer::Next(void)
{
	if(IsDelim(*m_cur))
		while(*m_cur != '\0' && IsDelim(*m_cur)) m_cur++;
	while(*m_cur != '\0' && !IsDelim(*m_cur)) m_cur++;
	while(*m_cur != '\0' && IsDelim(*m_cur)) m_cur++;
}

// 0x0061a250 : copie le jeton courant jusqu'à `stop` ou un délimiteur ;
// retourne nil si le jeton a dû être tronqué.
char *
CTokenizer::Copy(char *dest, uint32 size, char stop, bool useDelims)
{
	const char *p = m_cur;
	while(*p != stop && !(useDelims && IsDelim(*p))) p++;
	uint32 len = (uint32)(p - m_cur);
	char *res = dest;
	if(len >= size){ res = nil; len = size - 1; }
	memcpy(dest, m_cur, len);
	dest[len] = '\0';
	return res;
}

// 0x0061a430
int32
CTokenizer::AsLong(void)
{
	char buf[16];
	if(Copy(buf, sizeof(buf), '\0', true) == nil) return 0;
	return (int32)atol(buf);
}

// 0x0061a460
float
CTokenizer::AsFloat(void)
{
	char buf[16];
	if(Copy(buf, sizeof(buf), '\0', true) == nil) return 0.0f;
	return (float)atof(buf);
}
