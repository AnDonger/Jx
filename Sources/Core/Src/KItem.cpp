#include "KCore.h"
#include "MyAssert.H"
#include "KTabFile.h"
#include "KNpc.h"
#include "KItem.h"
#include "KItemSet.h"
#include "KItemGenerator.h"
#include "KMagicDesc.h"
#ifndef _STANDALONE
#include "../../../lib/S3DBInterface.h"
#else
#include "S3DBInterface.h"
#endif

#ifndef _SERVER
#include "ImgRef.h"
#include "KPlayer.h"
#include "../../Represent/iRepresent/iRepresentshell.h"
#endif

KItem	Item[MAX_ITEM];
int GetRandomNumber(int nMin, int nMax);

KItem::KItem()
{
	::memset(&m_CommonAttrib,    0, sizeof(m_CommonAttrib));
	::memset(m_aryBaseAttrib,    0, sizeof(m_aryBaseAttrib));
	::memset(m_aryRequireAttrib, 0, sizeof(m_aryRequireAttrib));
	::memset(m_aryMagicAttrib,   0, sizeof(m_aryMagicAttrib));
	::memset(&m_GeneratorParam,	 0, sizeof(m_GeneratorParam));
	m_nCurrentDur = -1;
	m_nStackCount = 1;
	m_dwUpgradeData = 0;
	m_btGenerationFlags = 0;
	m_btStateFlags = 0;
	m_btBindingMode = 0;
	m_btReforgeLuck = 0;
#ifndef _SERVER
	::memset(&m_Image,   0, sizeof(KRUImage));
#endif
	m_nIndex = 0;
}

KItem::~KItem()
{
}

void* KItem::GetRequirement(IN int nReq)
{
	int i = sizeof(m_aryRequireAttrib)/sizeof(m_aryRequireAttrib[0]);
	if (nReq >= i)
		return NULL;

	return &m_aryRequireAttrib[nReq];
}
/******************************************************************************
功能:	将item上的魔法应用到NPC身上
入口：	pNPC: 指向NPC的指针，nMagicAcive：打开的隐藏属性数目
出口:	魔法被应用。
		具体工作由KNpc的成员函数完成。
		KItem 对象本身没有成员变量被修改
******************************************************************************/
void KItem::ApplyMagicAttribToNPC(IN KNpc* pNPC, IN int nMagicActive /* = 0 */) const
{
	_ASSERT(this != NULL);
	_ASSERT(nMagicActive >= 0);

	int nCount = nMagicActive;
	int i;

	// 基础属性调整NPC
	for (i = 0; i < sizeof(m_aryBaseAttrib)/sizeof(m_aryBaseAttrib[0]); i++)
	{
		const KItemNormalAttrib* pAttrib;
		pAttrib = &(m_aryBaseAttrib[i]);
		if (INVALID_ATTRIB != pAttrib->nAttribType)
		{
			pNPC->ModifyAttrib(pNPC->m_Index, (void *)pAttrib);
		}
	}
	// 魔法属性调整NPC
	for (i = 0; i < sizeof(m_aryMagicAttrib)/sizeof(m_aryMagicAttrib[0]); i++)
	{
		const KItemNormalAttrib* pAttrib;
		pAttrib = &(m_aryMagicAttrib[i]);

		if (INVALID_ATTRIB != pAttrib->nAttribType)
		{
			if (i & 1)						// 为奇数，是后缀（i从零开始）
			{
				if (nCount > 0)
				{
					pNPC->ModifyAttrib(pNPC->m_Index, (void *)pAttrib);
					nCount--;
				}
			}
			else
			{
				pNPC->ModifyAttrib(pNPC->m_Index, (void *)pAttrib);
			}
		}
	}
}

/******************************************************************************
功能:	将item上的魔法从NPC身上移除
入口：	pNPC: 指向NPC的指针，nMagicAcive：打开的隐藏属性数目
出口:	魔法被应用。
		具体工作由KNpc的成员函数完成。
		KItem 对象本身没有成员变量被修改
******************************************************************************/
void KItem::RemoveMagicAttribFromNPC(IN KNpc* pNPC, IN int nMagicActive /* = 0 */) const
{
	_ASSERT(this != NULL);
	_ASSERT(nMagicActive >= 0);

	int nCount = nMagicActive;
	int	i;
	
	// 基础属性调整NPC
	for (i = 0; i < sizeof(m_aryBaseAttrib)/sizeof(m_aryBaseAttrib[0]); i++)
	{
		const KItemNormalAttrib* pAttrib;
		pAttrib = &(m_aryBaseAttrib[i]);
		if (INVALID_ATTRIB != pAttrib->nAttribType)
		{
			KItemNormalAttrib RemoveAttrib;
			RemoveAttrib.nAttribType = pAttrib->nAttribType;
			RemoveAttrib.nValue[0] = -pAttrib->nValue[0];
			RemoveAttrib.nValue[1] = -pAttrib->nValue[1];
			RemoveAttrib.nValue[2] = -pAttrib->nValue[2];
			pNPC->ModifyAttrib(pNPC->m_Index, (void *)&RemoveAttrib);
		}
	}

	for (i = 0; i < sizeof(m_aryMagicAttrib)/sizeof(m_aryMagicAttrib[0]); i++)
	{
		const KItemNormalAttrib* pAttrib;
		pAttrib = &(m_aryMagicAttrib[i]);

		if (INVALID_ATTRIB != pAttrib->nAttribType)		// TODO: 为 -1 定义一个常量?
		{
			KItemNormalAttrib RemoveAttrib;
			if (i & 1)						// 为奇数，是后缀（i从零开始）
			{
				if (nCount > 0)
				{
					RemoveAttrib.nAttribType = pAttrib->nAttribType;
					RemoveAttrib.nValue[0] = -pAttrib->nValue[0];
					RemoveAttrib.nValue[1] = -pAttrib->nValue[1];
					RemoveAttrib.nValue[2] = -pAttrib->nValue[2];
					pNPC->ModifyAttrib(pNPC->m_Index, (void *)&RemoveAttrib);
					nCount--;
				}
			}
			else
			{
				RemoveAttrib.nAttribType = pAttrib->nAttribType;
				RemoveAttrib.nValue[0] = -pAttrib->nValue[0];
				RemoveAttrib.nValue[1] = -pAttrib->nValue[1];
				RemoveAttrib.nValue[2] = -pAttrib->nValue[2];
				pNPC->ModifyAttrib(pNPC->m_Index, (void *)&RemoveAttrib);
			}
		}
	}
}

