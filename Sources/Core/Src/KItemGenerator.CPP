//---------------------------------------------------------------------------
// Sword3 Core (c) 2002 by Kingsoft
//
// File:	KItemGenerator.CPP
// Date:	2002.08.26
// Code:	DongBo
// Desc:    CPP file. 本文件实现的类用于生成道具
//---------------------------------------------------------------------------
#include "KEngine.h"
#include "KCore.h"
#include "MyAssert.H"
#include "KItem.h"
#include "KSubWorldSet.h"
#include "KItemGenerator.h"

KItemGenerator	ItemGen;			//	装备生成器

KItemGenerator::KItemGenerator()
{
	m_nCurrentVersion = -1;
	m_uRandomSeed = 42;
	m_bReforgeEnabled = FALSE;
	m_nReforgeBaseRate = 0;
	m_nReforgeLuckMin = 5;
	m_nReforgeLuckMax = 10;
}

// 当前客户端物品使用MSVC随机序列，不能沿用引擎旧随机序列。
void KItemGenerator::SetRandomSeed(UINT uSeed)
{
	m_uRandomSeed = uSeed;
}

static int NextItemRandom(UINT& uSeed, int nMin, int nMax)
{
	UINT nRange = (UINT)(nMax - nMin) + 1;
	if (nRange == 0)
		return nMin;
	uSeed = uSeed * 214013U + 2531011U;
	return nMin + (int)(((uSeed >> 16) & 0x7fffU) % nRange);
}

int KItemGenerator::GetRandomNumber(int nMin, int nMax)
{
	return NextItemRandom(m_uRandomSeed, nMin, nMax);
}

KItemGenerator::~KItemGenerator()
{
	std::map<int, KLibOfBPT*>::iterator it;
	for (it = m_VersionLibraries.begin(); it != m_VersionLibraries.end(); ++it)
		delete it->second;
	m_VersionLibraries.clear();
}

/******************************************************************************
	功能：	数据初始化. 从tab file中读取数据
******************************************************************************/
KLibOfBPT* KItemGenerator::GetVersionLibrary(int nVersion)
{
	if (nVersion == -1)
		nVersion = m_nCurrentVersion;
	if (nVersion < 0 || nVersion > 999)
		return NULL;
	if (nVersion == m_nCurrentVersion)
		return &m_BPTLib;

	std::map<int, KLibOfBPT*>::iterator it = m_VersionLibraries.find(nVersion);
	if (it != m_VersionLibraries.end())
		return it->second;

	KLibOfBPT* pLibrary = new KLibOfBPT;
	if (!pLibrary->Init(nVersion))
	{
		delete pLibrary;
		g_DebugLog("[Item] Equipment resource version is unavailable: %d", nVersion);
		return NULL;
	}
	m_VersionLibraries[nVersion] = pLibrary;
	return pLibrary;
}

static int EquipmentUpgradeClass(int nDetail, int nParticular)
{
	if (nDetail == 4 || nDetail == 13 || nDetail == 14 || nDetail == 15)
		return 0;
	if (nParticular < 0 || nParticular >= 74)
		return -1;
	// Current client 0x7382F0; independent from the wearer's profession.
	static const signed char Class0[74] = {
		0, 0, 1, 2, 0, 0, 1, 2, -1, 0, 1, 2, -1, 0, 0, 0, 0, 0, 0, 1,
		1, 1, 2, 2, 2, 0, 0, 0, 0, 1, 2, 0, 0, 1, 1, 2, 2, 0, 0, 1,
		2, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2, 0,
		0, 1, 1, 2, 2, -1, -1, -1, 0, 0, 1, 2, -1, -1,
	};
	static const signed char Class2[74] = {
		0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2,
		2, 2, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2, 0, 1, 2, 0, 1, 2,
		0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 1, 1, 2, 2, 0, 1,
		2, 0, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2,
	};
	static const signed char Class5[74] = {
		0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1,
		2, 2, 0, 1, 2, 0, 1, 2, 0, 0, 1, 1, 2, 2, 0, 1, 2, 0, 0, 1,
		2, 0, 1, 2, 0, 1, 2, 0, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	};
	static const signed char Class6[74] = {
		0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1,
		2, 2, 0, 1, 2, 0, 1, 2, 0, 0, 1, 1, 2, 2, 0, 1, 2, 0, 0, 1,
		2, 0, 1, 2, 0, 1, 2, 0, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	};
	static const signed char Class7[74] = {
		0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1,
		2, 2, 0, 1, 2, 0, 1, 2, 0, 0, 1, 1, 2, 2, 0, 1, 2, 0, 0, 1,
		2, 0, 1, 2, 0, 1, 2, 0, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	};
	static const signed char Class9[74] = {
		0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2,
		2, 2, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2, 0, 1, 2, 0, 1, 2,
		0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 1, 1, 2, 2, 0, 1,
		2, 0, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2,
	};
	switch (nDetail)
	{
	case 0: return Class0[nParticular];
	case 2: return Class2[nParticular];
	case 5: return Class5[nParticular];
	case 6: return Class6[nParticular];
	case 7: return Class7[nParticular];
	case 9: return Class9[nParticular];
	default: return -1;
	}
}

int KItemGenerator::GetUpgradeAttribValue(const KItem& Data, int nAttrib)
{
	int nCount = Data.GetUpgradeData() & 0xff;
	int nClass = EquipmentUpgradeClass(Data.GetDetailType(), Data.GetParticular());
	if (Data.GetGenre() != item_equip || nCount <= 0 || nCount > 15 || nClass < 0)
		return 0;
	KLibOfBPT* pLibrary = GetVersionLibrary(Data.m_GeneratorParam.nVersion);
	return pLibrary ? pLibrary->GetUpgradeAttribValue(Data.GetDetailType(), nClass,
		Data.GetUpgradeData() >> 8, nAttrib, nCount) : 0;
}

BOOL KItemGenerator::UsesClassMagicWeights(int nVersion)
{
	KLibOfBPT* pLibrary = GetVersionLibrary(nVersion);
	const KMAGICATTRIB_TABFILE* pFormat = pLibrary ? pLibrary->GetMARecord(0) : NULL;
	return pFormat && pFormat->m_bClassWeights;
}

const KGREEN_ADDON* KItemGenerator::GetGreenAddon(int nGreenId, int nVersion)
{
	KLibOfBPT* pLibrary = GetVersionLibrary(nVersion);
	return pLibrary ? pLibrary->GetGreenAddon(nGreenId) : NULL;
}

int KItemGenerator::GetGreenMagicAttribCount(int nGreenId, int nVersion)
{
	KLibOfBPT* pLibrary = GetVersionLibrary(nVersion);
	return pLibrary ? pLibrary->GetGreenMagicAttribCount(nGreenId) : 0;
}

BOOL KItemGenerator::Init()
{
	KIniFile itemVersion;
	m_nCurrentVersion = 1;
	if (itemVersion.Load("\\settings\\item\\itemversion.ini"))
		itemVersion.GetInteger("ItemVersion", "Version", 1, &m_nCurrentVersion);
	if (m_nCurrentVersion < 0 || m_nCurrentVersion > 999)
		return FALSE;
	if (!m_BPTLib.Init(m_nCurrentVersion))		// 此调用从若干的tab file中载入所有的初始属性
		return FALSE;
	if (!InitGemRules())
		return FALSE;

	for (int i = equip_meleeweapon; i < equip_detailnum - equip_meleeweapon; i++)
	{
		switch(i)
		{
		case equip_meleeweapon:
			m_EquipNumOfEntries[i] = m_BPTLib.GetMeleeWeaponRecordNumber();
			break;
		case equip_rangeweapon:
			m_EquipNumOfEntries[i] = m_BPTLib.GetRangeWeaponRecordNumber();
			break;
		case equip_armor:
			m_EquipNumOfEntries[i] = m_BPTLib.GetArmorRecordNumber();
			break;
		case equip_helm:
			m_EquipNumOfEntries[i] = m_BPTLib.GetHelmRecordNumber();
			break;
		case equip_boots:
			m_EquipNumOfEntries[i] = m_BPTLib.GetBootRecordNumber();
			break;
		case equip_belt:
			m_EquipNumOfEntries[i] = m_BPTLib.GetBeltRecordNumber();
			break;
		case equip_cuff:
			m_EquipNumOfEntries[i] = m_BPTLib.GetCuffRecordNumber();
			break;
		case equip_amulet:
			m_EquipNumOfEntries[i] = m_BPTLib.GetAmuletRecordNumber();
			break;
		case equip_ring:
			m_EquipNumOfEntries[i] = m_BPTLib.GetRingRecordNumber();
			break;
		case equip_pendant:
			m_EquipNumOfEntries[i] = m_BPTLib.GetPendantRecordNumber();
			break;
		case equip_horse:
			m_EquipNumOfEntries[i] = m_BPTLib.GetHorseRecordNumber();
			break;
		default:
			m_EquipNumOfEntries[i] = m_BPTLib.GetExtraEquipmentRecordNumber(i);
			break;
		}
	}
	m_MedNumOfEntries = m_BPTLib.GetMedicineRecordNumber();
#ifdef _SERVER
	InitReforge();
#endif
	return TRUE;
/*
	m_BPTLib.Init();
	int naryMALevel[] = {1,2,3,4,5,6};
	KItem theItem;
	Gen_Equipment(1, 1, 0, 5, naryMALevel, 45, &theItem);
	return TRUE;
*/
}

