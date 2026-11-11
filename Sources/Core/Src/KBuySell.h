#ifndef	KBuySellH
#define	KBuySellH

#define	BUY_SELL_SCALE		4

class KItem;

typedef struct
{
	int		nGenre;
	int		nDetailType;
	int		nParticularType;
	int		nSeriesReq;
	int		nLevel;
	int		nPrice;
} ItemGenerate;

class KInventory;
struct GAME_MALL_GOODS;

class KBuySell
{
public:
	KBuySell();
	~KBuySell();
	BOOL			Init();
	int				GetWidth() { return m_Width; }
	int				GetHeight() { return m_Height; }
	KItem*			GetItem(int nIndex);
	int				GetItemIndex(int nShop, int nIndex);
private:
	int**			m_SellItem;
	KItem*			m_Item;
	int				m_Width;
	int				m_Height;
	int				m_MaxItem;
	BOOL*			m_CashGoods;
#ifdef _SERVER
	GAME_MALL_GOODS*	m_MallGoods;
	KItem*			m_MallItem;
	int				m_MallCount;
	BOOL			InitMall();
	int*			m_AutoBuyCurrency;
	void			InitAutoMedicine();
#endif
public:
#ifdef _SERVER
	void			OpenSale(int nPlayerIdx, int nShop, int nCurrency = 0);
	BOOL			Buy(int nPlayerIdx, int nBuy, int nBuyIdx, int nPlace, int nX, int nY);	// 买第几个买卖列表中的第几项道具
	BOOL			Sell(int nPlayerIdx, int nBuy, int nIdx);
	BOOL			BuyBack(int nPlayerIdx, DWORD dwItemID);
	BOOL			Repair(int nPlayerIdx, DWORD dwItemID);
	BOOL			OpenMall(int nPlayerIdx);
	BOOL			BuyMall(int nPlayerIdx, int nGoodsID, int nCount, int nCurrency);
#endif
#ifndef _SERVER
	KInventory*		m_pShopRoom;
	void			OpenSale(int nShop);
	void			PaintItem(int nIdx, int nX, int nY);
#endif
};

extern KBuySell	BuySell;
#endif
