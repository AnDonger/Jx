//---------------------------------------------------------------------------
// Sword3 Engine (c) 2002 by Kingsoft
//
// File:	KPlayerTask.cpp
// Date:	2002.10.05
// Code:	边城浪子
// Desc:	PlayerTask Class
//---------------------------------------------------------------------------

#include	"KCore.h"
#include	"KPlayerTask.h"
#ifdef _SERVER
#include "KPlayer.h"
#endif

#ifdef _SERVER
int			g_TaskGlobalValue[TASKGLOBALVALUENUM]; //全局的变量，用于服务器脚本系统

KTabFile	g_WayPointTabFile;
KTabFile	g_StationTabFile;
KTabFile	g_DockTabFile;
KTabFile	g_StationPriceTabFile;
KTabFile	g_WayPointPriceTabFile;
KTabFile	g_DockPriceTabFile;

int	*		g_pStationPriceTab = NULL;
int	*		g_pWayPointPriceTab = NULL;
int	*		g_pDockPriceTab = NULL;

#endif
//---------------------------------------------------------------------------
//	功能：构造函数
//---------------------------------------------------------------------------
KPlayerTask::KPlayerTask()
{
	Release();
}

//---------------------------------------------------------------------------
//	功能：清空
//---------------------------------------------------------------------------
void	KPlayerTask::Release()
{
	memset(nSave, 0, sizeof(nSave));
	memset(nClear, 0, sizeof(nClear));
#ifdef _SERVER
	memset(m_Notes, 0, sizeof(m_Notes));
#endif
}

//---------------------------------------------------------------------------
//	功能：清除临时过程控制变量
//---------------------------------------------------------------------------
void	KPlayerTask::ClearTempVar()
{
	memset(nClear, 0, sizeof(nClear));
}

//---------------------------------------------------------------------------
//	功能：得到任务完成情况
//---------------------------------------------------------------------------
int		KPlayerTask::GetSaveVal(int nNo)
{
	if (nNo < 0 || nNo >= MAX_TASK)
		return 0;
	return nSave[nNo];
}

//---------------------------------------------------------------------------
//	功能：设定任务完成情况
//---------------------------------------------------------------------------
void	KPlayerTask::SetSaveVal(int nNo, BOOL bFlag)
{
	if (nNo < 0 || nNo >= MAX_TASK)
		return;
	nSave[nNo] = bFlag;
}

//---------------------------------------------------------------------------
//	功能：得到临时过程控制变量值
//---------------------------------------------------------------------------
int		KPlayerTask::GetClearVal(int nNo)
{
	if (nNo < 0 || nNo >= MAX_TEMP_TASK)
		return 0;
	return nClear[nNo];
}

//---------------------------------------------------------------------------
//	功能：设定临时过程控制变量值
//---------------------------------------------------------------------------
void	KPlayerTask::SetClearVal(int nNo, int nVal)
{
	if (nNo < 0 || nNo >= MAX_TEMP_TASK)
		return;
	nClear[nNo] = nVal;
}


#ifdef _SERVER
// 任务日志独立于 GetTask/SetTask 变量。客户端按原 taskinfo.tbl 显示标题和步骤。
static BOOL CheckTaskNote(int nTaskId, int nStep)
{
	if (nTaskId <= 0 || nTaskId > 65535 || nStep < -1 || nStep > 254)
		return FALSE;
	static KIniFile TaskInfo;
	static BOOL bLoaded = FALSE;
	if (!bLoaded)
	{
		if (!TaskInfo.Load("\\settings\\taskinfo.tbl"))
			return FALSE;
		bLoaded = TRUE;
	}
	char szSection[32], szKey[32], szValue[1024];
	sprintf(szSection, "Task_%d", nTaskId);
	if (!TaskInfo.GetString(szSection, "Title", "", szValue, sizeof(szValue)) || !szValue[0])
		return FALSE;
	if (nStep < 0)
		return TRUE;
	sprintf(szKey, "Step_%d", nStep);
	return TaskInfo.GetString(szSection, szKey, "", szValue, sizeof(szValue)) && szValue[0];
}

BOOL KPlayerTask::SetNote(int nTaskId, int nStep, const char *pszText)
{
	if (!pszText || strlen(pszText) >= MAX_TASK_NOTE_TEXT || !CheckTaskNote(nTaskId, nStep))
		return FALSE;
	int nFree = -1;
	for (int i = 0; i < MAX_TASK_NOTE; i++)
	{
		if (m_Notes[i].TaskId == nTaskId)
		{
			nFree = i;
			break;
		}
		if (!m_Notes[i].TaskId && nFree < 0)
			nFree = i;
	}
	// 仓库许可的 Step_1 在三个原脚本/本批录包中均结束任务日志。
	// 其他任务不按“最后一项”猜测完成，仍保留脚本指定的步骤。
	BOOL bRemove = nStep < 0 || (nStep == 1 && (nTaskId == 3 || nTaskId == 9 || nTaskId == 15));
	if (nFree < 0)
		return bRemove;
	ZeroMemory(&m_Notes[nFree], sizeof(m_Notes[nFree]));
	if (!bRemove)
	{
		m_Notes[nFree].TaskId = nTaskId;
		m_Notes[nFree].Step = nStep;
		strcpy(m_Notes[nFree].Text, pszText);
	}
	return TRUE;
}

