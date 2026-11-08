#ifndef KSORTSCRIPT_H
#define KSORTSCRIPT_H

#include "KCore.h"
#include "KLuaScript.h"
//排序二叉树，保存经过文件名转数字的脚本的id,并实现快速查找对应脚本

#include "KBinTreeNode.h"
#include "KBinsTree.h"
extern char g_szCurScriptDir[MAX_PATH];

#ifdef _SERVER 
#define MAX_SCRIPT_IN_SET 10000
#else
#define MAX_SCRIPT_IN_SET 5
#endif

extern KLuaScript g_ScriptSet[MAX_SCRIPT_IN_SET];

class KSortScriptNode
{

	DWORD m_dwScriptIndex;
	DWORD m_dwScriptID;
public:

#ifdef _DEBUG
	char m_szScriptName[100];
#endif

	DWORD GetScriptIndex(){return m_dwScriptIndex;};
	DWORD GetScriptID(){return m_dwScriptID;};

	void SetScriptIndex(DWORD dwScriptIndex){m_dwScriptIndex = dwScriptIndex;};
	void SetScriptID(DWORD dwScriptID){m_dwScriptID = dwScriptID;};

	const KScript * GetScript()
	{
		if (m_dwScriptIndex >= 0 && m_dwScriptIndex <MAX_SCRIPT_IN_SET) 
			return &g_ScriptSet[m_dwScriptIndex];
		else
			return NULL;
	}
	;
	
};

typedef	BinSTree<KSortScriptNode> KScriptBinTree;
extern KScriptBinTree g_ScriptBinTree;
extern unsigned long g_IniScriptEngine();
extern const KScript * g_GetScript(DWORD dwScriptId);
extern const KScript * g_GetScript(const char * szRelativeScriptFile);
extern BOOL ReLoadScript(const char * szRelativePathScript);
#ifdef _SERVER
extern BOOL g_CheckNpcScriptFunction(DWORD dwScriptId, const char *pszFunction, char *pszMissing, int nSize, BOOL bItemTransaction = FALSE);

// NPC材料交换失败时恢复物品、现金和任务，经验奖励在成功后结算。
class KNpcScriptTransaction
{
private:
	int		m_nPlayer;
	BOOL	m_bCommitted;
public:
	KNpcScriptTransaction(int nPlayer);
	~KNpcScriptTransaction();
	void Commit() { m_bCommitted = TRUE; }
	BOOL HasFailed() const;
};

#define MAX_SCRIPT_SELECTED_ITEMS 9

// 仅在一次窗口请求回调期间有效，Lua使用1起始的槽号，不能传任意服务端物品索引。
class KNpcScriptSelection
{
private:
	int m_nPlayer;
public:
	KNpcScriptSelection(int nPlayer, const int* pItems, int nCount);
	~KNpcScriptSelection();
	BOOL IsValid() const { return m_nPlayer > 0; }
	int GetResult() const;
};
#endif
extern unsigned long  ReLoadAllScript();
#endif
