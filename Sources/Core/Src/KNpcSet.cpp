#include "KCore.h"
#include <math.h>
#include "KNpc.h"
#include "KSubWorld.h"
#include "KRegion.h"
#include "GameDataDef.h"
#include "KNpcSet.h"
#include "KPlayer.h"
#ifndef _SERVER
#include "CoreShell.h"
#include "Scene\KScenePlaceC.h"
#include "../../Represent/iRepresent/iRepresentshell.h"
#include "KOption.h"
#ifndef TOOLVERSION
#include "../../Headers/IClient.h"
#endif
#endif
#include "Scene/SceneDataDef.h"
#include "KMath.h"

//#define	ENCHANT_SETTING_PATH	"settings\\npc"
//#define	ENCHANT_NORMAL_FILE		"normalunique.txt"
//#define	ENCHANT_SPECIAL_FILE	"speicalunique.txt"

KNpcSet	NpcSet;

KNpcSet::KNpcSet()
{
	m_dwIDCreator = 1000;
}

// 黄金怪物设定文件的初始化没有放在这里，直接放在 core 的初始化里面
void KNpcSet::Init()
{
    // Add by Freeway Chen in 2003.6.29    
    GenRelationTable();

	m_FreeIdx.Init(MAX_NPC);
	m_UseIdx.Init(MAX_NPC);
	int i; //tuannd
	// 开始时所有的数组元素都为空
	for ( i = MAX_NPC - 1; i > 0; i--)
	{
		m_FreeIdx.Insert(i);
		Npc[i].m_Node.m_nIndex = i;
#ifdef _SERVER
		Npc[i].m_cDeathCalcExp.Init(i);
#endif
		Npc[i].m_cGold.Init(i);
	}

	LoadPlayerBaseValue(PLAYER_BASE_VALUE);

#ifdef _SERVER
	KNpcGold::LoadHardNpcConfig();
	KIniFile	cPKIni;
//	g_SetFilePath("\\");
	if (cPKIni.Load(PLAYER_PK_RATE_FILE))
	{
		cPKIni.GetInteger("PK", "rate", 20, &m_nPKDamageRate);
		cPKIni.GetInteger("PK", "FactionPKFaction", 1, &m_nFactionPKFactionAddPKValue);
		cPKIni.GetInteger("PK", "KillerPKFaction", 1, &m_nKillerPKFactionAddPKValue);
		cPKIni.GetInteger("PK", "EnmityPK", 2, &m_nEnmityAddPKValue);
		cPKIni.GetInteger("PK", "BeKilled", -1, &m_nBeKilledAddPKValue);
		cPKIni.GetInteger("PK", "LevelDistance", 25, &m_nLevelDistance);
	}
	else
	{
		m_nPKDamageRate = 20;
		m_nFactionPKFactionAddPKValue = 1;
		m_nKillerPKFactionAddPKValue = 1;
		m_nEnmityAddPKValue = 2;
		m_nBeKilledAddPKValue = -1;
		m_nLevelDistance = 25;
	}
#endif

#ifndef _SERVER
	m_nShowPateFlag = PATE_CHAT;
	ZeroMemory(m_RequestNpc, sizeof(m_RequestNpc));
	m_RequestFreeIdx.Init(MAX_NPC_REQUEST);
	m_RequestUseIdx.Init(MAX_NPC_REQUEST);
 // Fixed By MrChuCong@gmail.com
	for (int i = MAX_NPC_REQUEST - 1; i > 0; i--)
	{
		m_RequestFreeIdx.Insert(i);
	}
#endif

}

void KNpcSet::LoadPlayerBaseValue(LPSTR szFile)
{
	KIniFile	File;

	File.Load(szFile);

	File.GetInteger("Common", "HurtFrame", 12, &m_cPlayerBaseValue.nHurtFrame);
	File.GetInteger("Common", "RunSpeed", 10, &m_cPlayerBaseValue.nRunSpeed);
	File.GetInteger("Common", "WalkSpeed", 5, &m_cPlayerBaseValue.nWalkSpeed);
	File.GetInteger("Common", "AttackFrame", 20, &m_cPlayerBaseValue.nAttackFrame);
#ifndef _SERVER
	File.GetInteger("Male", "WalkFrame", 15, &m_cPlayerBaseValue.nWalkFrame[0]);
	File.GetInteger("Female", "WalkFrame", 15, &m_cPlayerBaseValue.nWalkFrame[1]);
	File.GetInteger("Male", "RunFrame", 15, &m_cPlayerBaseValue.nRunFrame[0]);
	File.GetInteger("Female", "RunFrame", 15, &m_cPlayerBaseValue.nRunFrame[1]);
	File.GetInteger("Male", "StandFrame", 15, &m_cPlayerBaseValue.nStandFrame[0]);
	File.GetInteger("Female", "StandFrame", 15, &m_cPlayerBaseValue.nStandFrame[1]);
#endif
	File.Clear();
}

BOOL KNpcSet::IsNpcExist(int nIdx, DWORD dwId)
{
	if (Npc[nIdx].m_dwID == dwId)
		return TRUE;
	else
		return FALSE;
}

// Add by Freeway Chen in 2003.6.29
NPC_RELATION KNpcSet::GenOneRelation(NPCKIND Kind1, NPCKIND Kind2, NPCCAMP Camp1, NPCCAMP Camp2)
{
	// 路人NPC没有战斗关系
	if (Kind1 == kind_dialoger || Kind2 == kind_dialoger)
		return relation_dialog;

	// Current-client camp 7 protects players, but never prevents fighting monsters.
	if (Kind1 == kind_player && Kind2 == kind_player &&
		(Camp1 == camp_pk_closed || Camp2 == camp_pk_closed))
		return relation_ally;
	if (Kind1 == kind_player && Camp1 >= camp_pk_closed && Kind2 != kind_player)
		Camp1 = camp_justice;
	if (Kind2 == kind_player && Camp2 >= camp_pk_closed && Kind1 != kind_player)
		Camp2 = camp_justice;
	// 路人阵营没有战斗关系
	if (Camp1 == camp_event || Camp2 == camp_event)
		return relation_none;
	
	// 新手和动物还是战斗关系
	if ((Camp1 == camp_begin && Camp2 == camp_animal)
		||(Camp1 == camp_animal && Camp2 == camp_begin))
		return relation_enemy;

	// 只要有一个新手，就不存在战斗关系(是同盟关系，大家帮新手)
	if (Camp1 == camp_begin || Camp2 == camp_begin)
		return relation_ally;
	
    // 两个都是玩家
	if (Kind1 == kind_player && Kind2 == kind_player)
	{
		// 只要有一个玩家开了PK，就一定存在战斗关系
		if (Camp1 == camp_free || Camp2 == camp_free)
		{
			return relation_enemy;
		}
	}
	// 同阵营为伙伴关系
	if (Camp1 == Camp2)
		return relation_ally;

	// 其他情况为战斗关系
	return relation_enemy;
}


// Add by Freeway Chen in 2003.7.14
int KNpcSet::GenRelationTable()
{
    int nKind1 = 0;
    int nKind2 = 0;
    int nCamp1 = 0;
    int nCamp2 = 0;

    for (nKind1 = 0; nKind1 < kind_num; nKind1++)
    {
        for (nKind2 = 0; nKind2 < kind_num; nKind2++)
        {
            for (nCamp1 = 0; nCamp1 < camp_num; nCamp1++)
            {
                for (nCamp2 = 0; nCamp2 < camp_num; nCamp2++)
                {
                    m_RelationTable[nKind1][nKind2][nCamp1][nCamp2] = GenOneRelation(
                        (NPCKIND)nKind1, 
                        (NPCKIND)nKind2, 
                        (NPCCAMP)nCamp1, 
                        (NPCCAMP)nCamp2
                    );
                }
            }
        }

    }

    return true;

}




