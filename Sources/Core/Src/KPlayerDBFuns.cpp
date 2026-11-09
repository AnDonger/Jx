#include "KCore.h"

#ifdef _SERVER
#include "KEngine.h"
#include "KSubWorldSet.h"
#include "KSubWorld.h"
#include "KPlayer.h"
#ifndef _STANDALONE
#include "../../../lib/S3DBInterface.h"
#else
#include "S3DBInterface.h"
#endif
#include "KNpc.h"
#include "KItem.h"
#include "KItemList.h"
#include "KItemGenerator.h"
#include "KItemSet.h"
#include "KNpcSet.h"
#include "KPlayerSet.h"
#include "KItemChangeRes.h"
#include <time.h>
//#include "MyAssert.H"
#include "KTaskFuns.h"


// 可选技能扩展紧随标准战斗技能，dwLSkillOffset 指向扩展之后。
// 保留旧 TDBSkillData ABI；没有扩展的旧存档仍按初始化列表加载。
struct PLAYER_SKILL_EXT_HEADER
{
	DWORD Magic;
	DWORD Version;
	DWORD SkillCount;
	DWORD BuffCount;
};

struct PLAYER_SKILL_EXT_RECORD
{
	DWORD SkillId;
	DWORD SkillStatus;
	DWORD TotalExp;
};

// SPG1版本3只扩展技能记录；旧版1/2的12字节记录和宠物数据仍可读取。
struct PLAYER_SKILL_EXT_RECORD_V3
{
	PLAYER_SKILL_EXT_RECORD Skill;
	DWORD CooldownEndLow;
	DWORD CooldownEndHigh;
};

static_assert(sizeof(PLAYER_SKILL_EXT_RECORD_V3) == 20, "SPG1 v3 skill record");

static const DWORD PLAYER_SKILL_EXT_MAGIC = 0x31475053; // SPG1

static BOOL LoadPlayerSkillExtension(const TRoleData* pRole, KSkillList& List, KPlayer& Player)
{
	DWORD dwOffset = pRole->dwFSkillOffset + pRole->nFightSkillCount * sizeof(TDBSkillData);
	if (dwOffset > pRole->dwLSkillOffset || pRole->dwLSkillOffset > pRole->dwDataLen)
		return FALSE;
	DWORD dwLength = pRole->dwLSkillOffset - dwOffset;
	if (!dwLength)
		return TRUE;
	if (dwLength < sizeof(PLAYER_SKILL_EXT_HEADER))
		return FALSE;
	const BYTE* pData = (const BYTE*)pRole + dwOffset;
	PLAYER_SKILL_EXT_HEADER Header;
	memcpy(&Header, pData, sizeof(Header));
	DWORD dwRecordSize = Header.Version == 3 ? sizeof(PLAYER_SKILL_EXT_RECORD_V3) : sizeof(PLAYER_SKILL_EXT_RECORD);
	if (Header.Magic != PLAYER_SKILL_EXT_MAGIC || (Header.Version < 1 || Header.Version > 3) ||
		Header.SkillCount != pRole->nFightSkillCount || Header.SkillCount >= MAX_NPCSKILL || Header.BuffCount > 16 ||
		dwLength != sizeof(Header) + Header.SkillCount * dwRecordSize +
		Header.BuffCount * sizeof(PLAYER_SKILL_BUFF) +
		(Header.Version >= 2 ? sizeof(PLAYER_PET_DATA) : 0))
		return FALSE;
	BOOL Seen[MAX_NPCSKILL];
	PLAYER_SKILL_EXT_RECORD Records[MAX_NPCSKILL];
	ULONGLONG Cooldowns[MAX_NPCSKILL] = {};
	PLAYER_SKILL_BUFF Buffs[16];
	memset(Seen, 0, sizeof(Seen));
	memset(Records, 0, sizeof(Records));
	memset(Buffs, 0, sizeof(Buffs));
	pData += sizeof(Header);
	for (DWORD i = 0; i < Header.SkillCount; i++)
	{
		PLAYER_SKILL_EXT_RECORD Record;
		memcpy(&Record, pData, sizeof(Record));
		int nIndex = List.FindSame(Record.SkillId);
		if (!nIndex || Seen[nIndex] || Record.SkillStatus < 1 || Record.SkillStatus > 4)
			return FALSE;
		if (Header.Version == 3)
		{
			PLAYER_SKILL_EXT_RECORD_V3 RecordV3;
			memcpy(&RecordV3, pData, sizeof(RecordV3));
			Cooldowns[nIndex] = ((ULONGLONG)RecordV3.CooldownEndHigh << 32) | RecordV3.CooldownEndLow;
			ULONGLONG ullNow = KSkillList::GetPlayerSkillCooldownClock();
			if (Cooldowns[nIndex] > ullNow && Cooldowns[nIndex] - ullNow > MAXDWORD)
				return FALSE;
		}
		pData += dwRecordSize;
		Seen[nIndex] = TRUE;
		Records[nIndex] = Record;
	}
	DWORD dwNow = (DWORD)time(NULL);
	for (DWORD i = 0; i < Header.BuffCount; i++)
	{
		PLAYER_SKILL_BUFF Buff;
		memcpy(&Buff, pData, sizeof(Buff));
		pData += sizeof(Buff);
		const PLAYER_SKILL_BUFF_RULE* pRule = g_SkillManager.GetPlayerBuffRule(Buff.BuffId);
		if (pRule && !pRule->OfflineTime)
		{
			// 在线计时的存档字段保存剩余秒数，重登重新建立当前会话到期时间。
			if (Buff.EndTime > (DWORD)pRule->MaxDuration)
				return FALSE;
			Buff.EndTime += dwNow;
		}
		if ((Buff.BuffId != 7 && Buff.BuffId != 8 && Buff.BuffId != 23 &&
			Buff.BuffId != 107 && Buff.BuffId != 271 && Buff.BuffId != 272) ||
			Buff.Value <= 0 || Buff.Value > 5 || Buff.SkillId < 0 || Buff.SkillId >= MAX_SKILL ||
			(Buff.BuffId != 8 && Buff.SkillId != 0) ||
			(pRule && Buff.Value != pRule->Multiplier) ||
			(Buff.EndTime > dwNow && Buff.EndTime - dwNow > 100 * 3600))
			return FALSE;
		if (Buff.BuffId == 8)
		{
			int nIndex = List.FindSame(Buff.SkillId);
			if (!nIndex || Records[nIndex].SkillStatus != 3)
				return FALSE;
		}
		for (DWORD j = 0; j < i; j++)
		{
			if (Buffs[j].BuffId == Buff.BuffId)
				return FALSE;
			if (pRule && Buff.EndTime > dwNow && Buffs[j].EndTime > dwNow &&
				g_SkillManager.GetPlayerBuffRule(Buffs[j].BuffId))
				return FALSE;
		}
		Buffs[i] = Buff;
	}
	PLAYER_PET_DATA Pet;
	ZeroMemory(&Pet, sizeof(Pet));
	if (Header.Version >= 2)
	{
		memcpy(&Pet, pData, sizeof(Pet));
		if (Pet.Type)
		{
			if (!Player.IsValidPetType(Pet.Type) || Pet.Honor > 100 || Pet.Released > 1 ||
				(Pet.Released && !Pet.Honor) || !Pet.Name[0] || !memchr(Pet.Name, 0, sizeof(Pet.Name)) ||
				(Pet.Authority & ~31) || (Pet.Active & ~Pet.Authority))
				return FALSE;
		}
		else
		{
			PLAYER_PET_DATA Empty;
			ZeroMemory(&Empty, sizeof(Empty));
			if (memcmp(&Pet, &Empty, sizeof(Pet)))
				return FALSE;
		}
	}
	DWORD PreviousCastTimes[MAX_NPCSKILL];
	for (int i = 0; i < MAX_NPCSKILL; i++)
		PreviousCastTimes[i] = List.m_Skills[i].NextCastTime;
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (Seen[i] && !List.RestorePlayerSkillCooldown(Records[i].SkillId, Cooldowns[i]))
		{
			for (int j = 0; j < MAX_NPCSKILL; j++)
				List.m_Skills[j].NextCastTime = PreviousCastTimes[j];
			return FALSE;
		}
	}
	Player.m_Pet = Pet;
	// 完整验证后再恢复状态，损坏的扩展不能写入一半技能或BUFF。
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		if (Seen[i])
		{
			List.m_Skills[i].SkillStatus = (BYTE)Records[i].SkillStatus;
			List.m_Skills[i].TotalExp = Records[i].TotalExp;
		}
	}
	memset(List.m_SkillBuffs, 0, sizeof(List.m_SkillBuffs));
	for (DWORD i = 0; i < Header.BuffCount; i++)
	{
		if (Buffs[i].EndTime > dwNow)
			List.m_SkillBuffs[i] = Buffs[i];
	}
	return TRUE;
}

