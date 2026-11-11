-- 背包和仓库永久扩展；费用来源：serverlist.pak/00587-29b2371f.bin。
-- 仓库菜单/确认流程来源：00596-2a5f65da.bin；原脚本旧价格已按当前直扣表校正。
-- BagCosts 对应原表 ID30～36；StoreCosts 对应 ID1/2/3/72/73/74。
-- 窗口上下文由C++建立，Lua只使用当前角色的服务端选择；禁止Lua直接操作数据库。
BagCosts = {10,20,30,40,80,120,160}
StoreCosts = {10,20,40,80,80,80}

function GetCost(kind, index)
	if kind == 1 and index >= 0 and index <= 6 then
		return BagCosts[index + 1]
	elseif kind == 2 and index >= 0 and index <= 5 then
		return StoreCosts[index + 1]
	end
	return 0
end

function main()
	local kind, index = GetInventoryPurchase()
	local cost = GetCost(kind, index)
	if cost <= 0 then return end
	if kind == 1 then
		if GetBagRowState(index) == 1 then
			Talk(1,"no","您已经永久开启了此行背包格。")
			return
		end
		if index > 0 and GetBagRowState(index - 1) == 0 then
			Talk(1,"no","背包格只能逐行依次开启，请先购买前一行。")
			return
		end
		MsgBox(format("您好！此页提供背包栏扩展功能，只能逐行(1*5格)依次开启，开启第<c=g>%d行<c>，新行被开启后，您将获得此行的<c=r>永久<c>使用权，系统会自动扣除您身上携带的<c=g>%d<c>铜钱，您想开启此行吗？",index+1,cost),"Buy_Confirm","no")
	else
		if GetStoreBoxRentTime(index) >= 31536000 then
			Talk(1,"no","您已经<c=yel>永久购买<c>了此页仓库，不需要再为此支付费用了！")
			return
		end
		SayTask("是否花费"..cost.."铜钱开启此页仓库的永久使用权？",{{"确定购买","Buy_ByTongBao";show=1}})
	end
end

function Buy_ByTongBao()
	no()
	local kind, index = GetInventoryPurchase()
	local cost = GetCost(kind, index)
	if kind == 2 and cost > 0 then
		MsgBox("您确定要花费"..cost.."个铜钱永久购买此页仓库吗？","Buy_Confirm","no")
	end
end

function Buy_Confirm()
	no()
	local kind, index = GetInventoryPurchase()
	local cost = GetCost(kind, index)
	if cost <= 0 then return end
	if kind == 1 then
		if GetBagRowState(index) == 1 then return end
		if index > 0 and GetBagRowState(index - 1) == 0 then return end
	elseif GetStoreBoxRentTime(index) >= 31536000 then
		return
	end
	if GetCopperCashCount() < cost then
		Talk(1,"no","对不起，你的铜钱数量不够负担购买费用，请先确定是否携带了足够的铜钱。")
		return
	end
	assert(WasteCopperCash(cost) == 1)
	if kind == 1 then
		assert(ExtendBag(index) == 1)
		Msg2Player(format("成功开启了第%d行背包格",index+1))
	else
		assert(RentStoreBox(index,31536000,cost) == 1)
		Talk(1,"no","你已经成功购买了这页仓库！")
	end
	WriteLog(format("永久扩展：类型%d，序号%d，扣除%d铜钱。",kind,index,cost))
end

function no()
	CloseDialog()
end
