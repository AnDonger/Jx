#include "KCore.h"
#ifdef _SERVER
#include "KCompoundRule.h"
#include "KPlayer.h"
#include "KNpc.h"
#include "KNpcSet.h"
#include "KItemSet.h"
#include "KItemList.h"
#include "KSubWorldSet.h"
#include "KSortScript.h"
#include "KMath.h"

void KPlayer::CompoundMessage(const char* pszMessage)
{
	PLAYER_SCRIPTACTION_SYNC Info;
	ZeroMemory(&Info, sizeof(Info));
	Info.m_bUIId = UI_MSGINFO;
	Info.m_bParam2 = 1;
	g_StrCpyLen(Info.m_pContent, pszMessage, sizeof(Info.m_pContent));
	Info.m_nBufferLen = strlen(Info.m_pContent);
	DoScriptAction(&Info);
}

BOOL KPlayer::CanCompoundItems()
{
	if (!m_bFinishLoading || m_nIndex <= 0 || m_nIndex >= MAX_NPC ||
		CheckTrading() || m_bExchangeServer || m_ItemList.Hand() ||
		Npc[m_nIndex].m_Doing == do_death || Npc[m_nIndex].m_CurrentLife <= 0 ||
		!m_dwCompoundNpcID || Npc[m_nIndex].m_SubWorldIndex != m_nCompoundWorld)
		return FALSE;
	int nNpc = FindAroundNpc(m_dwCompoundNpcID);
	if (nNpc <= 0 || nNpc >= MAX_NPC || Npc[nNpc].m_Kind != kind_dialoger ||
		Npc[nNpc].m_SubWorldIndex != m_nCompoundWorld ||
		Npc[nNpc].m_ActionScriptID != m_dwCompoundScriptID ||
		NpcSet.GetDistance(nNpc, m_nIndex) > Npc[nNpc].m_DialogRadius * 2)
		return FALSE;
	// 原玲珑锁脚本以464保存设置。解锁状态接口尚未接通，已设置者保守拒绝。
	// 不能以固定返回0的脚本接口绕过物品保护。
	if (m_cTask.GetSaveVal(464) != 0)
		return FALSE;
	return TRUE;
}

BOOL KPlayer::OpenItemCompound(BOOL bNewWindow)
{
	m_dwCompoundNpcID = 0;
	if (m_nPeapleIdx <= 0 || m_nPeapleIdx >= MAX_NPC ||
		m_nIndex <= 0 || m_nIndex >= MAX_NPC)
		return FALSE;
	m_dwCompoundNpcID = Npc[m_nPeapleIdx].m_dwID;
	m_dwCompoundScriptID = Npc[m_nPeapleIdx].m_ActionScriptID;
	m_nCompoundWorld = Npc[m_nIndex].m_SubWorldIndex;
	if (!CanCompoundItems())
	{
		m_dwCompoundNpcID = 0;
		CompoundMessage("当前无法合成，请放下鼠标物品并检查玲珑锁保护状态。");
		return FALSE;
	}
	if (bNewWindow)
	{
		// 20261106实录，赤松子“合成物品(新)”打开合成强化窗口。
		BYTE byOpen[] = {0xF8, 4, 0, 0x73, 0x90};
		g_pServer->PackDataToClient(m_nNetConnectIdx, byOpen, sizeof(byOpen));
	}
	else
	{
		PLAYER_SCRIPTACTION_SYNC Info;
		ZeroMemory(&Info, sizeof(Info));
		Info.m_bUIId = UI_COMPOUND;
		Info.m_bParam2 = 1;
		DoScriptAction(&Info);
	}
	return TRUE;
}

