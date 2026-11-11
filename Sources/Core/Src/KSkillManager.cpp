 
#include "KCore.h"
#include "KSkillManager.h"
#include "MyAssert.H"
#include "KThiefSkill.h"

unsigned long g_ulSkillCount = 0;
KSkillManager g_SkillManager;

KSkillManager::KSkillManager()
{
	memset(m_SkillInfo, 0, sizeof(m_SkillInfo));
	memset(m_pOrdinSkill, 0, sizeof(m_pOrdinSkill));
	memset(m_PlayerRules, 0, sizeof(m_PlayerRules));
	memset(m_PlayerBuffRules, 0, sizeof(m_PlayerBuffRules));
	m_nHitSkillExpRate = 1;
	m_nSupportSkillExpRate = 1;
}

// Add By Freeway Chen in 2003.6.6
KSkillManager::~KSkillManager()
{
    unsigned long i = 0;
    unsigned long j = 0;
    
    for (i = 0; i < MAX_SKILL; i++)
    {
        for (j = 0; j < MAX_SKILLLEVEL; j++)
        {
            if (m_pOrdinSkill[i][j])
            {
                delete m_pOrdinSkill[i][j];
                m_pOrdinSkill[i][j] = NULL;
            }
        }
    }
}

BOOL KSkillManager::Init()
{
	ZeroMemory(m_EquipmentSkillSeries, sizeof(m_EquipmentSkillSeries));
	KTabFile SeriesTable;
	if (SeriesTable.Load("\\settings\\npc\\player\\skillseries.txt"))
	{
		for (int nRow = 2; nRow <= SeriesTable.GetHeight(); nRow++)
		{
			int nSeries = -1;
			char szSkills[2048] = {};
			SeriesTable.GetInteger(nRow, 1, -1, &nSeries);
			SeriesTable.GetString(nRow, 3, "", szSkills, sizeof(szSkills));
			if (nSeries < 0 || nSeries >= 8)
				continue;
			char* pSkill = szSkills;
			while (pSkill && *pSkill)
			{
				int nSkill = atoi(pSkill);
				if (nSkill > 0 && nSkill < MAX_SKILL)
					m_EquipmentSkillSeries[nSeries][nSkill] = TRUE;
				pSkill = strchr(pSkill, '|');
				if (pSkill)
					pSkill++;
			}
		}
	}

    int i = 0;
    int j = 0;
    for (i = 0; i < MAX_SKILL; i++)
    {
        for (j = 0; j < MAX_SKILLLEVEL; j++)
        {
            m_pOrdinSkill[i][j] = NULL;
        }
    }

	memset(m_SkillInfo, 0,  sizeof(m_SkillInfo));

	if (!LoadPlayerRules() || !LoadPlayerBuffRules())
		return FALSE;

	//	Load OrdinSkill Info 
	int nSkillNum = g_OrdinSkillsSetting.GetHeight() - 1;
	if (nSkillNum <= 0 ) return FALSE;
	
	for (i = 0; i < nSkillNum; i ++)
	{
		int nSkillId = -1;
		int nSkillStyle = -1;
		int nSkillMaxLevel = 0;
		g_OrdinSkillsSetting.GetInteger(i + 2, "SkillId", -1, &nSkillId);
		g_OrdinSkillsSetting.GetInteger(i + 2, "SkillStyle", -1, &nSkillStyle);
		g_OrdinSkillsSetting.GetInteger(i + 2, "MaxLevel", 20, &nSkillMaxLevel);
		if (nSkillId <= 0 || nSkillId > MAX_SKILL || nSkillMaxLevel < 0)
		{
			printf("[Skills] Invalid skill ID %d at row %d (limit %d).\n", nSkillId, i + 2, MAX_SKILL);
			continue;
		}
		
		if (nSkillId > 0 && nSkillStyle >= 0)
		{
			m_SkillInfo[nSkillId - 1].m_nSkillStyle = nSkillStyle;
			m_SkillInfo[nSkillId - 1].m_nTabFileRowId = i + 2;
			m_SkillInfo[nSkillId - 1].m_ulMaxSkillLevel = nSkillMaxLevel;
		}
		else
		{
			m_SkillInfo[nSkillId - 1].m_nSkillStyle = -1;
			m_SkillInfo[nSkillId - 1].m_nTabFileRowId = 0;
			m_SkillInfo[nSkillId - 1].m_ulMaxSkillLevel = 0;
		}
	}
	KTabFile ThiefSkillTab;
	if (ThiefSkillTab.Load(THIEFSKILL_SETTINGFILE))
	{
		int nSkillId = 0;
		ThiefSkillTab.GetInteger(2, "SkillId", 400, &nSkillId);
		if (nSkillId <= 0 || nSkillId > MAX_SKILL)
			return FALSE;
		m_SkillInfo[nSkillId - 1].m_nSkillStyle = SKILL_SS_Thief;
		m_SkillInfo[nSkillId - 1].m_nTabFileRowId = 2;
		m_SkillInfo[nSkillId - 1].m_ulMaxSkillLevel = 1;
	}
	return TRUE;
}

