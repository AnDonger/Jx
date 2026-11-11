/*******************************************************************************
// FileName			:	ScriptFuns.cpp
// FileAuthor		:	RomanDou
// FileCreateDate	:	2002-11-19 15:58:20
// FileDescription	:	脚本指令集
// Revision Count	:	
*******************************************************************************/
#ifndef WIN32
#include <string>
#endif

#include "KWin32.h"
#include "KEngine.h"
#include "KDebug.h"
#include "KStepLuaScript.h"
#include "LuaLib.h"
#include "KScriptList.h"
#include <string.h>
#include <time.h>
#include "LuaFuns.h"
#include "KCore.h"
#include "KNpc.h"
#include "KSubWorld.h"
#include "KObjSet.h"
#include "KItemSet.h"
#include "KItemGenerator.h"
//#include "KNetClient.h"
#include "KScriptValueSet.h"
#include "KNpcSet.h"
#include "KPlayerSet.h"
#include "KPlayer.h"
#include "KSubWorldSet.h"
#include "KProtocolProcess.h"
#include "KBuySell.h"
#include "KTaskFuns.h"
#include "KPlayerDef.h"
#include "TaskDef.h"
#ifdef _SERVER
//#include "KNetServer.h"
//#include "../MultiServer/Heaven/interface/iServer.h"
#include "KNewProtocolProcess.h"
#include <vector>
#include <string>
#include "KTabFile.h"
#endif
#include "KSortScript.h"
#ifndef __linux
#include "Shlwapi.h"
#define WIN32_LEAN_AND_MEAN // Fixed By MrChuCong@gmail.com
#include "windows.h"
#include "Winsock2.h" // Fixed By MrChuCong@gmail.com
#include "winbase.h"
#include <direct.h>
#else
#include "unistd.h"
#endif

#ifdef _STANDALONE
#include "KSG_StringProcess.h"
#else
#include "../../Engine/Src/KSG_StringProcess.h"
#endif

#ifndef WIN32
typedef struct  _SYSTEMTIME
{
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
}	SYSTEMTIME;
typedef struct  _FILETIME
{
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
}	FILETIME;
#endif

inline const char* _ip2a(DWORD ip) { in_addr ia; ia.s_addr = ip; return inet_ntoa(ia); }
inline DWORD _a2ip(const char* cp) { return inet_addr(cp); }

KScriptList		g_StoryScriptList;
KStepLuaScript * LuaGetScript(Lua_State * L);
int	GetPlayerIndex(Lua_State * L);
extern int g_GetPriceToStation(int,int);
extern int g_GetPriceToWayPoint(int,int);
extern int g_GetPriceToDock(int ,int );

//BitValue = GetBit(Value, BitNo)
int LuaGetBit(Lua_State * L)
{
	int nBitValue = 0;
	int nIntValue = (int)Lua_ValueToNumber(L, 1);
	int nBitNumber = (int)Lua_ValueToNumber(L, 2);
	
	if (nBitNumber >= 32 || nBitNumber <= 0) 
		goto lab_getbit;
	nBitValue = (nIntValue & (1 << (nBitNumber - 1))) != 0;
lab_getbit:
	Lua_PushNumber(L, nBitValue);
	return 1;
}

//NewBit = SetBit(Value, BitNo, BitValue)
int LuaSetBit(Lua_State * L)
{
	int nIntValue = (int)Lua_ValueToNumber(L, 1);
	int nBitNumber = (int)Lua_ValueToNumber(L, 2);
	int nBitValue = (int)Lua_ValueToNumber(L,3);
	nBitValue = (nBitValue == 1);
	
	if (nBitNumber > 32 || nBitNumber <= 0) 
		goto lab_setbit;
	
	nIntValue = (nIntValue | (1 << (nBitNumber - 1)));
lab_setbit:
	Lua_PushNumber(L, nIntValue);
	return 1;
}

//ByteValue = GetByte(Value, ByteNo)
int LuaGetByte(Lua_State * L)
{
	int nByteValue = 0;
	int nIntValue = (int)Lua_ValueToNumber(L, 1);
	int nByteNumber = (int)Lua_ValueToNumber(L, 2);
	
	if (nByteNumber > 4 || nByteNumber <= 0) 
		goto lab_getByte;
	nByteValue = (nIntValue & (0xff << ((nByteNumber - 1) * 8) )) >> ((nByteNumber - 1) * 8);
	
lab_getByte:
	Lua_PushNumber(L, nByteValue);
	return 1;
}

//NewByte = SetByte(Value, ByteNo, ByteValue)
int LuaSetByte(Lua_State * L)
{
	BYTE * pByte =	NULL;
	int nIntValue = (int)Lua_ValueToNumber(L, 1);
	int nByteNumber = (int)Lua_ValueToNumber(L, 2);
	int nByteValue = (int)Lua_ValueToNumber(L,3);
	nByteValue = (nByteValue & 0xff);
	
	if (nByteNumber > 4 || nByteNumber <= 0) 
		goto lab_setByte;
	
	pByte = (BYTE*)&nIntValue;
	*(pByte + (nByteNumber -1)) = (BYTE)nByteValue;
	//nIntValue = (nIntValue | (0xff << ((nByteNumber - 1) * 8) )) ;
lab_setByte:
	Lua_PushNumber(L, nIntValue);
	return 1;
}
#ifdef _SERVER
// 只保留本次NPC回调会修改的随身物品。装备和仓库不参与原任务交材料。
struct KNpcScriptItem
{
	KItem Data;
	int Place;
	int X;
	int Y;
};

struct KNpcScriptNotice
{
	PLAYER_SCRIPTACTION_SYNC Action;
	std::string Chat;
	std::vector<BYTE> Raw;
	BOOL IsChat;
	KNpcScriptNotice()
	{
		ZeroMemory(&Action, sizeof(Action));
		IsChat = FALSE;
	}
};

// 玩家自动回调只生成操作计划，Lua成功返回后才执行实际修理和通知。
// 不复用NPC背包快照去声称已经覆盖穿戴装备、属性或地面物品。
struct KPlayerScriptAction
{
	DWORD ItemID;
	DWORD RepairID;
	BOOL AllowRepair;
	std::vector<std::string> Notices;
};

static KPlayerScriptAction* g_PlayerScriptAction[MAX_PLAYER] = {NULL};

struct KNpcScriptExchange
{
	KPlayerTask Task;
	PLAYER_PET_DATA Pet;
	PLAYER_SKILL_BUFF Buffs[16];
	BOOL HasMorph;
	int MorphType;
	BOOL MorphCanCast;
	int MorphSeconds;
	std::vector<KNpcScriptItem> Items;
	std::vector<KNpcScriptNotice> Notices;
	int Money[room_num];
	int Depth;
	int Experience;
	BOOL Failed;
	BOOL HasTeleport;
	BOOL ConsumedUseItem;
	DWORD TeleportItemID;
	int TeleportMap;
	int TeleportX;
	int TeleportY;
	int TeleportFight;
	int TeleportRevival;
};

static KNpcScriptExchange *g_NpcScriptExchange[MAX_PLAYER] = {NULL};

BOOL g_QueueNpcScriptAction(int nPlayer, const PLAYER_SCRIPTACTION_SYNC& Action)
{
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER || !g_NpcScriptExchange[nPlayer])
		return FALSE;
	KNpcScriptNotice Notice;
	Notice.Action = Action;
	g_NpcScriptExchange[nPlayer]->Notices.push_back(Notice);
	return TRUE;
}


KNpcScriptTransaction::KNpcScriptTransaction(int nPlayer)
{
	m_nPlayer = nPlayer > 0 && nPlayer < MAX_PLAYER ? nPlayer : 0;
	m_bCommitted = FALSE;
	if (!m_nPlayer)
		return;
	KNpcScriptExchange *&pState = g_NpcScriptExchange[m_nPlayer];
	if (pState)
	{
		pState->Depth++;
		return;
	}
	pState = new KNpcScriptExchange;
	pState->Task = Player[m_nPlayer].m_cTask;
	pState->Pet = Player[m_nPlayer].m_Pet;
	memcpy(pState->Buffs, Npc[Player[m_nPlayer].m_nIndex].m_SkillList.m_SkillBuffs, sizeof(pState->Buffs));
	pState->HasMorph = FALSE;
	pState->Depth = 1;
	pState->Experience = 0;
	pState->Failed = FALSE;
	pState->HasTeleport = FALSE;
	pState->ConsumedUseItem = FALSE;
	pState->TeleportRevival = 0;
	KItemList& List = Player[m_nPlayer].m_ItemList;
	for (int i = 0; i < room_num; i++)
		pState->Money[i] = List.GetMoney(i);
	for (PlayerItem *pItem = List.GetFirstItem(); pItem; pItem = List.GetNextItem())
	{
		if (pItem->nPlace != pos_equiproom && pItem->nPlace != pos_immediacy &&
			pItem->nPlace != pos_hand)
			continue;
		KNpcScriptItem Saved;
		Saved.Data = Item[pItem->nIdx];
		Saved.Place = pItem->nPlace;
		Saved.X = pItem->nX;
		Saved.Y = pItem->nY;
		pState->Items.push_back(Saved);
	}
}

BOOL KNpcScriptTransaction::HasFailed() const
{
	return !m_nPlayer || !g_NpcScriptExchange[m_nPlayer] ||
		g_NpcScriptExchange[m_nPlayer]->Failed;
}

static BOOL IsScriptTeleportTarget(int nMap, int nX, int nY)
{
	int nWorld = g_SubWorldSet.SearchWorld(nMap);
	if (nWorld < 0 || nWorld >= MAX_SUBWORLD || nX <= 0 || nY <= 0)
		return FALSE;
	int nRegion = -1, nMapX = 0, nMapY = 0, nOffX = 0, nOffY = 0;
	SubWorld[nWorld].Mps2Map(nX, nY, &nRegion, &nMapX, &nMapY, &nOffX, &nOffY);
	return nRegion >= 0;
}

static BOOL IsScriptTeleportValid(int nPlayer, const KNpcScriptExchange& State)
{
	KPlayer& Role = Player[nPlayer];
	int nItem = Role.FindUseScriptItem();
	if (!nItem || Item[nItem].GetID() != State.TeleportItemID ||
		!IsScriptTeleportTarget(State.TeleportMap, State.TeleportX, State.TeleportY))
		return FALSE;
	if (State.TeleportRevival)
	{
		KIniFile Revivals;
		char szMap[32], szRevival[32], szValue[128];
		sprintf(szMap, "%d", State.TeleportMap);
		sprintf(szRevival, "%d", State.TeleportRevival);
		if (!Revivals.Load("\\settings\\RevivePos.ini") ||
			!Revivals.GetString(szMap, szRevival, "", szValue, sizeof(szValue)))
			return FALSE;
		int nRevivalX = 0, nRevivalY = 0;
		Revivals.GetInteger2(szMap, szRevival, &nRevivalX, &nRevivalY);
		if (!IsScriptTeleportTarget(State.TeleportMap, nRevivalX, nRevivalY))
			return FALSE;
	}
	return TRUE;
}

KNpcScriptTransaction::~KNpcScriptTransaction()
{
	if (!m_nPlayer)
		return;
	KNpcScriptExchange *pState = g_NpcScriptExchange[m_nPlayer];
	if (!m_bCommitted)
		pState->Failed = TRUE;
	if (--pState->Depth)
		return;
	g_NpcScriptExchange[m_nPlayer] = NULL;
	KPlayer& Role = Player[m_nPlayer];
	if (!pState->Failed && pState->HasTeleport)
	{
		// 同步游戏线程中重检全部条件，再扣确切实例；未加载地图不能跨服半提交。
		if (!IsScriptTeleportValid(m_nPlayer, *pState))
			pState->Failed = TRUE;
		else
		{
			int nItem = Role.FindUseScriptItem();
			if (!Role.m_ItemList.ConsumeItem(nItem, 1) ||
				Npc[Role.m_nIndex].ChangeWorld(pState->TeleportMap,
					pState->TeleportX, pState->TeleportY) != 1)
				pState->Failed = TRUE;
			else
			{
				Npc[Role.m_nIndex].SetFightMode(pState->TeleportFight);
				if (pState->TeleportRevival)
					Role.SetRevivalPos(pState->TeleportMap, pState->TeleportRevival);
				Role.m_ulLastSaveTime = 0;
				Role.m_bWaitingPlayerFeedBack = false;
				Role.m_nAvailableAnswerNum = 0;
				Role.m_dwScriptDialogScriptID = 0;
				Role.ClearUseItemContext();
				g_DebugLog("[ItemTeleport] player=%d item=%lu map=%d position=(%d,%d)",
					m_nPlayer, pState->TeleportItemID, pState->TeleportMap,
					pState->TeleportX, pState->TeleportY);
			}
		}
	}
	if (!pState->Failed && pState->HasMorph &&
		!Npc[Role.m_nIndex].PolyMorph(pState->MorphType, pState->MorphCanCast, 0, -1, pState->MorphSeconds))
		pState->Failed = TRUE;
	if (pState->Failed)
	{
		KSkillList& Skills = Npc[Role.m_nIndex].m_SkillList;
		if (memcmp(Skills.m_SkillBuffs, pState->Buffs, sizeof(pState->Buffs)))
		{
			memcpy(Skills.m_SkillBuffs, pState->Buffs, sizeof(pState->Buffs));
			Skills.RefreshPlayerSkillLevels();
			Skills.SyncPlayerSkillBuffs();
		}
		KItemList& List = Role.m_ItemList;
		std::vector<int> Indices;
		for (PlayerItem *pItem = List.GetFirstItem(); pItem; pItem = List.GetNextItem())
		{
			if (pItem->nPlace == pos_equiproom || pItem->nPlace == pos_immediacy ||
				pItem->nPlace == pos_hand)
				Indices.push_back(pItem->nIdx);
		}
		for (size_t i = 0; i < Indices.size(); i++)
		{
			List.Remove(Indices[i]);
			ItemSet.Remove(Indices[i]);
		}
		BOOL bRestored = TRUE;
		for (size_t i = 0; i < pState->Items.size(); i++)
		{
			KNpcScriptItem& Saved = pState->Items[i];
			int nItem = ItemSet.Add(&Saved.Data);
			if (nItem > 0)
			{
				Item[nItem].SetID(Saved.Data.GetID());
				if (!List.Add(nItem, Saved.Place, Saved.X, Saved.Y))
				{
					ItemSet.Remove(nItem);
					bRestored = FALSE;
				}
			}
			else
				bRestored = FALSE;
		}
		for (int i = 0; i < room_num; i++)
			List.SetRoomMoney(i, pState->Money[i]);
		List.SendMoneySync();
		Role.m_Pet = pState->Pet;
		Role.m_cTask = pState->Task;
		Role.m_cTask.SyncNotes(m_nPlayer);
		Role.m_bWaitingPlayerFeedBack = false;
		Role.m_nAvailableAnswerNum = 0;
		PLAYER_SCRIPTACTION_SYNC Info;
		ZeroMemory(&Info, sizeof(Info));
		Info.m_bUIId = UI_MSGINFO;
		Info.m_bParam2 = 1;
		strcpy(Info.m_pContent, "操作未完成，物品、金钱和任务进度已恢复，请检查背包空间及材料。");
		Info.m_nBufferLen = strlen(Info.m_pContent);
		Role.DoScriptAction(&Info);
		if (!bRestored)
			Role.m_bFinishLoading = FALSE;
		g_DebugLog("[NpcScript] Exchange rolled back: player=%d items=%d restored=%d",
			m_nPlayer, (int)pState->Items.size(), bRestored);
	}
	else
	{
		if (memcmp(&pState->Pet, &Role.m_Pet, sizeof(Role.m_Pet)) != 0)
			Role.SyncPetState();
		KSkillList& Skills = Npc[Role.m_nIndex].m_SkillList;
		if (memcmp(Skills.m_SkillBuffs, pState->Buffs, sizeof(pState->Buffs)))
		{
			Skills.RefreshPlayerSkillLevels();
			Skills.SyncPlayerSkillBuffs();
		}
		if (pState->Experience > 0)
			Role.DirectAddExp(pState->Experience);
		for (size_t i = 0; i < pState->Notices.size(); i++)
		{
			KNpcScriptNotice& Notice = pState->Notices[i];
			if (!Notice.Raw.empty())
				g_pServer->PackDataToClient(Role.m_nNetConnectIdx, &Notice.Raw[0], Notice.Raw.size());
			else if (Notice.IsChat)
				KPlayerChat::SendSystemInfo(1, m_nPlayer, MESSAGE_SYSTEM_ANNOUCE_HEAD,
					(char*)Notice.Chat.c_str(), (int)Notice.Chat.size());
			else
				Role.DoScriptAction(&Notice.Action);
		}
	}
	delete pState;
}

static void RejectNpcScriptItem(Lua_State *L, int nPlayer)
{
	if (nPlayer > 0 && nPlayer < MAX_PLAYER && g_NpcScriptExchange[nPlayer])
	{
		g_NpcScriptExchange[nPlayer]->Failed = TRUE;
		lua_error(L, "NPC item exchange could not be completed");
	}
}

struct KNpcSelectedItem
{
	int Index;
	DWORD ID;
	int Remaining;
};

struct KNpcSelectedItems
{
	std::vector<KNpcSelectedItem> Items;
	int Result;
};

static KNpcSelectedItems* g_NpcSelectedItems[MAX_PLAYER] = {NULL};

KNpcScriptSelection::KNpcScriptSelection(int nPlayer, const int* pItems, int nCount)
{
	m_nPlayer = 0;
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER || !pItems || nCount <= 0 ||
		nCount > MAX_SCRIPT_SELECTED_ITEMS || g_NpcSelectedItems[nPlayer])
		return;
	for (int i = 0; i < nCount; i++)
	{
		if (pItems[i] <= 0 || pItems[i] >= MAX_ITEM || !Item[pItems[i]].GetID())
			return;
		for (int j = 0; j < i; j++)
		{
			if (pItems[i] == pItems[j])
				return;
		}
	}
	KNpcSelectedItems* pState = new KNpcSelectedItems;
	pState->Result = -1;
	for (int i = 0; i < nCount; i++)
	{
		KNpcSelectedItem Selected;
		Selected.Index = pItems[i];
		Selected.ID = Item[pItems[i]].GetID();
		Selected.Remaining = Item[pItems[i]].GetStackCount();
		pState->Items.push_back(Selected);
	}
	g_NpcSelectedItems[nPlayer] = pState;
	m_nPlayer = nPlayer;
}

KNpcScriptSelection::~KNpcScriptSelection()
{
	if (m_nPlayer)
	{
		delete g_NpcSelectedItems[m_nPlayer];
		g_NpcSelectedItems[m_nPlayer] = NULL;
	}
}

int KNpcScriptSelection::GetResult() const
{
	return m_nPlayer ? g_NpcSelectedItems[m_nPlayer]->Result : -1;
}

static void SetScriptItemInteger(Lua_State* L, int nTable, const char* pszName, int nValue)
{
	Lua_PushString(L, pszName);
	Lua_PushNumber(L, nValue);
	Lua_SetTable(L, nTable);
}

// 当前使用的真实道具；既不接受客户端下标，也不允许借另一叠同名物品续用旧菜单。
int LuaGetUseItemInfo(Lua_State* L)
{
	int nPlayer = GetPlayerIndex(L);
	int nItem = nPlayer > 0 ? Player[nPlayer].FindUseScriptItem() : 0;
	if (!nItem)
	{
		Lua_PushNil(L);
		return 1;
	}
	lua_newtable(L);
	int nTable = Lua_GetTopIndex(L);
	SetScriptItemInteger(L, nTable, "Genre", Item[nItem].GetGenre());
	SetScriptItemInteger(L, nTable, "Detail", Item[nItem].GetDetailType());
	SetScriptItemInteger(L, nTable, "Particular", Item[nItem].GetParticular());
	SetScriptItemInteger(L, nTable, "Level", Item[nItem].GetLevel());
	SetScriptItemInteger(L, nTable, "Count", Item[nItem].GetStackCount());
	return 1;
}

// 只消耗当前使用实例；须在道具自动事务内，与模板自动扣除及延迟传送互斥。
// 发奖失败由外层快照完整恢复，不按模板另找同名礼包扣取。
int LuaConsumeUseItem(Lua_State* L)
{
	int nPlayer = GetPlayerIndex(L);
	KNpcScriptExchange* pState = nPlayer > 0 ? g_NpcScriptExchange[nPlayer] : NULL;
	int nArguments = Lua_GetTopIndex(L);
	double Count = nArguments == 0 ? 1 : Lua_ValueToNumber(L, 1);
	int nItem = nPlayer > 0 ? Player[nPlayer].FindUseScriptItem() : 0;
	BOOL bConsumeOnUse = FALSE;
	if (nItem)
		ItemGen.GetUseItemScript(Item[nItem], &bConsumeOnUse);
	if (!pState || pState->Failed || pState->HasTeleport || pState->ConsumedUseItem ||
		!nItem || bConsumeOnUse || nArguments > 1 ||
		(nArguments == 1 && !Lua_IsNumber(L, 1)) ||
		!(Count >= 1 && Count <= 100000) || Count != (int)Count ||
		Item[nItem].GetStackCount() < (int)Count)
	{
		RejectNpcScriptItem(L, nPlayer);
		Lua_PushNumber(L, 0);
		return 1;
	}
	if (!Player[nPlayer].m_ItemList.ConsumeItem(nItem, (int)Count))
	{
		RejectNpcScriptItem(L, nPlayer);
		Lua_PushNumber(L, 0);
		return 1;
	}
	pState->ConsumedUseItem = TRUE;
	Player[nPlayer].m_ulLastSaveTime = 0;
	Lua_PushNumber(L, 1);
	return 1;
}

// TeleportWithItem()回当前重生点；显式参数使用脚本格坐标。
// 只登记一次延迟计划，不在Lua执行期间扣符、切图或更改战斗状态。
int LuaTeleportWithItem(Lua_State* L)
{
	int nCount = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	KNpcScriptExchange* pState = nPlayer > 0 ? g_NpcScriptExchange[nPlayer] : NULL;
	if (!pState || pState->Failed || pState->HasTeleport || pState->ConsumedUseItem ||
		!Player[nPlayer].FindUseScriptItem())
	{
		Lua_PushNumber(L, 0);
		return 1;
	}
	int nMap = 0, nX = 0, nY = 0, nFight = 0, nRevival = 0;
	if (nCount == 0)
		Player[nPlayer].GetDeathRevivalPos(&nMap, &nX, &nY);
	else
	{
		if (nCount != 4 && nCount != 5)
		{
			Lua_PushNumber(L, 0);
			return 1;
		}
		for (int i = 1; i <= nCount; i++)
		{
			double Value = Lua_ValueToNumber(L, i);
			if (!Lua_IsNumber(L, i) || !(Value >= 0 && Value <= 0x03ffffff) || Value != (int)Value)
			{
				Lua_PushNumber(L, 0);
				return 1;
			}
		}
		nMap = (int)Lua_ValueToNumber(L, 1);
		nX = (int)Lua_ValueToNumber(L, 2) * 32;
		nY = (int)Lua_ValueToNumber(L, 3) * 32;
		nFight = (int)Lua_ValueToNumber(L, 4);
		nRevival = nCount == 5 ? (int)Lua_ValueToNumber(L, 5) : 0;
	}
	pState->TeleportItemID = Player[nPlayer].m_dwUseItemID;
	pState->TeleportMap = nMap;
	pState->TeleportX = nX;
	pState->TeleportY = nY;
	pState->TeleportFight = nFight;
	pState->TeleportRevival = nRevival;
	if (nFight > 1 || !IsScriptTeleportValid(nPlayer, *pState))
	{
		Lua_PushNumber(L, 0);
		return 1;
	}
	pState->HasTeleport = TRUE;
	Lua_PushNumber(L, 1);
	return 1;
}

static BOOL ReadScriptItemOption(Lua_State* L, int nTable, const char* pszName,
	int nMinimum, int nMaximum, int* pnValue)
{
	Lua_PushString(L, pszName);
	Lua_GetTable(L, nTable);
	BOOL bValid = TRUE;
	if (!lua_isnil(L, -1))
	{
		double nValue = Lua_ValueToNumber(L, -1);
		bValid = Lua_IsNumber(L, -1) && nValue >= nMinimum &&
			nValue <= nMaximum && nValue == (int)nValue;
		if (bValid)
			*pnValue = (int)nValue;
	}
	Lua_Pop(L, 1);
	return bValid;
}

// 原资源表按1起始的行、列返回字符串，首行表头与空单元格均保留。
// 业务解析和缓存由Lua负责；失败返回nil，不创建替代表。
int LuaReadTabFile(Lua_State* L)
{
	const char* pszPath = Lua_ValueToString(L, 1);
	if (Lua_GetTopIndex(L) != 1 || !pszPath || pszPath[0] != '\\' ||
		strlen(pszPath) >= MAX_PATH || strstr(pszPath, "..") || strchr(pszPath, ':'))
	{
		Lua_PushNil(L);
		return 1;
	}
	KTabFile Table;
	if (!Table.Load((char*)pszPath) || Table.GetHeight() <= 0 || Table.GetHeight() > 20000 ||
		Table.GetWidth() <= 0 || Table.GetWidth() > 256 ||
		Table.GetHeight() * Table.GetWidth() > 1000000)
	{
		Lua_PushNil(L);
		return 1;
	}
	lua_newtable(L);
	int nResult = Lua_GetTopIndex(L);
	char szValue[4097];
	for (int nRow = 1; nRow <= Table.GetHeight(); nRow++)
	{
		lua_newtable(L);
		int nColumns = Lua_GetTopIndex(L);
		for (int nColumn = 1; nColumn <= Table.GetWidth(); nColumn++)
		{
			// 旧引擎GetValue在截断分支额外写一个结束符，预留最后一个字节。
			Table.GetString(nRow, nColumn, "", szValue, sizeof(szValue) - 1);
			if (strlen(szValue) == sizeof(szValue) - 1)
			{
				Lua_PushNil(L);
				return 1;
			}
			Lua_PushString(L, szValue);
			lua_rawseti(L, nColumns, nColumn);
		}
		lua_rawseti(L, nResult, nRow);
	}
	return 1;
}

int LuaGetGameVersion(Lua_State* L)
{
	Lua_PushNumber(L, g_SubWorldSet.GetGameVersion());
	return 1;
}

int LuaGetSelectedItems(Lua_State* L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	KNpcSelectedItems* pState = nPlayer > 0 && nPlayer < MAX_PLAYER ? g_NpcSelectedItems[nPlayer] : NULL;
	if (!pState || pState->Result >= 0)
	{
		Lua_PushNil(L);
		return 1;
	}
	lua_newtable(L);
	int nResult = Lua_GetTopIndex(L);
	for (size_t i = 0; i < pState->Items.size(); i++)
	{
		KNpcSelectedItem& Selected = pState->Items[i];
		if (!Selected.Remaining || Item[Selected.Index].GetID() != Selected.ID)
		{
			Lua_PushNil(L);
			return 1;
		}
		KItem& Data = Item[Selected.Index];
		lua_newtable(L);
		int nTable = Lua_GetTopIndex(L);
		SetScriptItemInteger(L, nTable, "Genre", Data.GetGenre());
		SetScriptItemInteger(L, nTable, "Detail", Data.GetDetailType());
		SetScriptItemInteger(L, nTable, "Particular", Data.GetParticular());
		SetScriptItemInteger(L, nTable, "Level", Data.GetLevel());
		SetScriptItemInteger(L, nTable, "Count", Data.GetStackCount());
		SetScriptItemInteger(L, nTable, "Bound", Data.IsBound() ? 1 : 0);
		SetScriptItemInteger(L, nTable, "BindingMode", Data.GetBindingMode());
		lua_rawseti(L, nResult, (int)i + 1);
	}
	return 1;
}

int LuaDelSelectedItem(Lua_State* L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	KNpcSelectedItems* pState = nPlayer > 0 && nPlayer < MAX_PLAYER ? g_NpcSelectedItems[nPlayer] : NULL;
	double nSlot = Lua_ValueToNumber(L, 1);
	double nCount = Lua_ValueToNumber(L, 2);
	if (Lua_GetTopIndex(L) != 2 || !pState || pState->Result >= 0 ||
		!Lua_IsNumber(L, 1) || !(nSlot >= 1 && nSlot <= pState->Items.size()) || nSlot != (int)nSlot ||
		!Lua_IsNumber(L, 2) || !(nCount >= 1 && nCount <= 100000) || nCount != (int)nCount ||
		!g_NpcScriptExchange[nPlayer])
	{
		RejectNpcScriptItem(L, nPlayer);
		Lua_PushNumber(L, 0);
		return 1;
	}
	KNpcSelectedItem& Selected = pState->Items[(int)nSlot - 1];
	if (Selected.Remaining < nCount || Item[Selected.Index].GetID() != Selected.ID ||
		Item[Selected.Index].GetStackCount() != Selected.Remaining ||
		!Player[nPlayer].m_ItemList.ConsumeItem(Selected.Index, (int)nCount))
	{
		RejectNpcScriptItem(L, nPlayer);
		Lua_PushNumber(L, 0);
		return 1;
	}
	Selected.Remaining -= (int)nCount;
	Lua_PushNumber(L, 1);
	return 1;
}

// 0为已完成的概率失败，1为已完成的成功；不调用表示整次操作未完成并回滚。
int LuaCompleteItemOperation(Lua_State* L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	KNpcSelectedItems* pState = nPlayer > 0 && nPlayer < MAX_PLAYER ? g_NpcSelectedItems[nPlayer] : NULL;
	double nResult = Lua_ValueToNumber(L, 1);
	if (Lua_GetTopIndex(L) != 1 || !pState || pState->Result >= 0 ||
		!Lua_IsNumber(L, 1) || nResult != 0 && nResult != 1 || !g_NpcScriptExchange[nPlayer])
	{
		RejectNpcScriptItem(L, nPlayer);
		Lua_PushNumber(L, 0);
		return 1;
	}
	pState->Result = (int)nResult;
	Lua_PushNumber(L, 1);
	return 1;
}

int LuaModifyRepute(Lua_State *L)
{
	if (Lua_GetTopIndex(L) <1) 
		return 0;
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0)
		return 0;
	int nValue = (int)Lua_ValueToNumber(L,1);
	
	int nOld = Player[nPlayerIndex].m_cTask.GetSaveVal(TASKVALUE_REPUTE);
	if ((__int64)nOld + nValue < 0 || (__int64)nOld + nValue > 0x7fffffff)
	{
		RejectNpcScriptItem(L, nPlayerIndex);
		return 0;
	}
	Player[nPlayerIndex].m_cTask.SetSaveVal(TASKVALUE_REPUTE, nOld + nValue);
	if (nValue < 0)
	{
		char szMsg[100];
		sprintf(szMsg, "您的声望减少了%d点!", -nValue);		
//		KPlayerChat::SendSystemInfo(1, nPlayerIndex, MESSAGE_SYSTEM_ANNOUCE_HEAD, (char *) szMsg, strlen(szMsg) );
	}
	else
	{
		char szMsg[100];
		sprintf(szMsg, "您的声望增加了%d点!", nValue);		
//		KPlayerChat::SendSystemInfo(1, nPlayerIndex, MESSAGE_SYSTEM_ANNOUCE_HEAD, (char *) szMsg, strlen(szMsg) );
	}
	
	
	return 0;	
}

