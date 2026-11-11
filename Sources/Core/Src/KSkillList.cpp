 #include "KEngine.h"
#include "KCore.h"
#include "SkillDef.h"
#include "KSkillList.h"
#ifndef _STANDALONE
#include "../../../lib/S3DBInterface.h"
#include "crtdbg.h"
#else
#include "S3DBInterface.h"
#endif
#include "KSkills.h"
#include "KPlayer.h"
#include <time.h>
#include <limits.h>

#ifndef _SERVER
#include "cOREsHELL.H"
#endif

KSkillList::KSkillList()
{
	ZeroMemory(m_Skills, sizeof(NPCSKILL) * MAX_NPCSKILL);
	m_nNpcIndex = 0;
	memset(m_SkillBuffs, 0, sizeof(m_SkillBuffs));
	m_nExpAuraId = 0;
	m_nAuraExpFrames = 0;
}

KSkillList::~KSkillList()
{
}


static int g_CallCount = 0;

int	KSkillList::FindSame(int nSkillID)
{
    g_CallCount++;

	if (!nSkillID)
		return 0;
	
	for (int i=1; i<MAX_NPCSKILL; i++)
	{
		if (m_Skills[i].SkillId == nSkillID)
		{
			return i;
		}
	}
	return 0;	
}

int KSkillList::FindFree()
{
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (!m_Skills[i].SkillId)
		{
			return i;
		}
	}
	return 0;	
}

#ifdef _SERVER
void KSkillList::SetNpcSkill(int nSkillNo, int nSkillId, int nSkillLevel)
{
	if (nSkillNo <= 0 || nSkillLevel <= 0 || nSkillId <= 0)
		return ;
	if (nSkillNo >= MAX_NPCSKILL) 
		return ;
	
	m_Skills[nSkillNo].SkillId = nSkillId;
	m_Skills[nSkillNo].CurrentSkillLevel = nSkillLevel;
	m_Skills[nSkillNo].SkillLevel = nSkillLevel;
	m_Skills[nSkillNo].MaxTimes = 0;
	m_Skills[nSkillNo].RemainTimes = 0;

	_ASSERT(nSkillId > 0 && nSkillLevel > 0);
	KSkill * pOrdinSkill = (KSkill *) g_SkillManager.GetSkill(nSkillId, nSkillLevel);
	if (!pOrdinSkill) 
        return ;
	
    if (pOrdinSkill->GetSkillStyle() == SKILL_SS_PassivityNpcState)
	{
		pOrdinSkill->Cast(m_nNpcIndex, -1, m_nNpcIndex);
	}
}

void KSkillList::RemoveNpcSkill(int nSkillNo)
{
	if (nSkillNo <= 0 )
		return ;
	if (nSkillNo >= MAX_NPCSKILL) 
		return ;
	
	m_Skills[nSkillNo].SkillId = 0;
	m_Skills[nSkillNo].CurrentSkillLevel = 0;
	m_Skills[nSkillNo].SkillLevel = 0;
	m_Skills[nSkillNo].MaxTimes = 0;
	m_Skills[nSkillNo].RemainTimes = 0;
}

#endif

int KSkillList::GetCount()
{
	int nCount = 0;
	
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (m_Skills[i].SkillId)
		{
			nCount++;
		}
	}
	return nCount;
}

#ifndef _SERVER
void KSkillList::SetSkillLevel(int nId/*技能的ID号*/, int nLevel)
{
	int i = FindSame(nId);
	
	if (!i)
	{
		Add(nId, nLevel);
		return;		
	}
	else
		//m_Skills[i].SkillLevel = nLevel;
		SetLevel(i, nLevel);
}
#endif

#ifndef _SERVER
BOOL KSkillList::SetLevel(int nIndex/*技能列表的索引号*/, int nLevel)
{
	if (nIndex <= 0 || nIndex >= MAX_NPCSKILL)
		return FALSE;
	if (nLevel < 0 || nLevel >= MAX_SKILLLEVEL)
		return FALSE;
	
	int nInc = nLevel - m_Skills[nIndex].SkillLevel;
	IncreaseLevel(nIndex, nInc);
	return TRUE;
}
#endif

BOOL KSkillList::IncreaseLevel(int nIdx, int nLvl)
{
	if (nIdx <= 0 || nIdx >= MAX_NPCSKILL)
		return FALSE;
	if (nLvl <= 0)
		return FALSE;
	
	m_Skills[nIdx].SkillLevel += nLvl;
	m_Skills[nIdx].CurrentSkillLevel += nLvl;
	
	
	KSkill * pOrdinSkill = (KSkill *)g_SkillManager.GetSkill(m_Skills[nIdx].SkillId, m_Skills[nIdx].CurrentSkillLevel);
	if (!pOrdinSkill) 
        return FALSE;

	if (pOrdinSkill->GetSkillStyle() == SKILL_SS_PassivityNpcState)
	{
		pOrdinSkill->Cast(m_nNpcIndex, -1, m_nNpcIndex);
	}
	return TRUE;
}

int KSkillList::Add(int nSkillID, int nLevel, int nMaxTimes, int RemainTimes)
{

	int i;
	
	if (nSkillID <= 0 || nLevel < 0)
		return 0;
	i = FindSame(nSkillID);
	if (i)
	{
		if (nLevel > m_Skills[i].SkillLevel)
		{
			int nInc = nLevel - m_Skills[i].SkillLevel;
			IncreaseLevel(i, nInc);
		}
		m_Skills[i].MaxTimes = 0;
		m_Skills[i].RemainTimes = 0;
		
		if (nLevel  > 0)
		{
			ISkill * pSkill = g_SkillManager.GetSkill(nSkillID, m_Skills[i].CurrentSkillLevel);
			if (!pSkill) 
				return 0;
			
			//如果改技能属于被动辅助技能时，则设置Npc状态
			if (pSkill->GetSkillStyle() == SKILL_SS_PassivityNpcState)
			{
				((KSkill *)pSkill)->Cast(m_nNpcIndex, -1, m_nNpcIndex);
			}
		}

		return i;
	}

	i = FindFree();
	if (i)
	{
		m_Skills[i].SkillId = nSkillID;
		m_Skills[i].SkillStatus = 1;
		m_Skills[i].SkillLevel = nLevel;
		m_Skills[i].CurrentSkillLevel = nLevel;
		m_Skills[i].MaxTimes = 0;
		m_Skills[i].RemainTimes = 0;

		if (nLevel > 0)
		{
			ISkill * pSkill =  g_SkillManager.GetSkill(nSkillID, nLevel);
			if (!pSkill) 
				return 0 ;
			
			if (pSkill->GetSkillStyle() == SKILL_SS_PassivityNpcState)
			{
				((KSkill*)pSkill)->Cast(m_nNpcIndex, -1, m_nNpcIndex);
			}
		}

		return i;
	}
	
	return 0;
}