/******************************************************************************
	功能：	生成药品
	入口:	nDetailType: 药品类型
			nLevel: 等级
			nVersion: 版本号
	出口:	成功时返回非零, 相关数据在 pItem 所指对象中给出
			失败时返回零
******************************************************************************/
BOOL KItemGenerator::Gen_Medicine(IN int nDetailType,
								  IN int nLevel,
								  IN int nVersion,
								  IN OUT KItem* pItem)
{
	_ASSERT(this != NULL);
	_ASSERT(pItem != NULL);

	BOOL bEC = FALSE;
	
#ifdef _SERVER	// 服务器版本
	pItem->m_GeneratorParam.uRandomSeed = g_GetRandomSeed();
	pItem->m_GeneratorParam.nVersion = g_SubWorldSet.GetGameVersion();
	pItem->m_GeneratorParam.nLuck = 0;
#else
	g_RandomSeed(pItem->m_GeneratorParam.uRandomSeed);
	int nGameVersion = pItem->m_GeneratorParam.nVersion;
#endif

												// 等级数据从1开始，所以要减回来
	const KBASICPROP_MEDICINE* pMed = NULL;
	pMed = m_BPTLib.FindMedicine(nDetailType, nLevel);
	if (NULL == pMed)
	{
		_ASSERT(FALSE);
		return bEC;
	}
	ZeroMemory(pItem->m_GeneratorParam.nGeneratorLevel, sizeof(pItem->m_GeneratorParam.nGeneratorLevel));
	pItem->SetDurability(-1);
	*pItem = *pMed;
	return TRUE;
}
/******************************************************************************
	功能：	生成随机装备
	入口:	nSeriesReq: 五行属性
			nLevel: 等级
			nLucky: 运气值
			nVersion: 版本号
	出口:	成功时返回非零, 相关数据在 pItem 所指对象中给出
			失败时返回零
******************************************************************************/
BOOL KItemGenerator::Gen_Item(IN int nItemGenre,
							  IN int nSeriesReq,
							  IN int nLevel,
							  IN int nLucky,
							  IN OUT KItem* pItem)
{
	_ASSERT(this != NULL);
	_ASSERT(pItem != NULL);

	switch (nItemGenre)
	{
	case item_equip:
		{
			int nDetailType, nParticularType, i;
			// 专用装备不加入旧随机掉落池，避免改变原掉落分布及抽到未确认的12类。
			nDetailType = g_Random(equip_jadependant);
			nParticularType = g_Random(m_EquipNumOfEntries[nDetailType] + 1);
			int naryMALevel[6];
			ZeroMemory(naryMALevel, 6 * sizeof(int));
			for (i = 0; i < 6; i++)
			{
				naryMALevel[i] = g_Random(nLevel / 10 + 1);
				if (!naryMALevel[i])
					break;
			}
			Gen_Equipment(nDetailType, nParticularType, nSeriesReq, nLevel, naryMALevel, nLucky, g_SubWorldSet.GetGameVersion(), pItem);
		}
		break;
	case item_medicine:
		{
			int nDetailType;
			nDetailType = g_Random(medicine_detailnum);
			Gen_Medicine(nDetailType, nLevel, g_SubWorldSet.GetGameVersion(), pItem);
		}
		break;
	case item_task:
		{
			Gen_Quest(nSeriesReq, pItem);			
		}
		break;
	case item_townportal:
		{
			Gen_TownPortal(pItem);
		}
		break;
	default:
		break;
	}
	
	return TRUE;
}
/******************************************************************************
	功能：	生成随机装备
	入口:	nDetailType: 具体类别, 如近程武器、暗器、......
			nParticularType: 详细类别
			nSeriesReq: 五行属性
			nLevel: 等级
			pnaryMALevel: 魔法属性等级数组[6]
			nLucky: 运气值
	出口:	成功时返回非零, 相关数据在 pItem 所指对象中给出
			失败时返回零
******************************************************************************/
BOOL KItemGenerator::Gen_Equipment(IN int nDetailType,
								   IN int nParticularType,
								   IN int nSeriesReq,
								   IN int nLevel,
								   IN const int* pnaryMALevel,
								   IN int nLucky,
								   IN int nVersion,
								   IN OUT KItem* pItem)
{
	_ASSERT(this != NULL);
	_ASSERT(pItem != NULL);

	BOOL bEC = FALSE;
#ifdef _SERVER	// 服务器版本
	pItem->m_GeneratorParam.uRandomSeed = g_GetRandomSeed();
	// 装备使用独立序列，同时推进引擎种子，避免连续生成时重复用同一种子。
	g_Random(0xffffffffU);
	if (pnaryMALevel)
		memcpy(pItem->m_GeneratorParam.nGeneratorLevel, pnaryMALevel, sizeof(int) * 6);
	else
		ZeroMemory(pItem->m_GeneratorParam.nGeneratorLevel, sizeof(int) * 6);
	pItem->m_GeneratorParam.nLuck = nLucky;
#else
	pItem->m_GeneratorParam.nLuck = nLucky;
	if (pnaryMALevel)
		memcpy(pItem->m_GeneratorParam.nGeneratorLevel, pnaryMALevel, sizeof(int) * 6);
#endif
	SetRandomSeed(pItem->m_GeneratorParam.uRandomSeed);
	pItem->m_GeneratorParam.nVersion = nVersion == -1 ? m_nCurrentVersion : nVersion;
	int nGameVersion = pItem->m_GeneratorParam.nVersion;

	// 根据入口参数, 确定装备的基本数据
	KLibOfBPT* pLibrary = GetVersionLibrary(pItem->m_GeneratorParam.nVersion);
	if (!pLibrary)
		return FALSE;
	const KBASICPROP_EQUIPMENT* pEqu =
		pLibrary->FindEquipment(nDetailType, nParticularType, nLevel);
	if (NULL == pEqu)
		{ _ASSERT(FALSE); return bEC; }
		// 运行至此的直接原因: 只有n种装备, 而上面 i 的值在[0,n-1]之外
		// 检查3点: nParticularType 有误?
		//			nLevel 有误?
		//			原始的tab file有误导致 m_BPTLib.m_BPTEquipment 所
		//			管理的数据有问题?
	pItem->SetAttrib_CBR(pEqu);
	pItem->SetSeries(nSeriesReq);

	if (NULL == pnaryMALevel || pEqu->m_nSeries != 0)
		return TRUE;
	// 确定装备的魔法属性
	KItemNormalAttrib	sMA[6];	// 道具的魔法属性
	int nClass = -1;
	for (int n = 0; n < 6; n++)
		if (pEqu->m_aryPropReq[n].nType == magic_requireseries)
			nClass = pEqu->m_aryPropReq[n].nPara;
	bEC = Gen_MagicAttrib(nDetailType, pnaryMALevel, nSeriesReq, nLucky, sMA, nGameVersion, pLibrary, nClass, nLevel);
	if (bEC)
		pItem->SetAttrib_MA(sMA);

	return bEC;
}

