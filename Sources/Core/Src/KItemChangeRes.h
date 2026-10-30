//---------------------------------------------------------------------------
// Sword3 Core (c) 2002 by Kingsoft
//
// File:	KItemChangeRes.h
// Date:	2002.12
// Code:	Spe
// Desc:	Header File
//---------------------------------------------------------------------------

#ifndef	KItemChangeResH
#define	KItemChangeResH

#include "KTabFile.h"
#include <map>

class KItemChangeRes
{
private:
	int			m_nVersion;
	std::map<int, KItemChangeRes*> m_VersionResources;
	KItemChangeRes* GetVersionResources(int nVersion);
	KTabFile	m_MeleeWeapon;
	KTabFile	m_RangeWeapon;
	KTabFile	m_Armor;
	KTabFile	m_Helm;
	KTabFile	m_Horse;
	KTabFile	m_MeleeEquipment;
	KTabFile	m_RangeEquipment;
	KTabFile	m_ArmorEquipment;
	KTabFile	m_HelmEquipment;
	KTabFile	m_HorseEquipment;
public:
	KItemChangeRes();
	~KItemChangeRes();
	BOOL		Init(int nVersion = -1);
	int			GetWeaponRes(int nDetail, int nParti, int nLevel, int nVersion = -1);
	int			GetArmorRes(int nParti, int nLevel, int nVersion = -1);
	int			GetHelmRes(int nParti, int nLevel, int nVersion = -1);
	int			GetHorseRes(int nParti, int nLevel, int nVersion = -1);
};

extern KItemChangeRes	g_ItemChangeRes;
#endif
