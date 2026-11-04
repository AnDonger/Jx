//---------------------------------------------------------------------------
// Sword3 Engine (c) 2002 by Kingsoft
//
// File:	KPlayerTask.h
// Date:	2002.10.05
// Code:	边城浪子
// Desc:	PlayerTask Class
//---------------------------------------------------------------------------

#ifndef KPLAYERTASK_H
#define KPLAYERTASK_H

// 当前原始 NPC 脚本使用 304、1276、1765 等持久任务号，沿用原存档的稀疏记录格式。
#define		MAX_TASK		2048
#define		MAX_TEMP_TASK	2560



#ifdef _SERVER
#define TASKGLOBALVALUENUM 5000
extern int		g_TaskGlobalValue[TASKGLOBALVALUENUM];

extern KTabFile g_WayPointTabFile;
extern KTabFile g_StationTabFile;
extern KTabFile g_DockTabFile;

extern KTabFile g_StationPriceTabFile;
extern KTabFile g_WayPointPriceTabFile;
extern KTabFile g_DockPriceTabFile;

extern int	*g_pStationPriceTab;
extern int  *g_pWayPointPriceTab;
extern int  *g_pDockPriceTab;

#endif


#define MAX_TASK_NOTE 64
#define MAX_TASK_NOTE_TEXT 128

struct KTaskNote
{
	int		TaskId;
	int		Step;
	char	Text[MAX_TASK_NOTE_TEXT];
};

class KPlayerTask
{
public:
	int			nSave[MAX_TASK];					// 用于记录任务是否完成，须保存到数据库
	int			nClear[MAX_TEMP_TASK];				// 用于记录任务过程中的中间步骤的完成情况，不保存到数据库，玩家下线后次数据清空
#ifdef _SERVER
	KTaskNote	m_Notes[MAX_TASK_NOTE];
	BOOL		SetNote(int nTaskId, int nStep, const char *pszText);
	BOOL		SendNote(int nPlayer, int nTaskId, int nStep, const char *pszText);
	BOOL		SyncNotes(int nPlayer);
	BOOL		LoadNotes(const BYTE *pBuffer, DWORD dwLength);
	int			SaveNotes(BYTE *pBuffer, DWORD dwCapacity);
#endif
public:
	KPlayerTask();									// 构造函数
	void		Release();							// 清空
	void		ClearTempVar();						// 清除临时过程控制变量
	int			GetSaveVal(int nNo);				// 得到任务完成情况
	void		SetSaveVal(int nNo, BOOL bFlag);	// 设定任务完成情况
	int			GetClearVal(int nNo);				// 得到临时过程控制变量值
	void		SetClearVal(int nNo, int nVal);		// 设定临时过程控制变量值
};

#endif
