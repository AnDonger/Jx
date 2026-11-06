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

-- 合成会话、距离、物品保护检查统一在原服务端玩家模块处理。
function dz_new()
	CloseDialog()
	OpenNewCompound()
end

function dz()
	CloseDialog()
	EnchaseItem()
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