#include "KEngine.h"
#include "KCore.h"
#include "KTabFile.h"
#include "KPlayer.h"
#include "KNpc.h"
#include "KItemGenerator.h"
#include "KSubWorldSet.h"
#include "KItemSet.h"
#include "KBuySell.h"
#ifndef _STANDALONE
#include "CoreShell.h"
#include "crtdbg.h"
#endif
#include "CoreUseNameDef.h"
#ifdef _SERVER
//#include "../MultiServer/Heaven/Interface/iServer.h"
#endif

#define	SHOP_BOX_WIDTH		6
#define	SHOP_BOX_HEIGHT		10

KBuySell	BuySell;

KBuySell::KBuySell()
{
#ifndef _SERVER
	m_pShopRoom = NULL;
#endif
	m_Item = NULL;
	m_SellItem = NULL;
	m_Width = 0;
	m_Height = 0;
	m_MaxItem = 0;
	m_CashGoods = NULL;
#ifdef _SERVER
	m_MallGoods = NULL;
	m_MallItem = NULL;
	m_MallCount = 0;
	m_AutoBuyCurrency = NULL;
#endif
}

KBuySell::~KBuySell()
{
#ifndef _SERVER
	if (m_pShopRoom)
	{
		delete m_pShopRoom;
		m_pShopRoom = NULL;
	}
#endif
	if (m_Item)
	{
		delete [] m_Item;
		m_Item = NULL;
	}
	if (m_SellItem)
	{
		for (int i = 0; i < m_Height; i++)
		{
			if (m_SellItem[i])
			{
				delete [] m_SellItem[i];
				m_SellItem[i] = NULL;
			}
		}
		delete [] m_SellItem;
		m_SellItem = NULL;
	}
	if (m_CashGoods)
	{
		delete [] m_CashGoods;
		m_CashGoods = NULL;
	}
#ifdef _SERVER
	delete [] m_AutoBuyCurrency;
	m_AutoBuyCurrency = NULL;
	delete [] m_MallGoods;
	delete [] m_MallItem;
	m_MallGoods = NULL;
	m_MallItem = NULL;
	m_MallCount = 0;
#endif
	m_Width = 0;
	m_Height = 0;
	m_MaxItem = 0;
}