/******************************************************************************
功能:	将item上的第N项隐藏魔法属性应用到NPC身上
入口：	pNPC: 指向NPC的指针
出口:	魔法被应用。
		具体工作由KNpc的成员函数完成。
		KItem 对象本身没有成员变量被修改
******************************************************************************/
void KItem::ApplyHiddenMagicAttribToNPC(IN KNpc* pNPC, IN int nMagicActive) const
{
	_ASSERT(this != NULL);
	if (nMagicActive <= 0)
		return;

	const KItemNormalAttrib* pAttrib;
	pAttrib = &(m_aryMagicAttrib[(nMagicActive << 1) - 1]);	// 后缀为隐藏属性所以乘2减一
	if (-1 != pAttrib->nAttribType)
	{
		pNPC->ModifyAttrib(pNPC->m_Index, (void *)pAttrib);
	}
}

/******************************************************************************
功能:	将item上的第N项隐藏魔法属性从NPC身上移除
入口：	pNPC: 指向NPC的指针，nMagicActive：第n项魔法属性
出口:	魔法被移除。
		具体工作由KNpc的成员函数完成。
		KItem 对象本身没有成员变量被修改
******************************************************************************/
void KItem::RemoveHiddenMagicAttribFromNPC(IN KNpc* pNPC, IN int nMagicActive) const
{
	_ASSERT(this != NULL);
	if (nMagicActive <= 0)
		return;

	const KItemNormalAttrib* pAttrib;
	pAttrib = &(m_aryMagicAttrib[(nMagicActive << 1) - 1]);	// 后缀为隐藏属性所以乘2减一
	if (-1 != pAttrib->nAttribType)
	{
		KItemNormalAttrib RemoveAttrib;
		RemoveAttrib.nAttribType = pAttrib->nAttribType;
		RemoveAttrib.nValue[0] = -pAttrib->nValue[0];
		RemoveAttrib.nValue[1] = -pAttrib->nValue[1];
		RemoveAttrib.nValue[2] = -pAttrib->nValue[2];
		pNPC->ModifyAttrib(pNPC->m_Index, (void *)&RemoveAttrib);
	}
}

/******************************************************************************
功能:	根据配置文件中的数据,为item的各项赋初值
入口：	pData: 给出来自配置文件的数据
出口:	成功时返回非零, 以下成员变量被值:
			m_CommonAttrib,m_aryBaseAttrib,m_aryRequireAttrib
		失败时返回零
说明:	CBR: Common,Base,Require
******************************************************************************/
BOOL KItem::SetAttrib_CBR(IN const KBASICPROP_EQUIPMENT* pData)
{
	_ASSERT(pData != NULL);
	
	BOOL bEC = FALSE;
	if (pData)
	{
		//SetAttrib_Common(pData);
		*this = *pData;		// 运算符重载
		SetAttrib_Base(pData->m_aryPropBasic);
		SetAttrib_Req(pData->m_aryPropReq);
		bEC = TRUE;
	}
	return bEC;
}

BOOL KItem::SetAttrib_Base(const KEQCP_BASIC* pBasic)
{
	m_nCurrentDur = -1;
	for (int i = 0;
		 i < sizeof(m_aryBaseAttrib)/sizeof(m_aryBaseAttrib[0]); i++)
	{
		KItemNormalAttrib* pDst;
		const KEQCP_BASIC* pSrc;
		pDst = &(m_aryBaseAttrib[i]);
		pSrc = &(pBasic[i]);
		pDst->nAttribType = pSrc->nType;
		// 当前客户端前五项为基础属性，有颜色的装备再从模板生成五项固定附加属性。
		// 普通白装不消耗这五次随机数，保持后续随机魔法属性的序列。
		// Luck为0时基础属性取最小值；非0时按客户端种子生成。
		if (i >= 5 && m_CommonAttrib.nSeries == 0)
			pDst->nValue[0] = pSrc->sRange.nMax;
		else if (i < 5 && m_GeneratorParam.nLuck == 0)
			pDst->nValue[0] = pSrc->sRange.nMin;
		else
			pDst->nValue[0] = ItemGen.GetRandomNumber(pSrc->sRange.nMin, pSrc->sRange.nMax);
		pDst->nValue[1] = 0;	// RESERVED
		pDst->nValue[2] = 0;	// RESERVED
		if (pDst->nAttribType == magic_durability_v)
			SetDurability(pDst->nValue[0]);
	}
	if (m_nCurrentDur == 0)	// 说明没有耐久度属性
		m_nCurrentDur = -1;
	return TRUE;
}

