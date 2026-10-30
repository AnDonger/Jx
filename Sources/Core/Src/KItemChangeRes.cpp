//---------------------------------------------------------------------------
// Sword3 Core (c) 2002 by Kingsoft
//
// File:	KItemChangeRes.h
// Date:	2002.12
// Code:	Spe
// Desc:	Header File
//---------------------------------------------------------------------------
#include "KCore.h"
#include "KEngine.h"
#include "KItemChangeRes.h"
#include "KItem.h"
#include "CoreUseNameDef.h"


static BOOL LoadItemChangeTable(KTabFile* pTable, const char* pszName, int nVersion)
{
	char szFileName[MAX_PATH];
	sprintf(szFileName, "\\settings\\item\\%03d\\%s", nVersion, pszName);
	if (pTable->Load(szFileName))
		return TRUE;
	sprintf(szFileName, "\\settings\\item\\%s", pszName);
	return pTable->Load(szFileName);
}

static int FindItemChangeRow(KTabFile* pEquipment, int nDetail,
	int nParticular, int nLevel, BOOL bDefaultRow)
{
	if (pEquipment->GetHeight() > 1)
	{
		for (int nRow = 2; nRow <= pEquipment->GetHeight(); nRow++)
		{
			int nItemDetail;
			int nItemParticular;
			int nItemLevel;
			pEquipment->GetInteger(nRow, 3, -1, &nItemDetail);
			pEquipment->GetInteger(nRow, 4, -1, &nItemParticular);
			pEquipment->GetInteger(nRow, 12, -1, &nItemLevel);
			if (nItemDetail == nDetail && nItemParticular == nParticular &&
				nItemLevel == nLevel)
				return nRow + (bDefaultRow ? 1 : 0);
		}
		return 0;
	}

	// Original unversioned equipment tables use ten levels per particular type.
	return nParticular * 10 + nLevel + (bDefaultRow ? 2 : 1);
}

KItemChangeRes	g_ItemChangeRes;
KItemChangeRes::KItemChangeRes()
{
	m_nVersion = -1;
}

KItemChangeRes::~KItemChangeRes()
{
	std::map<int, KItemChangeRes*>::iterator it;
	for (it = m_VersionResources.begin(); it != m_VersionResources.end(); ++it)
		delete it->second;
	m_VersionResources.clear();
}

KItemChangeRes* KItemChangeRes::GetVersionResources(int nVersion)
{
	if (nVersion == -1 || nVersion == m_nVersion)
		return this;
	if (nVersion < 0 || nVersion > 999)
		return NULL;

	std::map<int, KItemChangeRes*>::iterator it = m_VersionResources.find(nVersion);
	if (it != m_VersionResources.end())
		return it->second;

	KItemChangeRes* pResources = new KItemChangeRes;
	if (!pResources->Init(nVersion))
	{
		delete pResources;
		g_DebugLog("[Item] Appearance resource version is unavailable: %d", nVersion);
		return NULL;
	}
	m_VersionResources[nVersion] = pResources;
	return pResources;
}

BOOL KItemChangeRes::Init(int nVersion)
{
	if (nVersion == -1)
	{
		KIniFile itemVersion;
		nVersion = 1;
		if (itemVersion.Load("\\settings\\item\\itemversion.ini"))
			itemVersion.GetInteger("ItemVersion", "Version", 1, &nVersion);
	}
	if (nVersion < 0 || nVersion > 999)
		return FALSE;

	m_nVersion = nVersion;
	BOOL bResult = TRUE;
	bResult = LoadItemChangeTable(&m_MeleeWeapon, "MeleeRes.txt", nVersion) && bResult;
	bResult = LoadItemChangeTable(&m_RangeWeapon, "RangeRes.txt", nVersion) && bResult;
	bResult = LoadItemChangeTable(&m_Armor, "ArmorRes.txt", nVersion) && bResult;
	bResult = LoadItemChangeTable(&m_Helm, "HelmRes.txt", nVersion) && bResult;
	bResult = LoadItemChangeTable(&m_Horse, "HorseRes.txt", nVersion) && bResult;

	LoadItemChangeTable(&m_MeleeEquipment, "meleeweapon.txt", nVersion);
	LoadItemChangeTable(&m_RangeEquipment, "rangeweapon.txt", nVersion);
	LoadItemChangeTable(&m_ArmorEquipment, "armor.txt", nVersion);
	LoadItemChangeTable(&m_HelmEquipment, "helm.txt", nVersion);
	LoadItemChangeTable(&m_HorseEquipment, "horse.txt", nVersion);
	return bResult;
}

int KItemChangeRes::GetWeaponRes(int nDetail, int nParti, int nLevel, int nVersion)
{
	KItemChangeRes* pResources = GetVersionResources(nVersion);
	if (!pResources)
		return 0;
	if (pResources != this)
		return pResources->GetWeaponRes(nDetail, nParti, nLevel);

	int nRet = 2;
	if (nLevel == 0)
	{
		m_MeleeWeapon.GetInteger(2, 2, 2, &nRet);
		return nRet - 2;
	}

	int nRow = 0;
	switch (nDetail)
	{
	case equip_meleeweapon:
		nRow = FindItemChangeRow(&m_MeleeEquipment, nDetail, nParti, nLevel, TRUE);
		if (nRow > 0)
			m_MeleeWeapon.GetInteger(nRow, 2, 2, &nRet);
		break;
	case equip_rangeweapon:
		nRow = FindItemChangeRow(&m_RangeEquipment, nDetail, nParti, nLevel, FALSE);
		if (nRow > 0)
			m_RangeWeapon.GetInteger(nRow, 2, 2, &nRet);
		break;
	}
	return nRet - 2;
}

int KItemChangeRes::GetArmorRes(int nParti, int nLevel, int nVersion)
{
	KItemChangeRes* pResources = GetVersionResources(nVersion);
	if (!pResources)
		return 0;
	if (pResources != this)
		return pResources->GetArmorRes(nParti, nLevel);

	int nRet = 2;
	int nRow = 2;
	if (nLevel != 0)
		nRow = FindItemChangeRow(&m_ArmorEquipment, equip_armor, nParti, nLevel, TRUE);
	if (nRow > 0)
		m_Armor.GetInteger(nRow, 2, 2, &nRet);
	return nRet - 2;
}

int KItemChangeRes::GetHelmRes(int nParti, int nLevel, int nVersion)
{
	KItemChangeRes* pResources = GetVersionResources(nVersion);
	if (!pResources)
		return 0;
	if (pResources != this)
		return pResources->GetHelmRes(nParti, nLevel);

	int nRet = 2;
	int nRow = 2;
	if (nLevel != 0)
		nRow = FindItemChangeRow(&m_HelmEquipment, equip_helm, nParti, nLevel, TRUE);
	if (nRow > 0)
		m_Helm.GetInteger(nRow, 2, 2, &nRet);
	return nRet - 2;
}

int KItemChangeRes::GetHorseRes(int nParti, int nLevel, int nVersion)
{
	KItemChangeRes* pResources = GetVersionResources(nVersion);
	if (!pResources)
		return -1;
	if (pResources != this)
		return pResources->GetHorseRes(nParti, nLevel);

	if (nLevel == 0)
		return -1;

	int nRet = 2;
	int nRow = FindItemChangeRow(&m_HorseEquipment, equip_horse, nParti, nLevel, TRUE);
	if (nRow > 0)
		m_Horse.GetInteger(nRow, 2, 2, &nRet);
	return nRet - 2;
}