BOOL KBuySell::Init()
{
	KTabFile		GoodsFile;
	KTabFile		BuySellFile;

//	g_SetFilePath("\\");
	if (!BuySellFile.Load(BUYSELL_FILE) || !GoodsFile.Load(GOODS_FILE))
		return FALSE;

	int nHeight = GoodsFile.GetHeight() - 1;
	int nWidth = GoodsFile.GetWidth() - 1;
	if (nWidth == 0 || nHeight == 0)
		return FALSE;

	m_Item = (KItem *)new KItem[nHeight];
	if (!m_Item)
		return FALSE;

	m_CashGoods = new BOOL[nHeight];
	if (!m_CashGoods)
		return FALSE;

	ItemGenerate ItemGenerator;
	for (int k = 0; k < nHeight; k++)
	{
		GoodsFile.GetInteger(k + 2, 1, -1, &ItemGenerator.nGenre);
		GoodsFile.GetInteger(k + 2, 2, -1, &ItemGenerator.nDetailType);
		GoodsFile.GetInteger(k + 2, 3, -1, &ItemGenerator.nParticularType);
		GoodsFile.GetInteger(k + 2, 4, -1, &ItemGenerator.nSeriesReq);
		GoodsFile.GetInteger(k + 2, 5, -1, &ItemGenerator.nLevel);
		int nCount, nCredit, nDays, nPoints;
		GoodsFile.GetInteger(k + 2, 6, 1, &nCount);
		GoodsFile.GetInteger(k + 2, 7, 0, &nCredit);
		GoodsFile.GetInteger(k + 2, 8, 0, &nDays);
		GoodsFile.GetInteger(k + 2, 9, 0, &nPoints);
		// Only ordinary cash rows are handled here; retain every original slot index.
		m_CashGoods[k] = nCount >= 0 && nCount <= 1 && nCredit == 0 && nDays == 0 && nPoints == 0;
		switch(ItemGenerator.nGenre)
		{
		case item_equip:
			ItemGen.Gen_Equipment(ItemGenerator.nDetailType,
				ItemGenerator.nParticularType,
				ItemGenerator.nSeriesReq,
				ItemGenerator.nLevel,
				NULL,
				0,
				g_SubWorldSet.GetGameVersion(),
				&m_Item[k]);
			break;
		case item_medicine:
			ItemGen.Gen_Medicine(ItemGenerator.nDetailType, 
				ItemGenerator.nLevel, 
				g_SubWorldSet.GetGameVersion(),
				&m_Item[k]);
			break;
		case item_materials:
			// material.txt 无等级列，当前客户端材料的实际等级固定为 0。
			ItemGen.Gen_Material(ItemGenerator.nDetailType, ItemGenerator.nParticularType,
				0, g_SubWorldSet.GetGameVersion(), &m_Item[k]);
			break;
		case item_magicorscript:
		case item_skillbook:
			ItemGen.Gen_NormalItem(ItemGenerator.nGenre, ItemGenerator.nDetailType,
				ItemGenerator.nParticularType, ItemGenerator.nLevel,
				g_SubWorldSet.GetGameVersion(), &m_Item[k]);
			break;
		case item_townportal:
			ItemGen.Gen_TownPortal(&m_Item[k]);
			break;
		default:
			break;
		}
		if (!m_Item[k].GetName()[0] || m_Item[k].GetWidth() <= 0 || m_Item[k].GetHeight() <= 0)
		{
			g_DebugLog("[Shop] Goods template unavailable: goods=%d genre=%d detail=%d particular=%d level=%d",
				k + 1, ItemGenerator.nGenre, ItemGenerator.nDetailType,
				ItemGenerator.nParticularType, ItemGenerator.nLevel);
		}
		m_MaxItem++;
	}

	m_Height = BuySellFile.GetHeight() - 1;
	m_Width  = BuySellFile.GetWidth();
	
	if (m_Width == 0 || m_Height == 0)
		return FALSE;

	m_SellItem = (int **)new int*[m_Height];
	if (!m_SellItem)
		return FALSE;

	for (int i = 0; i < m_Height; i++)
	{
		m_SellItem[i] = NULL;
		m_SellItem[i] = (int *)new int[m_Width];
		if (!m_SellItem[i])
			return FALSE;
		for (int j = 0; j < m_Width; j++)
		{
			BuySellFile.GetInteger(i + 2, j + 1, -1, &m_SellItem[i][j]);
			if (m_SellItem[i][j] <= 0)
			{
				m_SellItem[i][j] = -1;
				continue;
			}
			_ASSERT(m_SellItem[i][j] > 0);		// 策划是从1开始的
			if (m_SellItem[i][j] > 0)
				m_SellItem[i][j] -= 1;			// 为了策划从1开始填表
		}
	}

#ifndef _SERVER
	if (!m_pShopRoom)
	{
		m_pShopRoom = new KInventory;
		m_pShopRoom->Init(SHOP_BOX_WIDTH, SHOP_BOX_HEIGHT);
	}
#endif
#ifdef _SERVER
	if (!InitMall())
		return FALSE;
	InitAutoMedicine();
#endif
	return TRUE;
}

KItem* KBuySell::GetItem(int nIndex)
{
	if (nIndex < 0 || nIndex >= m_MaxItem || !m_Item)
		return NULL;

	return &m_Item[nIndex];
}

int KBuySell::GetItemIndex(int nShop, int nIndex)
{
	if (!m_SellItem || nShop < 0 || nShop >= m_Height || nIndex < 0 || nIndex >= m_Width)
		return -1;

	if (!m_SellItem[nShop])
		return -1;

	return m_SellItem[nShop][nIndex];
}

