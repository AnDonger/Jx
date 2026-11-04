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
static BOOL CheckShopPlayer(int nPlayerIdx, int nShop)
{
	if (nPlayerIdx <= 0 || nPlayerIdx >= MAX_PLAYER)
		return FALSE;
	KPlayer &Role = Player[nPlayerIdx];
	if (!Role.m_bFinishLoading || Role.CheckTrading() || nShop < 0 || nShop >= BuySell.GetHeight() ||
		nShop != Role.m_BuyInfo.m_nBuyIdx || Role.m_nIndex <= 0 || Role.m_nIndex >= MAX_NPC)
		return FALSE;
	KNpc &Hero = Npc[Role.m_nIndex];
	if (Hero.m_CurrentLife <= 0 || Hero.m_Doing == do_death || Hero.m_RegionIndex < 0 ||
		Hero.m_bExchangeServer || Hero.m_SubWorldIndex != Role.m_BuyInfo.m_SubWorldID)
		return FALSE;
	int nX, nY;
	Hero.GetMpsPos(&nX, &nY);
	__int64 nDX = (__int64)nX - Role.m_BuyInfo.m_nMpsX;
	__int64 nDY = (__int64)nY - Role.m_BuyInfo.m_nMpsY;
	return nDX * nDX + nDY * nDY <= 384 * 384;
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

BOOL KBuySell::Buy(int nPlayerIdx, int nBuy, int nBuyIdx, int nPlace, int nX, int nY)
{
	if (!CheckShopPlayer(nPlayerIdx, nBuy))
		return FALSE;
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
	if (!CheckShopPlayer(nPlayerIdx, nBuy))
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
		!CheckShopPlayer(nPlayerIdx, Player[nPlayerIdx].m_BuyInfo.m_nBuyIdx))
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
void KBuySell::OpenSale(int nPlayerIdx, int nShop)
{
	if (nPlayerIdx <= 0 || nPlayerIdx >= MAX_PLAYER || nShop < 0 || nShop >= m_Height)
	{
		return;
	}

	if (!Player[nPlayerIdx].m_bFinishLoading || Player[nPlayerIdx].CheckTrading() ||
		Player[nPlayerIdx].m_nIndex <= 0 || Player[nPlayerIdx].m_nIndex >= MAX_NPC)
		return;
	Player[nPlayerIdx].m_BuyInfo.m_nBuyIdx = nShop;
	Player[nPlayerIdx].m_BuyInfo.m_SubWorldID = Npc[Player[nPlayerIdx].m_nIndex].m_SubWorldIndex;
	Npc[Player[nPlayerIdx].m_nIndex].GetMpsPos(
		&Player[nPlayerIdx].m_BuyInfo.m_nMpsX,
		&Player[nPlayerIdx].m_BuyInfo.m_nMpsY);

	SALE_BOX_SYNC saleSync;
	saleSync.ProtocolType = 0x8F;
	saleSync.ShopType = 0;
	saleSync.nShopIndex = nShop;
	g_pServer->PackDataToClient(Player[nPlayerIdx].m_nNetConnectIdx, &saleSync, sizeof(SALE_BOX_SYNC));
}
#endif
