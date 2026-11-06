#include "KCore.h"
#include "KCompoundRule.h"
#include "KTabFile.h"
#include "KItem.h"
#include <limits.h>

KCompoundRules g_CompoundRule;

BOOL KCompoundRules::ParseDescriptor(const char* pszText, KItemDescriptor* pItem)
{
	if (!pszText || !pItem || !pszText[0])
		return FALSE;
	// []品质、()等级掩码属于装备升级规则，不能当成普通材料数量解析。
	const char* p = pszText;
	int Values[4] = {0};
	for (int i = 0; i < 4; i++)
	{
		if (*p < '0' || *p > '9')
			return FALSE;
		while (*p >= '0' && *p <= '9')
		{
			if (Values[i] > (INT_MAX - (*p - '0')) / 10)
				return FALSE;
			Values[i] = Values[i] * 10 + *p++ - '0';
		}
		if (i < 3 && *p++ != '.')
			return FALSE;
	}
	if (*p || (Values[0] != item_materials && Values[0] != item_magicorscript) ||
		Values[3] <= 0 || Values[3] > 100000)
		return FALSE;
	pItem->Genre = Values[0];
	pItem->Detail = Values[1];
	pItem->Particular = Values[2];
	pItem->Count = Values[3];
	return TRUE;
}

BOOL KCompoundRules::Init(int nVersion)
{
	m_Rules.clear();
	if (nVersion < 0 || nVersion > 999)
		return FALSE;
	KTabFile Rules;
	KTabFile Money;
	char szRules[160];
	char szMoney[160];
	sprintf(szRules, "\\settings\\item\\%03d\\装备合成规则表.txt", nVersion);
	sprintf(szMoney, "\\settings\\item\\%03d\\装备合成金钱需求.txt", nVersion);
	if (!Rules.Load(szRules) || !Money.Load(szMoney) || Rules.GetHeight() != Money.GetHeight())
	{
		g_DebugLog("[Compound] Original rule/money table unavailable: version=%d", nVersion);
		return FALSE;
	}
	for (int nRow = 2; nRow <= Rules.GetHeight(); nRow++)
	{
		int nType, nSkill, nFaction;
		Rules.GetInteger(nRow, 16, 0, &nType);
		Rules.GetInteger(nRow, 15, -1, &nSkill);
		Rules.GetInteger(nRow, 18, -2, &nFaction);
		if (nType != 6 || nSkill < 0 || nSkill > 1 || nFaction != -1)
			continue;
		KRuleEntry Rule;
		ZeroMemory(&Rule, sizeof(Rule));
		Rules.GetInteger(nRow, 1, 0, &Rule.RuleID);
		Rules.GetInteger(nRow, 3, 0, &Rule.SourceCount);
		Rules.GetInteger(nRow, 17, -1, &Rule.SuccessRate);
		int nMoneyRule;
		Money.GetInteger(nRow, 1, 0, &nMoneyRule);
		Money.GetInteger(nRow, 2, 0, &Rule.Money);
		if (Rule.RuleID <= 0 || nMoneyRule != Rule.RuleID || Rule.Money < 0 ||
			Rule.SourceCount <= 0 || Rule.SourceCount > MAX_COMPOUND_MATERIALS ||
			Rule.SuccessRate < 0 || Rule.SuccessRate > 100)
			continue;
		char szValue[128];
		Rules.GetString(nRow, 14, "", szValue, sizeof(szValue));
		if (!ParseDescriptor(szValue, &Rule.Target))
			continue;
		BOOL bSupported = TRUE;
		for (int i = 0; i < 10; i++)
		{
			Rules.GetString(nRow, 4 + i, "", szValue, sizeof(szValue));
			if (i < Rule.SourceCount)
			{
				if (!ParseDescriptor(szValue, &Rule.Sources[i]))
					bSupported = FALSE;
			}
			else if (szValue[0])
				bSupported = FALSE;
		}
		if (bSupported)
			m_Rules.push_back(Rule);
	}
	g_DebugLog("[Compound] Loaded original material rules: version=%d count=%d",
		nVersion, (int)m_Rules.size());
	return !m_Rules.empty();
}

const KCompoundRules::KRuleEntry* KCompoundRules::Match(const int* pItems,
	int nCount, int* pConsume) const
{
	if (!pItems || !pConsume || nCount <= 0 || nCount > MAX_COMPOUND_MATERIALS)
		return NULL;
	for (size_t n = 0; n < m_Rules.size(); n++)
	{
		const KRuleEntry& Rule = m_Rules[n];
		ZeroMemory(pConsume, sizeof(int) * MAX_COMPOUND_MATERIALS);
		BOOL bMatch = TRUE;
		for (int i = 0; i < Rule.SourceCount; i++)
		{
			const KItemDescriptor& Source = Rule.Sources[i];
			int nLeft = Source.Count;
			for (int j = 0; j < nCount && nLeft > 0; j++)
			{
				if (pItems[j] <= 0 || pItems[j] >= MAX_ITEM)
					continue;
				KItem& Data = Item[pItems[j]];
				if (Data.GetGenre() != Source.Genre || Data.GetDetailType() != Source.Detail ||
					Data.GetParticular() != Source.Particular || Data.GetLevel() != 0)
					continue;
				int nTake = Data.GetStackCount() - pConsume[j];
				if (nTake > nLeft)
					nTake = nLeft;
				pConsume[j] += nTake;
				nLeft -= nTake;
			}
			if (nLeft > 0)
				bMatch = FALSE;
		}
		// 不能忽略请求中多塞的道具，也不能将同一叠重复计数。
		for (int i = 0; i < nCount; i++)
		{
			if (pConsume[i] <= 0)
				bMatch = FALSE;
		}
		if (bMatch)
			return &Rule;
	}
	return NULL;
}