static int SavePlayerSkillExtension(KSkillList& List, const PLAYER_PET_DATA& Pet, BYTE* pData, DWORD dwCapacity)
{
	DWORD dwNow = (DWORD)time(NULL);
	DWORD dwBuffCount = 0;
	for (int i = 0; i < 16; i++)
	{
		if (List.m_SkillBuffs[i].BuffId && List.m_SkillBuffs[i].EndTime > dwNow)
			dwBuffCount++;
	}
	PLAYER_SKILL_EXT_HEADER Header = {PLAYER_SKILL_EXT_MAGIC, 3, List.GetCount(), dwBuffCount};
	DWORD dwSize = sizeof(Header) + Header.SkillCount * sizeof(PLAYER_SKILL_EXT_RECORD_V3) +
		dwBuffCount * sizeof(PLAYER_SKILL_BUFF) + sizeof(Pet);
	if (dwSize > dwCapacity)
		return -1;
	memcpy(pData, &Header, sizeof(Header));
	pData += sizeof(Header);
	for (int i = 1; i < MAX_NPCSKILL; i++)
	{
		const NPCSKILL& Skill = List.m_Skills[i];
		if (!Skill.SkillId)
			continue;
		ULONGLONG ullEndTime = List.GetPlayerSkillCooldownEndTime(Skill.SkillId);
		PLAYER_SKILL_EXT_RECORD_V3 Record = {{(DWORD)Skill.SkillId, Skill.SkillStatus, Skill.TotalExp},
			(DWORD)ullEndTime, (DWORD)(ullEndTime >> 32)};
		memcpy(pData, &Record, sizeof(Record));
		pData += sizeof(Record);
	}
	for (int i = 0; i < 16; i++)
	{
		PLAYER_SKILL_BUFF Buff = List.m_SkillBuffs[i];
		if (!Buff.BuffId || Buff.EndTime <= dwNow)
			continue;
		const PLAYER_SKILL_BUFF_RULE* pRule = g_SkillManager.GetPlayerBuffRule(Buff.BuffId);
		if (pRule && !pRule->OfflineTime)
			Buff.EndTime -= dwNow;
		memcpy(pData, &Buff, sizeof(Buff));
		pData += sizeof(Buff);
	}
	memcpy(pData, &Pet, sizeof(Pet));
	return dwSize;
}

// 是否将数据库存取档的数据保存下来以供调试
//#define DEBUGOPT_DB_ROLEDATA_OUT 




// 可选物品数量扩展位于标准物品记录后、好友区前；不占用旧记录未知字段。
// 旧存档没有扩展时，每件数量为 1。所有扩展均受角色区间和版本校验。
struct PLAYER_ITEM_COUNT_HEADER
{
	DWORD Magic;
	DWORD Version;
	DWORD ItemCount;
};

static const DWORD PLAYER_ITEM_COUNT_MAGIC = 0x314B5453;

// Version 2保留原数量扩展头，每件保存12字节；不借用TDBItemData未知字段。
struct PLAYER_ITEM_STATE
{
	DWORD UpgradeData;
	BYTE Count;
	BYTE GenerationFlags;
	BYTE StateFlags;
	BYTE BindingMode;
	BYTE ReforgeLuck;
	BYTE Reserved[3];
};
static_assert(sizeof(PLAYER_ITEM_STATE) == 12, "saved item state size");

// 旧 TDBTaskData 的任务号只有一个字节，保持原记录及 nTaskCount 的含义。
// nTaskCount 也是一个字节；255 及以上的任务变量单独扩展，避免满 256 条溢出。
struct PLAYER_TASK_EXT_HEADER
{
	DWORD Magic;
	DWORD Version;
	DWORD Count;
};

struct PLAYER_TASK_EXT_RECORD
{
	DWORD TaskId;
	int Value;
};

static const DWORD PLAYER_TASK_EXT_MAGIC = 0x3158544b;
static const int PLAYER_TASK_EXT_BEGIN = 255;

static BOOL LoadPlayerTaskExtension(KPlayerTask& Task, const BYTE *&pData, DWORD& dwLength)
{
	if (dwLength < sizeof(DWORD))
		return TRUE;
	DWORD dwMagic;
	memcpy(&dwMagic, pData, sizeof(dwMagic));
	if (dwMagic != PLAYER_TASK_EXT_MAGIC)
		return TRUE;
	if (dwLength < sizeof(PLAYER_TASK_EXT_HEADER))
		return FALSE;
	PLAYER_TASK_EXT_HEADER Header;
	memcpy(&Header, pData, sizeof(Header));
	if ((Header.Version != 1 && Header.Version != 2) || Header.Count == 0 ||
		Header.Count > MAX_TASK - PLAYER_TASK_EXT_BEGIN)
		return FALSE;
	DWORD dwSize = sizeof(Header) + Header.Count * sizeof(PLAYER_TASK_EXT_RECORD);
	if (dwSize > dwLength)
		return FALSE;
	DWORD dwPrevious = PLAYER_TASK_EXT_BEGIN - 1;
	for (DWORD i = 0; i < Header.Count; i++)
	{
		PLAYER_TASK_EXT_RECORD Record;
		memcpy(&Record, pData + sizeof(Header) + i * sizeof(Record), sizeof(Record));
		if (Record.TaskId <= dwPrevious || Record.TaskId >= MAX_TASK || Record.Value == 0)
			return FALSE;
		dwPrevious = Record.TaskId;
	}
	for (DWORD i = 0; i < Header.Count; i++)
	{
		PLAYER_TASK_EXT_RECORD Record;
		memcpy(&Record, pData + sizeof(Header) + i * sizeof(Record), sizeof(Record));
		Task.SetSaveVal(Record.TaskId, Record.Value);
	}
	pData += dwSize;
	dwLength -= dwSize;
	return TRUE;
}

static int SavePlayerTaskExtension(KPlayerTask& Task, BYTE *pData, DWORD dwCapacity)
{
	DWORD dwCount = 0;
	for (int i = PLAYER_TASK_EXT_BEGIN; i < MAX_TASK; i++)
	{
		if (Task.nSave[i])
			dwCount++;
	}
	if (!dwCount)
		return 0;
	DWORD dwSize = sizeof(PLAYER_TASK_EXT_HEADER) + dwCount * sizeof(PLAYER_TASK_EXT_RECORD);
	if (dwSize > dwCapacity)
		return -1;
	PLAYER_TASK_EXT_HEADER Header = {PLAYER_TASK_EXT_MAGIC, 1, dwCount};
	memcpy(pData, &Header, sizeof(Header));
	BYTE *pOut = pData + sizeof(Header);
	for (int i = PLAYER_TASK_EXT_BEGIN; i < MAX_TASK; i++)
	{
		if (!Task.nSave[i])
			continue;
		PLAYER_TASK_EXT_RECORD Record = {(DWORD)i, Task.nSave[i]};
		memcpy(pOut, &Record, sizeof(Record));
		pOut += sizeof(Record);
	}
	return dwSize;
}

static BOOL GetPlayerItemCounts(const TRoleData* pRoleData, const BYTE** ppCounts,
	const PLAYER_ITEM_STATE** ppStates)
{
	*ppCounts = NULL;
	*ppStates = NULL;
	if (pRoleData->nItemCount < 0 || pRoleData->nItemCount >= MAX_PLAYER_ITEM ||
		pRoleData->dwItemOffset < 0 || pRoleData->dwFriendOffset < 0)
		return FALSE;
	DWORD nEnd = (DWORD)pRoleData->dwItemOffset +
		(DWORD)pRoleData->nItemCount * sizeof(TDBItemData);
	if (nEnd > (DWORD)pRoleData->dwFriendOffset ||
		(DWORD)pRoleData->dwFriendOffset > pRoleData->dwDataLen)
		return FALSE;
	DWORD nExtra = pRoleData->dwFriendOffset - nEnd;
	if (nExtra == 0)
		return TRUE;
	if (nExtra < sizeof(PLAYER_ITEM_COUNT_HEADER))
		return FALSE;
	PLAYER_ITEM_COUNT_HEADER Header;
	memcpy(&Header, (const BYTE*)pRoleData + nEnd, sizeof(Header));
	if (Header.Magic != PLAYER_ITEM_COUNT_MAGIC ||
		Header.ItemCount != (DWORD)pRoleData->nItemCount || (Header.Version != 1 && Header.Version != 2))
		return FALSE;
	DWORD nStride = Header.Version == 1 ? 1 : sizeof(PLAYER_ITEM_STATE);
	if (nExtra != sizeof(Header) + Header.ItemCount * nStride)
		return FALSE;
	const BYTE* pData = (const BYTE*)pRoleData + nEnd + sizeof(Header);
	if (Header.Version == 1)
	{
		for (int i = 0; i < pRoleData->nItemCount; i++)
			if (!pData[i])
				return FALSE;
		*ppCounts = pData;
	}
	else
	{
		const PLAYER_ITEM_STATE* pStates = (const PLAYER_ITEM_STATE*)pData;
		for (int i = 0; i < pRoleData->nItemCount; i++)
		{
			if (!pStates[i].Count || pStates[i].ReforgeLuck > 100 ||
				pStates[i].Reserved[0] || pStates[i].Reserved[1] || pStates[i].Reserved[2])
				return FALSE;
		}
		*ppStates = pStates;
	}
	return TRUE;
}