int	KNpcSet::SearchID(DWORD dwID)
{
//	int nMaxNpc = m_BinTree.GetCount();
//	for (int i = 0; i < MAX_NPC; i++)
//	{
//		if (Npc[i].m_dwID == dwID)
//			return Npc[i].m_Index;
//	}
	int nIdx = 0;
	while (1)
	{
		nIdx = m_UseIdx.GetNext(nIdx);
		if (nIdx == 0)
			break;
		if (Npc[nIdx].m_dwID == dwID)
			return nIdx;
	}

	return 0;
}

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：查找某个ClientID的npc是否存在
//---------------------------------------------------------------------------
int		KNpcSet::SearchClientID(KClientNpcID sClientID)
{
	int nIdx = 0;
	while (1)
	{
		nIdx = m_UseIdx.GetNext(nIdx);
		if (nIdx == 0)
			break;
		if (Npc[nIdx].m_sClientNpcID.m_dwRegionID == sClientID.m_dwRegionID &&
			Npc[nIdx].m_sClientNpcID.m_nNo == sClientID.m_nNo)
			return nIdx;
	}

	return 0;
}
#endif

int KNpcSet::SearchName(LPSTR szName)
{
//	for (int i = 0; i < MAX_NPC; i ++)
//	{
//		if (!strcmp(Npc[i].Name, szName))
//			return i ;
//	}
	int nIdx = 0;
	while (1)
	{
		nIdx = m_UseIdx.GetNext(nIdx);
		if (nIdx == 0)
			break;
		if (g_StrCmp(Npc[nIdx].Name, szName))
			return nIdx;
	}
	return 0;
}

int KNpcSet::SearchNameID(DWORD dwID)
{
	int nIdx = 0;
	DWORD	dwNameID = 0;
	while(1)
	{
		nIdx = m_UseIdx.GetNext(nIdx);
		if (nIdx == 0)
			break;
		if (Npc[nIdx].m_Kind != kind_player)
			continue;
		dwNameID = g_FileName2Id(Npc[nIdx].Name);
		if (dwID == dwNameID)
			return nIdx;
	}
	return 0;
}

int KNpcSet::FindFree()
{
	return m_FreeIdx.GetNext(0);
}

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：添加一个客户端npc（需要设定ClientNpcID）
//---------------------------------------------------------------------------
int		KNpcSet::AddClientNpc(int nTemplateID, int nRegionX, int nRegionY, int nMpsX, int nMpsY, int nNo)
{
	int		nNpcNo, nNpcSettingIdxInfo, nMapX, nMapY, nOffX, nOffY, nRegion;

	nNpcSettingIdxInfo = MAKELONG(1, nTemplateID);
	SubWorld[0].Mps2Map(nMpsX, nMpsY, &nRegion, &nMapX, &nMapY, &nOffX, &nOffY);
	if (nRegion < 0)
		return 0;
	nNpcNo = this->Add(nNpcSettingIdxInfo, 0, nRegion, nMapX, nMapY, nOffX, nOffY);
	if (nNpcNo > 0)
	{
		Npc[nNpcNo].m_sClientNpcID.m_dwRegionID = MAKELONG(nRegionX, nRegionY);
		Npc[nNpcNo].m_sClientNpcID.m_nNo = nNo;
		Npc[nNpcNo].m_RegionIndex = nRegion;
		Npc[nNpcNo].m_dwRegionID = SubWorld[0].m_Region[nRegion].m_RegionID;
		Npc[nNpcNo].m_bClientOnly = TRUE;
		Npc[nNpcNo].m_SyncSignal = SubWorld[0].m_dwCurrentTime;
		SubWorld[0].m_Region[nRegion].DecRef(Npc[nNpcNo].m_MapX, Npc[nNpcNo].m_MapY, obj_npc);
	}

	return nNpcNo;
}
#endif

int KNpcSet::Add(int nSubWorld, void* pNpcInfo)
{
	KSPNpc*	pKSNpcInfo = (KSPNpc *)pNpcInfo;

	int nMpsX = pKSNpcInfo->nPositionX;
	int nMpsY = pKSNpcInfo->nPositionY;
	int	nNpcSettingIdxInfo = MAKELONG(pKSNpcInfo->nLevel, pKSNpcInfo->nTemplateID);

	int nRet = Add(nNpcSettingIdxInfo, nSubWorld, nMpsX, nMpsY);
	
	if (nRet)
	{
		Npc[nRet].m_TrapScriptID = 0;
		g_StrCpyLen(Npc[nRet].Name, pKSNpcInfo->szName, sizeof(Npc[nRet].Name));
		Npc[nRet].m_Kind = pKSNpcInfo->shKind;
		Npc[nRet].m_Camp = pKSNpcInfo->cCamp;
		Npc[nRet].m_CurrentCamp = pKSNpcInfo->cCamp;
		Npc[nRet].m_Series = pKSNpcInfo->cSeries;
		// 当前资源使用蓝怪表，忽略旧黄金标记，普通野怪每次刷新独立抽签。
		Npc[nRet].m_cGold.SetGoldType(FALSE);
#ifdef _SERVER
		Npc[nRet].m_bHardNpcSpawn = Npc[nRet].m_Kind == kind_normal;
		Npc[nRet].RefreshHardNpc();
#endif

		if (pKSNpcInfo->szScript[0])
		{
			if (pKSNpcInfo->szScript[0] == '.')
				g_StrCpyLen(Npc[nRet].ActionScript, &pKSNpcInfo->szScript[1], sizeof(Npc[nRet].ActionScript));
			else
				g_StrCpyLen(Npc[nRet].ActionScript, pKSNpcInfo->szScript, sizeof(Npc[nRet].ActionScript));
			// 保持小写，保证脚本对应关系
			g_StrLower(Npc[nRet].ActionScript);
			Npc[nRet].m_ActionScriptID = g_FileName2Id(Npc[nRet].ActionScript);
			g_DebugLog("[Script]Npc %s,%d", Npc[nRet].ActionScript, Npc[nRet].m_ActionScriptID);
		}
		else
		{
			Npc[nRet].m_ActionScriptID = 0;
		}
	}
	return nRet;
}

int KNpcSet::Add(int nNpcSettingIdxInfo, int nSubWorld, int nMpsX, int nMpsY)
{
	int nRegion, nMapX, nMapY, nOffX, nOffY;
	if (nSubWorld < 0 || nSubWorld >= MAX_SUBWORLD)
		return 0;
	SubWorld[nSubWorld].Mps2Map(nMpsX, nMpsY, &nRegion, &nMapX, &nMapY, &nOffX, &nOffY);
	if (nRegion < 0)
		return 0;
	return Add(nNpcSettingIdxInfo, nSubWorld, nRegion, nMapX, nMapY, nOffX, nOffY);
}