BOOL KPlayerTask::SendNote(int nPlayer, int nTaskId, int nStep, const char *pszText)
{
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER || !pszText ||
		strlen(pszText) >= MAX_TASK_NOTE_TEXT || !CheckTaskNote(nTaskId, nStep))
		return FALSE;
	PLAYER_SCRIPTACTION_SYNC Info;
	ZeroMemory(&Info, sizeof(Info));
	Info.m_bUIId = UI_NOTEINFO;
	Info.m_bParam2 = 1;
	Info.m_nOperateType = SCRIPTACTION_UISHOW;
	Info.m_nParam = nTaskId;
	BOOL bRemove = nStep < 0 || (nStep == 1 && (nTaskId == 3 || nTaskId == 9 || nTaskId == 15));
	Info.m_bParam1 = bRemove ? 255 : (BYTE)nStep;
	Info.m_nBufferLen = strlen(pszText) + 1;
	memcpy(Info.m_pContent, pszText, Info.m_nBufferLen);
	Player[nPlayer].DoScriptAction(&Info);
	return TRUE;
}

BOOL KPlayerTask::SyncNotes(int nPlayer)
{
	for (int i = 0; i < MAX_TASK_NOTE; i++)
	{
		if (m_Notes[i].TaskId && !SendNote(nPlayer, m_Notes[i].TaskId, m_Notes[i].Step, m_Notes[i].Text))
			return FALSE;
	}
	return TRUE;
}

// 可选版本扩展放在标准 TDBTaskData 后、dwItemOffset 前；旧档无此扩展。
// 每条为 DWORD任务ID、DWORD步骤、DWORD文本长度、带结尾0的GBK文本。
static const DWORD TASK_NOTE_MAGIC = 0x31534B54;

int KPlayerTask::SaveNotes(BYTE *pBuffer, DWORD dwCapacity)
{
	DWORD dwCount = 0;
	DWORD dwSize = 3 * sizeof(DWORD);
	for (int i = 0; i < MAX_TASK_NOTE; i++)
	{
		if (!m_Notes[i].TaskId)
			continue;
		dwCount++;
		dwSize += 3 * sizeof(DWORD) + strlen(m_Notes[i].Text) + 1;
	}
	if (!dwCount)
		return 0;
	if (!pBuffer || dwSize > dwCapacity)
		return -1;
	DWORD Header[3] = {TASK_NOTE_MAGIC, 1, dwCount};
	memcpy(pBuffer, Header, sizeof(Header));
	BYTE *pOut = pBuffer + sizeof(Header);
	for (int i = 0; i < MAX_TASK_NOTE; i++)
	{
		if (!m_Notes[i].TaskId)
			continue;
		DWORD Record[3] = {(DWORD)m_Notes[i].TaskId, (DWORD)m_Notes[i].Step,
			(DWORD)strlen(m_Notes[i].Text) + 1};
		memcpy(pOut, Record, sizeof(Record));
		pOut += sizeof(Record);
		memcpy(pOut, m_Notes[i].Text, Record[2]);
		pOut += Record[2];
	}
	return dwSize;
}

BOOL KPlayerTask::LoadNotes(const BYTE *pBuffer, DWORD dwLength)
{
	if (!dwLength)
	{
		ZeroMemory(m_Notes, sizeof(m_Notes));
		return TRUE;
	}
	if (!pBuffer || dwLength < 3 * sizeof(DWORD))
		return FALSE;
	DWORD Header[3];
	memcpy(Header, pBuffer, sizeof(Header));
	if (Header[0] != TASK_NOTE_MAGIC || Header[1] != 1 || Header[2] > MAX_TASK_NOTE)
		return FALSE;
	KTaskNote Notes[MAX_TASK_NOTE];
	ZeroMemory(Notes, sizeof(Notes));
	const BYTE *pIn = pBuffer + sizeof(Header);
	const BYTE *pEnd = pBuffer + dwLength;
	for (DWORD i = 0; i < Header[2]; i++)
	{
		DWORD Record[3];
		if (pEnd - pIn < sizeof(Record))
			return FALSE;
		memcpy(Record, pIn, sizeof(Record));
		pIn += sizeof(Record);
		if (!Record[2] || Record[2] > MAX_TASK_NOTE_TEXT || pEnd - pIn < Record[2] ||
			pIn[Record[2] - 1] != 0 || strlen((const char*)pIn) + 1 != Record[2] ||
			!CheckTaskNote(Record[0], Record[1]))
			return FALSE;
		for (DWORD j = 0; j < i; j++)
		{
			if (Notes[j].TaskId == Record[0])
				return FALSE;
		}
		Notes[i].TaskId = Record[0];
		Notes[i].Step = Record[1];
		memcpy(Notes[i].Text, pIn, Record[2]);
		pIn += Record[2];
	}
	if (pIn != pEnd)
		return FALSE;
	memcpy(m_Notes, Notes, sizeof(m_Notes));
	return TRUE;
}
#endif