int KSkillList::GetLevel(int nSkillID)
{
	int i;
	
	if (!nSkillID)
		return 0;
	
	i = FindSame(nSkillID);
	if (i)
	{
		_ASSERT(m_Skills[i].SkillLevel < MAX_SKILLLEVEL);
		return m_Skills[i].SkillLevel;
	}

	return 0;
}

int KSkillList::GetCurrentLevel(int nSkillID)
{
	int i;
	
	if (!nSkillID)
		return 0;
	
	i = FindSame(nSkillID);

	if (i)
	{
		int nLevel = m_Skills[i].CurrentSkillLevel;
		if (nLevel > 0 && m_Skills[i].SkillStatus == 3 && m_nNpcIndex > 0 &&
			m_nNpcIndex < MAX_NPC && Npc[m_nNpcIndex].IsPlayer())
		{
			const int* pAttrib = Npc[m_nNpcIndex].m_CurrentAdditionalAttrib;
			int nBonus = pAttrib[139];
			static const int SpecificSkills[58] =
			{
				27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,
				3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,
				0,43,0,44,0,45,0,46,0,47,0,48,0,49,0,50,0,51
			};
			for (int j = 0; j < 58; j++)
				if (SpecificSkills[j] == nSkillID)
					nBonus += pAttrib[194 + j];
			nBonus += g_SkillManager.GetEquipmentSeriesBonus(nSkillID, pAttrib);
			nLevel = max(1, min(MAX_SKILLLEVEL - 1, nLevel + nBonus));
		}
		return nLevel;
	}

	return 0;
}

BOOL KSkillList::CanCast(int nSkillID, DWORD dwTime)
{
	
	if (!nSkillID)
		return FALSE;
	
	int i = FindSame(nSkillID);
	if (!i)
	{
		return FALSE;
	}

	if (m_Skills[i].CurrentSkillLevel <= 0 || m_Skills[i].NextCastTime > dwTime
#ifdef _SERVER
		|| (m_nNpcIndex > 0 && Npc[m_nNpcIndex].IsPlayer() && m_Skills[i].SkillStatus != 3)
#endif
		)
		return FALSE;
	
	return TRUE;
}

void KSkillList::SetNextCastTime(int nSkillID, DWORD dwTime, BOOL bSync)
{
//	m_Skills[nSkillID].NextCastTime = dwTime;
//	return;
	if (!nSkillID)
		return;;
	
	int i = FindSame(nSkillID);
	if (!i)
		return;
	
	m_Skills[i].NextCastTime = dwTime;
#ifdef _SERVER
	if (bSync)
		SyncPlayerSkillCooldown(nSkillID);
#endif
}	
#ifdef _SERVER
DWORD KSkillList::GetPlayerSkillCooldownFrames(int nSkillID)
{
	int nIndex = FindSame(nSkillID);
	if (!nIndex || m_nNpcIndex <= 0 || m_nNpcIndex >= MAX_NPC)
		return 0;
	int nWorld = Npc[m_nNpcIndex].m_SubWorldIndex;
	if (nWorld < 0 || nWorld >= MAX_SUBWORLD)
		return 0;
	DWORD dwNow = SubWorld[nWorld].m_dwCurrentTime;
	return m_Skills[nIndex].NextCastTime > dwNow ? m_Skills[nIndex].NextCastTime - dwNow : 0;
}

// 存档使用自1601年起的毫秒到期时间，避免把本次服务端世界帧写入下次会话。
ULONGLONG KSkillList::GetPlayerSkillCooldownClock()
{
	FILETIME Time;
	GetSystemTimeAsFileTime(&Time);
	return (((ULONGLONG)Time.dwHighDateTime << 32) | Time.dwLowDateTime) / 10000;
}

ULONGLONG KSkillList::GetPlayerSkillCooldownEndTime(int nSkillID)
{
	DWORD dwFrames = GetPlayerSkillCooldownFrames(nSkillID);
	if (!dwFrames)
		return 0;
	return GetPlayerSkillCooldownClock() + ((ULONGLONG)dwFrames * 1000 + 17) / 18;
}

BOOL KSkillList::RestorePlayerSkillCooldown(int nSkillID, ULONGLONG ullEndTime)
{
	int nIndex = FindSame(nSkillID);
	if (!nIndex || m_nNpcIndex <= 0 || m_nNpcIndex >= MAX_NPC)
		return FALSE;
	int nWorld = Npc[m_nNpcIndex].m_SubWorldIndex;
	if (nWorld < 0 || nWorld >= MAX_SUBWORLD)
		return FALSE;
	ULONGLONG ullNow = GetPlayerSkillCooldownClock();
	DWORD dwFrames = 0;
	if (ullEndTime > ullNow)
	{
		// 限制损坏存档的时间跨度，保证毫秒和世界帧转换不会溢出。
		if (ullEndTime - ullNow > MAXDWORD)
			return FALSE;
		dwFrames = (DWORD)(((ullEndTime - ullNow) * 18 + 999) / 1000);
	}
	if (dwFrames > MAXDWORD - SubWorld[nWorld].m_dwCurrentTime)
		return FALSE;
	m_Skills[nIndex].NextCastTime = dwFrames ? SubWorld[nWorld].m_dwCurrentTime + dwFrames : 0;
	return TRUE;
}