int LuaGetRepute(Lua_State *L)
{
	int nValue = 0;
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0)
		goto lab_getrepute;
	nValue = Player[nPlayerIndex].m_cTask.GetSaveVal(TASKVALUE_REPUTE);
lab_getrepute:
	Lua_PushNumber(L, nValue);
	return 1;	
}
#endif

int GetSubWorldIndex(Lua_State * L)
{
	Lua_GetGlobal(L, SCRIPT_SUBWORLDINDEX);
	if (lua_isnil(L,Lua_GetTopIndex(L)))
        return -1;
	int nIndex = (int)Lua_ValueToNumber(L, Lua_GetTopIndex(L));
    if (nIndex >= MAX_SUBWORLD || nIndex <= 0) 
	{
		_ASSERT(0);
		return -1;
	}
	if (SubWorld[nIndex].m_nIndex >= MAX_SUBWORLD || SubWorld[nIndex].m_nIndex < 0)
	{
		_ASSERT(0);
		return -1;
	}
    return nIndex;
}

//Idx = SubWorldID2Idx(dwID)
int LuaSubWorldIDToIndex(Lua_State * L)
{
	int nTargetSubWorld = -1;
	int nSubWorldID = 0;
	if (Lua_GetTopIndex(L) < 1)
		goto lab_subworldid2idx;
	
	nSubWorldID = (int)Lua_ValueToNumber(L, 1);
	nTargetSubWorld = g_SubWorldSet.SearchWorld(nSubWorldID);	
	
lab_subworldid2idx:
	Lua_PushNumber(L, nTargetSubWorld);
	return 1;
}


/*
Say(sMainInfo, nSelCount, sSel1, sSel2, sSel3, .....,sSeln) 
Say(nMainInfo, nSelCount, sSel1, sSel2, sSel3, .....,sSeln) 
Say(nMainInfo, nSelCount, SelTab)
如果是客户端的则不会向服务器端发送任何操作

  Say(100, 3, 10, 23,43)
  Say("选择什么？", 2, "是/yes", "否/no");
  Say("选什么呀", 2, SelTab);
*/
//**************************************************************************************************************************************************************
//												界面脚本
//**************************************************************************************************************************************************************
int LuaSelectUI(Lua_State * L)
{
	char * strMain  = NULL;
	int nMainInfo = 0;
	int nDataType = 0;
	int nOptionNum = 0;
	char * pContent = NULL;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) return 0;
	Player[nPlayerIndex].m_bWaitingPlayerFeedBack = false;
	
	int nParamNum = Lua_GetTopIndex(L);
	if (nParamNum < 2) return 0;
	
	if (Lua_IsNumber(L,2))
	{
		nOptionNum = (int)Lua_ValueToNumber(L,2);
	}
	else 
	{
		_ASSERT(0);
		return 0;
	}
	
	if  (Lua_IsNumber(L,1))
	{
		nMainInfo = (int)Lua_ValueToNumber(L,1);
		nDataType = 1 ;
	}
	else if (Lua_IsString(L, 1)) 	//检查主信息是字符串还是字符串标识号
	{
		strMain = (char *)Lua_ValueToString(L, 1);
		nDataType = 0 ;
	}
	else
		return 0;
	
	BOOL bStringTab = FALSE;//标识传进来的选项数据存放在一个数组中，还是许多字符串里
	
	if (Lua_IsString(L,3))
		bStringTab = FALSE;
	else if (Lua_IsTable(L, 3))
	{
		bStringTab = TRUE;
	}
	else 
	{if (nOptionNum > 0)  return 0;
	}
	
	if (bStringTab == FALSE)
	{
		//获得实际传入的选项的个数
		if (nOptionNum > nParamNum - 2) nOptionNum = nParamNum - 2;
	}
	
	if (nOptionNum < 0) return 0;
	if (nOptionNum > MAX_ANSWERNUM) nOptionNum = MAX_ANSWERNUM;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_SELECTDIALOG;
	UiInfo.m_bParam1 = nDataType;//主信息的类型，字符串(0)或数字(1)
	UiInfo.m_bOptionNum = nOptionNum;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	//主信息为字符串
	if (nDataType == 0)
	{
		if (strMain)
			g_StrCpyLen(UiInfo.m_pContent, strMain, sizeof(UiInfo.m_pContent));
		pContent = UiInfo.m_pContent;
	}
	else if (nDataType == 1) //主信息为数字标识
	{
		*(int *)UiInfo.m_pContent = nMainInfo;
		pContent = UiInfo.m_pContent + sizeof(int);
		*pContent = 0;
	}
	
	if (nOptionNum > MAX_ANSWERNUM)
		nOptionNum = MAX_ANSWERNUM;
	
	Player[nPlayerIndex].m_nAvailableAnswerNum = nOptionNum;		
	
	for (int i  = 0; i < nOptionNum; i ++)
	{	
		char  pAnswer[100];
		pAnswer[0] = 0;
		
		if (bStringTab)
		{
			Lua_PushNumber(L, i + 1);
			Lua_RawGet(L, 3);
			char * pszString = (char *)Lua_ValueToString(L, Lua_GetTopIndex(L));
			if (pszString)
			{
				g_StrCpyLen(pAnswer, pszString, 100);
			}
		}
		else 
		{
			char * pszString = (char *)Lua_ValueToString(L, i + 3);
			if (pszString)
				g_StrCpyLen(pAnswer, pszString, 100);
		}
		
		char * pFunName = strstr(pAnswer, "/");
		
		if (pFunName)
		{
			g_StrCpyLen(Player[nPlayerIndex].m_szTaskAnswerFun[i], pFunName + 1, sizeof(Player[nPlayerIndex].m_szTaskAnswerFun[0]));
			*pFunName = 0;
		}
		else
		{
			strcpy(Player[nPlayerIndex].m_szTaskAnswerFun[i], "main");
		}
		size_t nContentLen = strlen(pContent);
		size_t nAnswerLen = strlen(pAnswer);
		size_t nPrefixLen = pContent - UiInfo.m_pContent;
		if (nPrefixLen + nContentLen + nAnswerLen + 2 > sizeof(UiInfo.m_pContent))
		{
			Player[nPlayerIndex].m_nAvailableAnswerNum = 0;
			return 0;
		}
		pContent[nContentLen] = '|';
		memcpy(pContent + nContentLen + 1, pAnswer, nAnswerLen + 1);
	}
	
	if (nDataType == 0)
		UiInfo.m_nBufferLen  = strlen(pContent);
	else 
		UiInfo.m_nBufferLen = strlen(pContent) + sizeof(int);
	
#ifndef _SERVER
	UiInfo.m_bParam2 = 0;
#else
	UiInfo.m_bParam2 = 1;
#endif
	
	if (nOptionNum == 0)
	{
		Player[nPlayerIndex].m_bWaitingPlayerFeedBack = false;
	}
	else
	{
		Player[nPlayerIndex].m_bWaitingPlayerFeedBack = true;
	}
	
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
	return 0;
}



// 原任务脚本的菜单项为 {文字, 回调; show=0/1}，过滤后再绑定反馈下标。
int LuaSayTask(Lua_State *L)
{
	int nParamNum = Lua_GetTopIndex(L);
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0 || nParamNum < 2 || !Lua_IsTable(L, 2))
		return 0;

	KPlayer& Role = Player[nPlayerIndex];
	Role.m_bWaitingPlayerFeedBack = false;
	Role.m_nAvailableAnswerNum = 0;
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_SELECTDIALOG;
	UiInfo.m_bParam2 = 1;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	UiInfo.m_nParam = 1;
	int nLength = 0;
	if (Lua_IsNumber(L, 1))
	{
		int nQuestion = (int)Lua_ValueToNumber(L, 1);
		UiInfo.m_bParam1 = 1;
		memcpy(UiInfo.m_pContent, &nQuestion, sizeof(nQuestion));
		nLength = sizeof(nQuestion);
	}
	else if (Lua_IsString(L, 1))
	{
		const char *pszQuestion = Lua_ValueToString(L, 1);
		nLength = strlen(pszQuestion);
		if (nLength >= sizeof(UiInfo.m_pContent))
			return 0;
		memcpy(UiInfo.m_pContent, pszQuestion, nLength);
	}
	else
		return 0;

	for (int nRow = 1; nRow <= 256; nRow++)
	{
		Lua_RawGetI(L, 2, nRow);
		int nTable = Lua_GetTopIndex(L);
		if (lua_isnil(L, nTable))
		{
			Lua_Pop(L, 1);
			break;
		}
		if (!Lua_IsTable(L, nTable))
		{
			Lua_Pop(L, 1);
			continue;
		}
		Lua_PushString(L, "show");
		Lua_GetTable(L, nTable);
		BOOL bShow = lua_isnil(L, -1) || Lua_ValueToNumber(L, -1) != 0;
		Lua_Pop(L, 1);
		if (!bShow)
		{
			Lua_Pop(L, 1);
			continue;
		}
		char szLabel[100];
		char szFunction[sizeof(Role.m_szTaskAnswerFun[0])];
		ZeroMemory(szLabel, sizeof(szLabel));
		ZeroMemory(szFunction, sizeof(szFunction));
		Lua_RawGetI(L, nTable, 1);
		const char *pszLabel = Lua_ValueToString(L, -1);
		BOOL bValid = pszLabel && strlen(pszLabel) < sizeof(szLabel);
		if (bValid)
			strcpy(szLabel, pszLabel);
		Lua_Pop(L, 1);
		Lua_RawGetI(L, nTable, 2);
		const char *pszFunction = Lua_ValueToString(L, -1);
		bValid = bValid && pszFunction && strlen(pszFunction) < sizeof(szFunction);
		if (bValid)
			strcpy(szFunction, pszFunction);
		Lua_Pop(L, 2);
		int nLabelLength = strlen(szLabel);
		if (!bValid || !nLabelLength || !szFunction[0] ||
			Role.m_nAvailableAnswerNum >= MAX_ANSWERNUM ||
			nLength + nLabelLength + 1 >= sizeof(UiInfo.m_pContent))
		{
			Role.m_nAvailableAnswerNum = 0;
			return 0;
		}
		UiInfo.m_pContent[nLength++] = '|';
		memcpy(UiInfo.m_pContent + nLength, szLabel, nLabelLength);
		nLength += nLabelLength;
		strcpy(Role.m_szTaskAnswerFun[Role.m_nAvailableAnswerNum++], szFunction);
	}
	UiInfo.m_bOptionNum = Role.m_nAvailableAnswerNum;
	UiInfo.m_nBufferLen = nLength;
	Role.m_bWaitingPlayerFeedBack = Role.m_nAvailableAnswerNum > 0;
	Role.DoScriptAction(&UiInfo);
	return 0;
}

// MsgBox 使用客户端原生确认框；与普通 Say 的选项文字布局不同。
int LuaMsgBox(Lua_State *L)
{
	int nParamNum = Lua_GetTopIndex(L);
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0 || nParamNum < 2 || nParamNum > 3)
		return 0;
	KPlayer& Role = Player[nPlayerIndex];
	Role.m_bWaitingPlayerFeedBack = false;
	Role.m_nAvailableAnswerNum = 0;
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_SELECTDIALOG;
	UiInfo.m_bParam2 = 1;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	UiInfo.m_nParam = nParamNum == 3 ? 3 : 1;
	if (Lua_IsNumber(L, 1))
	{
		int nQuestion = (int)Lua_ValueToNumber(L, 1);
		UiInfo.m_bParam1 = 1;
		UiInfo.m_nBufferLen = sizeof(nQuestion);
		memcpy(UiInfo.m_pContent, &nQuestion, sizeof(nQuestion));
	}
	else if (Lua_IsString(L, 1))
	{
		const char *pszQuestion = Lua_ValueToString(L, 1);
		UiInfo.m_nBufferLen = strlen(pszQuestion);
		if (UiInfo.m_nBufferLen >= sizeof(UiInfo.m_pContent))
			return 0;
		memcpy(UiInfo.m_pContent, pszQuestion, UiInfo.m_nBufferLen);
	}
	else
		return 0;
	for (int i = 2; i <= nParamNum; i++)
	{
		const char *pszFunction = Lua_ValueToString(L, i);
		if (!pszFunction || !pszFunction[0] ||
			strlen(pszFunction) >= sizeof(Role.m_szTaskAnswerFun[0]))
			return 0;
		strcpy(Role.m_szTaskAnswerFun[i - 2], pszFunction);
	}
	Role.m_nAvailableAnswerNum = nParamNum - 1;
	Role.m_bWaitingPlayerFeedBack = true;
	Role.DoScriptAction(&UiInfo);
	return 0;
}

// 四参数旧脚本只匹配普通背包；数量按堆叠计，不包括仓库、身穿和鼠标物品。
static BOOL MatchNormalItem(PlayerItem *pItem, int nGenre, int nDetail, int nParticular, int nLevel)
{
	if (!pItem || pItem->nPlace != pos_equiproom || pItem->nIdx <= 0 || pItem->nIdx >= MAX_ITEM)
		return FALSE;
	KItem& Data = Item[pItem->nIdx];
	return Data.GetGenre() == nGenre && Data.GetDetailType() == nDetail &&
		Data.GetParticular() == nParticular && Data.GetLevel() == nLevel;
}

static int CountNormalItems(KItemList& List, int nGenre, int nDetail, int nParticular, int nLevel)
{
	int nCount = 0;
	for (PlayerItem *pItem = List.GetFirstItem(); pItem; pItem = List.GetNextItem())
	{
		if (MatchNormalItem(pItem, nGenre, nDetail, nParticular, nLevel))
			nCount += Item[pItem->nIdx].GetStackCount();
	}
	return nCount;
}


#ifdef _SERVER
// 这些接口供原技能丹、临仙露脚本使用；未适配的BUFF编号不能产生占位效果。
static KSkillList* GetPlayerScriptSkills(Lua_State* L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER ||
		Player[nPlayer].m_nIndex <= 0 || Player[nPlayer].m_nIndex >= MAX_NPC)
		return NULL;
	return &Npc[Player[nPlayer].m_nIndex].m_SkillList;
}

int LuaIsSkillActived(Lua_State* L)
{
	KSkillList* pSkills = GetPlayerScriptSkills(L);
	int nSkill = (int)Lua_ValueToNumber(L, 1);
	Lua_PushNumber(L, pSkills && pSkills->IsPlayerSkillLearned(nSkill) ? 1 : 0);
	return 1;
}

int LuaIsExistBuff(Lua_State* L)
{
	KSkillList* pSkills = GetPlayerScriptSkills(L);
	int nBuff = (int)Lua_ValueToNumber(L, 1);
	Lua_PushNumber(L, pSkills && pSkills->GetPlayerSkillBuffTime(nBuff) ? 1 : 0);
	return 1;
}

int LuaGetIBBuffLeftTimes(Lua_State* L)
{
	KSkillList* pSkills = GetPlayerScriptSkills(L);
	int nBuff = (int)Lua_ValueToNumber(L, 1);
	Lua_PushNumber(L, pSkills ? pSkills->GetPlayerSkillBuffTime(nBuff) : 0);
	return 1;
}

int LuaRemoveBuff(Lua_State* L)
{
	KSkillList* pSkills = GetPlayerScriptSkills(L);
	int nBuff = (int)Lua_ValueToNumber(L, 1);
	Lua_PushNumber(L, pSkills && pSkills->RemovePlayerSkillBuff(nBuff) ? 1 : 0);
	return 1;
}

int LuaGetMorphType(Lua_State* L)
{
	int nPlayer = GetPlayerIndex(L);
	Lua_PushNumber(L, nPlayer > 0 ? Npc[Player[nPlayer].m_nIndex].GetMorphType() : -1);
	return 1;
}

int LuaPolyMorph(Lua_State* L)
{
	int nCount = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nCount);
	int Args[5] = {0};
	BOOL bValid = nPlayer > 0 && nCount == 5;
	for (int i = 0; i < 5 && bValid; i++)
	{
		double Value = Lua_ValueToNumber(L, i + 1);
		bValid = Lua_IsNumber(L, i + 1) && Value >= -1 && Value <= 0x7fffffff && Value == (int)Value;
		Args[i] = (int)Value;
	}
	if (!bValid || !Npc[Player[nPlayer].m_nIndex].CanPolyMorph(Args[0], Args[1], Args[2], Args[3], Args[4]))
	{
		RejectNpcScriptItem(L, nPlayer);
		Lua_PushNumber(L, 0);
		return 1;
	}
	KNpcScriptExchange* pState = g_NpcScriptExchange[nPlayer];
	if (pState)
	{
		if (pState->HasMorph || pState->HasTeleport)
			RejectNpcScriptItem(L, nPlayer);
		pState->HasMorph = TRUE;
		pState->MorphType = Args[0];
		pState->MorphCanCast = Args[1];
		pState->MorphSeconds = Args[4];
		Lua_PushNumber(L, 1);
	}
	else
		Lua_PushNumber(L, Npc[Player[nPlayer].m_nIndex].PolyMorph(Args[0], Args[1], Args[2], Args[3], Args[4]) ? 1 : 0);
	return 1;
}

int LuaGetLastSkillIdx(Lua_State* L)
{
	KSkillList* pSkills = GetPlayerScriptSkills(L);
	int nLast = 0;
	for (int i = 1; pSkills && i < MAX_NPCSKILL; i++)
	{
		if (pSkills->m_Skills[i].SkillId)
			nLast = i;
	}
	Lua_PushNumber(L, nLast);
	return 1;
}

int LuaIsSkillActivedByIdx(Lua_State* L)
{
	KSkillList* pSkills = GetPlayerScriptSkills(L);
	int nIndex = (int)Lua_ValueToNumber(L, 1);
	Lua_PushNumber(L, pSkills && nIndex > 0 && nIndex < MAX_NPCSKILL &&
		pSkills->m_Skills[nIndex].SkillId && pSkills->m_Skills[nIndex].SkillStatus == 3 ? 1 : 0);
	return 1;
}

int LuaRoulette(Lua_State* L)
{
	int nCount = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nCount);
	int nResult = (int)Lua_ValueToNumber(L, 1);
	DWORD dwScript = nPlayer > 0 ? Npc[Player[nPlayer].m_nIndex].m_ActionScriptID : 0;
	char Missing[128];
	if (nCount != 1 || !Lua_IsNumber(L, 1) || Lua_ValueToNumber(L, 1) != nResult ||
		nResult < 0 || nResult >= 12 || !dwScript ||
		!g_CheckNpcScriptFunction(dwScript, "Finished", Missing, sizeof(Missing), 0))
	{
		RejectNpcScriptItem(L, nPlayer);
		Lua_PushNumber(L, 0);
		return 1;
	}
	PLAYER_SCRIPTACTION_SYNC Info;
	ZeroMemory(&Info, sizeof(Info));
	Info.m_bUIId = UI_ROULETTE;
	Info.m_bParam2 = 1;
	Info.m_nBufferLen = 4;
	WORD Content[2] = {(WORD)nResult, 15}; // 第二WORD选择客户端原转盘Tips配置15，来自正式服包。
	memcpy(Info.m_pContent, Content, sizeof(Content));
	Player[nPlayer].m_bWaitingPlayerFeedBack = false;
	Player[nPlayer].m_nAvailableAnswerNum = 0;
	Player[nPlayer].DoScriptAction(&Info);
	Lua_PushNumber(L, 1);
	return 1;
}

int LuaAddBuff(Lua_State* L)
{
	KSkillList* pSkills = GetPlayerScriptSkills(L);
	if (!pSkills || Lua_GetTopIndex(L) != 5)
	{
		Lua_PushNumber(L, 0);
		return 1;
	}
	int nBuff = (int)Lua_ValueToNumber(L, 1);
	int nTime = (int)Lua_ValueToNumber(L, 2);
	int nFirst = (int)Lua_ValueToNumber(L, 3);
	int nSecond = (int)Lua_ValueToNumber(L, 4);
	BOOL bAdded = FALSE;
	int nPlayer = pSkills ? Npc[pSkills->m_nNpcIndex].GetPlayerIdx() : 0;
	BOOL bSync = !nPlayer || !g_NpcScriptExchange[nPlayer];
	if (nBuff == 5 || nBuff == 7 || nBuff == 12 || nBuff == 13 || nBuff == 14 || nBuff == 23)
		bAdded = pSkills->AddPlayerSkillBuff(nBuff, nTime, 0, nFirst, FALSE, bSync);
	else if (nBuff == 8)
		bAdded = pSkills->AddPlayerSkillBuff(nBuff, nTime, nFirst, nSecond, FALSE, bSync);
	if (!bAdded)
		RejectNpcScriptItem(L, nPlayer);
	Lua_PushNumber(L, bAdded ? 1 : 0);
	return 1;
}

static BOOL CanAddScriptDew(KSkillList& Skills, int nBuff)
{
	int Buffs[] = {107, 271, 272};
	const PLAYER_SKILL_BUFF_RULE* pRule = g_SkillManager.GetPlayerBuffRule(nBuff);
	if (!pRule || Skills.GetPlayerSkillBuffTime(nBuff) > pRule->MaxDuration - pRule->Duration)
		return FALSE;
	for (int i = 0; i < 3; i++)
	{
		if (nBuff != Buffs[i] && Skills.GetPlayerSkillBuffTime(Buffs[i]))
			return FALSE;
	}
	return TRUE;
}

// 原临仙露回调先DelNormalItem再AddIBBuff；扣物品时再次检查互斥和100小时上限。
static BOOL CanConsumeScriptDew(Lua_State* L, int nGenre, int nDetail, int nParticular)
{
	if (nGenre != item_magicorscript || nDetail != 1)
		return TRUE;
	int nBuff = 0;
	if (nParticular == 508 || nParticular == 512 || nParticular == 1091)
		nBuff = 107;
	else if (nParticular == 942 || nParticular == 943 || nParticular == 1092)
		nBuff = 271;
	else if (nParticular == 944 || nParticular == 945 || nParticular == 1093)
		nBuff = 272;
	if (!nBuff)
		return TRUE;
	KSkillList* pSkills = GetPlayerScriptSkills(L);
	return pSkills && CanAddScriptDew(*pSkills, nBuff);
}

int LuaAddIBBuff(Lua_State* L)
{
	KSkillList* pSkills = GetPlayerScriptSkills(L);
	int nBuff = (int)Lua_ValueToNumber(L, 1);
	const PLAYER_SKILL_BUFF_RULE* pRule = g_SkillManager.GetPlayerBuffRule(nBuff);
	BOOL bAdded = pSkills && pRule && CanAddScriptDew(*pSkills, nBuff) &&
		pSkills->AddPlayerSkillBuff(nBuff, pRule->Duration, 0, pRule->Multiplier, TRUE);
	Lua_PushNumber(L, bAdded ? 1 : 0);
	return 1;
}

static int QuickScriptItem(Lua_State* L, BOOL bConsume)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER || Lua_GetTopIndex(L) != 4)
	{
		Lua_PushNumber(L, 0);
		return 1;
	}
	int nGenre = (int)Lua_ValueToNumber(L, 1);
	int nDetail = (int)Lua_ValueToNumber(L, 2);
	int nParticular = (int)Lua_ValueToNumber(L, 3);
	int nLevel = (int)Lua_ValueToNumber(L, 4);
	KItemList& Items = Player[nPlayer].m_ItemList;
	int nCount = 0;
	for (PlayerItem* pItem = Items.GetFirstItem(); pItem; pItem = Items.GetNextItem())
	{
		if (pItem->nPlace != pos_immediacy || pItem->nIdx <= 0 || pItem->nIdx >= MAX_ITEM)
			continue;
		KItem& Data = Item[pItem->nIdx];
		if (Data.GetGenre() != nGenre || Data.GetDetailType() != nDetail ||
			Data.GetParticular() != nParticular || Data.GetLevel() != nLevel)
			continue;
		if (bConsume)
		{
			BOOL bAllowed = CanConsumeScriptDew(L, nGenre, nDetail, nParticular);
			Lua_PushNumber(L, bAllowed && Items.ConsumeItem(pItem->nIdx, 1) ? 1 : 0);
			return 1;
		}
		nCount += Data.GetStackCount();
	}
	Lua_PushNumber(L, nCount);
	return 1;
}

int LuaHaveNormalItemInQuick(Lua_State* L)
{
	return QuickScriptItem(L, FALSE);
}

int LuaDelNormalItemInQuick(Lua_State* L)
{
	return QuickScriptItem(L, TRUE);
}
#endif

#ifdef _SERVER
// 原瑶池脚本的 Item2 系列仍按种类、类别、具体类别、等级匹配。
// 第四参数是装备资源等级，不是绑定标志；仓库、装备栏和鼠标不参与交材料。
int LuaGetItemLevel2(Lua_State *L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	int nLevel = 0;
	if (nPlayer > 0 && nTop == 3)
	{
		int nGenre = (int)Lua_ValueToNumber(L, 1);
		int nDetail = (int)Lua_ValueToNumber(L, 2);
		int nParticular = (int)Lua_ValueToNumber(L, 3);
		KItemList& List = Player[nPlayer].m_ItemList;
		for (PlayerItem *pItem = List.GetFirstItem(); pItem; pItem = List.GetNextItem())
		{
			if (pItem->nPlace != pos_equiproom || pItem->nIdx <= 0 || pItem->nIdx >= MAX_ITEM)
				continue;
			KItem& Data = Item[pItem->nIdx];
			if (Data.GetGenre() == nGenre && Data.GetDetailType() == nDetail &&
				Data.GetParticular() == nParticular && Data.GetLevel() > nLevel)
				nLevel = Data.GetLevel();
		}
	}
	Lua_PushNumber(L, nLevel);
	return 1;
}

// Credit 与本服现有 AddRepute/GetRepute 共用持久声望，不能另建隐形余额。
int LuaDecCredit(Lua_State *L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	int nResult = 0;
	if (nPlayer > 0 && nTop == 1 && Lua_IsNumber(L, 1))
	{
		double nAmount = Lua_ValueToNumber(L, 1);
		int nCredit = Player[nPlayer].m_cTask.GetSaveVal(TASKVALUE_REPUTE);
		if (nAmount > 0 && nAmount <= nCredit && nAmount == (int)nAmount)
		{
			Player[nPlayer].m_cTask.SetSaveVal(TASKVALUE_REPUTE, nCredit - (int)nAmount);
			nResult = 1;
		}
	}
	Lua_PushNumber(L, nResult);
	return 1;
}
#endif

int LuaHaveNormalItem(Lua_State *L)
{
	int nParamNum = Lua_GetTopIndex(L);
	int nPlayerIndex = GetPlayerIndex(L);
	int nCount = 0;
	if (nPlayerIndex > 0 && nParamNum == 4)
	{
		nCount = CountNormalItems(Player[nPlayerIndex].m_ItemList,
			(int)Lua_ValueToNumber(L, 1), (int)Lua_ValueToNumber(L, 2),
			(int)Lua_ValueToNumber(L, 3), (int)Lua_ValueToNumber(L, 4));
	}
	Lua_PushNumber(L, nCount);
	return 1;
}

#ifdef _SERVER
static BOOL ReadReforgeInteger(Lua_State* L, int nTable, const char* pszKey, int* pnValue)
{
	Lua_PushString(L, pszKey);
	Lua_GetTable(L, nTable);
	BOOL bValid = Lua_IsNumber(L, -1);
	if (bValid)
		*pnValue = (int)Lua_ValueToNumber(L, -1);
	Lua_Pop(L, 1);
	return bValid;
}

int LuaGetCopperCashCount(Lua_State* L)
{
	int nPlayer = GetPlayerIndex(L);
	Lua_PushNumber(L, nPlayer > 0 ? Player[nPlayer].m_ItemList.GetCopperCashCount() : 0);
	return 1;
}

// 原玉石还原脚本的费用来自自身配方，按背包实际铜钱数量扣除并同步通知。
int LuaWasteCopperCash(Lua_State *L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	if (nPlayer <= 0 || nTop != 1 || !Lua_IsNumber(L, 1))
	{
		RejectNpcScriptItem(L, nPlayer);
		Lua_PushNumber(L, 0);
		return 1;
	}
	double nAmount = Lua_ValueToNumber(L, 1);
	KItemList& List = Player[nPlayer].m_ItemList;
	if (nAmount <= 0 || nAmount > List.GetCopperCashCount() || nAmount != (int)nAmount)
	{
		RejectNpcScriptItem(L, nPlayer);
		Lua_PushNumber(L, 0);
		return 1;
	}
	std::vector<int> Costs;
	for (PlayerItem *pItem = List.GetFirstItem(); pItem; pItem = List.GetNextItem())
	{
		if (pItem->nPlace == pos_equiproom && pItem->nIdx > 0 &&
			Item[pItem->nIdx].GetGenre() == item_task && Item[pItem->nIdx].GetDetailType() == 47)
			Costs.push_back(pItem->nIdx);
	}
	int nLeft = (int)nAmount;
	for (size_t i = 0; i < Costs.size() && nLeft > 0; i++)
	{
		int nTake = Item[Costs[i]].GetStackCount();
		if (nTake > nLeft)
			nTake = nLeft;
		if (!List.ConsumeItem(Costs[i], nTake))
		{
			RejectNpcScriptItem(L, nPlayer);
			Lua_PushNumber(L, 0);
			return 1;
		}
		nLeft -= nTake;
	}
	GAME_CURRENCY_NOTICE_SYNC Notice;
	ZeroMemory(&Notice, sizeof(Notice));
	Notice.ProtocolType = 0xCC;
	Notice.Currency = 1;
	Notice.Amount = (int)nAmount;
	if (g_NpcScriptExchange[nPlayer])
	{
		KNpcScriptNotice Pending;
		Pending.Raw.assign((BYTE*)&Notice, (BYTE*)&Notice + sizeof(Notice));
		g_NpcScriptExchange[nPlayer]->Notices.push_back(Pending);
	}
	else
		g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, &Notice, sizeof(Notice));
	Lua_PushNumber(L, 1);
	return 1;
}