#ifdef _SERVER
// 自动买药只开放原资源列出的商品；同时核对商店槽位和真实模板，避免旧表错配。
void KBuySell::InitAutoMedicine()
{
	delete [] m_AutoBuyCurrency;
	m_AutoBuyCurrency = new int[m_Height * m_Width];
	for (int i = 0; i < m_Height * m_Width; i++)
		m_AutoBuyCurrency[i] = -1;
	KTabFile Table;
	if (!Table.Load("\\settings\\autobuymedicine.txt"))
	{
		g_DebugLog("[AutoMedicine] Original resource unavailable; remote purchase disabled.");
		return;
	}
	int nLoaded = 0;
	// 第一行为字段名，第二行为中文说明，第三行起是实际数据。
	for (int nRow = 3; nRow <= Table.GetHeight(); nRow++)
	{
		int nGenre, nDetail, nParticular, nShop, nSlot, nCurrency;
		Table.GetInteger(nRow, "general", -1, &nGenre);
		Table.GetInteger(nRow, "detail", -1, &nDetail);
		Table.GetInteger(nRow, "particular", -1, &nParticular);
		Table.GetInteger(nRow, "shopId", -1, &nShop);
		Table.GetInteger(nRow, "itemBuyIdx", -1, &nSlot);
		Table.GetInteger(nRow, "cashType", -1, &nCurrency);
		int nIndex = GetItemIndex(nShop, nSlot);
		KItem* pItem = GetItem(nIndex);
		if (!pItem || !m_CashGoods[nIndex] || nCurrency < 0 || nCurrency > 1 ||
			!pItem->GetName()[0] || pItem->GetWidth() <= 0 || pItem->GetHeight() <= 0 ||
			pItem->GetGenre() != nGenre || pItem->GetDetailType() != nDetail ||
			pItem->GetParticular() != nParticular)
		{
			g_DebugLog("[AutoMedicine] Ignored unmatched resource row=%d shop=%d slot=%d", nRow, nShop, nSlot);
			continue;
		}
		int& nAllowed = m_AutoBuyCurrency[nShop * m_Width + nSlot];
		if (nAllowed == -1)
		{
			nAllowed = nCurrency;
			nLoaded++;
		}
		else if (nAllowed != nCurrency)
		{
			// 不同货币的冲突行保持禁用，不因后续行重新开放。
			nAllowed = -2;
			g_DebugLog("[AutoMedicine] Conflicting currency row=%d shop=%d slot=%d", nRow, nShop, nSlot);
		}
	}
	g_DebugLog("[AutoMedicine] Loaded %d verified original resource entries.", nLoaded);
}

static BOOL CheckBuyPlayer(int nPlayerIdx)
{
	if (nPlayerIdx <= 0 || nPlayerIdx >= MAX_PLAYER)
		return FALSE;
	KPlayer &Role = Player[nPlayerIdx];
	if (!Role.m_bFinishLoading || Role.CheckTrading() || Role.m_nIndex <= 0 || Role.m_nIndex >= MAX_NPC)
		return FALSE;
	KNpc &Hero = Npc[Role.m_nIndex];
	return Hero.m_CurrentLife > 0 && Hero.m_Doing != do_death && Hero.m_Doing != do_revive &&
		Hero.m_RegionIndex >= 0 && Hero.m_SubWorldIndex >= 0 && !Hero.m_bExchangeServer;
}

static BOOL CheckShopPlayer(int nPlayerIdx, int nShop)
{
	if (!CheckBuyPlayer(nPlayerIdx))
		return FALSE;
	KPlayer &Role = Player[nPlayerIdx];
	KNpc &Hero = Npc[Role.m_nIndex];
	if (nShop < 0 || nShop >= BuySell.GetHeight() || nShop != Role.m_BuyInfo.m_nBuyIdx ||
		Hero.m_SubWorldIndex != Role.m_BuyInfo.m_SubWorldID)
		return FALSE;
	int nX, nY;
	Hero.GetMpsPos(&nX, &nY);
	__int64 nDX = (__int64)nX - Role.m_BuyInfo.m_nMpsX;
	__int64 nDY = (__int64)nY - Role.m_BuyInfo.m_nMpsY;
	return nDX * nDX + nDY * nDY <= 384 * 384;
}

// 客户端单件修理必须来自当前普通商店；宠物仍使用 KPlayer 的通用修理接口。
BOOL KBuySell::Repair(int nPlayerIdx, DWORD dwItemID)
{
	if (!CheckBuyPlayer(nPlayerIdx))
		return FALSE;
	KPlayer &Role = Player[nPlayerIdx];
	if (Role.m_BuyInfo.m_nCurrency != 0 || !CheckShopPlayer(nPlayerIdx, Role.m_BuyInfo.m_nBuyIdx))
		return FALSE;
	int nItem = Role.m_ItemList.SearchID(dwItemID);
	if (nItem <= 0 || nItem >= MAX_ITEM)
		return FALSE;
	int nPlace = Role.m_ItemList.GetItemPlace(nItem);
	if (nPlace != pos_equip && nPlace != pos_equiproom)
		return FALSE;
	int nCost = 0;
	if (!Role.CheckRepairItem(dwItemID, &nCost))
		return FALSE;
	BOOL bResult = Role.RepairItem(dwItemID);
	g_DebugLog("[Repair] player=%d item=%lu shop=%d cost=%d result=%d",
		nPlayerIdx, dwItemID, Role.m_BuyInfo.m_nBuyIdx, nCost, bResult);
	return bResult;
}