void KSkillList::SyncPlayerSkillCooldown(int nSkillID)
{
	if (m_nNpcIndex <= 0 || m_nNpcIndex >= MAX_NPC || !Npc[m_nNpcIndex].IsPlayer() || !g_pServer)
		return;
	DWORD dwFrames = GetPlayerSkillCooldownFrames(nSkillID);
	if (!dwFrames)
		return;
	if (dwFrames > 0xffff)
	{
		SyncPlayerSkillCooldowns(nSkillID);
		return;
	}
	int nPlayer = Npc[m_nNpcIndex].GetPlayerIdx();
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER || Player[nPlayer].m_nNetConnectIdx < 0)
		return;
	GAME_SKILL_COOLDOWN_SYNC Sync;
	memset(&Sync, 0, sizeof(Sync));
	Sync.ProtocolType = 0xf8;
	Sync.Length = sizeof(Sync) - 1;
	Sync.SubType = 0x2d;
	Sync.SkillId = nSkillID;
	Sync.unknown_1 = 1;
	Sync.RemainingFrames = (WORD)dwFrames;
	g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, &Sync, sizeof(Sync));
	// 新客户端F8/2D采用本地预计算值，效果帧先后不同会读到旧值。
	// F8/4B直接写入服务器到期帧，紧接着校正本次技能，避免依赖客户端动作时序。
	SyncPlayerSkillCooldowns(nSkillID);
}

BOOL KSkillList::SyncPlayerSkillCooldowns(int nSkillID)
{
	if (m_nNpcIndex <= 0 || m_nNpcIndex >= MAX_NPC || !Npc[m_nNpcIndex].IsPlayer() || !g_pServer)
		return FALSE;
	int nPlayer = Npc[m_nNpcIndex].GetPlayerIdx();
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER || Player[nPlayer].m_nNetConnectIdx < 0)
		return FALSE;
	BYTE Data[sizeof(GAME_SKILL_COOLDOWN_LIST_HEADER) + MAX_NPCSKILL * sizeof(GAME_SKILL_COOLDOWN_RECORD) + 1];
	memset(Data, 0, sizeof(Data));
	GAME_SKILL_COOLDOWN_LIST_HEADER* pHeader = (GAME_SKILL_COOLDOWN_LIST_HEADER*)Data;
	GAME_SKILL_COOLDOWN_RECORD* pRecords = (GAME_SKILL_COOLDOWN_RECORD*)(Data + sizeof(*pHeader));
	pHeader->ProtocolType = 0xf8;
	pHeader->SubType = 0x4b;
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (m_Skills[i].SkillId <= 0 || (nSkillID && m_Skills[i].SkillId != nSkillID))
			continue;
		GAME_SKILL_COOLDOWN_RECORD& Record = pRecords[pHeader->Count++];
		Record.SkillId = (WORD)m_Skills[i].SkillId;
		Record.NextCastFrame = GetPlayerSkillCooldownFrames(m_Skills[i].SkillId) ? m_Skills[i].NextCastTime : 0;
	}
	if (nSkillID && !pHeader->Count)
		return FALSE;
	// 正式服列表在Count条记录后还有1字节unknown_1；客户端未读取，保留样本中的0。
	int nSize = sizeof(*pHeader) + pHeader->Count * sizeof(*pRecords) + 1;
	pHeader->Length = (WORD)(nSize - 1);
	return SUCCEEDED(g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, Data, nSize));
}
#endif

#ifndef _SERVER
int	KSkillList::GetSkillSortList(KUiSkillData * pSkillList)
{
	//Ques
	if (!pSkillList) return 0;
	memset(pSkillList, 0, sizeof(KUiSkillData) * 50);
	int nCount = 0;
	ISkill* pSkill = NULL;
	KSkill * pOrdinSkill = NULL;
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (m_Skills[i].SkillId)
		{
			_ASSERT(m_Skills[i].SkillLevel >= 0);
			
			if (!m_Skills[i].SkillLevel)
			{
				pSkill =  g_SkillManager.GetSkill(m_Skills[i].SkillId, 1);
			}
			else 
			{
				pSkill =  g_SkillManager.GetSkill(m_Skills[i].SkillId, m_Skills[i].SkillLevel);
			}
			
			if (!pSkill)
				continue;
			eSkillStyle eStyle = (eSkillStyle)pSkill->GetSkillStyle();
			
			switch(eStyle)
			{
			case SKILL_SS_Missles:			//	子弹类		本技能用于发送子弹类
			case SKILL_SS_Melee:
			case SKILL_SS_InitiativeNpcState:	//	主动类		本技能用于改变当前Npc的主动状态
			case SKILL_SS_PassivityNpcState:		//	被动类		本技能用于改变Npc的被动状态
				{
					pOrdinSkill = (KSkill * ) pSkill;
					if (pOrdinSkill->IsPhysical())
						continue;

				}break;
			case SKILL_SS_Thief:					//	偷窃类
				{
					
				}
				break;
			default:
				continue;
			}
			
            KUiSkillData * pSkill = pSkillList + nCount;
			pSkill->uGenre = CGOG_SKILL_FIGHT;
			pSkill->uId = m_Skills[i].SkillId;
			pSkill->nLevel = m_Skills[i].SkillLevel;
			if ((++nCount) == 50)
			{
				g_DebugLog("Npc的技能数量超过了限制50！！！，请检查!!!");			
				break;
			}
		}
	}
	return nCount;
}