KList g_DBMsgList;

int KPlayer::AddDBPlayer(char * szPlayerName, int sex, DWORD * pdwID)
{
	return FALSE;
}

int KPlayer::LoadDBPlayerInfo(BYTE *pPlayerInfo, int &nStep, unsigned int &nParam)
{
	_ASSERT(pPlayerInfo);
	int nRet = 0;
	int nRetValue = 0;
	
	switch(nStep)
	{
	case STEP_BASE_INFO:
		m_bFinishLoading = FALSE;
		//数据库中玩家基本信息表
		m_pCurStatusOffset = pPlayerInfo;
		
		if ((nRet = LoadPlayerBaseInfo(pPlayerInfo, m_pCurStatusOffset, nParam)) == 1)
		{
			nRetValue = SendSyncData(nStep, nParam);
			nStep++; 
			nParam = 0;
		}
		else 
		{
			if (nRet == -1)
			{
				nStep++;
				nParam = 0;
				return 0;
			}
			else
			{
				nRetValue = SendSyncData(nStep, nParam);
			}
		}
		break;
	case STEP_FIGHT_SKILL_LIST:
		//************************************************
		// 玩家的战斗技能列表
		if ((nRet = LoadPlayerFightSkillList(pPlayerInfo, m_pCurStatusOffset, nParam)) == 1)
		{
			nRetValue = SendSyncData(nStep, nParam);
			nStep++;
			nParam = 0;
		}
		else
		{
			if (nRet == -1)
			{
				g_DebugLog("[SkillStorage] Rejected invalid skill data: player=%s", m_PlayerName);
				return 0;
			}
			else
			{
				// Continue a partial database batch; synchronize after the full list is loaded.
				nRetValue = 1;
			}
		}
		break;
	case STEP_LIFE_SKILL_LIST:
		//*************************************************
		// 玩家的生活技能列表
		if ((nRet = LoadPlayerLifeSkilllList(pPlayerInfo, m_pCurStatusOffset, nParam)) == 1)
		{
			nRetValue = SendSyncData(nStep, nParam);
			nStep++;
			nParam = 0;
		}
		else
		{
			if (nRet == -1)
			{
				nStep ++;
				nParam = 0;
				return 0;
			}
			else
			{
				nRetValue = SendSyncData(nStep, nParam);
			}
		}
		
		break;
		
	case STEP_TASK_LIST:
		//*************************************************		
		// 玩家的任务列表
		if ((nRet = LoadPlayerTaskList(pPlayerInfo, m_pCurStatusOffset, nParam)) == 1)
		{
			
			int i = 0;
			for (i  = 0; i < TASKVALUE_MAXWAYPOINT_COUNT; i ++)
			{
				int nWayPoint = m_cTask.nSave[TASKVALUE_SAVEWAYPOINT_BEGIN + i];
				if (nWayPoint)
				{
					KIndexNode * pNewNode = new KIndexNode;
					pNewNode->m_nIndex = nWayPoint;
					m_PlayerWayPointList.AddTail(pNewNode);
				}
			}
			
			for (i  = 0; i < TASKVALUE_MAXSTATION_COUNT / 2; i ++)
			{
				DWORD Stations = 0;
				if (Stations = (DWORD) m_cTask.GetSaveVal(TASKVALUE_SAVESTATION_BEGIN + i))
				{
					int nStation1 = (int) HIWORD(Stations);
					int nStation2 = (int) LOWORD(Stations);
					
					if (nStation1)
					{
						KIndexNode * pNewNode = new KIndexNode;
						pNewNode->m_nIndex = nStation1;
						m_PlayerStationList.AddTail(pNewNode);
					}
					
					if (nStation2)
					{
						KIndexNode * pNewNode = new KIndexNode;
						pNewNode->m_nIndex = nStation2;
						m_PlayerStationList.AddTail(pNewNode);
					}
				}
			}
			g_TimerTask.LoadTask(this);
			nRetValue = SendSyncData(nStep, nParam);
			nStep++;
			nParam = 0;
		}
		else
		{
			if (nRet == -1)
			{
				m_bFinishLoading = FALSE;
				return 0;
			}
			else
			{
				nRetValue = SendSyncData(nStep, nParam);
			}
		}
		break;
	case STEP_ITEM_LIST:
		//*************************************************
		// 玩家的装备列表
		if ((nRet = LoadPlayerItemList(pPlayerInfo, m_pCurStatusOffset, nParam)) == 1)
		{
			// 装备和已启用被动技能加载完成后，再限制旧存档中的当前血蓝。
			// 仅裁掉超过上限的值，受伤角色不会因重登回满。
			KNpc& PlayerNpc = Npc[m_nIndex];
			if (PlayerNpc.m_CurrentLife > PlayerNpc.m_CurrentLifeMax)
				PlayerNpc.m_CurrentLife = PlayerNpc.m_CurrentLifeMax;
			if (PlayerNpc.m_CurrentMana > PlayerNpc.m_CurrentManaMax)
				PlayerNpc.m_CurrentMana = PlayerNpc.m_CurrentManaMax;
			if (PlayerNpc.m_CurrentStamina > PlayerNpc.m_CurrentStaminaMax)
				PlayerNpc.m_CurrentStamina = PlayerNpc.m_CurrentStaminaMax;
			m_bFinishLoading = TRUE;
			nRetValue = SendSyncData(nStep, nParam);
			nStep++;
			nParam = 0;
		}
		else
		{
			if (nRet == -1)
			{
				m_bFinishLoading = FALSE;
				g_DebugLog("[Item] Role item loading failed; saving remains disabled: %s", m_PlayerName);
				return 0;
			}
			else
			{
				nRetValue = SendSyncData(nStep, nParam);
			}
		}
		break;
	default:
		nStep = STEP_SYNC_END;
		break;
	}
	return nRetValue;
}

int KPlayer::UpdateDBPlayerInfo(BYTE* pPlayerInfo)
{
	if (!m_bFinishLoading)
		return -1;

	//注意，存档取档都必须按照P顺序
	if (!pPlayerInfo)
		return -1;

	//数据库中玩家基本信息表
	SavePlayerBaseInfo(pPlayerInfo);

	// 玩家的战斗技能列表
	if (SavePlayerFightSkillList(pPlayerInfo) != 1)
		return -1;

	// 玩家的生活技能列表
	SavePlayerLifeSkilllList(pPlayerInfo);

	// 玩家的任务列表
	if (SavePlayerTaskList(pPlayerInfo) != 1)
		return -1;

	// 玩家的物品列表
	if (SavePlayerItemList(pPlayerInfo) != 1)
		return -1;

	//玩家的好友列表//
//	SavePlayerFriendList(pPlayerInfo);

	return 1;	
}