ISkill*	KSkillManager::InstanceSkill( unsigned long ulSkillID, unsigned long ulSkillLevel)
{
	ISkill *pRetSkill = NULL;

    int nStyle = GetSkillStyle(ulSkillID);
	
	switch (nStyle) // eSkillStyle
	{
	case SKILL_SS_Attack:
	case SKILL_SS_Missles:			        //	子弹类		本技能用于发送子弹类
	case SKILL_SS_Melee:
	case SKILL_SS_InitiativeNpcState:	    //	主动类		本技能用于改变当前Npc的主动状态
	case SKILL_SS_PassivityNpcState:		//	被动类		本技能用于改变Npc的被动状态
		{
            KSkill * pNewOrdinSkill = NULL;
			unsigned long ulFirstLoadLevel = 0;

            if (m_pOrdinSkill[ulSkillID - 1][ulSkillLevel - 1])
            { 
                pRetSkill = m_pOrdinSkill[ulSkillID - 1][ulSkillLevel - 1];
                goto Exit1;
            }

			pNewOrdinSkill = new KSkill;
			ulFirstLoadLevel = m_SkillInfo[ulSkillID - 1].m_ulFirstLoadLevel;

			if (!ulFirstLoadLevel)
			{
				pNewOrdinSkill->GetInfoFromTabFile(m_SkillInfo[ulSkillID - 1].m_nTabFileRowId);
				pNewOrdinSkill->LoadSkillLevelData(ulSkillLevel, m_SkillInfo[ulSkillID - 1].m_nTabFileRowId);
				m_SkillInfo[ulSkillID - 1].m_ulFirstLoadLevel = ulSkillLevel;
			}
			else
			{
				_ASSERT(m_pOrdinSkill[ulSkillID - 1][ulFirstLoadLevel - 1]);
				*pNewOrdinSkill = *(KSkill*)m_pOrdinSkill[ulSkillID - 1][ulFirstLoadLevel - 1];
				pNewOrdinSkill->LoadSkillLevelData(ulSkillLevel, m_SkillInfo[ulSkillID - 1].m_nTabFileRowId);
			}
			
			pNewOrdinSkill->SetSkillId(ulSkillID);
			pNewOrdinSkill->SetSkillLevel(ulSkillLevel);

			m_pOrdinSkill[ulSkillID - 1][ulSkillLevel - 1] = pNewOrdinSkill;
			pRetSkill = pNewOrdinSkill;
            pNewOrdinSkill = NULL;

		
        }    break;
	
	case SKILL_SS_Thief:
		{
			if (!m_pOrdinSkill[ulSkillID - 1][0])
			{
				m_pOrdinSkill[ulSkillID - 1][0] = (ISkill*)new KThiefSkill;
			}

			pRetSkill = m_pOrdinSkill[ulSkillID - 1][0];
			
			((KThiefSkill*)pRetSkill)->LoadSetting(THIEFSKILL_SETTINGFILE);

		}break;
		
	default:
		goto Exit0;
	}
    	
Exit1:
Exit0:	
	return pRetSkill;
}




