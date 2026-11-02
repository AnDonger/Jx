#include "KCore.h"

#include "KEngine.h"
#include "KMagicAttrib.h"
#include "KMagicDesc.h"
#include "GameDataDef.h"
#include "KFaction.h"

#define		MAGICDESC_FILE		"\\settings\\MagicDesc.Ini"
extern const char * g_MagicID2String(int nAttrib);
const char MAGIC_ATTRIB_STRING[][100] = 
{
//改变技能的属性
	"skill_begin",
	"skill_cost_v",							// 消耗MANA
	"skill_costtype_v",		//	
	"skill_mintimepercast_v",		// 每次发魔法的间隔时间
	"skill_misslenum_v",		// 
	"skill_misslesform_v",		
	"skill_param1_v",		
	"skill_param2_v",
	"skill_reserve1",
	"skill_reserve2",
	"skill_reserve3",
	"skill_reserve4",
	"skill_eventskilllevel",
	"skill_end",

//改变发出子弹的属性
	"missle_begin",
	"missle_movekind_v",
	"missle_speed_v",
	"missle_lifetime_v",
	"missle_height_v",
	"missle_damagerange_v",
	"missle_radius_v",
	"missle_reserve1",
	"missle_reserve2",
	"missle_reserve3",
	"missle_reserve4",
	"missle_reserve5",
	"missle_end",

//装备属性	
	"item_begin",
	"weapondamagemin_v",
	"weapondamagemax_v",
	"armordefense_v",
	"durability_v",
	"requirestr",
	"requiredex",
	"requirevit",
	"requireeng",
	"requirelevel",
	"requireseries",
	"requiresex",
	"requiremenpai",
	"weapondamageenhance_p",
	"armordefenseenhance_p",
	"requirementreduce_p",
	"indestructible_b",
	"item_reserve1",
	"item_reserve2",
	"item_reserve3",
	"item_reserve4",
	"item_reserve5",
	"item_reserve6",
	"item_reserve7",
	"item_reserve8",
	"item_reserve9",
	"item_reserve10",
	"item_end",

	"damage_begin",		
	"attackrating_v",
	"attackrating_p",
	"ignoredefense_p",
	"physicsdamage_v",
	"colddamage_v",
	"firedamage_v",
	"lightingdamage_v",
	"poisondamage_v",
	"magicdamage_v",
	"physicsenhance_p",
	"steallife_p",
	"stealmana_p",
	"stealstamina_p",
	"knockback_p",
	"deadlystrike_p",
	"fatallystrike_p",
	"stun_p",
	"damage_reserve1",
	"damage_reserve2",
	"damage_reserve3",
	"damage_reserve4",
	"damage_reserve5",
	"damage_reserve6",
	"damage_reserve7",
	"damage_reserve8",
	"damage_reserve9",
	"damage_reserve10",
	"damage_end",

	"normal_begin",
	"lifemax_v",
	"lifemax_p",
	"life_v",
	"lifereplenish_v",
	"manamax_v",
	"manamax_p",
	"mana_v",
	"manareplenish_v",
	"staminamax_v",
	"staminamax_p",
	"stamina_v",
	"staminareplenish_v",
	"strength_v",
	"dexterity_v",
	"vitality_v",
	"energy_v",
	"poisonres_p",
	"fireres_p",
	"lightingres_p",
	"physicsres_p",
	"coldres_p",
	"freezetimereduce_p",
	"burntimereduce_p",
	"poisontimereduce_p",
	"poisondamagereduce_v",
	"stuntimereduce_p",
	"fastwalkrun_p",
	"visionradius_p",
	"fasthitrecover_v",
	"allres_p",
	"attackspeed_v",
	"castspeed_v",
	"meleedamagereturn_v",
	"meleedamagereturn_p",
	"rangedamagereturn_v",
	"rangedamagereturn_p",
	"addphysicsdamage_v",
	"addfiredamage_v",
	"addcolddamage_v",
	"addlightingdamage_v",
	"addpoisondamage_v",
	"addphysicsdamage_p",
	"slowmissle_b",
	"changecamp_b",
	"physicsarmor_v",
	"coldarmor_v",
	"firearmor_v",
	"poisonarmor_v",
	"lightingarmor_v",
	"damagetomana_p",
	"lucky_v",
	"steallifeenhance_p",
	"stealmanaenhance_p",
	"stealstaminaenhance_p",
	"allskill_v",
	"metalskill_v",
	"woodskill_v",
	"waterskill_v",
	"fireskill_v",
	"earthskill_v",
	"knockbackenhance_p",
	"deadlystrikeenhance_p",
	"stunenhance_p",
	"badstatustimereduce_v",
	"manashield_p",
	"adddefense_v",
	"adddefense_p",
	"fatallystrikeenhance_p",
	"lifepotion_v",
	"manapotion_v",
	"physicsresmax_p",
	"coldresmax_p",
	"fireresmax_p",
	"lightingresmax_p",
	"poisonresmax_p",
	"allresmax_p",
	"coldenhance_p",
	"fireenhance_p",
	"lightingenhance_p",
	"poisonenhance_p",
	"magicenhance_p",
	"attackratingenhance_v",
	"attackratingenhance_p",
	"addexdefense_v",
	"normal_end",
};