int	KPlayer::LoadPlayerBaseInfo(BYTE  * pRoleBuffer, BYTE * &pCurData, unsigned int &nParam)
{
	if (!pRoleBuffer)
	{
		KASSERT(pRoleBuffer);
		return -1;
	}
	
	TRoleData * pRoleData = (TRoleData*)pRoleBuffer;
	
	if (nParam != 0) return -1;
	
	int		nSex;
	int		nLevel;	


	KIniFile playerTemplate;
	char szPlayerTemplate[128];
	sprintf(szPlayerTemplate, "\\settings\\npc\\player\\newplayerini%02d.ini",
		pRoleData->BaseInfo.ifiveprop * 2 + (pRoleData->BaseInfo.bSex ? 1 : 0));
	if (playerTemplate.Load(szPlayerTemplate))
	{
		playerTemplate.GetInteger("ROLE", "iweightmax", 0, &m_nWeightMax);
	}

	nLevel = pRoleData->BaseInfo.ifightlevel;
	int nSeries = pRoleData->BaseInfo.ifiveprop;
	if (nSeries < 0 || nSeries > 2)
	{
		return -1;
	}
	// The current client orders male classes first, then female classes.
	nSex = -1 - nSeries - (pRoleData->BaseInfo.bSex ? 3 : 0);
	nSex = MAKELONG(nLevel, nSex);

	//登入点
	m_sLoginRevivalPos.m_nSubWorldID = pRoleData->BaseInfo.irevivalid;
	m_sLoginRevivalPos.m_ReviveID = pRoleData->BaseInfo.irevivalx;
	//---------------------------------------------------------------------------------------------
label_retry:
	POINT Pos;
	g_SubWorldSet.GetRevivalPosFromId(m_sLoginRevivalPos.m_nSubWorldID,
		m_sLoginRevivalPos.m_ReviveID,
		&Pos);
	
	m_sLoginRevivalPos.m_nMpsX = Pos.x;
	m_sLoginRevivalPos.m_nMpsY = Pos.y;
		//---------------------------------------------------------------------------------------------
	PLAYER_REVIVAL_POS	tempPos;

	if (pRoleData->BaseInfo.cUseRevive)
	{
		tempPos.m_nSubWorldID = m_sLoginRevivalPos.m_nSubWorldID;
		tempPos.m_ReviveID = m_sLoginRevivalPos.m_ReviveID;
		tempPos.m_nMpsX = m_sLoginRevivalPos.m_nMpsX;
		tempPos.m_nMpsY = m_sLoginRevivalPos.m_nMpsY;
	}
	else
	{
		tempPos.m_nSubWorldID = pRoleData->BaseInfo.ientergameid;
		tempPos.m_nMpsX = pRoleData->BaseInfo.ientergamex;
		tempPos.m_nMpsY = pRoleData->BaseInfo.ientergamey;
	}

	m_nIndex = NpcSet.Add(nSex, 
		g_SubWorldSet.SearchWorld(tempPos.m_nSubWorldID),
		tempPos.m_nMpsX,
		tempPos.m_nMpsY);

	if(m_nIndex <= 0) 
	{
		g_DebugLog("[Error!]AddNpc Error DBFuns.cpp");
		if (pRoleData->BaseInfo.cUseRevive)
		{
			return -1;
		}
		else
		{
			pRoleData->BaseInfo.cUseRevive = 1;
			goto label_retry;
		}
	}

	m_sDeathRevivalPos = m_sLoginRevivalPos;

	KNpc* pNpc = &Npc[m_nIndex];
	pNpc->m_Kind = kind_player;
	pNpc->SetPlayerIdx(m_nPlayerIndex);
	pNpc->m_Level = nLevel;

	//玩家信息
	strcpy(pNpc->Name, pRoleData->BaseInfo.szName);
	m_nForbiddenFlag = pRoleData->BaseInfo.nForbiddenFlag;
	
	m_nAttributePoint = pRoleData->BaseInfo.ileftprop;
	m_nSkillPoint = pRoleData->BaseInfo.ileftfight;

	m_nStrength		= pRoleData->BaseInfo.ipower;
	m_nDexterity	= pRoleData->BaseInfo.iagility;
	m_nVitality		= pRoleData->BaseInfo.iouter;
	m_nEngergy		= pRoleData->BaseInfo.iinside;
	m_nLucky		= pRoleData->BaseInfo.iluck;
	m_cTong.Clear();
	m_cTong.DBSetTongNameID(pRoleData->BaseInfo.dwTongID);

	m_nCurStrength = m_nStrength;
	m_nCurDexterity = m_nDexterity;
	m_nCurVitality = m_nVitality;
	m_nCurEngergy = m_nEngergy;
	m_nCurLucky = m_nLucky;
	this->SetFirstDamage();
	this->SetBaseAttackRating();
	this->SetBaseDefence();

	//玩家等级信息
	m_nExp			= pRoleData->BaseInfo.fightexp;
	m_nNextLevelExp = PlayerSet.m_cLevelAdd.GetLevelExp(pNpc->m_Level, pRoleData->BaseInfo.ifiveprop);
	m_dwLeadLevel	= pRoleData->BaseInfo.ileadlevel;
	m_dwLeadExp		= pRoleData->BaseInfo.ileadexp;

	//门派信息
	m_cFaction.m_nCurFaction = (char)pRoleData->BaseInfo.nSect;
	m_cFaction.m_nFirstAddFaction = (char)pRoleData->BaseInfo.nFirstSect;
	m_cFaction.m_nAddTimes			= pRoleData->BaseInfo.ijoincount;
	pNpc->m_btRankId				= pRoleData->BaseInfo.isectrole;
	m_nWorldStat	= pRoleData->BaseInfo.nWorldStat;
	m_nSectStat		= pRoleData->BaseInfo.nSectStat;
	
	//现金和贮物箱中的钱
	int nCashMoney = 0;
	int nSaveMoney = 0;
//	this->m_ItemList.Init(GetPlayerIndex());
	nCashMoney		= pRoleData->BaseInfo.imoney;
	nSaveMoney		= pRoleData->BaseInfo.isavemoney;
	m_ItemList.SetMoney(nCashMoney, nSaveMoney,0);

	pNpc->m_Series	= pRoleData->BaseInfo.ifiveprop;
	pNpc->m_Camp	= pRoleData->BaseInfo.iteam;
	
	pNpc->m_nSex	= pRoleData->BaseInfo.bSex;

	pNpc->m_LifeMax	= pRoleData->BaseInfo.imaxlife;
	pNpc->m_StaminaMax = pRoleData->BaseInfo.imaxstamina;
	pNpc->m_ManaMax = pRoleData->BaseInfo.imaxinner;

	pNpc->m_LifeReplenish = PLAYER_LIFE_REPLENISH;
	pNpc->m_ManaReplenish = PLAYER_MANA_REPLENISH;
	pNpc->m_StaminaGain = PLAYER_STAMINA_GAIN;
	pNpc->m_StaminaLoss = PLAYER_STAMINA_LOSS;

	this->SetBaseResistData();
	SetBaseSpeedAndRadius();
	pNpc->RestoreNpcBaseInfo();
	
	pNpc->m_CurrentLife = pRoleData->BaseInfo.icurlife;
	pNpc->m_CurrentMana = pRoleData->BaseInfo.icurinner;
	pNpc->m_CurrentStamina = pRoleData->BaseInfo.icurstamina;

	// PK信息
	m_cPK.SetNormalPKState(pRoleData->BaseInfo.cPkStatus);
	m_cPK.SetPKValue(pRoleData->BaseInfo.ipkvalue);

	// 初始化部分数据（这些数据数据库不存储）
	m_BuyInfo.Clear();
	m_cMenuState.Release();
	m_cChat.Release();
	m_cTeam.Release();
	m_cTeam.SetCanTeamFlag(m_nPlayerIndex, TRUE);
	m_nPeapleIdx = 0;
	m_nObjectIdx = 0;
	memset(m_szTaskAnswerFun, 0, sizeof(m_szTaskAnswerFun));
	m_nAvailableAnswerNum = 0;
	Npc[m_nIndex].m_ActionScriptID = 0;
	Npc[m_nIndex].m_TrapScriptID = 0;
	m_nViewEquipTime = 0;

	pNpc->m_Experience = 0;
//	memset(pNpc->m_szChatBuffer, 0, sizeof(pNpc->m_szChatBuffer));
//	pNpc->m_nCurChatTime = 0;

	pNpc->m_WeaponType = g_ItemChangeRes.GetWeaponRes(0, 0, 0);
	pNpc->m_ArmorType = g_ItemChangeRes.GetArmorRes(0, 0);
	pNpc->m_HelmType = g_ItemChangeRes.GetHelmRes(0, 0);
	pNpc->m_HorseType = g_ItemChangeRes.GetHorseRes(0, 0);
	pNpc->m_bRideHorse = FALSE;
	nParam = 1;
	pCurData = (BYTE *)&pRoleData->pBuffer;
	// 登入游戏时战斗模式
	pNpc->m_FightMode = pRoleData->BaseInfo.cFightMode;
	// 是否是处于跨服务器状态
	if (pRoleData->BaseInfo.cIsExchange)
	{
	}
	return 1;
}