int KItemList::CompoundItems(const GAME_COMPOUND_ITEM_POS* pPositions, int nCount)
{
	if (!pPositions || nCount != MAX_COMPOUND_MATERIALS ||
		!Player[m_PlayerIdx].CanCompoundItems())
		return -1;
	int Indices[MAX_COMPOUND_MATERIALS] = {0};
	int nItems = 0;
	for (int i = 0; i < nCount; i++)
	{
		const GAME_COMPOUND_ITEM_POS& Pos = pPositions[i];
		if (Pos.Place == 0 && Pos.X == 0 && Pos.Y == 0)
			continue;
		if (Pos.Place != pos_equiproom || Pos.X < 0 || Pos.X >= EQUIPMENT_ROOM_WIDTH ||
			Pos.Y < 0 || Pos.Y >= EQUIPMENT_ROOM_HEIGHT)
			return -1;
		int nItem = 0;
		for (int n = m_UseIdx.GetNext(0); n; n = m_UseIdx.GetNext(n))
		{
			if (m_Items[n].nPlace == Pos.Place && m_Items[n].nX == Pos.X && m_Items[n].nY == Pos.Y)
			{
				nItem = m_Items[n].nIdx;
				break;
			}
		}
		if (nItem <= 0 || nItem >= MAX_ITEM ||
			Item[nItem].m_GeneratorParam.nVersion != g_SubWorldSet.GetGameVersion())
			return -1;
		for (int j = 0; j < nItems; j++)
		{
			if (Indices[j] == nItem)
				return -1;
		}
		Indices[nItems++] = nItem;
	}
	int Consume[MAX_COMPOUND_MATERIALS] = {0};
	const KCompoundRules::KRuleEntry* pRule = g_CompoundRule.Match(Indices, nItems, Consume);
	if (!pRule || GetMoney(room_equipment) < pRule->Money)
		return -1;
	int nNew = ItemSet.Add(pRule->Target.Genre, 0, 0, 0, pRule->Target.Detail,
		pRule->Target.Particular, NULL, g_SubWorldSet.GetGameVersion());
	if (nNew <= 0)
		return -1;
	if (!Item[nNew].GetName()[0] || Item[nNew].GetWidth() <= 0 || Item[nNew].GetHeight() <= 0 ||
		!Item[nNew].SetStackCount(pRule->Target.Count))
	{
		ItemSet.Remove(nNew);
		return -1;
	}
	for (int i = 0; i < nItems; i++)
	{
		if (Item[Indices[i]].IsBound())
		{
			Item[nNew].m_btStateFlags |= 1;
			if (Item[nNew].m_btBindingMode < Item[Indices[i]].m_btBindingMode)
				Item[nNew].m_btBindingMode = Item[Indices[i]].m_btBindingMode;
		}
	}
	Item[nNew].SetDurability(Item[nNew].GetMaxDurability());
	DWORD dwNewID = Item[nNew].GetID();
	BOOL bSuccess = g_Random(100) < pRule->SuccessRate;
	// 当前游戏线程顺序处理请求；失败、满包及异常均由原NPC事务恢复。
	KNpcScriptTransaction Transaction(m_PlayerIdx);
	for (int i = 0; i < nItems; i++)
	{
		if (!ConsumeItem(Indices[i], Consume[i]))
		{
			ItemSet.Remove(nNew);
			return -1;
		}
	}
	if (!Player[m_PlayerIdx].Pay(pRule->Money))
	{
		ItemSet.Remove(nNew);
		return -1;
	}
	if (bSuccess)
	{
		if (!AddPickupItem(nNew, -1, -1))
		{
			ItemSet.Remove(nNew);
			return -1;
		}
	}
	else
		ItemSet.Remove(nNew);
	Transaction.Commit();
	if (pRule->Money > 0)
	{
		char szMessage[96];
		sprintf(szMessage, "合成消耗%d两金钱。", pRule->Money);
		Player[m_PlayerIdx].CompoundMessage(szMessage);
	}
	g_DebugLog("[Compound] player=%s rule=%d target=%d,%d,%d count=%d money=%d rate=%d success=%d item=%lu",
		Player[m_PlayerIdx].m_PlayerName, pRule->RuleID, pRule->Target.Genre,
		pRule->Target.Detail, pRule->Target.Particular, pRule->Target.Count,
		pRule->Money, pRule->SuccessRate, bSuccess, bSuccess ? dwNewID : 0);
	return bSuccess ? 1 : 0;
}
#endif