static void ShopMessage(int nPlayerIdx, const char *pszMessage)
{
	PLAYER_SCRIPTACTION_SYNC Info;
	ZeroMemory(&Info, sizeof(Info));
	Info.m_bUIId = UI_MSGINFO;
	Info.m_bParam2 = 1;
	g_StrCpyLen(Info.m_pContent, pszMessage, sizeof(Info.m_pContent));
	Info.m_nBufferLen = strlen(Info.m_pContent);
	Player[nPlayerIdx].DoScriptAction(&Info);
}

static int ParseMallBits(const char* pszText, BOOL bTabs)
{
	int nBits = 0;
	while (*pszText)
	{
		while (*pszText == ' ' || *pszText == '\t')
			pszText++;
		if (!*pszText)
			break;
		char* pEnd;
		long nValue = strtol(pszText, &pEnd, 10);
		if (pEnd == pszText || (bTabs ? (nValue < 1 || nValue > 6) : (nValue != 1 && nValue != 2)))
			return -1;
		nBits |= bTabs ? 1 << (nValue - 1) : nValue;
		pszText = pEnd;
	}
	return nBits;
}

BOOL KBuySell::InitMall()
{
	KTabFile GoodsFile;
	if (!GoodsFile.Load("\\settings\\ibshop.txt"))
	{
		g_DebugLog("[Mall] Original settings/ibshop.txt is unavailable; mall disabled.");
		return TRUE;
	}
	int nRows = GoodsFile.GetHeight() - 2;
	if (nRows <= 0 || nRows > 1000)
		return FALSE;
	m_MallGoods = new GAME_MALL_GOODS[nRows];
	m_MallItem = new KItem[nRows];
	if (!m_MallGoods || !m_MallItem)
		return FALSE;
	m_MallCount = 0;
	for (int nRow = 3; nRow <= GoodsFile.GetHeight(); nRow++)
	{
		GAME_MALL_GOODS Goods;
		ZeroMemory(&Goods, sizeof(Goods));
		int nID, nStack;
		GoodsFile.GetInteger(nRow, "Id", 0, &nID);
		GoodsFile.GetInteger(nRow, "StackCount", 1, &nStack);
		char szTabs[64], szFlags[32], szSaleDay[64];
		GoodsFile.GetString(nRow, "ShopTab", "", szTabs, sizeof(szTabs));
		GoodsFile.GetString(nRow, "Flags", "", szFlags, sizeof(szFlags));
		GoodsFile.GetString(nRow, "SaleDay", "", szSaleDay, sizeof(szSaleDay));
		if (szSaleDay[0])
		{
			// 原表的限时行依赖开服天数，尚无可靠开服日期时不自行选择价格。
			g_DebugLog("[Mall] Timed goods withheld: id=%d SaleDay=%s (server opening date not configured)", nID, szSaleDay);
			continue;
		}
		int nTabs = ParseMallBits(szTabs, TRUE);
		int nFlags = ParseMallBits(szFlags, FALSE);
		if (nID <= 0 || nID > 65535 || nStack <= 0 || nStack > 255 || nTabs <= 0 || nFlags < 0)
			return FALSE;
		for (int i = 0; i < m_MallCount; i++)
		{
			if (m_MallGoods[i].GoodsID == nID)
				return FALSE;
		}
		Goods.GoodsID = (WORD)nID;
		Goods.StackCount = (BYTE)nStack;
		Goods.Tabs = (BYTE)nTabs;
		Goods.Flags = (BYTE)nFlags;
		GoodsFile.GetInteger(nRow, "Genre", -1, &Goods.Genre);
		GoodsFile.GetInteger(nRow, "Detail", -1, &Goods.Detail);
		GoodsFile.GetInteger(nRow, "Particular", -1, &Goods.Particular);
		GoodsFile.GetInteger(nRow, "Level", -1, &Goods.Level);
		const char* Prices[] = {"CopperOriginalPrice", "CopperDiscountPrice", "YuanBaoOriginalPrice",
			"YuanBaoDiscountPrice", "MoneyOriginalPrice", "MoneyDiscountPrice"};
		int* Values[] = {&Goods.CopperPrice, &Goods.CopperDiscount, &Goods.YuanBaoPrice,
			&Goods.YuanBaoDiscount, &Goods.MoneyPrice, &Goods.MoneyDiscount};
		for (int i = 0; i < 6; i++)
		{
			int nPrice;
			GoodsFile.GetInteger(nRow, (char*)Prices[i], 0, &nPrice);
			if (nPrice < 0 || nPrice > 0x7FFFFFFF / 100)
				return FALSE;
			*Values[i] = nPrice * 100;
		}
		KItem& Template = m_MallItem[m_MallCount];
		BOOL bGenerated = FALSE;
		if (Goods.Genre == item_magicorscript || Goods.Genre == item_skillbook)
		{
			bGenerated = ItemGen.Gen_NormalItem(Goods.Genre, Goods.Detail, Goods.Particular,
				Goods.Level, -1, &Template);
		}
		else if (Goods.Genre == item_materials)
		{
			bGenerated = ItemGen.Gen_Material(Goods.Detail, Goods.Particular, 0,
				-1, &Template);
		}
		else if (Goods.Genre == item_medicine)
		{
			bGenerated = ItemGen.Gen_Medicine(Goods.Detail, Goods.Level,
				-1, &Template);
		}
		if (!bGenerated || !Template.GetName()[0] || Template.GetWidth() <= 0 || Template.GetHeight() <= 0)
		{
			g_DebugLog("[Mall] Goods template unavailable: id=%d genre=%d detail=%d particular=%d level=%d",
				nID, Goods.Genre, Goods.Detail, Goods.Particular, Goods.Level);
			continue;
		}
		m_MallGoods[m_MallCount++] = Goods;
	}
	g_DebugLog("[Mall] Loaded %d goods from original settings/ibshop.txt", m_MallCount);
	return TRUE;
}