KMagicDesc	g_MagicDesc;
KMagicDesc::KMagicDesc()
{
	m_szDesc[0] = 0;
}

KMagicDesc::~KMagicDesc()
{
}

BOOL KMagicDesc::Init()
{
//	g_SetFilePath("\\");
	return (m_IniFile.Load(MAGICDESC_FILE));
}

const char* KMagicDesc::GetDesc(void *pData)
{
	
	char	szTempDesc[128];
	char*	pTempDesc = szTempDesc;
	int		i = 0;

	ZeroMemory(m_szDesc, 128);
	
	if (!pData)
		return NULL;

	KMagicAttrib* pAttrib = (KMagicAttrib *)pData;

	const char	*pszKeyName = g_MagicID2String(pAttrib->nAttribType);
	m_IniFile.GetString("Descript", pszKeyName, "", szTempDesc, 128);
	while(*pTempDesc)
	{
		if (*pTempDesc == '#')
		{
			int	nDescAddType = 0;
			switch(*pTempDesc + 3)
			{
			case '+':
				nDescAddType = 1;
				break;
			default:
				nDescAddType = 0;
				break;
			}
			int nValue = 0;
			
			switch(*(pTempDesc + 2))
			{
			case '1':
				nValue = pAttrib->nValue[0];
				break;
			case '2':
				nValue = pAttrib->nValue[1];
				break;
			case '3':
				nValue = pAttrib->nValue[2];
				break;
			default:
				nValue = pAttrib->nValue[0];
				break;
			}
			switch(*(pTempDesc+1))
			{
			case 'm':		// 门派
				strcat(m_szDesc, g_Faction.m_sAttribute[nValue].m_szName);
				i += strlen(g_Faction.m_sAttribute[nValue].m_szName);
				break;
			case 's':		// 五行
				switch(nValue)
				{
				case series_metal:
					strcat(m_szDesc, "金系");
					break;
				case series_wood:
					strcat(m_szDesc, "木系");
					break;
				case series_water:
					strcat(m_szDesc, "水系");
					break;
				case series_fire:
					strcat(m_szDesc, "火系");
					break;
				case series_earth:
					strcat(m_szDesc, "土系");
					break;
				default:
					strcat(m_szDesc, "无系");
					break;
				}
				i += 4;
				break;
			case 'k':		// 消耗类型
				switch(nValue)
				{
				case 0:
					strcat(m_szDesc, "内力");
					break;
				case 1:
					strcat(m_szDesc, "生命");
					break;
				case 2:
					strcat(m_szDesc, "体力");
					break;
				case 3:
					strcat(m_szDesc, "金钱");
					break;
				default:
					strcat(m_szDesc, "内力");
					break;
				}
				i += 4;
				break;
			case 'd':		// 数字
				{
					//if (nValue == 0)	// 数值为零的话，不显示
					{
					//	return NULL;
					}

					switch(nDescAddType)
					{
					case 1:
						if (nValue > 0)
						{
							strcat(m_szDesc, "增加");
						}
						else
						{
							nValue = -nValue;
							strcat(m_szDesc, "减少");
						}
						break;
					default:
						break;
					}
					char	szMsg[16];
					sprintf(szMsg, "%d", nValue);
					strcat(m_szDesc, szMsg);
					i += strlen(szMsg);
				}
				break;
			case 'x':		// 性别
				if (nValue)
					strcat(m_szDesc, "女性");
				else
					strcat(m_szDesc, "男性");
				i += 4;
				break;
			default:
				break;
			}
			pTempDesc += 4;
		}
		else
		{
			m_szDesc[i] = *pTempDesc;
			pTempDesc++;
			i++;
		}
	}
	return m_szDesc;
}