int KNpcSet::Add(int nNpcSettingIdxInfo, int nSubWorld, int nRegion, int nMapX, int nMapY, int nOffX /* = 0 */, int nOffY /* = 0 */)
{
	int i = FindFree();

	if (i == 0)
		return 0;

#ifndef _SERVER
	Npc[i].m_sClientNpcID.m_dwRegionID = 0;
	Npc[i].m_sClientNpcID.m_nNo = -1;
	Npc[i].Remove();
	Npc[i].ClearBlood();
#endif

	int nNpcSettingIdx = (short)HIWORD(nNpcSettingIdxInfo);// >> 7; //除于128
	int nLevel = LOWORD(nNpcSettingIdxInfo);// & 0x7f; 

	Npc[i].m_Index = i;
	Npc[i].m_SkillList.m_nNpcIndex = i;
	Npc[i].Load(nNpcSettingIdx, nLevel);
	Npc[i].m_SubWorldIndex = nSubWorld;
	Npc[i].m_RegionIndex = nRegion;
#ifndef _SERVER
	if (nRegion >= 0 && nRegion < 9)
		Npc[i].m_dwRegionID = SubWorld[nSubWorld].m_Region[nRegion].m_RegionID;
#endif
	Npc[i].m_MapX = nMapX;
	Npc[i].m_MapY = nMapY;
	Npc[i].m_OffX = nOffX;
	Npc[i].m_OffY = nOffY;
	
	SubWorld[nSubWorld].Map2Mps(nRegion, nMapX, nMapY, nOffX, nOffY, &Npc[i].m_OriginX, &Npc[i].m_OriginY);

#ifdef _SERVER
	SetID(i);
#endif
	// 修改可用与使用表
	m_FreeIdx.Remove(i);
	m_UseIdx.Insert(i);
	SubWorld[nSubWorld].m_Region[nRegion].AddNpc(i);//m_WorldMessage.Send(GWM_NPC_ADD, nRegion, i);
	SubWorld[nSubWorld].m_Region[nRegion].AddRef(nMapX, nMapY, obj_npc);

#ifndef _SERVER
	Npc[i].m_dwRegionID = SubWorld[nSubWorld].m_Region[nRegion].m_RegionID;
#endif

	return i;
}

void KNpcSet::Remove(int nIdx)
{
	if (nIdx <= 0 || nIdx >= MAX_NPC)
		return;
#ifdef _SERVER
	RemoveOwnedSummon(nIdx);
	BroadcastRemove(nIdx);
#endif
	Npc[nIdx].ClearNpcState();
	Npc[nIdx].m_SkillList.Clear();
	Npc[nIdx].Remove();

	m_FreeIdx.Insert(nIdx);
	m_UseIdx.Remove(nIdx);
}

void KNpcSet::RemoveAll()
{
	int nIdx = m_UseIdx.GetNext(0);
	int nIdx1 = 0;
	while(nIdx)
	{
		nIdx1 = m_UseIdx.GetNext(nIdx);
		Npc[nIdx].ClearNpcState();
		Npc[nIdx].m_SkillList.Clear();
		Npc[nIdx].Remove();
		m_FreeIdx.Insert(nIdx);
		m_UseIdx.Remove(nIdx);
		nIdx = nIdx1;
	}
}

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：从npc数组中寻找属于某个region的 client npc ，添加进去
//---------------------------------------------------------------------------
void	KNpcSet::InsertNpcToRegion(int nRegionIdx)
{
	if (nRegionIdx < 0 || nRegionIdx >= MAX_REGION)
		return;
	int nIdx = 0;
	while (1)
	{
		nIdx = m_UseIdx.GetNext(nIdx);
		if (nIdx == 0)
			break;

		if (Npc[nIdx].m_sClientNpcID.m_dwRegionID > 0 && Npc[nIdx].m_dwRegionID == (DWORD)SubWorld[0].m_Region[nRegionIdx].m_RegionID)
		{
			SubWorld[0].m_Region[nRegionIdx].AddNpc(nIdx);
			Npc[nIdx].m_RegionIndex = nRegionIdx;
			Npc[nIdx].m_dwRegionID = SubWorld[0].m_Region[nRegionIdx].m_RegionID;
			Npc[nIdx].m_SyncSignal = SubWorld[0].m_dwCurrentTime;
			Npc[nIdx].SendCommand(do_stand);
		}
	}
}
#endif


void KNpcSet::SetID(int m_nIndex)
{
	if (m_nIndex <= 0 || m_nIndex >= MAX_NPC)
		return;

	Npc[m_nIndex].m_dwID = m_dwIDCreator;
	m_dwIDCreator++;
}

int KNpcSet::GetDistance(int nIdx1, int nIdx2)
{
	int	nRet = 0;
	if (Npc[nIdx1].m_SubWorldIndex != Npc[nIdx2].m_SubWorldIndex)
		return -1;

	if (Npc[nIdx1].m_RegionIndex == Npc[nIdx2].m_RegionIndex)
	{
		int XOff = (Npc[nIdx1].m_MapX - Npc[nIdx2].m_MapX) * SubWorld[Npc[nIdx1].m_SubWorldIndex].m_nCellWidth;
		XOff += (Npc[nIdx1].m_OffX - Npc[nIdx2].m_OffX) >> 10;

		int YOff = (Npc[nIdx1].m_MapY - Npc[nIdx2].m_MapY) * SubWorld[Npc[nIdx1].m_SubWorldIndex].m_nCellHeight;
		YOff += (Npc[nIdx1].m_OffY - Npc[nIdx2].m_OffY) >> 10;

		nRet = (int)sqrt(double(XOff * XOff + YOff * YOff)); // Fixed By MrChuCong@gmail.com
	}
	else
	{
		int X1, Y1;
		SubWorld[Npc[nIdx1].m_SubWorldIndex].Map2Mps(Npc[nIdx1].m_RegionIndex, 
			Npc[nIdx1].m_MapX,
			Npc[nIdx1].m_MapY,
			Npc[nIdx1].m_OffX,
			Npc[nIdx1].m_OffY,
			&X1,
			&Y1);
		int X2, Y2;
		SubWorld[Npc[nIdx2].m_SubWorldIndex].Map2Mps(Npc[nIdx2].m_RegionIndex, 
			Npc[nIdx2].m_MapX,
			Npc[nIdx2].m_MapY,
			Npc[nIdx2].m_OffX,
			Npc[nIdx2].m_OffY,
			&X2,
			&Y2);

		nRet = (int)sqrt(double(X2 - X1) * (X2 - X1) + (Y2 - Y1) * (Y2 - Y1)); // Fixed By MrChuCong@gmail.com
	}
	return nRet;
}

int		KNpcSet::GetDistanceSquare(int nIdx1, int nIdx2)
{
	int	nRet = 0;
	if (Npc[nIdx1].m_SubWorldIndex != Npc[nIdx2].m_SubWorldIndex)
		return -1;

	if (Npc[nIdx1].m_RegionIndex == Npc[nIdx2].m_RegionIndex)
	{
		int XOff = (Npc[nIdx1].m_MapX - Npc[nIdx2].m_MapX) * SubWorld[Npc[nIdx1].m_SubWorldIndex].m_nCellWidth;
		XOff += (Npc[nIdx1].m_OffX - Npc[nIdx2].m_OffX) >> 10;

		int YOff = (Npc[nIdx1].m_MapY - Npc[nIdx2].m_MapY) * SubWorld[Npc[nIdx1].m_SubWorldIndex].m_nCellHeight;
		YOff += (Npc[nIdx1].m_OffY - Npc[nIdx2].m_OffY) >> 10;

		nRet = (int)(XOff * XOff + YOff * YOff);
	}
	else
	{
		int X1, Y1;
		SubWorld[Npc[nIdx1].m_SubWorldIndex].Map2Mps(Npc[nIdx1].m_RegionIndex, 
			Npc[nIdx1].m_MapX,
			Npc[nIdx1].m_MapY,
			Npc[nIdx1].m_OffX,
			Npc[nIdx1].m_OffY,
			&X1,
			&Y1);
		int X2, Y2;
		SubWorld[Npc[nIdx2].m_SubWorldIndex].Map2Mps(Npc[nIdx2].m_RegionIndex, 
			Npc[nIdx2].m_MapX,
			Npc[nIdx2].m_MapY,
			Npc[nIdx2].m_OffX,
			Npc[nIdx2].m_OffY,
			&X2,
			&Y2);

		nRet = (int)((X2 - X1) * (X2 - X1) + (Y2 - Y1) * (Y2 - Y1));
	}
	return nRet;
}

int		KNpcSet::GetNextIdx(int nIdx)
{
	if (nIdx < 0 || nIdx >= MAX_NPC)
		return 0;
	return m_UseIdx.GetNext(nIdx);
}