static BOOL CheckMallPlayer(int nPlayerIdx)
{
	if (nPlayerIdx <= 0 || nPlayerIdx >= MAX_PLAYER)
		return FALSE;
	KPlayer& Role = Player[nPlayerIdx];
	if (!Role.m_bFinishLoading || Role.CheckTrading() || Role.m_bIsQuiting ||
		Role.m_nIndex <= 0 || Role.m_nIndex >= MAX_NPC)
		return FALSE;
	KNpc& Hero = Npc[Role.m_nIndex];
	return Hero.m_CurrentLife > 0 && Hero.m_Doing != do_death && Hero.m_RegionIndex >= 0 &&
		!Hero.m_bExchangeServer && !Role.m_bExchangeServer;
}

BOOL KBuySell::OpenMall(int nPlayerIdx)
{
	if (!CheckMallPlayer(nPlayerIdx))
		return FALSE;
	Player[nPlayerIdx].m_bMallOpen = FALSE;
	if (!m_MallGoods || !m_MallCount)
	{
		ShopMessage(nPlayerIdx, "奇货商场暂未开放。");
		return FALSE;
	}
	// F8/55 operation 5 + DWORD byte count + the original 45-byte goods records.
	int nDataLength = m_MallCount * sizeof(GAME_MALL_GOODS);
	int nLength = 9 + nDataLength;
	BYTE* pBuffer = new BYTE[nLength];
	if (!pBuffer)
		return FALSE;
	ZeroMemory(pBuffer, 9);
	pBuffer[0] = 0xF8;
	WORD wLength = (WORD)(nLength - 1);
	memcpy(pBuffer + 1, &wLength, sizeof(wLength));
	pBuffer[3] = 0x55;
	pBuffer[4] = 5;
	memcpy(pBuffer + 5, &nDataLength, sizeof(nDataLength));
	memcpy(pBuffer + 9, m_MallGoods, nDataLength);
	HRESULT hResult = g_pServer->PackDataToClient(Player[nPlayerIdx].m_nNetConnectIdx, pBuffer, nLength);
	delete [] pBuffer;
	if (FAILED(hResult))
		return FALSE;
	Player[nPlayerIdx].m_bMallOpen = TRUE;
	g_DebugLog("[Mall] Open: player=%d goods=%d", nPlayerIdx, m_MallCount);
	return TRUE;
}