int KSkillList::GetLeftSkillSortList(KUiSkillData* pSkillList)
{
	if (!pSkillList) return 0;
	memset(pSkillList, 0, sizeof(KUiSkillData) * 65);
	int nCount = 1;
	
	pSkillList->uGenre = CGOG_SKILL_SHORTCUT;
	pSkillList->uId = Npc[Player[CLIENT_PLAYER_INDEX].m_nIndex].GetCurActiveWeaponSkill();//物理技能置前!
	pSkillList->nData = 0;
	
	ISkill * pISkill = NULL;
	KSkill * pOrdinSkill = NULL;
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (m_Skills[i].SkillId && m_Skills[i].SkillLevel > 0)
		{
			_ASSERT(m_Skills[i].SkillId > 0);
			pISkill = g_SkillManager.GetSkill(m_Skills[i].SkillId, m_Skills[i].SkillLevel);
			if (!pISkill)
				continue;
			
			eSkillStyle eStyle = (eSkillStyle)pISkill->GetSkillStyle();
			switch(eStyle)
			{
			case SKILL_SS_Missles:			//	子弹类		本技能用于发送子弹类
			case SKILL_SS_Melee:
			case SKILL_SS_InitiativeNpcState:	//	主动类		本技能用于改变当前Npc的主动状态
			case SKILL_SS_PassivityNpcState:		//	被动类		本技能用于改变Npc的被动状态
				{
					pOrdinSkill = (KSkill * ) pISkill;
					if ((!pOrdinSkill->IsPhysical()) &&	
						(pOrdinSkill->GetSkillLRInfo() == BothSkill) || 
						(pOrdinSkill->GetSkillLRInfo() == leftOnlySkill)
						)
					{
						
					}
					else 
						continue;
					
				}
				break;
				
			case SKILL_SS_Thief:
				{
					
					continue;
					
				}break;
			}
			
			KUiSkillData * pSkill = pSkillList + nCount;
			pSkill->uGenre = CGOG_SKILL_SHORTCUT;
			pSkill->uId = m_Skills[i].SkillId;
			pSkill->nData = nCount / 8;
			nCount ++;
			if (nCount >= 65)
				break;
			
		}
	}
	return nCount;
}

int KSkillList::GetRightSkillSortList(KUiSkillData* pSkillList)
{
	if (!pSkillList) return 0;
	memset(pSkillList, 0, sizeof(KUiSkillData) * 65);
	
	int nCount = 1;
	pSkillList->uGenre = CGOG_SKILL_SHORTCUT;
	//pSkillList->uId = (unsigned int)-1;//物理技能置前!
	pSkillList->uId = Npc[Player[CLIENT_PLAYER_INDEX].m_nIndex].GetCurActiveWeaponSkill();//物理技能置前!;	 
	pSkillList->nData = 0;
	ISkill * pISkill = NULL;
	KSkill * pOrdinSkill = NULL;
	
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (m_Skills[i].SkillId && m_Skills[i].SkillLevel > 0)
		{
			_ASSERT(m_Skills[i].SkillId > 0);
			pISkill  =  g_SkillManager.GetSkill(m_Skills[i].SkillId, m_Skills[i].SkillLevel);
			if(!pISkill)
				continue;

			eSkillStyle eStyle = ( eSkillStyle ) pISkill->GetSkillStyle();
			
			switch(eStyle)
			{
			case SKILL_SS_Missles:			//	子弹类		本技能用于发送子弹类
			case SKILL_SS_Melee:
			case SKILL_SS_InitiativeNpcState:	//	主动类		本技能用于改变当前Npc的主动状态
			case SKILL_SS_PassivityNpcState:		//	被动类		本技能用于改变Npc的被动状态
				{
					pOrdinSkill = (KSkill * ) pISkill;
					if (
						(!pOrdinSkill->IsPhysical()) && 
						(pOrdinSkill->GetSkillLRInfo() == BothSkill) || 
						(pOrdinSkill->GetSkillLRInfo() == RightOnlySkill)
						)
					{
						
					}
					else 
						continue;
				}
				break;
				
			case SKILL_SS_Thief:
				{
					
				}break;
			}
			
			
			
			KUiSkillData * pSkill = pSkillList + nCount;
			pSkill->uGenre = CGOG_SKILL_SHORTCUT;
			pSkill->uId = m_Skills[i].SkillId;
			pSkill->nData = nCount / 8;
			nCount ++;
			if (nCount >= 65)
				break;
			
		}
	}
	return nCount;
}




int KSkillList::GetSkillPosition(int nSkillId)//获得技能在技能界面的位置
{
	if (nSkillId <= 0) return -1;
	KSkill * pOrdinSkill = NULL;
	
	ISkill * pISkill = NULL;
	int nCount = 0;
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		_ASSERT(m_Skills[i].SkillLevel >= 0);
		if (m_Skills[i].SkillId<= 0) continue;
		
		if (m_Skills[i].SkillLevel == 0)
		{
			pISkill = g_SkillManager.GetSkill(m_Skills[i].SkillId, 1);
		}
		else
		{
			pISkill =  g_SkillManager.GetSkill(m_Skills[i].SkillId, m_Skills[i].SkillLevel);
		}

		if (!pISkill) 
        	continue ;


		eSkillStyle eStyle = ( eSkillStyle ) pISkill->GetSkillStyle();
		switch(eStyle)
		{
		case SKILL_SS_Missles:			//	子弹类		本技能用于发送子弹类
		case SKILL_SS_Melee:
		case SKILL_SS_InitiativeNpcState:	//	主动类		本技能用于改变当前Npc的主动状态
		case SKILL_SS_PassivityNpcState:		//	被动类		本技能用于改变Npc的被动状态
			{
				pOrdinSkill = (KSkill * ) pISkill;
				if (
					(!m_Skills[i].SkillId) || 
					(pOrdinSkill->IsPhysical())
					)
				{
					continue;
				}
			}
			break;
			
		case SKILL_SS_Thief:
			{
				
				
			}break;
		}
		
		
		if (m_Skills[i].SkillId == nSkillId)
		{
			return nCount;
		}
		nCount ++;
		
	}
	return nCount;
}
#endif