int LuaImproveItemMagicAttr(Lua_State* L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	int Args[10] = {0};
	BOOL bValid = nPlayer > 0 && nTop == 10;
	for (int i = 0; i < 10 && bValid; i++)
	{
		bValid = Lua_IsNumber(L, i + 1);
		if (bValid)
			Args[i] = (int)Lua_ValueToNumber(L, i + 1);
	}
	if (!bValid || Args[0] != item_equip || Args[4] != item_equip || Args[1] != Args[5])
	{
		Lua_PushNumber(L, -1);
		return 1;
	}
	// 第九参数1000保留原脚本位置，其业务含义未确认，不解释为概率。
	// 第十参数仅用于回查当前原脚本的SuitMaterial、ItemCostIBIndex，费用不另建表。
	Lua_GetGlobal(L, "SuitMaterial");
	int nTable = Lua_GetTopIndex(L);
	bValid = Lua_IsTable(L, nTable);
	if (bValid)
	{
		Lua_RawGetI(L, nTable, Args[9]);
		nTable = Lua_GetTopIndex(L);
		bValid = Lua_IsTable(L, nTable);
	}
	int Identity[4] = {0};
	const char* Keys[] = {"id1", "id2", "id3", "id4"};
	for (int i = 0; i < 4 && bValid; i++)
		bValid = ReadReforgeInteger(L, nTable, Keys[i], &Identity[i]);
	bValid = bValid && Identity[0] == Args[4] && Identity[1] == Args[5] &&
		Identity[2] == Args[6] && Args[7] >= Identity[3] && Args[7] <= 10;
	int nCostSlot = 0;
	int nCount = 0;
	if (bValid)
	{
		Lua_PushString(L, "CostItem");
		Lua_GetTable(L, nTable);
		nTable = Lua_GetTopIndex(L);
		bValid = Lua_IsTable(L, nTable);
		for (int i = 1; i <= 6 && bValid; i++)
		{
			Lua_RawGetI(L, nTable, i);
			bValid = Lua_IsNumber(L, -1);
			int nValue = bValid ? (int)Lua_ValueToNumber(L, -1) : -1;
			Lua_Pop(L, 1);
			if (nValue < 0 || nValue > 1000 || (nValue && nCostSlot))
				bValid = FALSE;
			if (nValue > 0)
			{
				nCostSlot = i;
				nCount = nValue;
			}
		}
	}
	int Cost[4] = {0};
	int nCopperUnit = 0;
	if (bValid && nCostSlot)
	{
		Lua_GetGlobal(L, "ItemCostIBIndex");
		nTable = Lua_GetTopIndex(L);
		bValid = Lua_IsTable(L, nTable);
		if (bValid)
		{
			Lua_RawGetI(L, nTable, nCostSlot);
			nTable = Lua_GetTopIndex(L);
			bValid = Lua_IsTable(L, nTable);
		}
		for (int i = 0; i < 4 && bValid; i++)
			bValid = ReadReforgeInteger(L, nTable, Keys[i], &Cost[i]);
		if (bValid)
			bValid = ReadReforgeInteger(L, nTable, "Coppernums", &nCopperUnit);
	}
	else
		bValid = FALSE;
	Lua_SetTopIndex(L, nTop);
	int nResult = -1;
	if (bValid && nCopperUnit > 0 && nCopperUnit <= 100 && Cost[0] == item_materials)
	{
		KItemList& List = Player[nPlayer].m_ItemList;
		int nItem = 0;
		int nMatches = 0;
		for (PlayerItem* p = List.GetFirstItem(); p; p = List.GetNextItem())
		{
			if (MatchNormalItem(p, Args[0], Args[1], Args[2], Args[3]))
			{
				nItem = p->nIdx;
				nMatches += Item[nItem].GetStackCount();
			}
		}
		if (nMatches == 1)
			nResult = List.ReforgeItem(nItem, Args[6], Args[7], Cost[0], Cost[1], Cost[2], Cost[3],
				nCount, nCopperUnit * nCount);
	}
	if (nResult < 0)
		g_DebugLog("[Reforge] Rejected script transaction: player=%d table=%d", nPlayer, Args[9]);
	Lua_PushNumber(L, nResult);
	return 1;
}
#endif

int LuaDelNormalItem(Lua_State *L)
{
	int nParamNum = Lua_GetTopIndex(L);
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0 || nParamNum < 4 || nParamNum > 5)
	{
#ifdef _SERVER
		RejectNpcScriptItem(L, nPlayerIndex);
#endif
		Lua_PushNumber(L, 0);
		return 1;
	}
	int nGenre = (int)Lua_ValueToNumber(L, 1);
	int nDetail = (int)Lua_ValueToNumber(L, 2);
	int nParticular = (int)Lua_ValueToNumber(L, 3);
	int nLevel = (int)Lua_ValueToNumber(L, 4);
	int nCount = nParamNum == 5 ? (int)Lua_ValueToNumber(L, 5) : 1;
	KItemList& List = Player[nPlayerIndex].m_ItemList;
	if (nCount <= 0 || CountNormalItems(List, nGenre, nDetail, nParticular, nLevel) < nCount
#ifdef _SERVER
		|| !CanConsumeScriptDew(L, nGenre, nDetail, nParticular)
#endif
		)
	{
#ifdef _SERVER
		RejectNpcScriptItem(L, nPlayerIndex);
#endif
		Lua_PushNumber(L, 0);
		return 1;
	}
	// 先收集索引，避免 Remove 改变 GetNextItem 的遍历链。
	int Indices[MAX_PLAYER_ITEM];
	int nItems = 0;
	for (PlayerItem *pItem = List.GetFirstItem(); pItem; pItem = List.GetNextItem())
	{
		if (MatchNormalItem(pItem, nGenre, nDetail, nParticular, nLevel))
			Indices[nItems++] = pItem->nIdx;
	}
	for (int i = 0; i < nItems && nCount > 0; i++)
	{
		int nIndex = Indices[i];
		int nStack = Item[nIndex].GetStackCount();
		if (nStack > nCount)
		{
			Item[nIndex].SetStackCount(nStack - nCount);
			List.SyncItemCount(nIndex);
			nCount = 0;
		}
		else
		{
			if (!List.Remove(nIndex))
				break;
			ItemSet.Remove(nIndex);
			nCount -= nStack;
		}
	}
#ifdef _SERVER
	if (nCount > 0)
		RejectNpcScriptItem(L, nPlayerIndex);
#endif
	Lua_PushNumber(L, nCount == 0 ? 1 : 0);
	return 1;
}

int LuaTaskNote(Lua_State *L)
{
	int nParamNum = Lua_GetTopIndex(L);
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0 || nParamNum < 2 || nParamNum > 3)
		return 0;
	int nTaskId = (int)Lua_ValueToNumber(L, 1);
	int nStep = (int)Lua_ValueToNumber(L, 2);
	const char *pszText = nParamNum == 3 ? Lua_ValueToString(L, 3) : "";
	KPlayerTask& Tasks = Player[nPlayerIndex].m_cTask;
	if (!pszText || !Tasks.SetNote(nTaskId, nStep, pszText))
	{
		g_DebugLog("[Task] Rejected TaskNote: player=%d task=%d step=%d", nPlayerIndex, nTaskId, nStep);
		return 0;
	}
	Tasks.SendNote(nPlayerIndex, nTaskId, nStep, pszText);
	return 0;
}

//PutMsg(szMsg/MsgId)
int LuaSendMessageInfo(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) return 0;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_MSGINFO;
	UiInfo.m_bOptionNum = 1;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	int nMsgId = 0;
	
	if (Lua_IsNumber(L,1))
	{
		nMsgId = (int)Lua_ValueToNumber(L,1);
		*((int *)(UiInfo.m_pContent)) = nMsgId;
		UiInfo.m_bParam1 = 1;
		UiInfo.m_nBufferLen = sizeof(int);
	}
	else 
	{
		
		g_StrCpyLen(UiInfo.m_pContent, Lua_ValueToString(L,1), 64);
		UiInfo.m_nBufferLen = strlen(((char *)UiInfo.m_pContent));
		UiInfo.m_bParam1 = 0;
	}
	
#ifndef _SERVER
	UiInfo.m_bParam2 = 0;
#else
	UiInfo.m_bParam2 = 1;
#endif
#ifdef _SERVER
	if (nPlayerIndex > 0 && g_NpcScriptExchange[nPlayerIndex])
	{
		KNpcScriptNotice Notice;
		Notice.IsChat = FALSE;
		Notice.Action = UiInfo;
		g_NpcScriptExchange[nPlayerIndex]->Notices.push_back(Notice);
	}
	else
#endif
		Player[nPlayerIndex].DoScriptAction(&UiInfo);
	return 0;
}

//AddGlobalNews(Newsstr)
int LuaAddGlobalNews(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_NEWSINFO;
	UiInfo.m_bOptionNum = NEWSMESSAGE_NORMAL;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	int nMsgId = 0;
	
	if (Lua_IsNumber(L,1))
	{
		nMsgId = (int)Lua_ValueToNumber(L,1);
		*((int *)(UiInfo.m_pContent)) = nMsgId;
		UiInfo.m_bParam1 = 1;
		UiInfo.m_nBufferLen = sizeof(int);
	}
	else 
	{
		g_StrCpyLen(UiInfo.m_pContent, Lua_ValueToString(L,1), 64);
		UiInfo.m_nBufferLen = strlen(((char *)UiInfo.m_pContent));
		UiInfo.m_bParam1 = 0;
	}
	
#ifndef _SERVER
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) return 0;

	UiInfo.m_bParam2 = 0;
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
#else
	UiInfo.m_bParam2 = 1;
	UiInfo.ProtocolType = (BYTE)s2c_scriptaction;
	UiInfo.m_wProtocolLong = sizeof(PLAYER_SCRIPTACTION_SYNC) - MAX_SCIRPTACTION_BUFFERNUM + UiInfo.m_nBufferLen - 1;
	g_NewProtocolProcess.BroadcastGlobal(&UiInfo, UiInfo.m_wProtocolLong + 1);
#endif
	return 0;
}

//AddLocalNews(Newsstr)
int LuaAddLocalNews(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_NEWSINFO;
	UiInfo.m_bOptionNum = NEWSMESSAGE_NORMAL;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	int nMsgId = 0;
	
	if (Lua_IsNumber(L,1))
	{
		nMsgId = (int)Lua_ValueToNumber(L,1);
		*((int *)(UiInfo.m_pContent)) = nMsgId;
		UiInfo.m_bParam1 = 1;
		UiInfo.m_nBufferLen = sizeof(int);
	}
	else 
	{
		g_StrCpyLen(UiInfo.m_pContent, Lua_ValueToString(L,1), 64);
		UiInfo.m_nBufferLen = strlen(((char *)UiInfo.m_pContent));
		UiInfo.m_bParam1 = 0;
	}
	
#ifndef _SERVER
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) return 0;

	UiInfo.m_bParam2 = 0;
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
#else
	UiInfo.m_bParam2 = 1;
	UiInfo.ProtocolType = (BYTE)s2c_scriptaction;
	UiInfo.m_wProtocolLong = sizeof(PLAYER_SCRIPTACTION_SYNC) - MAX_SCIRPTACTION_BUFFERNUM + UiInfo.m_nBufferLen - 1;
	g_NewProtocolProcess.BroadcastLocalServer(&UiInfo, UiInfo.m_wProtocolLong + 1);
#endif
	return 0;
}

//AddGlobalCountNews(strNew/newid, time)
int LuaAddGlobalCountNews(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 2) 
		return 0;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_NEWSINFO;
	UiInfo.m_bOptionNum = NEWSMESSAGE_COUNTING;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	int nMsgId = 0;
	
	int nTime = (int)Lua_ValueToNumber(L,2);
	
	if (nTime <= 0)
		nTime = 1;
	
	if (Lua_IsNumber(L,1))
	{
		nMsgId = (int)Lua_ValueToNumber(L,1);
		*((int *)(UiInfo.m_pContent)) = nMsgId;
		UiInfo.m_bParam1 = 1;
		*(int *)((char *)UiInfo.m_pContent + sizeof(int)) = nTime;
		UiInfo.m_nBufferLen = sizeof(int) * 2;
	}
	else 
	{
		g_StrCpyLen(UiInfo.m_pContent, Lua_ValueToString(L,1), 64);
		UiInfo.m_nBufferLen = strlen(((char *)UiInfo.m_pContent));
		*(int *)((char *)UiInfo.m_pContent + UiInfo.m_nBufferLen) = nTime;
		UiInfo.m_nBufferLen += sizeof(int);
		UiInfo.m_bParam1 = 0;
	}
	
#ifndef _SERVER
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) return 0;

	UiInfo.m_bParam2 = 0;
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
#else
	UiInfo.m_bParam2 = 1;
	UiInfo.ProtocolType = (BYTE)s2c_scriptaction;
	UiInfo.m_wProtocolLong = sizeof(PLAYER_SCRIPTACTION_SYNC) - MAX_SCIRPTACTION_BUFFERNUM + UiInfo.m_nBufferLen - 1;
	g_NewProtocolProcess.BroadcastGlobal(&UiInfo, UiInfo.m_wProtocolLong + 1);
#endif
	return 0;
}

//AddLocalCountNews(strNew/newid, time)
int LuaAddLocalCountNews(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 2) 
		return 0;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_NEWSINFO;
	UiInfo.m_bOptionNum = NEWSMESSAGE_COUNTING;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	int nMsgId = 0;
	
	int nTime = (int)Lua_ValueToNumber(L,2);
	
	if (nTime <= 0)
		nTime = 1;
	
	if (Lua_IsNumber(L,1))
	{
		nMsgId = (int)Lua_ValueToNumber(L,1);
		*((int *)(UiInfo.m_pContent)) = nMsgId;
		UiInfo.m_bParam1 = 1;
		*(int *)((char *)UiInfo.m_pContent + sizeof(int)) = nTime;
		UiInfo.m_nBufferLen = sizeof(int) * 2;
	}
	else 
	{
		g_StrCpyLen(UiInfo.m_pContent, Lua_ValueToString(L,1), 64);
		UiInfo.m_nBufferLen = strlen(((char *)UiInfo.m_pContent));
		*(int *)((char *)UiInfo.m_pContent + UiInfo.m_nBufferLen) = nTime;
		UiInfo.m_nBufferLen += sizeof(int);
		UiInfo.m_bParam1 = 0;
	}
	
#ifndef _SERVER
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) return 0;

	UiInfo.m_bParam2 = 0;
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
#else
	UiInfo.m_bParam2 = 1;
	UiInfo.ProtocolType = (BYTE)s2c_scriptaction;
	UiInfo.m_wProtocolLong = sizeof(PLAYER_SCRIPTACTION_SYNC) - MAX_SCIRPTACTION_BUFFERNUM + UiInfo.m_nBufferLen - 1;
	g_NewProtocolProcess.BroadcastLocalServer(&UiInfo, UiInfo.m_wProtocolLong + 1);
#endif
	return 0;
}

//AddGlobalTimeNews(strNew/newid, year,month,day,hour,mins)
int LuaAddGlobalTimeNews(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 6) 
		return 0;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_NEWSINFO;
	UiInfo.m_bOptionNum = NEWSMESSAGE_TIMEEND;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	int nMsgId = 0;
	
	if (Lua_IsNumber(L,1))
	{
		nMsgId = (int)Lua_ValueToNumber(L,1);
		*((int *)(UiInfo.m_pContent)) = nMsgId;
		UiInfo.m_bParam1 = 1;
		UiInfo.m_nBufferLen = sizeof(int) + sizeof(SYSTEMTIME);
	}
	else 
	{
		g_StrCpyLen(UiInfo.m_pContent, Lua_ValueToString(L,1), 64);
		UiInfo.m_nBufferLen = strlen(((char *)UiInfo.m_pContent)) + sizeof(SYSTEMTIME);
		UiInfo.m_bParam1 = 0;
	}
	
	SYSTEMTIME *pSystemTime = 	(SYSTEMTIME *)((char *)UiInfo.m_pContent + UiInfo.m_nBufferLen - sizeof(SYSTEMTIME));
	memset(pSystemTime, 0, sizeof(SYSTEMTIME));
	
	SYSTEMTIME LocalTime ;
	memset(&LocalTime, 0, sizeof(SYSTEMTIME));
	
	LocalTime.wYear = (WORD)Lua_ValueToNumber(L,2);
	LocalTime.wMonth =(WORD)Lua_ValueToNumber(L,3);
	LocalTime.wDay = (WORD)Lua_ValueToNumber(L, 4);
	LocalTime.wHour = (WORD)Lua_ValueToNumber(L,5);
	LocalTime.wMinute = (WORD)Lua_ValueToNumber(L,6);
	FILETIME ft;
	FILETIME sysft;
#ifdef WIN32
	SystemTimeToFileTime(&LocalTime, &ft);
	LocalFileTimeToFileTime(&ft, &sysft);
	FileTimeToSystemTime(&sysft, pSystemTime);
#else
	memcpy(pSystemTime, &LocalTime, sizeof(LocalTime));
#endif
	
#ifndef _SERVER
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) return 0;

	UiInfo.m_bParam2 = 0;
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
#else
	UiInfo.m_bParam2 = 1;
	UiInfo.ProtocolType = (BYTE)s2c_scriptaction;
	UiInfo.m_wProtocolLong = sizeof(PLAYER_SCRIPTACTION_SYNC) - MAX_SCIRPTACTION_BUFFERNUM + UiInfo.m_nBufferLen - 1;
	g_NewProtocolProcess.BroadcastGlobal(&UiInfo, UiInfo.m_wProtocolLong + 1);
#endif
	return 0;
}

//AddLocalTimeNews(strNew/newid, year,month,day,hour,mins)
int LuaAddLocalTimeNews(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 6) 
		return 0;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_NEWSINFO;
	UiInfo.m_bOptionNum = NEWSMESSAGE_TIMEEND;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	int nMsgId = 0;
	
	if (Lua_IsNumber(L,1))
	{
		nMsgId = (int)Lua_ValueToNumber(L,1);
		*((int *)(UiInfo.m_pContent)) = nMsgId;
		UiInfo.m_bParam1 = 1;
		UiInfo.m_nBufferLen = sizeof(int) + sizeof(SYSTEMTIME);
	}
	else 
	{
		g_StrCpyLen(UiInfo.m_pContent, Lua_ValueToString(L,1), 64);
		UiInfo.m_nBufferLen = strlen(((char *)UiInfo.m_pContent)) + sizeof(SYSTEMTIME);
		UiInfo.m_bParam1 = 0;
	}
	
	SYSTEMTIME *pSystemTime = 	(SYSTEMTIME *)((char *)UiInfo.m_pContent + UiInfo.m_nBufferLen - sizeof(SYSTEMTIME));
	memset(pSystemTime, 0, sizeof(SYSTEMTIME));
	
	SYSTEMTIME LocalTime ;
	memset(&LocalTime, 0, sizeof(SYSTEMTIME));
	
	LocalTime.wYear = (WORD)Lua_ValueToNumber(L,2);
	LocalTime.wMonth =(WORD)Lua_ValueToNumber(L,3);
	LocalTime.wDay = (WORD)Lua_ValueToNumber(L, 4);
	LocalTime.wHour = (WORD)Lua_ValueToNumber(L,5);
	LocalTime.wMinute = (WORD)Lua_ValueToNumber(L,6);
	FILETIME ft;
	FILETIME sysft;
#ifdef WIN32
	SystemTimeToFileTime(&LocalTime, &ft);
	LocalFileTimeToFileTime(&ft, &sysft);
	FileTimeToSystemTime(&sysft, pSystemTime);
#else
	memcpy(pSystemTime, &LocalTime, sizeof(LocalTime));
#endif
	
#ifndef _SERVER
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) return 0;

	UiInfo.m_bParam2 = 0;
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
#else
	UiInfo.m_bParam2 = 1;
	UiInfo.ProtocolType = (BYTE)s2c_scriptaction;
	UiInfo.m_wProtocolLong = sizeof(PLAYER_SCRIPTACTION_SYNC) - MAX_SCIRPTACTION_BUFFERNUM + UiInfo.m_nBufferLen - 1;
	g_NewProtocolProcess.BroadcastLocalServer(&UiInfo, UiInfo.m_wProtocolLong + 1);
#endif
	return 0;
}

//AddNote(str/strid)
int LuaAddNote(Lua_State * L)
{
	char * strMain  = NULL;
	int nMainInfo = 0;
	int nDataType = 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0)
		return 0;
	
	int nParamNum = Lua_GetTopIndex(L);
	if (nParamNum < 1) 
		return 0;
	
	int nParam2 = 0;
	
	
	if  (Lua_IsNumber(L,1))
	{
		nMainInfo = (int)Lua_ValueToNumber(L,1);
		nDataType = 1 ;
	}
	else if (Lua_IsString(L, 1)) 	//检查主信息是字符串还是字符串标识号
	{
		strMain = (char *)Lua_ValueToString(L, 1);
		nDataType = 0 ;
	}
	else
		return 0;
	
	if (nParamNum > 1)
	{
		nParam2 = (int)Lua_ValueToNumber(L, 2);
	}
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_NOTEINFO;
	UiInfo.m_bParam1 = nDataType;//主信息的类型，字符串(0)或数字(1)
#ifndef _SERVER
	UiInfo.m_bParam2 = 0;
#else
	UiInfo.m_bParam2 = 1;
#endif
	
	UiInfo.m_bOptionNum = 0;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	//主信息为字符串
	if (nDataType == 0)
	{
		if (strMain)
			sprintf(UiInfo.m_pContent, "%s", strMain);
		int nLen = strlen(strMain);
		*(int*)(UiInfo.m_pContent + nLen) = nParam2;
		UiInfo.m_nBufferLen = nLen + sizeof(int);
	}
	else if (nDataType == 1) //主信息为数字标识
	{
		*(int *)UiInfo.m_pContent = nMainInfo;
		*(int *)(UiInfo.m_pContent + sizeof(int)) = nParam2;
		UiInfo.m_nBufferLen = sizeof(int) + sizeof(int);
	}
	
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
	return 0;
}

/*
**
**格式1:Talk(SentenceNum, CallBack-Fun(结束后的回调函数), sTalk1, sTalk2, sTalk3, sTalk4,...sTalkN);  
Talk(SentenceNum, CallBack-Fun(结束后的回调函数), nTalk1, nTalk2,nTalk3,nTalk4,...nTalkN);  
**格式2:Talk(SentenceNum, CallBack-Fun, SentenceTab);
**例子:Talk(3,"EndTalk", "玩家：请问现在几点钟了？", "雇员：现在5点钟了","太谢谢你了！");
**
*/

int LuaTalkUI(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) 
		return 0;
	Player[nPlayerIndex].m_bWaitingPlayerFeedBack = false;
	int nMainInfo = 0;
	int nDataType = 0;
	int nOptionNum = 0;
	char * pContent = NULL;
	
	int nParamNum = Lua_GetTopIndex(L);
	if (nParamNum < 3) 
		return 0;
	
	if (Lua_IsNumber(L,1))
	{
		double nRequested = Lua_ValueToNumber(L, 1);
		if (!(nRequested >= 1 && nRequested <= 255) || nRequested != (int)nRequested)
			return 0;
		nOptionNum = (int)nRequested;
	}
	else 
	{
		_ASSERT(0);
		return 0;
	}
	
	const char * pCallBackFun = Lua_ValueToString(L,2);
	
	//检查主信息是字符串还是字符串标识号
	
	if  (Lua_IsNumber(L,3))
	{
		nDataType = 1 ;
	}
	else if (Lua_IsString(L, 3)) 
	{
		nDataType = 0 ;
	}
	else
		return 0;
	
	
	//获得实际传入的选项的个数
	if (nOptionNum > nParamNum - 2) 
		nOptionNum = nParamNum - 2;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_TALKDIALOG;
	UiInfo.m_bParam1 = nDataType;//主信息的类型，字符串(0)或数字(1)
	UiInfo.m_bOptionNum = nOptionNum;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	pContent = UiInfo.m_pContent;
	pContent[0] = 0;
	size_t nContentLen = 0;
	for (int i = 0; i < nOptionNum; i++)
	{
		char szNumber[32];
		const char* pString = NULL;
		if (!nDataType)
			pString = Lua_ValueToString(L, i + 3);
		else
		{
			double nValue = Lua_ValueToNumber(L, i + 3);
			if (!Lua_IsNumber(L, i + 3) || !(nValue >= 0 && nValue <= 0x7fffffff) ||
				nValue != (int)nValue)
				return 0;
			sprintf(szNumber, "%d", (int)nValue);
			pString = szNumber;
		}
		if (!pString)
			return 0;
		size_t nLength = strlen(pString);
		// 每段的分隔符和末尾NUL也占空间，禁止重叠sprintf及越界写入。
		if (nLength + 2 > sizeof(UiInfo.m_pContent) - nContentLen)
		{
			g_DebugLog("[ScriptUI] Talk content exceeds %u bytes; player=%d sentences=%d kept=%d",
				(unsigned int)sizeof(UiInfo.m_pContent), nPlayerIndex, nOptionNum, i);
			nOptionNum = i;
			break;
		}
		memcpy(pContent + nContentLen, pString, nLength);
		nContentLen += nLength;
		pContent[nContentLen++] = '|';
		pContent[nContentLen] = 0;
	}
	if (nOptionNum <= 0)
		return 0;
	UiInfo.m_bOptionNum = (BYTE)nOptionNum;
	UiInfo.m_nBufferLen = (int)nContentLen;
	
	if (!pCallBackFun || strlen(pCallBackFun) <= 0)
	{
		UiInfo.m_nParam = 0;
		Player[nPlayerIndex].m_nAvailableAnswerNum = 0;
		Player[nPlayerIndex].m_bWaitingPlayerFeedBack = false;
	}
	else
	{
		UiInfo.m_nParam = 1;
		Player[nPlayerIndex].m_nAvailableAnswerNum = 1;
		g_StrCpyLen(Player[nPlayerIndex].m_szTaskAnswerFun[0], pCallBackFun, sizeof(Player[nPlayerIndex].m_szTaskAnswerFun[0]));
		Player[nPlayerIndex].m_bWaitingPlayerFeedBack = true;
	}
	
#ifndef _SERVER
	UiInfo.m_bParam2 = 0;
#else
	UiInfo.m_bParam2 = 1;
#endif
	
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
	return 0;
	
}

// 当前客户端使用 UI action 15 关闭脚本对话框。
int LuaCloseDialog(Lua_State *L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)
		return 0;
	Player[nPlayerIndex].m_bWaitingPlayerFeedBack = false;
	Player[nPlayerIndex].m_nAvailableAnswerNum = 0;
	Player[nPlayerIndex].m_dwScriptDialogScriptID = 0;
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_CLOSEDIALOG;
	UiInfo.m_bParam2 = 1;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
	return 0;
}

int LuaIncludeFile(Lua_State * L)
{
	if (Lua_GetTopIndex(L) <= 0) return 0;
	
	if (Lua_IsString(L,1))
	{
		const char * pFileName = lua_tostring(L,1);	
		char lszCurrentDirectory[MAX_PATH];
		int nLen = 0;
		if (pFileName[0] != '\\' && pFileName[0] != '/')
		{
			getcwd(lszCurrentDirectory, MAX_PATH);
			nLen = strlen(lszCurrentDirectory);
			if (lszCurrentDirectory[nLen - 1] == '\\' || lszCurrentDirectory[nLen - 1] == '/')
				lszCurrentDirectory[nLen - 1] = 0;
#ifdef WIN32
			g_StrCat(lszCurrentDirectory, "\\");
			g_StrCat(lszCurrentDirectory, (char*)pFileName);
#else
			g_StrCat(lszCurrentDirectory, "/");
			g_StrCat(lszCurrentDirectory, (char*)pFileName);
			for (int i = 0; lszCurrentDirectory[i]; i++)
			{
				if (lszCurrentDirectory[i] == '\\')
					lszCurrentDirectory[i] = '/';
			}
#endif
		}
		else
		{
			g_GetRootPath(lszCurrentDirectory);
			nLen = strlen(lszCurrentDirectory);
			if (lszCurrentDirectory[nLen - 1] == '\\' || lszCurrentDirectory[nLen - 1] == '/')
				lszCurrentDirectory[nLen - 1] = 0;
#ifdef WIN32
			g_StrCat(lszCurrentDirectory, "\\");
			g_StrCat(lszCurrentDirectory, (char*)pFileName + 1);
#else
			g_StrCat(lszCurrentDirectory, "/");
			g_StrCat(lszCurrentDirectory, (char*)pFileName + 1);
			for (int i = 0; lszCurrentDirectory[i]; i++)
			{
				if (lszCurrentDirectory[i] == '\\')
					lszCurrentDirectory[i] = '/';
			}
#endif
		}
		strlwr(lszCurrentDirectory + nLen);
		lua_dofile(L, lszCurrentDirectory);
		return 0;
	}
	else
		return 0;
}

//**************************************************************************************************************************************************************
//												任务脚本
//**************************************************************************************************************************************************************

int LuaGetTaskValue(Lua_State * L)
{
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0) 
	{
		int nValue = (int)Player[nPlayerIndex].m_cTask.GetSaveVal((int)Lua_ValueToNumber(L,1));		
		Lua_PushNumber(L, nValue);
	}
	else
		Lua_PushNil(L);
	
	return 1;
}

int LuaSetTaskValue(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	int nValueIndex = (int)Lua_ValueToNumber(L, 1);
	int nValue = (int)Lua_ValueToNumber(L, 2);
	
	if (nPlayerIndex <= 0) return 0;
	Player[nPlayerIndex].m_cTask.SetSaveVal(nValueIndex, nValue);
	return 0;
}


#ifndef _SERVER
#define MAX_TEMPVALUENUM_INCLIENT 500
int g_TempValue[MAX_TEMPVALUENUM_INCLIENT];
#endif

int LuaGetTempTaskValue(Lua_State * L)
{
	int nTempIndex = (int)Lua_ValueToNumber(L, Lua_GetTopIndex(L));
	
#ifdef _SERVER
	if(nTempIndex >= MAX_TEMP_TASK)
	{
		Lua_PushNil(L) ;
		return 1;
	}
	int nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex <= 0) 
	{
		Lua_PushNil(L);
		return 1;
	}
	
	int nValue = Player[nPlayerIndex].m_cTask.GetClearVal(nTempIndex);
	Lua_PushNumber(L, nValue);
#else
	
	if (nTempIndex >= 0 && nTempIndex < MAX_TEMPVALUENUM_INCLIENT)
		Lua_PushNumber(L, g_TempValue[nTempIndex]);
	else 
		Lua_PushNil(L);
#endif
	return 1;
}

int LuaSetTempTaskValue(Lua_State * L)
{
	int nTempIndex = (int)Lua_ValueToNumber(L, Lua_GetTopIndex(L) - 1);
	int nValue = (int)Lua_ValueToNumber(L, Lua_GetTopIndex(L));
#ifdef _SERVER	
	Lua_GetGlobal(L, SCRIPT_PLAYERINDEX);
	int nPlayerIndex = (int)Lua_ValueToNumber(L, Lua_GetTopIndex(L));
	if (nPlayerIndex <= 0) return 0;
	Player[nPlayerIndex].m_cTask.SetClearVal(nTempIndex, nValue);
#else
	g_TempValue[nTempIndex] = nValue;
#endif
	return 0;
}