/******************************************************************************
	功能：	生成魔法属性
	入口:	nType: 装备类型
			pnaryMALevel: 数组指针, 给出魔法的等级要求, 共6个
			nSeriesReq: 五行属性
			nLucky: 运气值
	出口:	在 pnaryMA 所指数组中返回魔法的属性核心数据, 共6个
******************************************************************************/
// Add by Freeway Chen in 2003.5.30
BOOL KItemGenerator::Gen_MagicAttrib(
    int nType, const int* pnaryMALevel, int nSeriesReq, int nLucky,
	KItemNormalAttrib* pnaryMA, int nGameVersion, KLibOfBPT* pLibrary, int nClass, int nEquipmentLevel
)
{
    int nResult = false;

    KBPT_ClassMAIT  SelectedMagicTable;	
	// 记录每一个记录选中的魔法属性，主要是为了最后将使用标志清除
    KMAGICATTRIB_TABFILE *pMagicAttrTable[6] = { NULL, NULL, NULL, NULL, NULL, NULL };

    int i = 0;

    _ASSERT(pnaryMALevel);
    _ASSERT(pnaryMA);
	if (!pnaryMALevel || !pnaryMA || nType < 0 || nType >= MATF_CBDR)
		return FALSE;

	const KMAGICATTRIB_TABFILE* pFormat = pLibrary->GetMARecord(0);
	if (pFormat && pFormat->m_bClassWeights)
	{
		// GUI IDA 0x75BA20：现代原表按职业/部位分类，不使用旧前后缀及五行索引。
		// 每轮两个随机阈值、候选索引、三个参数的抽取顺序必须与客户端一致。
		ZeroMemory(pnaryMA, sizeof(KItemNormalAttrib) * 6);
		if (nClass == -1)
			nClass = 0;
		if (nClass < 0 || nClass >= 3)
			return FALSE;
		int nCount = pLibrary->GetModernMagicCount(nType, nEquipmentLevel);
		for (i = 0; i < nCount && pnaryMALevel[i] != 0; i++)
		{
			SelectedMagicTable.Clear();
			int nRate1 = GetRandomNumber(0, pLibrary->GetModernMagicRateLimit(i, 0));
			int nRate2 = GetRandomNumber(0, pLibrary->GetModernMagicRateLimit(i, 1));
			int nThreshold = nRate1 * 1000 + nRate2;
			for (int nRow = 0; nRow < pLibrary->GetMARecordNumber(); nRow++)
			{
				const KMAGICATTRIB_TABFILE* pCandidate = pLibrary->GetMARecord(nRow);
				if (!pCandidate || !pCandidate->m_bClassWeights ||
					pCandidate->m_nLevel != pnaryMALevel[0] ||
					pCandidate->m_ClassDropRate[nClass][nType] <= nThreshold)
					continue;
				int nPrevious = 0;
				for (; nPrevious < i; nPrevious++)
					if (pnaryMA[nPrevious].nAttribType == pCandidate->m_MagicAttrib.nPropKind)
						break;
				if (nPrevious == i)
					SelectedMagicTable.Insert(nRow);
			}
			if (SelectedMagicTable.GetCount() == 0)
				break;
			int nSelected = GetRandomNumber(0, SelectedMagicTable.GetCount() - 1);
			const KMAGICATTRIB_TABFILE* pSelected = pLibrary->GetMARecord(SelectedMagicTable.Get(nSelected));
			pnaryMA[i].nAttribType = pSelected->m_MagicAttrib.nPropKind;
			for (int nParam = 0; nParam < 3; nParam++)
				pnaryMA[i].nValue[nParam] = GetRandomNumber(
					pSelected->m_MagicAttrib.aryRange[nParam].nMin,
					pSelected->m_MagicAttrib.aryRange[nParam].nMax) *
					pSelected->m_ParamMultiplier[nClass][nParam];
		}
		return TRUE;
	}

    for (i = 0; i < 6; i++) 
    {
        // 如果发现级别为零，表示不需要继续处理了，退出
        if (pnaryMALevel[i] == 0)	
        	break;

        SelectedMagicTable.Clear();

        // 根据前缀后缀、物品类型、五行需求、需求级别将所有符合条件的魔法属性项取出来
        const KBPT_ClassMAIT *pCMITItem = pLibrary->GetCMIT(
            1 - (i & 1),        // 前缀后缀
            nType,              // 物品类型
            nSeriesReq,         // 五行需求 
            pnaryMALevel[i]     // 需求级别
        );

        _ASSERT(pCMITItem);
        if (!pCMITItem)
        {
            g_DebugLog(
                "[GenMagicAttrib] GetCMIT Error, nPrefixPostfix = %d, nType = %d, nSeries = %d, nLevel = %d", 
                1 - (i & 1), nType, nSeriesReq, pnaryMALevel[i]
            );
            break;
        }

        // 遍历符合条件的魔法属性，根据
        // 1.是否已经使用，
        // 2.运气值
        // 3.是否有相同的“属性调整类别”
        // 取出相应的项放在SelectedMagicTable中
        int nCMITItemCount = pCMITItem->GetCount();
        int j = 0;
        int nDecide = GetRandomNumber(0, 100 - 1) / (1 + nLucky * 20 / 100);

        for (j = 0; j < nCMITItemCount; j++)
        {
            // 根据索引值，取出相应的项
            int nMAIndex = pCMITItem->Get(j);
            KMAGICATTRIB_TABFILE *pMAItem = (KMAGICATTRIB_TABFILE *)pLibrary->GetMARecord(nMAIndex);
            if (!pMAItem)
            {
                _ASSERT(pMAItem);
                continue;
            }

            // 如果已经使用，跳过取下一个
            if (pMAItem->m_nUseFlag)
                continue;

            // 如果运气不足，取下一个
            if (pMAItem->m_bClassWeights ?
				(nClass < 0 || nClass >= 3 || pMAItem->m_ClassDropRate[nClass][nType] <= 0) :
				(pMAItem->m_DropRate[nType] <= nDecide))
                continue;
            
            // 检查是否已经有相同的“属性调整类别”
            int k = 0;
            for (k = 0; k < i; k++)
            {
                _ASSERT(pMagicAttrTable[k]);

                if ((pMagicAttrTable[k]->m_MagicAttrib.nPropKind) == (pMAItem->m_MagicAttrib.nPropKind))
                    break;
            }

            // 如果发现 “属性调整类别”相同，说明已经有一个相同的值被选中，那么取下一个
            if (k < i)
                continue;

            SelectedMagicTable.Insert(nMAIndex);
        }

        int nSelectedCount = SelectedMagicTable.GetCount();

        if (nSelectedCount == 0)
        {   
            // 如果没有满足条件的魔法属性，就退出
            break;
        }
        
        // 如果发现满足条件的魔法属性，就设置
        int nLuckyItemIndex = GetRandomNumber(0, nSelectedCount - 1);
		const KMAGICATTRIB_TABFILE* pFirst = pLibrary->GetMARecord(SelectedMagicTable.Get(0));
		if (pFirst && pFirst->m_bClassWeights)
		{
			__int64 nTotalWeight = 0;
			for (int n = 0; n < nSelectedCount; n++)
				nTotalWeight += pLibrary->GetMARecord(SelectedMagicTable.Get(n))->m_ClassDropRate[nClass][nType];
			// Scale the deterministic 15-bit item RNG instead of taking modulo of a million-point weight.
			__int64 nChoice = (__int64)GetRandomNumber(0, 32767) * nTotalWeight / 32768;
			for (int n = 0; n < nSelectedCount; n++)
			{
				nChoice -= pLibrary->GetMARecord(SelectedMagicTable.Get(n))->m_ClassDropRate[nClass][nType];
				if (nChoice < 0) { nLuckyItemIndex = n; break; }
			}
		}
        KMAGICATTRIB_TABFILE *pMAItem = (KMAGICATTRIB_TABFILE *)pLibrary->GetMARecord(
             SelectedMagicTable.Get(nLuckyItemIndex)
        );

        // 保存满足条件的魔法属性，以便在最后的时候将使用标志清除
        pMagicAttrTable[i] = pMAItem;

        _ASSERT(pMAItem);
        _ASSERT(!(pMAItem->m_nUseFlag));     // 确定还没有使用

        // 设置使用标志
        pMAItem->m_nUseFlag = true;
		
        pnaryMA[i].nAttribType = pMAItem->m_MagicAttrib.nPropKind;
        pnaryMA[i].nValue[0] = GetRandomNumber(
            pMAItem->m_MagicAttrib.aryRange[0].nMin, 
            pMAItem->m_MagicAttrib.aryRange[0].nMax
        );
        pnaryMA[i].nValue[1] = GetRandomNumber(
            pMAItem->m_MagicAttrib.aryRange[1].nMin, 
            pMAItem->m_MagicAttrib.aryRange[1].nMax
        );
        pnaryMA[i].nValue[2] = GetRandomNumber(
            pMAItem->m_MagicAttrib.aryRange[2].nMin, 
            pMAItem->m_MagicAttrib.aryRange[2].nMax
        );
		if (pMAItem->m_bClassWeights)
			for (int n = 0; n < 3; n++)
				pnaryMA[i].nValue[n] *= pMAItem->m_ParamMultiplier[nClass][n];
		g_DebugLog(
			"[GenMagicAttrib] pnaryMA[%d]: (%d:%s), nAttribType = %d, nValue[0] = %d, nValue[1] = %d, nValue[2] = %d,", 
			i,
			SelectedMagicTable.Get(nLuckyItemIndex),
			pMAItem->m_szName,
			pnaryMA[i].nAttribType,
			pnaryMA[i].nValue[0],
			pnaryMA[i].nValue[1],
			pnaryMA[i].nValue[2]
		);

    } // for 遍历需要取回的魔法属性表

    for (NULL; i < 6; i++)
    {
        // 清空剩下的项
        pnaryMA[i].nAttribType = 0;
        pnaryMA[i].nValue[0] = 0;
        pnaryMA[i].nValue[1] = 0;
        pnaryMA[i].nValue[2] = 0;
    }

    for (i = 0; i < 6; i++)
    {
        if (!pMagicAttrTable[i])
            break;

        _ASSERT(pMagicAttrTable[i]->m_nUseFlag);   // 确定已经使用

        pMagicAttrTable[i]->m_nUseFlag = false;    // 设置为没用使用
    }

    nResult = true;
//Exit0:
	return nResult;
}