#ifdef _SERVER
// Restore player skills without invoking the legacy passive-skill executor.
int KSkillList::LoadPlayerSkill(int nSkillID, int nLevel)
{
	if (nSkillID <= 0 || nSkillID >= MAX_SKILL)
		return 0;
	if (nLevel < 0 || nLevel >= MAX_SKILLLEVEL)
		return 0;

	int nIndex = FindSame(nSkillID);
	if (!nIndex)
	{
		nIndex = FindFree();
		if (!nIndex)
			return 0;
		memset(&m_Skills[nIndex], 0, sizeof(m_Skills[nIndex]));
	}

	m_Skills[nIndex].SkillId = nSkillID;
	m_Skills[nIndex].SkillLevel = nLevel;
	m_Skills[nIndex].CurrentSkillLevel = nLevel;
	m_Skills[nIndex].SkillStatus = nLevel > 1 ? 3 : 1;
	return nIndex;
}

void KSkillList::ApplyPlayerPassiveSkills()
{
	if (m_nNpcIndex <= 0 || m_nNpcIndex >= MAX_NPC || !Npc[m_nNpcIndex].IsPlayer())
		return;
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (m_Skills[i].SkillId <= 0)
			continue;
		KSkill* pSkill = (KSkill*)g_SkillManager.GetSkill(m_Skills[i].SkillId,
			max(1, GetCurrentLevel(m_Skills[i].SkillId)));
		if (!pSkill || pSkill->GetSkillStyle() != SKILL_SS_PassivityNpcState)
			continue;
		// 状态3表示已启用；装备门槛变化后同步撤销旧的永久状态。
		if (m_Skills[i].SkillStatus == 3 && m_Skills[i].CurrentSkillLevel > 0 &&
			pSkill->MatchesWeapon(m_nNpcIndex))
			pSkill->ApplyStateSkill(m_nNpcIndex);
		else
			Npc[m_nNpcIndex].RemoveStateSkillEffect(m_Skills[i].SkillId);
	}
}

BOOL KSkillList::LoadPlayerInitialSkills(int nSeries)
{
	static const char *szSections[] = { "Knight", "Enchanter", "Monstrous" };
	if (nSeries < 0 || nSeries >= 3)
		return FALSE;

	KIniFile IniFile;
	char szInitialSkills[2048];
	if (!IniFile.Load("\\settings\\playerskills.ini") ||
		!IniFile.GetString(szSections[nSeries], "InitSkills", "", szInitialSkills, sizeof(szInitialSkills)))
	{
		printf("[Skills] Cannot load InitSkills for profession %d.\n", nSeries);
		return FALSE;
	}

	char *pEnd = NULL;
	long nCount = strtol(szInitialSkills, &pEnd, 10);
	if (nCount <= 0 || nCount >= MAX_NPCSKILL || !pEnd || *pEnd != ':')
		return FALSE;

	int nSkillIds[MAX_NPCSKILL];
	int nSkillLevels[MAX_NPCSKILL];
	char *pCurrent = pEnd + 1;
	int i;
	for (i = 0; i < nCount; i++)
	{
		long nSkillID = strtol(pCurrent, &pEnd, 10);
		if (pEnd == pCurrent || nSkillID <= 0 || nSkillID >= MAX_SKILL || *pEnd != '&')
			return FALSE;
		pCurrent = pEnd + 1;
		long nLevel = strtol(pCurrent, &pEnd, 10);
		if (pEnd == pCurrent || nLevel < 0 || nLevel >= MAX_SKILLLEVEL)
			return FALSE;
		if ((i + 1 < nCount && *pEnd != ',') || (i + 1 == nCount && *pEnd != 0))
			return FALSE;
		for (int j = 0; j < i; j++)
		{
			if (nSkillIds[j] == nSkillID)
				return FALSE;
		}
		nSkillIds[i] = nSkillID;
		nSkillLevels[i] = nLevel;
		pCurrent = pEnd + 1;
	}

	for (i = 0; i < nCount; i++)
	{
		int nIndex = FindSame(nSkillIds[i]);
		if (!nIndex)
			nIndex = LoadPlayerSkill(nSkillIds[i], nSkillLevels[i]);
		if (!nIndex)
		{
			printf("[Skills] Initial skill list is full for profession %d.\n", nSeries);
			return FALSE;
		}

		BYTE btStatus = 1;
		if (i == 0)
			btStatus = 3;
		else
		{
			switch (nSkillIds[i])
			{
			case 553:
			case 554:
			case 558:
			case 559:
			case 561:
			case 562:
			case 564:
				btStatus = 2;
				break;
			}
		}
		if (m_Skills[nIndex].SkillStatus != 3)
			m_Skills[nIndex].SkillStatus = btStatus;
	}
	return TRUE;
}

int	KSkillList::UpdateDBSkillList(BYTE * pSkillBuffer)
{
	if (!pSkillBuffer)
		return -1;
	
	int nCount = 0;

	TDBSkillData * pSkillData = (TDBSkillData *) pSkillBuffer;
	for (int i = 0 ; i < MAX_NPCSKILL; i ++)
	{
		if (m_Skills[i].SkillId > 0)
		{
			pSkillData->m_nSkillId = m_Skills[i].SkillId;
			pSkillData->m_nSkillLevel = m_Skills[i].SkillLevel;
			pSkillData++;
			nCount ++;
		}
	}
	return nCount;
}

int		KSkillList::ClearSkillLevel(bool bClearPhysicalLevel)
{
	int nTotalSkillPoint = 0;

	for (int i = 0 ; i < MAX_NPCSKILL; i ++)
	{
		if (m_Skills[i].SkillId > 0)
		{
			ISkill * pSkill = g_SkillManager.GetSkill(m_Skills[i].SkillId, 1);
			if (pSkill)
			{
				if (pSkill->IsPhysical())
					continue;
				nTotalSkillPoint += m_Skills[i].SkillLevel;
				m_Skills[i].SkillLevel = 0;
				m_Skills[i].CurrentSkillLevel = 0;
			}
		}
	}
	return nTotalSkillPoint;
}
#endif

#ifdef _SERVER
BOOL KSkillList::IsPlayerSkillLearned(int nSkillID)
{
	int nIndex = FindSame(nSkillID);
	return nIndex && m_Skills[nIndex].SkillStatus == 3 && m_Skills[nIndex].SkillLevel > 0;
}

