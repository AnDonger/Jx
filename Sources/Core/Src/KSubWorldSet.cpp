#include "KCore.h"
#include <errno.h>
#include "KObjSet.h"
#include "KNpcSet.h"
#include "KSubWorld.h"
#include "KNpc.h"
#include "KIniFile.h"
#include "KSubWorldSet.h"
#include "KMissleSet.h"
#include "LuaFuns.h"
#include "KNpcTemplate.h"
#include "KPlayerSet.h"
#include "KPlayer.h"
extern KNpcTemplate	* g_pNpcTemplate[MAX_NPCSTYLE][MAX_NPC_LEVEL]; //0,0为起点
KSubWorldSet g_SubWorldSet;

KSubWorldSet::KSubWorldSet()
{
	m_nLoopRate = 0;
	m_nGameVersion = ITEM_VERSION;
#ifndef _SERVER
	m_dwPing = 0;
#endif
}

int KSubWorldSet::SearchWorld(DWORD dwID)
{
	for (int i = 0; i < MAX_SUBWORLD; i++)
	{
		if ((DWORD)SubWorld[i].m_SubWorldID == dwID)
			return i;
	}
	return -1;
}

BOOL KSubWorldSet::Load(LPSTR szFileName)
{
	if (SubWorld[0].m_SubWorldID != -1)
		return FALSE;
	KIniFile IniFile;
	char szKeyName[256];
	int nWorldID[MAX_SUBWORLD];
	int nWorldCount = 0;
	int nLoadedCount = 0;
	BOOL bMapList = TRUE;

	if (!IniFile.Load(szFileName))
	{
		// 新地图清单不存在时，才兼容旧版部署清单。
		if (!IniFile.Load("\\maps\\WorldSet.ini"))
		{
			printf("[MapList] Cannot open %s or legacy WorldSet.ini.\n", szFileName);
			return FALSE;
		}
		bMapList = FALSE;
		printf("[MapList] Using legacy WorldSet.ini.\n");
	}

	int nLegacyCount = 0;
	if (IniFile.GetInteger("Init", "Count", 0, &nLegacyCount))
		bMapList = FALSE;

	if (bMapList)
	{
		szKeyName[0] = 0;
		while (IniFile.GetNextKey("List", szKeyName, szKeyName))
		{
			char* pEnd;
			errno = 0;
			long nId = strtol(szKeyName, &pEnd, 10);
			if (errno == ERANGE)
				return FALSE;
			if (szKeyName[0] < '0' || szKeyName[0] > '9' || *pEnd || nId <= 0)
				continue;
			if (nWorldCount >= MAX_SUBWORLD)
			{
				printf("[MapList] Too many maps in %s (limit %d).\n", szFileName, MAX_SUBWORLD);
				return FALSE;
			}
			nWorldID[nWorldCount++] = nId;
		}
	}
	else
	{
		if (!IniFile.GetInteger("Init", "Count", 0, &nWorldCount) ||
			nWorldCount <= 0 || nWorldCount > MAX_SUBWORLD)
			return FALSE;
		for (int i = 0; i < nWorldCount; i++)
		{
			sprintf(szKeyName, "World%02d", i);
			if (!IniFile.GetInteger("World", szKeyName, 0, &nWorldID[i]) || nWorldID[i] <= 0)
				return FALSE;
		}
	}

#ifdef _SERVER
	for (int i = 0; i < nWorldCount; i++)
	{
		if (SearchWorld(nWorldID[i]) >= 0)
			continue;
		SubWorld[nLoadedCount].m_nIndex = nLoadedCount;
		if (SubWorld[nLoadedCount].LoadMap(nWorldID[i]))
			nLoadedCount++;
		else
			printf("[MapList] Map %d was not loaded.\n", nWorldID[i]);
	}
	printf("[MapList] Loaded %d of %d maps from %s.\n", nLoadedCount, nWorldCount,
		bMapList ? szFileName : "\\maps\\WorldSet.ini");
	fflush(stdout);
	return nLoadedCount > 0;
#else
	return nWorldCount > 0;
#endif
}

int nActiveRegionCount;

void KSubWorldSet::MainLoop()
{
	m_nLoopRate++;

#ifndef _SERVER
//	if (!(m_nLoopRate % 20))
//		SendClientCmdPing();
	// 游戏音乐的处理
	this->m_cMusic.Play(SubWorld[0].m_SubWorldID, SubWorld[0].m_dwCurrentTime, Npc[Player[CLIENT_PLAYER_INDEX].m_nIndex].m_FightMode);

#endif
#ifdef _SERVER
//		g_GlobalMissionArray.Activate();
#endif
	nActiveRegionCount = 0;
	for (int i = 0; i < MAX_SUBWORLD; i++)
	{
		SubWorld[i].Activate();
#ifndef _SERVER
		NpcSet.CheckBalance();
#endif
	}

//	if ((m_nLoopRate % 100) == 0)
//		printf("Region:%d:%d\n", m_nLoopRate, nActiveRegionCount);
#ifdef _SERVER
	PlayerSet.AutoSave();
#endif
}

void KSubWorldSet::MessageLoop()
{
	
	for (int i = 0; i < MAX_SUBWORLD; i++)
	{
		SubWorld[i].MessageLoop();
	}
}

BOOL KSubWorldSet::SendMessage(int nSubWorldID, DWORD dwMsgType, int nParam1, int nParam2, int nParam3)
{
	KWorldMsgNode *pNode = NULL;
	
	pNode = new KWorldMsgNode;
	if (!pNode)
		return FALSE;
	
	pNode->m_dwMsgType	= dwMsgType;
	pNode->m_nParam[0]	= nParam1;
	pNode->m_nParam[1]	= nParam2;
	pNode->m_nParam[2]	= nParam3;
	if (pNode->m_dwMsgType == 4001) g_DebugLog("Send !!!!");
	return SubWorld[nSubWorldID].m_WorldMessage.Send(pNode);
}

void KSubWorldSet::Close()
{
	for (int i = 0; i < MAX_SUBWORLD; i++)
	{
		SubWorld[i].Close();
	}
	NpcSet.RemoveAll();
#ifndef _SERVER
	Player[CLIENT_PLAYER_INDEX].m_ItemList.RemoveAll();
	Player[CLIENT_PLAYER_INDEX].m_cTeam.Release();
	g_Team[0].Release();
	m_cMusic.Stop();
#endif
}
#ifndef _SERVER
void KSubWorldSet::Paint()
{
	SubWorld[0].Paint();
}
#endif

#ifdef _SERVER
void KSubWorldSet::GetRevivalPosFromId(DWORD dwSubWorldId, int nRevivalId, POINT* pPos)
{
	if (!pPos)
		return;

//	int nIdx = SearchWorld(dwSubWorldId);
//	if (nIdx >= 0)
//	{
	KIniFile IniFile;
	char	szKeyName[32];
	char	szSection[32];
	
	g_SetFilePath("\\settings");
	IniFile.Load("RevivePos.ini");
	sprintf(szSection, "%d", dwSubWorldId);
	sprintf(szKeyName, "%d", nRevivalId);
	
	int nX = 51200;
	int nY = 102400;
	IniFile.GetInteger2(szSection, szKeyName, &nX, &nY);
	
	pPos->x = nX;
	pPos->y = nY;
//	}
}
#endif