const char * g_MagicID2String(int nAttrib)
{
	if ((nAttrib < 0) || nAttrib >= magic_normal_end) return MAGIC_ATTRIB_STRING[magic_normal_end];
	return 	MAGIC_ATTRIB_STRING[nAttrib];
}

int	g_String2MagicID(char * szMagicAttribName)
{
	if ((!szMagicAttribName) || (!szMagicAttribName[0])) return -1;

	//nValue2 当值为-1时为永久性状态，0为非状态，其它值为有时效性状态魔法效果
	//需要将状态数据与非状态数据分离出来，放入相应的数组内，并记录总数量
	
	for (int i  = 0 ; i <= magic_normal_end; i ++)
	{
		if (!strcmp(szMagicAttribName, g_MagicID2String(i)))
			return i;
	}
	return -1;
}


// 当前资源属性编号与旧内部枚举不同，未确认编号不外发。
int GetCurrentMagicAttribId(int nAttrib)
{
	switch (nAttrib)
	{
	case magic_skill_begin: return 0;
	case magic_skill_cost_v: return 1;
	case magic_skill_costtype_v: return 2;
	case magic_skill_mintimepercast_v: return 3;
	case magic_skill_misslenum_v: return 4;
	case magic_skill_misslesform_v: return 5;
	case magic_skill_param1_v: return 6;
	case magic_skill_param2_v: return 7;
	case magic_skill_reserve1: return 8;
	case magic_skill_reserve2: return 9;
	case magic_skill_reserve3: return 10;
	case magic_skill_reserve4: return 11;
	case magic_skill_end: return 13;
	case magic_missle_begin: return 14;
	case magic_missle_movekind_v: return 15;
	case magic_missle_speed_v: return 16;
	case magic_missle_lifetime_v: return 17;
	case magic_missle_height_v: return 18;
	case magic_missle_damagerange_v: return 19;
	case magic_missle_radius_v: return 20;
	case magic_missle_reserve1: return 21;
	case magic_missle_reserve2: return 22;
	case magic_missle_reserve3: return 23;
	case magic_missle_reserve4: return 24;
	case magic_missle_reserve5: return 25;
	case magic_missle_end: return 26;
	case magic_item_begin: return 27;
	case magic_weapondamagemin_v: return 28;
	case magic_weapondamagemax_v: return 29;
	case magic_armordefense_v: return 30;
	case magic_durability_v: return 31;
	case magic_requirestr: return 32;
	case magic_requiredex: return 33;
	case magic_requirevit: return 34;
	case magic_requireeng: return 35;
	case magic_requirelevel: return 36;
	case magic_requireseries: return 37;
	case magic_requiresex: return 38;
	case magic_requiremenpai: return 39;
	case magic_weapondamageenhance_p: return 40;
	case magic_armordefenseenhance_p: return 41;
	case magic_requirementreduce_p: return 42;
	case magic_indestructible_b: return 43;
	case magic_item_reserve3: return 46;
	case magic_item_reserve4: return 47;
	case magic_item_reserve5: return 48;
	case magic_item_reserve6: return 49;
	case magic_item_reserve7: return 50;
	case magic_item_reserve8: return 51;
	case magic_item_reserve9: return 52;
	case magic_item_reserve10: return 53;
	case magic_item_end: return 54;
	case magic_damage_begin: return 55;
	case magic_attackrating_v: return 56;
	case magic_attackrating_p: return 57;
	case magic_ignoredefense_p: return 58;
	case magic_physicsdamage_v: return 59;
	case magic_colddamage_v: return 60;
	case magic_firedamage_v: return 61;
	case magic_lightingdamage_v: return 62;
	case magic_poisondamage_v: return 63;
	case magic_magicdamage_v: return 64;
	case magic_physicsenhance_p: return 65;
	case magic_steallife_p: return 66;
	case magic_stealmana_p: return 67;
	case magic_stealstamina_p: return 68;
	case magic_knockback_p: return 69;
	case magic_deadlystrike_p: return 70;
	case magic_fatallystrike_p: return 71;
	case magic_stun_p: return 72;
	case magic_damage_reserve2: return 74;
	case magic_damage_reserve3: return 75;
	case magic_damage_reserve4: return 76;
	case magic_damage_reserve5: return 77;
	case magic_damage_reserve6: return 78;
	case magic_damage_reserve7: return 79;
	case magic_damage_reserve8: return 80;
	case magic_damage_reserve9: return 81;
	case magic_damage_reserve10: return 82;
	case magic_damage_end: return 83;
	case magic_normal_begin: return 84;
	case magic_lifemax_v: return 85;
	case magic_lifemax_p: return 86;
	case magic_life_v: return 87;
	case magic_lifereplenish_v: return 88;
	case magic_manamax_v: return 89;
	case magic_manamax_p: return 90;
	case magic_mana_v: return 91;
	case magic_manareplenish_v: return 92;
	case magic_staminamax_v: return 93;
	case magic_staminamax_p: return 94;
	case magic_stamina_v: return 95;
	case magic_staminareplenish_v: return 96;
	case magic_strength_v: return 97;
	case magic_dexterity_v: return 98;
	case magic_poisonres_p: return 101;
	case magic_fireres_p: return 102;
	case magic_lightingres_p: return 103;
	case magic_physicsres_p: return 104;
	case magic_coldres_p: return 105;
	case magic_freezetimereduce_p: return 106;
	case magic_burntimereduce_p: return 107;
	case magic_poisontimereduce_p: return 108;
	case magic_poisondamagereduce_v: return 109;
	case magic_stuntimereduce_p: return 110;
	case magic_fastwalkrun_p: return 111;
	case magic_visionradius_p: return 112;
	case magic_fasthitrecover_v: return 113;
	case magic_allres_p: return 114;
	case magic_attackspeed_v: return 115;
	case magic_castspeed_v: return 116;
	case magic_meleedamagereturn_v: return 117;
	case magic_meleedamagereturn_p: return 118;
	case magic_rangedamagereturn_v: return 119;
	case magic_rangedamagereturn_p: return 120;
	case magic_addphysicsdamage_v: return 121;
	case magic_addfiredamage_v: return 122;
	case magic_addcolddamage_v: return 123;
	case magic_addlightingdamage_v: return 124;
	case magic_addpoisondamage_v: return 125;
	case magic_addphysicsdamage_p: return 126;
	case magic_slowmissle_b: return 127;
	case magic_changecamp_b: return 128;
	case magic_physicsarmor_v: return 129;
	case magic_coldarmor_v: return 130;
	case magic_firearmor_v: return 131;
	case magic_poisonarmor_v: return 132;
	case magic_lightingarmor_v: return 133;
	case magic_damagetomana_p: return 134;
	case magic_lucky_v: return 135;
	case magic_steallifeenhance_p: return 163;
	case magic_stealmanaenhance_p: return 164;
	case magic_stealstaminaenhance_p: return 165;
	case magic_allskill_v: return 136;
	case magic_metalskill_v: return 137;
	case magic_woodskill_v: return 138;
	case magic_waterskill_v: return 139;
	case magic_fireskill_v: return 140;
	case magic_earthskill_v: return 141;
	case magic_knockbackenhance_p: return 166;
	case magic_deadlystrikeenhance_p: return 142;
	case magic_stunenhance_p: return 167;
	case magic_badstatustimereduce_v: return 143;
	case magic_manashield_p: return 144;
	case magic_adddefense_v: return 145;
	case magic_adddefense_p: return 146;
	case magic_fatallystrikeenhance_p: return 147;
	case magic_lifepotion_v: return 148;
	case magic_manapotion_v: return 149;
	case magic_physicsresmax_p: return 150;
	case magic_coldresmax_p: return 151;
	case magic_fireresmax_p: return 152;
	case magic_lightingresmax_p: return 153;
	case magic_poisonresmax_p: return 154;
	case magic_allresmax_p: return 155;
	case magic_coldenhance_p: return 156;
	case magic_fireenhance_p: return 157;
	case magic_lightingenhance_p: return 158;
	case magic_poisonenhance_p: return 159;
	case magic_magicenhance_p: return 160;
	case magic_attackratingenhance_v: return 161;
	case magic_attackratingenhance_p: return 162;
	case magic_addexdefense_v: return 179;
	default: return -1;
	}
}