BOOL KSkillList::CanLearnPlayerSkill(int nSkillID)
{
	const PLAYER_SKILL_RULE* pRule = g_SkillManager.GetPlayerRule(nSkillID);
	if (!pRule || pRule->BaseSkill || pRule->NewBirthSkill || !FindSame(nSkillID) ||
		m_nNpcIndex <= 0 || !Npc[m_nNpcIndex].IsPlayer() ||
		Npc[m_nNpcIndex].m_Level < pRule->PlayerLevel)
		return FALSE;
	for (int i = 0; pRule->UseSkillParent && i < pRule->ParentCount; i++)
	{
		if (!IsPlayerSkillLearned(pRule->ParentId[i]) ||
			GetLevel(pRule->ParentId[i]) < pRule->ParentLevel[i])
			return FALSE;
	}
	if (IsPlayerSkillLearned(nSkillID))
	{
		int nLevel = GetLevel(nSkillID);
		if (nLevel < 10 || nLevel >= 13 || nLevel >= pRule->MaxLevel)
			return FALSE;
		for (int i = 0; i < pRule->ActiveCount; i++)
		{
			if (pRule->ActiveLevel[i] == nLevel)
				return TRUE;
		}
		return FALSE;
	}
	return TRUE;
}

BOOL KSkillList::LearnPlayerSkill(int nSkillID)
{
	if (!CanLearnPlayerSkill(nSkillID))
		return FALSE;
	int nIndex = FindSame(nSkillID);
	NPCSKILL& Skill = m_Skills[nIndex];
	const PLAYER_SKILL_RULE* pRule = g_SkillManager.GetPlayerRule(nSkillID);
	if (Skill.SkillStatus != 3)
	{
		Skill.SkillLevel = 1;
		Skill.TotalExp = 0;
		Skill.SkillStatus = 3;
	}
	else
	{
		if (Skill.SkillLevel < 10 || Skill.SkillLevel >= 13 || Skill.SkillLevel >= pRule->MaxLevel)
			return FALSE;
		BOOL bActiveLevel = FALSE;
		for (int i = 0; i < pRule->ActiveCount; i++)
		{
			if (pRule->ActiveLevel[i] == Skill.SkillLevel)
				bActiveLevel = TRUE;
		}
		if (!bActiveLevel)
			return FALSE;
		Skill.SkillLevel++;
		if (Skill.TotalExp < pRule->LevelExp[Skill.SkillLevel - 1])
			Skill.TotalExp = pRule->LevelExp[Skill.SkillLevel - 1];
	}
	RefreshPlayerSkillLevels();
	SyncPlayerSkill(nIndex);
	return TRUE;
}

void KSkillList::SyncPlayerSkill(int nIndex)
{
	if (nIndex <= 0 || nIndex >= MAX_NPCSKILL || m_nNpcIndex <= 0 ||
		!Npc[m_nNpcIndex].IsPlayer() || !g_pServer)
		return;
	int nPlayer = Npc[m_nNpcIndex].GetPlayerIdx();
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER || Player[nPlayer].m_nNetConnectIdx < 0)
		return;
	const NPCSKILL& Skill = m_Skills[nIndex];
	GAME_SKILL_LEVEL_SYNC Sync;
	Sync.ProtocolType = 0x74;
	Sync.SkillId = Skill.SkillId;
	Sync.SkillLevel = Skill.SkillLevel;
	Sync.SkillStatus = Skill.SkillStatus;
	Sync.TotalExp = Skill.TotalExp;
	g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, &Sync, sizeof(Sync));
}

BOOL KSkillList::IncreasePlayerSkillExp(int nSkillID, DWORD dwExp)
{
	int nIndex = FindSame(nSkillID);
	const PLAYER_SKILL_RULE* pRule = g_SkillManager.GetPlayerRule(nSkillID);
	if (!nIndex || !dwExp || !pRule || pRule->BaseSkill || pRule->NewBirthSkill ||
		!IsPlayerSkillLearned(nSkillID) || pRule->MaxLevel <= 1)
		return FALSE;
	NPCSKILL& Skill = m_Skills[nIndex];
	int nMaxLevel = pRule->MaxLevel < 10 ? pRule->MaxLevel : 10;
	if (Skill.SkillLevel >= nMaxLevel)
		return FALSE;
	DWORD dwMaxExp = pRule->LevelExp[nMaxLevel - 1];
	if (Skill.TotalExp >= dwMaxExp)
		return FALSE;
	if (dwExp > dwMaxExp - Skill.TotalExp)
		dwExp = dwMaxExp - Skill.TotalExp;
	Skill.TotalExp += dwExp;
	int nOldLevel = Skill.SkillLevel;
	while (Skill.SkillLevel < nMaxLevel && Skill.TotalExp >= pRule->LevelExp[Skill.SkillLevel])
		Skill.SkillLevel++;
	if (nOldLevel != Skill.SkillLevel)
	{
		RefreshPlayerSkillLevels();
		SyncPlayerSkill(nIndex);
	}
	else if (m_nNpcIndex > 0 && Npc[m_nNpcIndex].IsPlayer() && g_pServer)
	{
		int nPlayer = Npc[m_nNpcIndex].GetPlayerIdx();
		if (nPlayer > 0 && nPlayer < MAX_PLAYER && Player[nPlayer].m_nNetConnectIdx >= 0)
		{
			GAME_SKILL_EXP_SYNC Sync;
			Sync.ProtocolType = 0xB9;
			Sync.SkillId = Skill.SkillId;
			Sync.TotalExp = Skill.TotalExp;
			g_pServer->PackDataToClient(Player[nPlayer].m_nNetConnectIdx, &Sync, sizeof(Sync));
		}
	}
	return TRUE;
}