#ifdef _SERVER
//---------------------------------交易、买卖、打开储藏箱-----------------------
//Sale(id)
//------------------------------------------------------------------------------
int LuaSale(Lua_State * L)
{
	if (Lua_GetTopIndex(L) <= 0) return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		int nShopId = (int)Lua_ValueToNumber(L,1);
		//----在以下加入买卖的实际代码!
		BuySell.OpenSale(nPlayerIndex, nShopId - 1);
	}
	return 0;
}

static BOOL ReadScriptInteger(Lua_State* L, int nArgument, int nMinimum, int nMaximum, int* pnValue)
{
	double nValue = Lua_ValueToNumber(L, nArgument);
	if (!Lua_IsNumber(L, nArgument) || !(nValue >= nMinimum && nValue <= nMaximum) ||
		nValue != (int)nValue)
		return FALSE;
	*pnValue = (int)nValue;
	return TRUE;
}

// 自动回调保存并恢复Lua上下文，不改变NPC对话的ActionScriptID或等待状态。
BOOL g_CallPlayerScript(int nPlayer, DWORD dwScript, const char* pszFunction,
	DWORD dwParameter, int* pnResult, BOOL bItemAction)
{
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER || !pnResult || !pszFunction ||
		g_PlayerScriptAction[nPlayer] || (bItemAction && g_NpcScriptExchange[nPlayer]))
		return FALSE;
	KLuaScript* pScript = (KLuaScript*)g_GetScript(dwScript);
	if (!pScript || !pScript->m_LuaState)
		return FALSE;
	char szMissing[128];
	if (!g_CheckNpcScriptFunction(dwScript, pszFunction, szMissing, sizeof(szMissing), bItemAction ? 3 : 2))
	{
		static DWORD dwLastError[MAX_PLAYER] = {0};
		DWORD dwNow = GetTickCount();
		if (!dwLastError[nPlayer] || dwNow - dwLastError[nPlayer] >= 5000)
		{
			dwLastError[nPlayer] = dwNow;
			g_DebugLog("[PlayerScript] Rejected callback player=%d script=%lu function=%s missing=%s",
				nPlayer, dwScript, pszFunction, szMissing);
		}
		return FALSE;
	}
	KPlayerScriptAction Action;
	Action.ItemID = dwParameter;
	Action.RepairID = 0;
	Action.AllowRepair = bItemAction;
	g_PlayerScriptAction[nPlayer] = &Action;
	Lua_State* L = pScript->m_LuaState;
	int nTop = Lua_GetTopIndex(L);
	const char* Names[] = {SCRIPT_PLAYERINDEX, SCRIPT_PLAYERID, SCRIPT_SUBWORLDINDEX};
	for (int i = 0; i < 3; i++)
		Lua_GetGlobal(L, Names[i]);
	KPlayer& Role = Player[nPlayer];
	Lua_PushNumber(L, nPlayer);
	Lua_SetGlobal(L, SCRIPT_PLAYERINDEX);
	Lua_PushNumber(L, Role.m_dwID);
	Lua_SetGlobal(L, SCRIPT_PLAYERID);
	Lua_PushNumber(L, Role.m_nIndex > 0 && Role.m_nIndex < MAX_NPC ? Npc[Role.m_nIndex].m_SubWorldIndex : -1);
	Lua_SetGlobal(L, SCRIPT_SUBWORLDINDEX);
	BOOL bSuccess = pScript->CallFunction((char*)pszFunction, 1, "n", (double)dwParameter);
	if (bSuccess)
	{
		double nValue = Lua_ValueToNumber(L, -1);
		bSuccess = Lua_IsNumber(L, -1) && nValue >= 0 && nValue <= 0x7FFFFFFF && nValue == (int)nValue;
		if (bSuccess)
			*pnResult = (int)nValue;
	}
	for (int i = 0; i < 3; i++)
	{
		Lua_PushValue(L, nTop + i + 1);
		Lua_SetGlobal(L, Names[i]);
	}
	Lua_SetTopIndex(L, nTop);
	g_PlayerScriptAction[nPlayer] = NULL;
	if (!bSuccess)
	{
		g_DebugLog("[PlayerScript] Aborted callback player=%d script=%lu function=%s; no action committed",
			nPlayer, dwScript, pszFunction);
		return FALSE;
	}
	if (Action.RepairID)
	{
		// 每个回调仅允许一件原物品操作；再次校验价格、归属、现金和角色状态。
		if (*pnResult != 1 || !Role.RepairItem(Action.RepairID))
			return FALSE;
		g_DebugLog("[PlayerScript] Repair committed player=%d item=%lu", nPlayer, Action.RepairID);
		for (size_t i = 0; i < Action.Notices.size(); i++)
		{
			KPlayerChat::SendSystemInfo(1, nPlayer, MESSAGE_SYSTEM_ANNOUCE_HEAD,
				(char*)Action.Notices[i].c_str(), (int)Action.Notices[i].size());
		}
	}
	return TRUE;
}

int LuaSaleEx(Lua_State* L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	int nShop = 0;
	if (nPlayer > 0 && nPlayer < MAX_PLAYER && nTop == 1 &&
		ReadScriptInteger(L, 1, 1, 65535, &nShop))
		BuySell.OpenSale(nPlayer, nShop - 1, 1);
	return 0;
}

static KPlayer* GetScriptPetPlayer(Lua_State* L, BOOL bWrite)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER)
		return NULL;
	if (bWrite && !g_NpcScriptExchange[nPlayer])
	{
		lua_error(L, "Pet changes require an item or NPC transaction");
		return NULL;
	}
	return &Player[nPlayer];
}

int LuaGetPetType(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, FALSE);
	Lua_PushNumber(L, pPlayer ? pPlayer->m_Pet.Type : 0);
	return 1;
}

int LuaGetPetHonor(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, FALSE);
	Lua_PushNumber(L, pPlayer ? pPlayer->m_Pet.Honor : 0);
	return 1;
}

int LuaGetPetColor(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, FALSE);
	Lua_PushNumber(L, pPlayer ? pPlayer->m_Pet.Color : 0);
	return 1;
}

int LuaGetPetName(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, FALSE);
	Lua_PushString(L, pPlayer ? pPlayer->m_Pet.Name : "");
	return 1;
}

int LuaIsPetReleased(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, FALSE);
	Lua_PushNumber(L, pPlayer ? pPlayer->m_Pet.Released : 0);
	return 1;
}

int LuaGenPet(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, TRUE);
	int nType = 0, nHonor = 0, nColor = 0;
	BOOL bDone = pPlayer && Lua_GetTopIndex(L) == 4 && lua_type(L, 1) == LUA_TSTRING &&
		ReadScriptInteger(L, 2, 1, 127, &nType) && ReadScriptInteger(L, 3, 1, 100, &nHonor) &&
		ReadScriptInteger(L, 4, 0, 255, &nColor) &&
		strlen(lua_tostring(L, 1)) == lua_strlen(L, 1) &&
		pPlayer->CreatePet(lua_tostring(L, 1), nType, nHonor, nColor);
	if (!bDone)
		RejectNpcScriptItem(L, GetPlayerIndex(L));
	Lua_PushNumber(L, bDone ? 1 : 0);
	return 1;
}

int LuaDelPet(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, TRUE);
	if (!pPlayer || Lua_GetTopIndex(L) != 0 || !pPlayer->m_Pet.Type)
	{
		RejectNpcScriptItem(L, GetPlayerIndex(L));
		Lua_PushNumber(L, 0);
		return 1;
	}
	pPlayer->DeletePet();
	Lua_PushNumber(L, 1);
	return 1;
}

int LuaSetPetType(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, TRUE);
	int nValue = 0;
	BOOL bDone = pPlayer && Lua_GetTopIndex(L) == 1 &&
		ReadScriptInteger(L, 1, 1, 127, &nValue) && pPlayer->SetPetType(nValue);
	if (!bDone)
		RejectNpcScriptItem(L, GetPlayerIndex(L));
	Lua_PushNumber(L, bDone ? 1 : 0);
	return 1;
}

int LuaSetPetHonor(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, TRUE);
	int nValue = 0;
	BOOL bDone = pPlayer && Lua_GetTopIndex(L) == 1 &&
		ReadScriptInteger(L, 1, 0, 100, &nValue) && pPlayer->SetPetHonor(nValue);
	if (!bDone)
		RejectNpcScriptItem(L, GetPlayerIndex(L));
	Lua_PushNumber(L, bDone ? 1 : 0);
	return 1;
}

int LuaReleasePet(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, TRUE);
	int nValue = 0;
	BOOL bDone = pPlayer && Lua_GetTopIndex(L) == 1 &&
		ReadScriptInteger(L, 1, 0, 1, &nValue) && pPlayer->SetPetReleased(nValue);
	if (!bDone)
		RejectNpcScriptItem(L, GetPlayerIndex(L));
	Lua_PushNumber(L, bDone ? 1 : 0);
	return 1;
}

int LuaGetPetFuncAuthority(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, FALSE);
	int nFunction = 0;
	Lua_PushNumber(L, pPlayer && Lua_GetTopIndex(L) == 1 &&
		ReadScriptInteger(L, 1, 0, 4, &nFunction) &&
		(pPlayer->m_Pet.Authority & (1 << nFunction)) ? 1 : 0);
	return 1;
}

int LuaSetPetFuncAuthority(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, TRUE);
	int nFunction = 0, nEnabled = 0;
	BOOL bDone = pPlayer && Lua_GetTopIndex(L) == 2 &&
		ReadScriptInteger(L, 1, 0, 4, &nFunction) && ReadScriptInteger(L, 2, 0, 1, &nEnabled) &&
		pPlayer->SetPetAuthority(nFunction, nEnabled);
	if (!bDone)
		RejectNpcScriptItem(L, GetPlayerIndex(L));
	Lua_PushNumber(L, bDone ? 1 : 0);
	return 1;
}

int LuaGetPetFuncActive(Lua_State* L)
{
	int nTop = Lua_GetTopIndex(L);
	KPlayer* pPlayer = GetScriptPetPlayer(L, FALSE);
	int nFunction = 0;
	BOOL bActive = pPlayer && nTop == 1 && ReadScriptInteger(L, 1, 0, 4, &nFunction) &&
		(pPlayer->m_Pet.Active & (1 << nFunction));
	Lua_PushNumber(L, bActive ? 1 : 0);
	return 1;
}

int LuaGetPetSettings(Lua_State* L)
{
	int nTop = Lua_GetTopIndex(L);
	KPlayer* pPlayer = GetScriptPetPlayer(L, FALSE);
	if (!pPlayer || nTop != 0)
	{
		Lua_PushNil(L);
		return 1;
	}
	lua_newtable(L);
	int nTable = Lua_GetTopIndex(L);
	SetScriptItemInteger(L, nTable, "PickMask", pPlayer->m_Pet.PickMask);
	SetScriptItemInteger(L, nTable, "SellMask", pPlayer->m_Pet.SellMask);
	return 1;
}

static BOOL ReadScriptItemID(Lua_State* L, DWORD* pdwItem)
{
	double nValue = Lua_ValueToNumber(L, 1);
	if (Lua_GetTopIndex(L) != 1 || !Lua_IsNumber(L, 1) ||
		!(nValue >= 1 && nValue <= 4294967295.0) || nValue != (DWORD)nValue)
		return FALSE;
	*pdwItem = (DWORD)nValue;
	return TRUE;
}

int LuaGetItemInfo(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, FALSE);
	DWORD dwItem = 0;
	int nItem = 0;
	if (pPlayer && ReadScriptItemID(L, &dwItem))
	{
		KPlayerScriptAction* pAction = g_PlayerScriptAction[pPlayer->GetPlayerIndex()];
		if (pAction && pAction->ItemID == dwItem)
			nItem = ItemSet.SearchID(dwItem);
		else
			nItem = pPlayer->m_ItemList.SearchID(dwItem);
	}
	if (nItem <= 0 || nItem >= MAX_ITEM)
	{
		Lua_PushNil(L);
		return 1;
	}
	KItem& Goods = Item[nItem];
	lua_newtable(L);
	int nTable = Lua_GetTopIndex(L);
	Lua_PushString(L, "Name");
	Lua_PushString(L, Goods.GetName());
	Lua_SetTable(L, nTable);
	SetScriptItemInteger(L, nTable, "Genre", Goods.GetGenre());
	SetScriptItemInteger(L, nTable, "DetailType", Goods.GetDetailType());
	SetScriptItemInteger(L, nTable, "Price", Goods.GetPrice());
	SetScriptItemInteger(L, nTable, "Bound", Goods.IsBound() ? 1 : 0);
	SetScriptItemInteger(L, nTable, "GenerationFlags", Goods.GetGenerationFlags());
	SetScriptItemInteger(L, nTable, "Upgraded", Goods.GetUpgradeData() ? 1 : 0);
	SetScriptItemInteger(L, nTable, "Durability", Item[nItem].GetDurability());
	SetScriptItemInteger(L, nTable, "MaxDurability", Item[nItem].GetMaxDurability());
	SetScriptItemInteger(L, nTable, "RepairPrice", Item[nItem].GetRepairPrice());
	SetScriptItemInteger(L, nTable, "Place", pPlayer->m_ItemList.GetItemPlace(nItem));
	SetScriptItemInteger(L, nTable, "MagicCount", Goods.GetMagicAttribCount());
	SetScriptItemInteger(L, nTable, "RequireLevel", Goods.GetRequireLevel());
	const KBASICPROP_EQUIPMENT* pTemplate = Goods.GetGenre() == item_equip ?
		ItemGen.GetEquipmentTemplate(Goods.GetDetailType(), Goods.GetParticular(),
			Goods.GetLevel(), Goods.m_GeneratorParam.nVersion) : NULL;
	SetScriptItemInteger(L, nTable, "TemplateValid", pTemplate ? 1 : 0);
	SetScriptItemInteger(L, nTable, "TemplateSeries", pTemplate ? pTemplate->m_nSeries : 0);
	return 1;
}

int LuaRepairItem(Lua_State* L)
{
	KPlayer* pPlayer = GetScriptPetPlayer(L, FALSE);
	KPlayerScriptAction* pAction = pPlayer ? g_PlayerScriptAction[pPlayer->GetPlayerIndex()] : NULL;
	DWORD dwItem = 0;
	if (!pAction || !pAction->AllowRepair || !ReadScriptItemID(L, &dwItem) ||
		pAction->ItemID != dwItem || pAction->RepairID)
	{
		lua_error(L, "RepairItem requires one current item action; changes were not committed");
		return 0;
	}
	int nCost = 0;
	if (!pPlayer->CheckRepairItem(dwItem, &nCost))
	{
		Lua_PushNumber(L, 0);
		return 1;
	}
	pAction->RepairID = dwItem;
	Lua_PushNumber(L, 1);
	return 1;
}

int LuaTrade(Lua_State * L)
{
	//Question 未写
	return 0;
}

int LuaOpenBox(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	
	Player[nPlayerIndex].OpenStoreBox((int)Lua_ValueToNumber(L, 1));
	return 0;
}

//---------------------------------时间任务-------------------------------------
//SetTimer(Time, TimerTaskId)
int LuaSetTimer(Lua_State  * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	if (nParamCount < 2 ) return 0;
	int nPlayerIndex  = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	Player[nPlayerIndex].SetTimer((DWORD) (int)Lua_ValueToNumber(L, 1), (int)Lua_ValueToNumber(L,2));
	return 0;
}

int LuaStopTimer(Lua_State * L)
{
	int nPlayerIndex  = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	Player[nPlayerIndex].CloseTimer();
	return 0;
}

int LuaGetCurTimerId(Lua_State * L)
{
	int nPlayerIndex  = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) 
	{
		Lua_PushNumber(L,0);
		return 1;
	}
	int nTimerId = Player[nPlayerIndex].m_TimerTask.GetTaskId();
	Lua_PushNumber(L, nTimerId);
	return 1;
}

int LuaGetRestTime(Lua_State * L)
{
	int nPlayerIndex  = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) 
	{
		Lua_PushNil(L);
		return 1;
	}
	int nRestTime = Player[nPlayerIndex].m_TimerTask.GetRestTime();//m_dwTimeTaskTime - g_SubWorldSet.GetGameTime();
	
	if (nRestTime > 0)
		Lua_PushNumber(L, nRestTime);
	else
		Lua_PushNumber(L, 0);
	
	return 1;
}

int LuaGetMissionRestTime(Lua_State * L)
{
	int RestTime = 0;
	if (Lua_GetTopIndex(L) >= 2)
	{
		int nSubWorldIndex = GetSubWorldIndex(L);
		if (nSubWorldIndex >= 0) 
		{
			int nMissionId = (int)Lua_ValueToNumber(L, 1);
			int nTimerId = (int)Lua_ValueToNumber(L, 2);
			
			if (nMissionId < 0 || nTimerId < 0 )
				goto lab_getmissionresttime;
			
			KMission Mission;
			Mission.SetMissionId(nMissionId);
			KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
			if (pMission)
			{
				RestTime = (int)pMission->GetTimerRestTimer(nTimerId);
			}
		}
	}
	
lab_getmissionresttime:
	Lua_PushNumber(L, RestTime);
	return 1;
}


//**************************************************************************************************************************************************************
//												组队脚本
//**************************************************************************************************************************************************************
int LuaIsLeader(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0 ) 
	{
		if (Player[nPlayerIndex].m_cTeam.m_nFlag && Player[nPlayerIndex].m_cTeam.m_nFigure == TEAM_CAPTAIN)
			Lua_PushNumber(L,1);
		else 
			Lua_PushNumber(L,0);
		
	}
	else 
		Lua_PushNumber(L, 0);
	return 1;
}

int LuaGetTeamId(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		if (Player[nPlayerIndex].m_cTeam.m_nFlag)
			Lua_PushNumber(L, Player[nPlayerIndex].m_cTeam.m_nID);
		else Lua_PushNil(L);
	}
	else 
		Lua_PushNil(L);
	return 1;
}

int LuaGetTeamSize(Lua_State * L)
{
	int nTeamSize = 0;
	int nTeamId = -1;
	if (Lua_GetTopIndex(L) >= 1)
	{
		nTeamId = Lua_ValueToNumber(L, 1);
	}
	else
	{
		int nPlayerIndex = GetPlayerIndex(L);
		if (nPlayerIndex > 0)
		{
			if (Player[nPlayerIndex].m_cTeam.m_nFlag)
				nTeamId = Player[nPlayerIndex].m_cTeam.m_nID;
			else 
				nTeamId = -1;
		}
	}
	
	if (nTeamId < 0)
		nTeamSize = 0;
	else
		nTeamSize = g_Team[nTeamId].m_nMemNum + 1;
	Lua_PushNumber(L, nTeamSize);
	return 1;
}

int LuaLeaveTeam(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		if (Player[nPlayerIndex].m_cTeam.m_nFlag)
		{
			PLAYER_APPLY_LEAVE_TEAM	sLeaveTeam;
			sLeaveTeam.ProtocolType = c2s_teamapplyleave;
			Player[nPlayerIndex].LeaveTeam((BYTE*)&sLeaveTeam);
		}
	}
	return 0;
}

int LuaSetCreateTeamOption(Lua_State * L)
{
	int nState = (int)Lua_ValueToNumber(L, 1);
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		if (nState)
			Player[nPlayerIndex].m_cTeam.SetCanTeamFlag(nPlayerIndex, TRUE);
		else
			Player[nPlayerIndex].m_cTeam.SetCanTeamFlag(nPlayerIndex, FALSE);
	}
	return 0;
}
//**************************************************************************************************************************************************************
//												聊天消息脚本
//**************************************************************************************************************************************************************

int	LuaMsgToPlayer(Lua_State * L)
{
	if (Lua_GetTopIndex(L) <= 0) return 0;
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0) 
	{
		const char *  szMsg = Lua_ValueToString(L,1);
		if (szMsg)
		{
#ifdef _SERVER
			KPlayerScriptAction* pAction = g_PlayerScriptAction[nPlayerIndex];
			if (pAction)
			{
				if (!pAction->AllowRepair || !pAction->RepairID || strlen(szMsg) > 512 ||
					pAction->Notices.size() >= 8)
					lua_error(L, "Player callback message requires a pending item action");
				pAction->Notices.push_back(szMsg);
				return 0;
			}
			KNpcScriptExchange* pState = g_NpcScriptExchange[nPlayerIndex];
			if (pState)
			{
				KNpcScriptNotice Notice;
				Notice.IsChat = TRUE;
				Notice.Chat = szMsg;
				pState->Notices.push_back(Notice);
			}
			else
#endif
				KPlayerChat::SendSystemInfo(1, nPlayerIndex, MESSAGE_SYSTEM_ANNOUCE_HEAD,
					(char*)szMsg, strlen(szMsg));
		}
	}
	
	return 0;
}

int LuaMsgToTeam(Lua_State * L)
{
	if (Lua_GetTopIndex(L) <= 0	) return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		if (Player[nPlayerIndex].m_cTeam.m_nID >= 0)
		{
			const	char * szMsg = Lua_ValueToString(L,1);
			int nTeamLeaderId = g_Team[Player[nPlayerIndex].m_cTeam.m_nID].m_nCaptain;
			if (nTeamLeaderId > 0)
				KPlayerChat::SendSystemInfo(1, nTeamLeaderId, "组队消息",(char *) szMsg, strlen(szMsg) );
			
			for (int i = 0; i < MAX_TEAM_MEMBER; i ++)
			{
				int nMemberId = g_Team[Player[nPlayerIndex].m_cTeam.m_nID].m_nMember[i] ;
				if (nMemberId > 0)
				{
					if (szMsg)
						KPlayerChat::SendSystemInfo(1, nMemberId, "组队消息", (char *)szMsg, strlen(szMsg) );
				}
			}
		}
	}
	return 0;
}

int LuaMsgToSubWorld(Lua_State * L)
{
	if (Lua_GetTopIndex(L) <= 0 ) return 0;
	
	const char * szMsg = Lua_ValueToString(L,1);
	if (szMsg)
		KPlayerChat::SendSystemInfo(0, 0, MESSAGE_SYSTEM_ANNOUCE_HEAD, (char *)szMsg, strlen(szMsg) );
	return 0;
}

int LuaMsgToAroundRegion(Lua_State * L)
{
	
	return 0;
}

//**************************************************************************************************************************************************************
//												主角脚本
//**************************************************************************************************************************************************************

//**************************************************************************************************************************************************************
//												主角脚本
//**************************************************************************************************************************************************************

/*功能：让玩家进入新的一个游戏世界
nPlayerIndex:主角的Index
nSubWorldIndex:游戏世界id
nPosX:
nPosY:
*/
//NewWorld(WorldId, X,Y)
int  LuaEnterNewWorld(Lua_State * L) 
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) 
		return 0;
	
	int nResult = 0;
	if (Lua_GetTopIndex(L) >= 3)
	{
		DWORD dwWorldId = (DWORD)Lua_ValueToNumber(L, 1);
		nResult = Npc[Player[nPlayerIndex].m_nIndex].ChangeWorld(dwWorldId, (int)Lua_ValueToNumber(L,2) * 32, (int)Lua_ValueToNumber(L,3) * 32);
	}
	Lua_PushNumber(L, nResult);
	return 1;
}

//SetPos(X,Y)
int LuaSetPos(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	if (nParamCount != 2) return 0;
	int nPlayerIndex = GetPlayerIndex(L);
	
	int nX = (int) Lua_ValueToNumber(L,1);
	int nY = (int) Lua_ValueToNumber(L,2);
	
	if (nPlayerIndex > 0)
	{
		Npc[Player[nPlayerIndex].m_nIndex].SetPos(nX * 32, nY * 32);
	}
	return 0;
}

int LuaGetPos(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex > 0)
	{
		int nPosX = 0;
		int nPosY = 0;
		Npc[Player[nPlayerIndex].m_nIndex].GetMpsPos(&nPosX, &nPosY);
		Lua_PushNumber(L, nPosX);
		Lua_PushNumber(L, nPosY);
		Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_SubWorldIndex); 
	}
	else
		return 0;
	return 3;
}

//W,X,Y = GetWorldPos()
int LuaGetNewWorldPos(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex > 0)
	{
		int nPosX = 0;
		int nPosY = 0;
		Npc[Player[nPlayerIndex].m_nIndex].GetMpsPos(&nPosX, &nPosY);

		int nSubWorldIndex = Npc[Player[nPlayerIndex].m_nIndex].m_SubWorldIndex;
		int nSubWorldID = 0;
		if (nSubWorldIndex >= 0 && nSubWorldIndex < MAX_SUBWORLD)
		{
			nSubWorldID = SubWorld[nSubWorldIndex].m_SubWorldID;
		}

		Lua_PushNumber(L, nSubWorldID); 
		Lua_PushNumber(L, ((int)(nPosX / 32)));
		Lua_PushNumber(L, ((int)(nPosY / 32)));
	}
	else
	{
		Lua_PushNumber(L, 0);
		Lua_PushNumber(L, 0);
		Lua_PushNumber(L, 0);
		return 3;
	}
	return 3;
}


int LuaDropItem(Lua_State *L)
{
	return 0;
}

/*AddEventItem(id)*/
KTabFile g_EventItemTab;
int LuaAddEventItem(Lua_State *L)
{
	// modify by spe 03/06/13
	if (Lua_GetTopIndex(L) < 1)
		return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex <= 0)
	{
		Lua_PushNumber(L,0);
		return 1;
	}

	int nEventId = 0;
	if (Lua_IsNumber(L, 1))
	{
		nEventId = (int)Lua_ValueToNumber(L,1);
	}
	else
	{
		char * szEventItm = (char *)Lua_ValueToString(L,1);
		
		if (!g_EventItemTab.GetInteger(szEventItm, "具体类别", 0, &nEventId))
		{
			Lua_PushNumber(L,0);
			return 1;
		}
	}
	
#ifdef _SERVER
	if (g_NpcScriptExchange[nPlayerIndex])
	{
		int nItem = ItemSet.Add(item_task, 0, 1, 0, nEventId, 1, NULL,
			g_SubWorldSet.GetGameVersion());
		BOOL bAdded = nItem > 0 && Player[nPlayerIndex].m_ItemList.AddPickupItem(nItem, -1, -1);
		if (!bAdded)
		{
			if (nItem > 0)
				ItemSet.Remove(nItem);
			RejectNpcScriptItem(L, nPlayerIndex);
		}
		Lua_PushNumber(L, bAdded ? 1 : 0);
		return 1;
	}
#endif
	int nIndex = ItemSet.Add(item_task, 0, 0, 0, nEventId, 0, 0, 1, 0);
	
	if (nIndex <= 0) 
	{
		Lua_PushNumber(L,0);
		return 1;
	}
	
	int		x, y;
	if (Player[nPlayerIndex].m_ItemList.CheckCanPlaceInEquipment(Item[nIndex].GetWidth(), Item[nIndex].GetHeight(), &x, &y))
	{
		Player[nPlayerIndex].m_ItemList.Add(nIndex, pos_equiproom, x, y);
	}
	else
	{
		int		nIdx = Player[nPlayerIndex].m_ItemList.Hand();
		if (nIdx)
		{
			Player[nPlayerIndex].m_ItemList.Remove(nIdx);
			
			KMapPos			sMapPos;
			KObjItemInfo	sInfo;
			
			Player[nPlayerIndex].GetAboutPos(&sMapPos);
			
			sInfo.m_nItemID = nIdx;
			sInfo.m_nItemWidth = Item[nIdx].GetWidth();
			sInfo.m_nItemHeight = Item[nIdx].GetHeight();
			sInfo.m_nMoneyNum = 0;
			strcpy(sInfo.m_szName, Item[nIdx].GetName());
			sInfo.m_nColorID = 0;
			sInfo.m_nMovieFlag = 1;
			sInfo.m_nSoundFlag = 1;
			
			int nObj = ObjSet.Add(Item[nIdx].GetObjIdx(), sMapPos, sInfo);
			if (nObj >= 0)
			{
				if (Item[nIdx].GetGenre() == item_task)
				{
					Object[nObj].SetEntireBelong(nPlayerIndex);
				}
				else
				{
					Object[nObj].SetItemBelong(nPlayerIndex);
				}
			}
		}
		Player[nPlayerIndex].m_ItemList.Add(nIndex, pos_hand, 0 ,0);
	}		
	Lua_PushNumber(L, 1);
	return 1;
}

/*AddItem(nItemClass, nDetailType, nParticualrType, nLevel, nSeries, nLuck, nItemLevel..6)*/
int LuaAddItem(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex <= 0)
	{
		Lua_PushNumber(L,0);
		return 1;
	}
	
	int nParamNum = Lua_GetTopIndex(L);
	if (nParamNum < 7)
	{
		g_DebugLog("[Script]使用AddItem参数数量不符!");
		Lua_PushNumber(L,0);
		return 1;
	}
	
	int nItemClass		= (int)Lua_ValueToNumber(L, 1);
	int nDetailType		= (int)Lua_ValueToNumber(L, 2);
	int nParticularType	= (int)Lua_ValueToNumber(L, 3);
	int nLevel			= (int)Lua_ValueToNumber(L, 4);
	int nSeries			= (int)Lua_ValueToNumber(L, 5);
	int nLuck			= (int)Lua_ValueToNumber(L, 6);
	int nItemLevel[6];
	
	ZeroMemory(nItemLevel, sizeof(nItemLevel));
	nItemLevel[0] = (int)Lua_ValueToNumber(L, 7);
	
	if (nParamNum >= 12)
	{
		nItemLevel[1] = (int)Lua_ValueToNumber(L, 8);
		nItemLevel[2] = (int)Lua_ValueToNumber(L, 9);
		nItemLevel[3] = (int)Lua_ValueToNumber(L, 10);
		nItemLevel[4] = (int)Lua_ValueToNumber(L, 11);
		nItemLevel[5] = (int)Lua_ValueToNumber(L, 12);
	}
	else
	{
		for (int i = 0; i < 5; i ++)
			nItemLevel[i + 1] = nItemLevel[0];
	}
	
	int nIndex = ItemSet.Add(nItemClass, nSeries, nLevel, nLuck, nDetailType, nParticularType,  nItemLevel, g_SubWorldSet.GetGameVersion());
	if (nIndex <= 0)
	{
		Lua_PushNumber(L, 0);
		return 1;
	}
	
	int		x, y;
	if (Player[nPlayerIndex].m_ItemList.CheckCanPlaceInEquipment(Item[nIndex].GetWidth(), Item[nIndex].GetHeight(), &x, &y))
	{
		Player[nPlayerIndex].m_ItemList.Add(nIndex, pos_equiproom, x, y);
	}
	else
	{
		int		nIdx = Player[nPlayerIndex].m_ItemList.Hand();
		if (nIdx)
		{
			Player[nPlayerIndex].m_ItemList.Remove(nIdx);
			
			KMapPos sMapPos;
			KObjItemInfo	sInfo;
			
			Player[nPlayerIndex].GetAboutPos(&sMapPos);
			
			sInfo.m_nItemID = nIdx;
			sInfo.m_nItemWidth = Item[nIdx].GetWidth();
			sInfo.m_nItemHeight = Item[nIdx].GetHeight();
			sInfo.m_nMoneyNum = 0;
			strcpy(sInfo.m_szName, Item[nIdx].GetName());
			sInfo.m_nColorID = 0;
			sInfo.m_nMovieFlag = 1;
			sInfo.m_nSoundFlag = 1;
			
			int nObj = ObjSet.Add(Item[nIdx].GetObjIdx(), sMapPos, sInfo);
			if (nObj >= 0)
			{
				if (Item[nIdx].GetGenre() == item_task)
				{
					Object[nObj].SetEntireBelong(nPlayerIndex);
				}
				else
				{
					Object[nObj].SetItemBelong(nPlayerIndex);
				}
			}
		}
		Player[nPlayerIndex].m_ItemList.Add(nIndex, pos_hand, 0 ,0);
	}
	
	Lua_PushNumber(L,1);
	
	return 1;
	
}