int	KPlayer::LoadPlayerItemList(BYTE * pRoleBuffer , BYTE* &pItemBuffer, unsigned int &nParam)
{
	KASSERT(pRoleBuffer);
	
	const BYTE* pCounts = NULL;
	const PLAYER_ITEM_STATE* pStates = NULL;
	if (!GetPlayerItemCounts((const TRoleData*)pRoleBuffer, &pCounts, &pStates))
	{
		g_DebugLog("[Item] Invalid saved item count block: role=%s", m_PlayerName);
		return -1;
	}
	int nItemCount = ((TRoleData *)pRoleBuffer)->nItemCount;
	TDBItemData * pItemData = (TDBItemData *)pItemBuffer;
	
	if (nItemCount == 0) return 1;
	if (nParam != 0)
//		m_ItemList.Init(m_nPlayerIndex);
//	else 
	{
		//如果要求获得的物品号大于实际的物品数量，则退出
		if (nParam >= nItemCount )
			return -1;
	}

	if (nParam == 0)
	{
		const TDBItemData* pCheck = pItemData;
		for (int n = 0; n < nItemCount; n++)
		{
			if (pCheck[n].ilocal == pos_equip &&
				KItemList::ClientEquipmentSlotToPart(pCheck[n].ix) < 0)
			{
				g_DebugLog("[Item] Unsupported saved equipment slot: role=%s, slot=%d", m_PlayerName, pCheck[n].ix);
				return -1;
			}
		}
	}

	char szSection[100];
	char szSubSection[100];
	char szKey[100];
	int nItemClass;
	int nItemBaseInfoCount ;
	int nItemEfficInfoCount;
	int nItemRequireInfoCount;
	int dwItemID = 0; 
	int nDetailType = 0;
	int nParticularType = 0;
	int nSeries = 0;
	int nLevel = 0;
	int nLocal = 0;
	int nItemX = 0;
	int nItemY = 0;
	int	nLucky = 0;
	int nBegin = nParam;
	int nEnd = nParam + DBLOADPERTIME_ITEM;
	
	if ( nEnd > nItemCount)
		nEnd = nItemCount;

	nParam = nEnd;		
		
	for (int i = nBegin ; i < nEnd; i ++)
	{
		
		KItem NewItem;	
		ZeroMemory(&NewItem, sizeof(KItem));
		
		//物件基本信息**************************************************************
		nItemClass	= pItemData->iequipclasscode;
		NewItem.m_CommonAttrib.nDetailType		= pItemData->idetailtype;
		NewItem.m_CommonAttrib.nParticularType	= pItemData->iparticulartype;
		NewItem.m_CommonAttrib.nLevel			= pItemData->ilevel;
		NewItem.m_CommonAttrib.nSeries			= pItemData->iseries;
		nItemX			= pItemData->ix;
		nItemY			= pItemData->iy;
		nLocal			= pItemData->ilocal;
		if (nLocal == pos_equip)
			nItemX = KItemList::ClientEquipmentSlotToPart(nItemX);

		//增加物件至世界的ItemSet中,以及增加到玩家物品列表中**************************************************************
		//NewItem.SetID(dwItemID);
		NewItem.m_GeneratorParam.nGeneratorLevel[0] = pItemData->iparam1;
		NewItem.m_GeneratorParam.nGeneratorLevel[1] = pItemData->iparam2;
		NewItem.m_GeneratorParam.nGeneratorLevel[2] = pItemData->iparam3;
		NewItem.m_GeneratorParam.nGeneratorLevel[3] = pItemData->iparam4;
		NewItem.m_GeneratorParam.nGeneratorLevel[4] = pItemData->iparam5;
		NewItem.m_GeneratorParam.nGeneratorLevel[5] = pItemData->iparam6;
		
		NewItem.m_GeneratorParam.nVersion			= pItemData->iequipversion;
		NewItem.m_GeneratorParam.uRandomSeed		= pItemData->irandseed;
		NewItem.m_GeneratorParam.nLuck				= pItemData->ilucky;

		if (pStates)
		{
			NewItem.m_dwUpgradeData = pStates[i].UpgradeData;
			NewItem.m_btGenerationFlags = pStates[i].GenerationFlags;
			NewItem.m_btStateFlags = pStates[i].StateFlags;
			NewItem.m_btBindingMode = pStates[i].BindingMode;
			NewItem.m_btReforgeLuck = pStates[i].ReforgeLuck;
		}
		BOOL bGetEquiptResult = 0;

		//根据不同的物件类型加载
		//不同的相关资源信息
		switch(nItemClass)
		{
		case item_equip :			// 装备
			bGetEquiptResult = ItemGen.Gen_ExistEquipment(
				NewItem.m_CommonAttrib.nDetailType, 
				NewItem.m_CommonAttrib.nParticularType,
				NewItem.m_CommonAttrib.nSeries,
				NewItem.m_CommonAttrib.nLevel, 
				NewItem.m_GeneratorParam.nGeneratorLevel, 
				NewItem.m_GeneratorParam.nLuck,
				NewItem.m_GeneratorParam.nVersion,
				&NewItem);
			break;
		case item_medicine:			// 药品
				bGetEquiptResult = ItemGen.Gen_Medicine(
					NewItem.m_CommonAttrib.nDetailType,
					NewItem.m_CommonAttrib.nLevel,
					NewItem.m_GeneratorParam.nVersion, 
					&NewItem);
			break;

		case item_mine:				// 矿石
			{
				
			}
			break;
		case item_materials:			// 普通材料
			bGetEquiptResult = ItemGen.Gen_Material(
				NewItem.m_CommonAttrib.nDetailType,
				NewItem.m_CommonAttrib.nParticularType,
				NewItem.m_CommonAttrib.nLevel,
				NewItem.m_GeneratorParam.nVersion,
				&NewItem);
			break;
		case item_magicorscript:
		case item_skillbook:
			bGetEquiptResult = ItemGen.Gen_NormalItem(
				nItemClass,
				NewItem.m_CommonAttrib.nDetailType,
				NewItem.m_CommonAttrib.nParticularType,
				NewItem.m_CommonAttrib.nLevel,
				NewItem.m_GeneratorParam.nVersion,
				&NewItem);
			break;
		case item_task:				// 任务
			{
				bGetEquiptResult = ItemGen.Gen_Quest(NewItem.m_CommonAttrib.nDetailType, &NewItem);
			}
			break;
		case item_townportal:
			{
				bGetEquiptResult = ItemGen.Gen_TownPortal(&NewItem);
			}
			break;
		}

		if (!bGetEquiptResult)
		{
			g_DebugLog("[Item] Cannot generate saved item: role=%s, item=%d, class=%d, detail=%d, particular=%d, level=%d, version=%d", m_PlayerName, i, nItemClass, NewItem.m_CommonAttrib.nDetailType, NewItem.m_CommonAttrib.nParticularType, NewItem.m_CommonAttrib.nLevel, NewItem.m_GeneratorParam.nVersion);
			return -1;
		}
		// 把生成的装备的耐久度调整为数据库中存储的。
		if (pItemData->idurability != DB_ITEM_DURABILITY_INITIAL)
		{
			NewItem.SetDurability(pItemData->idurability);
		}
		else if (nItemClass == item_equip)
		{
			NewItem.SetDurability(NewItem.GetMaxDurability());
		}

		// 旧生成算法可能将当前耐久保存为高于客户端上限的数值。
		if (nItemClass == item_equip && NewItem.GetMaxDurability() >= 0 &&
			NewItem.GetDurability() > NewItem.GetMaxDurability())
		{
			NewItem.SetDurability(NewItem.GetMaxDurability());
		}

		if (!NewItem.SetStackCount(pStates ? pStates[i].Count : (pCounts ? pCounts[i] : 1)))
		{
			g_DebugLog("[Item] Invalid saved stack count: role=%s, item=%d", m_PlayerName, i);
			return -1;
		}

		pItemData ++;
		
		int nIndex = ItemSet.Add(&NewItem);
		
		if (nIndex <= 0)
		{
			g_DebugLog("[Item] Cannot allocate saved item: role=%s, item=%d, class=%d, detail=%d, particular=%d, level=%d", m_PlayerName, i, nItemClass, NewItem.m_CommonAttrib.nDetailType, NewItem.m_CommonAttrib.nParticularType, NewItem.m_CommonAttrib.nLevel);
			return -1;
		}
		if (m_ItemList.Add(nIndex, nLocal, nItemX, nItemY) <= 0)
		{
			ItemSet.Remove(nIndex);
			g_DebugLog("[Item] Cannot restore saved item: role=%s, place=%d, x=%d, y=%d", m_PlayerName, nLocal, nItemX, nItemY);
			return -1;
		}
		
	}

	pItemBuffer	= (BYTE *)pItemData;

	if (nParam >= nItemCount)
		return 1;
	else 
		return 0;
}