const PLAYER_SKILL_RULE* KSkillManager::GetPlayerRule(int nSkill) const
{
	if (nSkill <= 0 || nSkill > MAX_SKILL || !m_PlayerRules[nSkill - 1].Loaded)
		return NULL;
	return &m_PlayerRules[nSkill - 1];
}

static BOOL ReadSkillNumberList(KIniFile& File, const char* pszSection, const char* pszKey,
	int* pValues, int nCapacity, int& nCount)
{
	char szValue[2048];
	File.GetString(pszSection, pszKey, "0:", szValue, sizeof(szValue));
	char* pEnd = NULL;
	long nSize = strtol(szValue, &pEnd, 10);
	if (pEnd == szValue || *pEnd != ':' || nSize < 0 || nSize > nCapacity)
		return FALSE;
	nCount = nSize;
	char* pCurrent = pEnd + 1;
	for (int i = 0; i < nCount; i++)
	{
		long nValue = strtol(pCurrent, &pEnd, 10);
		if (pEnd == pCurrent || nValue < 0 ||
			(i + 1 < nCount ? *pEnd != ',' : *pEnd != 0))
			return FALSE;
		pValues[i] = nValue;
		pCurrent = pEnd + 1;
	}
	return !nCount ? *pCurrent == 0 : TRUE;
}

BOOL KSkillManager::LoadPlayerRules()
{
	memset(m_PlayerRules, 0, sizeof(m_PlayerRules));
	// 沿用参考项目的原配置键；Skill120Rate为百分比，缺省1%。
	KIniFile Settings;
	Settings.Load("\\settings\\gamesetting.ini");
	Settings.GetInteger("ServerConfig", "SkillRate", 1, &m_nHitSkillExpRate);
	Settings.GetInteger("ServerConfig", "Skill120Rate", 1, &m_nSupportSkillExpRate);
	if (m_nHitSkillExpRate < 0 || m_nHitSkillExpRate > 10000 ||
		m_nSupportSkillExpRate < 0 || m_nSupportSkillExpRate > 10000)
		return FALSE;
	KIniFile File;
	if (!File.Load("\\settings\\skills.ini"))
		return FALSE;
	for (int i = 1; i <= MAX_SKILL; i++)
	{
		char szSection[16];
		sprintf(szSection, "%d", i);
		char szExp[2048];
		if (!File.GetString(szSection, "SkillLiveUpExp", "", szExp, sizeof(szExp)) || !szExp[0])
			continue;
		PLAYER_SKILL_RULE& Rule = m_PlayerRules[i - 1];
		int Values[MAX_SKILLLEVEL];
		if (!ReadSkillNumberList(File, szSection, "SkillLiveUpExp", Values, MAX_SKILLLEVEL, Rule.MaxLevel) ||
			!ReadSkillNumberList(File, szSection, "SkillActiveLevel", Rule.ActiveLevel, MAX_SKILLLEVEL, Rule.ActiveCount))
			return FALSE;
		for (int j = 0; j < Rule.MaxLevel; j++)
		{
			if (j && Values[j] < Values[j - 1])
				return FALSE;
			Rule.LevelExp[j] = Values[j];
		}
		File.GetInteger(szSection, "BaseSkill", 0, &Rule.BaseSkill);
		File.GetInteger(szSection, "NewBirthSkill", 0, &Rule.NewBirthSkill);
		File.GetInteger(szSection, "PlayerLevel", 0, &Rule.PlayerLevel);
		File.GetInteger(szSection, "UseSkillParent", 1, &Rule.UseSkillParent);
		char szParents[512];
		File.GetString(szSection, "SkillParent", "0:", szParents, sizeof(szParents));
		char* pEnd = NULL;
		Rule.ParentCount = strtol(szParents, &pEnd, 10);
		if (pEnd == szParents || *pEnd != ':' || Rule.ParentCount < 0 || Rule.ParentCount > 16)
			return FALSE;
		char* pCurrent = pEnd + 1;
		for (int j = 0; j < Rule.ParentCount; j++)
		{
			Rule.ParentId[j] = strtol(pCurrent, &pEnd, 10);
			if (pEnd == pCurrent || *pEnd != '&' || Rule.ParentId[j] <= 0 || Rule.ParentId[j] > MAX_SKILL)
				return FALSE;
			pCurrent = pEnd + 1;
			Rule.ParentLevel[j] = strtol(pCurrent, &pEnd, 10);
			if (pEnd == pCurrent || Rule.ParentLevel[j] <= 0 || Rule.ParentLevel[j] >= MAX_SKILLLEVEL ||
				(j + 1 < Rule.ParentCount ? *pEnd != ',' : *pEnd != 0))
				return FALSE;
			pCurrent = pEnd + 1;
		}
		Rule.Loaded = TRUE;
	}
	return TRUE;
}