// Following Code is implement by DongBo
//BOOL KItemGenerator::Gen_MagicAttrib(int nType, const int* pnaryMALevel, int nSeriesReq, int nLucky,
//									 KItemNormalAttrib* pnaryMA)
//{
//	
//    // 确定适用于该装备的全部魔法属性及其数量
//	if (FALSE == GMA_GetAvaliableMA(nType))
//		{ _ASSERT(FALSE); return FALSE; }
//
//	// 确定上述各魔法属性的等级,出现概率
//	if (FALSE == GMA_GetLevelAndDropRate(nType))
//		{ _ASSERT(FALSE); return FALSE; }
//
//	// 从上述魔法属性中选出符合条件的. 策划要求选出6个
//	for (int i = 0; i < 6; i++)				// 奇数: 前缀, 偶数: 后缀
//	{
//		GMA_ChooseMA((i&1), pnaryMALevel[i], nLucky, &(pnaryMA[i]));
//		if (pnaryMA[i].nAttribType == 0)
//			break;
//	}
//	return TRUE;
//}

/******************************************************************************
	功能:	确定适用于指定装备的全部魔法属性及其数量
	入口:	nType: 指定装备
	出口:	成功时返回非零. m_GMA_naryMA[0]给出适用于该装备的全部魔法属性前缀
							m_GMA_nCount[0]给出上述魔法属性的总数
							m_GMA_naryMA[1]给出适用于该装备的全部魔法属性后缀
							m_GMA_nCount[1]给出上述魔法属性的总数
			失败时返回零
******************************************************************************/
BOOL KItemGenerator::GMA_GetAvaliableMA(int nType)
{
	//TODO: 可能需要对nType进行转换, 从装备类型值变成 m_BPTLib.m_CMAT 数组的索引
	for (int i = 0; i < 2; i++)		// 0: 前缀, 1: 后缀
	{
		const KBPT_ClassifiedMAT* pCMAT;
		pCMAT = m_BPTLib.GetCMAT(i, nType);
		if (pCMAT == NULL)
			{ _ASSERT(FALSE); return FALSE; }
		m_GMA_nCount[i] = NUMOFCMA;
		pCMAT->GetAll((int*)m_GMA_naryMA[i], &(m_GMA_nCount[i]));
	}
	return TRUE;
}

/******************************************************************************
	功能:	确定各给定魔法属性的等级要求和出现概率
	入口:	nType: 指定装备
			m_GMA_naryMA数组: 给出全部魔法属性的索引值
	出口:	成功时返回非零. m_GMA_naryLevel数组给出上述魔法属性的等级要求
							m_GMA_naryDropRate给出上述魔法属性的出现概率
			失败时返回零
******************************************************************************/
BOOL KItemGenerator::GMA_GetLevelAndDropRate(int nType)
{
	//TODO: 可能需要对nType进行转换, 从装备类型值变成 m_BPTLib.m_CMAT 数组的索引

	for (int i = 0; i < 2; i++)		// 0: 前缀, 1: 后缀
	{
		for (int n = 0; n < m_GMA_nCount[i]; n++)
		{
			const KMAGICATTRIB_TABFILE* pMATF;
			pMATF = GetMARecord(m_GMA_naryMA[i][n]);
			if (pMATF == NULL)
				{ _ASSERT(FALSE); return FALSE; }
			m_GMA_naryLevel[i][n] = pMATF->m_nLevel;
			m_GMA_naryDropRate[i][n] = pMATF->m_DropRate[nType];		// 可优化
		}
	}
	return TRUE;
}

/******************************************************************************
	功能:	从全部可用的魔法属性中选出符合条件的魔法
	入口:	nPos: 0: 选前缀, 1: 选后缀
			nLevel: 对魔法属性的等级要求
			nLucky: 运气值
			m_GMA_naryMA数组: 给出全部可用的魔法属性的索引值
	出口:	*pINA 给出所选魔法的核心参数
******************************************************************************/
void KItemGenerator::GMA_ChooseMA(int nPos, int nLevel, int nLucky, KItemNormalAttrib* pINA)
{
	_ASSERT(pINA != NULL);

	// 赋初值
	pINA->nAttribType = 0;
	pINA->nValue[0] = 0;
	pINA->nValue[1] = 0;
	pINA->nValue[2] = 0;

	// 筛选出全部可用的魔法属性
	int nDropRate = GetRandomNumber(0, 100) - nLucky;
	int nCount = GMA_GetCandidateMA(nPos, nLevel, nDropRate);
	if (nCount > 0)
	{
		int nFinal = GetRandomNumber(0, nCount);
		int nMAi = m_GMA_naryCandidateMA[nFinal];
		int nMA = m_GMA_naryMA[nPos][nMAi];
		if (nMA == -1)	// 随机出重复的属性了
		{
			pINA->nAttribType = 0;
			return;
		}
		m_GMA_naryMA[nPos][nMAi] = -1;		// 置为-1,表示该MA已用
		const KMAGICATTRIB_TABFILE* pMATF = GetMARecord(nMA);
		if (NULL != pMATF)
		{
			const KMACP* pMACP = &(pMATF->m_MagicAttrib);
			pINA->nAttribType = pMACP->nPropKind;
			pINA->nValue[0] = GetRandomNumber(pMACP->aryRange[0].nMin, pMACP->aryRange[0].nMax);
			pINA->nValue[1] = GetRandomNumber(pMACP->aryRange[1].nMin, pMACP->aryRange[1].nMax);
			pINA->nValue[2] = GetRandomNumber(pMACP->aryRange[2].nMin, pMACP->aryRange[2].nMax);
		}
	}
}