int	KPlayer::LoadPlayerFightSkillList(BYTE * pRoleBuffer, BYTE * &pFightBuffer, unsigned int& nParam)
{
	KASSERT(pRoleBuffer);
	if (!pRoleBuffer)
		return -1;

	int nSkillCount = ((TRoleData*)pRoleBuffer)->nFightSkillCount;
	if (nSkillCount < 0 || nSkillCount >= MAX_NPCSKILL)
		return -1;

	if (nSkillCount > 0)
	{
		if (nParam >= nSkillCount)
			return -1;

		int nBegin = nParam;
		int nEnd = nBegin + DBLOADPERTIME_SKILL;
		if (nEnd > nSkillCount)
			nEnd = nSkillCount;

		TDBSkillData *pSkillData = NULL;
		if (nBegin == 0)
			pSkillData = (TDBSkillData*)(pRoleBuffer + ((TRoleData*)pRoleBuffer)->dwFSkillOffset);
		else
			pSkillData = (TDBSkillData*)pFightBuffer;

		for (int i = nBegin; i < nEnd; i++, pSkillData++)
		{
			if (!Npc[m_nIndex].m_SkillList.LoadPlayerSkill(pSkillData->m_nSkillId, pSkillData->m_nSkillLevel))
				return -1;
		}
		pFightBuffer = (BYTE*)pSkillData;
		nParam = nEnd;
		if (nParam < nSkillCount)
			return 0;
	}

	// Current character templates leave FSKILLS empty; use the shared client resource.
	// For existing roles, add missing initial skills without changing learned levels.
	if (!Npc[m_nIndex].m_SkillList.LoadPlayerInitialSkills(Npc[m_nIndex].m_Series))
		return -1;
	if (!LoadPlayerSkillExtension((const TRoleData*)pRoleBuffer, Npc[m_nIndex].m_SkillList, *this))
		return -1;
	Npc[m_nIndex].m_SkillList.RefreshPlayerSkillLevels();
	return 1;
}

int	KPlayer::LoadPlayerLifeSkilllList(BYTE * pRoleBuffer, BYTE * &pFriendBuffer, unsigned int& nParam)
{
	KASSERT(pRoleBuffer);
	int nSkillCount = 0;
	char szSkillId[100];
	char szSkillLevel[100];
	nSkillCount = ((TRoleData*)(pRoleBuffer))->nLiveSkillCount;
	
	if (nSkillCount == 0)
		return 1;

	if (nParam >= nSkillCount )		return -1;
	int nBegin	= nParam;
	int nEnd	= nBegin + DBLOADPERTIME_SKILL;
	if (nEnd > nSkillCount) 	nEnd = nSkillCount;
	nParam = nEnd;

	TDBSkillData * pSkillData = (TDBSkillData*)pFriendBuffer;
	for (int i = nBegin ; i < nEnd; i ++, pSkillData ++ )
	{
		int nSkillId = 0;
		int nSkillLevel = 0;
		nSkillId = pSkillData->m_nSkillId;
		nSkillLevel = pSkillData->m_nSkillLevel;
		Npc[m_nIndex].m_SkillList.Add(nSkillId, nSkillLevel);
	}
	pFriendBuffer = (BYTE*) pSkillData;

	if (nParam >= nSkillCount) return 1;
	else 
		return 0;

}

int	KPlayer::LoadPlayerFriendList(BYTE * pRoleBuffer, BYTE * &pFriendBuffer, unsigned int &nParam)
{
	KASSERT(pRoleBuffer);

	if (nParam == 1)
		return 0;
	if (nParam == 2)
		return 1;

	int		nFriendCount;
	DWORD	dwID;
	char	szBuffer[32], szName[32];
	nFriendCount = ((TRoleData*)pRoleBuffer)->nFriendCount;
	if (nFriendCount == 0)
	{
		nParam = 2;
		return 1;
	}
	char * pCurFriend = (char *)pFriendBuffer;
	for (int i = 0; i < nFriendCount; i++)
	{
		
		strcpy(szName, (char *)pFriendBuffer);
		if (!szName[0])
			continue;
		dwID = g_FileName2Id(szName);
		m_cChat.DataBaseAddOne(this->m_nPlayerIndex, dwID, szName);
		pFriendBuffer += (strlen(szName) + 1);
	}
	this->m_cChat.StartSendFriendData();

	nParam = 1;
	return 0;
}

int	KPlayer::LoadPlayerTaskList(BYTE * pRoleBuffer, BYTE * &pTaskBuffer, unsigned int& nParam)
{
	KASSERT(pRoleBuffer);
	while(m_PlayerStationList.GetHead())
	{
		KIndexNode * pNode = (KIndexNode * ) m_PlayerStationList.GetHead();
		m_PlayerStationList.RemoveHead();
		delete pNode;
	}
	while(m_PlayerWayPointList.GetHead())
	{
		KIndexNode * pNode = (KIndexNode * ) m_PlayerWayPointList.GetHead();
		m_PlayerWayPointList.RemoveHead();
		delete pNode;
	}

	TRoleData *pRoleData = (TRoleData*)pRoleBuffer;
	if (pRoleData->nTaskCount < 0 || pRoleData->nTaskCount > PLAYER_TASK_EXT_BEGIN ||
		pRoleData->dwTaskOffset < sizeof(TRoleData) - 1 || pRoleData->dwItemOffset < 0 ||
		pRoleData->dwDataLen > sizeof(m_SaveBuffer) ||
		(DWORD)pRoleData->dwTaskOffset > pRoleData->dwDataLen ||
		(DWORD)pRoleData->dwItemOffset > pRoleData->dwDataLen ||
		(DWORD)pRoleData->dwItemOffset < (DWORD)pRoleData->dwTaskOffset)
		return -1;
	DWORD dwTaskBytes = (DWORD)pRoleData->nTaskCount * sizeof(TDBTaskData);
	if (dwTaskBytes > (DWORD)pRoleData->dwItemOffset - (DWORD)pRoleData->dwTaskOffset)
		return -1;
	DWORD dwTaskEnd = (DWORD)pRoleData->dwTaskOffset + dwTaskBytes;
	if (nParam == 0)
	{
		m_cTask.Release();
		const BYTE *pExtra = pRoleBuffer + dwTaskEnd;
		DWORD dwExtraLength = pRoleData->dwItemOffset - dwTaskEnd;
		if (!LoadPlayerTaskExtension(m_cTask, pExtra, dwExtraLength) ||
			!m_cTask.LoadNotes(pExtra, dwExtraLength))
		{
			g_DebugLog("[Task] Invalid saved task journal: role=%s", m_PlayerName);
			return -1;
		}
		pTaskBuffer = pRoleBuffer + pRoleData->dwTaskOffset;
	}

	int nTaskCount = 0;
	int nTaskId = 0;
	int nTaskDegee = 0;
	char szTaskIDKey[100];
	char szTaskValueKey[100];
	nTaskCount = ((TRoleData* )pRoleBuffer)->nTaskCount;

	if (nTaskCount == 0)
	{
		pTaskBuffer = pRoleBuffer + pRoleData->dwItemOffset;
		return 1;
	}
	if (nParam >= nTaskCount ) return -1;
	int nBegin	= nParam;
	int nEnd	= nBegin + DBLOADPERTIME_TASK;
	if (nEnd > nTaskCount) nEnd = nTaskCount;
	nParam = nEnd;
	TDBTaskData * pTaskData = (TDBTaskData*) pTaskBuffer;
	for (int i = nBegin; i < nEnd; i ++ , pTaskData ++)
	{
		nTaskId = pTaskData->m_nTaskId;
		nTaskDegee = pTaskData->m_nTaskValue;
		if (nTaskId < 0 || nTaskId >= MAX_TASK)
		{
			KASSERT(0);
			continue;//任务ID超过上限了！
		}
		m_cTask.SetSaveVal(nTaskId, nTaskDegee);
	}
	pTaskBuffer = (BYTE*) pTaskData;
	if (nParam >= nTaskCount)
	{
		// 标准任务变量后还有可选任务日志，下一阶段必须从物品偏移开始。
		pTaskBuffer = pRoleBuffer + pRoleData->dwItemOffset;
		return 1;
	}
	else 
	return 0;
}

