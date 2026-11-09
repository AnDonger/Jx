#include "KCore.h"
#include "KSortScript.h"
#include "LuaFuns.h"
#include "KFilePath.h"
#include "KDebug.h"
#ifdef _SERVER
#include "KPakFile.h"
#include <map>
#include <set>
#include <string>
#include <vector>
#endif
#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#else 
#include <io.h>
#include <direct.h>
#endif
//#include "Shlwapi.h"

KLuaScript g_ScriptSet[MAX_SCRIPT_IN_SET];
KScriptBinTree g_ScriptBinTree;
unsigned int   nCurrentScriptNum;
char g_szCurScriptDir[MAX_PATH];

#ifdef _SERVER
// 从原脚本文本建立直接调用关系，不改写 Lua。回调引用的整个函数链均须可执行。
// 这只是新旧接口适配检查；任务条件、奖励和对话内容仍由原脚本决定。
typedef std::map<std::string, std::set<std::string> > KNpcScriptCalls;
static std::map<DWORD, KNpcScriptCalls> g_NpcScriptCalls;

static BOOL IsScriptNameChar(unsigned char c)
{
	return c == '_' || c >= 128 || c >= 'a' && c <= 'z' ||
		c >= 'A' && c <= 'Z' || c >= '0' && c <= '9';
}

static void ReadScriptTokens(const std::string& Text, std::vector<std::string>& Tokens)
{
	for (size_t i = 0; i < Text.size();)
	{
		unsigned char c = Text[i];
		if (c <= ' ')
		{
			i++;
			continue;
		}
		if (Text.compare(i, 2, "--") == 0)
		{
			i += 2;
			if (Text.compare(i, 2, "[[") == 0)
			{
				size_t nEnd = Text.find("]]", i + 2);
				i = nEnd == std::string::npos ? Text.size() : nEnd + 2;
			}
			else
			{
				while (i < Text.size() && Text[i] != '\n' && Text[i] != '\r')
					i++;
			}
			continue;
		}
		if (c == '\'' || c == '"' || Text.compare(i, 2, "[[") == 0)
		{
			if (Text.compare(i, 2, "[[") == 0)
			{
				size_t nEnd = Text.find("]]", i + 2);
				i = nEnd == std::string::npos ? Text.size() : nEnd + 2;
			}
			else
			{
				i++;
				while (i < Text.size())
				{
					unsigned char nNext = Text[i++];
					// 原始脚本为 GBK，汉字的第二字节可等于反斜线。
					if (nNext >= 128 && i < Text.size())
						i++;
					else if (nNext == '\\' && i < Text.size())
						i++;
					else if (nNext == c)
						break;
				}
			}
			Tokens.push_back("<string>");
			continue;
		}
		if (IsScriptNameChar(c))
		{
			size_t nBegin = i++;
			while (i < Text.size() && IsScriptNameChar((unsigned char)Text[i]))
				i++;
			Tokens.push_back(Text.substr(nBegin, i - nBegin));
		}
		else
		{
			Tokens.push_back(Text.substr(i++, 1));
		}
	}
}