BOOL KItem::SetAttrib_Req(const KEQCP_REQ* pReq)
{
	for (int i = 0;
		 i < sizeof(m_aryRequireAttrib)/sizeof(m_aryRequireAttrib[0]); i++)
	{
		KItemNormalAttrib* pDst;
		pDst = &(m_aryRequireAttrib[i]);
		pDst->nAttribType = pReq[i].nType;
		pDst->nValue[0] = pReq[i].nPara;
		pDst->nValue[1] = 0;	// RESERVED
		pDst->nValue[2] = 0;	// RESERVED
	}
	return TRUE;
}

/******************************************************************************
功能:	根据传入的数据, 为item的魔法属性赋初值
入口：	pMA: 给出数据
出口:	成功时返回非零, 以下成员变量被值:
			m_aryMagicAttrib
		失败时返回零
******************************************************************************/
BOOL KItem::SetAttrib_MA(IN const KItemNormalAttrib* pMA)
{
	if (NULL == pMA)
		{ _ASSERT(FALSE); return FALSE; }

	for (int i = 0; i < sizeof(m_aryMagicAttrib) / sizeof(m_aryMagicAttrib[0]); i++)
	{
		m_aryMagicAttrib[i] = pMA[i];
		// 永不磨损
		if (m_aryMagicAttrib[i].nAttribType == magic_indestructible_b)
		{
			SetDurability(-1);
		}
	}
	return TRUE;
}

/******************************************************************************
功能:	根据传入的数据, 为item的魔法属性赋初值
入口：	pMA: 给出数据
出口:	成功时返回非零, 以下成员变量被值:
			m_aryMagicAttrib
		失败时返回零
******************************************************************************/
BOOL KItem::SetAttrib_MA(IN const KMACP* pMA)
{
	if (NULL == pMA)
		{ _ASSERT(FALSE); return FALSE; }

	for (int i = 0; i < sizeof(m_aryMagicAttrib) / sizeof(m_aryMagicAttrib[0]); i++)
	{
		const KMACP* pSrc;
		KItemNormalAttrib* pDst;
		pSrc = &(pMA[i]);
		pDst = &(m_aryRequireAttrib[i]);

		pDst->nAttribType = pSrc->nPropKind;
		pDst->nValue[0] =  ::GetRandomNumber(pSrc->aryRange[0].nMin, pSrc->aryRange[0].nMax);
		pDst->nValue[1] =  ::GetRandomNumber(pSrc->aryRange[1].nMin, pSrc->aryRange[1].nMax);
		pDst->nValue[2] =  ::GetRandomNumber(pSrc->aryRange[2].nMin, pSrc->aryRange[2].nMax);
	}
	return TRUE;
}

void KItem::operator = (const KBASICPROP_EQUIPMENT& sData)
{
	KItemCommonAttrib* pCA = &m_CommonAttrib;
	pCA->nItemGenre		 = sData.m_nItemGenre;
	pCA->nDetailType	 = sData.m_nDetailType;
	pCA->nParticularType = sData.m_nParticularType;
	pCA->nObjIdx		 = sData.m_nObjIdx;
	pCA->bStack			 = sData.m_bStack;
	pCA->nWidth			 = sData.m_nWidth;
	pCA->nHeight		 = sData.m_nHeight;
	pCA->nPrice			 = sData.m_nPrice;
	pCA->nLevel			 = sData.m_nLevel;
	pCA->nSeries		 = sData.m_nSeries;
	::strcpy(pCA->szItemName,  sData.m_szName);
#ifndef _SERVER
	::strcpy(pCA->szImageName, sData.m_szImageName);
	::strcpy(pCA->szIntro,	   sData.m_szIntro);

	m_Image.Color.Color_b.a = 255;
	m_Image.nFrame = 0;
	m_Image.nISPosition = IMAGE_IS_POSITION_INIT;
	m_Image.nType = ISI_T_SPR;
	::strcpy(m_Image.szImage, pCA->szImageName);
	m_Image.uImage = 0;
#endif
}

