--description: 赤松子
--author: yichuan
--date: 2004/6/10
--modify:liuying 2005/4/5(加入了他山之石的交换任务)

function main(sel)
			tasks = 
			{
				{"合成物品(新)","dz_new";show=1},
				{"合成物品","dz";show=1},
				{"他山之石","tszs";show=1},
				{"合成宝典","book";show=1},
				{"上古之魂合成","ancient_soul";show=1},
				{"永久绑定说明","binding_info";show=1}
			}
			SayTask(10499,tasks)
end;

function no()
		CloseDialog()
end;

-- 会话和距离由通用窗口接口核实，物品保护和合成规则由本脚本检查。
function dz_new()
	if compound_can_start() ~= 1 then
		return
	end
	CloseDialog()
	OpenNewCompound("OnCompound")
end

function dz()
	if compound_can_start() ~= 1 then
		return
	end
	CloseDialog()
	EnchaseItem("OnCompound")
end

function book()
	SayTask("赤松子：你想了解哪一种合成？", {
		{"玉石合成", "yshc";show=1},
		{"宝石合成", "bshc";show=1},
		{"返回", "main";show=1}
	})
end

function ancient_soul()
	Talk(1,"main","赤松子：上古之魂合成尚未适配，请先使用已开放的材料合成。")
end

function binding_info()
	Talk(1,"main","赤松子：永久绑定说明正在核对原始规则，本入口尚未适配。")
end

function  tszs()		--声望换他山之石
	MsgBox(11256,"change","no")	
end;


function   yshc()
		CloseDialog()
		Talk(4,"main",11488,11489,11490,11491)
end;

function   bshc()
		CloseDialog()
		Talk(2,"bshc2",11492,11493)
end;

function  bshc2()
		CloseDialog()
		Talk(1,"main", 11534)
end
function  change()
	if(GetCredit()>=10)then
		DecCredit(10)
		AddNormalItem(3,82,0,1,0,0)
		MsgBox(11257,"no")
	else
		MsgBox(11258,"no")
	end;
end;
-- 材料合成使用原资源表，不在脚本中另列配方、费用或精石概率。
-- 新旧窗口都回调OnCompound；回调未完成或接口报错，由服务端事务整次恢复。

function compound_integer(value, default)
	if value == nil or value == "" then
		return default
	end
	local number = tonumber(value)
	if number == nil or number < 0 or number > 2147483647 or number ~= floor(number) then
		return nil
	end
	return number
end

function compound_descriptor(text)
	if text == nil then
		return nil
	end
	local first, last, genre, detail, particular, count = strfind(text, "^(%d+)%.(%d+)%.(%d+)%.(%d+)$")
	if first == nil then
		return nil
	end
	genre = compound_integer(genre)
	detail = compound_integer(detail)
	particular = compound_integer(particular)
	count = compound_integer(count)
	-- 带品质[]、等级掩码()的装备配方另有流程，不能混作材料合成。
	if (genre ~= 3 and genre ~= 6) or detail == nil or particular == nil or
		count == nil or count < 1 or count > 100000 then
		return nil
	end
	return {Genre=genre, Detail=detail, Particular=particular, Count=count}
end

function compound_load_rules()
	local version = GetGameVersion()
	if compound_rules ~= nil and compound_version == version then
		return compound_rules
	end
	local base = format("\\settings\\item\\%03d\\", version)
	local source = ReadTabFile(base.."装备合成规则表.txt")
	local money = ReadTabFile(base.."装备合成金钱需求.txt")
	if source == nil or money == nil or getn(source) ~= getn(money) then
		WriteLog("赤松子：原始合成规则或费用表加载失败。")
		return nil
	end
	local rules = {}
	for row = 2, getn(source) do
		local data = source[row]
		local skill = compound_integer(data[15])
		if tonumber(data[16]) == 6 and skill ~= nil and skill <= 1 and tonumber(data[18]) == -1 then
			local rule = {}
			rule.ID = compound_integer(data[1])
			rule.SourceCount = compound_integer(data[3])
			rule.Rate = compound_integer(data[17])
			rule.Money = compound_integer(money[row][2], 0)
			rule.Target = compound_descriptor(data[14])
			local valid = rule.ID ~= nil and rule.ID > 0 and tonumber(money[row][1]) == rule.ID and
				rule.SourceCount ~= nil and rule.SourceCount >= 1 and rule.SourceCount <= 9 and
				rule.Rate ~= nil and rule.Rate <= 100 and rule.Money ~= nil and rule.Target ~= nil
			if valid then
				rule.Sources = {}
				for column = 1, 10 do
					local text = data[3 + column]
					if column <= rule.SourceCount then
						rule.Sources[column] = compound_descriptor(text)
						if rule.Sources[column] == nil then
							valid = nil
						end
					elseif text ~= nil and text ~= "" then
						valid = nil
					end
				end
				if valid then
					rules[getn(rules) + 1] = rule
				end
			end
		end
	end
	if getn(rules) == 0 then
		return nil
	end
	compound_rules = rules
	compound_version = version
	WriteLog(format("赤松子：加载原始材料合成配方%d条，版本%d。", getn(rules), version))
	return rules
end

function compound_match(rule, items)
	local consume = {}
	for index = 1, getn(items) do
		consume[index] = 0
	end
	for index = 1, rule.SourceCount do
		local source = rule.Sources[index]
		local left = source.Count
		for slot = 1, getn(items) do
			local item = items[slot]
			if left > 0 and item.Genre == source.Genre and item.Detail == source.Detail and
				item.Particular == source.Particular and item.Level == 0 then
				local take = min(item.Count - consume[slot], left)
				consume[slot] = consume[slot] + take
				left = left - take
			end
		end
		if left > 0 then
			return nil
		end
	end
	-- 每个提交槽都必须参与本次配方，不忽略多塞的其他物品。
	for slot = 1, getn(items) do
		if consume[slot] <= 0 then
			return nil
		end
	end
	return consume
end

function compound_can_start()
	-- 原玲珑锁脚本使用任务464保存设置，解锁接口尚未接通时保留保护。
	if GetTask(464) ~= 0 then
		Msg2Player("请先检查玲珑锁保护状态，再进行合成。")
		return 0
	end
	return 1
end

function OnCompound()
	if compound_can_start() ~= 1 then
		return
	end
	local items = GetSelectedItems()
	local rules = compound_load_rules()
	if items == nil or rules == nil then
		return
	end
	local rule = nil
	local consume = nil
	for index = 1, getn(rules) do
		consume = compound_match(rules[index], items)
		if consume ~= nil then
			rule = rules[index]
			break
		end
	end
	if rule == nil or GetCash() < rule.Money then
		return
	end
	local bound = 0
	local binding_mode = 0
	for slot = 1, getn(items) do
		if items[slot].Bound == 1 then
			bound = 1
			binding_mode = max(binding_mode, items[slot].BindingMode)
		end
	end
	local success = random(1, 100) <= rule.Rate
	for slot = 1, getn(items) do
		if DelSelectedItem(slot, consume[slot]) ~= 1 then
			return
		end
	end
	if rule.Money > 0 and Pay(rule.Money) ~= 1 then
		return
	end
	local result = 0
	if success then
		local target = rule.Target
		if AddNormalItem(target.Genre, target.Detail, target.Particular, 0, 0, 0,
			{Count=target.Count, Bound=bound, BindingMode=binding_mode}) ~= 1 then
			return
		end
		result = 1
	end
	WriteLog(format("赤松子合成：配方%d，费用%d，成功率%d，结果%d。", rule.ID, rule.Money, rule.Rate, result))
	CompleteItemOperation(result)
end