static void LoadNpcScriptCalls(DWORD dwScriptId, const char *pszFile)
{
	KNpcScriptCalls& Calls = g_NpcScriptCalls[dwScriptId];
	Calls.clear();
	KPakFile File;
	if (!File.Open(pszFile) || File.Size() == 0 || File.Size() > 1024 * 1024)
		return;
	std::string Text(File.Size(), '\0');
	if (File.Read(&Text[0], (unsigned int)Text.size()) != Text.size())
		return;
	std::vector<std::string> Tokens;
	ReadScriptTokens(Text, Tokens);
	std::string Function;
	int nDepth = 0;
	int nPendingDo = 0;
	for (size_t i = 0; i < Tokens.size(); i++)
	{
		const std::string& Token = Tokens[i];
		if (Token == "function")
		{
			if (nDepth == 0 && i + 2 < Tokens.size() && Tokens[i + 2] == "(")
			{
				Function = Tokens[++i];
				Calls[Function];
				while (i < Tokens.size() && Tokens[i] != ")")
					i++;
			}
			else if (!Function.empty())
			{
				// 不把匿名或嵌套闭包当成已经核实的静态函数链。
				Calls[Function].insert("<dynamic function>");
			}
			nDepth++;
			continue;
		}
		if (!nDepth)
			continue;
		if (Token == "if" || Token == "repeat")
			nDepth++;
		else if (Token == "for" || Token == "while")
		{
			nDepth++;
			nPendingDo++;
		}
		else if (Token == "do")
		{
			if (nPendingDo)
				nPendingDo--;
			else
				nDepth++;
		}
		else if (Token == "end" || Token == "until")
		{
			if (--nDepth == 0)
				Function.clear();
		}
		if (Function.empty() || i + 1 >= Tokens.size() || Tokens[i + 1] != "(" ||
			Token == "if" || Token == "elseif" || Token == "while" ||
			Token == "and" || Token == "or" || Token == "not" || Token == "return")
			continue;
		if (Token == ")" || Token == "]")
			Calls[Function].insert("<dynamic function>");
		else if (IsScriptNameChar((unsigned char)Token[0]))
		{
			// 两个点是字符串连接，不能把 ..GetName() 误判为对象方法调用。
			if (i > 0 && (Tokens[i - 1] == ":" ||
				Tokens[i - 1] == "." && (i < 2 || Tokens[i - 2] != ".")))
				Calls[Function].insert("<dynamic function>");
			else
				Calls[Function].insert(Token);
		}
	}
}

static BOOL IsItemTransactionFunction(const std::string& Function)
{
	// 只开放已纳入快照的变更及只读接口；传送只许延迟提交，其他地图、技能、Buff及外部IO禁用。
	static const char* Names[] =
	{
		"Say", "Talk", "MsgBox", "CloseDialog", "PutMessage", "Msg2Player", "WriteLog",
		"HaveNormalItem", "HaveItem2", "IsExistItem", "DelNormalItem", "AddNormalItem", "AddNormalItemPile",
		"GetCash", "Pay", "Earn", "GetCopperCashCount", "WasteCopperCash",
		"GetTask", "SetTask", "GetTaskTemp", "SetTaskTemp", "GetTaskByte", "SetTaskByte",
		"GetName", "GetLevel", "GetSex", "GetSeries", "GetPlayerType",
		"GetWorldPos", "GetFightState", "GetCamp", "GetPK", "GetUseItemInfo", "ConsumeUseItem", "TeleportWithItem",
		"GetPetType", "GetPetHonor", "GetPetColor", "GetPetName", "IsPetReleased",
		"GenPet", "DelPet", "SetPetType", "SetPetHonor", "ReleasePet",
		"GetPetFuncAuthority", "SetPetFuncAuthority",
		"assert", "error", "type", "tonumber", "tostring", "getn", "tinsert", "tremove",
		"format", "strlen", "strsub", "strfind", "strlower", "strupper", "gsub",
		"random", "floor", "ceil", "min", "max", "abs", "mod"
	};
	for (int i = 0; i < sizeof(Names) / sizeof(Names[0]); i++)
	{
		if (Function == Names[i])
			return TRUE;
	}
	return FALSE;
}

// 自动回调不允许任意NPC写接口、外部IO或动态执行；操作在Lua成功结束后提交。
static BOOL IsPlayerCallbackFunction(const std::string& Function, int nScope)
{
	static const char* Names[] =
	{
		"GetPetType", "GetPetHonor", "IsPetReleased", "GetPetFuncAuthority",
		"GetPetFuncActive", "GetPetSettings", "GetItemInfo", "GetCash",
		"GetBit", "GetByte", "format", "type", "tonumber", "tostring",
		"floor", "min", "max", "abs", "mod", "assert", "error"
	};
	for (int i = 0; i < sizeof(Names) / sizeof(Names[0]); i++)
	{
		if (Function == Names[i])
			return TRUE;
	}
	return nScope == 3 && (Function == "RepairItem" || Function == "Msg2Player");
}