void KItem::operator = (const KBASICPROP_MEDMATERIAL& sData)
{
	// 赋值: 共同属性部分
	KItemCommonAttrib* pCA = &(m_CommonAttrib);
	pCA->nItemGenre		 = sData.m_nItemGenre;
	pCA->nDetailType	 = sData.m_nDetailType;
	pCA->nParticularType = sData.m_nParticularType;
	pCA->nObjIdx		 = sData.m_nObjIdx;
	pCA->bStack			 = sData.m_bStack;
	pCA->nWidth			 = sData.m_nWidth;
	pCA->nHeight		 = sData.m_nHeight;
	pCA->nPrice			 = sData.m_nPrice;
	pCA->nLevel			 = sData.m_nLevel;
	pCA->nSeries		 = sData.m_nSeries;
	::strcpy(pCA->szItemName,  sData.m_szName);
#ifndef _SERVER
	::strcpy(pCA->szImageName, sData.m_szImageName);
	::strcpy(pCA->szIntro,	   sData.m_szIntro);
#endif
	// 赋值: 基本属性部分
	ZeroMemory(m_aryBaseAttrib, sizeof(m_aryBaseAttrib));
	ZeroMemory(m_aryRequireAttrib, sizeof(m_aryRequireAttrib));
	ZeroMemory(m_aryMagicAttrib, sizeof(m_aryMagicAttrib));
	KItemNormalAttrib* pBA = m_aryBaseAttrib;
	pBA[0].nAttribType = sData.m_nAttrib1_Type;
	pBA[0].nValue[0]   = sData.m_nAttrib1_Para;
	pBA[1].nAttribType = sData.m_nAttrib2_Type;
	pBA[1].nValue[0]   = sData.m_nAttrib2_Para;
	pBA[2].nAttribType = sData.m_nAttrib3_Type;
	pBA[2].nValue[0]   = sData.m_nAttrib3_Para;
	
	// 赋值: 需求属性部分: 无
	// 赋值: 魔法属性部分: 无
#ifndef _SERVER
	m_Image.Color.Color_b.a = 255;
	m_Image.nFrame = 0;
	m_Image.nISPosition = IMAGE_IS_POSITION_INIT;
	m_Image.nType = ISI_T_SPR;
	::strcpy(m_Image.szImage, pCA->szImageName);
	m_Image.uImage = 0;
#endif
}

void KItem::operator = (const KBASICPROP_MINE& sData)
{
	// 赋值: 共同属性部分
	KItemCommonAttrib* pCA = &m_CommonAttrib;
	pCA->nItemGenre		 = sData.m_nItemGenre;
	pCA->nDetailType	 = sData.m_nDetailType;
	pCA->nParticularType = sData.m_nParticularType;
	pCA->nObjIdx		 = sData.m_nObjIdx;
	pCA->bStack			 = sData.m_bStack;
	pCA->nWidth			 = sData.m_nWidth;
	pCA->nHeight		 = sData.m_nHeight;
	pCA->nPrice			 = sData.m_nPrice;
	pCA->nLevel			 = sData.m_nLevel;
	::strcpy(pCA->szItemName,  sData.m_szName);
#ifndef _SERVER
	::strcpy(pCA->szImageName, sData.m_szImageName);
	::strcpy(pCA->szIntro,	   sData.m_szIntro);
#endif
	// 赋值: 需求属性部分: 无
	// 赋值: 魔法属性部分: 无
#ifndef _SERVER
	m_Image.Color.Color_b.a = 255;
	m_Image.nFrame = 0;
	m_Image.nISPosition = IMAGE_IS_POSITION_INIT;
	m_Image.nType = ISI_T_SPR;
	::strcpy(m_Image.szImage, pCA->szImageName);
	m_Image.uImage = 0;
#endif
}

void KItem::operator = (const KBASICPROP_QUEST& sData)
{
	// 赋值: 共同属性部分
	KItemCommonAttrib* pCA = &m_CommonAttrib;
	pCA->nItemGenre		 = sData.m_nItemGenre;
	pCA->nDetailType	 = sData.m_nDetailType;
	pCA->nParticularType = sData.m_nParticularType;
	pCA->nObjIdx		 = sData.m_nObjIdx;
	pCA->bStack			 = sData.m_bStack;
	pCA->nWidth			 = sData.m_nWidth;
	pCA->nHeight		 = sData.m_nHeight;
	pCA->nPrice			 = 0;
	pCA->nLevel			 = 1;
	pCA->nSeries		 = -1;
	::strcpy(pCA->szItemName,  sData.m_szName);
#ifndef _SERVER
	::strcpy(pCA->szImageName, sData.m_szImageName);
	::strcpy(pCA->szIntro,	   sData.m_szIntro);
#endif
	ZeroMemory(m_aryBaseAttrib, sizeof(m_aryBaseAttrib));
	// 赋值: 需求属性部分: 无
	ZeroMemory(m_aryRequireAttrib, sizeof(m_aryRequireAttrib));
	// 赋值: 魔法属性部分: 无
	ZeroMemory(m_aryMagicAttrib, sizeof(m_aryMagicAttrib));
#ifndef _SERVER
	m_Image.Color.Color_b.a = 255;
	m_Image.nFrame = 0;
	m_Image.nISPosition = IMAGE_IS_POSITION_INIT;
	m_Image.nType = ISI_T_SPR;
	::strcpy(m_Image.szImage, pCA->szImageName);
	m_Image.uImage = 0;
#endif
}