#ifdef _SERVER
BOOL KNpcSet::SyncNpc(DWORD dwID, int nPlayerIdx)
{
	if (!g_pServer || nPlayerIdx <= 0 || nPlayerIdx >= MAX_PLAYER ||
		Player[nPlayerIdx].m_nNetConnectIdx < 0)
		return FALSE;
	int nObserver = Player[nPlayerIdx].m_nIndex;
	if (nObserver <= 0 || nObserver >= MAX_NPC ||
		Npc[nObserver].m_SubWorldIndex < 0 || Npc[nObserver].m_SubWorldIndex >= MAX_SUBWORLD ||
		Npc[nObserver].m_RegionIndex < 0 ||
		Npc[nObserver].m_RegionIndex >= SubWorld[Npc[nObserver].m_SubWorldIndex].m_nTotalRegion)
		return FALSE;

	// 迟到的补实体请求不能把已过图角色重新建立在旧地图上。
	int nFindIndex = Player[nPlayerIdx].FindAroundNpc(dwID);
	if (nFindIndex <= 0 || Npc[nFindIndex].m_bExchangeServer)
	{
		NPC_REQUEST_FAIL RequestFail = {};
		RequestFail.ProtocolType = s2c_requestnpcfail;
		RequestFail.ID = dwID;
		g_pServer->PackDataToClient(Player[nPlayerIdx].m_nNetConnectIdx, &RequestFail, sizeof(RequestFail));
		return FALSE;
	}
	return Npc[nFindIndex].SendSyncData(Player[nPlayerIdx].m_nNetConnectIdx);
}

#endif

#ifndef _SERVER
void KNpcSet::CheckBalance()
{
	int nIdx;
	nIdx = m_UseIdx.GetNext(0);
	while(nIdx)
	{
		int nTmpIdx = m_UseIdx.GetNext(nIdx);
		if (SubWorld[0].m_dwCurrentTime - Npc[nIdx].m_SyncSignal > 1000)
		{
			if (nIdx != Player[CLIENT_PLAYER_INDEX].m_nIndex)
			{
				if (Npc[nIdx].m_RegionIndex >= 0)
				{
					SubWorld[0].m_Region[Npc[nIdx].m_RegionIndex].RemoveNpc(nIdx);
					SubWorld[0].m_Region[Npc[nIdx].m_RegionIndex].DecRef(Npc[nIdx].m_MapX, Npc[nIdx].m_MapY, obj_npc);
				}
				Remove(nIdx);
			}
		}
		nIdx = nTmpIdx;
	}
	nIdx = m_RequestUseIdx.GetNext(0);
	while(nIdx)
	{
		int nTmpIdx = m_RequestUseIdx.GetNext(nIdx);
		if (SubWorld[0].m_dwCurrentTime - m_RequestNpc[nIdx].dwRequestTime > 100)
		{
			DWORD	dwID = m_RequestNpc[nIdx].dwRequestId;
			m_RequestNpc[nIdx].dwRequestId = 0;	
			m_RequestNpc[nIdx].dwRequestTime = 0;
			
			m_RequestUseIdx.Remove(nIdx);
			m_RequestFreeIdx.Insert(nIdx);
			g_DebugLog("[Request]Remove %d from %d on %d timeout", dwID, nIdx, SubWorld[0].m_dwCurrentTime);
		}
		nIdx = nTmpIdx;
	}
}
#endif

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：获得周围玩家列表，用于界面，队伍邀请列表
//-------------------------------------------------------------------------
int		KNpcSet::GetAroundPlayerForTeamInvite(KUiPlayerItem *pList, int nCount)
{
	int nCamp = Npc[Player[CLIENT_PLAYER_INDEX].m_nIndex].m_Camp;
	int nNum = 0, i;

	if (nCount == 0)
	{
		int nIdx = 0;
		while (1)
		{
			nIdx = m_UseIdx.GetNext(nIdx);
			if (nIdx == 0)
				break;
			if (Npc[nIdx].m_Kind != kind_player)
				continue;
			if (nIdx == Player[CLIENT_PLAYER_INDEX].m_nIndex)
				continue;
			if (Npc[nIdx].m_Camp != camp_begin && nCamp == camp_begin)
				continue;
			if (Npc[nIdx].m_RegionIndex < 0)
				continue;
			for (i = 0; i < MAX_TEAM_MEMBER; i++)
			{
				if ((DWORD)g_Team[0].m_nMember[i] == Npc[nIdx].m_dwID)
					break;
			}
			if (i < MAX_TEAM_MEMBER)
				continue;
			if ((DWORD)g_Team[0].m_nCaptain == Npc[nIdx].m_dwID)
				continue;
			nNum++;
		}
		
		return nNum;
	}

	if (!pList)
		return 0;

	int nIdx = 0;
	while (1)
	{
		nIdx = m_UseIdx.GetNext(nIdx);
		if (nIdx == 0)
			break;
		if (Npc[nIdx].m_Kind != kind_player)
			continue;
		if (nIdx == Player[CLIENT_PLAYER_INDEX].m_nIndex)
			continue;
		if (Npc[nIdx].m_Camp != camp_begin && nCamp == camp_begin)
			continue;
		if (Npc[nIdx].m_RegionIndex < 0)
			continue;
		for (i = 0; i < MAX_TEAM_MEMBER; i++)
		{
			if ((DWORD)g_Team[0].m_nMember[i] == Npc[nIdx].m_dwID)
				break;
		}
		if (i < MAX_TEAM_MEMBER)
			continue;
		if ((DWORD)g_Team[0].m_nCaptain == Npc[nIdx].m_dwID)
			continue;
		pList[nNum].nIndex = nIdx;
		pList[nNum].uId = Npc[nIdx].m_dwID;
		strcpy(pList[nNum].Name, Npc[nIdx].Name);
		nNum++;
	}
	
	return nNum;
}
#endif

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：获得周围玩家列表(用于列表)
//-------------------------------------------------------------------------
int		KNpcSet::GetAroundPlayer(KUiPlayerItem *pList, int nCount)
{
	int nNum = 0;

	if (nCount <= 0)
	{
		int nIdx = 0;
		while (1)
		{
			nIdx = m_UseIdx.GetNext(nIdx);
			if (nIdx == 0)
				break;
			if (Npc[nIdx].m_Kind != kind_player ||
				nIdx == Player[CLIENT_PLAYER_INDEX].m_nIndex ||
				Npc[nIdx].m_RegionIndex < 0)
			{
				continue;
			}
//			if (Player[CLIENT_PLAYER_INDEX].m_cChat.CheckExist(Npc[nIdx].Name))
//				continue;
			nNum++;
		}
		
		return nNum;
	}

	if (!pList)
		return 0;

	int nIdx = 0;
	while (nNum < nCount)
	{
		nIdx = m_UseIdx.GetNext(nIdx);
		if (nIdx == 0)
			break;
		if (Npc[nIdx].m_Kind != kind_player ||
			nIdx == Player[CLIENT_PLAYER_INDEX].m_nIndex ||
			Npc[nIdx].m_RegionIndex < 0)
		{
			continue;
		}
//		if (Player[CLIENT_PLAYER_INDEX].m_cChat.CheckExist(Npc[nIdx].Name))
//			continue;
		pList[nNum].nIndex = nIdx;
		pList[nNum].uId = Npc[nIdx].m_dwID;
		strcpy(pList[nNum].Name, Npc[nIdx].Name);
		pList[nNum].nData = Npc[nIdx].GetMenuState();
		nNum++;
	}
	
	return nNum;
}
#endif

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：设定是否全部显示玩家的名字
//			bFlag ==	TRUE 显示，bFlag == FALSE 不显示 zroc add
//-------------------------------------------------------------------------
void	KNpcSet::SetShowNameFlag(BOOL bFlag)
{
	if (bFlag)
		m_nShowPateFlag |= PATE_NAME;
	else
		m_nShowPateFlag &= ~PATE_NAME;
}
#endif

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：判断是否全部显示玩家的名字  返回值 TRUE 显示，FALSE 不显示
//-------------------------------------------------------------------------
BOOL	KNpcSet::CheckShowName()
{
	return m_nShowPateFlag & PATE_NAME;
}
#endif

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：设定是否全部显示玩家的血
//			bFlag ==	TRUE 显示，bFlag == FALSE 不显示 zroc add
//-------------------------------------------------------------------------
void	KNpcSet::SetShowLifeFlag(BOOL bFlag)
{
	if (bFlag)
		m_nShowPateFlag |= PATE_LIFE;
	else
		m_nShowPateFlag &= ~PATE_LIFE;
}
#endif

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：判断是否全部显示玩家的血  返回值 TRUE 显示，FALSE 不显示
//-------------------------------------------------------------------------
BOOL	KNpcSet::CheckShowLife()
{
	return m_nShowPateFlag & PATE_LIFE;
}
#endif

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：设定是否全部显示玩家的聊天
//			bFlag ==	TRUE 显示，bFlag == FALSE 不显示 zroc add
//-------------------------------------------------------------------------
void	KNpcSet::SetShowChatFlag(BOOL bFlag)
{
	if (bFlag)
		m_nShowPateFlag |= PATE_CHAT;
	else
		m_nShowPateFlag &= ~PATE_CHAT;
}
#endif

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：判断是否全部显示玩家的聊天  返回值 TRUE 显示，FALSE 不显示
//-------------------------------------------------------------------------
BOOL	KNpcSet::CheckShowChat()
{
	return m_nShowPateFlag & PATE_CHAT;
}
#endif

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：设定是否全部显示玩家的内力
//			bFlag ==	TRUE 显示，bFlag == FALSE 不显示 zroc add
//-------------------------------------------------------------------------
void	KNpcSet::SetShowManaFlag(BOOL bFlag)
{
	if (bFlag)
		m_nShowPateFlag |= PATE_MANA;
	else
		m_nShowPateFlag &= ~PATE_MANA;
}
#endif

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：判断是否全部显示玩家的内力  返回值 TRUE 显示，FALSE 不显示
//-------------------------------------------------------------------------
BOOL	KNpcSet::CheckShowMana()
{
	return m_nShowPateFlag & PATE_MANA;
}
#endif