static BOOL CheckNpcScriptCalls(KLuaScript *pScript, const KNpcScriptCalls& Calls,
	const std::string& Function, std::set<std::string>& Visited, std::string& Missing,
	int nScope)
{
	if (!Visited.insert(Function).second)
		return TRUE;
	KNpcScriptCalls::const_iterator pFunction = Calls.find(Function);
	if (pFunction == Calls.end())
	{
		Missing = Function;
		return FALSE;
	}
	for (std::set<std::string>::const_iterator pCall = pFunction->second.begin();
		pCall != pFunction->second.end(); ++pCall)
	{
		if (Calls.find(*pCall) != Calls.end())
		{
			if (!CheckNpcScriptCalls(pScript, Calls, *pCall, Visited, Missing, nScope))
				return FALSE;
			continue;
		}
		if ((nScope == 1 && !IsItemTransactionFunction(*pCall)) ||
			(nScope >= 2 && !IsPlayerCallbackFunction(*pCall, nScope)))
		{
			Missing = *pCall;
			return FALSE;
		}
		int nTop = Lua_GetTopIndex(pScript->m_LuaState);
		Lua_GetGlobal(pScript->m_LuaState, pCall->c_str());
		BOOL bExists = Lua_IsCFunction(pScript->m_LuaState, -1);
		Lua_SetTopIndex(pScript->m_LuaState, nTop);
		if (!bExists)
		{
			Missing = *pCall;
			return FALSE;
		}
	}
	return TRUE;
}

BOOL g_CheckNpcScriptFunction(DWORD dwScriptId, const char *pszFunction, char *pszMissing, int nSize, int nScope)
{
	KLuaScript *pScript = (KLuaScript*)g_GetScript(dwScriptId);
	std::map<DWORD, KNpcScriptCalls>::const_iterator pCalls = g_NpcScriptCalls.find(dwScriptId);
	std::string Missing;
	std::set<std::string> Visited;
	BOOL bResult = pScript && pszFunction && pCalls != g_NpcScriptCalls.end() &&
		CheckNpcScriptCalls(pScript, pCalls->second, pszFunction, Visited, Missing, nScope);
	if (pszMissing && nSize > 0)
		g_StrCpyLen(pszMissing, Missing.empty() ? "script source" : Missing.c_str(), nSize);
	return bResult;
}
#endif

void	LoadScriptInDirectory(LPSTR lpszRootDir, LPSTR lpszSubDir);

int	operator<(KSortScriptNode ScriptLeft, KSortScriptNode ScriptRight)
	{
		return ScriptLeft.GetScriptID() < ScriptRight.GetScriptID();
	};
	
	int operator==(KSortScriptNode ScriptLeft, KSortScriptNode ScriptRight)
	{
		return ScriptLeft.GetScriptID() == ScriptRight.GetScriptID();
	};

//将szFilePath目录下的所有脚本文件加载进ScriptBinTree二叉树中
static unsigned long LoadAllScript(char * szFilePath)
{
	g_SetFilePath("\\");
	char szRootPath[100];
	char szOldRootPath[MAX_PATH];

//	GetCurrentDirectory(MAX_PATH, szOldRootPath);
	getcwd(szOldRootPath, MAXPATH);
	g_GetFullPath(szRootPath,szFilePath);
	LoadScriptInDirectory(szRootPath, "");
	chdir(szOldRootPath);
//	SetCurrentDirectory(szOldRootPath);

	return nCurrentScriptNum;
}

unsigned long g_IniScriptEngine()
{
	g_szCurScriptDir[0] = 0;
	nCurrentScriptNum = 0;
	g_ScriptBinTree.ClearList();
#ifdef _SERVER
	g_NpcScriptCalls.clear();
#endif
	return LoadAllScript("\\script");
		
}