int LuaGetTaskItemCount(Lua_State * L)
{
	
	if (Lua_GetTopIndex(L) > 0)
	{
		int nPlayerIndex = GetPlayerIndex(L);
		if (nPlayerIndex > 0)
		{
			int nEventId = 0;
			if (Lua_IsNumber(L, 1))
			{
				nEventId = (int)Lua_ValueToNumber(L,1);
			}
			else
			{
				char * szEventItm = (char *)Lua_ValueToString(L,1);
				
				if (!g_EventItemTab.GetInteger(szEventItm, "具体类别", 0, &nEventId))
				{
					Lua_PushNumber(L,0);
					return 1;
				}
			}
			
			int nCount = Player[nPlayerIndex].m_ItemList.GetTaskItemNum(nEventId);
			Lua_PushNumber(L, nCount);
		}
		else 
		{
			g_DebugLog("GetItemCount nPlayerIndex <= 0");
			Lua_PushNumber(L,0);
		}
	}
	else
	{
		g_DebugLog("GetItem 参数不够!");
		Lua_PushNumber(L , 0);
	}
	
	return 1;
}


/*nPlayerIndex, nItemTemplateId
功能：给玩家某个物品
*/
/*
DelItem (nPlayerIndex, nItemTemplateId)
功能：删除玩家身上某个物品
*/
int LuaDelItem(Lua_State * L)
{
	if (Lua_GetTopIndex(L) > 0)
	{
		int nPlayerIndex = GetPlayerIndex(L);
		if (nPlayerIndex > 0)
		{
			int nEventId = 0;
			if (Lua_IsNumber(L, 1))
			{
				nEventId = (int)Lua_ValueToNumber(L,1);
			}
			else
			{
				char * szEventItm = (char *)Lua_ValueToString(L,1);
				if (!g_EventItemTab.GetInteger(szEventItm, "具体类别", 0, &nEventId))
				{
					return 0;
				}
			}
			
			int nResult = Player[nPlayerIndex].m_ItemList.RemoveTaskItem(nEventId);
			
		}
		else 
		{
			g_DebugLog("[TASK] Can Not Del Item ");
		}
	}
	else
	{
		g_DebugLog("HaveItem 参数不够!");
	}
	
	return 0;
}
/*HaveItem(nItemTemplateId);
功能:检查玩家是否有某个物品
*/
int LuaHaveItem(Lua_State *L)
{
	if (Lua_GetTopIndex(L) > 0)
	{
		int nPlayerIndex = GetPlayerIndex(L);
		if (nPlayerIndex > 0)
		{
			int nEventId = 0;
			if (Lua_IsNumber(L, 1))
			{
				nEventId = (int)Lua_ValueToNumber(L,1);
			}
			else
			{
				char * szEventItm = (char *)Lua_ValueToString(L,1);
				
				if (!g_EventItemTab.GetInteger(szEventItm, "具体类别", 0, &nEventId))
				{
					Lua_PushNumber(L,0);
					return 1;
				}
			}
			
			int nResult = Player[nPlayerIndex].m_ItemList.IsTaskItemExist(nEventId);
			Lua_PushNumber(L, (nResult != 0));
		}
		else 
		{
			g_DebugLog("HaveItem nPlayerIndex <= 0");
			Lua_PushNumber(L,0);
		}
	}
	else
	{
		g_DebugLog("HaveItem 参数不够!");
		Lua_PushNumber(L , 0);
	}
	
	return 1;
}

/*
AddMagic(nPlayerIndex, nMagicID, nLevel)
DelMagic(nPlayerIndex, nMagicId)
HaveMagic(nPlayerIndex, nMagicId)
GetMagicLevel(nPlayerIndex, nMagicId)
SetMagicLevel(nPlayerIndex, nMagicId, nLevel)
ModifyMagicLevel(nPlayerIndex ,nMagicId, nDLevel)
*/
int LuaAddMagic(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	int nPlayerIndex = 0;
	if (nParamCount < 1) return 0;
	nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	int nSkillId = 0;
	if (Lua_IsNumber(L, 1))
	{
		nSkillId = (int)Lua_ValueToNumber (L, 1);
	}
	else 
	{
		const char * sSkillName = Lua_ValueToString(L, 1);
		g_OrdinSkillsSetting.GetInteger((char*)sSkillName, "SkillId", 0, &nSkillId);
		if (nSkillId <= 0 ) return 0;
	}
    Player[nPlayerIndex].m_nIndex;
	int nLevel = 0;
	if (nParamCount >=2)
		nLevel = (int)Lua_ValueToNumber(L,2);
	else
		nLevel = 0;
	
    int nRet = Npc[Player[nPlayerIndex].m_nIndex].m_SkillList.Add(nSkillId, nLevel);
	if (nRet)
	{
		PLAYER_SKILL_LEVEL_SYNC NewSkill;
		NewSkill.ProtocolType = s2c_playerskilllevel;
		NewSkill.m_nSkillID = nSkillId;
		NewSkill.m_nSkillLevel = Npc[Player[nPlayerIndex].m_nIndex].m_SkillList.GetCurrentLevel(nSkillId);
		NewSkill.m_nLeavePoint = Player[nPlayerIndex].m_nSkillPoint;
		g_pServer->PackDataToClient(Player[nPlayerIndex].m_nNetConnectIdx, (BYTE*)&NewSkill, sizeof(PLAYER_SKILL_LEVEL_SYNC));
	}
	
	return 0;
}

int LuaDelMagic(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	int nPlayerIndex = 0;
	if (nParamCount < 1) return 0;
	
    nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex <= 0) return 0;
	
	int nSkillId = 0;
	if (Lua_IsNumber(L, 1))
	{
		nSkillId = (int)Lua_ValueToNumber (L, 1);
	}
	else 
	{
		const char * sSkillName = Lua_ValueToString(L, 1);
		g_OrdinSkillsSetting.GetInteger((char*)sSkillName, "SkillId", 0, &nSkillId);
		if (nSkillId <= 0 ) return 0;
	}
	Npc[Player[nPlayerIndex].m_nIndex].m_SkillList.Remove(nSkillId);
	return 0;
	
}



int LuaHaveMagic(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	int nPlayerIndex = 0;
	if (nParamCount < 1) return 0;
	
	nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex <= 0) return 0;
	
	int nSkillId = 0;
	if (Lua_IsNumber(L, 1))
	{
		nSkillId = (int)Lua_ValueToNumber (L, 1);
	}
	else 
	{
		const char * sSkillName = Lua_ValueToString(L, 1);
		g_OrdinSkillsSetting.GetInteger((char *)sSkillName, "SkillId", 0, &nSkillId);
		if (nSkillId <= 0 ) 
		{
			Lua_PushNumber(L, -1);
		}
		return 1;
	}
	
	if (Npc[Player[nPlayerIndex].m_nIndex].m_SkillList.FindSame(nSkillId))
	{
		Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_SkillList.GetLevel(nSkillId));
	}
	else
	{
		Lua_PushNumber(L, -1);
	}
	
	return 1;
}

int LuaGetSkillIdInSkillList(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) 
		return 0;
	int nSkillIndex = (int)Lua_ValueToNumber (L, 1);
	int nSkillId = 0;
	if (nSkillIndex > 0) 
	{
		nSkillId = Npc[Player[nPlayerIndex].m_nIndex].m_SkillList.GetSkillId(nSkillIndex);
	}
	Lua_PushNumber(L, nSkillId);
	return 1;
}

int LuaSetSkillLevel(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 2) 
		return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) 
		return 0;
	int nSkillId = 0;
	
	if (Lua_IsNumber(L, 1))
	{
		nSkillId = (int)Lua_ValueToNumber (L, 1);
	}
	else 
	{
		const char * sSkillName = Lua_ValueToString(L, 1);
		g_OrdinSkillsSetting.GetInteger((char *)sSkillName, "SkillId", 0, &nSkillId);
		if (nSkillId <= 0 ) return 0;
	}
	int nLevel = (int)Lua_ValueToNumber(L, 2);
	if (nLevel >= 0)
		Npc[Player[nPlayerIndex].m_nIndex].m_SkillList.SetSkillLevelDirectlyUsingId(nSkillId, nLevel);
	return 0;
}

int LuaRollBackSkills(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) 
		return 0;
	int nSkillId = 0;
	int nTotalSkill = Npc[Player[nPlayerIndex].m_nIndex].m_SkillList.ClearSkillLevel(true);
	Lua_PushNumber(L, nTotalSkill);
	return 1;
}

int LuaUpdateSkillList(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) 
		return 0;
	Player[nPlayerIndex].SendSyncData_Skill();
	return 0;
}

int LuaGetMagicLevel(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	int nPlayerIndex = 0;
	
	if (nParamCount < 1) return 0;
	nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex <= 0) return 0;
	
	int nSkillId = 0;
	if (Lua_IsNumber(L, 1))
	{
		nSkillId = (int)Lua_ValueToNumber (L, 1);
	}
	else 
	{
		const char * sSkillName = Lua_ValueToString(L, 1);
		g_OrdinSkillsSetting.GetInteger((char *)sSkillName, "SkillId", 0, &nSkillId);
		if (nSkillId <= 0 ) return 0;
	}
	Lua_PushNumber(L,Npc[Player[nPlayerIndex].m_nIndex].m_SkillList.GetLevel(nSkillId));
	return 1;
	
}
/*
int LuaSetMagicLevel(Lua_State * L)
{
int nParamCount = Lua_GetTopIndex(L);
int nPlayerIndex = 0;
int nTemp = 0;
if (nParamCount < 1) return 0;

  nPlayerIndex = GetPlayerIndex(L);
  
	if (nPlayerIndex <= 0) return 0;
	
	  int nSkillId = 0;
	  if (Lua_IsNumber(L, nTemp))
	  {
	  nSkillId = (int)Lua_ValueToNumber (L, 1);
	  }
	  else 
	  {
	  const char * sSkillName = Lua_ValueToString(L, 1);
	  nSkillId = g_OrdinSkillsSetting.FindRow((char *)sSkillName) - 2;
	  if (nSkillId <= 0 ) return 0;
	  }
	  int nNpcIndex = Player[nPlayerIndex].m_nIndex;
	  if (nNpcIndex > 0)
	  Lua_PushNumber(L,Npc[nNpcIndex].m_SkillList.SetSkillLevel(nSkillId, (int)Lua_ValueToNumber(L, 2)));
	  return 0;
	  }
*/
//**************************************************************************************************************************************************************
//												NPC操作脚本
//**************************************************************************************************************************************************************
/*nNpcTemplateId GetNpcTmpId(sName)
功能从Npc模板中获得名称为sName的Npc在模板中的Id
sName:Npc名称
nNpcTemplateID:模板中Id
*/

int LuaGetNpcTemplateID(Lua_State * L)
{
	if (Lua_GetTopIndex(L) > 0)
	{
		if (Lua_IsString(L,1))
		{
			const char * pName = lua_tostring(L,1);	
			int nId = g_NpcSetting.FindRow((char*)pName) - 2;
			Lua_PushNumber(L, nId);
		}
		else
			return 0;
	}
	else 
		return 0;
	return 1;
}

/*
nNpcIndex AddNpc(nNpcTemplateId,nLevel, nSubWorldIndex, nPosX, nPosY )

  功能：增加一个特定的NPC
  参数：
  nNpcTemplateId: NPC在NPC模板中的id
  nLevel:Npc的等级
  nSubWorldIndex:所处的世界id
  nPosX：X (点坐标)
  nPosY：Y (点坐标)
  nNpcIndex:增加后，将返回该Npc在游戏世界的Index，如果不成功返回nil 
*/

int LuaAddNpc(Lua_State * L)
{
	char * pName = NULL;
	int	   nId = 0;
	if (Lua_GetTopIndex(L) < 5) return 0;
	
	if (Lua_IsNumber(L,1))
	{
		nId = (int)Lua_ValueToNumber(L,1);
	}
	else if	(Lua_IsString(L,1))
	{
		pName = (char *)lua_tostring(L,1);	
		nId = g_NpcSetting.FindRow((char*)pName) - 2;
	}
	else return 0;
	
	if (nId < 0) nId = 0;	
	
	int nLevel = (int)lua_tonumber(L,2);
	//if (nLevel >= 128) nLevel = 127;
	if (nLevel < 0 ) nLevel = 1;
	
	int	nNpcIdxInfo = MAKELONG(nLevel, nId);//(nId << 7) + nLevel;
	//question
	int nNpcIdx = NpcSet.Add(nNpcIdxInfo, (int)lua_tonumber(L, 3), (int)lua_tonumber(L,4), (int)lua_tonumber(L,5));
	//	g_StrCpy(Npc[nNpcIdx].Name, (char*)pName);
	Lua_PushNumber(L, nNpcIdx);
    return 1;
}

/*nResult DelNpc (nNpcIndex)
功能：删除一个特定的NPC
nResult:返回成功与否,1为成功,0为失败
*/
int LuaDelNpc(Lua_State * L)
{
	if (Lua_GetTopIndex(L) <= 0 ) return 0 ;
	int nNpcIndex = (int)Lua_ValueToNumber(L, 1);
	if (nNpcIndex > 0)
	{
		if (Npc[nNpcIndex].m_RegionIndex >= 0)
		{
			SubWorld[Npc[nNpcIndex].m_SubWorldIndex].m_Region[Npc[nNpcIndex].m_RegionIndex].RemoveNpc(nNpcIndex);
			SubWorld[Npc[nNpcIndex].m_SubWorldIndex].m_Region[Npc[nNpcIndex].m_RegionIndex].DecRef(Npc[nNpcIndex].m_MapX, Npc[nNpcIndex].m_MapY, obj_npc);
		}
		NpcSet.Remove(nNpcIndex);
	}
	return 0;
}

/*
nDelCount DelNpcsInRgn(nSubWorld,nRegionId, nKind) 
功能：删除某个游戏世界中某个Region内的所有某类的NPC
返回:删除的Npc个数
*/

int LuaDelNpcsInRgn(Lua_State * L)
{
	//Question
	return 0;
}
/*
nDelCount DelNpcsInWld(nSubWorldId, nKind)
功能：删除某个游戏世界中的所有Npc
*/
int LuaDelNpcsInWld(Lua_State * L)
{
	return 0;
}

/*
SetNpcPos (nNpcIndex, x, y)
功能：设置/修改一个NPC的位置
参数：
nNpcIndex:Npc的id 
x：X坐标
y：Y坐标
*/
int LuaSetNpcPos(Lua_State * L)
{
	int nParamCount = 0;
	if ((nParamCount = Lua_GetTopIndex(L)) < 3) return 0;
	int nNpcIndex = (int)Lua_ValueToNumber(L, 1);
	if (nNpcIndex <= 0) return 0;
	Npc[nNpcIndex].m_MapX = (int)Lua_ValueToNumber(L, 2);
	Npc[nNpcIndex].m_MapY = (int)Lua_ValueToNumber(L, 3);
	
	return 0;
}


/*SetNpcDthSct (nNpcIndex, map, “*.txt” )
功能：设置NPC死亡脚本
参数：
nNpcIndex：NPCIndex
*.txt：脚本文件名
*/
int LuaSetNpcActionScript(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 2 ) return 0;
	int nNpcIndex = (int)Lua_ValueToNumber(L, 1);
	if (nNpcIndex <= 0 || nNpcIndex >= MAX_NPC) return 0;
	strcpy(Npc[nNpcIndex].ActionScript, Lua_ValueToString(L,2));
	Npc[nNpcIndex].m_ActionScriptID = g_FileName2Id((char *)Lua_ValueToString(L,2));
	return 0;
}

/*
SetRevivalPos(subworldid = -1, revid )
功能：设置Npc重生点
*/
int LuaSetPlayerRevivalPos(Lua_State * L)
{
	//Question
	int nParamCount = Lua_GetTopIndex(L);
	int nPlayerIndex = 0;
	int nBeginIndex = 2;
	nPlayerIndex = GetPlayerIndex(L);
	int nSubWorldId = 0;
	int nRevId = 0;
	if (nPlayerIndex < 0) 
	{
		return 0;
	}
	
	if (nParamCount >= 2)
	{
		nSubWorldId = (int) Lua_ValueToNumber(L, 1);
		nRevId = (int) Lua_ValueToNumber(L, 2);
	}
	else if (nParamCount == 1)
	{
		nSubWorldId = -1;
		nRevId = (int) Lua_ValueToNumber(L, 1);
	}
	else 
	{
		return 0;
	}
	
	Player[nPlayerIndex].SetRevivalPos(nSubWorldId, nRevId);
	return 0;
}

//**************************************************************************************************************************************************************
//												聊天消息脚本
//**************************************************************************************************************************************************************

//**********************************************************************************************
//							主角属性获得
//**********************************************************************************************


#define MacroFun_GetPlayerInfoInt(L, MemberName) { int nPlayerIndex = GetPlayerIndex(L);\
	if (nPlayerIndex > 0){	int nNpcIndex = Player[nPlayerIndex].m_nIndex;	if (nNpcIndex > 0)Lua_PushNumber(L, Npc[nNpcIndex].MemberName);\
	else Lua_PushNil(L);}\
	else Lua_PushNil(L);\
return 1;}														

//阵营
int LuaGetPlayerCurrentCamp(Lua_State * L)
{
	MacroFun_GetPlayerInfoInt(L, m_CurrentCamp);
}

int LuaGetPlayerCamp(Lua_State * L)
{
	MacroFun_GetPlayerInfoInt(L, m_Camp);
}

int LuaSetPlayerCamp(Lua_State * L)
{
	int nValue = (int)Lua_ValueToNumber(L,1);
	if (nValue < 0 ) return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		Npc[Player[nPlayerIndex].m_nIndex].SetCamp(nValue);
	}
	return 0;
}

int LuaSetPlayerCurrentCamp(Lua_State * L)
{
	int nValue = (int)Lua_ValueToNumber(L,1);
	if (nValue < 0 ) return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		Npc[Player[nPlayerIndex].m_nIndex].SetCurrentCamp(nValue);
	}
	return 0;
}

int LuaSetNpcCurCamp(Lua_State * L)
{
	int nNpcIndex = (int)Lua_ValueToNumber(L,1);
	if (nNpcIndex <= 0 && nNpcIndex > MAX_NPC) return 0;
	int nValue = (int )Lua_ValueToNumber(L,2);
	if (nValue >= camp_num) return 0;
	Npc[nNpcIndex].SetCurrentCamp(nValue);
	return 0;
}

int LuaRestorePlayerCamp(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		Npc[Player[nPlayerIndex].m_nIndex].RestoreCurrentCamp();
	}
	return 0;
}

int LuaOpenTong(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) return 0;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_OPENTONGUI;
	UiInfo.m_bOptionNum = 0;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	int nMsgId = 0;

	UiInfo.m_bParam1 = 0;
	UiInfo.m_nBufferLen = sizeof(int);
	
#ifndef _SERVER
	UiInfo.m_bParam2 = 0;
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
#else
	UiInfo.m_bParam2 = 1;
	UiInfo.ProtocolType = (BYTE)s2c_scriptaction;
	UiInfo.m_wProtocolLong = sizeof(PLAYER_SCRIPTACTION_SYNC) - MAX_SCIRPTACTION_BUFFERNUM + UiInfo.m_nBufferLen - 1;
	g_NewProtocolProcess.BroadcastGlobal(&UiInfo, UiInfo.m_wProtocolLong + 1);
#endif
	return 0;
}



//门派
int LuaGetPlayerFaction(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);				
	if (nPlayerIndex > 0)								
	{	
		char FactionName[100];
		Player[nPlayerIndex].GetFactionName(FactionName, 100);
		Lua_PushString(L, FactionName);
	}
	else
	{
		Lua_PushString(L,"");
	}
	return 1;
}

int LuaChangePlayerFaction(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);				
	int nResult = 0;
	if (nPlayerIndex > 0)							
	{
		const char * szFactionName = Lua_ValueToString(L,1);
		Player[nPlayerIndex].LeaveCurFaction();
		if (strlen(szFactionName) == 0) 
		{
			nResult = 1;
		}
		else
		{
			nResult = Player[nPlayerIndex].AddFaction((char *)szFactionName);
		}
	}
	Lua_PushNumber(L, nResult);
	return 1;
}

//抗性 *************************************************************************************
//0表示当前,1表示原始的,2表示最大的
int LuaGetPlayerColdResist(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		int nType = (int)Lua_ValueToNumber(L,1);
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_CurrentColdResist);break;
		case 1:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_ColdResist); break;
		case 2:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_ColdResistMax);break;
		default:
			Lua_PushNil(L);
		} 
	}
	else 
		Lua_PushNil(L);
	return 1;
}

int LuaSetPlayerColdResist(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		int nValue = (int)Lua_ValueToNumber(L,2);
		if (nValue < 0) nValue = 0;
		if (nValue > Npc[Player[nPlayerIndex].m_nIndex].m_ColdResistMax) nValue = Npc[Player[nPlayerIndex].m_nIndex].m_ColdResistMax;
		
		int nType = (int)Lua_ValueToNumber(L,1);
		
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Npc[Player[nPlayerIndex].m_nIndex].m_CurrentColdResist = nValue;
			break;
			
		case 1:
			Npc[Player[nPlayerIndex].m_nIndex].m_ColdResist = nValue;
			break;
		case 2:
			Npc[Player[nPlayerIndex].m_nIndex].m_ColdResistMax = nValue;
			break;
		}
		
		
	} 
	return 0;
	
}

int LuaGetPlayerFireResist(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		int nType = (int)Lua_ValueToNumber(L,1);
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_CurrentFireResist);break;
		case 1:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_FireResist); break;
		case 2:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_FireResistMax);break;
		default:
			Lua_PushNil(L);
		}
	} 
	else 
		Lua_PushNil(L);
	return 1;
}

int LuaSetPlayerFireResist(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		
		int nValue = (int)Lua_ValueToNumber(L,2);
		if (nValue < 0) nValue = 0;
		if (nValue > Npc[Player[nPlayerIndex].m_nIndex].m_FireResistMax) nValue = Npc[Player[nPlayerIndex].m_nIndex].m_FireResistMax;
		
		int nType = (int)Lua_ValueToNumber(L,1);
		
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Npc[Player[nPlayerIndex].m_nIndex].m_CurrentFireResist = nValue;
			break;
			
		case 1:
			Npc[Player[nPlayerIndex].m_nIndex].m_FireResist = nValue;
			break;
		case 2:
			Npc[Player[nPlayerIndex].m_nIndex].m_FireResistMax = nValue;
			break;
		}
		
		
	} 
	return 0;
	
}


int LuaGetPlayerLightResist(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		int nType = (int)Lua_ValueToNumber(L,1);
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_CurrentLightResist);break;
		case 1:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_LightResist); break;
		case 2:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_LightResistMax);break;
		default:
			Lua_PushNil(L);
		}
	} 
	else
		Lua_PushNil(L);
	return 1;
}

int LuaSetPlayerLightResist(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		int nValue = (int)Lua_ValueToNumber(L,2);
		if (nValue < 0) nValue = 0;
		if (nValue > Npc[Player[nPlayerIndex].m_nIndex].m_LightResistMax) nValue = Npc[Player[nPlayerIndex].m_nIndex].m_LightResistMax;
		
		int nType = (int)Lua_ValueToNumber(L,1);
		
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Npc[Player[nPlayerIndex].m_nIndex].m_CurrentLightResist = nValue;
			break;
			
		case 1:
			Npc[Player[nPlayerIndex].m_nIndex].m_LightResist = nValue;
			break;
		case 2:
			Npc[Player[nPlayerIndex].m_nIndex].m_LightResistMax = nValue;
			break;
		}
	} 
	else 
		Lua_PushNil(L);
	return 0;
}


int LuaGetPlayerPoisonResist(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		int nType = (int)Lua_ValueToNumber(L,1);
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_CurrentPoisonResist);break;
		case 1:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_PoisonResist); break;
		case 2:
			Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_PoisonResistMax);break;
		default:
			Lua_PushNil(L);
		}
	} 
	else
		Lua_PushNil(L);
	return 1;
}

int LuaSetPlayerPoisonResist(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		{
			int nValue = (int)Lua_ValueToNumber(L,2);
            if (nValue < 0) nValue = 0;
            if (nValue > Npc[Player[nPlayerIndex].m_nIndex].m_PoisonResistMax) nValue = Npc[Player[nPlayerIndex].m_nIndex].m_PoisonResistMax;
			
            int nType = (int)Lua_ValueToNumber(L,1);
			
			switch((int)Lua_ValueToNumber(L,1))
			{
			case 0:
				Npc[Player[nPlayerIndex].m_nIndex].m_CurrentPoisonResist = nValue;
				break;
				
			case 1:
				Npc[Player[nPlayerIndex].m_nIndex].m_PoisonResist = nValue;
                break;
			case 2:
				Npc[Player[nPlayerIndex].m_nIndex].m_PoisonResistMax = nValue;
                break;
			}
		}
		
	} 
	
	
	return 0;
	
}

int LuaGetPlayerPhysicsResist(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		{
			int nType = (int)Lua_ValueToNumber(L,1);
			switch((int)Lua_ValueToNumber(L,1))
			{
			case 0:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_CurrentPhysicsResist);break;
			case 1:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_PhysicsResist); break;
			case 2:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_PhysicsResistMax);break;
			default:
				Lua_PushNil(L);
			}
		}
		
	} 
	else
		Lua_PushNil(L);
	return 1;
}



int LuaSetPlayerPhysicsResist(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		{
			int nValue = (int)Lua_ValueToNumber(L,2);
            if (nValue < 0) nValue = 0;
            if (nValue > Npc[Player[nPlayerIndex].m_nIndex].m_PhysicsResistMax) nValue = Npc[Player[nPlayerIndex].m_nIndex].m_PhysicsResistMax;
			
            int nType = (int)Lua_ValueToNumber(L,1);
			
			switch((int)Lua_ValueToNumber(L,1))
			{
			case 0:
				Npc[Player[nPlayerIndex].m_nIndex].m_CurrentPhysicsResist = nValue;
				break;
				
			case 1:
				Npc[Player[nPlayerIndex].m_nIndex].m_PhysicsResist = nValue;
                break;
			case 2:
				Npc[Player[nPlayerIndex].m_nIndex].m_PhysicsResistMax = nValue;
                break;
			}
		}
		
	} 
	return 0;
	
}


//经验值*********************************************************************
int LuaGetPlayerExp(Lua_State *L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
        Lua_PushNumber(L, Player[nPlayerIndex].m_nExp);
	}
	else
		Lua_PushNil(L);
	return 1;
}

//AddExp(200,10,0)
int LuaModifyPlayerExp(Lua_State * L)
{
	int bAllTeamGet = 0;
	if(Lua_GetTopIndex(L) >= 3)		bAllTeamGet = (int)Lua_ValueToNumber(L,3);
	
	int nDValue = (int)Lua_ValueToNumber(L,1);
	int nTarLevel = (int)Lua_ValueToNumber(L,2);
	
	int nPlayerIndex = GetPlayerIndex(L);				
	if (nPlayerIndex > 0)								
	{									
		if (bAllTeamGet)
			Player[nPlayerIndex].AddExp(nDValue, nTarLevel);
		else
			Player[nPlayerIndex].AddSelfExp(nDValue, nTarLevel);
	}
	return 0;
}

int LuaAddOwnExp(Lua_State * L)
{
	if (Lua_GetTopIndex(L) <=0 ) return 0;
	int nPlayerIndex = GetPlayerIndex(L);				
	if (nPlayerIndex > 0)								
	{									
		int nExp = (int)Lua_ValueToNumber(L,1);
#ifdef _SERVER
		KNpcScriptExchange *pState = g_NpcScriptExchange[nPlayerIndex];
		if (pState && nExp >= 0)
		{
			if (nExp > 0x7fffffff - pState->Experience)
				RejectNpcScriptItem(L, nPlayerIndex);
			pState->Experience += nExp;
			return 0;
		}
#endif
		if (nExp >= 0)
			Player[nPlayerIndex].DirectAddExp(nExp);
	}
	return 0;
}

int LuaGetPlayerLevel(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_Level);
	} 
	else
		Lua_PushNil(L);
	return 1;
}

// 新资源仍以一个持久任务整数保存四个字节，序号与原 GetByte/SetByte 一样从 1 开始。
int LuaGetTaskByte(Lua_State *L)
{
	int nResult = 0;
	int nParamNum = Lua_GetTopIndex(L);
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0 && nParamNum == 2)
	{
		int nTaskId = (int)Lua_ValueToNumber(L, 1);
		int nByte = (int)Lua_ValueToNumber(L, 2);
		if (nTaskId >= 0 && nTaskId < MAX_TASK && nByte >= 1 && nByte <= 4)
		{
			DWORD dwValue = (DWORD)Player[nPlayerIndex].m_cTask.GetSaveVal(nTaskId);
			nResult = (int)((dwValue >> ((nByte - 1) * 8)) & 0xff);
		}
	}
	Lua_PushNumber(L, nResult);
	return 1;
}

