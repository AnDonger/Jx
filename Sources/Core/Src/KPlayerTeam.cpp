//---------------------------------------------------------------------------
// Sword3 Engine (c) 2002 by Kingsoft
//
// File:	KPlayerTeam.cpp
// Date:	2002.01.06
// Code:	边城浪子
// Desc:	Team Class
//---------------------------------------------------------------------------

#include	"KCore.h"
#include	"KNpc.h"
#ifdef _SERVER
//#include	"KNetServer.h"
//#include "../../Headers/IServer.h"
#else
//#include	"KNetClient.h"
#include "../../Headers/IClient.h"
#include	"CoreShell.h"
#endif
#include	"KPlayer.h"
#include	"KPlayerSet.h"
#include	"KPlayerTeam.h"
//#include	"MyAssert.h"


enum
{
	Team_S_Close = 0,
	Team_S_Open,
};

KTeam		g_Team[MAX_TEAM];
#ifdef _SERVER
KTeamSet	g_TeamSet;
#endif

#ifndef _SERVER
KPlayerTeam::KPlayerTeam()
{
	Release();
}
#endif

#ifndef _SERVER
void	KPlayerTeam::Release()
{
	m_nFlag = 0;
	m_nFigure = TEAM_CAPTAIN;
	m_nApplyCaptainID = -1;
	m_nApplyCaptainID = 0;
	m_dwApplyTimer = 0;
	m_bAutoRefuseInviteFlag = FALSE;	 // TRUE 自动拒绝   FALSE 手动
	ReleaseList();
}
#endif

#ifndef _SERVER
void	KPlayerTeam::ReleaseList()
{
	for (int i = 0; i < MAX_TEAM_APPLY_LIST; i++)
		m_sApplyList[i].Release();
}
#endif