const KScript * g_GetScript(DWORD dwScriptId)
{
	KSortScriptNode ScriptNode;
	ScriptNode.SetScriptID(dwScriptId);
	if (g_ScriptBinTree.Find(ScriptNode))
	{
		return ScriptNode.GetScript();
	}
	return NULL;
}
const KScript * g_GetScript(const char * szRelativeScriptFile)
{
	DWORD dwScriptId = g_FileName2Id((LPSTR)szRelativeScriptFile);
	
	return g_GetScript(dwScriptId);
}

extern int LuaIncludeFile(Lua_State * L);

//加载脚本，该文件名参数为相对目录
static BOOL LoadScriptToSortListA(char * szRelativeFile)
{
	if (!szRelativeFile || !szRelativeFile[0]) return FALSE;
	KSortScriptNode ScriptNode ;
	ScriptNode.SetScriptID(g_FileName2Id(szRelativeFile));
	int t  =strlen(szRelativeFile);
	if (t >= 90)
		t ++;

#ifdef _DEBUG
	//strcpy(ScriptNode.m_szScriptName, szRelativeFile);
#endif
	if (nCurrentScriptNum < MAX_SCRIPT_IN_SET)
	{
		g_ScriptSet[nCurrentScriptNum].Init();
		g_ScriptSet[nCurrentScriptNum].RegisterFunctions(GameScriptFuns, g_GetGameScriptFunNum());
		g_StrCpyLen(g_ScriptSet[nCurrentScriptNum].m_szScriptName, szRelativeFile, 100);
		if (g_ScriptSet[nCurrentScriptNum].Load(szRelativeFile))
		{
#ifdef _SERVER
			LoadNpcScriptCalls(ScriptNode.GetScriptID(), szRelativeFile);
#endif
		}
		else
		{
			g_DebugLog("[脚本]加载脚本%s，出错，该脚本无法加载！！请检查！！", szRelativeFile);
			return FALSE;
		}
	}
	else
	{
		g_DebugLog("[脚本]严重错误!脚本数量超限制%d！请立即解决！！", MAX_SCRIPT_IN_SET);
		return FALSE;
	}
	
	ScriptNode.SetScriptIndex(nCurrentScriptNum++);
	g_ScriptBinTree.Insert(ScriptNode);
	return TRUE;
}

//加载脚本，该文件名参数为实际目录
static BOOL LoadScriptToSortList(char * szFileName)
{
	if (!szFileName || !szFileName[0]) return FALSE;
	if (nCurrentScriptNum>= MAX_SCRIPT_IN_SET)
	{
		g_DebugLog("[Script]脚本总容量超过%d,严重错误请检查!",MAX_SCRIPT_IN_SET);
		return FALSE;
	}

	int nFileNameLen = strlen(szFileName);
	
	char szRootPath[MAX_PATH];	
	g_GetRootPath(szRootPath);
//	char szRelativePath[MAX_PATH];
	char *szRelativePath;
	char szCurrentDirectory[MAX_PATH];


//	GetCurrentDirectory(MAX_PATH, szCurrentDirectory);
	getcwd(szCurrentDirectory, MAX_PATH);
	szRelativePath = szCurrentDirectory + strlen(szRootPath);
//	PathRelativePathTo(szRelativePath,szRootPath, FILE_ATTRIBUTE_DIRECTORY,szCurrentDirectory , FILE_ATTRIBUTE_NORMAL );
	char szRelativeFile[MAX_PATH];
	if (szRelativePath[0] == '.' && szRelativePath[1] == '\\')
		sprintf(szRelativeFile, "%s\\%s", szRelativePath + 1, szFileName);
	else
		sprintf(szRelativeFile, "%s\\%s", szRelativePath, szFileName);
	g_StrLower(szRelativeFile);
	g_DebugLog("[Script]Loading Script %s %d", szRelativeFile, g_FileName2Id(szRelativeFile));
//
	return LoadScriptToSortListA(szRelativeFile);
//	return FALSE;
}


