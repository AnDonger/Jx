#ifndef KMagicDescH
#define	KMagicDescH

#include "KIniFile.h"

class KMagicDesc
{
private:
	KIniFile	m_IniFile;
	char		m_szDesc[128];
public:
	KMagicDesc();
	~KMagicDesc();
	BOOL		Init();
	const char*	GetDesc(void* pData);
	const char*	GetGemDesc(const KMagicAttrib& Attrib);
};

extern KMagicDesc	g_MagicDesc;
#endif