/******************************************************************************
	功能:	获取指定的魔法属性记录
	入口:	i: 指定记录
	出口:	成功时返回指向该记录的指针
			失败时返回NULL
******************************************************************************/
const KMAGICATTRIB_TABFILE* KItemGenerator::GetMARecord(int i) const
{
	return m_BPTLib.GetMARecord(i);
}

/******************************************************************************
	功能:	从全部可用的魔法属性中选出符合条件的魔法
	入口:	nPos: 0: 选前缀, 1: 选后缀
			nLevel: 对魔法属性的等级要求
			nDropRate: 选出出现概率大于此值的魔法属性
			m_GMA_naryMA数组: 给出全部可用的魔法属性的索引值
			m_GMA_naryLevel数组: 给出上述魔法属性的等级要求
			m_GMA_naryDropRate数组: 给出上述魔法属性的出现概率
	出口:	m_GMA_naryCandidateMA数组给出符合条件的魔法
	说明:	若 n = m_GMA_naryMA[nPos][m_GMA_naryCandidateMA[i]]
			则 GetMARecord(n) 返回指向魔法属性结构的指针
******************************************************************************/
int KItemGenerator::GMA_GetCandidateMA(int nPos, int nLevel, int nDropRate)
{
	int nCount = 0;
	for (int i = 0; i < m_GMA_nCount[nPos]; i++)
	{
		if (m_GMA_naryMA[nPos][i] != -1 &&		// -1表示该MA已用过
			m_GMA_naryLevel[nPos][i] == nLevel &&
			m_GMA_naryDropRate[nPos][i] >= nDropRate)
		{
			m_GMA_naryCandidateMA[nCount++] = i;
		}
	}
	return nCount;
}

BOOL KItemGenerator::GetMedicineCommonAttrib(IN int nDetailType, IN int nLevel, IN OUT KItem* pItem)
{
	if (NULL == pItem)
	{
		KASSERT(FALSE);
		return FALSE;
	}


	const KBASICPROP_MEDICINE* pMed = NULL;
	pMed = m_BPTLib.FindMedicine(nDetailType, nLevel);
	if (NULL == pMed)
	{
		_ASSERT(FALSE);
		return FALSE;
	}
	*pItem = *pMed;
	return TRUE;
}

/******************************************************************************
	功能：	随机生成黄金装备装备
	入口:	nCondition: 前提条件，(Emf+Pmf)*Level的运算结果
			pItem: 装备项目指针。
	出口:	成功时返回真, 相关数据在 pItem 所指对象中给出
			失败时返回假
******************************************************************************/
// Add by flying on May.30.2003
// Try to get a "Gold Item" if possible.
BOOL KItemGenerator::GetGoldItemByRandom(IN int nCondition, OUT KItem * pItem)
{
	BOOL bResult = FALSE;
	const KBASICPROP_EQUIPMENT_GOLD* pArray[MAX_ITEM];
	int i = 0;
	int j = 0;
	int nIndex = 0;

	::memset(pArray, 0, sizeof(KBASICPROP_EQUIPMENT_GOLD *) * MAX_ITEM);

	for (i = 0; i < MAX_ITEM; i++)
	{
		const KBASICPROP_EQUIPMENT_GOLD* pTemp = m_BPTLib.GetGoldItemRecord(i);
		if (pTemp == 0)
			break;
		if (pTemp->m_nRarity < nCondition)
			pArray[j] = pTemp;
	}

	if (j == 0)
		goto Exit0;			 
	nIndex = GetRandomNumber(0, j);	
	*pItem = *pArray[nIndex];
Exit0:
	return FALSE;
}

/******************************************************************************
	功能：	生成指定黄金装备装备
	入口:	nCondition: 前提条件，(Emf+Pmf)*Level的运算结果
			pItem: 装备项目指针。
	出口:	成功时返回真, 相关数据在 pItem 所指对象中给出
			失败时返回假
******************************************************************************/
// Add by flying on May.30.2003
// Try to get a "Gold Item" if possible.
BOOL KItemGenerator::GetGoldItemByIndex(IN int nIndex, OUT KItem * pItem)
{
	BOOL bResult = FALSE;
	int i = 0;
	int j = 0;
	int nCount = 0;

	nCount = m_BPTLib.GetGoldItemNumber();
	const KBASICPROP_EQUIPMENT_GOLD* pTemp = m_BPTLib.GetGoldItemRecord(i);
	if (nIndex >= nCount)
		goto Exit0;
	if (pTemp == NULL)
		goto Exit0;
	*pItem = *pTemp;
	bResult = TRUE;
Exit0:
	return bResult;
}

BOOL KItemGenerator::GetEquipmentCommonAttrib(IN int nDetailType, IN int nParticularType, IN int nLevel, IN int nSeries, IN OUT KItem* pItem)
{
	if (NULL == pItem)
	{
		KASSERT(FALSE);
		return FALSE;
	}

	KLibOfBPT* pLibrary = GetVersionLibrary(pItem->m_GeneratorParam.nVersion);
	if (!pLibrary)
		return FALSE;
	const KBASICPROP_EQUIPMENT* pEqu =
		pLibrary->FindEquipment(nDetailType, nParticularType, nLevel);
	if (NULL == pEqu)
		{ _ASSERT(FALSE); return FALSE; }
		// 运行至此的直接原因: 只有n种装备, 而上面 i 的值在[0,n-1]之外
		// 检查3点: nParticularType 有误?
		//			nLevel 有误?
		//			原始的tab file有误导致 m_BPTLib.m_BPTEquipment 所
		//			管理的数据有问题?
	pItem->SetAttrib_CBR(pEqu);
	pItem->SetSeries(nSeries);
	return TRUE;
}

/******************************************************************************
	功能：	生成随机装备
	入口:	nDetailType: 具体类别, 如近程武器、暗器、......
			nParticularType: 详细类别
			nSeriesReq: 五行属性
			nLevel: 等级
			pnaryMALevel: 魔法属性等级数组[6]
			nLucky: 运气值
	出口:	成功时返回非零, 相关数据在 pItem 所指对象中给出
			失败时返回零
******************************************************************************/
BOOL KItemGenerator::Gen_ExistEquipment(IN int nDetailType,
								   IN int nParticularType,
								   IN int nSeriesReq,
								   IN int nLevel,
								   IN const int* pnaryMALevel,
								   IN int nLucky,
								   IN int nVersion,
								   IN OUT KItem* pItem)
{
	_ASSERT(this != NULL);
	_ASSERT(pItem != NULL);

	BOOL bEC = FALSE;

	pItem->m_GeneratorParam.nLuck = nLucky;
	if (pnaryMALevel)
		memcpy(pItem->m_GeneratorParam.nGeneratorLevel, pnaryMALevel, sizeof(int) * 6);
	SetRandomSeed(pItem->m_GeneratorParam.uRandomSeed);
	pItem->m_GeneratorParam.nVersion = nVersion == -1 ? m_nCurrentVersion : nVersion;
	int nGameVersion = pItem->m_GeneratorParam.nVersion;

	// 根据入口参数, 确定装备的基本数据
	KLibOfBPT* pLibrary = GetVersionLibrary(pItem->m_GeneratorParam.nVersion);
	if (!pLibrary)
		return FALSE;
	const KBASICPROP_EQUIPMENT* pEqu =
		pLibrary->FindEquipment(nDetailType, nParticularType, nLevel);
	if (NULL == pEqu)
		{ _ASSERT(FALSE); return bEC; }
		// 运行至此的直接原因: 只有n种装备, 而上面 i 的值在[0,n-1]之外
		// 检查3点: nParticularType 有误?
		//			nLevel 有误?
		//			原始的tab file有误导致 m_BPTLib.m_BPTEquipment 所
		//			管理的数据有问题?
	pItem->SetAttrib_CBR(pEqu);
	pItem->SetSeries(nSeriesReq);

	if (NULL == pnaryMALevel || pEqu->m_nSeries != 0)
		return TRUE;
	// 确定装备的魔法属性
	KItemNormalAttrib	sMA[6];	// 道具的魔法属性
	int nClass = -1;
	for (int n = 0; n < 6; n++)
		if (pEqu->m_aryPropReq[n].nType == magic_requireseries)
			nClass = pEqu->m_aryPropReq[n].nPara;
	bEC = Gen_MagicAttrib(nDetailType, pnaryMALevel, nSeriesReq, nLucky, sMA, nGameVersion, pLibrary, nClass, nLevel);
	if (bEC)
		pItem->SetAttrib_MA(sMA);

	return bEC;
}