void	LoadScriptInDirectory(LPSTR lpszRootDir, LPSTR lpszSubDir)
{
	int				nFlag;
	char			szRealDir[MAX_PATH];
#ifdef WIN32
	sprintf(szRealDir, "%s\\%s", lpszRootDir, lpszSubDir);
#else
	sprintf(szRealDir, "%s/%s", lpszRootDir, lpszSubDir);
        char *ptr = szRealDir;
        while(*ptr) { if(*ptr == '\\') *ptr = '/';  ptr++;  }
#endif

#ifdef WIN32
	if(chdir(szRealDir)) return;
	_finddata_t FindData;
	long dir = _findfirst("*.*", &FindData);
	while(dir != -1) {
		if(strcmp(FindData.name, ".") == 0 || strcmp(FindData.name, "..") == 0)	{
			if(_findnext(dir, &FindData)) break;
			continue;
		}
		if(FindData.attrib == _A_SUBDIR)
		{
			LoadScriptInDirectory(szRealDir, FindData.name);
		}
		else
		{	
			nFlag = 0;
			for (int i = 0; i < (int)strlen(FindData.name);  i++)
			{
				if (FindData.name[i] == '.')
					break;
				if (FindData.name[i] == '\\')
				{
					nFlag = 1;
					break;
				}
			}
			if (nFlag == 1)
			{
				LoadScriptInDirectory(szRealDir, FindData.name);
			}
			else
			{
				char szExt[50];
				if (strlen(FindData.name) >= 4) 
				{
					strcpy(szExt, FindData.name + strlen(FindData.name) - 4);
					_strupr(szExt);
					if ( (!strcmp(szExt, ".LUA")) || (!strcmp(szExt, ".TXT")))
						if (!LoadScriptToSortList(FindData.name))
							g_DebugLog("加载%s文件出错", FindData.name);
				}
			}
		}
		if(_findnext(dir, &FindData)) break;
	} 
	_findclose(dir);
	chdir(lpszRootDir);
#else
     DIR *dp;
     int i;
     struct dirent *ep;
     if(chdir(szRealDir)) return;
     dp = opendir(".");
     if(dp) {
          while(ep = readdir(dp)) {
            if(strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0) continue;
            
            if(ep->d_type ==4) {
                LoadScriptInDirectory(szRealDir, ep->d_name);
            }
            else {
			nFlag = 0;
			for (i = 0; i < (int)strlen(ep->d_name);  i++)
			{
				if (ep->d_name[i] == '.')
					break;
				if (ep->d_name[i] == '\\')
				{
					nFlag = 1;
					break;
				}
			}
			if (nFlag == 1)
			{
				LoadScriptInDirectory(szRealDir,ep->d_name);
			}
			else
			{
				char szExt[50];
				if (strlen(ep->d_name) >= 4)
				{
					strcpy(szExt, ep->d_name + strlen(ep->d_name) - 4);
                                        g_StrUpper(szExt);
///					_strupr(szExt);
					if ( (!strcmp(szExt, ".LUA")) || (!strcmp(szExt, ".TXT")))
						if (!LoadScriptToSortList(ep->d_name))
							g_DebugLog("加载%s文件出错", ep->d_name);
				}
			}
		}
	          }
          closedir(dp);
     }
	chdir(lpszRootDir);
#endif
}

void UnLoadScript(DWORD dwScriptID)
{
	KSortScriptNode ScriptNode;
	ScriptNode.SetScriptID(dwScriptID);
	g_ScriptBinTree.Delete(ScriptNode);
}

BOOL ReLoadScript(const char * szRelativePathScript)
{
	if (!szRelativePathScript || !szRelativePathScript[0])
		return FALSE;
	char script[MAX_PATH];
	strcpy(script, szRelativePathScript);
//	_strlwr(script);
        g_StrLower(script);
	UnLoadScript(g_FileName2Id(script));
	return LoadScriptToSortListA(script);
}

unsigned long  ReLoadAllScript()
{
	g_ScriptBinTree.ClearList();
	return g_IniScriptEngine();
}