int LuaSetTaskByte(Lua_State *L)
{
	int nParamNum = Lua_GetTopIndex(L);
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0 || nParamNum != 3)
		return 0;
	int nTaskId = (int)Lua_ValueToNumber(L, 1);
	int nByte = (int)Lua_ValueToNumber(L, 2);
	if (nTaskId < 0 || nTaskId >= MAX_TASK || nByte < 1 || nByte > 4)
		return 0;
	DWORD dwShift = (nByte - 1) * 8;
	DWORD dwByte = (DWORD)(int)Lua_ValueToNumber(L, 3) & 0xff;
	DWORD dwValue = (DWORD)Player[nPlayerIndex].m_cTask.GetSaveVal(nTaskId);
	dwValue = (dwValue & ~(0xffUL << dwShift)) | (dwByte << dwShift);
	Player[nPlayerIndex].m_cTask.SetSaveVal(nTaskId, (int)dwValue);
	return 0;
}

// 原 NPC 六参数接口：种类、详细类别、具体类别、等级、五行、幸运。
// 模板、版本和堆叠规则全部复用现有道具生成器及背包入包流程。
int LuaAddNormalItem(Lua_State *L)
{
	int nParamNum = Lua_GetTopIndex(L);
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0 || nParamNum < 6 || nParamNum > 7 ||
		nParamNum == 7 && !Lua_IsTable(L, 7) &&
		(!Lua_IsNumber(L, 7) || Lua_ValueToNumber(L, 7) != 0))
	{
		RejectNpcScriptItem(L, nPlayerIndex);
		Lua_PushNumber(L, 0);
		return 1;
	}
	int Values[6];
	for (int i = 0; i < 6; i++)
	{
		double nValue = Lua_ValueToNumber(L, i + 1);
		if (!Lua_IsNumber(L, i + 1) || nValue < 0 || nValue > 2147483647.0 ||
			nValue != (int)nValue)
		{
			RejectNpcScriptItem(L, nPlayerIndex);
			Lua_PushNumber(L, 0);
			return 1;
		}
		Values[i] = (int)nValue;
	}
	// 可选表参数兼容原六参数接口：Count、Bound、BindingMode。
	int nCount = 1, nBound = 0, nBindingMode = 0;
	if (nParamNum == 7 && Lua_IsTable(L, 7) &&
		(!ReadScriptItemOption(L, 7, "Count", 1, 100000, &nCount) ||
		!ReadScriptItemOption(L, 7, "Bound", 0, 1, &nBound) ||
		!ReadScriptItemOption(L, 7, "BindingMode", 0, 255, &nBindingMode) ||
		!nBound && nBindingMode != 0))
	{
		RejectNpcScriptItem(L, nPlayerIndex);
		Lua_PushNumber(L, 0);
		return 1;
	}
	int MagicLevels[6] = {0};
	int nItem = ItemSet.Add(Values[0], Values[4], Values[3], Values[5],
		Values[1], Values[2], MagicLevels, g_SubWorldSet.GetGameVersion());
	// 新材料表没有等级列，旧 NPC 发奖仍有等级1的引用；只兼容这一种引用。
	if (nItem <= 0 && Values[0] == item_materials && Values[3] == 1)
	{
		nItem = ItemSet.Add(Values[0], Values[4], 0, Values[5],
			Values[1], Values[2], MagicLevels, g_SubWorldSet.GetGameVersion());
	}
	if (nItem <= 0)
	{
		RejectNpcScriptItem(L, nPlayerIndex);
		Lua_PushNumber(L, 0);
		return 1;
	}
	if (!Item[nItem].SetStackCount(nCount) ||
		Item[nItem].GetGenre() == item_equip && nCount != 1)
	{
		ItemSet.Remove(nItem);
		RejectNpcScriptItem(L, nPlayerIndex);
		Lua_PushNumber(L, 0);
		return 1;
	}
	if (nBound)
	{
		Item[nItem].SetBinding(TRUE, (BYTE)nBindingMode);
	}
	if (Item[nItem].GetGenre() == item_equip)
		Item[nItem].SetDurability(Item[nItem].GetMaxDurability());
	char szMagic[512];
	if (!Item[nItem].GetMagicDescription(szMagic, sizeof(szMagic)))
	{
		ItemSet.Remove(nItem);
		RejectNpcScriptItem(L, nPlayerIndex);
		Lua_PushNumber(L, 0);
		Lua_PushString(L, "");
		return 2;
	}
	BOOL bAdded = Player[nPlayerIndex].m_ItemList.AddPickupItem(nItem, -1, -1);
	if (!bAdded)
	{
		ItemSet.Remove(nItem);
		RejectNpcScriptItem(L, nPlayerIndex);
	}
	Lua_PushNumber(L, bAdded ? 1 : 0);
	Lua_PushString(L, bAdded ? szMagic : "");
	return 2;
}

// 新资源中的职业编号：0甲士、1道士、2异人，与角色存档和初始技能一致。
int LuaGetPlayerType(Lua_State *L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0 && Player[nPlayerIndex].m_nIndex > 0)
		Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_Series);
	else
		Lua_PushNil(L);
	return 1;
}

int LuaSystemTime(Lua_State *L)
{
	Lua_PushNumber(L, (double)time(NULL));
	return 1;
}

int LuaGetHMS(Lua_State *L)
{
	time_t nTime = time(NULL);
	tm *pTime = localtime(&nTime);
	if (!pTime)
		return 0;
	Lua_PushNumber(L, pTime->tm_hour);
	Lua_PushNumber(L, pTime->tm_min);
	Lua_PushNumber(L, pTime->tm_sec);
	return 3;
}

int LuaGetYMD(Lua_State *L)
{
	time_t nTime = time(NULL);
	tm *pTime = localtime(&nTime);
	if (!pTime)
		return 0;
	Lua_PushNumber(L, pTime->tm_year + 1900);
	Lua_PushNumber(L, pTime->tm_mon + 1);
	Lua_PushNumber(L, pTime->tm_mday);
	return 3;
}

int LuaWriteLog(Lua_State *L)
{
	if (Lua_GetTopIndex(L) < 1 || !Lua_IsString(L, 1))
		return 0;
	const char *pszMessage = Lua_ValueToString(L, 1);
	if (!pszMessage)
		return 0;

	char szMessage[512];
	g_StrCpyLen(szMessage, pszMessage, sizeof(szMessage));
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0 && nPlayerIndex < MAX_PLAYER)
	{
		g_DebugLog("[LuaScript] player=%d name=%s: %s", nPlayerIndex,
			Player[nPlayerIndex].m_PlayerName, szMessage);
	}
	else
	{
		g_DebugLog("[LuaScript] player=0: %s", szMessage);
	}
	return 0;
}

int LuaTime2LocalYMD(Lua_State *L)
{
	if (Lua_GetTopIndex(L) != 1 || !Lua_IsNumber(L, 1))
		return 0;
	time_t nTime = (time_t)Lua_ValueToNumber(L, 1);
	tm *pTime = localtime(&nTime);
	if (!pTime)
		return 0;
	Lua_PushNumber(L, pTime->tm_year + 1900);
	Lua_PushNumber(L, pTime->tm_mon + 1);
	Lua_PushNumber(L, pTime->tm_mday);
	return 3;
}

int LuaGetPlayerLife(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		{
			int nType = (int)Lua_ValueToNumber(L,1);
			switch((int)Lua_ValueToNumber(L,1))
			{
			case 0:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_CurrentLife);break;
			case 1:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_CurrentLifeMax); break;
			case 2:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_LifeMax);break;
			default:
				Lua_PushNil(L);
			}
		}
		
	} 
	else
		Lua_PushNil(L);
	return 1;
}

int LuaRestorePlayerLife(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		Npc[Player[nPlayerIndex].m_nIndex].RestoreLife();
		Player[nPlayerIndex].SendCurNormalSyncData();
	}
	return 0;
}

int LuaRestorePlayerMana(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		Npc[Player[nPlayerIndex].m_nIndex].RestoreMana();
		Player[nPlayerIndex].SendCurNormalSyncData();
	}
	return 0;
}

int LuaRestorePlayerStamina(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		Npc[Player[nPlayerIndex].m_nIndex].RestoreStamina();
	}
	return 0;
}

int LuaGetPlayerLifeReplenish(Lua_State * L)
{
	MacroFun_GetPlayerInfoInt( L, m_LifeReplenish);
}


int LuaGetPlayerMana(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		{
			int nType = (int)Lua_ValueToNumber(L,1);
			switch((int)Lua_ValueToNumber(L,1))
			{
			case 0:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_CurrentMana);break;
			case 1:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_CurrentManaMax); break;
			case 2:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_ManaMax);break;
			default:
				Lua_PushNil(L);
			}
		}
		
	} 
	else
		Lua_PushNil(L);
	return 1;
}

int LuaGetPlayerStamina(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		{
			int nType = (int)Lua_ValueToNumber(L,1);
			switch((int)Lua_ValueToNumber(L,1))
			{
			case 0:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_CurrentStamina);break;
			case 1:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_StaminaMax); break;
			case 2:
				Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_StaminaMax);break;
			default:
				Lua_PushNil(L);
			}
		}
		
	} 
	else
		Lua_PushNil(L);
	return 1;
}


int LuaGetPlayerManaReplenish(Lua_State * L)
{
	MacroFun_GetPlayerInfoInt(L , m_ManaReplenish);
}

int LuaGetPlayerDefend(Lua_State * L)
{
    MacroFun_GetPlayerInfoInt(L , m_Defend);
}

int LuaGetPlayerSex(Lua_State * L)
{
	MacroFun_GetPlayerInfoInt(L , m_nSex);
}

int LuaGetPlayerIndex(Lua_State * L)
{
	MacroFun_GetPlayerInfoInt(L , GetPlayerIdx());
	return 0;
}

int LuaGetPlayerSeries(Lua_State * L)
{
	MacroFun_GetPlayerInfoInt(L , m_Series);
}

int LuaSetPlayerSeries(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		int nValue = (int)Lua_ValueToNumber(L,1);
		Npc[Player[nPlayerIndex].m_nIndex].SetSeries(nValue);
	} 
	return 0;
    
}

int LuaGetPlayerCount(Lua_State * L)
{
	Lua_PushNumber(L, PlayerSet.GetPlayerNumber());
	return 1;
}

int LuaGetPlayerName(Lua_State * L)
{
    int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
        Lua_PushString(L, Player[nPlayerIndex].m_PlayerName);
	}
	else
		Lua_PushNil(L);
	
	return 1;
	
}


int LuaGetPlayerID(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
        Lua_PushNumber(L, Player[nPlayerIndex].m_dwID);
	}
	else
		Lua_PushNil(L);
	
	return 1;
}

int LuaGetPlayerLeadExp(Lua_State * L)
{
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
        Lua_PushNumber(L, Player[nPlayerIndex].m_dwLeadExp);
	}
	else
		Lua_PushNil(L);
	
	return 1;
}

int LuaGetPlayerLeadLevel(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
        Lua_PushNumber(L, Player[nPlayerIndex].m_dwLeadLevel);
	}
	else
		Lua_PushNil(L);
	
	return 1;
}



int LuaGetPlayerRestAttributePoint(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
        Lua_PushNumber(L, Player[nPlayerIndex].m_nAttributePoint);
	}
	else
		Lua_PushNil(L);
	
	return 1;
}

int LuaGetPlayerRestSkillPoint(Lua_State * L)
{
    int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
        Lua_PushNumber(L, Player[nPlayerIndex].m_nSkillPoint);
	}
	else
		Lua_PushNil(L);
	
	return 1;
}

/*
int LuaModifyPlayerRestSkillPoint(Lua_State *L)
{
//Question
int nPlayerIndex = GetPlayerIndex(L);
if (nPlayerIndex > 0)
{
int nDValue = (int)Lua_ValueToNumber(L, 1);

  int nNewSkillPoint = Player[nPlayerIndex].m_nSkillPoint + nDValue;
  if (nNewSkillPoint < 0 ) return 0;
  
	Player[nPlayerIndex].m_nSkillPoint = nNewSkillPoint;
	}
	
	  return 0;
	  }
*/

//基本属性
int LuaGetPlayerLucky(Lua_State * L)
{
    int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		int nType = (int)Lua_ValueToNumber(L,1);
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nCurLucky);break;
		case 1:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nLucky);break;
		case 2:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nLucky);break;
		default:
			Lua_PushNil(L);
		}
		
	} 
	return 1;
}

int LuaGetPlayerEngergy(Lua_State * L)
{
    int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		int nType = (int)Lua_ValueToNumber(L,1);
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nCurEngergy);break;
		case 1:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nEngergy);break;
		case 2:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nEngergy);break;
		default:
			Lua_PushNil(L);
		}
	}
	
	return 1;
}

int LuaGetPlayerDexterity(Lua_State * L)
{
    int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		int nType = (int)Lua_ValueToNumber(L,1);
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nCurDexterity);break;
		case 1:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nDexterity);break;
		case 2:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nDexterity);break;
		default:
			Lua_PushNil(L);
		}
		
	} 
	return 1;
}

int LuaGetPlayerStrength(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		int nType = (int)Lua_ValueToNumber(L,1);
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nCurStrength);break;
		case 1:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nStrength);break;
		case 2:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nStrength);break;
		default:
			Lua_PushNil(L);
		}
		
	} 
	return 1;
	
}

int LuaGetPlayerVitality(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0)
	{
		
		int nType = (int)Lua_ValueToNumber(L,1);
		switch((int)Lua_ValueToNumber(L,1))
		{
		case 0:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nCurVitality);break;
		case 1:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nVitality);break;
		case 2:
			Lua_PushNumber(L, Player[nPlayerIndex].m_nVitality);break;
		default:
			Lua_PushNil(L);
		}
		
	} 
	return 1;
	
}

int LuaGetPlayerCashMoney(Lua_State * L)
{
    int nPlayerIndex = GetPlayerIndex(L);
	
    if (nPlayerIndex > 0)
	{
		Lua_PushNumber(L, Player[nPlayerIndex].m_ItemList.GetMoney(room_equipment));
	}
	else Lua_PushNumber(L,0);
	
	return 1;
}

int LuaPlayerPayMoney(Lua_State * L)
{
    
    int nPlayerIndex = GetPlayerIndex(L);
    if (nPlayerIndex > 0)
	{
        int nMoney = (int)Lua_ValueToNumber(L, 1);
        if (nMoney <= 0) return 0;
        if (Player[nPlayerIndex].Pay(nMoney))
			Lua_PushNumber(L, 1);
		else
			Lua_PushNumber(L, 0);
	}
	else
		Lua_PushNumber(L, 0);
	
	return 1;
}

int LuaPlayerEarnMoney (Lua_State  *L)
{
    int nPlayerIndex = GetPlayerIndex(L);
    if (nPlayerIndex > 0)
	{
        int nMoney = (int)Lua_ValueToNumber(L, 1);
        if (nMoney <= 0) return 0;
        if (!Player[nPlayerIndex].Earn(nMoney))
			RejectNpcScriptItem(L, nPlayerIndex);
	}
	return 0;	
}

int LuaPlayerPrePayMoney(Lua_State *L)
{   
    int nPlayerIndex = GetPlayerIndex(L);
    if (nPlayerIndex > 0)
	{
        int nMoney = (int)Lua_ValueToNumber(L, 1);
        if (nMoney <= 0) return 0;
        if (Player[nPlayerIndex].PrePay(nMoney))
			Lua_PushNumber(L, 1);
		else
			Lua_PushNumber(L, 0);
	}
	else
		Lua_PushNumber(L, 0);
	
	return 1;
}


int LuaPlayerExecuteScript(Lua_State * L)
{
	
	return 1;
}

//Attack dwID, Damage
int LuaAttackNpc(Lua_State * L)
{
	int nParamCount = 0;
	if ( (nParamCount = Lua_GetTopIndex(L)) < 2) return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	
	DWORD nNpcID = (DWORD)Lua_ValueToNumber(L,1);
	int nNpcIndex = Player[nPlayerIndex].FindAroundNpc(nNpcID);//NpcSet.SearchID(nNpcID);
	if (nNpcIndex <= 0) return 0;
	
	KMagicAttrib DamageMagicAttribs[MAX_MISSLE_DAMAGEATTRIB];
	memset(DamageMagicAttribs, 0, sizeof(DamageMagicAttribs));
	
	DamageMagicAttribs[0].nAttribType = magic_attackrating_v;
	DamageMagicAttribs[0].nValue[0] = 65534;
	
	DamageMagicAttribs[1].nAttribType = magic_ignoredefense_p;
	DamageMagicAttribs[1].nValue[0] = 1;
	
	for (int i = 0; i < nParamCount - 1; i++)
	{
		int nVlau = (int)Lua_ValueToNumber(L, 2 + i);
		DamageMagicAttribs[i + 2].nValue[0] = (int)Lua_ValueToNumber(L, 2 + i);
		DamageMagicAttribs[i + 2].nValue[2] = (int)Lua_ValueToNumber(L, 2 + i);
	}
	
	Npc[nNpcIndex].ReceiveDamage(Player[nPlayerIndex].m_nIndex, 0, DamageMagicAttribs, 0, TRUE);
	return 0;
}

int LuaSetPlayerChatForbiddenFlag(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0)
		return 0;
	int nFlag = Lua_ValueToNumber(L, 1);
	Player[nPlayerIndex].SetChatForbiddenFlag(nFlag);
	return 0;
}


int LuaKickOutPlayer(Lua_State *L)
{
	if (Lua_GetTopIndex(L) < 1)
		return 0;

	const char*	pszName = (const char*)Lua_ValueToString(L, 1);
	
	int nIndex = PlayerSet.GetFirstPlayer();
	while(nIndex > 0)
	{
		if (strcmp(Player[nIndex].m_PlayerName, pszName) == 0)
			break;
		
		nIndex = PlayerSet.GetNextPlayer();
	}
	if (nIndex && Player[nIndex].m_nNetConnectIdx >= 0)
	{
		printf("GM Kick out specific player.\n");
		g_pServer->ShutdownClient(Player[nIndex].m_nNetConnectIdx);
	}
	return 0;
}

int LuaKickOutAccount(Lua_State *L)
{
	if (Lua_GetTopIndex(L) < 1)
		return 0;

	const char*	pszName = (const char*)Lua_ValueToString(L, 1);
	
	int nIndex = PlayerSet.GetFirstPlayer();
	while(nIndex > 0)
	{
		if (strcmpi(Player[nIndex].m_AccoutName, pszName) == 0)
			break;
		
		nIndex = PlayerSet.GetNextPlayer();
	}
	if (nIndex && Player[nIndex].m_nNetConnectIdx >= 0)
	{
		printf("GM Kick out specific player.\n");
		g_pServer->ShutdownClient(Player[nIndex].m_nNetConnectIdx);
	}
	return 0;
}

int LuaKickOutSelf(Lua_State *L)
{
	int nIndex = GetPlayerIndex(L);
	
	if (nIndex <= 0)
		return 0;
	
	if (Player[nIndex].m_nNetConnectIdx >= 0)
	{
		printf("GM Kick out player one.\n");
		g_pServer->ShutdownClient(Player[nIndex].m_nNetConnectIdx);
	}
	return 0;
}

int LuaKillNpc(Lua_State * L)
{
	int nParamCount = 0;
	if ( (nParamCount = Lua_GetTopIndex(L)) < 1) return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	
	DWORD nNpcID = (DWORD)Lua_ValueToNumber(L,1);
	int nNpcIndex = Player[nPlayerIndex].FindAroundNpc(nNpcID);
	if (nNpcIndex <= 0) return 0;
	
	KMagicAttrib DamageMagicAttribs[MAX_MISSLE_DAMAGEATTRIB];
	memset(DamageMagicAttribs, 0, sizeof(DamageMagicAttribs));
	
	DamageMagicAttribs[0].nAttribType = magic_attackrating_v;
	DamageMagicAttribs[0].nValue[0] = 50000;
	
	
	
	DamageMagicAttribs[1].nAttribType = magic_ignoredefense_p;
	DamageMagicAttribs[1].nValue[0] = 1;
	
	
	DamageMagicAttribs[2].nValue[0] = 50000;
	DamageMagicAttribs[2].nValue[2] = 50000;
	
	Npc[nNpcIndex].ReceiveDamage(Player[nPlayerIndex].m_nIndex, 0, DamageMagicAttribs, 0, TRUE);
	return 0;
}

int LuaKillPlayer(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) 
		return 0;
	KMagicAttrib DamageMagicAttribs[MAX_MISSLE_DAMAGEATTRIB];
	memset(DamageMagicAttribs, 0, sizeof(DamageMagicAttribs));
	
	DamageMagicAttribs[0].nAttribType = magic_attackrating_v;
	DamageMagicAttribs[0].nValue[0] = 50000;
	
	DamageMagicAttribs[1].nAttribType = magic_ignoredefense_p;
	DamageMagicAttribs[1].nValue[0] = 1;
	
	
	DamageMagicAttribs[2].nValue[0] = 50000;
	DamageMagicAttribs[2].nValue[2] = 50000;
	
	Npc[Player[nPlayerIndex].m_nIndex].ReceiveDamage(Player[nPlayerIndex].m_nIndex, 0, DamageMagicAttribs, 0, TRUE);
	return 0;
}

int LuaSetFightState(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	Npc[Player[nPlayerIndex].m_nIndex].SetFightMode(Lua_ValueToNumber(L,1) != 0);
	return 0;
}

int LuaGetFightState(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_FightMode);
	return 1;
}

int LuaGetLevel(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)		Lua_PushNumber(L,0);
	
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	Lua_PushNumber(L, Npc[Player[nPlayerIndex].m_nIndex].m_Level);
	return 1;
}
//
int	LuaUseTownPortal(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	
	Player[nPlayerIndex].UseTownPortal();
	return 0;
}

int LuaReturnFromTownPortal(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	
	Player[nPlayerIndex].BackToTownPortal();
	return 0;
}
#endif

int GetPlayerIndex(Lua_State * L)
{
	Lua_GetGlobal(L, SCRIPT_PLAYERINDEX);
    if (lua_isnil(L,Lua_GetTopIndex(L)))
        return -1;
	int nIndex = (int)Lua_ValueToNumber(L, Lua_GetTopIndex(L));
    if (nIndex >= MAX_PLAYER || nIndex <= 0) 
	{
		_ASSERT(0);
		return -1;
	}	if (Player[nIndex].m_nIndex >= MAX_NPC || Player[nIndex].m_nIndex < 0)
	{
		_ASSERT(0);
		return -1;
	}
    return nIndex;
}

int GetObjIndex(Lua_State * L)
{
	Lua_GetGlobal(L, SCRIPT_OBJINDEX);
    if (lua_isnil(L,Lua_GetTopIndex(L)))
        return -1;
	int nIndex = (int)Lua_ValueToNumber(L, Lua_GetTopIndex(L));
    if (nIndex >= MAX_OBJECT || nIndex <= 0) 
	{
		_ASSERT(0);
		return -1;
	}
	if (Object[nIndex].m_nIndex != nIndex)
	{
		_ASSERT(0);
		return -1;
	}
    return nIndex;
}




int  LuaMessage(Lua_State * L)
{
	const char * szString;
	szString  = lua_tostring (L,1);
	g_DebugLog((char *)szString);
	return 0;
}
#ifdef _SERVER
//AddStation(N)
int LuaAddPlayerWayPoint(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	int nWayPoint = (int)Lua_ValueToNumber(L,1);
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	{
		KIndexNode * pNode = (KIndexNode*) Player[nPlayerIndex].m_PlayerWayPointList.GetHead();
		while(pNode)
		{
			if (pNode->m_nIndex == nWayPoint) return 0;
			pNode = (KIndexNode*)pNode->GetNext();
		}
		
		KIndexNode * pNewNode = new KIndexNode;
		pNewNode->m_nIndex = nWayPoint;
		int nCount = Player[nPlayerIndex].m_PlayerWayPointList.GetNodeCount();
		for (int i = 0; i < nCount - 2; i ++ )
		{
			KIndexNode *  pDelNode = (KIndexNode*)Player[nPlayerIndex].m_PlayerWayPointList.RemoveHead();
			delete pDelNode;
		}
		Player[nPlayerIndex].m_PlayerWayPointList.AddTail(pNewNode);
	}
	return 0;
}

int LuaAddPlayerStation(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	int nStation = (int )Lua_ValueToNumber(L,1);
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	{
		KIndexNode * pNode = (KIndexNode*) Player[nPlayerIndex].m_PlayerStationList.GetHead();
		while(pNode)
		{
			if (pNode->m_nIndex == nStation) return 0;
			pNode = (KIndexNode*)pNode->GetNext();
		}
		
		KIndexNode * pNewNode = new KIndexNode;
		pNewNode->m_nIndex = nStation;
		Player[nPlayerIndex].m_PlayerStationList.AddTail(pNewNode);
	}
	return 0;
}

int LuaGetPlayerStationCount(Lua_State *L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	Lua_PushNumber(L, Player[nPlayerIndex].m_PlayerStationList.GetNodeCount());
	return 1;
}

//获得当前玩家有效城市列表中的第n个（除去当前所在城市）
int LuaGetPlayerStation(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	
	if (Lua_GetTopIndex(L) < 2)
	{
		Lua_PushNumber(L, 0);
		return 1;
	}
	
	int nStationId = 0;
	KIndexNode * pNode = 	(KIndexNode*)Player[nPlayerIndex].m_PlayerStationList.GetHead();
	if (pNode)
	{
		int nNo = (int )Lua_ValueToNumber(L, 1);
		int nCurStation = (int) Lua_ValueToNumber(L,2);
		int nVisitNo = 0;
		while(pNode)
		{
			if (pNode->m_nIndex != nCurStation && g_GetPriceToStation( nCurStation , pNode->m_nIndex) > 0) 
			{
				nVisitNo ++;
				if (nVisitNo == nNo)
				{
					nStationId = pNode->m_nIndex;
					break;
				}
			}
			pNode = (KIndexNode*)pNode->GetNext();
		}
	}
	
	Lua_PushNumber(L, nStationId);
	
	return 1;
}

int LuaGetPlayerWayPoint(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	
	KIndexNode * pNode = (KIndexNode*)	Player[nPlayerIndex].m_PlayerWayPointList.GetHead();
	if (pNode)
	{
		int nNo = (int)Lua_ValueToNumber(L, 1);
		if (nNo > TASKVALUE_MAXWAYPOINT_COUNT) 
			Lua_PushNumber(L, 0);
		else
		{
			for (int i = 0; i < nNo - 1; i ++)
			{
				if (pNode == NULL ) break;
				pNode = (KIndexNode *)pNode->GetNext();
			}
			
			if (pNode)
				Lua_PushNumber(L, pNode->m_nIndex);
			else
				Lua_PushNumber(L, 0);
			
		}
	}
	else
		Lua_PushNumber(L, 0);
	
	return 1;
}
//根据传入的城市唯一id获得其名称
int LuaGetStationName(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	if (Lua_GetTopIndex(L) <= 0 ) 
	{
		Lua_PushString(L, "");
		return 1;
	}
	int nStationId = (int)Lua_ValueToNumber(L,1);
	char szName[50];
	g_StationTabFile.GetString(nStationId + 1, "DESC", "无名城",  szName, 50 );
	Lua_PushString(L, szName);
	return 1;
}

int LuaGetWayPointName(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	if (Lua_GetTopIndex(L) <= 0 ) 
	{
		Lua_PushString(L, "");
		return 1;
	}
	int nWayPointId = (int)Lua_ValueToNumber(L,1);
	char szName[50];
	g_WayPointTabFile.GetString(nWayPointId + 1, "DESC", "未记录",  szName, 50 );
	Lua_PushString(L, szName);
	return 1;
}

//GetCityCount 
int LuaGetAllStationCount(Lua_State * L)
{
	int nCityCount = g_StationTabFile.GetHeight() - 1;
	if (nCityCount < 0) nCityCount = 0;
	Lua_PushNumber(L,nCityCount);
	return 1;
}

//cityid, price = GetCity(citynum, curcity)
int LuaGetCity(Lua_State * L)
{
	return 0;
}


int LuaGetPriceToWayPoint(Lua_State *L)
{
	
	int nCurStation = (int)Lua_ValueToNumber(L,1);
	int nDesWayPoint = (int)Lua_ValueToNumber(L,2);
	Lua_PushNumber(L, g_GetPriceToWayPoint(nCurStation, nDesWayPoint));
	return 1;
}

int LuaGetPriceToStation(Lua_State *L)
{
	int nCurStation	 = (int)Lua_ValueToNumber(L,1);
	int nNextStation = (int)Lua_ValueToNumber(L,2);
	Lua_PushNumber(L, g_GetPriceToStation(nCurStation, nNextStation));
	return 1;
}

int LuaGetStationPos(Lua_State * L)
{
	int nStationId = (int)Lua_ValueToNumber(L,1);
	char szPos[100] ;
	int nCount = 0;
	int nRow = g_StationTabFile.FindColumn("COUNT");
	g_StationTabFile.GetInteger(nStationId + 1, nRow,  0, &nCount);
	if (nCount <= 0) return 0;
	int nRandSect = g_Random(100) % nCount + 1;
	char szSectName[32];
	sprintf(szSectName, "SECT%d", nRandSect);
	char szValue[100];
	nRow = g_StationTabFile.FindColumn(szSectName);
	g_StationTabFile.GetString(nStationId + 1, nRow, "0,0,0", szValue, 100);
	
    int nX, nY, nWorld;
    const char *pcszTemp = szValue;
    
    nWorld = KSG_StringGetInt(&pcszTemp, 0);
    KSG_StringSkipSymbol(&pcszTemp, ',');
    nX = KSG_StringGetInt(&pcszTemp, 0);
    KSG_StringSkipSymbol(&pcszTemp, ',');
    nY = KSG_StringGetInt(&pcszTemp, 0);
	//sscanf(szValue, "%d,%d,%d", &nWorld, &nX, &nY);
	
    Lua_PushNumber(L,nWorld);
	Lua_PushNumber(L,nX);
	Lua_PushNumber(L,nY);
	return 3;
}

int LuaGetWayPointPos(Lua_State * L)
{
	int nWayPointId = (int)Lua_ValueToNumber(L,1);
	char szPos[100] ;
	int nCount = 0;
	char szValue[30];
	int nRow;
	nRow = g_WayPointTabFile.FindColumn("SECT");
	g_WayPointTabFile.GetString(nWayPointId + 1, nRow, "0,0,0", szValue, 30);
	int nX, nY, nWorld;
    const char *pcszTemp = szValue;
    
    nWorld = KSG_StringGetInt(&pcszTemp, 0);
    KSG_StringSkipSymbol(&pcszTemp, ',');
    nX = KSG_StringGetInt(&pcszTemp, 0);
    KSG_StringSkipSymbol(&pcszTemp, ',');
    nY = KSG_StringGetInt(&pcszTemp, 0);
	//sscanf(szValue, "%d,%d,%d", &nWorld, &nX, &nY);
	
	Lua_PushNumber(L,nWorld);
	Lua_PushNumber(L,nX);
	Lua_PushNumber(L,nY);
	return 3;
}