void KItem::operator = (const KBASICPROP_TOWNPORTAL& sData)
{
	// 赋值: 共同属性部分
	KItemCommonAttrib* pCA = &m_CommonAttrib;
	pCA->nItemGenre		 = sData.m_nItemGenre;
	pCA->nDetailType	 = 0;
	pCA->nParticularType = 0;
	pCA->nObjIdx		 = sData.m_nObjIdx;
	pCA->bStack			 = sData.m_bStack;
	pCA->nWidth			 = sData.m_nWidth;
	pCA->nHeight		 = sData.m_nHeight;
	pCA->nPrice			 = sData.m_nPrice;
	pCA->nLevel			 = 1;
	pCA->nSeries		 = -1;
	::strcpy(pCA->szItemName,  sData.m_szName);
#ifndef _SERVER
	::strcpy(pCA->szImageName, sData.m_szImageName);
	::strcpy(pCA->szIntro,	   sData.m_szIntro);
#endif
	ZeroMemory(m_aryBaseAttrib, sizeof(m_aryBaseAttrib));
	// 赋值: 需求属性部分: 无
	ZeroMemory(m_aryRequireAttrib, sizeof(m_aryRequireAttrib));
	// 赋值: 魔法属性部分: 无
	ZeroMemory(m_aryMagicAttrib, sizeof(m_aryMagicAttrib));
#ifndef _SERVER
	m_Image.Color.Color_b.a = 255;
	m_Image.nFrame = 0;
	m_Image.nISPosition = IMAGE_IS_POSITION_INIT;
	m_Image.nType = ISI_T_SPR;
	::strcpy(m_Image.szImage, pCA->szImageName);
	m_Image.uImage = 0;
#endif
}

void KItem::operator = (const KBASICPROP_NORMALITEM& sData)
{
	KItemCommonAttrib* pCA = &m_CommonAttrib;
	ZeroMemory(pCA, sizeof(*pCA));
	pCA->nItemGenre = sData.m_nItemGenre;
	pCA->nDetailType = sData.m_nDetailType;
	pCA->nParticularType = sData.m_nParticularType;
	pCA->nObjIdx = sData.m_nObjIdx;
	pCA->bStack = sData.m_bStack;
	pCA->nWidth = sData.m_nWidth;
	pCA->nHeight = sData.m_nHeight;
	pCA->nPrice = sData.m_nPrice;
	pCA->nLevel = sData.m_nLevel;
	pCA->nSeries = sData.m_nSeries;
	strcpy(pCA->szItemName, sData.m_szName);
	ZeroMemory(m_aryBaseAttrib, sizeof(m_aryBaseAttrib));
	ZeroMemory(m_aryRequireAttrib, sizeof(m_aryRequireAttrib));
	ZeroMemory(m_aryMagicAttrib, sizeof(m_aryMagicAttrib));
#ifndef _SERVER
	strcpy(pCA->szImageName, sData.m_szImageName);
	g_StrCpyLen(pCA->szIntro, sData.m_szIntro, sizeof(pCA->szIntro));
	m_Image.Color.Color_b.a = 255;
	m_Image.nFrame = 0;
	m_Image.nISPosition = IMAGE_IS_POSITION_INIT;
	m_Image.nType = ISI_T_SPR;
	strcpy(m_Image.szImage, pCA->szImageName);
	m_Image.uImage = 0;
#endif
}

void KItem::operator = (const KBASICPROP_MEDICINE& sData)
{
	// 赋值: 共同属性部分
	KItemCommonAttrib* pCA = &m_CommonAttrib;
	pCA->nItemGenre		 = sData.m_nItemGenre;
	pCA->nDetailType	 = sData.m_nDetailType;
	pCA->nParticularType = sData.m_nParticularType;
	pCA->nObjIdx		 = sData.m_nObjIdx;
	pCA->bStack			 = sData.m_bStack;
	pCA->nWidth			 = sData.m_nWidth;
	pCA->nHeight		 = sData.m_nHeight;
	pCA->nPrice			 = sData.m_nPrice;
	pCA->nLevel			 = sData.m_nLevel;
	pCA->nSeries		 = sData.m_nSeries;
	::strcpy(pCA->szItemName,  sData.m_szName);
#ifndef _SERVER
	::strcpy(pCA->szImageName, sData.m_szImageName);
	::strcpy(pCA->szIntro,	   sData.m_szIntro);
#endif
	// 赋值: 基本属性部分
	ZeroMemory(m_aryBaseAttrib, sizeof(m_aryBaseAttrib));
	KItemNormalAttrib* pBA = m_aryBaseAttrib;
	pBA[0].nAttribType = sData.m_aryAttrib[0].nAttrib;
	pBA[0].nValue[0]   = sData.m_aryAttrib[0].nValue;
	pBA[0].nValue[1]   = sData.m_aryAttrib[0].nTime;
	pBA[1].nAttribType = sData.m_aryAttrib[1].nAttrib;
	pBA[1].nValue[0]   = sData.m_aryAttrib[1].nValue;
	pBA[1].nValue[1]   = sData.m_aryAttrib[1].nTime;
	
	ZeroMemory(m_aryRequireAttrib, sizeof(m_aryRequireAttrib));
	ZeroMemory(m_aryMagicAttrib, sizeof(m_aryMagicAttrib));

#ifndef _SERVER
	m_Image.Color.Color_b.a = 255;
	m_Image.nFrame = 0;
	m_Image.nISPosition = IMAGE_IS_POSITION_INIT;
	m_Image.nType = ISI_T_SPR;
	::strcpy(m_Image.szImage, pCA->szImageName);
	m_Image.uImage = 0;
#endif

}