//-------------------------------------------------------------------------
//	功能：把所有npc的 bActivateFlag 设为 FALSE
//		(每次游戏循环处理所有npc的activate之前做这个处理)
//-------------------------------------------------------------------------
void	KNpcSet::ClearActivateFlagOfAllNpc()
{
	int nIdx = 0;
	while (1)
	{
		nIdx = m_UseIdx.GetNext(nIdx);
		if (nIdx == 0)
			break;
		Npc[nIdx].m_bActivateFlag = FALSE;
	}
}

#ifndef _SERVER
//-------------------------------------------------------------------------
//	功能：获得周围同阵营的已开放队伍队长列表 不同阵营现在可以组队
//-------------------------------------------------------------------------
void	KNpcSet::GetAroundOpenCaptain(int nCamp)
{
	int		nIdx, nNum, nNo;

	nIdx = 0;
	nNum = 0;
	while (1)
	{
		nIdx = m_UseIdx.GetNext(nIdx);
		if (nIdx == 0)
			break;
		if (Npc[nIdx].m_Kind != kind_player)
			continue;
		if (nIdx == Player[CLIENT_PLAYER_INDEX].m_nIndex)
			continue;
		// 不同阵营现在可以组队，老手不能加入新人队伍，新人可以加入老手队伍
		if (Npc[nIdx].m_Camp == camp_begin && nCamp != camp_begin)
			continue;
//		if (Npc[nIdx].m_Camp != nCamp)
//			continue;
		if (Npc[nIdx].m_RegionIndex < 0)
			continue;
		if (Npc[nIdx].GetMenuState() == PLAYER_MENU_STATE_TEAMOPEN)
			nNum++;
	}
	
	if (nNum > 0)
	{
		KUiTeamItem* const pTeamList = new KUiTeamItem[nNum];
		nIdx = 0;
		nNo = 0;
		while (1)
		{
			nIdx = m_UseIdx.GetNext(nIdx);
			if (nIdx == 0)
				break;
			if (Npc[nIdx].m_Kind != kind_player)
				continue;
			if (nIdx == Player[CLIENT_PLAYER_INDEX].m_nIndex)
				continue;
			// 不同阵营现在可以组队，老手不能加入新人队伍，新人可以加入老手队伍
			if (Npc[nIdx].m_Camp == camp_begin && nCamp != camp_begin)
				continue;
//			if (Npc[nIdx].m_Camp != nCamp)
//				continue;
			if (Npc[nIdx].m_RegionIndex < 0)
				continue;
			if (Npc[nIdx].GetMenuState() == PLAYER_MENU_STATE_TEAMOPEN)
			{
				pTeamList[nNo].Leader.nIndex = nIdx;
				pTeamList[nNo].Leader.uId = Npc[nIdx].m_dwID;
				strcpy(pTeamList[nNo].Leader.Name, Npc[nIdx].Name);
				nNo++;
				if (nNo >= nNum)
					break;
			}
		}
		CoreDataChanged(GDCNI_TEAM_NEARBY_LIST, (unsigned int)pTeamList, nNo);
		delete []pTeamList;
	}
}
#endif

#ifndef _SERVER	// 用于客户端
int	KNpcSet::SearchNpcAt(int nX, int nY, int nRelation, int nRange)
{
	int nIdx;
	int	nMin = nRange;
	int nMinIdx = 0;
	int	nLength = 0;
	int nSrcX[2];
	int	nSrcY[2];

	nSrcX[0] = nX;
	nSrcY[0] = nY;
	g_ScenePlace.ViewPortCoordToSpaceCoord(nSrcX[0], nSrcY[0], 0);

	nSrcX[1] = nX;
	nSrcY[1] = nY;
	g_ScenePlace.ViewPortCoordToSpaceCoord(nSrcX[1], nSrcY[1], 120);

	int nDx = nSrcX[0] - nSrcX[1];
	int nDy = nSrcY[0] - nSrcY[1];

	nIdx = 0;
	while (1)
	{
		nIdx = m_UseIdx.GetNext(nIdx);

		if (nIdx == 0)
			break;

		if (Npc[nIdx].m_RegionIndex < 0)
			continue;

		if (nIdx == Player[CLIENT_PLAYER_INDEX].m_nIndex)
			continue;

		if (Npc[nIdx].m_bClientOnly)
			continue;

		if (!(GetRelation(Player[CLIENT_PLAYER_INDEX].m_nIndex, nIdx) & nRelation))
			continue;
		
		int x, y;
		SubWorld[0].Map2Mps(Npc[nIdx].m_RegionIndex, Npc[nIdx].m_MapX, Npc[nIdx].m_MapY,
			Npc[nIdx].m_OffX, Npc[nIdx].m_OffY, &x, &y);

		if (nSrcY[0] > y)
			continue;

		if (nSrcY[0] < y - 120)
			continue;

		nLength = abs(nDx * (nSrcY[0] - y) / nDy + nSrcX[0] - x);
		if (nLength < nMin)
		{
			nMin = nLength;
			nMinIdx = nIdx;
		}
	}

	return nMinIdx;
}
#endif

#ifndef _SERVER
BOOL KNpcSet::IsNpcRequestExist(DWORD dwID)
{
	return (GetRequestIndex(dwID) > 0);
}

void KNpcSet::InsertNpcRequest(DWORD dwID)
{
	if (IsNpcRequestExist(dwID))
	{
		return;
	}

	int nIndex = m_RequestFreeIdx.GetNext(0);
	if (!nIndex)
		return;

	m_RequestNpc[nIndex].dwRequestId = dwID;
	m_RequestNpc[nIndex].dwRequestTime = SubWorld[0].m_dwCurrentTime;
	m_RequestFreeIdx.Remove(nIndex);
	m_RequestUseIdx.Insert(nIndex);
	g_DebugLog("[Request]Insert %d at %d on %d", dwID, nIndex, SubWorld[0].m_dwCurrentTime);
}