int LuaGetRank(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	Lua_PushNumber(L,Npc[Player[nPlayerIndex].m_nIndex].m_btRankId);
	return 1;
}

int LuaSetRank(Lua_State * L)
{
	BYTE btRank = (BYTE)Lua_ValueToNumber(L,1);
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	Npc[Player[nPlayerIndex].m_nIndex].m_btRankId = btRank;
	return 0;
}

int LuaSetObjPropState(Lua_State *L)
{
	int  nParamNum = ( int ) Lua_GetTopIndex(L);
	int nState  = 1;
	
	if (nParamNum >= 1)
	{
		nState = (int)Lua_ValueToNumber(L,1);
		nState = (nState == 0)?0 : 1;
	}
	
	int nIndex = 0;
	if ((nIndex = GetObjIndex(L)) < 0) 
		return 0;
	
	Object[nIndex].SetState(nState);
	return 0;
}


int	LuaGetServerName(Lua_State * L)
{
	char szServerName[100]  ;
	unsigned long   stServerNameLen = 100;

#ifndef __linux
	if (GetComputerName(szServerName, &stServerNameLen))
	{
		Lua_PushString(L, szServerName);
	}
	else
#else
	if (SOCKET_ERROR != gethostname(szServerName, sizeof(szServerName)))
	{
		Lua_PushString(L, szServerName);
	}
	else
#endif
	Lua_PushString(L, "");

	return 1;
}

//GetWharfCount(nDock)
int LuaGetDockCount(Lua_State * L)
{
	int nCount = 0;
	int nCurStation = 0;
	int nTotalCount = 0;
	int i  = 0;
	if (Lua_GetTopIndex(L) < 1 )  
	{
		goto DockCount;
	}
	
	nCurStation = (int)Lua_ValueToNumber(L,1);
	nTotalCount = g_DockPriceTabFile.GetHeight() - 1;
	
	for (i = 0; i < nTotalCount; i ++)
	{
		int nPrice = g_GetPriceToDock(nCurStation, i + 1);
		if (nPrice > 0) nCount ++;
	}
	
DockCount:
	Lua_PushNumber(L, nCount);
	return 1;
}

int LuaGetDockPrice(Lua_State * L)
{
	int nCurDock = (int)Lua_ValueToNumber(L,1);
	int nDesDock = (int)Lua_ValueToNumber(L,2);
	Lua_PushNumber(L, g_GetPriceToDock(nCurDock, nDesDock));
	return 1;
}

int LuaGetDock(Lua_State * L)
{
	int nCurDock = (int)Lua_ValueToNumber(L, 1);
	int nDock = (int)Lua_ValueToNumber(L, 2);
	int nCount = 0;
	int nTotalCount = g_DockPriceTabFile.GetHeight() - 1;
	int nGetDock = 0;
	
	for (int i = 0; i < nTotalCount; i ++)
	{
		int nPrice = g_GetPriceToDock(nCurDock, i + 1);
		if (nPrice > 0) 
		{
			nCount ++ ;
			if (nCount == nDock)
			{
				nGetDock = i + 1;
				break;
			}
		}
	}
	Lua_PushNumber(L, nGetDock);
	return 1;
}

int LuaGetDockName(Lua_State * L)
{
	int nDock  = (int)Lua_ValueToNumber(L, 1);
	char szName[100] ;
	
	if (nDock > g_DockPriceTabFile.GetHeight() - 1)
	{
		strcpy(szName, "未知码头");
		goto DockName;
	}
	
	g_DockTabFile.GetString(nDock + 1, "DESC", "未知码头", szName, 100);
	
DockName:
	Lua_PushString (L, szName);
	return 1;
}

int LuaGetDockPos(Lua_State * L)
{
	int nDock  = (int)Lua_ValueToNumber(L, 1);
	if (nDock > g_DockTabFile.GetHeight() - 1) 
	{
		printf("GetWharfPos Script Is Error!");
		return 0;
	}
	
	char szPos[100] ;
	int nCount = 0;
	int nRow = g_DockTabFile.FindColumn("COUNT");
	g_DockTabFile.GetInteger(nDock + 1, nRow,  0, &nCount);
	if (nCount <= 0) return 0;
	int nRandSect = g_Random(100) % nCount + 1;
	char szSectName[32];
	sprintf(szSectName, "SECT%d", nRandSect);
	char szValue[100];
	nRow = g_DockTabFile.FindColumn(szSectName);
	g_DockTabFile.GetString(nDock + 1, nRow, "0,0,0", szValue, 100);
	int nX, nY, nWorld;
    const char *pcszTemp = szValue;
    
    nWorld = KSG_StringGetInt(&pcszTemp, 0);
    KSG_StringSkipSymbol(&pcszTemp, ',');
    nX = KSG_StringGetInt(&pcszTemp, 0);
    KSG_StringSkipSymbol(&pcszTemp, ',');
    nY = KSG_StringGetInt(&pcszTemp, 0);
	//sscanf(szValue, "%d,%d,%d", &nWorld, &nX, &nY);
	
	Lua_PushNumber(L,nWorld);
	Lua_PushNumber(L,nX);
	Lua_PushNumber(L,nY);
	return 3;
}

int LuaGetWayPointFightState(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) return 0;
	int nFightState = 0;
	if (Lua_GetTopIndex(L) > 0 ) 
	{
		int nWayPointId = (int)Lua_ValueToNumber(L,1);
		g_WayPointTabFile.GetInteger(nWayPointId + 1, "FightState", 0,  &nFightState);
	}
	Lua_PushNumber(L, nFightState);
	return 1;
}

// SetMissionValue(valueid, value)
int LuaSetMissionValue(Lua_State * L)
{
	int nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex < 0) 
		return 0;
	
	int nParamCount = Lua_GetTopIndex(L);
	if (nParamCount < 2) 
		return 0;
	
	int nValueId = (int)Lua_ValueToNumber(L, 1);
	int nValue = (int)Lua_ValueToNumber(L, 2);
	
	if (nValueId  < 0)
		return 0;
	SubWorld[nSubWorldIndex].m_MissionArray.SetMissionValue(nValueId, nValue);
	return 0;
}

int LuaGetMissionValue(Lua_State * L)
{
	int nResultValue = 0;
	int nSubWorldIndex = -1;
	int nParamCount = Lua_GetTopIndex(L);
	if (nParamCount < 1) 
		goto lab_getmissionvalue;
	
	nSubWorldIndex = GetSubWorldIndex(L);
	
	if (nSubWorldIndex >= 0) 
	{
		int  nValueId = (int)Lua_ValueToNumber(L, 1);
		if (nValueId > 0)
			nResultValue = SubWorld[nSubWorldIndex].m_MissionArray.GetMissionValue(nValueId);
	}
	
lab_getmissionvalue:
	Lua_PushNumber(L, nResultValue);
	return 1;
}


// SetMissionValue(mapid/mapname, valueid, value)
int LuaSetGlobalMissionValue(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	if (nParamCount < 2) 
		return 0;
	
	int nValueId = (int)Lua_ValueToNumber(L, 1);
	int nValue = (int)Lua_ValueToNumber(L, 2);
	
	if (nValueId  < 0)
		return 0;
	g_GlobalMissionArray.SetMissionValue(nValueId, nValue);
	return 0;
}

int LuaGetGlobalMissionValue(Lua_State * L)
{
	int nResultValue = 0;
	int nValueId = 0;
	int nParamCount = Lua_GetTopIndex(L);
	if (nParamCount < 1) 
		goto lab_getglobalmissionvalue;
	nValueId = (int)Lua_ValueToNumber(L, 1);
	if (nValueId < 0)
		goto lab_getglobalmissionvalue;
	
	nResultValue = g_GlobalMissionArray.GetMissionValue(nValueId);
	
lab_getglobalmissionvalue:
	Lua_PushNumber(L, nResultValue);
	return 1;
}

//StartMission(missionid)
int LuaInitMission(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	
	int nMissionId = (int)Lua_ValueToNumber(L, 1);
	if (nMissionId < 0 )
		return 0;
	
	int nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex < 0) 
		return 0;
	int nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex <= 0) 
		return 0;
	
	KMission Mission;
	Mission.SetMissionId(nMissionId);
	KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
	if (pMission)
	{
		_ASSERT(0);
		return 0;
	}
	
	pMission = SubWorld[nSubWorldIndex].m_MissionArray.Add();
	if (pMission)
	{
		pMission->m_MissionPlayer.Clear();
		pMission->SetMissionId(nMissionId);
		char szScript[MAX_PATH];
		g_MissionTabFile.GetString(nMissionId + 1, 2, "", szScript, MAX_PATH);
		if (szScript[0])
			Player[nPlayerIndex].ExecuteScript(szScript, "InitMission", "");
	}
	
	return 0;
}

int LuaRunMission(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	
	int nMissionId = (int)Lua_ValueToNumber(L, 1);
	if (nMissionId < 0 )
		return 0;
	
	int nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex < 0) 
		return 0;
	int nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex <= 0) 
		return 0;
	KMission Mission;
	Mission.SetMissionId(nMissionId);
	KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
	if (pMission)
	{
		char szScript[MAX_PATH];
		g_MissionTabFile.GetString(nMissionId + 1, 2, "", szScript, MAX_PATH);
		if (szScript[0])
			Player[nPlayerIndex].ExecuteScript(szScript, "RunMission", "");
	}
	
	return 0;
}
//CloseMission(missionId)
int LuaCloseMission(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	
	int nMissionId = (int)Lua_ValueToNumber(L, 1);
	if (nMissionId < 0 )
		return 0;
	
	int nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex < 0) 
		return 0;
	KMission StopMission;
	StopMission.SetMissionId(nMissionId);
	KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&StopMission);
	if (pMission)
	{
		char szScript[MAX_PATH];
		g_MissionTabFile.GetString(nMissionId + 1, 2, "", szScript, MAX_PATH);
		if (szScript[0])
			pMission->ExecuteScript(szScript, "EndMission",0);
		pMission->StopMission();
		SubWorld[nSubWorldIndex].m_MissionArray.Remove(pMission);
		
	}
	return 0;
}
//StopMissionTimer(missionid, timerid)
int LuaStopMissionTimer(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 2) 
		return 0;
	int nMissionId = (int)Lua_ValueToNumber(L, 1);
	int nTimerId = (int)Lua_ValueToNumber(L, 2);
	int nSubWorldIndex = GetSubWorldIndex(L);
	
	if (nMissionId < 0 || nTimerId < 0 ) 
		return 0;
	
	if (nSubWorldIndex >= 0) 
	{
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			KTimerTaskFun StopTimer;
			StopTimer.SetTimer(1, nTimerId);
			KTimerTaskFun * pTimer = pMission->m_cTimerTaskSet.GetData(&StopTimer);
			if (pTimer)
			{
				pTimer->CloseTimer();
				pMission->m_cTimerTaskSet.Remove(pTimer);
			}
		}
		
	}
	
	return 0;
}

//StartMissionTimer(missionid, timerid, time)
int LuaStartMissionTimer(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 3) 
		return 0;
	int nMissionId = (int)Lua_ValueToNumber(L, 1);
	int nTimerId = (int)Lua_ValueToNumber(L, 2);
	int nTimeInterval = (int)Lua_ValueToNumber(L, 3);
	int nSubWorldIndex = GetSubWorldIndex(L);
	
	if (nMissionId < 0 || nTimerId < 0 || nTimeInterval < 0) 
		return 0;
	
	if (nSubWorldIndex >= 0) 
	{
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			KTimerTaskFun * pTimer = pMission->m_cTimerTaskSet.Add();
			if (pTimer)
			{
				pTimer->SetTimer(nTimeInterval, nTimerId);
			}
		}
		
	}
	return 0;
}
//SetTempRev(worldid, x, y)
int LuaSetDeathRevivalPos(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	
	if (nPlayerIndex <= 0) 
		return 0;
	int nParamCount = Lua_GetTopIndex(L);
	
	PLAYER_REVIVAL_POS * pTempRev = Player[nPlayerIndex].GetDeathRevivalPos();
	
	if (nParamCount > 2)
	{
		pTempRev->m_nSubWorldID  = (int) Lua_ValueToNumber(L, 1);
		pTempRev->m_nMpsX = (int) Lua_ValueToNumber(L, 2);
		pTempRev->m_nMpsY = (int) Lua_ValueToNumber(L, 3); 
	}
	else if (nParamCount == 1)
	{
		pTempRev->m_nSubWorldID = SubWorld[Npc[Player[nPlayerIndex].m_nIndex].m_SubWorldIndex].m_SubWorldID;
		POINT Pos;
		int nRevId = (int) Lua_ValueToNumber(L, 1);
		g_SubWorldSet.GetRevivalPosFromId(pTempRev->m_nSubWorldID, nRevId, &Pos);
		pTempRev->m_ReviveID = nRevId;
		pTempRev->m_nMpsX = Pos.x;
		pTempRev->m_nMpsY = Pos.y;
	}
	else 
	{
		return 0;
	}
	
	return 0;
}

//AddMSPlayer(MissionId, PlayerIndex, groupid); / AddMSPlayer(MissionId, groupid)
int LuaAddMissionPlayer(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	if (nParamCount < 2) 
		return 0;
	int nMissionId = 0;
	int nPlayerIndex = 0;
	int nGroupId = 0;
	if (nParamCount >=3)
	{
		nMissionId = (int)Lua_ValueToNumber(L,1);
		nPlayerIndex = (int )Lua_ValueToNumber(L,2);
		nGroupId = (int) Lua_ValueToNumber(L,3);
	}
	else
	{
		nMissionId = (int)Lua_ValueToNumber(L,1);
		nGroupId = (int) Lua_ValueToNumber(L,2);
		nPlayerIndex = GetPlayerIndex(L);
	}
	
	if (nMissionId < 0 || nPlayerIndex <= 0 || nGroupId <0)
		return 0;
	
	int nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			pMission->AddPlayer(nPlayerIndex, Player[nPlayerIndex].m_dwID, nGroupId);
		}
	}
	return 0;
}

int LuaRevivalAllNpc(Lua_State * L)
{
	int nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		SubWorld[nSubWorldIndex].RevivalAllNpc();
	}
	return 0;
}

//RemoveMSPlayer(MissionId, PlayerIndex, groupid)
int LuaRemoveMissionPlayer(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	if (nParamCount < 2) 
		return 0;
	int nMissionId = 0;
	int nPlayerIndex = 0;
	int nGroupId = 0;
	if (nParamCount >=3)
	{
		nMissionId = (int)Lua_ValueToNumber(L,1);
		nPlayerIndex = (int )Lua_ValueToNumber(L,2);
		nGroupId = (int) Lua_ValueToNumber(L,3);
	}
	else
	{
		nMissionId = (int)Lua_ValueToNumber(L,1);
		nGroupId = (int) Lua_ValueToNumber(L,2);
		nPlayerIndex = GetPlayerIndex(L);
	}
	
	if (nMissionId < 0 || nPlayerIndex <= 0 || nGroupId <0)
		return 0;
	
	int nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			pMission->RemovePlayer(nPlayerIndex);
			//pMission->AddPlayer(nPlayerIndex, Player[nPlayerIndex].m_dwID, nGroupId);
		}
	}
	return 0;
}
//GetNextPlayer(mission, idx,group)
int LuaGetNextPlayer(Lua_State * L)
{
	unsigned long nPlayerIndex = 0;
	
	if (Lua_GetTopIndex(L) < 2)
	{
		Lua_PushNumber(L ,0);
		Lua_PushNumber(L, 0);
		return 2;
	}
	
	int nMissionId	= (int)Lua_ValueToNumber(L, 1);
	int nIdx		= (int)Lua_ValueToNumber(L, 2);
	int nGroup		= (int)Lua_ValueToNumber(L, 3);
	int nSubWorldIndex = GetSubWorldIndex(L);
	int nResultIdx = 0;
	
	if (nMissionId < 0 || nIdx < 0 || nGroup < 0) 
		goto lab_getnextplayer;
	
	if (nSubWorldIndex >= 0) 
	{
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			nResultIdx = pMission->GetNextPlayer(nIdx, nGroup, nPlayerIndex);
		}
	}
	
lab_getnextplayer:	
	Lua_PushNumber(L, nResultIdx);
	Lua_PushNumber(L, nPlayerIndex);
	return 2;
}

//MSMsg2Group(missionid, string , group)
int LuaMissionMsg2Group(Lua_State * L)
{
	int nMissionId = (int)Lua_ValueToNumber(L,1);
	char * strMsg = (char *)Lua_ValueToString(L, 2);
	int	nGroupId = (int) Lua_ValueToNumber(L, 3);
	
	if (nMissionId < 0 || !strMsg || nGroupId <0)
		return 0;
	
	int nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			pMission->Msg2Group(strMsg, nGroupId);
		}
	}
	
	return 0;
}

//MSMsg2Group(missionid, string)
int LuaMissionMsg2All(Lua_State * L)
{
	int nMissionId = (int)Lua_ValueToNumber(L,1);
	char * strMsg = (char *)Lua_ValueToString(L, 2);
	
	if (nMissionId < 0 || !strMsg)
		return 0;
	
	int nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			pMission->Msg2All(strMsg);
		}
	}
	
	return 0;
}

//MSMsg2Group(missionid, string , group)
int LuaMissionMsg2Player(Lua_State * L)
{
	int nMissionId = (int)Lua_ValueToNumber(L,1);
	char * strMsg = (char *)Lua_ValueToString(L, 2);
	int	nPlayerIndex = (int) Lua_ValueToNumber(L, 3);
	
	if (nMissionId < 0 || !strMsg || nPlayerIndex <0)
		return 0;
	
	int nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			pMission->Msg2Group(strMsg, nPlayerIndex);
		}
	}
	
	return 0;
}

int LuaMissionPlayerCount(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	unsigned long ulCount = 0;
	int nMissionId = 0;
	int nGroupId = 0;
	int nSubWorldIndex = 0;
	if (nParamCount < 1) 
		goto lab_getmissionplayercount;
	
	if (nParamCount >= 2)
	{
		nMissionId = (int)Lua_ValueToNumber(L,1);
		nGroupId = (int) Lua_ValueToNumber(L,2);
	}
	else
	{
		nMissionId = (int)Lua_ValueToNumber(L,1);
	}
	
	if (nMissionId < 0 || nGroupId <0)
		goto lab_getmissionplayercount;
	
	nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			ulCount = pMission->GetGroupPlayerCount(nGroupId);
		}
	}
	
lab_getmissionplayercount:
	Lua_PushNumber(L, ulCount);
	return 1;
}

int LuaSetPlayerDeathScript(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) 
		return 0;
	char * szScript = (char *)Lua_ValueToString(L, 1);
	Player[nPlayerIndex].m_dwDeathScriptId = g_FileName2Id(szScript);
	return 0;
}

int LuaNpcIndexToPlayerIndex(Lua_State * L)
{
	int nResult = 0;
	int nNpcIndex = (int)Lua_ValueToNumber(L, 1);
	if (nNpcIndex <=  0 || nNpcIndex >= MAX_NPC)
		goto lab_npcindextoplayerindex;
	
	if (Npc[nNpcIndex].m_Index > 0 && Npc[nNpcIndex].IsPlayer())
	{
		if (Npc[nNpcIndex].GetPlayerIdx() > 0)
			nResult = Npc[nNpcIndex].GetPlayerIdx();
	}
	
lab_npcindextoplayerindex:
	Lua_PushNumber(L, nResult);
	return 1;
	
}
//  
int LuaGetMissionPlayer_PlayerIndex(Lua_State * L)
{
	unsigned long nResult = 0;
	int nSubWorldIndex = 0;
	if (Lua_GetTopIndex(L) < 2) 
		goto lab_getmissionplayer_npcindex;
	
	nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		int nMissionId = (int)Lua_ValueToNumber(L, 1);
		int nDataIndex = (int)Lua_ValueToNumber(L, 2);
		if (nMissionId < 0 || nDataIndex < 0)
			goto lab_getmissionplayer_npcindex;
		
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			nResult = pMission->GetMissionPlayer_PlayerIndex(nDataIndex);
		}
	}
	
lab_getmissionplayer_npcindex:
	Lua_PushNumber(L,nResult);
	return 1;
}

int LuaGetMissionPlayer_DataIndex(Lua_State * L)
{
	unsigned long nResult = 0;
	int nSubWorldIndex = 0;
	if (Lua_GetTopIndex(L) < 2) 
		goto lab_getmissionplayer_dataindex;
	
	nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		int nMissionId = (int)Lua_ValueToNumber(L, 1);
		int nPlayerIndex = (int)Lua_ValueToNumber(L, 2);
		if (nMissionId < 0 || nPlayerIndex < 0)
			goto lab_getmissionplayer_dataindex;
		
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			nResult = pMission->GetMissionPlayer_DataIndex(nPlayerIndex);
		}
	}
	
lab_getmissionplayer_dataindex:
	Lua_PushNumber(L,nResult);
	return 1;
}

//SetMPParam(missionid, nDidx, vid, v)
int LuaSetMissionPlayerParam(Lua_State * L)
{
	int nSubWorldIndex = 0;
	if (Lua_GetTopIndex(L) < 4) 
		return 0;
	
	nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		int nMissionId = (int)Lua_ValueToNumber(L, 1);
		int nDataIndex = (int)Lua_ValueToNumber(L, 2);
		int nParamId =	 (int)Lua_ValueToNumber(L ,3);
		int nValue =	 (int )Lua_ValueToNumber(L, 4);
		
		if (nMissionId < 0 || nDataIndex < 0 || nParamId > 2)
			return 0;
		
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			if (nParamId == 1)
				pMission->m_MissionPlayer.SetParam1(nDataIndex, nValue);
			else
				pMission->m_MissionPlayer.SetParam2(nDataIndex, nValue);
		}
	}
	return 0;
}

int LuaGetMissionPlayerParam(Lua_State * L)
{
	int nResult = 0;
	int nSubWorldIndex = 0;
	if (Lua_GetTopIndex(L) < 3) 
		goto lab_getmissionplayerparam;
	
	nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		int nMissionId = (int)Lua_ValueToNumber(L, 1);
		int nDataIndex = (int)Lua_ValueToNumber(L, 2);
		int nParamId =	 (int)Lua_ValueToNumber(L ,3);
		
		if (nMissionId < 0 || nDataIndex < 0 || nParamId > 2)
			goto lab_getmissionplayerparam;
		
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			if (nParamId == 1)
				nResult = pMission->m_MissionPlayer.GetParam1(nDataIndex);
			else
				nResult = pMission->m_MissionPlayer.GetParam2(nDataIndex);
		}
	}
lab_getmissionplayerparam:
	Lua_PushNumber(L, nResult);
	return 1;
}

int LuaGetPlayerMissionGroup(Lua_State * L)
{
	int nResult = 0;
	int nSubWorldIndex = 0;
	if (Lua_GetTopIndex(L) < 2) 
		goto lab_getmissionplayergroup;
	
	nSubWorldIndex = GetSubWorldIndex(L);
	if (nSubWorldIndex >= 0) 
	{
		int nMissionId = (int)Lua_ValueToNumber(L, 1);
		int nNpcIndex = (int)Lua_ValueToNumber(L, 2);
		
		if (nMissionId < 0 || nNpcIndex < 0)
			goto lab_getmissionplayergroup;
		
		KMission Mission;
		Mission.SetMissionId(nMissionId);
		KMission * pMission = SubWorld[nSubWorldIndex].m_MissionArray.GetData(&Mission);
		if (pMission)
		{
			nResult = pMission->GetMissionPlayer_GroupId(nNpcIndex);
		}			
	}
lab_getmissionplayergroup:
	Lua_PushNumber(L ,nResult);
	return 1;
	
}

int LuaSetPlayerRevivalOptionWhenLogout(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0) return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) 
		return 0;
	int nType = (int)Lua_ValueToNumber(L, 1);
	
	if (nType)
		Player[nPlayerIndex].SetLoginType(1);
	else
		Player[nPlayerIndex].SetLoginType(0);
	
	return 0;
}



int LuaSetPlayerPKValue(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;	
	int nPKValue = (int)Lua_ValueToNumber(L,1);
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)
		return 0;
	if (Player[nPlayerIndex].m_nIndex <= 0) 
		return 0;
	Player[nPlayerIndex].m_cPK.SetPKValue(nPKValue);
	return 0;
}

int LuaGetPlayerPKValue(Lua_State * L)
{
	int nPKValue = 0;
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)
		goto lab_getplayerpkvalue;
	
	if (Player[nPlayerIndex].m_nIndex <= 0) 
		goto lab_getplayerpkvalue;
	nPKValue = Player[nPlayerIndex].m_cPK.GetPKValue();
	
lab_getplayerpkvalue:
	Lua_PushNumber(L, nPKValue);
	return 1;
}


int	LuaGetCurNpcIndex(Lua_State * L)
{
	int nNpcIndex = 0;
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)
		goto lab_getcurnpcindex;
	
lab_getcurnpcindex:
	Lua_PushNumber(L, Player[nPlayerIndex].m_nIndex);
	return 1;
}




//showladder(count, ladderid1, ladderid2.....)
int LuaShowLadder(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	if (nParamCount < 2) 
		return 0;
	int nLadderCount = (DWORD) Lua_ValueToNumber(L, 1);
	if (nLadderCount <= 0)
		return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)
		return 0;
	
	if (nLadderCount > nParamCount - 1 )
		nLadderCount = nParamCount - 1;
	BYTE Buffer[sizeof(LADDER_LIST) + 50 * sizeof(DWORD)];
	LADDER_LIST * pLadderList = (LADDER_LIST*)&Buffer;
	pLadderList->ProtocolType = s2c_ladderlist;
	pLadderList->nCount = nLadderCount;
	pLadderList->wSize = sizeof(LADDER_LIST) + nLadderCount * sizeof(DWORD) - 1;
	for (int i = 0; i < nLadderCount; i ++)
	{
		pLadderList->dwLadderID[i] = (DWORD)Lua_ValueToNumber(L, i + 2);
	}
	g_pServer->PackDataToClient(Player[nPlayerIndex].m_nNetConnectIdx, &Buffer, pLadderList->wSize + 1);
	return 0;
	
}

int LuaSwearBrother(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	int nTeamId = Lua_ValueToNumber(L, 1);
	
	if (nTeamId >= MAX_TEAM || nTeamId < 0)
		return 0;
	
	KPlayerChat::STRINGLIST BrotherList;
	_ASSERT(g_Team[nTeamId].m_nCaptain > 0);

	std::string strCapName;
	strCapName = Npc[Player[g_Team[nTeamId].m_nCaptain].m_nIndex].Name;
	BrotherList.push_back(strCapName);

	for (int i  = 0; i < g_Team[nTeamId].m_nMemNum; i++)
	{
		int nPlayerIndex = g_Team[nTeamId].m_nMember[i];
		if ( nPlayerIndex > 0 && nPlayerIndex < MAX_PLAYER)
		{
			std::string strName;
			strName = 	Npc[Player[nPlayerIndex].m_nIndex].Name;
			BrotherList.push_back(strName);
		}
	}
	
	KPlayerChat::MakeBrother(BrotherList);
	return 0;
}

int LuaMakeEnemy(Lua_State * L)
{
	
	return 0;
}


int LuaAddLeadExp(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1)
		return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)
		return 0;
	
	int nLeadExp = (int)Lua_ValueToNumber(L, 1);
	Player[nPlayerIndex].AddLeadExp(nLeadExp);
	return 0;
}

int LuaGetLeadLevel(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	int nLeadLevel = 0;
	if (nPlayerIndex <= 0)
		goto lab_getleadlevel;
	nLeadLevel = (int)Player[nPlayerIndex].m_dwLeadLevel;
	
lab_getleadlevel:
	Lua_PushNumber(L, nLeadLevel);
	return 1;
}

int LuaAddMagicPoint(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1)
		return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)
		return 0;
	int nSkillPoint = (int)Lua_ValueToNumber(L, 1);
	
	Player[nPlayerIndex].m_nSkillPoint += nSkillPoint;
	if (Player[nPlayerIndex].m_nSkillPoint < 0)
		Player[nPlayerIndex].m_nSkillPoint = 0;
	
	return 0;
}

int LuaGetMagicPoint(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	int nSkillPoint = 0;
	if (nPlayerIndex <= 0)
		goto lab_getmagicpoint;
	
lab_getmagicpoint:
	Lua_PushNumber(L, Player[nPlayerIndex].m_nSkillPoint);
	return 1;
}

int LuaAddPropPoint(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	int nPropPoint = 0;
	if (nPlayerIndex <= 0)
		return 0;
	nPropPoint = (int)Lua_ValueToNumber(L, 1);
	Player[nPlayerIndex].m_nAttributePoint += nPropPoint;
	return 0;
}


//PayExtPoint
int LuaPayExtPoint(Lua_State * L)
{
	int nResult = 0;
	int nPay = 0;
	int nPlayerIndex = 0;
	if (Lua_GetTopIndex(L) < 1)
		goto lab_payextpoint;
	
	nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)
		goto lab_payextpoint;
	nPay = Lua_ValueToNumber(L, 1);
	if (nPay < 0)
		goto lab_payextpoint;
	nResult = Player[nPlayerIndex].PayExtPoint(nPay);

lab_payextpoint:
	Lua_PushNumber(L, nResult);
	return 1;
}

//PayExtPoint
int LuaGetExtPoint(Lua_State * L)
{
	int nResult = 0;
	int nPlayerIndex = 0;
	if (Lua_GetTopIndex(L) < 1)
		goto lab_getextpoint;
	
	nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)
		goto lab_getextpoint;
	
	nResult = Player[nPlayerIndex].GetExtPoint();
	
lab_getextpoint:
	Lua_PushNumber(L, nResult);
	return 1;
}

int LuaGetRestPropPoint(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	int nPropPoint = 0;
	if (nPlayerIndex <= 0)
		goto lab_getrestproppoint;
	nPropPoint = Player[nPlayerIndex].m_nAttributePoint;
	
lab_getrestproppoint:
	Lua_PushNumber(L, nPropPoint);
	return 1;
}

//Msg2GM(str, id)
int LuaMsgToGameMaster(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	if ( nParamCount < 1 ) 
		return 0;
	int nParamID = 0;
	const char * szMsg = Lua_ValueToString(L, 1) ;
	if (!szMsg) 
		return 0;

	if (nParamCount < 2)
	{
		nParamID = 0;
	}
	else
	{
		nParamID = (int) Lua_ValueToNumber(L, 2);
	}

	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex > 0) 
	{
		char szID[32];

		sprintf(szID, "%d", nParamID);
		//KPlayerChat::SendInfoToGM(Player[nPlayerIndex].m_AccoutName, Npc[Player[nPlayerIndex].m_nIndex].Name, (char *) szMsg, strlen(szMsg) );
		KPlayerChat::SendInfoToGM("GM", szID, (char *) szMsg, strlen(szMsg) );
	}
	return 0;
}