BOOL KSkillManager::LoadPlayerBuffRules()
{
	KTabFile File;
	if (!File.Load("\\settings\\item\\001\\ibitem.txt"))
		return FALSE;
	int BuffIds[] = {107, 271, 272};
	for (int i = 0; i < 3; i++)
	{
		PLAYER_SKILL_BUFF_RULE& Rule = m_PlayerBuffRules[i];
		memset(&Rule, 0, sizeof(Rule));
		for (int nRow = 2; nRow <= File.GetHeight(); nRow++)
		{
			int nId = -1;
			File.GetInteger(nRow, "具体类别", -1, &nId);
			if (nId != BuffIds[i])
				continue;
			int nMaxCount = 0;
			int nAttribute = 0;
			int nBonus = 0;
			File.GetInteger(nRow, "属性时间(秒）", 0, &Rule.Duration);
			File.GetInteger(nRow, "自身额外叠加最大数量", 0, &nMaxCount);
			File.GetInteger(nRow, "基础属性1", 0, &nAttribute);
			File.GetInteger(nRow, "基础属性1数值", 0, &nBonus);
			// 原表第14列：0只计在线时间，1离线也计时；列标题包含原始引号。
			File.GetInteger(nRow, 14, 0, &Rule.OfflineTime);
			if (Rule.Duration <= 0 || nMaxCount <= 0 || nMaxCount > 100 ||
				Rule.Duration > 100 * 3600 / nMaxCount || nAttribute != 187 ||
				nBonus < 0 || nBonus > 400 || nBonus % 100)
				return FALSE;
			Rule.BuffId = nId;
			Rule.MaxDuration = Rule.Duration * nMaxCount;
			Rule.Multiplier = 1 + nBonus / 100;
			break;
		}
		if (!Rule.BuffId)
			return FALSE;
	}
	return TRUE;
}

const PLAYER_SKILL_BUFF_RULE* KSkillManager::GetPlayerBuffRule(int nBuffId) const
{
	for (int i = 0; i < 3; i++)
	{
		if (m_PlayerBuffRules[i].BuffId == nBuffId)
			return &m_PlayerBuffRules[i];
	}
	return NULL;
}

int KSkillManager::GetEquipmentSeriesBonus(int nSkillId, const int* pAttrib) const
{
	if (!pAttrib || nSkillId <= 0 || nSkillId >= MAX_SKILL)
		return 0;
	int nBonus = 0;
	for (int i = 0; i < 8; i++)
		if (m_EquipmentSkillSeries[i][nSkillId])
			nBonus += pAttrib[252 + i];
	return nBonus;
}