void KNpcSet::RemoveNpcRequest(DWORD dwID)
{
	if(!IsNpcRequestExist(dwID))
	{
		return;
	}
	int nIndex = GetRequestIndex(dwID);

	// because _ASSERT(IsNpcRequestExist()); so nIndex > 0;
	m_RequestNpc[nIndex].dwRequestId = 0;	
	m_RequestNpc[nIndex].dwRequestTime = 0;

	m_RequestUseIdx.Remove(nIndex);
	m_RequestFreeIdx.Insert(nIndex);
	g_DebugLog("[Request]Remove %d from %d on %d", dwID, nIndex, SubWorld[0].m_dwCurrentTime);
}

int KNpcSet::GetRequestIndex(DWORD dwID)
{
	int nIndex = m_RequestUseIdx.GetNext(0);

	while(nIndex)
	{
		if (m_RequestNpc[nIndex].dwRequestId == dwID)
		{
			return nIndex;
		}
		nIndex = m_RequestUseIdx.GetNext(nIndex);
	}
	return 0;
}
#endif


// Beneficial player skills use owner/team/camp eligibility independently of attack PK rules.
NPC_RELATION KNpcSet::GetSupportRelation(int nIdx1, int nIdx2)
{
#ifdef _SERVER
	if (nIdx1 <= 0 || nIdx1 >= MAX_NPC || nIdx2 <= 0 || nIdx2 >= MAX_NPC)
		return relation_none;
	int nFirst = Npc[nIdx1].GetCombatOwner();
	int nSecond = Npc[nIdx2].GetCombatOwner();
	if (!nFirst || !nSecond)
		return GetRelation(nIdx1, nIdx2);
	if (nFirst == nSecond)
		return nIdx1 == nIdx2 ? relation_self : relation_ally;
	int nPlayer1 = Npc[nFirst].m_nPlayerIdx;
	int nPlayer2 = Npc[nSecond].m_nPlayerIdx;
	if (nPlayer1 <= 0 || nPlayer1 >= MAX_PLAYER || nPlayer2 <= 0 || nPlayer2 >= MAX_PLAYER ||
		Player[nPlayer1].m_nIndex != nFirst || Player[nPlayer2].m_nIndex != nSecond)
		return relation_none;
	KPlayerTeam& FirstTeam = Player[nPlayer1].m_cTeam;
	KPlayerTeam& SecondTeam = Player[nPlayer2].m_cTeam;
	if (FirstTeam.m_nFlag && SecondTeam.m_nFlag && FirstTeam.m_nID >= 0 &&
		FirstTeam.m_nID < MAX_TEAM && FirstTeam.m_nID == SecondTeam.m_nID &&
		g_Team[FirstTeam.m_nID].CheckIn(nPlayer1) && g_Team[FirstTeam.m_nID].CheckIn(nPlayer2))
		return relation_ally;
	int nCamp1 = Npc[nFirst].m_CurrentCamp;
	int nCamp2 = Npc[nSecond].m_CurrentCamp;
	if (nCamp1 == camp_free || nCamp2 == camp_free)
		return relation_none;
	// Non-team players must share the actual camp, including PK-closed and nation-war camps.
	return nCamp1 == nCamp2 ? relation_ally : relation_none;
#else
	return GetRelation(nIdx1, nIdx2);
#endif
}

NPC_RELATION KNpcSet::GetRelation(int nId1, int nId2)
{
	if (nId1 <= 0 || nId1 >= MAX_NPC || nId2 <= 0 || nId2 >= MAX_NPC)
		return relation_none;
#ifdef _SERVER
	if (Npc[nId1].m_Kind == kind_summon || Npc[nId2].m_Kind == kind_summon)
	{
		int nFirst = Npc[nId1].m_Kind == kind_summon ? Npc[nId1].GetCombatOwner() : nId1;
		int nSecond = Npc[nId2].m_Kind == kind_summon ? Npc[nId2].GetCombatOwner() : nId2;
		if (!nFirst || !nSecond)
			return relation_none;
		if (nFirst == nSecond)
			return nId1 == nId2 ? relation_self : relation_ally;
		return GetRelation(nFirst, nSecond);
	}
#endif
	// 同一个人
	if (nId1 == nId2)
		return relation_self;

#ifndef _SERVER
	if (Npc[nId1].m_bClientOnly || Npc[nId2].m_bClientOnly)
		return relation_none;
#endif

    _ASSERT(
        ((Npc[nId1].m_Kind >= 0) && (Npc[nId1].m_Kind < kind_num)) &&
        ((Npc[nId2].m_Kind >= 0) && (Npc[nId2].m_Kind < kind_num)) &&
        ((Npc[nId1].m_CurrentCamp >= 0) && (Npc[nId1].m_CurrentCamp < camp_num)) &&
        ((Npc[nId2].m_CurrentCamp >= 0) && (Npc[nId2].m_CurrentCamp < camp_num))
    );

#ifndef _SERVER
	if (Player[CLIENT_PLAYER_INDEX].m_nIndex != nId1 && Player[CLIENT_PLAYER_INDEX].m_nIndex != nId2)
	{
		if (Npc[nId1].m_Kind == kind_player && Npc[nId2].m_Kind == kind_player && Npc[nId1].m_nPKFlag == FALSE)
			return relation_none;

		return (NPC_RELATION)m_RelationTable
			[Npc[nId1].m_Kind]
			[Npc[nId2].m_Kind]
			[Npc[nId1].m_CurrentCamp]
			[Npc[nId2].m_CurrentCamp];
	}
	else if (Player[CLIENT_PLAYER_INDEX].m_nIndex == nId1)
	{
		if (Player[CLIENT_PLAYER_INDEX].m_cPK.GetExercisePKAim() == Npc[nId2].m_dwID)
			return relation_enemy;
		if (Player[CLIENT_PLAYER_INDEX].m_cPK.GetEnmityPKState() == enumPK_ENMITY_STATE_PKING &&
			Player[CLIENT_PLAYER_INDEX].m_cPK.GetEnmityPKAimNpcID() == Npc[nId2].m_dwID)
			return relation_enemy;
		if (Npc[nId2].m_Kind == kind_player && Player[CLIENT_PLAYER_INDEX].m_cPK.GetNormalPKState() == FALSE)
			return relation_none;
		return (NPC_RELATION)m_RelationTable
			[Npc[nId1].m_Kind]
			[Npc[nId2].m_Kind]
			[Npc[nId1].m_CurrentCamp]
			[Npc[nId2].m_CurrentCamp];
	}
	else	// if (Player[CLIENT_PLAYER_INDEX].m_nIndex == nId2)
	{
		if (Player[CLIENT_PLAYER_INDEX].m_cPK.GetExercisePKAim() == Npc[nId1].m_dwID)
			return relation_enemy;
		if (Player[CLIENT_PLAYER_INDEX].m_cPK.GetEnmityPKState() == enumPK_ENMITY_STATE_PKING &&
			Player[CLIENT_PLAYER_INDEX].m_cPK.GetEnmityPKAimNpcID() == Npc[nId1].m_dwID)
			return relation_enemy;
		if (Npc[nId1].m_Kind == kind_player && Npc[nId2].m_Kind == kind_player && !Npc[nId1].m_nPKFlag)
			return relation_none;
		return (NPC_RELATION)m_RelationTable
			[Npc[nId1].m_Kind]
			[Npc[nId2].m_Kind]
			[Npc[nId1].m_CurrentCamp]
			[Npc[nId2].m_CurrentCamp];
	}
#endif

#ifdef _SERVER
	if (Npc[nId1].m_Kind != kind_player || Npc[nId2].m_Kind != kind_player)
	{
		return (NPC_RELATION)m_RelationTable
			[Npc[nId1].m_Kind]
			[Npc[nId2].m_Kind]
			[Npc[nId1].m_CurrentCamp]
			[Npc[nId2].m_CurrentCamp];
	}
	else
	{
		if (Player[Npc[nId1].m_nPlayerIdx].m_cPK.GetExercisePKAim() == Npc[nId2].m_nPlayerIdx)
			return relation_enemy;
		if (Player[Npc[nId1].m_nPlayerIdx].m_cPK.GetEnmityPKState() == enumPK_ENMITY_STATE_PKING
			&& Player[Npc[nId1].m_nPlayerIdx].m_cPK.GetEnmityPKAim() == Npc[nId2].m_nPlayerIdx)
			return relation_enemy;
//		if (!Player[Npc[nId1].m_nPlayerIdx].m_cPK.GetNormalPKState())
//			return relation_none;
		NPC_RELATION nRelation =  (NPC_RELATION)m_RelationTable
			[Npc[nId1].m_Kind]
			[Npc[nId2].m_Kind]
			[Npc[nId1].m_CurrentCamp]
			[Npc[nId2].m_CurrentCamp];
		if (nRelation == relation_enemy)
		{
			if (!Player[Npc[nId1].m_nPlayerIdx].m_cPK.GetNormalPKState())
				return relation_none;
		}
		return nRelation;
	}
#endif
}