//Msg2IP(IP, ID, str)
int LuaMsgToIP(Lua_State * L)
{
	int nParamCount = Lua_GetTopIndex(L);
	if ( nParamCount < 3 ) 
		return 0;
	int nIP = 0;
	const char * szIP = Lua_ValueToString(L, 1) ;
	nIP = _a2ip(szIP);
	if (nIP == 0)
		return 0;

	int nID = (int) Lua_ValueToNumber(L, 2);
	const char * szMsg = Lua_ValueToString(L, 3) ;
	if (!szMsg) 
		return 0;

	int nParamID = 0;
	if (nParamCount < 4)
	{
		nParamID = 0;
	}
	else
	{
		nParamID = (int) Lua_ValueToNumber(L, 4);
	}
	char szID[32];
	sprintf(szID, "%d", nParamID);
	KPlayerChat::SendInfoToIP(nIP, nID, "GM", szID, (char *) szMsg, strlen(szMsg) );
	return 0;
}

int LuaGetPlayerInfo(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	char szDesMsg[250];
	char szMsg[800];
	int nNpcIdx = 0;
	KNpc * pNpc = NULL;
	KPlayer* pPlayer = NULL;
	szDesMsg[0] = 0;
	if (nPlayerIndex <= 0)
		goto lab_getplayerinfo;
	
	pPlayer = &Player[nPlayerIndex];
	nNpcIdx = pPlayer->m_nIndex;
	pNpc = &Npc[nNpcIdx];
	sprintf(szMsg, "Name:%s,Lvl:%d,IP:%s,Lf:%d|%d,Mn:%d|%d,Mny:%d,sp:%d,ap:%d", pNpc->Name, pNpc->m_Level, g_pServer->GetClientInfo(pPlayer->m_nNetConnectIdx), pNpc->m_CurrentLife, pNpc->m_CurrentLifeMax, pNpc->m_CurrentMana, pNpc->m_CurrentManaMax, pPlayer->m_ItemList.GetMoney(room_equipment), pPlayer->m_nSkillPoint, pPlayer->m_nAttributePoint );
	g_StrCpyLen(szDesMsg, szMsg, 250);
	
lab_getplayerinfo:
	Lua_PushString(L, szDesMsg);
	return 1;
}

int LuaGetIP(Lua_State * L)
{
	int nPlayerIndex = GetPlayerIndex(L);
	char szDesMsg[200];
	szDesMsg[0] = 0;
	KPlayer * pPlayer = NULL;
	if (nPlayerIndex <= 0)
		goto lab_getplayerip;
	pPlayer = &Player[nPlayerIndex];
	strcpy(szDesMsg, g_pServer->GetClientInfo(pPlayer->m_nNetConnectIdx));
	
lab_getplayerip:
	Lua_PushString(L, szDesMsg);
	return 1;
}

int LuaSetDeathPunish(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;

	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex <= 0)
	{
		int nState = Lua_ValueToNumber(L, 1);
		if (nState == 0)
			Npc[Player[nPlayerIndex].m_nIndex].m_nCurPKPunishState = 0;
		else
			Npc[Player[nPlayerIndex].m_nIndex].m_nCurPKPunishState = enumDEATH_MODE_PKBATTLE_PUNISH;
	}
	return 0;
}



int LuaHideNpc(Lua_State * L)
{
	if (Lua_GetTopIndex(L) < 2)
		return 0;
	
	int nNpcIndex  = 0;
	
	if (Lua_IsNumber(L,1))
	{
		nNpcIndex = (int)Lua_ValueToNumber(L, 1);
	}
	else 
	{
		const char * szName = Lua_ValueToString(L, 1);
		int nSubWorldIndex = GetSubWorldIndex(L);
		if (nSubWorldIndex < 0)
			return 0;
		
		nNpcIndex = SubWorld[nSubWorldIndex].FindNpcFromName(szName);
	}
	
	if (nNpcIndex > 0 || nNpcIndex < MAX_NPC)
	{
		int nFrame = Lua_ValueToNumber(L, 2);
		if (nFrame <= 0)
			nFrame = 1;
		
		Npc[nNpcIndex].ExecuteRevive();
		Npc[nNpcIndex].m_Frames.nTotalFrame = nFrame;
		Npc[nNpcIndex].m_Frames.nCurrentFrame = 0;
	}
	
	return 0;
}

#endif

int LuaPlayMusic(Lua_State * L)//PlayMusic(musicname,loop=1, vol );
{
	if (Lua_GetTopIndex(L) < 1) 
		return 0;
	
	int nPlayerIndex = GetPlayerIndex(L);
	if (nPlayerIndex < 0) return 0;
	
	PLAYER_SCRIPTACTION_SYNC UiInfo;
	ZeroMemory(&UiInfo, sizeof(UiInfo));
	UiInfo.m_bUIId = UI_PLAYMUSIC;
	UiInfo.m_bOptionNum = 1;
	UiInfo.m_nOperateType = SCRIPTACTION_UISHOW;
	
	int nMsgId = 0;
	
	g_StrCpyLen(UiInfo.m_pContent, Lua_ValueToString(L,1), sizeof(UiInfo.m_pContent));
	UiInfo.m_nBufferLen = strlen(((char *)UiInfo.m_pContent));
	UiInfo.m_bParam1 = 0;
	
#ifndef _SERVER
	UiInfo.m_bParam2 = 0;
#else
	UiInfo.m_bParam2 = 1;
#endif
	
	Player[nPlayerIndex].DoScriptAction(&UiInfo);
	return 0;
}

int LuaFadeInMusic(Lua_State * L)
{
	return 0;
}

int LuaFadeOutMusic(Lua_State * L)
{
	return 0;
}




#ifndef _SERVER

int LuaPlaySound(Lua_State * L)
{
	
	return 0;
}

int LuaPlaySprMovie(Lua_State * L)
{
	return 0;
}
#endif
#ifdef _SERVER
int LuaEnchaseItem(Lua_State* L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	if (nPlayer > 0 && nPlayer < MAX_PLAYER)
		Player[nPlayer].OpenItemCompound(FALSE, nTop > 0 && lua_type(L, 1) == LUA_TSTRING ? Lua_ValueToString(L, 1) : "OnCompound");
	return 0;
}

int LuaOpenNewCompound(Lua_State* L)
{
	int nTop = Lua_GetTopIndex(L);
	int nPlayer = GetPlayerIndex(L);
	Lua_SetTopIndex(L, nTop);
	if (nPlayer > 0 && nPlayer < MAX_PLAYER)
		Player[nPlayer].OpenItemCompound(TRUE, nTop > 0 && lua_type(L, 1) == LUA_TSTRING ? Lua_ValueToString(L, 1) : "OnCompound");
	return 0;
}
#endif

TLua_Funcs GameScriptFuns[] = 
{
	//通用指令
	
	{"Say", LuaSelectUI},
	{"SayTask", LuaSayTask},
	{"TaskNote", LuaTaskNote},
	{"MsgBox", LuaMsgBox},
	{"HaveNormalItem", LuaHaveNormalItem},
#ifdef _SERVER
	{"EnchaseItem", LuaEnchaseItem},
	{"OpenNewCompound", LuaOpenNewCompound},
	{"ReadTabFile", LuaReadTabFile},
	{"GetItemInfo", LuaGetItemInfo},
	{"RepairItem", LuaRepairItem},
	{"GetPetFuncActive", LuaGetPetFuncActive},
	{"GetPetSettings", LuaGetPetSettings},
	{"GetGameVersion", LuaGetGameVersion},
	{"Roulette", LuaRoulette},
	{"GetMorphType", LuaGetMorphType},
	{"PolyMorph", LuaPolyMorph},
	{"GetLastSkillIdx", LuaGetLastSkillIdx},
	{"IsSkillActivedByIdx", LuaIsSkillActivedByIdx},
	{"GetSelectedItems", LuaGetSelectedItems},
	{"DelSelectedItem", LuaDelSelectedItem},
	{"CompleteItemOperation", LuaCompleteItemOperation},
	{"HaveItem2", LuaHaveNormalItem},
	{"DelItem2", LuaDelNormalItem},
	{"GetItemLevel2", LuaGetItemLevel2},
	{"IsExistItem", LuaHaveNormalItem},
	{"AddNormalItem2", LuaAddNormalItem},
	{"AddNormalItemPile", LuaAddNormalItem},
	{"GetCredit", LuaGetRepute},
	{"DecCredit", LuaDecCredit},
	{"GetCopperCashCount", LuaGetCopperCashCount},
	{"WasteCopperCash", LuaWasteCopperCash},
	{"ImproveItemMagicAttr", LuaImproveItemMagicAttr},
#endif
	{"DelNormalItem", LuaDelNormalItem},
	{"Talk", LuaTalkUI},
	{"GetTaskTemp", LuaGetTempTaskValue},
	{"SetTaskTemp", LuaSetTempTaskValue},
	{"Message", LuaMessage},
	
	{"GetBit",	LuaGetBit},
	{"GetByte",	LuaGetByte},
	{"SetBit",	LuaSetBit},
	{"SetByte",	LuaSetByte},
	{"Include",LuaIncludeFile},
	{"PutMessage", LuaSendMessageInfo},
	{"AddGlobalNews",LuaAddGlobalNews},
	{"AddGlobalTimeNews",LuaAddGlobalTimeNews},
	{"AddGlobalCountNews",LuaAddGlobalCountNews	},
	{"AddLocalNews",LuaAddLocalNews},
	{"AddLocalTimeNews",LuaAddLocalTimeNews},
	{"AddLocalCountNews",LuaAddLocalCountNews	},
	
	//服务器端脚本指令
#ifdef _SERVER
	//-----------------基本功能
	{"GetTaskByte", LuaGetTaskByte},
	{"SetTaskByte", LuaSetTaskByte},
	{"AddNormalItem", LuaAddNormalItem},
	{"GetPlayerType", LuaGetPlayerType},
	{"IsSkillActived", LuaIsSkillActived},
	{"IsExistBuff", LuaIsExistBuff},
	{"AddBuff", LuaAddBuff},
	{"RemoveBuff", LuaRemoveBuff},
	{"HaveIBBuff", LuaIsExistBuff},
	{"GetIBBuffLeftTimes", LuaGetIBBuffLeftTimes},
	{"AddIBBuff", LuaAddIBBuff},
	{"HaveNormalItemInQuick", LuaHaveNormalItemInQuick},
	{"DelNormalItemInQuick", LuaDelNormalItemInQuick},
	{"SystemTime", LuaSystemTime},
	{"GetHMS", LuaGetHMS},
	{"GetYMD", LuaGetYMD},
	{"WriteLog", LuaWriteLog},
	{"Time2LocalYMD", LuaTime2LocalYMD},
	{"HaveEventItem", LuaGetTaskItemCount},
	{"DelEventItem", LuaDelItem},
	{"AddRepute", LuaModifyRepute},	//修改声望值
	{"GetRepute",LuaGetRepute},		
	{"GetNpcIdx", LuaGetCurNpcIndex},
	
	{"SetTimer",		LuaSetTimer},		//SetTimer(时间量, 时间TaskId):给玩家打开计时器,时间到时将自动调用OnTimer函数
	{"StopTimer",		LuaStopTimer},		//StopTimer()：关闭当前玩家的计时器
	{"GetRestTime",		LuaGetRestTime},	//GetRestTime:获得计时器将触发的剩于时间	
	{"GetTimerId",		LuaGetCurTimerId},	//CurTimerId = GetTimerId():获得当前执行的计时器的id,如果没有则返回0
	{"GetTask",			LuaGetTaskValue},	//GetTask(任务号):获得当前玩家该任务号的值
	{"SetTask",			LuaSetTaskValue},	//SetTask(任务号,值):设置任务值
	{"IsCaptain",		LuaIsLeader},		//IsCaptain()是否为队长
	{"GetTeam",			LuaGetTeamId},		//GetTeam()返回玩家队伍ID
	{"GetTeamSize",		LuaGetTeamSize},	//GetTeamSize()
	{"LeaveTeam",		LuaLeaveTeam},		//LeaveTeam()让玩家离开自身队伍
	{"TopMessage",		LuaMsgToPlayer	},
	{"Msg2Player",		LuaMsgToPlayer	},	//Msg2Player(消息)
	{"Msg2Team",		LuaMsgToTeam},		//Msg2Team(消息)通知玩家的组
	{"Msg2SubWorld",	LuaMsgToSubWorld},	//Msg2SubWorld(消息)通知世界
	{"Msg2Region",		LuaMsgToAroundRegion},//Msg2Region(消息)通知周围Region
	{"Msg2GM",			LuaMsgToGameMaster}, //Msg2GM(StrInfo)
	{"Msg2IP",			LuaMsgToIP}, //Msg2IP(IP, ID, StrInfo)
	{"GetInfo",			LuaGetPlayerInfo}, //str = GetInfo()
	{"GetIP",			LuaGetIP},
	{"SetPos",			LuaSetPos},			//SetPos(x,y)进入某点	
	{"GetPos",			LuaGetPos},			//GetPos() return x,y,subworldindex
	{"GetWorldPos",		LuaGetNewWorldPos},	//W,X,Y = GetWorldPos()返回于NewWorld配陪的坐标
	{"NewWorld",		LuaEnterNewWorld},
	{"GetUseItemInfo", LuaGetUseItemInfo},
	{"ConsumeUseItem", LuaConsumeUseItem},
	{"TeleportWithItem", LuaTeleportWithItem},
	{"CloseDialog",		LuaCloseDialog},
	{"DropItem",		LuaDropItem},		//DropItem(NpcId, 物品id或物品名)
	{"AddItem",			LuaAddItem},		//AddItem(nItemClass, nDetailType, nParticualrType, nLevel, nSeries, nLuck, nItemLevel..6)
	{"AddEventItem",	LuaAddEventItem	},	//AddEventItem(事件物品ID)
	{"DelItem",			LuaDelItem},		//
	{"HaveItem",		LuaHaveItem},
	{"GetItemCount",	LuaGetTaskItemCount}, //GetItemCount(TaskItemName Or TaskItemId)
	{"AddMagic",		LuaAddMagic},		//AddMagic(魔法id或魔法名)给玩家加某个魔法
	{"DelMagic",		LuaDelMagic},		//DelMagic(魔法id或魔法名)
	{"HaveMagic",		LuaHaveMagic},		//HaveMagic(魔法id或魔法名)返回0或1
	{"GetMagicLevel",	LuaGetMagicLevel},	//GetMagicLevel(魔法id或魔法名)返回等级
	{"AddMagicPoint",	LuaAddMagicPoint},
	{"GetMagicPoint",	LuaGetMagicPoint},
	
	{"SubWorldID2Idx",	LuaSubWorldIDToIndex}, //SubWorldID2Idx
	
	
	{"AddLeadExp",		LuaAddLeadExp},
	{"GetLeadLevel",	LuaGetLeadLevel},
	
	{"SetFightState",	LuaSetFightState}, 
	{"GetFightState",	LuaGetFightState},
	
	{"AddNpc",			LuaAddNpc},			//AddNpc(人物模板id或人物模板名,所处世界id，点坐标x,点坐标y),返回npcid值
	{"DelNpc",			LuaDelNpc},			//DelNpc(Npcid)
	{"SetNpcScript",	LuaSetNpcActionScript},	//SetNpcScript(npcid, 脚本文件名)设置npc当前的脚本
	{"SetRevPos",		LuaSetPlayerRevivalPos},//SetRevPos(点位置X，点位置Y)设置玩家的当前世界的等入点位置
	{"SetTempRevPos",	LuaSetDeathRevivalPos}, //SetTempRevPos(subworldid, x, y ) or SetTempRevPos(id);
	{"GetCurCamp",		LuaGetPlayerCurrentCamp},//GetCurCamp()获得玩家的当前阵营
	{"GetCamp",			LuaGetPlayerCamp	},//GetCamp()获得玩家阵营
	{"SetCurCamp",		LuaSetPlayerCurrentCamp},//SetCurCamp(阵营号):设置玩家当前阵营
	{"SetCamp",			LuaSetPlayerCamp},		  //SetCamp(阵营号):设置阵营	
	{"RestoreCamp",		LuaRestorePlayerCamp	},//RestoreCamp()恢复阵营
	{"GetFaction",		LuaGetPlayerFaction,	},//GetFaction()获得玩家的门派名 
	{"SetFaction",		LuaChangePlayerFaction},  //SetFaction(门派名):设置玩家门派名
	{"GetColdR",		LuaGetPlayerColdResist},	
	{"SetColdR",		LuaGetPlayerColdResist},
	{"GetFireR",		LuaGetPlayerFireResist	},
	{"SetFireR",		LuaSetPlayerFireResist	},
	{"GetLightR",		LuaGetPlayerLightResist	},
	{"SetLightR",		LuaSetPlayerLightResist},
	{"GetPoisonR",		LuaGetPlayerPoisonResist},
	{"SetPoisonR",		LuaSetPlayerPoisonResist},
	{"GetPhyR",			LuaGetPlayerPhysicsResist	},
	{"SetPhyR",			LuaSetPlayerPhysicsResist	},
	{"GetExp",			LuaGetPlayerExp	},			//GetExp():获得玩家的当前经验值
	{"AddExp",			LuaModifyPlayerExp},		//AddExp(经验值，对方等级，是否组队共享经验值)
	{"AddOwnExp",		LuaAddOwnExp	},			//AddOwnExp(Exp)，给玩家直接加经验
	{"GetLife",			LuaGetPlayerLife},			//GetLife()获得玩家的生命值
	{"RestoreLife",		LuaRestorePlayerLife},		//RestoreLife()恢复玩家的生命
	{"GetMana",			LuaGetPlayerMana},			//GetMana()获得玩家的Mana
	{"RestoreMana",		LuaRestorePlayerMana},		//RestoreMana()恢复玩家的Mana
	{"GetStamina",		LuaGetPlayerStamina},		//GetStamina()获得玩家Stamina
	{"RestoreStamina",	LuaRestorePlayerStamina},	//RestoreMana()恢复玩家的Stamina
	{"GetDefend",		LuaGetPlayerDefend},		//GetDefend()获得玩家的防御力
	{"GetSex",			LuaGetPlayerSex},			//GetSex()获得玩家的性别
	{"GetSeries",		LuaGetPlayerSeries},		//GetSeries()获得玩家的系0man/1woman
	{"SetSeries",		LuaSetPlayerSeries},		//SetSeries(性别号)
	{"GetName",			LuaGetPlayerName},			//GetName()获得玩家的姓名
	{"GetUUID",			LuaGetPlayerID},			//GetUUID()获得玩家的唯一ID
	{"GetLeadExp",		LuaGetPlayerLeadExp},		//GetLeadExp()获得玩家的统率经验值
	{"GetLeadLevel",	LuaGetPlayerLeadLevel},		//GetLeadLevel()获得玩家的统率等级
	{"GetLevel",		LuaGetLevel},				//GetLevel()GetPlayers Level
	{"GetRestAP",		LuaGetPlayerRestAttributePoint},//GetRestAP()获得玩家的剩于属性点数
	{"GetRestSP",		LuaGetPlayerRestSkillPoint},	//GetRestSP()获得玩家的剩于技能点数	
	{"GetLucky",		LuaGetPlayerLucky},			//GetLucky()获得玩家的幸运值
	{"GetEng",			LuaGetPlayerEngergy},		//GetEng()获得玩家的力量值Eng
	{"GetDex",			LuaGetPlayerDexterity},		//GetDex()获得玩家的Dex
	{"GetStrg",			LuaGetPlayerStrength},		//GetStrg()
	{"GetVit",			LuaGetPlayerVitality},		//GetVit()
	{"GetCash",			LuaGetPlayerCashMoney},		//GetCash()获得玩家的现金
	{"Pay",				LuaPlayerPayMoney},			//Pay(金额数)扣除玩家金钱成功返回1，失败返回0
	{"Earn",			LuaPlayerEarnMoney},		//Earn(金额数)增加玩家金钱
	{"PrePay",			LuaPlayerPrePayMoney},		//付定金，成功返回1，失败返回0
	{"ExeScript",		LuaPlayerExecuteScript},	//ExeScript(脚本文件名,参数),执行时会调用main函数	
	{"AttackNpc",		LuaAttackNpc},				//AttackNpc(NpcDwid,物理伤害值，冰，火，电，毒）
	{"KillNpc",			LuaKillNpc},				//KillNpc(NpcDWID)
	{"KillPlayer",		LuaKillPlayer},				//KillPlayer();
	{"SaleEx", LuaSaleEx},
	{"GetPetType", LuaGetPetType},
	{"GetPetHonor", LuaGetPetHonor},
	{"GetPetColor", LuaGetPetColor},
	{"GetPetName", LuaGetPetName},
	{"IsPetReleased", LuaIsPetReleased},
	{"GenPet", LuaGenPet},
	{"DelPet", LuaDelPet},
	{"SetPetType", LuaSetPetType},
	{"SetPetHonor", LuaSetPetHonor},
	{"ReleasePet", LuaReleasePet},
	{"GetPetFuncAuthority", LuaGetPetFuncAuthority},
	{"SetPetFuncAuthority", LuaSetPetFuncAuthority},
	{"Sale",			LuaSale},					//Sale(SaleId)买卖，SaleId为便卖的物品信息列表id
	{"UseTownPortal",	LuaUseTownPortal	},
	{"ReturnFromPortal",LuaReturnFromTownPortal	},
	{"SetNpcCurCamp",	LuaSetNpcCurCamp},
	{"OpenBox",			LuaOpenBox},
	{"AddStation",	LuaAddPlayerStation},
	{"AddTermini",		LuaAddPlayerWayPoint},
	{"GetStation",		LuaGetPlayerStation	},
	{"GetStationCount", LuaGetPlayerStationCount},
	
	{"GetCityCount", LuaGetAllStationCount},
	{"GetCity", LuaGetCity},
	
	{"GetWayPoint",		LuaGetPlayerWayPoint},
	{"GetStationName",	LuaGetStationName},
	{"GetWayPointName", LuaGetWayPointName},
	{"GetPrice2Station", LuaGetPriceToStation},
	{"GetPrice2WayPoint", LuaGetPriceToWayPoint	},
	{"GetStationPos",	LuaGetStationPos},
	{"GetWayPointPos",	LuaGetWayPointPos},
	{"GetPlayerCount",	LuaGetPlayerCount},
	{"GetRank",			LuaGetRank},//GetRank()
	{"SetRank",			LuaSetRank},//SetRank(id)
	{"SetPropState",	LuaSetObjPropState},//SetPropState( hide = 1) hide obj
	{"GetServerName",	LuaGetServerName},
	
	
	//------------------Station Script ---------------
	{"GetWharfName",	LuaGetDockName},
	{"GetWharfCount",	LuaGetDockCount},
	{"GetWharfPrice",	LuaGetDockPrice},
	{"GetWharf",		LuaGetDock},
	{"GetWharfPos",		LuaGetDockPos},
	{"GetTerminiFState", LuaGetWayPointFightState},
	//------------------------------------------------
	{"KickOutPlayer",	LuaKickOutPlayer},
	{"KickOutSelf",		LuaKickOutSelf},
	{"GetSkillId",		LuaGetSkillIdInSkillList},
	{"SetSkillLevel",	LuaSetSkillLevel},
	{"SetChatFlag",		LuaSetPlayerChatForbiddenFlag},
	//------------------------------------------------
	
	{"AddNote", LuaAddNote},
	//-----------------Mission Script-----------------
	{"GetMissionV", LuaGetMissionValue},//GetMissionV(Vid)
	{"SetMissionV", LuaSetMissionValue},//SetMissionV(Vid, Value)
	{"GetGlbMissionV", LuaGetGlobalMissionValue	},
	{"SetGlbMissionV", LuaSetGlobalMissionValue	},
	{"OpenMission", LuaInitMission},//OpenMission(missionid)
	{"RunMission", LuaRunMission},
	{"CloseMission", LuaCloseMission},//CloseMission(missionid)
	{"StartMissionTimer", LuaStartMissionTimer},////StartMissionTimer(missionid, timerid, time)
	{"StopMissionTimer", LuaStopMissionTimer},
	{"GetMSRestTime", LuaGetMissionRestTime}, //GetMSRestTime(missionid, timerid)
	{"GetMSIdxGroup",LuaGetPlayerMissionGroup},//GetPlayerGroup(missionid, playerid);
	
	{"AddMSPlayer", LuaAddMissionPlayer},
	{"DelMSPlayer", LuaRemoveMissionPlayer},
	{"GetNextPlayer", LuaGetNextPlayer},
	{"PIdx2MSDIdx", LuaGetMissionPlayer_DataIndex},//(missionid, pidx)
	{"MSDIdx2PIdx", LuaGetMissionPlayer_PlayerIndex},//(missionid, dataidx)
	{"NpcIdx2PIdx", LuaNpcIndexToPlayerIndex},
	{"GetMSPlayerCount", LuaMissionPlayerCount},//GetMSPlayerCount(missionid, group = 0)

	{"RevivalAllNpc",	LuaRevivalAllNpc},
	
	{"SetPMParam", LuaSetMissionPlayerParam },
	{"GetPMParam", LuaGetMissionPlayerParam},
	{"Msg2MSGroup", LuaMissionMsg2Group},
	{"Msg2MSAll", LuaMissionMsg2All},
	{"Msg2MSPlayer", LuaMissionMsg2Player},
	{"SetDeathScript", LuaSetPlayerDeathScript},
	{"Death", LuaKillNpc},

	{"HideNpc", LuaHideNpc}	,//HideNpc(npcindex/npcname, hidetime)
	{"SetLogoutRV", LuaSetPlayerRevivalOptionWhenLogout},
	{"SetCreateTeam",LuaSetCreateTeamOption},
	{"GetPK", LuaGetPlayerPKValue},  //pkValue = GetPK() 
	{"SetPK", LuaSetPlayerPKValue}, //SetPK(pkValue)
	//------------------------------------------------
	//排名相关函数
	{"ShowLadder", LuaShowLadder}, //ShowLadder(LadderCount, LadderId1,LadderId2,...);
	//------------------------------------------------

	{"OpenTong",	LuaOpenTong},	//OpenTong()通知玩家打开帮会界面
	{"SetPunish",	LuaSetDeathPunish},// SetPunish(0/1) 0表示不受任何惩罚
	//-------------------------------------------------
	//结拜
	{ "SwearBrother", LuaSwearBrother}, // ret = SwearBrother(TeamId);
	{"MakeEnemy",	LuaMakeEnemy}, //结仇 MakeEnemy(enemyname)
	{"RollbackSkill", LuaRollBackSkills},
	{"UpdateSkill", LuaUpdateSkillList},
	//-------------------------------------------------

	{"AddProp",		LuaAddPropPoint},//加玩家属性点
	{"GetProp",		LuaGetRestPropPoint },
	
	{"GetExtPoint",	LuaGetExtPoint},
	{"PayExtPoint",	LuaPayExtPoint},
	
	//{"Trade",			LuaTrade	},				//Trade("maininfo", "IniFileName.ini的路径名")
	//Trade("MainInfo", n, "item1|price1|function1", "item2|price2|function2", ......, "itemn|pricen|functionn")
	
	//客户端脚本指令
#else 
	{"PlaySound", LuaPlaySound}, //PlaySound(Sound);
	{"PlaySprMovie",LuaPlaySprMovie},//PlaySprMovie(npcindex, Movie, times)
#endif
	{"PlayMusic", LuaPlayMusic}, //PlayMusic(Music,Loop)
	{"FadeInMusic",LuaFadeInMusic},
	{"FadeOutMusic",LuaFadeOutMusic},
};


TLua_Funcs WorldScriptFuns[] =// 非指对玩家的脚本指令集
{
	//通用指令

	{"AddLocalNews",LuaAddLocalNews},
	{"AddLoaclTimeNews",LuaAddLocalTimeNews},
	{"AddLocalCountNews",LuaAddLocalCountNews	},
	//服务器端脚本指令
#ifdef _SERVER
	{"Msg2SubWorld",	LuaMsgToSubWorld},	//Msg2SubWorld(消息)通知世界
	{"Msg2IP",			LuaMsgToIP}, //Msg2IP(IP, ID, StrInfo)	
	{"SubWorldID2Idx",	LuaSubWorldIDToIndex}, //SubWorldID2Idx
	{"GetServerName",	LuaGetServerName},
	{"KickOutPlayer",	LuaKickOutPlayer},
	{"KickOutAccount",	LuaKickOutAccount},
#endif

//	{"GetMissionV", LuaGetMissionValue},//GetMissionV(Vid)
//	{"SetMissionV", LuaSetMissionValue},//SetMissionV(Vid, Value)
//	{"GetGlbMissionV", LuaGetGlobalMissionValue	},
//	{"SetGlbMissionV", LuaSetGlobalMissionValue	},
//	{"OpenMission", LuaInitMission},//OpenMission(missionid)
//	{"RunMission", LuaRunMission},
//	{"CloseMission", LuaCloseMission},//CloseMission(missionid)
//	{"StartMissionTimer", LuaStartMissionTimer},////StartMissionTimer(missionid, timerid, time)
//	{"StopMissionTimer", LuaStopMissionTimer},
//	{"GetMSRestTime", LuaGetMissionRestTime}, //GetMSRestTime(missionid, timerid)
//	{"GetMSIdxGroup",LuaGetPlayerMissionGroup},//GetPlayerGroup(missionid, playerid);
//	
//	{"AddMSPlayer", LuaAddMissionPlayer},
//	{"DelMSPlayer", LuaRemoveMissionPlayer},
//	{"GetNextPlayer", LuaGetNextPlayer},
//	{"PIdx2MSDIdx", LuaGetMissionPlayer_DataIndex},//(missionid, pidx)
//	{"MSDIdx2PIdx", LuaGetMissionPlayer_PlayerIndex},//(missionid, dataidx)
//	{"NpcIdx2PIdx", LuaNpcIndexToPlayerIndex},

	//{"GetMSPlayerCount", LuaMissionPlayerCount},//GetMSPlayerCount(missionid, group = 0)

//	{"RevivalAllNpc",	LuaRevivalAllNpc},
	
	//{"SetPMParam", LuaSetMissionPlayerParam },
	//{"GetPMParam", LuaGetMissionPlayerParam},
	//{"Msg2MSGroup", LuaMissionMsg2Group},
	//{"Msg2MSAll", LuaMissionMsg2All},
	//{"Msg2MSPlayer", LuaMissionMsg2Player},


}; 

int g_GetGameScriptFunNum()
{
	return sizeof(GameScriptFuns)  / sizeof(GameScriptFuns[0]);
}

int g_GetWorldScriptFunNum()
{
	return sizeof(WorldScriptFuns)  / sizeof(WorldScriptFuns[0]);
}