// 原表目前每件玉石生成一条属性，类型与数值均由物品种子决定。
BOOL KItemGenerator::InitGemRules()
{
	m_GemRules.clear();
	m_SocketGemDetails.clear();
	m_SocketRules.clear();
	KTabFile Table;
	if (!Table.Load("\\settings\\item\\打孔镶嵌\\宝石属性表_sc.txt"))
	{
		g_DebugLog("[Gem] Original gem property table is unavailable.");
		return FALSE;
	}
	for (int nRow = 2; nRow <= Table.GetHeight(); nRow++)
	{
		int nGemID, nGenre, nDetail, nParticular, nLevel, nGenerateCount;
		Table.GetInteger(nRow, 1, 0, &nGemID);
		Table.GetInteger(nRow, 2, -1, &nGenre);
		Table.GetInteger(nRow, 3, -1, &nDetail);
		Table.GetInteger(nRow, 4, -1, &nParticular);
		Table.GetInteger(nRow, 5, -1, &nLevel);
		Table.GetInteger(nRow, 8, 0, &nGenerateCount);
		if (nGemID <= 0 || nGemID > 65535 || m_SocketGemDetails.find(nGemID) != m_SocketGemDetails.end() ||
			nGenre != item_materials || nDetail < 0 || nParticular != 0 ||
			nLevel != 0 || nGenerateCount != 1 || m_GemRules.find(nDetail) != m_GemRules.end())
		{
			g_DebugLog("[Gem] Unsupported or duplicate original gem row: row=%d", nRow);
			return FALSE;
		}
		GemRule Rule;
		ZeroMemory(&Rule, sizeof(Rule));
		int nPrevious = -1;
		for (int i = 0; i < 10; i++)
		{
			GemProperty& Property = Rule.Properties[i];
			Table.GetInteger(nRow, 9 + i * 5, 0, &Property.nType);
			if (Property.nType <= 0)
				break;
			Table.GetInteger(nRow, 10 + i * 5, 0, &Property.nMin);
			Table.GetInteger(nRow, 11 + i * 5, 0, &Property.nMax);
			Table.GetInteger(nRow, 12 + i * 5, 100, &Property.nThreshold);
			Table.GetInteger(nRow, 13 + i * 5, 0, &Property.nDecorationExtra);
			if (Property.nDecorationExtra < 0 || Property.nMin < 0 || Property.nMax < Property.nMin ||
				Property.nThreshold < nPrevious || Property.nThreshold > 100)
			{
				g_DebugLog("[Gem] Invalid original gem property: row=%d property=%d", nRow, i + 1);
				return FALSE;
			}
			nPrevious = Property.nThreshold;
			Rule.nCount++;
		}
		if (!Rule.nCount || nPrevious != 100)
			return FALSE;
		m_GemRules[nDetail] = Rule;
		m_SocketGemDetails[nGemID] = nDetail;
	}
	KTabFile Sockets;
	if (!Sockets.Load("\\settings\\item\\打孔镶嵌\\孔属性表_sc.txt"))
		return FALSE;
	for (int nRow = 2; nRow <= Sockets.GetHeight(); nRow++)
	{
		int nID;
		Sockets.GetInteger(nRow, 1, 0, &nID);
		SocketRule Rule;
		Sockets.GetInteger(nRow, 7, 0, &Rule.nQuality);
		if (nID <= 0 || nID > 65535 || (Rule.nQuality != 0 && Rule.nQuality != 1) ||
			m_SocketRules.find(nID) != m_SocketRules.end())
			return FALSE;
		char szPattern[2048];
		Sockets.GetString(nRow, 6, "", szPattern, sizeof(szPattern));
		if (szPattern[0])
		{
			// 原表的激活格式为 *.3.[详细类别列表或区间].*；不按颜色名称推断。
			if (strncmp(szPattern, "*.3.[", 5) != 0)
				return FALSE;
			const char* pPart = szPattern + 5;
			while (*pPart)
			{
				char* pEnd;
				long nFirst = strtol(pPart, &pEnd, 10);
				if (pEnd == pPart || nFirst < 0 || nFirst > 65535)
					return FALSE;
				long nLast = nFirst;
				if (*pEnd == '-')
				{
					pPart = pEnd + 1;
					nLast = strtol(pPart, &pEnd, 10);
					if (pEnd == pPart || nLast < nFirst || nLast > 65535)
						return FALSE;
				}
				for (int nDetail = nFirst; nDetail <= nLast; nDetail++)
					Rule.ActivatedDetails.insert(nDetail);
				if (strcmp(pEnd, "].*") == 0)
					break;
				if (*pEnd != ',')
					return FALSE;
				pPart = pEnd + 1;
			}
		}
		m_SocketRules[nID] = Rule;
	}
	g_DebugLog("[Gem] Original gem property rules loaded: count=%d", (int)m_GemRules.size());
	return !m_GemRules.empty();
}

BOOL KItemGenerator::GenerateGemAttrib(int nDetail, DWORD dwSeed, KItemNormalAttrib* pAttrib,
	int* pnDecorationExtra) const
{
	if (!pAttrib)
		return FALSE;
	ZeroMemory(pAttrib, sizeof(*pAttrib));
	if (pnDecorationExtra)
		*pnDecorationExtra = 0;
	std::map<int, GemRule>::const_iterator it = m_GemRules.find(nDetail);
	if (it == m_GemRules.end())
		return FALSE;
	UINT uSeed = dwSeed;
	int nRoll = NextItemRandom(uSeed, 0, 99);
	for (int i = 0; i < it->second.nCount; i++)
	{
		const GemProperty& Property = it->second.Properties[i];
		if (Property.nThreshold >= nRoll)
		{
			pAttrib->nAttribType = Property.nType;
			pAttrib->nValue[0] = NextItemRandom(uSeed, Property.nMin, Property.nMax);
			pAttrib->nValue[1] = -1;
			pAttrib->nValue[2] = -1;
			if (pnDecorationExtra)
				*pnDecorationExtra = Property.nDecorationExtra;
			return TRUE;
		}
	}
	return FALSE;
}

int KItemGenerator::GetSocketGemDetail(int nGemID) const
{
	std::map<int, int>::const_iterator it = m_SocketGemDetails.find(nGemID);
	return it == m_SocketGemDetails.end() ? -1 : it->second;
}

BOOL KItemGenerator::IsValidSocket(const KItemSocket& Socket) const
{
	if (!Socket.TypeID)
		return !Socket.GemID && !Socket.Seed;
	if (m_SocketRules.find(Socket.TypeID) == m_SocketRules.end())
		return FALSE;
	return Socket.GemID ? GetSocketGemDetail(Socket.GemID) >= 0 : !Socket.Seed;
}

BOOL KItemGenerator::GenerateSocketGem(int nGemID, DWORD dwSeed, KItem* pResult, int nVersion)
{
	int nDetail = GetSocketGemDetail(nGemID);
	if (!pResult || nDetail < 0)
		return FALSE;
	pResult->m_GeneratorParam.uRandomSeed = dwSeed;
	return Gen_Material(nDetail, 0, 0, nVersion, pResult);
}

BOOL KItemGenerator::GetSocketAttrib(const KItemSocket& Socket, KItemNormalAttrib* pAttrib,
	BOOL* pbActivated) const
{
	if (pbActivated)
		*pbActivated = FALSE;
	if (!pAttrib)
		return FALSE;
	ZeroMemory(pAttrib, sizeof(*pAttrib));
	if (!IsValidSocket(Socket) || !Socket.GemID)
		return FALSE;
	int nDetail = GetSocketGemDetail(Socket.GemID);
	int nExtra = 0;
	if (!GenerateGemAttrib(nDetail, Socket.Seed, pAttrib, &nExtra))
		return FALSE;
	const SocketRule& Rule = m_SocketRules.find(Socket.TypeID)->second;
	if (Rule.ActivatedDetails.find(nDetail) != Rule.ActivatedDetails.end())
	{
		// 极品仅将装饰附加值翻倍；宝石原属性与独立种子保持不变。
		pAttrib->nValue[0] += nExtra * (Rule.nQuality == 1 ? 2 : 1);
		if (pbActivated)
			*pbActivated = TRUE;
	}
	// 宝石表生成的是单值属性，-1仅供物品说明占位，不能作为第二、三参数应用。
	pAttrib->nValue[1] = 0;
	pAttrib->nValue[2] = 0;
	return TRUE;
}