//------------------------ class KInstantSpecial start -------------------------
#ifndef _SERVER
KInstantSpecial::KInstantSpecial()
{
	int		i;
	this->m_nLoadFlag = FALSE;
	for (i = 0; i < MAX_INSTANT_STATE; i++)
		this->m_szSprName[i][0] = 0;
	for (i = 0; i < MAX_INSTANT_SOUND; i++)
		this->m_szSoundName[i][0] = 0;

	m_pSoundNode = NULL;
	m_pWave = NULL;
}

void	KInstantSpecial::LoadSprName()
{
	int		i;
	for (i = 0; i < MAX_INSTANT_STATE; i++)
		m_szSprName[i][0] = 0;

	KTabFile	cSprName;
//	g_SetFilePath("\\");
	if (!cSprName.Load(PLAYER_INSTANT_SPECIAL_FILE))
		return;
	for (i = 0; i < MAX_INSTANT_STATE; i++)
		cSprName.GetString(i + 2, 3, "", m_szSprName[i], sizeof(m_szSprName[i]));
}

void	KInstantSpecial::LoadSoundName()
{
	int		i;
	for (i = 0; i < MAX_INSTANT_SOUND; i++)
		m_szSoundName[i][0] = 0;

	KIniFile	cSoundName;
	char		szTemp[32];
//	g_SetFilePath("\\");
	if (!cSoundName.Load(defINSTANT_SOUND_FILE))
		return;
	for (i = 0; i < MAX_INSTANT_SOUND; i++)
	{
		sprintf(szTemp, "%d", i);
		cSoundName.GetString("Game", szTemp, "", this->m_szSoundName[i], sizeof(m_szSoundName[i]));
	}
}

void	KInstantSpecial::GetSprName(int nNo, char *lpszName, int nLength)
{
	if (!lpszName || nLength <= 0)
		return;
	if (nNo < 0 || nNo >= MAX_INSTANT_STATE)
	{
		lpszName[0] = 0;
		return;
	}
	if (this->m_nLoadFlag == FALSE)
	{
		this->LoadSprName();
		this->LoadSoundName();
		m_nLoadFlag = TRUE;
	}

	if (strlen(this->m_szSprName[nNo]) < (DWORD)nLength)
		strcpy(lpszName, m_szSprName[nNo]);
	else
		lpszName[0] = 0;
}

void	KInstantSpecial::PlaySound(int nNo)
{
	if (this->m_nLoadFlag == FALSE)
	{
		this->LoadSprName();
		this->LoadSoundName();
		m_nLoadFlag = TRUE;
	}
	if (nNo < 0 || nNo >= MAX_INSTANT_SOUND)
		return;
	if ( !m_szSoundName[nNo][0] )
		return;

	m_pSoundNode = (KCacheNode*)g_SoundCache.GetNode(m_szSoundName[nNo], (KCacheNode*)m_pSoundNode);
	m_pWave = (KWavSound*)m_pSoundNode->m_lpData;
	if (m_pWave)
	{
		if (m_pWave->IsPlaying())
			return;
		m_pWave->Play(0, -10000 + Option.GetSndVolume() * 100, 0);
	}
}

#endif
//------------------------- class KInstantSpecial end --------------------------


#ifdef _SERVER
void KNpcSet::BroadcastRemove(int nIdx, int nExceptClient)
{
	if (!g_pServer || nIdx <= 0 || nIdx >= MAX_NPC || !Npc[nIdx].m_dwID)
		return;
	NPC_REMOVE_SYNC	NetCommand;

	NetCommand.ProtocolType = (BYTE)s2c_npcremove;
	NetCommand.ID = Npc[nIdx].m_dwID;

	int nSubWorld = Npc[nIdx].m_SubWorldIndex;
	int nRegion = Npc[nIdx].m_RegionIndex;

	if (nSubWorld >= 0 && nSubWorld < MAX_SUBWORLD && nRegion >= 0 && nRegion < SubWorld[nSubWorld].m_nTotalRegion)
	{
		POINT	POff[8] =
		{
			{0, 32},
			{-16, 32},
			{-16, 0},
			{-16, -32},
			{0, -32},
			{16, -32},
			{16, 0},
			{16, 32},
		};
		int nMaxCount = MAX_BROADCAST_COUNT;
		SubWorld[nSubWorld].m_Region[nRegion].BroadCast(&NetCommand, sizeof(NetCommand), nMaxCount, Npc[nIdx].m_MapX, Npc[nIdx].m_MapY, nExceptClient);

		int nConRegion;
		for (int i = 0; i < 8; i++)
		{
			nConRegion = SubWorld[nSubWorld].m_Region[nRegion].m_nConnectRegion[i];
			if (nConRegion == -1)
				continue;
			SubWorld[nSubWorld].m_Region[nConRegion].BroadCast(&NetCommand, sizeof(NetCommand), nMaxCount, Npc[nIdx].m_MapX - POff[i].x, Npc[nIdx].m_MapY - POff[i].y, nExceptClient);
		}
	}
}

BOOL KNpcSet::CanSummon(int nOwner, const KSkill* pSkill, int nX, int nY)
{
	if (nOwner <= 0 || nOwner >= MAX_NPC || !pSkill ||
		pSkill->GetSummonTemplate() < 0 || !FindFree())
		return FALSE;
	KNpc& Owner = Npc[nOwner];
	if (!Owner.IsPlayer() || Owner.m_RegionIndex < 0 || Owner.m_bExchangeServer ||
		Owner.m_CurrentLife <= 0 ||
		Owner.m_Doing == do_death || Owner.m_Doing == do_revive)
		return FALSE;
	if (nX == -1)
	{
		if (nY <= 0 || nY >= MAX_NPC || Npc[nY].m_RegionIndex < 0 ||
			Npc[nY].m_SubWorldIndex != Owner.m_SubWorldIndex)
			return FALSE;
		Npc[nY].GetMpsPos(&nX, &nY);
	}
	int nOwnerX, nOwnerY;
	Owner.GetMpsPos(&nOwnerX, &nOwnerY);
	return nX >= 0 && nY >= 0 &&
		g_GetDistance(nOwnerX, nOwnerY, nX, nY) <= pSkill->GetAttackRadius();
}