int	KPlayer::SavePlayerBaseInfo(BYTE * pRoleBuffer)
{
	_ASSERT(pRoleBuffer);
	if (m_nIndex <= 0) return -1;

	KNpc * pNpc = &Npc[m_nIndex];
	TRoleData * pRoleData = (TRoleData*)pRoleBuffer;
	//玩家信息
	memset(pRoleData, 0, sizeof(TRoleData));
	pRoleData->bBaseNeedUpdate = 1;
	strcpy(pRoleData->BaseInfo.szName, m_PlayerName);
	if (m_AccoutName[0])
		strcpy(pRoleData->BaseInfo.caccname, m_AccoutName);
	pRoleData->BaseInfo.nForbiddenFlag = m_nForbiddenFlag;
	pRoleData->BaseInfo.ileftprop = m_nAttributePoint;
	pRoleData->BaseInfo.ileftfight = m_nSkillPoint;
	pRoleData->BaseInfo.ipower = m_nStrength;
	pRoleData->BaseInfo.iagility = m_nDexterity;
	pRoleData->BaseInfo.iouter = m_nVitality;
	pRoleData->BaseInfo.iinside	=  m_nEngergy;
	pRoleData->BaseInfo.iluck = m_nLucky;
	pRoleData->BaseInfo.dwTongID = m_cTong.GetTongNameID();

	//玩家的显示信息	-- remark by spe because client ui display changed 2003/07/21
//	pRoleData->BaseInfo.ihelmres = pNpc->m_HelmType;
//	pRoleData->BaseInfo.iarmorres = pNpc->m_ArmorType;
//	pRoleData->BaseInfo.iweaponres = pNpc->m_WeaponType;
	
	//玩家等级信息
	pRoleData->BaseInfo.fightexp = m_nExp;
	pRoleData->BaseInfo.ifightlevel = pNpc->m_Level;
	
	pRoleData->BaseInfo.ileadlevel = m_dwLeadLevel;
	pRoleData->BaseInfo.ileadexp =	m_dwLeadExp;

	//门派信息
	pRoleData->BaseInfo.nSect =		m_cFaction.m_nCurFaction;
	pRoleData->BaseInfo.nFirstSect = 	m_cFaction.m_nFirstAddFaction;
	pRoleData->BaseInfo.ijoincount = m_cFaction.m_nAddTimes;
	pRoleData->BaseInfo.isectrole = pNpc->m_btRankId;
	pRoleData->BaseInfo.nWorldStat	= m_nWorldStat;
	pRoleData->BaseInfo.nSectStat	= m_nSectStat;
	
	//现金和贮物箱中的钱
	int nCashMoney = 0;
	int nSaveMoney = 0;
	
	nCashMoney  = m_ItemList.GetMoney(room_equipment);
	nSaveMoney	= m_ItemList.GetMoney(room_repository);
	pRoleData->BaseInfo.imoney			= nCashMoney;
	pRoleData->BaseInfo.isavemoney		= nSaveMoney;
	pRoleData->BaseInfo.ifiveprop		= pNpc->m_Series;
	pRoleData->BaseInfo.iteam			= pNpc->m_Camp;
	pRoleData->BaseInfo.bSex			= pNpc->m_nSex;
	pRoleData->BaseInfo.imaxlife		= pNpc->m_LifeMax;
	pRoleData->BaseInfo.imaxstamina		= pNpc->m_StaminaMax;
	pRoleData->BaseInfo.imaxinner		= pNpc->m_ManaMax;
	pRoleData->BaseInfo.icurlife		= pNpc->m_CurrentLife;
	pRoleData->BaseInfo.icurinner		= pNpc->m_CurrentMana;
	pRoleData->BaseInfo.icurstamina		= pNpc->m_CurrentStamina;

	//登入点
	pRoleData->BaseInfo.irevivalid = 	m_sLoginRevivalPos.m_nSubWorldID;
	pRoleData->BaseInfo.irevivalx = 	m_sLoginRevivalPos.m_ReviveID;
	pRoleData->BaseInfo.irevivaly = 	0;

	if (m_bExchangeServer)	// 准备跨服务器：把存盘点设为目标点
	{
		pRoleData->BaseInfo.cUseRevive = 0;
		pRoleData->BaseInfo.ientergameid = m_sExchangePos.m_dwMapID;
		pRoleData->BaseInfo.ientergamex = m_sExchangePos.m_nX;
		pRoleData->BaseInfo.ientergamey = m_sExchangePos.m_nY;
		pRoleData->BaseInfo.cFightMode = (BYTE)pNpc->m_FightMode;
	}
	else if (pNpc->m_SubWorldIndex >= 0 && pNpc->m_RegionIndex >= 0 && pNpc->m_Doing != do_death && pNpc->m_Doing != do_revive)
	{
		pRoleData->BaseInfo.cUseRevive = m_bUseReviveIdWhenLogin;
		pRoleData->BaseInfo.ientergameid = SubWorld[pNpc->m_SubWorldIndex].m_SubWorldID;
		pNpc->GetMpsPos(&pRoleData->BaseInfo.ientergamex, &pRoleData->BaseInfo.ientergamey);
		pRoleData->BaseInfo.cFightMode = (BYTE)pNpc->m_FightMode;
	}
	else
	{
		pRoleData->BaseInfo.cUseRevive = 1;
		pRoleData->BaseInfo.cFightMode = 0;
		if (pNpc->m_Doing == do_death || pNpc->m_Doing == do_revive)
		{
			pRoleData->BaseInfo.icurlife = pNpc->m_LifeMax;
			pRoleData->BaseInfo.icurinner = pNpc->m_ManaMax;
			pRoleData->BaseInfo.icurstamina = pNpc->m_StaminaMax;
		}
	}

	//PK相关
	pRoleData->BaseInfo.cPkStatus = (BYTE)m_cPK.GetNormalPKState();
	pRoleData->BaseInfo.ipkvalue = m_cPK.GetPKValue();
	pRoleData->dwFSkillOffset = (BYTE * )pRoleData->pBuffer - (BYTE *)pRoleBuffer;
	return 1;
	
}

int	KPlayer::SavePlayerItemList(BYTE * pRoleBuffer)
{
	_ASSERT(pRoleBuffer);

	TRoleData * pRoleData = (TRoleData*) pRoleBuffer;

	TDBItemData * pItemData = (TDBItemData*) ((BYTE*)pRoleData +  pRoleData->dwItemOffset);

	BYTE Counts[MAX_PLAYER_ITEM];
	PLAYER_ITEM_STATE States[MAX_PLAYER_ITEM];
	ZeroMemory(States, sizeof(States));
	BOOL bHasState = FALSE;

	int	nItemCount = 0;
	char szSection[100];
	char szKey[100];

	int nIdx = 0;
	while(1)
	{
		nIdx = m_ItemList.m_UseIdx.GetNext(nIdx);
		if (nIdx == 0 )
			break;
		int nItemIndex = m_ItemList.m_Items[nIdx].nIdx;
		
		sprintf(szSection, "%s%d", SECTION_ITEM, nItemCount + 1);
		//玩家基本信息*****************************************************************8	
		pItemData->iequipclasscode =  Item[nItemIndex].m_CommonAttrib.nItemGenre;
		pItemData->iequipcode =  Item[nItemIndex].GetID();
		pItemData->idetailtype =  Item[nItemIndex].m_CommonAttrib.nDetailType;
		pItemData->iparticulartype =  Item[nItemIndex].m_CommonAttrib.nParticularType;
		pItemData->ilevel =  Item[nItemIndex].m_CommonAttrib.nLevel;
		pItemData->iseries =  Item[nItemIndex].m_CommonAttrib.nSeries;
		pItemData->ilocal =  m_ItemList.m_Items[nIdx].nPlace;
		pItemData->ix = m_ItemList.m_Items[nIdx].nPlace == pos_equip ?
			KItemList::EquipmentPartToClientSlot(m_ItemList.m_Items[nIdx].nX) :
			m_ItemList.m_Items[nIdx].nX;
		pItemData->iy =  m_ItemList.m_Items[nIdx].nY;
		pItemData->iequipversion = Item[nItemIndex].GetItemParam()->nVersion;
		pItemData->irandseed = Item[nItemIndex].GetItemParam()->uRandomSeed;
		pItemData->iparam1 =  Item[nItemIndex].GetItemParam()->nGeneratorLevel[0];
		pItemData->iparam2 =  Item[nItemIndex].GetItemParam()->nGeneratorLevel[1];
		pItemData->iparam3 =  Item[nItemIndex].GetItemParam()->nGeneratorLevel[2];
		pItemData->iparam4 =  Item[nItemIndex].GetItemParam()->nGeneratorLevel[3];
		pItemData->iparam5 =  Item[nItemIndex].GetItemParam()->nGeneratorLevel[4];
		pItemData->iparam6 =  Item[nItemIndex].GetItemParam()->nGeneratorLevel[5];
		pItemData->ilucky = 	 Item[nItemIndex].GetItemParam()->nLuck;
		pItemData->idurability = Item[nItemIndex].GetDurability();
		pItemData->iidentify = 0;
		//*****************************************************************************8

		Counts[nItemCount] = (BYTE)Item[nItemIndex].GetStackCount();
		States[nItemCount].Count = Counts[nItemCount];
		States[nItemCount].UpgradeData = Item[nItemIndex].m_dwUpgradeData;
		States[nItemCount].GenerationFlags = Item[nItemIndex].m_btGenerationFlags;
		States[nItemCount].StateFlags = Item[nItemIndex].m_btStateFlags;
		States[nItemCount].BindingMode = Item[nItemIndex].m_btBindingMode;
		States[nItemCount].ReforgeLuck = Item[nItemIndex].m_btReforgeLuck;
		if (States[nItemCount].UpgradeData || States[nItemCount].GenerationFlags ||
			States[nItemCount].StateFlags || States[nItemCount].BindingMode || States[nItemCount].ReforgeLuck)
			bHasState = TRUE;

		pItemData++;
		nItemCount ++;
	}
	
	pRoleData->nItemCount = nItemCount;
	
	BYTE* pEnd = (BYTE*)pItemData;
	BOOL bHasStack = FALSE;
	for (int i = 0; i < nItemCount; i++)
	{
		if (Counts[i] > 1)
			bHasStack = TRUE;
	}
	if (bHasStack || bHasState)
	{
		DWORD dwStateSize = bHasState ? nItemCount * sizeof(PLAYER_ITEM_STATE) : nItemCount;
		if (pEnd - pRoleBuffer + sizeof(PLAYER_ITEM_COUNT_HEADER) + dwStateSize >= sizeof(m_SaveBuffer))
			return -1;
		PLAYER_ITEM_COUNT_HEADER Header;
		Header.Magic = PLAYER_ITEM_COUNT_MAGIC;
		Header.Version = bHasState ? 2 : 1;
		Header.ItemCount = nItemCount;
		memcpy(pEnd, &Header, sizeof(Header));
		pEnd += sizeof(Header);
		memcpy(pEnd, bHasState ? (const void*)States : (const void*)Counts, dwStateSize);
		pEnd += dwStateSize;
	}
	pRoleData->dwFriendOffset = pEnd - pRoleBuffer;
	pRoleData->dwDataLen = pEnd - pRoleBuffer;
	return 1;
}