// 命中技能与战斗精通由实际命中修炼；其余辅助技能随实际打怪所得经验修炼。
void KSkillList::AddMonsterSkillExp(int nExp)
{
	if (nExp <= 0)
		return;
	ULONGLONG ullExp = (ULONGLONG)nExp * g_SkillManager.GetSupportSkillExpRate() *
		GetSkillExpMultiplier() / 100;
	DWORD dwExp = ullExp > MAXDWORD ? MAXDWORD : (DWORD)ullExp;
	if (!dwExp)
		return;
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (!IsPlayerSkillLearned(m_Skills[i].SkillId))
			continue;
		KSkill* pSkill = (KSkill*)g_SkillManager.GetSkill(m_Skills[i].SkillId, GetCurrentLevel(m_Skills[i].SkillId));
		if (pSkill && !pSkill->IsTargetEnemy() && !pSkill->IsAura() &&
			!pSkill->IsElementalMastery() && !pSkill->IsWeaponMastery())
			IncreasePlayerSkillExp(m_Skills[i].SkillId, dwExp);
	}
}

// 命中修炼依据原技能伤害类型，不能让装备附带伤害训练无关的精通。
void KSkillList::AddHitSkillExp(int nSkillID)
{
	KSkill* pAttack = (KSkill*)g_SkillManager.GetSkill(nSkillID, GetCurrentLevel(nSkillID));
	if (!pAttack || !pAttack->IsTargetEnemy())
		return;
	DWORD dwExp = g_SkillManager.GetHitSkillExpRate() * GetSkillExpMultiplier();
	IncreasePlayerSkillExp(nSkillID, dwExp);
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (!IsPlayerSkillLearned(m_Skills[i].SkillId))
			continue;
		KSkill* pPassive = (KSkill*)g_SkillManager.GetSkill(m_Skills[i].SkillId, GetCurrentLevel(m_Skills[i].SkillId));
		if (pPassive && pPassive->EnhancesSkillDamage(pAttack, m_nNpcIndex))
			IncreasePlayerSkillExp(m_Skills[i].SkillId, dwExp);
	}
}

void KSkillList::ResetAuraSkillExp()
{
	m_nExpAuraId = 0;
	m_nAuraExpFrames = 0;
}

void KSkillList::UpdateAuraSkillExp()
{
	if (m_nNpcIndex <= 0 || !Npc[m_nNpcIndex].IsPlayer())
		return;
	KNpc& Owner = Npc[m_nNpcIndex];
	int nSkillID = Owner.m_ActiveAuraID;
	KSkill* pSkill = (KSkill*)g_SkillManager.GetSkill(nSkillID, GetCurrentLevel(nSkillID));
	if (!pSkill || !pSkill->IsAura() || !IsPlayerSkillLearned(nSkillID) ||
		Owner.m_RegionIndex < 0 || Owner.m_bExchangeServer || Owner.m_CurrentLife <= 0 ||
		Owner.m_Doing == do_death || Owner.m_Doing == do_revive)
	{
		ResetAuraSkillExp();
		return;
	}
	if (m_nExpAuraId != nSkillID)
	{
		ResetAuraSkillExp();
		m_nExpAuraId = nSkillID;
	}
	// GameServer 为18帧/秒；只累计实际激活帧，不补算离线和切图等待时间。
	if (++m_nAuraExpFrames >= 18 * 10)
	{
		m_nAuraExpFrames = 0;
		IncreasePlayerSkillExp(nSkillID, GetSkillExpMultiplier());
	}
}

void KSkillList::RefreshPlayerSkillLevels()
{
	DWORD dwNow = (DWORD)time(NULL);
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		NPCSKILL& Skill = m_Skills[i];
		if (!Skill.SkillId)
			continue;
		int nBonus = 0;
		if (Skill.SkillStatus == 3)
		{
			for (int j = 0; j < 16; j++)
			{
				const PLAYER_SKILL_BUFF& Buff = m_SkillBuffs[j];
				if (Buff.EndTime <= dwNow || (Buff.SkillId && Buff.SkillId != Skill.SkillId))
					continue;
				if (Buff.BuffId == 7 || Buff.BuffId == 8 || Buff.BuffId == 23)
					nBonus += Buff.Value;
			}
		}
		if (nBonus > 2)
			nBonus = 2;
		Skill.CurrentSkillLevel = Skill.SkillLevel + nBonus;
		if (Skill.CurrentSkillLevel >= MAX_SKILLLEVEL)
			Skill.CurrentSkillLevel = MAX_SKILLLEVEL - 1;
	}
	ApplyPlayerPassiveSkills();
}

int KSkillList::GetPlayerSkillBuffTime(DWORD dwBuffId)
{
	DWORD dwNow = (DWORD)time(NULL);
	for (int i = 0; i < 16; i++)
	{
		if (m_SkillBuffs[i].BuffId == dwBuffId && m_SkillBuffs[i].EndTime > dwNow)
			return m_SkillBuffs[i].EndTime - dwNow;
	}
	return 0;
}

int KSkillList::GetSkillExpMultiplier()
{
	int nMultiplier = 1;
	for (int i = 0; i < 16; i++)
	{
		const PLAYER_SKILL_BUFF_RULE* pRule = g_SkillManager.GetPlayerBuffRule(m_SkillBuffs[i].BuffId);
		if (pRule && GetPlayerSkillBuffTime(pRule->BuffId) && pRule->Multiplier > nMultiplier)
			nMultiplier = pRule->Multiplier;
	}
	return nMultiplier;
}