// 普通材料按原资源的类别、详细类别、等级和版本生成，不占用任务专用物品类型。
BOOL KItemGenerator::Gen_Material(IN int nDetailType,
								  IN int nParticularType,
								  IN int nLevel,
								  IN int nVersion,
								  IN OUT KItem* pItem)
{
	if (!pItem || nDetailType < 0 || nParticularType < 0 || nLevel < 0)
		return FALSE;
	KLibOfBPT* pLibrary = GetVersionLibrary(nVersion);
	if (!pLibrary)
		return FALSE;
	const KBASICPROP_MEDMATERIAL* pMaterial = pLibrary->FindMedMaterial(nDetailType, nParticularType, nLevel);
	if (!pMaterial)
		return FALSE;

	UINT uSeed = pItem->m_GeneratorParam.uRandomSeed;
	ZeroMemory(&pItem->m_GeneratorParam, sizeof(pItem->m_GeneratorParam));
	pItem->m_GeneratorParam.nVersion = nVersion == -1 ? m_nCurrentVersion : nVersion;
	*pItem = *pMaterial;
	pItem->SetDurability(-1);
	pItem->SetStackCount(1);
	std::map<int, GemRule>::const_iterator it = m_GemRules.find(nDetailType);
	if (it != m_GemRules.end() && nParticularType == 0 && nLevel == 0)
	{
		pItem->m_GeneratorParam.uRandomSeed = uSeed;
		KItemNormalAttrib Attributes[6];
		ZeroMemory(Attributes, sizeof(Attributes));
		if (!GenerateGemAttrib(nDetailType, uSeed, &Attributes[0]))
			return FALSE;
		pItem->SetMagicAttrib(Attributes);
	}
	return TRUE;
}

// 返回原模板脚本与入口消耗规则，不在这里判断具体道具业务。
const char* KItemGenerator::GetUseItemScript(KItem& Data, BOOL* pbConsumeOnUse)
{
	*pbConsumeOnUse = FALSE;
	if (Data.GetGenre() == item_townportal)
	{
		KLibOfBPT* pLibrary = GetVersionLibrary(0);
		const KBASICPROP_TOWNPORTAL* pPortal = pLibrary ? pLibrary->GetTownPortalRecord(0) : NULL;
		return pPortal ? pPortal->m_szScript : NULL;
	}
	const KBASICPROP_NORMALITEM* pTemplate = GetNormalItemTemplate(Data.GetGenre(),
		Data.GetDetailType(), Data.GetParticular(), Data.GetLevel(), Data.m_GeneratorParam.nVersion);
	if (!pTemplate)
		return NULL;
	*pbConsumeOnUse = pTemplate->m_bConsumeOnUse;
	return pTemplate->m_szScript;
}

const KBASICPROP_NORMALITEM* KItemGenerator::GetNormalItemTemplate(int nGenre, int nDetail,
	int nParticular, int nLevel, int nVersion)
{
	KLibOfBPT* pLibrary = GetVersionLibrary(nVersion);
	return pLibrary ? pLibrary->FindNormalItem(nGenre, nDetail, nParticular, nLevel) : NULL;
}

BOOL KItemGenerator::Gen_NormalItem(IN int nGenre,
									 IN int nDetailType,
									 IN int nParticularType,
									 IN int nLevel,
									 IN int nVersion,
									 IN OUT KItem* pItem)
{
	if (!pItem || (nGenre != item_magicorscript && nGenre != item_skillbook) ||
		nDetailType < 0 || nParticularType < 0 || nLevel < 0)
		return FALSE;
	KLibOfBPT* pLibrary = GetVersionLibrary(nVersion);
	if (!pLibrary)
		return FALSE;
	const KBASICPROP_NORMALITEM* pTemplate =
		pLibrary->FindNormalItem(nGenre, nDetailType, nParticularType, nLevel);
	if (!pTemplate)
		return FALSE;

	// 这里只恢复原表的共同模板，不执行脚本物品或学习技能书。
	ZeroMemory(&pItem->m_GeneratorParam, sizeof(pItem->m_GeneratorParam));
	pItem->m_GeneratorParam.nVersion = nVersion == -1 ? m_nCurrentVersion : nVersion;
	*pItem = *pTemplate;
	pItem->SetDurability(-1);
	pItem->SetStackCount(1);
	return TRUE;
}

BOOL KItemGenerator::IsTradeAllowed(const KItem& Data)
{
	if (Data.GetGenre() == item_task)
	{
		// 与 Gen_Quest 使用同一有效模板库，不能按第4类一概拒绝铜钱等可交易物品。
		const KBASICPROP_QUEST* pTemplate = m_BPTLib.GetQuestRecord(Data.GetDetailType());
		return pTemplate && pTemplate->m_bCanTrade;
	}
	if (Data.GetGenre() == item_magicorscript || Data.GetGenre() == item_skillbook)
	{
		const KBASICPROP_NORMALITEM* pTemplate = GetNormalItemTemplate(Data.GetGenre(),
			Data.GetDetailType(), Data.GetParticular(), Data.GetLevel(), Data.m_GeneratorParam.nVersion);
		return pTemplate && pTemplate->m_bCanTrade;
	}
	return Data.GetGenre() >= item_equip && Data.GetGenre() < item_number;
}

BOOL KItemGenerator::Gen_Quest(IN int nDetailType, IN OUT KItem* pItem)
{
	_ASSERT(this != NULL);
	_ASSERT(pItem != NULL);

	BOOL bEC = FALSE;
	ZeroMemory(&pItem->m_GeneratorParam, sizeof(pItem->m_GeneratorParam));
	
	const KBASICPROP_QUEST* pQuest = NULL;
	pQuest = m_BPTLib.GetQuestRecord(nDetailType);
	if (NULL == pQuest)
	{
		_ASSERT(FALSE);
		return bEC;
	}
	*pItem = *pQuest;
	pItem->m_GeneratorParam.nVersion = m_nCurrentVersion;
	pItem->SetDurability(-1);
	pItem->SetStackCount(1);
	return TRUE;
}

BOOL KItemGenerator::Gen_TownPortal(IN OUT KItem* pItem)
{
	_ASSERT(this != NULL);
	_ASSERT(pItem != NULL);

	// 回城符使用基础版本。客户端生成器及正式服同步均将版本置为 0，
	// 不能沿用装备当前版本；旧存档中误写的版本也在生成时恢复。
	KLibOfBPT* pLibrary = GetVersionLibrary(0);
	if (!pLibrary || pLibrary->GetTownPortalRecordNumber() <= 0)
		return FALSE;
	const KBASICPROP_TOWNPORTAL* pPortal = pLibrary->GetTownPortalRecord(0);
	if (!pPortal || pPortal->m_nItemGenre != item_townportal ||
		!pPortal->m_szName[0] || pPortal->m_nWidth <= 0 || pPortal->m_nHeight <= 0)
		return FALSE;

	ZeroMemory(&pItem->m_GeneratorParam, sizeof(pItem->m_GeneratorParam));
	pItem->SetDurability(-1);
	*pItem = *pPortal;
	return TRUE;
}