int KPlayer::SavePlayerFightSkillList(BYTE* pRoleBuffer)
{
	if (!pRoleBuffer || m_nIndex <= 0)
		return -1;
	TRoleData* pRoleData = (TRoleData*)pRoleBuffer;
	KSkillList& List = Npc[m_nIndex].m_SkillList;
	DWORD dwOffset = pRoleData->dwFSkillOffset;
	DWORD dwStandardSize = List.GetCount() * sizeof(TDBSkillData);
	if (dwOffset > sizeof(m_SaveBuffer) || dwStandardSize > sizeof(m_SaveBuffer) - dwOffset)
		return -1;
	int nCount = List.UpdateDBSkillList(pRoleBuffer + dwOffset);
	if (nCount < 0)
		return -1;
	pRoleData->nFightSkillCount = nCount;
	dwOffset += dwStandardSize;
	int nSize = SavePlayerSkillExtension(List, m_Pet, pRoleBuffer + dwOffset, sizeof(m_SaveBuffer) - dwOffset);
	if (nSize < 0)
		return -1;
	pRoleData->dwLSkillOffset = dwOffset + nSize;
	return 1;
}

int	KPlayer::SavePlayerLifeSkilllList(BYTE * pRoleBuffer)
{
	_ASSERT(pRoleBuffer);
	TRoleData * pRoleData = (TRoleData *)pRoleBuffer;
	TDBSkillData * pSkillData = (TDBSkillData *) (pRoleBuffer + pRoleData->dwLSkillOffset);
	pRoleData->dwTaskOffset = (BYTE*)pSkillData - pRoleBuffer;
	return 1;
}

int	KPlayer::SavePlayerFriendList(BYTE * pRoleBuffer)
{
	_ASSERT(pRoleBuffer);

	int		nFriendNo;
	char	szBuffer[32];
	CChatFriend	*pFriend;
	TRoleData * pRoleData = (TRoleData *)pRoleBuffer;

	nFriendNo = m_cChat.m_cFriendTeam[0].m_nFriendNo;
	char * pCurFriend = (char *)((BYTE *)pRoleData + pRoleData->dwFriendOffset);
	int nTotalFriendNo  = 0;
	pFriend = (CChatFriend*)m_cChat.m_cFriendTeam[0].m_cEveryOne.GetHead();
	while (pFriend)
	{
		nTotalFriendNo++;
		strcpy(pCurFriend, pFriend->m_szName);
		pCurFriend += strlen(pFriend->m_szName) + 1;
		pFriend = (CChatFriend*)pFriend->GetNext();
	}
	pRoleData->nFriendCount = nTotalFriendNo;
	_ASSERT(nTotalFriendNo == nFriendNo);
	pRoleData->dwDataLen = (BYTE*)pCurFriend - (BYTE*)pRoleBuffer;
	return 1;
}


int	KPlayer::SavePlayerTaskList(BYTE * pRoleBuffer)
{
	_ASSERT(pRoleBuffer);
	TRoleData * pRoleData = (TRoleData *) pRoleBuffer;
	int nTaskCount = 0;
	int nTaskId = 0;
	int nTaskDegee = 0;
	char szTaskIDKey[100];
	char szTaskValueKey[100];

	KIndexNode * pNode = (KIndexNode*)m_PlayerStationList.GetHead();
	int n = 0;
	memset(&m_cTask.nSave[TASKVALUE_SAVESTATION_BEGIN], 0, (TASKVALUE_MAXSTATION_COUNT / 2) * sizeof(int));
	memset(&m_cTask.nSave[TASKVALUE_SAVEWAYPOINT_BEGIN], 0, TASKVALUE_MAXWAYPOINT_COUNT * sizeof(int));

	while(pNode)
	{
		if (n >= TASKVALUE_MAXSTATION_COUNT) break;
		DWORD dwValue = m_cTask.nSave[TASKVALUE_SAVESTATION_BEGIN + n / 2];
		DWORD OrValue = pNode->m_nIndex ;
		OrValue = OrValue << ((n % 2) * 16);
		dwValue = dwValue | OrValue;
		m_cTask.nSave[TASKVALUE_SAVESTATION_BEGIN + n / 2] = dwValue;
		n++;
		pNode = (KIndexNode*)pNode->GetNext();
	}
	
	n = 0;
	pNode = (KIndexNode*) m_PlayerWayPointList.GetHead();
	while(pNode)
	{
		if (n >= TASKVALUE_MAXWAYPOINT_COUNT) break;
		if (pNode->m_nIndex == 0) continue;
		m_cTask.nSave[TASKVALUE_SAVEWAYPOINT_BEGIN + n] = pNode->m_nIndex;
		n++;
		pNode = (KIndexNode*)pNode->GetNext();
	}
	
	
	g_TimerTask.SaveTask(this);


	TDBTaskData * pTaskData = (TDBTaskData *)(pRoleBuffer + pRoleData->dwTaskOffset);
	for (int i = 0; i < PLAYER_TASK_EXT_BEGIN; i ++)
	{
		if (!m_cTask.nSave[i]) continue;
		// 扩展任务号不改变存档结构，写入每条记录前仍须保证缓冲区有余量。
		if ((BYTE *)(pTaskData + 1) - pRoleBuffer > sizeof(m_SaveBuffer))
			return -1;
		pTaskData->m_nTaskId = i;
		pTaskData->m_nTaskValue = m_cTask.GetSaveVal(i);
		nTaskCount ++;
		pTaskData ++;
	}
		
	pRoleData->nTaskCount = nTaskCount;
	DWORD dwOffset = (BYTE*)pTaskData - pRoleBuffer;
	if (dwOffset >= sizeof(m_SaveBuffer))
		return -1;
	int nExtensionLength = SavePlayerTaskExtension(m_cTask, pRoleBuffer + dwOffset,
		sizeof(m_SaveBuffer) - dwOffset);
	if (nExtensionLength < 0)
		return -1;
	dwOffset += nExtensionLength;
	int nNotesLength = m_cTask.SaveNotes(pRoleBuffer + dwOffset, sizeof(m_SaveBuffer) - dwOffset);
	if (nNotesLength < 0)
		return -1;
	dwOffset += nNotesLength;
	// 任务日志不能挤占随后物品记录的空间，提前计算完整存档上限。
	int nItems = 0;
	for (PlayerItem *pItem = m_ItemList.GetFirstItem(); pItem; pItem = m_ItemList.GetNextItem())
		nItems++;
	if (dwOffset + nItems * (sizeof(TDBItemData) + sizeof(PLAYER_ITEM_STATE)) + sizeof(PLAYER_ITEM_COUNT_HEADER) >= sizeof(m_SaveBuffer) - 32)
		return -1;
	pRoleData->dwItemOffset = dwOffset;
	return 1;
}
#endif