BOOL KBuySell::BuyMall(int nPlayerIdx, int nGoodsID, int nCount, int nCurrency)
{
	if (!CheckMallPlayer(nPlayerIdx) || !Player[nPlayerIdx].m_bMallOpen)
		return FALSE;
	if (nCurrency != 1 || nCount <= 0 || nCount > 65535)
		return FALSE;
	int nIndex = 0;
	while (nIndex < m_MallCount && m_MallGoods[nIndex].GoodsID != nGoodsID)
		nIndex++;
	if (nIndex == m_MallCount)
		return FALSE;
	const GAME_MALL_GOODS& Goods = m_MallGoods[nIndex];
	int nUnitPrice = Goods.CopperDiscount ? Goods.CopperDiscount : Goods.CopperPrice;
	__int64 nPrice = (__int64)nUnitPrice * nCount;
	__int64 nItems = (__int64)Goods.StackCount * nCount;
	if (nUnitPrice <= 0 || nPrice % 100 || nPrice / 100 > 0x7FFFFFFF ||
		nItems > (__int64)(MAX_PLAYER_ITEM - 1) * m_MallItem[nIndex].GetMaxStackCount())
		return FALSE;
	// 铜钱身份由原 questkey.txt 定义为 (genre=4, detail=47)，不是角色现金。
	BOOL bBought = Player[nPlayerIdx].m_ItemList.BuyWithItemCurrency(&m_MallItem[nIndex],
		(int)nItems, item_task, 47, (int)(nPrice / 100));
	if (bBought)
	{
		GAME_CURRENCY_NOTICE_SYNC Notice;
		ZeroMemory(&Notice, sizeof(Notice));
		Notice.ProtocolType = 0xCC;
		Notice.ChangeType = 0;
		Notice.Currency = 1;
		Notice.Amount = (DWORD)(nPrice / 100);
		g_pServer->PackDataToClient(Player[nPlayerIdx].m_nNetConnectIdx, &Notice, sizeof(Notice));
		char szMessage[160];
		sprintf(szMessage, "购买%d个%s成功", nCount, m_MallItem[nIndex].GetName());
		ShopMessage(nPlayerIdx, szMessage);
	}
	else
		ShopMessage(nPlayerIdx, "铜钱不足或背包空间不足，购买未完成。");
	g_DebugLog("[Mall] Buy: player=%d goods=%d count=%d items=%d copper=%d result=%d",
		nPlayerIdx, nGoodsID, nCount, (int)nItems, (int)(nPrice / 100), bBought);
	return bBought;
}

BOOL KBuySell::Buy(int nPlayerIdx, int nBuy, int nBuyIdx, int nPlace, int nX, int nY)
{
	if (!CheckBuyPlayer(nPlayerIdx))
		return FALSE;
	int nCurrency = -1;
	BOOL bAutoPlace = nPlace == 255 && nX == 255 && nY == 255;
	if (bAutoPlace && m_AutoBuyCurrency && nBuy >= 0 && nBuy < m_Height &&
		nBuyIdx >= 0 && nBuyIdx < m_Width)
		nCurrency = m_AutoBuyCurrency[nBuy * m_Width + nBuyIdx];
	if (nCurrency < 0)
	{
		if (!CheckShopPlayer(nPlayerIdx, nBuy))
			return FALSE;
		nCurrency = Player[nPlayerIdx].m_BuyInfo.m_nCurrency;
	}
	int nIdx = GetItemIndex(nBuy, nBuyIdx);
	KItem *pItem = GetItem(nIdx);
	if (!pItem || !m_CashGoods || !m_CashGoods[nIdx] ||
		pItem->GetWidth() <= 0 || pItem->GetHeight() <= 0 || !pItem->GetName()[0])
	{
		ShopMessage(nPlayerIdx, "该商品尚未适配，未扣除金钱。");
		return FALSE;
	}
	// Normal destination requests use the backpack. FF/FF/FF is the observed quick-buy form.
	if (nPlace == 255 && nX == 255 && nY == 255)
	{
		nPlace = pos_equiproom;
		nX = nY = -1;
	}
	if (nPlace != pos_equiproom && nPlace != pos_hand)
		return FALSE;
	KPlayer &Role = Player[nPlayerIdx];
	if (nPlace == pos_hand && (nX != 0 || nY != 0 || Role.m_ItemList.Hand() != 0))
		return FALSE;
	if (nCurrency == 1)
	{
		const KBASICPROP_NORMALITEM* pTemplate = ItemGen.GetNormalItemTemplate(pItem->GetGenre(),
			pItem->GetDetailType(), pItem->GetParticular(), pItem->GetLevel(),
			pItem->m_GeneratorParam.nVersion);
		if (!pTemplate || pTemplate->m_nCopperPrice <= 0 || nPlace != pos_equiproom)
			return FALSE;
		int nCost = pTemplate->m_nCopperPrice;
		BOOL bBought = Role.m_ItemList.BuyWithItemCurrency(pItem, 1, item_task, 47, nCost);
		if (bBought)
		{
			GAME_CURRENCY_NOTICE_SYNC Notice;
			ZeroMemory(&Notice, sizeof(Notice));
			Notice.ProtocolType = 0xCC;
			Notice.Currency = 1;
			Notice.Amount = nCost;
			g_pServer->PackDataToClient(Role.m_nNetConnectIdx, &Notice, sizeof(Notice));
			g_DebugLog("[Shop] Copper buy: player=%d shop=%d slot=%d cost=%d",
				nPlayerIdx, nBuy, nBuyIdx, nCost);
		}
		else
			ShopMessage(nPlayerIdx, "铜钱不足或背包空间不足，购买未完成。");
		return bBought;
	}
	int nPrice = pItem->GetPrice();
	if (nPrice < 0 || Role.m_ItemList.GetEquipmentMoney() < nPrice)
	{
		ShopMessage(nPlayerIdx, MSG_SHOP_NO_MONEY);
		return FALSE;
	}
	int nItemIdx = ItemSet.Add(pItem);
	if (!nItemIdx)
		return FALSE;
	Item[nItemIdx].SetStackCount(1);
	if (!Role.Pay(nPrice))
	{
		ItemSet.Remove(nItemIdx);
		return FALSE;
	}
	BOOL bAdded;
	if (nPlace == pos_hand)
		bAdded = Role.m_ItemList.Add(nItemIdx, pos_hand, 0, 0) != 0;
	else
		bAdded = Role.m_ItemList.AddPickupItem(nItemIdx, nX, nY);
	if (!bAdded)
	{
		ItemSet.Remove(nItemIdx);
		Role.Earn(nPrice);
		ShopMessage(nPlayerIdx, MSG_SHOP_NO_ROOM);
		return FALSE;
	}
	// AddPickupItem may have merged and released nItemIdx; use captured shop data only.
	g_DebugLog("[Shop] Buy: player=%d shop=%d slot=%d goods=%d price=%d",
		nPlayerIdx, nBuy, nBuyIdx, nIdx, nPrice);
	return TRUE;
}

