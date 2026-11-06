// 原始装备合成规则；当前先处理不改变装备属性的材料、脚本道具配方。
#ifndef KCOMPOUNDRULE_H
#define KCOMPOUNDRULE_H

#include <vector>

#define MAX_COMPOUND_MATERIALS 9

class KCompoundRules
{
public:
	struct KItemDescriptor
	{
		int Genre;
		int Detail;
		int Particular;
		int Count;
	};

	struct KRuleEntry
	{
		int RuleID;
		int SourceCount;
		KItemDescriptor Sources[MAX_COMPOUND_MATERIALS];
		KItemDescriptor Target;
		int Money;
		int SuccessRate;
	};

	BOOL Init(int nVersion);
	const KRuleEntry* Match(const int* pItems, int nCount, int* pConsume) const;
	int GetRuleCount() const { return (int)m_Rules.size(); }

private:
	std::vector<KRuleEntry> m_Rules;
	BOOL ParseDescriptor(const char* pszText, KItemDescriptor* pItem);
};

extern KCompoundRules g_CompoundRule;
#endif