#ifndef _SERVER
BOOL	KPlayerTeam::ApplyCreate()//char *lpszTeamName)
{
//	if (!lpszTeamName || !lpszTeamName[0])
//		return FALSE;
//	if (strlen(lpszTeamName) >= 32)
//		return FALSE;
	if (m_nFlag)
		return FALSE;

	PLAYER_APPLY_CREATE_TEAM	sCreateTeam;
	sCreateTeam.ProtocolType = c2s_teamapplycreate;
//	memset(sCreateTeam.m_szTeamName, 0, sizeof(sCreateTeam.m_szTeamName));
//	strcpy(sCreateTeam.m_szTeamName, lpszTeamName);
	if (g_pClient)
		g_pClient->SendPackToServer(&sCreateTeam, sizeof(PLAYER_APPLY_CREATE_TEAM));

	return TRUE;
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：邀请加入队伍
//---------------------------------------------------------------------------
void	KPlayerTeam::InviteAdd(DWORD dwNpcID)
{
//	if ( !this->m_nFlag || this->m_nFigure != TEAM_CAPTAIN || g_Team[0].m_nState != Team_S_Open)
//		return;
	TEAM_INVITE_ADD_COMMAND	sAdd;
	sAdd.ProtocolType = c2s_teaminviteadd;
	sAdd.m_dwNpcID = dwNpcID;
	if (g_pClient)
		g_pClient->SendPackToServer(&sAdd, sizeof(TEAM_INVITE_ADD_COMMAND));
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：收到邀请
//---------------------------------------------------------------------------
void	KPlayerTeam::ReceiveInvite(TEAM_INVITE_ADD_SYNC *pInvite)
{
	if ( !pInvite )
		return;
	char	szName[32];
	int		nIdx;
	memset(szName, 0, sizeof(szName));
	memcpy(szName, pInvite->m_szName, sizeof(pInvite->m_szName) - (sizeof(TEAM_INVITE_ADD_SYNC) - pInvite->m_wLength - 1));
	nIdx = pInvite->m_nIdx;
	if (m_bAutoRefuseInviteFlag)
	{
		ReplyInvite(nIdx, 0);
	}
	else
	{
		// 通知界面有人邀请玩家加入某个队伍
		KUiPlayerItem	sPlayer;
		KSystemMessage	sMsg;

		strcpy(sPlayer.Name, szName);
		sPlayer.nIndex = pInvite->m_nIdx;
		sPlayer.uId = 0;
		sPlayer.nData = 0;

		sprintf(sMsg.szMessage, MSG_TEAM_GET_INVITE, szName);
		sMsg.eType = SMT_TEAM;
		sMsg.byConfirmType = SMCT_UI_TEAM_INVITE;
		sMsg.byPriority = 3;
		sMsg.byParamSize = sizeof(KUiPlayerItem);
		CoreDataChanged(GDCNI_SYSTEM_MESSAGE, (unsigned int)&sMsg, (int)&sPlayer);
	}
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：回复邀请
//	参数：nResult  if == 0 拒绝  if == 1 接受
//---------------------------------------------------------------------------
void	KPlayerTeam::ReplyInvite(int nIdx, int nResult)
{
	if (nIdx < 0 || nResult < 0 || nResult > 1)
		return;
	TEAM_REPLY_INVITE_COMMAND	sReply;
	sReply.ProtocolType = c2s_teamreplyinvite;
	sReply.m_nIndex = nIdx;
	sReply.m_btResult = nResult;
	if (g_pClient)
		g_pClient->SendPackToServer(&sReply, sizeof(TEAM_REPLY_INVITE_COMMAND));
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：设定是否自动拒绝别人的加入队伍的邀请
//---------------------------------------------------------------------------
void	KPlayerTeam::SetAutoRefuseInvite(BOOL bFlag)
{
	KSystemMessage	sMsg;
	sMsg.eType = SMT_NORMAL;
	sMsg.byConfirmType = SMCT_NONE;
	sMsg.byPriority = 0;
	sMsg.byParamSize = 0;

	if (bFlag)
	{
		m_bAutoRefuseInviteFlag = TRUE;
		sprintf(sMsg.szMessage, MSG_TEAM_AUTO_REFUSE_INVITE);
	}
	else
	{
		m_bAutoRefuseInviteFlag = FALSE;
		sprintf(sMsg.szMessage, MSG_TEAM_NOT_AUTO_REFUSE_INVITE);
	}

	CoreDataChanged(GDCNI_SYSTEM_MESSAGE, (unsigned int)&sMsg, 0);
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：获得是否自动拒绝别人的加入队伍的邀请状态
//---------------------------------------------------------------------------
BOOL	KPlayerTeam::GetAutoRefuseState()
{
	return m_bAutoRefuseInviteFlag;
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：获得自身队伍信息（给界面）
//---------------------------------------------------------------------------
int		KPlayerTeam::GetInfo(KUiPlayerTeam *pTeam)
{
	if (!pTeam)
		return 0;
	pTeam->nCaptainPower = g_Team[0].CalcCaptainPower();

	if (m_nFlag == 0)
		return 0;

	if (m_nFigure == TEAM_CAPTAIN)
	{
		pTeam->bTeamLeader = true;
		int nNo = 0;
		for (int i = 0; i < MAX_TEAM_APPLY_LIST; i++)
		{
			if (this->m_sApplyList[i].m_dwNpcID > 0)
				nNo++;
		}
		pTeam->cNumTojoin = nNo;
	}
	else
	{
		pTeam->bTeamLeader = false;
		pTeam->cNumTojoin = 0;
	}
	pTeam->nTeamServerID = g_Team[0].m_nTeamServerID;
	pTeam->cNumMember = g_Team[0].m_nMemNum + 1;
	pTeam->bOpened = 0;
	if (g_Team[0].m_nState == Team_S_Open)
		pTeam->bOpened = 1;
	
	return 1;
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：更新界面显示
//---------------------------------------------------------------------------
void	KPlayerTeam::UpdateInterface()
{
	KUiPlayerTeam	sTeam;
	if (GetInfo(&sTeam) == 0)
	{
		CoreDataChanged(GDCNI_TEAM, NULL, 0);
	}
	else
	{
		CoreDataChanged(GDCNI_TEAM, (unsigned int)&sTeam, 0);
	}
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：从申请人列表中删除某个申请人
//---------------------------------------------------------------------------
void	KPlayerTeam::DeleteOneFromApplyList(DWORD dwNpcID)
{
	for (int i = 0; i < MAX_TEAM_APPLY_LIST; i++)
	{
		if (m_sApplyList[i].m_dwNpcID == dwNpcID)
		{
			m_sApplyList[i].Release();
			return;
		}
	}
}
#endif

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：清空
//---------------------------------------------------------------------------
void	KPlayerTeam::Release()
{
	m_nFlag = 0;
	m_nID = -1;
	m_nFigure = 0;
	m_nApplyCaptainID = 0;
	memset(m_nInviteList, 0, sizeof(m_nInviteList));
	memset(m_nInviteKind, 0, sizeof(m_nInviteKind));
	memset(m_dwInviteNpcID, 0, sizeof(m_dwInviteNpcID));
	memset(m_dwInviteTime, 0, sizeof(m_dwInviteTime));
	m_nListPos = 0;
}
#endif

#ifdef _SERVER
BOOL	KPlayerTeam::CreateTeam(int nIdx, PLAYER_APPLY_CREATE_TEAM *pCreateTeam)
{
	if (!pCreateTeam || nIdx <= 0 || nIdx >= MAX_PLAYER || !m_bCanTeamFlag ||
		m_nFlag || !Player[nIdx].m_bFinishLoading || Player[nIdx].m_bExchangeServer ||
		Player[nIdx].CheckTrading() || Player[nIdx].m_nIndex <= 0 || Player[nIdx].m_nIndex >= MAX_NPC)
		return FALSE;
	int nTeam = g_TeamSet.CreateTeam(nIdx);
	if (nTeam < 0)
		return FALSE;
	Release();
	m_nID = nTeam;
	m_nFlag = 1;
	m_nFigure = TEAM_CAPTAIN;
	Npc[Player[nIdx].m_nIndex].RestoreCurrentCamp();
	PLAYER_SEND_CREATE_TEAM_SUCCESS Success;
	Success.ProtocolType = 0x65;
	Success.nTeamServerID = nTeam;
	g_pServer->PackDataToClient(Player[nIdx].m_nNetConnectIdx, &Success, sizeof(Success));
	g_Team[nTeam].SetTeamOpen();
	Player[nIdx].SendCurNormalSyncData();
	return TRUE;
}
#endif

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：转发邀请加入队伍
//---------------------------------------------------------------------------
void	KPlayerTeam::InviteAdd(int nIdx, TEAM_INVITE_ADD_COMMAND *pAdd)
{
	if (!pAdd || nIdx <= 0 || nIdx >= MAX_PLAYER || !m_nFlag || m_nFigure != TEAM_CAPTAIN ||
		m_nID < 0 || m_nID >= MAX_TEAM || g_Team[m_nID].m_nCaptain != nIdx ||
		!g_Team[m_nID].IsOpen() || g_Team[m_nID].CheckFull())
		return;
	if (!Player[nIdx].m_bFinishLoading || Player[nIdx].m_bExchangeServer || Player[nIdx].CheckTrading())
		return;
	int nTarget = Player[nIdx].FindAroundPlayer(pAdd->m_dwNpcID);
	if (nTarget <= 0 || nTarget >= MAX_PLAYER || nTarget == nIdx ||
		Player[nTarget].m_nIndex <= 0 || !Player[nTarget].m_bFinishLoading ||
		Player[nTarget].m_bExchangeServer || Player[nTarget].m_cTeam.m_nFlag ||
		!Player[nTarget].m_cTeam.GetCanTeamFlag() || Player[nTarget].CheckTrading())
		return;
	// Reinviting refreshes one pending request; it must not create replayable duplicates.
	for (int i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		if (m_nInviteList[i] == nTarget)
			m_nInviteList[i] = 0;
	}
	m_nInviteList[m_nListPos] = nTarget;
	m_nInviteKind[m_nListPos] = 1;
	m_dwInviteNpcID[m_nListPos] = Npc[Player[nTarget].m_nIndex].m_dwID;
	m_dwInviteTime[m_nListPos] = g_SubWorldSet.GetGameTime();
	m_nListPos = (m_nListPos + 1) % MAX_TEAM_MEMBER;
	TEAM_INVITE_ADD_SYNC Invite;
	memset(&Invite, 0, sizeof(Invite));
	Invite.ProtocolType = 0x8B;
	Invite.m_nIdx = nIdx;
	g_StrCpyLen(Invite.m_szName, Npc[Player[nIdx].m_nIndex].Name, sizeof(Invite.m_szName));
	Invite.m_wLength = sizeof(Invite) - 1 - sizeof(Invite.m_szName) + strlen(Invite.m_szName);
	g_pServer->PackDataToClient(Player[nTarget].m_nNetConnectIdx, &Invite, Invite.m_wLength + 1);
}
#endif

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：收到邀请加入队伍的回复
//---------------------------------------------------------------------------
void	KPlayerTeam::GetInviteReply(int nSelfIdx, int nTargetIdx, int nResult, BOOL bApplication)
{
	if (nSelfIdx <= 0 || nSelfIdx >= MAX_PLAYER || nTargetIdx <= 0 || nTargetIdx >= MAX_PLAYER ||
		nResult < 0 || nResult > 1 || !m_nFlag || m_nFigure != TEAM_CAPTAIN ||
		m_nID < 0 || m_nID >= MAX_TEAM || g_Team[m_nID].m_nCaptain != nSelfIdx ||
		!g_Team[m_nID].IsOpen() || Player[nTargetIdx].m_nIndex <= 0)
		return;
	int nSlot;
	for (nSlot = 0; nSlot < MAX_TEAM_MEMBER; nSlot++)
	{
		if (m_nInviteList[nSlot] == nTargetIdx &&
			m_nInviteKind[nSlot] == (bApplication ? 2 : 1) &&
			m_dwInviteNpcID[nSlot] == Npc[Player[nTargetIdx].m_nIndex].m_dwID)
			break;
	}
	if (nSlot == MAX_TEAM_MEMBER)
		return;
	DWORD dwAge = g_SubWorldSet.GetGameTime() - m_dwInviteTime[nSlot];
	m_nInviteList[nSlot] = 0;
	m_nInviteKind[nSlot] = 0;
	m_dwInviteNpcID[nSlot] = 0;
	m_dwInviteTime[nSlot] = 0;
	// Invitation is consumed once, expires after the native apply timeout.
	if (!nResult || dwAge > 500 || !Player[nTargetIdx].m_bFinishLoading ||
		Player[nTargetIdx].m_bExchangeServer || Player[nTargetIdx].CheckTrading() ||
		!Player[nTargetIdx].m_cTeam.GetCanTeamFlag() || Player[nTargetIdx].m_cTeam.m_nFlag ||
		Player[nSelfIdx].FindAroundPlayer(Npc[Player[nTargetIdx].m_nIndex].m_dwID) != nTargetIdx ||
		!g_Team[m_nID].AddMember(nTargetIdx))
		return;
	Player[nTargetIdx].m_cTeam.Release();
	Player[nTargetIdx].m_cTeam.m_nFlag = 1;
	Player[nTargetIdx].m_cTeam.m_nFigure = TEAM_MEMBER;
	Player[nTargetIdx].m_cTeam.m_nID = m_nID;
	Npc[Player[nTargetIdx].m_nIndex].SetCurrentCamp(Npc[Player[nSelfIdx].m_nIndex].m_Camp);
	GAME_TEAM_MEMBER_SYNC Member;
	memset(&Member, 0, sizeof(Member));
	Member.ProtocolType = 0x69;
	Member.NpcID = Npc[Player[nTargetIdx].m_nIndex].m_dwID;
	Member.Level = (BYTE)Npc[Player[nTargetIdx].m_nIndex].m_Level;
	g_StrCpyLen(Member.Name, Npc[Player[nTargetIdx].m_nIndex].Name, sizeof(Member.Name));
	g_pServer->PackDataToClient(Player[nSelfIdx].m_nNetConnectIdx, &Member, sizeof(Member));
	for (int i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		int nPlayer = g_Team[m_nID].m_nMember[i];
		if (nPlayer > 0 && nPlayer != nTargetIdx)
			g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, &Member, sizeof(Member));
	}
	if (g_Team[m_nID].CheckFull())
		g_Team[m_nID].SetTeamClose();
	g_Team[m_nID].SendSelfInfo(nTargetIdx);
	Player[nTargetIdx].SendCurNormalSyncData();
}
#endif

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：收到邀请加入队伍的回复
//---------------------------------------------------------------------------
void	KPlayerTeam::SetCanTeamFlag(int nSelfIdx, BOOL bFlag)
{
	if (bFlag)
	{
		m_bCanTeamFlag = TRUE;
		return;
	}

	m_bCanTeamFlag = FALSE;
	if (nSelfIdx >= 0 && nSelfIdx < MAX_PLAYER)
	{
		PLAYER_APPLY_LEAVE_TEAM	sLeaveTeam;
		sLeaveTeam.ProtocolType = c2s_teamapplyleave;
		Player[nSelfIdx].LeaveTeam((BYTE*)&sLeaveTeam);
	}
}
#endif

//---------------------------------------------------------------------------
//	功能：构造函数
//---------------------------------------------------------------------------
KTeam::KTeam()
{
	Release();
}

//---------------------------------------------------------------------------
//	功能：清空
//---------------------------------------------------------------------------
void	KTeam::Release()
{
	m_nCaptain = -1;
	m_nMemNum = 0;
	int		i;
	for (i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		m_nMember[i] = -1;
	}
	m_nState = Team_S_Close;
#ifdef _SERVER
	m_nAllocationMode = 0x10;
	m_nPickupMode = 0;
#endif
#ifndef _SERVER
	for (i = 0; i < MAX_TEAM_MEMBER + 1; i++)
	{
		m_nMemLevel[i] = 0;
		memset(m_szMemName, 0, 32 * (MAX_TEAM_MEMBER + 1));
	}
	m_nTeamServerID = -1;
#endif
}

//---------------------------------------------------------------------------
//	功能：设定 Team 在 g_Team 中的位置
//---------------------------------------------------------------------------
void	KTeam::SetIndex(int nIndex)
{
	m_nIndex = nIndex;
}

//---------------------------------------------------------------------------
//	功能：寻找队员空位
//---------------------------------------------------------------------------
int		KTeam::FindFree()
{
	for (int i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		if (m_nMember[i] < 0)
			return i;
	}
	return -1;
}

//---------------------------------------------------------------------------
//	功能：寻找具有指定npc id的队员（不包括队长）
//	返回值：队员在 m_nMember 数组中的位置
//---------------------------------------------------------------------------
int		KTeam::FindMemberID(DWORD dwNpcID)
{
	for (int i = 0; i <	MAX_TEAM_MEMBER; i++)
	{
#ifdef _SERVER
		if (m_nMember[i] >= 0 && Npc[Player[m_nMember[i]].m_nIndex].m_dwID == dwNpcID)
			return i;
#else
		if (m_nMember[i] > 0 && (DWORD)m_nMember[i] == dwNpcID)
			return i;
#endif
	}
	return -1;
}

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：判断队伍是否已经满员
//	返回值：FALSE 未满员  TRUE 已满员
//---------------------------------------------------------------------------
BOOL	KTeam::CheckFull()
{
	if (m_nMemNum < CalcCaptainPower())
		return FALSE;
	return TRUE;
}
#endif

//---------------------------------------------------------------------------
//	功能：设定队伍状态：打开（允许接受新成员）
//---------------------------------------------------------------------------
BOOL	KTeam::SetTeamOpen()
{
#ifdef _SERVER
	if (m_nCaptain <= 0 || m_nCaptain >= MAX_PLAYER)
		return FALSE;
	if (CheckFull())
		return FALSE;
	m_nState = Team_S_Open;
	SendDistribution(0);
	Npc[Player[m_nCaptain].m_nIndex].SendMenuStateSync();
	return TRUE;
#else
	m_nState = Team_S_Open;
	Player[CLIENT_PLAYER_INDEX].m_cTeam.UpdateInterface();
	return TRUE;
#endif
}

//---------------------------------------------------------------------------
//	功能：设定队伍状态：关闭（不允许接受新成员）
//---------------------------------------------------------------------------
BOOL	KTeam::SetTeamClose()
{
#ifdef _SERVER
	if (m_nCaptain <= 0 || m_nCaptain >= MAX_PLAYER)
		return FALSE;
	m_nState = Team_S_Close;
	memset(Player[m_nCaptain].m_cTeam.m_nInviteList, 0, sizeof(Player[m_nCaptain].m_cTeam.m_nInviteList));
	SendDistribution(0);
	Npc[Player[m_nCaptain].m_nIndex].SendMenuStateSync();
	return TRUE;
#else
	m_nState = Team_S_Close;
	Player[CLIENT_PLAYER_INDEX].m_cTeam.UpdateInterface();
	return TRUE;
#endif
}

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：判断队伍是否是打开状态
//---------------------------------------------------------------------------
BOOL	KTeam::IsOpen()
{
	return m_nState != Team_S_Close;
}
#endif

//---------------------------------------------------------------------------
//	功能：计算队长能统帅队员的人数
//---------------------------------------------------------------------------
int		KTeam::CalcCaptainPower()
{
#ifdef _SERVER
	return MAX_TEAM_MEMBER;
#else
	return PlayerSet.m_cLeadExp.GetMemNumFromLevel(Player[CLIENT_PLAYER_INDEX].m_dwLeadLevel);
#endif
}

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：判断加入队伍的属性条件是否合适
//---------------------------------------------------------------------------
BOOL	KTeam::CheckAddCondition(int nPlayerIndex)
{
	if (nPlayerIndex <= 0 || nPlayerIndex >= MAX_PLAYER ||
		m_nCaptain <= 0 || m_nCaptain >= MAX_PLAYER ||
		Player[nPlayerIndex].m_nIndex <= 0 || Player[nPlayerIndex].m_nIndex >= MAX_NPC ||
		Player[m_nCaptain].m_nIndex <= 0 || Player[m_nCaptain].m_nIndex >= MAX_NPC)
		return FALSE;
	if (Npc[Player[nPlayerIndex].m_nIndex].m_Camp != camp_begin && Npc[Player[m_nCaptain].m_nIndex].m_Camp == camp_begin)
		return FALSE;
	return TRUE;
}
#endif

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：创建一支队伍
//---------------------------------------------------------------------------
BOOL	KTeam::CreateTeam(int nPlayerIndex)//, char *lpszName)
{
	Release();
	m_nCaptain = nPlayerIndex;
//	if (lpszName && lpszName[0])
//		strcpy(m_szName, lpszName);

	return TRUE;
}
#endif

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：添加一个队伍成员
//---------------------------------------------------------------------------
BOOL	KTeam::AddMember(int nPlayerIndex)
{
	if (nPlayerIndex <= 0 || nPlayerIndex >= MAX_PLAYER ||
		Player[nPlayerIndex].m_cTeam.m_nFlag || CheckIn(nPlayerIndex))
		return FALSE;
	// 队伍是否为空
	if (m_nCaptain < 0)
		return FALSE;
	// 阵营是否相同
	if ( !CheckAddCondition(nPlayerIndex) )
		return FALSE;
	// 是否已经满员
	if (CheckFull())
		return FALSE;

	int n;
	n = FindFree();
	if (n < 0)
		return FALSE;
	m_nMember[n] = nPlayerIndex;
	m_nMemNum++;

	return TRUE;
}
#endif

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：删除一个队伍成员
//---------------------------------------------------------------------------
BOOL	KTeam::DeleteMember(int nPlayerIndex)
{
	if (nPlayerIndex <= 0 || nPlayerIndex >= MAX_PLAYER ||
		m_nCaptain <= 0 || m_nCaptain >= MAX_PLAYER || !CheckIn(nPlayerIndex) ||
		Player[nPlayerIndex].m_nIndex <= 0 || Player[nPlayerIndex].m_nIndex >= MAX_NPC ||
		!Player[nPlayerIndex].m_cTeam.m_nFlag ||
		Player[nPlayerIndex].m_cTeam.m_nID != m_nIndex)
		return FALSE;

	if (nPlayerIndex == m_nCaptain)
	{
		int nSuccessor = -1;
		for (int i = 0; i < MAX_TEAM_MEMBER; i++)
		{
			int nMember = m_nMember[i];
			if (nMember > 0 && nMember < MAX_PLAYER &&
				Player[nMember].m_cTeam.m_nFlag && Player[nMember].m_cTeam.m_nID == m_nIndex &&
				Player[nMember].m_nIndex > 0 && Player[nMember].m_nIndex < MAX_NPC)
			{
				nSuccessor = i;
				break;
			}
		}
		if (nSuccessor >= 0)
		{
			// The capture sends 6B before 6A and keeps the allocation settings.
			// Departure must finish even when a manual transfer would fail camp checks.
			m_nCaptain = m_nMember[nSuccessor];
			m_nMember[nSuccessor] = nPlayerIndex;
			Player[nPlayerIndex].m_cTeam.m_nFigure = TEAM_MEMBER;
			Player[m_nCaptain].m_cTeam.m_nFigure = TEAM_CAPTAIN;
			memset(Player[m_nCaptain].m_cTeam.m_nInviteList, 0, sizeof(Player[m_nCaptain].m_cTeam.m_nInviteList));
			Npc[Player[m_nCaptain].m_nIndex].RestoreCurrentCamp();
			GAME_TEAM_CHANGE_CAPTAIN_SYNC Change;
			Change.ProtocolType = 0x6B;
			Change.m_dwCaptainID = Npc[Player[m_nCaptain].m_nIndex].m_dwID;
			Change.m_dwMemberID = Npc[Player[nPlayerIndex].m_nIndex].m_dwID;
			Change.State = (BYTE)m_nState;
			g_pServer->PackDataToClient(Player[m_nCaptain].m_nNetConnectIdx, &Change, sizeof(Change));
			Player[m_nCaptain].SendCurNormalSyncData();
			for (int i = 0; i < MAX_TEAM_MEMBER; i++)
			{
				int nMember = m_nMember[i];
				if (nMember <= 0 || nMember >= MAX_PLAYER)
					continue;
				memset(Player[nMember].m_cTeam.m_nInviteList, 0, sizeof(Player[nMember].m_cTeam.m_nInviteList));
				Npc[Player[nMember].m_nIndex].SetCurrentCamp(Npc[Player[m_nCaptain].m_nIndex].m_Camp);
				g_pServer->PackDataToClient(Player[nMember].m_nNetConnectIdx, &Change, sizeof(Change));
				Player[nMember].SendCurNormalSyncData();
			}
			Npc[Player[m_nCaptain].m_nIndex].NormalSync();
			Npc[Player[m_nCaptain].m_nIndex].SendMenuStateSync();
		}
		else
		{
			GAME_TEAM_LEAVE_SYNC Leave;
			Leave.ProtocolType = 0x6A;
			Leave.m_dwNpcID = Npc[Player[nPlayerIndex].m_nIndex].m_dwID;
			Player[nPlayerIndex].m_cTeam.Release();
			Npc[Player[nPlayerIndex].m_nIndex].RestoreCurrentCamp();
			g_pServer->PackDataToClient(Player[nPlayerIndex].m_nNetConnectIdx, &Leave, sizeof(Leave));
			Player[nPlayerIndex].SendCurNormalSyncData();
			Npc[Player[nPlayerIndex].m_nIndex].NormalSync();
			Npc[Player[nPlayerIndex].m_nIndex].SendMenuStateSync();
			Release();
			return TRUE;
		}
	}

	int nSlot = -1;
	for (int i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		if (m_nMember[i] == nPlayerIndex)
		{
			nSlot = i;
			break;
		}
	}
	if (nSlot < 0)
		return FALSE;
	m_nMember[nSlot] = -1;
	m_nMemNum--;

	GAME_TEAM_LEAVE_SYNC Leave;
	Leave.ProtocolType = 0x6A;
	Leave.m_dwNpcID = Npc[Player[nPlayerIndex].m_nIndex].m_dwID;
	Player[nPlayerIndex].m_cTeam.Release();
	Npc[Player[nPlayerIndex].m_nIndex].RestoreCurrentCamp();
	g_pServer->PackDataToClient(Player[nPlayerIndex].m_nNetConnectIdx, &Leave, sizeof(Leave));
	g_pServer->PackDataToClient(Player[m_nCaptain].m_nNetConnectIdx, &Leave, sizeof(Leave));
	for (int i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		int nMember = m_nMember[i];
		if (nMember > 0 && nMember < MAX_PLAYER)
			g_pServer->PackDataToClient(Player[nMember].m_nNetConnectIdx, &Leave, sizeof(Leave));
	}
	Player[nPlayerIndex].SendCurNormalSyncData();
	Npc[Player[nPlayerIndex].m_nIndex].NormalSync();
	Npc[Player[nPlayerIndex].m_nIndex].SendMenuStateSync();
	return TRUE;
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：客户端创建一支队伍（客户端只可能存在一支的队伍，属于本地玩家）
//---------------------------------------------------------------------------
void	KTeam::CreateTeam(int nCaptainNpcID, char *lpszCaptainName, int nCaptainLevel, DWORD nTeamServerID)
{
	Release();
	m_nCaptain = nCaptainNpcID;
	m_nMemLevel[0] = nCaptainLevel;
	strcpy(m_szMemName[0], lpszCaptainName);
	m_nTeamServerID = nTeamServerID;		// 队伍在服务器上的唯一标识
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：添加一个队伍成员
//---------------------------------------------------------------------------
BOOL	KTeam::AddMember(DWORD dwNpcID, int nLevel, char *lpszNpcName)
{
	for (int i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		if (m_nMember[i] == (int)dwNpcID)
			return TRUE;
	}

	int nFreeNo;
	nFreeNo = FindFree();
	if (nFreeNo < 0)
		return FALSE;
	m_nMember[nFreeNo] = dwNpcID;
	m_nMemLevel[nFreeNo + 1] = nLevel;
	strcpy(m_szMemName[nFreeNo + 1], lpszNpcName);
	m_nMemNum++;

	return TRUE;
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：客户端删除一个队伍成员
//---------------------------------------------------------------------------
void	KTeam::DeleteMember(DWORD dwNpcID)
{
	if (dwNpcID == (DWORD)m_nCaptain)
	{
		Release();
		return;
	}

	for (int i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		if ((DWORD)m_nMember[i] == dwNpcID)
		{
			m_nMemNum--;
			m_nMember[i] = -1;
			m_nMemLevel[i + 1] = 0;
			m_szMemName[i + 1][0] = 0;
			return;
		}
	}
}
#endif

#ifndef _SERVER
//---------------------------------------------------------------------------
//	功能：输出队员信息(按界面的要求，给界面)
//---------------------------------------------------------------------------
int		KTeam::GetMemberInfo(KUiPlayerItem *pList, int nCount)
{
	if (this->m_nCaptain < 0)
		return 0;
	int		i, nNum = 0;
	if (!pList || nCount <= m_nMemNum)
	{
		return m_nMemNum + 1;
	}
	
	// 队长名字
	strcpy(pList[nNum].Name, m_szMemName[0]);
	pList[nNum].uId = this->m_nCaptain;
	pList[nNum].nIndex = 0;
	nNum++;

	for (i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		if (nNum >= nCount)
			break;
		if (this->m_nMember[i] <= 0)
			continue;
		strcpy(pList[nNum].Name, m_szMemName[i + 1]);
		pList[nNum].uId = m_nMember[i];
		pList[nNum].nIndex = 0;
		nNum++;
	}

	return nNum;
}
#endif

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：判断某人是否是队伍成员
//---------------------------------------------------------------------------
BOOL	KTeam::CheckIn(int nPlayerIndex)
{
	if (nPlayerIndex <= 0)
		return FALSE;
	if (nPlayerIndex == m_nCaptain)
		return TRUE;
	for (int i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		if (m_nMember[i] == nPlayerIndex)
			return TRUE;
	}

	return FALSE;
}
#endif

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：初始化
//---------------------------------------------------------------------------
void	KTeamSet::Init()
{
	for (int i = 0; i < MAX_TEAM; i++)
		g_Team[i].SetIndex(i);
}
#endif

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：差找 g_Team 中的空位
//---------------------------------------------------------------------------
int		KTeamSet::FindFree()
{
	for (int i = 0; i < MAX_TEAM; i++)
	{
		if (g_Team[i].m_nCaptain < 0)
			return i;
	}
	return -1;
}
#endif

/*
#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：判断队名是否可用
//---------------------------------------------------------------------------
BOOL	KTeamSet::CheckName(char *lpszName)
{
	if (!lpszName || !lpszName[0])
		return FALSE;

	for (int i = 0; i < MAX_TEAM; i++)
	{
		if (g_Team[i].m_nCaptain < 0)
			continue;
		if (strcmp(lpszName, g_Team[i].m_szName) == 0)
			return FALSE;
	}

	return TRUE;
}
#endif
*/

#ifdef _SERVER
//---------------------------------------------------------------------------
//	功能：创建一支队伍
//---------------------------------------------------------------------------
int		KTeamSet::CreateTeam(int nPlayerID)//, char *lpszName)
{
	int nTeamID;
	nTeamID = FindFree();
	if (nTeamID < 0)
		return -2;
//	if (!CheckName(lpszName))
//		return -1;
	if (g_Team[nTeamID].CreateTeam(nPlayerID))//, lpszName))
		return nTeamID;
	return -1;
}
#endif























#ifdef _SERVER
void KTeam::Broadcast(const void* pData, int nSize)
{
	if (!g_pServer || m_nCaptain <= 0 || m_nCaptain >= MAX_PLAYER)
		return;
	g_pServer->PackDataToClient(Player[m_nCaptain].m_nNetConnectIdx, pData, nSize);
	for (int i = 0; i < MAX_TEAM_MEMBER; i++)
	{
		int nPlayer = m_nMember[i];
		if (nPlayer > 0 && nPlayer < MAX_PLAYER)
			g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, pData, nSize);
	}
}

void KTeam::SendDistribution(int nPlayer)
{
	BYTE Mode[] = {0x67, (BYTE)m_nAllocationMode, (BYTE)m_nPickupMode};
	BYTE State[] = {0x67, (BYTE)m_nState, (BYTE)m_nPickupMode};
	if (nPlayer > 0 && nPlayer < MAX_PLAYER)
	{
		g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, Mode, sizeof(Mode));
		g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, State, sizeof(State));
	}
	else
	{
		Broadcast(State, sizeof(State));
		Broadcast(Mode, sizeof(Mode));
	}
}

BOOL KTeam::SetDistribution(int nPlayer, int nMode, int nPickup)
{
	if (nPlayer != m_nCaptain || !Player[nPlayer].m_cTeam.m_nFlag ||
		Player[nPlayer].m_cTeam.m_nID != m_nIndex ||
		(nPickup < 0 || nPickup > 2))
		return FALSE;
	if (nMode == 0x10 || nMode == 0x20)
		m_nAllocationMode = nMode;
	else if (nMode >= 0 && nMode <= 2)
	{
		m_nState = nMode;
		if (!nMode)
			memset(Player[m_nCaptain].m_cTeam.m_nInviteList, 0, sizeof(Player[m_nCaptain].m_cTeam.m_nInviteList));
		SendDistribution(0);
		Npc[Player[m_nCaptain].m_nIndex].SendMenuStateSync();
		return TRUE;
	}
	else
		return FALSE;
	m_nPickupMode = nPickup;
	SendDistribution(0);
	Npc[Player[m_nCaptain].m_nIndex].SendMenuStateSync();
	return TRUE;
}

void KTeam::SendSelfInfo(int nPlayer)
{
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER || !CheckIn(nPlayer))
		return;
	GAME_TEAM_SELF_SYNC Sync;
	memset(&Sync, 0, sizeof(Sync));
	Sync.ProtocolType = 0x63;
	Sync.State = (BYTE)(m_nAllocationMode | m_nState);
	Sync.TeamID = m_nIndex;
	for (int i = 0; i <= MAX_TEAM_MEMBER; i++)
	{
		int nMember = i == 0 ? m_nCaptain : m_nMember[i - 1];
		if (nMember <= 0 || nMember >= MAX_PLAYER)
			continue;
		int nNpc = Player[nMember].m_nIndex;
		if (nNpc <= 0 || nNpc >= MAX_NPC)
			continue;
		Sync.NpcID[i] = Npc[nNpc].m_dwID;
		Sync.Level[i] = (BYTE)Npc[nNpc].m_Level;
		g_StrCpyLen(Sync.Name[i], Npc[nNpc].Name, sizeof(Sync.Name[i]));
	}
	g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, &Sync, sizeof(Sync));
	SendDistribution(nPlayer);
	SendHealthInfo(nPlayer);
	SendPositionInfo(nPlayer);
}
#endif

#ifdef _SERVER
BOOL KTeam::CanPickUp(int nPlayer, int nOwner, BOOL bTask)
{
	if (nOwner == -1)
		return TRUE;
	if (!CheckIn(nPlayer))
		return FALSE;
	if (bTask)
		return nOwner == nPlayer;
	if (!CheckIn(nOwner))
		return FALSE;
	return m_nPickupMode != 1 || nPlayer == m_nCaptain;
}
#endif

#ifdef _SERVER
void KTeam::SendHealthInfo(int nPlayer)
{
	if (!g_pServer || nPlayer <= 0 || nPlayer >= MAX_PLAYER ||
		!Player[nPlayer].m_bFinishLoading || Player[nPlayer].m_bExchangeServer ||
		Player[nPlayer].m_nNetConnectIdx == -1 ||
		!Player[nPlayer].m_cTeam.m_nFlag || Player[nPlayer].m_cTeam.m_nID != m_nIndex ||
		!CheckIn(nPlayer))
		return;

	GAME_TEAM_HEALTH_SYNC Sync;
	memset(&Sync, 0, sizeof(Sync));
	Sync.ProtocolType = 0xB8;
	int nCount = 0;
	for (int i = 0; i <= MAX_TEAM_MEMBER; i++)
	{
		int nMember = i == 0 ? m_nCaptain : m_nMember[i - 1];
		if (nMember <= 0 || nMember >= MAX_PLAYER)
			continue;
		KPlayer& Member = Player[nMember];
		int nNpc = Member.m_nIndex;
		if (!Member.m_cTeam.m_nFlag || Member.m_cTeam.m_nID != m_nIndex ||
			!Member.m_bFinishLoading || Member.m_bExchangeServer || Member.m_nNetConnectIdx == -1 ||
			nNpc <= 0 || nNpc >= MAX_NPC || Npc[nNpc].m_bExchangeServer ||
			Npc[nNpc].m_RegionIndex < 0 || Npc[nNpc].m_SubWorldIndex < 0)
			continue;
		GAME_TEAM_HEALTH_ENTRY& Entry = Sync.Member[nCount++];
		Entry.NpcID = Npc[nNpc].m_dwID;
		Entry.LifeMax = (DWORD)max(0, Npc[nNpc].m_CurrentLifeMax);
		Entry.Life = (DWORD)max(0, min(Npc[nNpc].m_CurrentLife, (int)Entry.LifeMax));
	}
	if (!nCount)
		return;
	Sync.Length = (WORD)(sizeof(Sync.Length) + nCount * sizeof(GAME_TEAM_HEALTH_ENTRY));
	g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, &Sync, Sync.Length + 1);
}
#endif

#ifdef _SERVER
void KTeam::SendPositionInfo(int nPlayer)
{
	// Party map positions must reach members outside the NPC broadcast region.
	if (!g_pServer || nPlayer <= 0 || nPlayer >= MAX_PLAYER ||
		!Player[nPlayer].m_bFinishLoading || Player[nPlayer].m_bExchangeServer ||
		Player[nPlayer].m_nNetConnectIdx == -1 ||
		!Player[nPlayer].m_cTeam.m_nFlag || Player[nPlayer].m_cTeam.m_nID != m_nIndex ||
		!CheckIn(nPlayer))
		return;

	GAME_TEAM_POSITION_SYNC Sync;
	memset(&Sync, 0, sizeof(Sync));
	Sync.ProtocolType = 0xF8;
	Sync.SubType = 0x1A;
	int nCount = 0;
	for (int i = 0; i <= MAX_TEAM_MEMBER; i++)
	{
		int nMember = i == 0 ? m_nCaptain : m_nMember[i - 1];
		if (nMember <= 0 || nMember >= MAX_PLAYER)
			continue;
		KPlayer& Member = Player[nMember];
		int nNpc = Member.m_nIndex;
		if (!Member.m_cTeam.m_nFlag || Member.m_cTeam.m_nID != m_nIndex ||
			!Member.m_bFinishLoading || Member.m_bExchangeServer || Member.m_nNetConnectIdx == -1 ||
			nNpc <= 0 || nNpc >= MAX_NPC || Npc[nNpc].m_bExchangeServer ||
			Npc[nNpc].m_RegionIndex < 0 || Npc[nNpc].m_SubWorldIndex < 0 ||
			Npc[nNpc].m_SubWorldIndex >= MAX_SUBWORLD)
			continue;
		int nX = 0;
		int nY = 0;
		Npc[nNpc].GetMpsPos(&nX, &nY);
		GAME_TEAM_POSITION_ENTRY& Entry = Sync.Member[nCount++];
		Entry.NpcID = Npc[nNpc].m_dwID;
		Entry.MapID = SubWorld[Npc[nNpc].m_SubWorldIndex].m_SubWorldID;
		Entry.X = nX;
		Entry.Y = nY;
	}
	if (!nCount)
		return;
	Sync.Length = (WORD)(sizeof(Sync.Length) + sizeof(Sync.SubType) +
		nCount * sizeof(GAME_TEAM_POSITION_ENTRY));
	g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, &Sync, Sync.Length + 1);
}
#endif