int KNpcSet::CreateSummon(int nOwner, const KSkill* pSkill, int nX, int nY)
{
	if (!CanSummon(nOwner, pSkill, nX, nY))
		return 0;
	KNpc& Owner = Npc[nOwner];
	// Both captures create the creature in the owner's current cell, not at the cursor.
	int nWorld = Owner.m_SubWorldIndex;
	int nIndex = Add(MAKELONG(pSkill->m_ulLevel, pSkill->GetSummonTemplate()),
		nWorld, Owner.m_RegionIndex, Owner.m_MapX, Owner.m_MapY, 0, 0);
	if (!nIndex)
		return 0;
	KNpc& Summon = Npc[nIndex];
	// Create first: a failed allocation must preserve the previous summon.
	RemoveOwnedSummon(nOwner);
	Summon.m_Kind = kind_summon;
	Summon.m_nSummonOwner = nOwner;
	Summon.m_dwSummonOwner = Owner.m_dwID;
	Summon.m_nSummonSkill = ((KSkill*)pSkill)->GetSkillId();
	Summon.m_Camp = Owner.m_Camp;
	Summon.m_CurrentCamp = Owner.m_CurrentCamp;
	Summon.m_Experience = Summon.m_CurrentExperience = 0;
	Summon.m_pDropRate = Summon.m_pDropRate1 = NULL;
	Summon.ActionScript[0] = 0;
	Summon.m_AIMAXTime = min(4, max(1, Summon.m_AIMAXTime));
	Summon.m_NextAITime = SubWorld[nWorld].m_dwCurrentTime;
	lstrcpyn(Summon.Name, Owner.Name, sizeof(Summon.Name));
	Owner.m_nSummonIndex = nIndex;
	Owner.m_dwSummonId = Summon.m_dwID;
	Summon.SetActiveSkill(1);
	Summon.RefreshSummonAttributes(TRUE);
	// Scene broadcasting discovers it naturally; send immediately to its owner.
	Summon.SendSyncData(Player[Owner.m_nPlayerIdx].m_nNetConnectIdx);
	g_DebugLog("[Summon] owner=%lu skill=%d level=%d entity=%lu template=%d life=%d ar=%d defense=%d dodge=%d",
		Owner.m_dwID, Summon.m_nSummonSkill, Summon.m_Level, Summon.m_dwID,
		Summon.m_NpcSettingIdx, Summon.m_CurrentLifeMax, Summon.m_CurrentAttackRating,
		Summon.m_CurrentDefend, Summon.m_CurrentExDefend);
	return nIndex;
}

void KNpcSet::RemoveSummon(int nIndex)
{
	if (nIndex <= 0 || nIndex >= MAX_NPC || Npc[nIndex].m_Kind != kind_summon)
		return;
	KNpc& Summon = Npc[nIndex];
	int nOwner = Summon.GetCombatOwner();
	if (nOwner && Npc[nOwner].GetSummon() == nIndex)
	{
		Summon.SyncSummon(TRUE);
		Npc[nOwner].m_nSummonIndex = 0;
		Npc[nOwner].m_dwSummonId = 0;
	}
	if (Summon.m_RegionIndex >= 0)
	{
		SubWorld[Summon.m_SubWorldIndex].m_Region[Summon.m_RegionIndex].RemoveNpc(nIndex);
		SubWorld[Summon.m_SubWorldIndex].m_Region[Summon.m_RegionIndex].DecRef(Summon.m_MapX, Summon.m_MapY, obj_npc);
	}
	Remove(nIndex);
}

void KNpcSet::NotifySummonAttacked(int nVictim, int nAttacker)
{
	if (nVictim <= 0 || nVictim >= MAX_NPC || nAttacker <= 0 || nAttacker >= MAX_NPC)
		return;
	KNpc& Victim = Npc[nVictim];
	KNpc& Attacker = Npc[nAttacker];
	int nOwner = Victim.GetCombatOwner();
	if (!nOwner || Victim.m_CurrentLife <= 0 || Victim.m_Doing == do_death ||
		Victim.m_Doing == do_revive || Victim.m_RegionIndex < 0 ||
		Attacker.m_CurrentLife <= 0 || Attacker.m_Doing == do_death ||
		Attacker.m_Doing == do_revive || Attacker.m_RegionIndex < 0 ||
		Attacker.m_SubWorldIndex != Victim.m_SubWorldIndex ||
		GetRelation(nVictim, nAttacker) != relation_enemy)
		return;
	int nSummon = Npc[nOwner].GetSummon();
	if (!nSummon)
		return;
	KNpc& Summon = Npc[nSummon];
	if (Summon.m_CurrentLife <= 0 || Summon.m_Doing == do_death ||
		Summon.m_Doing == do_revive || Summon.m_RegionIndex < 0 ||
		Summon.m_SubWorldIndex != Victim.m_SubWorldIndex ||
		GetDistance(nOwner, nAttacker) > Summon.m_CurrentActiveRadius)
		return;
	Summon.m_nPeopleIdx = nAttacker;
	Summon.m_dwSummonTarget = Attacker.m_dwID;
	Summon.m_NextAITime = SubWorld[Summon.m_SubWorldIndex].m_dwCurrentTime;
}

void KNpcSet::MoveOwnedSummon(int nOwner)
{
	if (nOwner <= 0 || nOwner >= MAX_NPC)
		return;
	KNpc& Owner = Npc[nOwner];
	int nSummon = Owner.GetSummon();
	if (!nSummon || Owner.m_SubWorldIndex < 0 || Owner.m_RegionIndex < 0)
		return;
	KNpc& Summon = Npc[nSummon];
	if (Summon.m_CurrentLife <= 0 || Summon.m_Doing == do_death || Summon.m_Doing == do_revive)
	{
		RemoveSummon(nSummon);
		return;
	}
	// Keep the same creature and life; moving maps is not another summon cast.
	BroadcastRemove(nSummon);
	if (Summon.m_SubWorldIndex >= 0 && Summon.m_RegionIndex >= 0)
	{
		KRegion& OldRegion = SubWorld[Summon.m_SubWorldIndex].m_Region[Summon.m_RegionIndex];
		OldRegion.RemoveNpc(nSummon);
		OldRegion.DecRef(Summon.m_MapX, Summon.m_MapY, obj_npc);
	}
	Summon.m_SubWorldIndex = Owner.m_SubWorldIndex;
	Summon.m_RegionIndex = Owner.m_RegionIndex;
	Summon.m_MapX = Owner.m_MapX;
	Summon.m_MapY = Owner.m_MapY;
	Summon.m_MapZ = 0;
	Summon.m_OffX = Owner.m_OffX;
	Summon.m_OffY = Owner.m_OffY;
	Owner.GetMpsPos(&Summon.m_OriginX, &Summon.m_OriginY);
	Summon.m_DesX = Summon.m_OriginX;
	Summon.m_DesY = Summon.m_OriginY;
	Summon.m_nPeopleIdx = 0;
	Summon.m_dwSummonTarget = 0;
	Summon.m_nLastDamageIdx = Summon.m_nLastPoisonDamageIdx = 0;
	Summon.m_SkillParam1 = Summon.m_SkillParam2 = 0;
	Summon.SendCommand(do_none);
	Summon.DoStand();
	Summon.m_NextAITime = SubWorld[Summon.m_SubWorldIndex].m_dwCurrentTime;
	KRegion& NewRegion = SubWorld[Summon.m_SubWorldIndex].m_Region[Summon.m_RegionIndex];
	NewRegion.AddNpc(nSummon);
	NewRegion.AddRef(Summon.m_MapX, Summon.m_MapY, obj_npc);
	Summon.SyncSummon();
	Summon.SendSyncData(Player[Owner.m_nPlayerIdx].m_nNetConnectIdx);
}

void KNpcSet::RemoveOwnedSummon(int nOwner)
{
	if (nOwner > 0 && nOwner < MAX_NPC)
		RemoveSummon(Npc[nOwner].GetSummon());
}
#endif