void KItem::operator = (const KBASICPROP_EQUIPMENT_UNIQUE& sData)
{
	// 赋值: 共同属性部分
	KItemCommonAttrib* pCA = &m_CommonAttrib;
	pCA->nItemGenre		 = sData.m_nItemGenre;
	pCA->nDetailType	 = sData.m_nDetailType;
	pCA->nParticularType = sData.m_nParticularType;
	pCA->nObjIdx		 = sData.m_nObjIdx;
	pCA->nPrice			 = sData.m_nPrice;
	pCA->nLevel			 = sData.m_nLevel;
	pCA->nSeries		 = sData.m_nSeries;
	::strcpy(pCA->szItemName,  sData.m_szName);
#ifndef _SERVER
	::strcpy(pCA->szImageName, sData.m_szImageName);
	::strcpy(pCA->szIntro,	   sData.m_szIntro);

	m_Image.Color.Color_b.a = 255;
	m_Image.nFrame = 0;
	m_Image.nISPosition = IMAGE_IS_POSITION_INIT;
	m_Image.nType = ISI_T_SPR;
	::strcpy(m_Image.szImage, pCA->szImageName);
	m_Image.uImage = 0;
#endif
}

//flying add this overloaded operator to generate a gold item.
void KItem::operator = (const KBASICPROP_EQUIPMENT_GOLD& sData)
{
	KItemCommonAttrib* pCA = &m_CommonAttrib;	
	pCA->nItemGenre		 = sData.m_nItemGenre;
	pCA->nDetailType	 = sData.m_nDetailType;
	pCA->nParticularType = sData.m_nParticularType;
	pCA->nObjIdx		 = sData.m_nObjIdx;
	pCA->nPrice			 = sData.m_nPrice;
	pCA->nLevel			 = sData.m_nLevel;
	pCA->nSeries		 = sData.m_nSeries;
	pCA->nWidth			 = sData.m_nWidth;
	pCA->nHeight		 = sData.m_nHeight;
	::strcpy(pCA->szItemName,  sData.m_szName);
#ifndef _SERVER
	::strcpy(pCA->szImageName, sData.m_szImageName);
	::strcpy(pCA->szIntro,	   sData.m_szIntro);
	m_Image.Color.Color_b.a = 255;
	m_Image.nFrame = 0;
	m_Image.nISPosition = IMAGE_IS_POSITION_INIT;
	m_Image.nType = ISI_T_SPR;
	::strcpy(m_Image.szImage, pCA->szImageName);
	m_Image.uImage = 0;
#endif
}

BOOL KItem::Gen_Equipment_Unique(const KBASICPROP_EQUIPMENT* pEqu,
								 const KBASICPROP_EQUIPMENT_UNIQUE* pUni)
{
	_ASSERT(this != NULL);
	_ASSERT(pEqu != NULL);
	_ASSERT(pUni != NULL);

	if (NULL == pEqu || NULL == pUni)
		{ _ASSERT(FALSE); return FALSE; }

	// 赋值: 共同属性部分
	*this = *pUni;		// 运算符重载
	KItemCommonAttrib* pCA = &m_CommonAttrib;
	pCA->bStack  = pEqu->m_bStack;
	pCA->nWidth  = pEqu->m_nWidth;
	pCA->nHeight = pEqu->m_nHeight;

	SetAttrib_Base(pEqu->m_aryPropBasic);		// 赋值: 基本属性部分
	SetAttrib_Req(pUni->m_aryPropReq);			// 赋值: 需求属性部分
	SetAttrib_MA(pUni->m_aryMagicAttribs);		// 赋值: 魔法属性部分

	return TRUE;
}

int KItem::GetMaxStackCount() const
{
	if (m_CommonAttrib.bStack <= 0)
		return 1;
	// 原物品表“是否叠加”可直接填写数量；旧表的1兼容布尔标记。
	if (m_CommonAttrib.bStack == 1 || m_CommonAttrib.bStack > 255)
		return 255;
	return m_CommonAttrib.bStack;
}

// 发奖脚本需要在入包合叠之前取得说明，避免继续访问已被合并的实例。
BOOL KItem::GetMagicDescription(char* pszText, int nSize) const
{
	if (!pszText || nSize <= 0)
		return FALSE;
	pszText[0] = 0;
	int nLength = 0;
	for (int i = 0; i < 6; i++)
	{
		if (!m_aryMagicAttrib[i].nAttribType)
			continue;
		const char* pszDesc = GetGenre() == item_materials ?
			g_MagicDesc.GetGemDesc(m_aryMagicAttrib[i]) :
			g_MagicDesc.GetDesc((void*)&m_aryMagicAttrib[i]);
		if (!pszDesc || !pszDesc[0])
			return FALSE;
		int nBytes = strlen(pszDesc);
		if (nLength + nBytes + (nLength ? 1 : 0) >= nSize)
			return FALSE;
		if (nLength)
			pszText[nLength++] = '\n';
		memcpy(pszText + nLength, pszDesc, nBytes);
		nLength += nBytes;
		pszText[nLength] = 0;
	}
	return TRUE;
}