BOOL KSkillList::AddPlayerSkillBuff(DWORD dwBuffId, int nSeconds, int nSkillId, int nValue, BOOL bAddTime)
{
	if (nSeconds <= 0 || nSeconds > 100 * 3600 || nValue <= 0 || nValue > 5 ||
		(dwBuffId == 8 && (!nSkillId || !IsPlayerSkillLearned(nSkillId))) ||
		(dwBuffId != 8 && nSkillId != 0) ||
		(dwBuffId != 7 && dwBuffId != 8 && dwBuffId != 23 && dwBuffId != 107 && dwBuffId != 271 && dwBuffId != 272))
		return FALSE;
	ExpirePlayerSkillBuffs();
	int nFree = -1;
	for (int i = 0; i < 16; i++)
	{
		if (m_SkillBuffs[i].BuffId == dwBuffId)
		{
			nFree = i;
			break;
		}
		if (!m_SkillBuffs[i].BuffId && nFree < 0)
			nFree = i;
	}
	if (nFree < 0)
		return FALSE;
	int nLeft = bAddTime ? GetPlayerSkillBuffTime(dwBuffId) : 0;
	const PLAYER_SKILL_BUFF_RULE* pRule = g_SkillManager.GetPlayerBuffRule(dwBuffId);
	int nMaxDuration = pRule ? pRule->MaxDuration : 100 * 3600;
	if (nLeft > nMaxDuration - nSeconds)
		return FALSE;
	PLAYER_SKILL_BUFF& Buff = m_SkillBuffs[nFree];
	Buff.BuffId = dwBuffId;
	Buff.EndTime = (DWORD)time(NULL) + nLeft + nSeconds;
	Buff.SkillId = nSkillId;
	Buff.Value = nValue;
	RefreshPlayerSkillLevels();
	SyncPlayerSkillBuffs();
	return TRUE;
}

BOOL KSkillList::RemovePlayerSkillBuff(DWORD dwBuffId)
{
	for (int i = 0; i < 16; i++)
	{
		if (m_SkillBuffs[i].BuffId != dwBuffId)
			continue;
		memset(&m_SkillBuffs[i], 0, sizeof(m_SkillBuffs[i]));
		RefreshPlayerSkillLevels();
		SyncPlayerSkillBuffs();
		return TRUE;
	}
	return FALSE;
}

void KSkillList::ExpirePlayerSkillBuffs()
{
	DWORD dwNow = (DWORD)time(NULL);
	BOOL bChanged = FALSE;
	for (int i = 0; i < 16; i++)
	{
		if (m_SkillBuffs[i].BuffId && m_SkillBuffs[i].EndTime <= dwNow)
		{
			memset(&m_SkillBuffs[i], 0, sizeof(m_SkillBuffs[i]));
			bChanged = TRUE;
		}
	}
	if (bChanged)
	{
		RefreshPlayerSkillLevels();
		SyncPlayerSkillBuffs();
	}
}

void KSkillList::SyncPlayerSkillBuffs(BOOL bInitial)
{
	if (m_nNpcIndex <= 0 || !Npc[m_nNpcIndex].IsPlayer() || !g_pServer)
		return;
	int nPlayer = Npc[m_nNpcIndex].GetPlayerIdx();
	if (nPlayer <= 0 || nPlayer >= MAX_PLAYER || Player[nPlayer].m_nNetConnectIdx < 0)
		return;
	int nClient = Player[nPlayer].m_nNetConnectIdx;
	// 全量E5不能覆盖已有项；增量先撤销再添加，避免反复叠加客户端技能等级。
	if (!bInitial)
	{
		int Kinds[] = {7, 8, 23};
		for (int i = 0; i < 3; i++)
		{
			GAME_SKILL_BUFF_CHANGE Sync;
			memset(&Sync, 0, sizeof(Sync));
			Sync.ProtocolType = 0xE5;
			Sync.Length = sizeof(Sync) - 1;
			Sync.SubType = 3;
			Sync.Buff.BuffKind = Kinds[i];
			g_pServer->PackDataToClient(nClient, &Sync, sizeof(Sync));
		}
	}
	GAME_SKILL_BUFF_SYNC Sync;
	memset(&Sync, 0, sizeof(Sync));
	Sync.ProtocolType = 0xE5;
	for (int i = 0; i < 16; i++)
	{
		const PLAYER_SKILL_BUFF& Buff = m_SkillBuffs[i];
		int nLeft = GetPlayerSkillBuffTime(Buff.BuffId);
		if (!nLeft || (Buff.BuffId != 7 && Buff.BuffId != 8 && Buff.BuffId != 23))
			continue;
		GAME_SKILL_BUFF_DATA& Data = Sync.Buffs[Sync.Count++];
		Data.RemainingSeconds = nLeft;
		Data.BuffKind = (BYTE)Buff.BuffId;
		Data.Value1 = Buff.BuffId == 8 ? Buff.SkillId : Buff.Value;
		Data.Value2 = Buff.BuffId == 8 ? Buff.Value : 0;
	}
	Sync.Length = 4 + sizeof(GAME_SKILL_BUFF_DATA) * Sync.Count;
	g_pServer->PackDataToClient(nClient, &Sync, Sync.Length + 1);

	// IB状态与经典技能加级状态分开同步；实例标识1..3只用于本模块三种临仙露。
	// 新增记录+2的DWORD须与尾部删除标识一致；+10的BYTE是属性参数，不是实例号。
	// 同包先删除旧实例再加入当前剩余时间，重饮及其他技能增益变化不会堆叠图标。
	BYTE Buffer[5 + 3 * sizeof(GAME_IB_BUFF_DATA) + 3 * sizeof(DWORD)];
	memset(Buffer, 0, sizeof(Buffer));
	Buffer[0] = 0xF7;
	int BuffIds[] = {107, 271, 272};
	int nOffset = 5;
	for (int i = 0; i < 3; i++)
	{
		int nLeft = GetPlayerSkillBuffTime(BuffIds[i]);
		if (!nLeft)
			continue;
		GAME_IB_BUFF_DATA Data;
		memset(&Data, 0, sizeof(Data));
		Data.TemplateId = BuffIds[i];
		Data.RemainingSeconds = nLeft;
		Data.InstanceId = i + 1;
		Data.unknown_1 = 1;
		memcpy(Buffer + nOffset, &Data, sizeof(Data));
		nOffset += sizeof(Data);
		Buffer[4]++;
	}
	for (DWORD dwInstance = 1; dwInstance <= 3; dwInstance++)
	{
		memcpy(Buffer + nOffset, &dwInstance, sizeof(dwInstance));
		nOffset += sizeof(dwInstance);
	}
	WORD wLength = nOffset - 1;
	memcpy(Buffer + 1, &wLength, sizeof(wLength));
	g_pServer->PackDataToClient(nClient, Buffer, nOffset);
}
#endif