/*******************************************************************************
参数 nIdx 指游戏里Item数组的编号
*******************************************************************************/
BOOL KBuySell::Sell(int nPlayerIdx, int nBuy, int nIdx)
{
	if (!CheckShopPlayer(nPlayerIdx, nBuy) || Player[nPlayerIdx].m_BuyInfo.m_nCurrency != 0)
		return FALSE;
	KItemList& List = Player[nPlayerIdx].m_ItemList;
	PlayerItem* pEntry = List.GetFirstItem();
	while (pEntry && pEntry->nIdx && pEntry->nIdx != nIdx)
		pEntry = List.GetNextItem();
	// The duplicate request finds the original item in the buyback container.
	if (nIdx <= 0 || nIdx >= MAX_ITEM || !pEntry || pEntry->nIdx != nIdx ||
		(pEntry->nPlace != pos_hand && pEntry->nPlace != pos_equiproom) ||
		Item[nIdx].GetGenre() == item_task || Item[nIdx].GetPrice() <= 0)
		return FALSE;

	// Official single-item sales use price / 4; stack multiplication follows the reference code.
	__int64 nPrice = Item[nIdx].GetPrice() / BUY_SELL_SCALE;
	if (nPrice < 1)
		nPrice = 1;
	nPrice *= Item[nIdx].GetStackCount();
	if (nPrice > 0x7FFFFFFF || (__int64)List.GetEquipmentMoney() + nPrice > 0x7FFFFFFF)
		return FALSE;

	int BuyBackItems[MAX_SHOP_BUYBACK_ITEM];
	int nCount = List.GetBuyBackItems(BuyBackItems);
	if (nCount == MAX_SHOP_BUYBACK_ITEM)
	{
		// Official buyback capacity is two columns by seven rows, oldest first.
		int nOldest = BuyBackItems[0];
		if (!List.Remove(nOldest))
			return FALSE;
		ItemSet.Remove(nOldest);
		for (int i = 1; i < nCount; i++)
			List.MoveShopItem(BuyBackItems[i], pos_buyback, (i - 1) % 2, (i - 1) / 2);
		nCount--;
	}
	if (!List.MoveShopItem(nIdx, pos_buyback, nCount % 2, nCount / 2))
		return FALSE;
	Player[nPlayerIdx].Earn((int)nPrice);
	g_DebugLog("[Shop] Sell: player=%d shop=%d item=%lu count=%d price=%d",
		nPlayerIdx, nBuy, Item[nIdx].GetID(), Item[nIdx].GetStackCount(), (int)nPrice);
	return TRUE;
}