BOOL KItem::CanStackWith(const KItem& Other) const
{
	if (GetMaxStackCount() <= 1 || Other.GetMaxStackCount() != GetMaxStackCount() ||
		GetGenre() == item_equip || GetGenre() != Other.GetGenre() ||
		GetDetailType() != Other.GetDetailType() || GetSeries() != Other.GetSeries() ||
		m_CommonAttrib.nParticularType != Other.m_CommonAttrib.nParticularType ||
		m_CommonAttrib.nLevel != Other.m_CommonAttrib.nLevel ||
		m_CommonAttrib.nWidth != Other.m_CommonAttrib.nWidth ||
		m_CommonAttrib.nHeight != Other.m_CommonAttrib.nHeight ||
		m_GeneratorParam.nVersion != Other.m_GeneratorParam.nVersion ||
		m_GeneratorParam.nLuck != Other.m_GeneratorParam.nLuck ||
		m_nCurrentDur != Other.m_nCurrentDur)
		return FALSE;
	// 相同表项仍须核对效果，避免把不同属性的消耗品合并。
	return memcmp(m_aryBaseAttrib, Other.m_aryBaseAttrib, sizeof(m_aryBaseAttrib)) == 0 &&
		memcmp(m_aryRequireAttrib, Other.m_aryRequireAttrib, sizeof(m_aryRequireAttrib)) == 0 &&
		memcmp(m_aryMagicAttrib, Other.m_aryMagicAttrib, sizeof(m_aryMagicAttrib)) == 0 &&
		memcmp(m_GeneratorParam.nGeneratorLevel, Other.m_GeneratorParam.nGeneratorLevel,
			sizeof(m_GeneratorParam.nGeneratorLevel)) == 0;
}

BOOL KItem::SetStackCount(int nCount)
{
	if (nCount < 1 || nCount > GetMaxStackCount())
		return FALSE;
	m_nStackCount = nCount;
	return TRUE;
}

void KItem::Remove()
{
	m_nIndex = 0;
	m_nStackCount = 1;
	m_dwUpgradeData = 0;
	m_btGenerationFlags = 0;
	m_btStateFlags = 0;
	m_btBindingMode = 0;
	m_btReforgeLuck = 0;
}

BOOL KItem::SetBaseAttrib(IN const KItemNormalAttrib* pAttrib)
{
	if (!pAttrib)
		return FALSE;

	for (int i = 0; i < sizeof(m_aryBaseAttrib) / sizeof(m_aryBaseAttrib[0]); i++)
	{
		m_aryBaseAttrib[i] = pAttrib[i];
	}
	return TRUE;
}

BOOL KItem::SetRequireAttrib(IN const KItemNormalAttrib* pAttrib)
{
	if (!pAttrib)
		return FALSE;

	for (int i = 0; i < sizeof(m_aryRequireAttrib) / sizeof(m_aryRequireAttrib[0]); i++)
	{
		m_aryRequireAttrib[i] = pAttrib[i];
	}
	return TRUE;
}

BOOL KItem::SetMagicAttrib(IN const KItemNormalAttrib* pAttrib)
{
	return SetAttrib_MA(pAttrib);
}

//------------------------------------------------------------------
//	磨损，返回值表示剩余耐久度
//------------------------------------------------------------------
int KItem::Abrade(IN const int nRandRange)
{
	if (m_nCurrentDur == -1 || nRandRange == 0)	// 永不磨损
		return -1;

	if (m_nCurrentDur <= 0)
		return 0;

	if (g_Random(nRandRange) == 0)	// nRandRange分之一的概率
	{
		m_nCurrentDur--;
		if (m_nCurrentDur == 0)
		{
			return 0;
		}
	}
	return m_nCurrentDur;
}

#ifndef _SERVER
void KItem::Paint(int nX, int nY)
{
	m_Image.oPosition.nX = nX;
	m_Image.oPosition.nY = nY;
	m_Image.bRenderStyle = IMAGE_RENDER_STYLE_ALPHA;
	g_pRepresent->DrawPrimitives(1, &m_Image, RU_T_IMAGE, TRUE);
}