#ifdef _SERVER
BOOL KItemGenerator::InitReforge()
{
	m_bReforgeEnabled = FALSE;
	KIniFile Config;
	if (!Config.Load("\\ServerConfig\\EquipmentReforge.ini"))
	{
		g_DebugLog("[Reforge] Configuration missing; reforging disabled.");
		return FALSE;
	}
	int nEnabled = 0;
	Config.GetInteger("Reforge", "Enabled", 0, &nEnabled);
	Config.GetInteger("Reforge", "BaseSuccessRate", 10, &m_nReforgeBaseRate);
	Config.GetInteger("Reforge", "LuckMin", 5, &m_nReforgeLuckMin);
	Config.GetInteger("Reforge", "LuckMax", 10, &m_nReforgeLuckMax);
	if (m_nReforgeBaseRate < 0 || m_nReforgeBaseRate > 100 ||
		m_nReforgeLuckMin < 1 || m_nReforgeLuckMin > m_nReforgeLuckMax ||
		m_nReforgeLuckMax > 100 || (nEnabled != 0 && nEnabled != 1))
	{
		g_DebugLog("[Reforge] Invalid configuration; reforging disabled.");
		return FALSE;
	}
	m_bReforgeEnabled = nEnabled != 0;
	g_DebugLog("[Reforge] enabled=%d base=%d luck=%d..%d", m_bReforgeEnabled,
		m_nReforgeBaseRate, m_nReforgeLuckMin, m_nReforgeLuckMax);
	return TRUE;
}

BOOL KItemGenerator::GetReforgeRule(int* pnBaseRate, int* pnLuckMin, int* pnLuckMax)
{
	*pnBaseRate = m_nReforgeBaseRate;
	*pnLuckMin = m_nReforgeLuckMin;
	*pnLuckMax = m_nReforgeLuckMax;
	return m_bReforgeEnabled;
}

const KBASICPROP_EQUIPMENT* KItemGenerator::GetEquipmentTemplate(int nDetail,
	int nParticular, int nLevel, int nVersion)
{
	KLibOfBPT* pLibrary = GetVersionLibrary(nVersion == -1 ? m_nCurrentVersion : nVersion);
	return pLibrary ? pLibrary->FindEquipment(nDetail, nParticular, nLevel) : NULL;
}

// 根据客户端的MSVC随机序列反求种子，不发送客户端不能还原的独立属性。
// 以范围最大的属性为锚点，逆推种子，再完整校验所有属性；不改变游戏随机序列。
static BOOL FindReforgeSeed(const KEQCP_BASIC* pBasic, const int* pValues, int nLuck, UINT* puSeed)
{
	int nAnchor = -1;
	int nAnchorStep = 0;
	int nSteps = 0;
	int nLargest = 0;
	for (int i = 0; i < 10; i++)
	{
		int nRange = pBasic[i].sRange.nMax - pBasic[i].sRange.nMin + 1;
		if (nRange <= 0 || nRange > 32768 || pValues[i] < pBasic[i].sRange.nMin ||
			pValues[i] > pBasic[i].sRange.nMax)
			return FALSE;
		if (i < 5 && !nLuck)
		{
			if (pValues[i] != pBasic[i].sRange.nMin)
				return FALSE;
			continue;
		}
		nSteps++;
		if (nRange > nLargest)
		{
			nLargest = nRange;
			nAnchor = i;
			nAnchorStep = nSteps;
		}
	}
	if (nAnchor < 0)
		return FALSE;
	// 214013 * 0xB9B33155 == 1 (mod 2^32).
	for (UINT nHigh = pValues[nAnchor] - pBasic[nAnchor].sRange.nMin;
		nHigh < 32768; nHigh += nLargest)
	{
		for (UINT nLow = 0; nLow < 65536; nLow++)
		{
			UINT uSeed = (nHigh << 16) | nLow;
			for (int n = 0; n < nAnchorStep; n++)
				uSeed = (uSeed - 2531011U) * 0xB9B33155U;
			UINT uState = uSeed;
			BOOL bMatch = TRUE;
			for (int j = 0; j < 10; j++)
			{
				if (j < 5 && !nLuck)
					continue;
				uState = uState * 214013U + 2531011U;
				int nRange = pBasic[j].sRange.nMax - pBasic[j].sRange.nMin + 1;
				int nValue = pBasic[j].sRange.nMin + ((uState >> 16) & 0x7fff) % nRange;
				if (nValue != pValues[j])
				{
					bMatch = FALSE;
					break;
				}
			}
			if (bMatch)
			{
				*puSeed = uSeed;
				return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL KItemGenerator::PrepareReforge(const KItem& Source, int nParticular, int nLevel,
	BOOL bImprove, BOOL bFull, KItem* pResult)
{
	const KBASICPROP_EQUIPMENT* pOld = GetEquipmentTemplate(Source.m_CommonAttrib.nDetailType,
		Source.m_CommonAttrib.nParticularType, Source.m_CommonAttrib.nLevel, Source.m_GeneratorParam.nVersion);
	const KBASICPROP_EQUIPMENT* pNew = GetEquipmentTemplate(Source.m_CommonAttrib.nDetailType,
		nParticular, nLevel, Source.m_GeneratorParam.nVersion);
	if (!pResult || !pOld || !pNew || !pOld->m_nSeries || !pNew->m_nSeries ||
		pOld->m_nSeries != pNew->m_nSeries || pOld->m_nWidth != pNew->m_nWidth ||
		pOld->m_nHeight != pNew->m_nHeight || Source.m_btGenerationFlags != 0)
		return FALSE;

	int nLuck = 0;
	int Values[10];
	int Slots[5];
	int nSlots = 0;
	for (int i = 0; i < 10; i++)
	{
		if (pOld->m_aryPropBasic[i].nType != pNew->m_aryPropBasic[i].nType)
			return FALSE;
		if (i < 5)
		{
			Values[i] = pNew->m_aryPropBasic[i].sRange.nMax;
			if (Values[i] != pNew->m_aryPropBasic[i].sRange.nMin)
				nLuck = 1;
		}
		else
		{
			Values[i] = Source.m_aryBaseAttrib[i].nValue[0];
			if (Values[i] < pNew->m_aryPropBasic[i].sRange.nMin ||
				Values[i] > pNew->m_aryPropBasic[i].sRange.nMax)
				return FALSE;
			if (pNew->m_aryPropBasic[i].nType && Values[i] < pNew->m_aryPropBasic[i].sRange.nMax)
				Slots[nSlots++] = i;
			if (bFull)
				Values[i] = pNew->m_aryPropBasic[i].sRange.nMax;
		}
	}
	if (bImprove && !bFull && nSlots)
	{
		// 原资源给出上下限；每一项保底原值，至少一项提升，不允许属性回落。
		int nGuaranteed = Slots[g_Random(nSlots)];
		for (int i = 5; i < 10; i++)
		{
			int nMin = Values[i] + (i == nGuaranteed ? 1 : 0);
			int nMax = pNew->m_aryPropBasic[i].sRange.nMax;
			Values[i] = nMin + g_Random(nMax - nMin + 1);
		}
	}
	UINT uSeed = 0;
	if (!FindReforgeSeed(pNew->m_aryPropBasic, Values, nLuck, &uSeed))
	{
		g_DebugLog("[Reforge] Cannot reconstruct attributes: detail=%d particular=%d level=%d version=%d",
			Source.m_CommonAttrib.nDetailType, nParticular, nLevel, Source.m_GeneratorParam.nVersion);
		return FALSE;
	}
	*pResult = Source;
	pResult->m_GeneratorParam.uRandomSeed = uSeed;
	if (!Gen_ExistEquipment(Source.m_CommonAttrib.nDetailType, nParticular, Source.m_CommonAttrib.nSeries, nLevel,
		Source.m_GeneratorParam.nGeneratorLevel, nLuck, Source.m_GeneratorParam.nVersion, pResult))
		return FALSE;
	for (int i = 0; i < 10; i++)
	{
		if (pResult->m_aryBaseAttrib[i].nValue[0] != Values[i])
			return FALSE;
	}
	int nDur = Source.m_nCurrentDur;
	if (pResult->GetMaxDurability() >= 0 && nDur > pResult->GetMaxDurability())
		nDur = pResult->GetMaxDurability();
	pResult->SetDurability(nDur);
	for (int i = 0; i < 4; i++)
		pResult->SetSocket(i, *Source.GetSocket(i));
	pResult->m_btStateFlags |= 1;
	pResult->m_btBindingMode = 2;
	return TRUE;
}
#endif