BOOL KBuySell::BuyBack(int nPlayerIdx, DWORD dwItemID)
{
	if (nPlayerIdx <= 0 || nPlayerIdx >= MAX_PLAYER ||
		!CheckShopPlayer(nPlayerIdx, Player[nPlayerIdx].m_BuyInfo.m_nBuyIdx) ||
		Player[nPlayerIdx].m_BuyInfo.m_nCurrency != 0)
		return FALSE;
	KItemList& List = Player[nPlayerIdx].m_ItemList;
	int BuyBackItems[MAX_SHOP_BUYBACK_ITEM];
	int nCount = List.GetBuyBackItems(BuyBackItems);
	int nSlot = 0;
	while (nSlot < nCount && Item[BuyBackItems[nSlot]].GetID() != dwItemID)
		nSlot++;
	if (nSlot >= nCount)
		return FALSE;
	int nIdx = BuyBackItems[nSlot];
	__int64 nPrice = (__int64)Item[nIdx].GetPrice() * Item[nIdx].GetStackCount();
	if (nPrice <= 0 || nPrice > List.GetEquipmentMoney())
	{
		ShopMessage(nPlayerIdx, MSG_SHOP_NO_MONEY);
		return FALSE;
	}
	int nX, nY;
	if (!List.CheckCanPlaceInEquipment(Item[nIdx].GetWidth(), Item[nIdx].GetHeight(), &nX, &nY))
	{
		ShopMessage(nPlayerIdx, MSG_SHOP_NO_ROOM);
		return FALSE;
	}
	// Move the original item without regenerating its ID, attributes, durability or count.
	if (!List.MoveShopItem(nIdx, pos_equiproom, nX, nY, FALSE))
		return FALSE;
	Player[nPlayerIdx].Pay((int)nPrice);
	List.SyncItemPosition(nIdx);
	for (int i = nSlot + 1; i < nCount; i++)
		List.MoveShopItem(BuyBackItems[i], pos_buyback, (i - 1) % 2, (i - 1) / 2);
	g_DebugLog("[Shop] BuyBack: player=%d item=%lu count=%d price=%d",
		nPlayerIdx, dwItemID, Item[nIdx].GetStackCount(), (int)nPrice);
	return TRUE;
}
#endif

#ifndef _SERVER
void KBuySell::PaintItem(int nIdx, int nX, int nY)
{
	int nShop = Player[CLIENT_PLAYER_INDEX].m_BuyInfo.m_nBuyIdx;
	if (nShop < 0 || nShop >= m_Height)
		return;
	int nItemIdx = GetItemIndex(nShop, nIdx);

	int x = nX;
	int y = nY;

	KItem* pItem = GetItem(nItemIdx);

	if (pItem)
	{
		pItem->Paint(x, y);
	}
}

void KBuySell::OpenSale(int nShop)
{
	if (nShop < 0 || nShop >= m_Height)
		return;
	Player[CLIENT_PLAYER_INDEX].m_BuyInfo.m_nBuyIdx = nShop;
	CoreDataChanged(GDCNI_NPC_TRADE, NULL, TRUE);
}
#endif

#ifdef _SERVER
void KBuySell::OpenSale(int nPlayerIdx, int nShop, int nCurrency)
{
	if (nPlayerIdx <= 0 || nPlayerIdx >= MAX_PLAYER || nShop < 0 || nShop >= m_Height ||
		(nCurrency != 0 && nCurrency != 1))
	{
		return;
	}

	if (!Player[nPlayerIdx].m_bFinishLoading || Player[nPlayerIdx].CheckTrading() ||
		Player[nPlayerIdx].m_nIndex <= 0 || Player[nPlayerIdx].m_nIndex >= MAX_NPC)
		return;
	Player[nPlayerIdx].m_BuyInfo.m_nBuyIdx = nShop;
	Player[nPlayerIdx].m_BuyInfo.m_nCurrency = nCurrency;
	Player[nPlayerIdx].m_BuyInfo.m_SubWorldID = Npc[Player[nPlayerIdx].m_nIndex].m_SubWorldIndex;
	Npc[Player[nPlayerIdx].m_nIndex].GetMpsPos(
		&Player[nPlayerIdx].m_BuyInfo.m_nMpsX,
		&Player[nPlayerIdx].m_BuyInfo.m_nMpsY);

	SALE_BOX_SYNC saleSync;
	saleSync.ProtocolType = 0x8F;
	saleSync.ShopType = (BYTE)nCurrency;
	saleSync.nShopIndex = nShop;
	g_pServer->PackDataToClient(Player[nPlayerIdx].m_nNetConnectIdx, &saleSync, sizeof(SALE_BOX_SYNC));
}
#endif