void KItem::GetDesc(char* pszMsg, bool bShowPrice, int nPriceScale, int nActiveAttrib)
{
	char	szColor[item_number][32] = 
	{
		"",
		"<color=White>",
		"",
		"",
		"<color=Yellow>",
	};

	if (m_CommonAttrib.nItemGenre == item_equip)
	{
		if (m_CommonAttrib.nLevel > 10)	// 黄金装备
		{
			strcpy(szColor[item_equip], "<color=Yellow>");
		}
		else if (m_aryMagicAttrib[0].nAttribType)	// 魔法装备
		{
			strcpy(szColor[item_equip], "<color=Blue>");
		}
		else
		{
			strcpy(szColor[item_equip], "<color=White>");
		}
	}
	strcpy(pszMsg, szColor[m_CommonAttrib.nItemGenre]);
	strcat(pszMsg, m_CommonAttrib.szItemName);
	strcat(pszMsg, "\n");
	if (bShowPrice && nPriceScale > 0)
	{
		char szPrice[32];
		sprintf(szPrice, "价格：%d", m_CommonAttrib.nPrice / nPriceScale);
		strcat(pszMsg, szPrice);
		strcat(pszMsg, "\n");
	}
	if (m_CommonAttrib.nItemGenre == item_equip)
	{
		switch(m_CommonAttrib.nSeries)
		{
		case series_metal:
			strcat(pszMsg, "<color=White>五行属性：<color=Metal>金");
			break;
		case series_wood:
			strcat(pszMsg, "<color=White>五行属性：<color=Wood>木");
			break;
		case series_water:
			strcat(pszMsg, "<color=White>五行属性：<color=Water>水");
			break;
		case series_fire:
			strcat(pszMsg, "<color=White>五行属性：<color=Fire>火");
			break;
		case series_earth:
			strcat(pszMsg, "<color=White>五行属性：<color=Earth>土");
			break;
		}
	}
	strcat(pszMsg, "\n");
	strcat(pszMsg, "<color=White>");
	strcat(pszMsg, m_CommonAttrib.szIntro);
	strcat(pszMsg, "\n");
	int i;//tuannd
	for ( i = 0; i < sizeof(m_aryBaseAttrib) / sizeof(m_aryBaseAttrib[0]); i++)
	{
		if (!m_aryBaseAttrib[i].nAttribType)
		{
			continue;
		}
		if (m_aryBaseAttrib[i].nAttribType == magic_durability_v)
		{
			char	szDurInfo[32];
			if (m_nCurrentDur == -1)
				sprintf(szDurInfo, "<color=Yellow>永不磨损<color=White>");
			else
				sprintf(szDurInfo, "耐久度 %3d / %3d", GetDurability(), GetMaxDurability());
			strcat(pszMsg, szDurInfo);
		}
		else
		{
			char* pszInfo = (char *)g_MagicDesc.GetDesc(&m_aryBaseAttrib[i]);
			if (!pszInfo || !pszInfo[0])
				continue;
			strcat(pszMsg, pszInfo);
		}
		strcat(pszMsg, "\n");
	}
 
	for (i = 0; i < 6; i++)
	{
		if (!m_aryRequireAttrib[i].nAttribType)
		{
			continue;
		}
		char* pszInfo = (char *)g_MagicDesc.GetDesc(&m_aryRequireAttrib[i]);
		if (!pszInfo || !pszInfo[0])
			continue;
		if (Player[CLIENT_PLAYER_INDEX].m_ItemList.EnoughAttrib(&m_aryRequireAttrib[i]))
		{
			strcat(pszMsg, "<color=White>");
		}
		else
		{
			strcat(pszMsg, "<color=Red>");
		}
		strcat(pszMsg, pszInfo);
		strcat(pszMsg, "\n");
	}

	for (i = 0; i < 6; i++)
	{
		if (!m_aryMagicAttrib[i].nAttribType)
		{
			continue;
		}
		char* pszInfo = (char *)g_MagicDesc.GetDesc(&m_aryMagicAttrib[i]);
		if (!pszInfo || !pszInfo[0])
			continue;
		if ((i & 1) == 0)
		{
			strcat(pszMsg, "<color=HBlue>");
		}
		else
		{
			if ((i>>1) < nActiveAttrib)
			{
				strcat(pszMsg, "<color=HBlue>");
			}
			else
			{
				strcat(pszMsg, "<color=DBlue>");
			}
		}
		strcat(pszMsg, pszInfo);
		strcat(pszMsg, "\n");
	}
}
#endif

int KItem::GetMaxDurability()
{
	for (int i = 0; i < sizeof(m_aryBaseAttrib) / sizeof(m_aryBaseAttrib[0]); i++)
	{
		if (m_aryBaseAttrib[i].nAttribType == magic_durability_v)
		{
			return m_aryBaseAttrib[i].nValue[0];
		}
	}
	return -1;
}

int KItem::GetTotalMagicLevel()
{
	int nRet = 0;
	for (int i = 0; i < 6; i++)
	{
		nRet += m_GeneratorParam.nGeneratorLevel[i];
	}
	return nRet;
}

int KItem::GetMagicAttribCount() const
{
	int nCount = 0;
	for (int i = 0; i < 6; i++)
	{
		if (m_aryMagicAttrib[i].nAttribType > 0)
			nCount++;
	}
	return nCount;
}

int KItem::GetRequireLevel() const
{
	int nLevel = 0;
	for (int i = 0; i < 6; i++)
	{
		if (m_aryRequireAttrib[i].nAttribType == magic_requirelevel)
			nLevel = max(nLevel, m_aryRequireAttrib[i].nValue[0]);
	}
	return nLevel;
}

int KItem::GetRepairPrice()
{
	if (ItemSet.m_sRepairParam.nMagicScale == 0)
		return 0;

	if (GetGenre() != item_equip)
		return 0;

	if (m_nCurrentDur == -1)
		return 0;

	int nMaxDur = GetMaxDurability();
	int nSumMagic = GetTotalMagicLevel();

	if (nMaxDur <= 0)
		return 0;


	return m_CommonAttrib.nPrice * ItemSet.m_sRepairParam.nPriceScale / 100 * (nMaxDur - m_nCurrentDur) / nMaxDur * (ItemSet.m_sRepairParam.nMagicScale + nSumMagic) / ItemSet.m_sRepairParam.nMagicScale;
}

BOOL KItem::CanBeRepaired()
{
	if (GetGenre() != item_equip)
		return FALSE;

	if (m_nCurrentDur == -1)
		return FALSE;

	int nMaxDur = GetMaxDurability();
	if (m_nCurrentDur == nMaxDur)
		return FALSE;

	return TRUE;
}