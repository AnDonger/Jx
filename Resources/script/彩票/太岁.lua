--description: 太岁之轮——太岁之轮
--author:chensong	
--date: 2004/7/14

--764,记录当天转动次数
Roulette_key = 1281 -- 转太岁的调用finish开关
costIb_key = 543 -- 记录使用道具的有效次数,非零代表使用道具后,未使用的次数

function main()
	-- 已付费但断线的转盘，恢复同一次结果，不再扣费或随机。
	if GetTask(Roulette_key) == 1 then
		Roulette(GetTask(72))
		return
	end
	local UTask_cp_2=GetTask(72)
	tasks = 
			{
			 {"转太岁","yes_2";show=0},
			 {"了解太岁","yes_1";show=1},
			 {"替身纸人","zhiren";show=1},
			 {"替身木人","muren";show=1},
			 {"活动说明","huodong";show=0}
			}	
	if(GetLevel()>=40) and (GetMorphType()==-1) then
		tasks[1].show=1
	end;
	SayTask(11079,tasks)	
end;

function zhiren()
	Talk(1,"zhiren_2","太岁师：太岁之轮是上古神器，转动一次需要消耗自身的原神，一天以后才能恢复。但是我有一个办法可以让你一天多次转动太岁之轮，你想知道吗?")
end

function zhiren_2()
	Talk(1,"zhiren_3","太岁师：其实在转动太岁之轮的时候，巨大的能量需要一个身体来承受，但是这个身体不一定是你的本体，有一种叫做<color=green>替身纸人<color>的东西，可以帮你承受那种巨大的力量。")
end

function zhiren_3()
	Talk(1,"no","太岁师：听说最近在瑶池的<color=red>福星<color>那有一些<color=green>替身纸人<color>，如果大家有需要请到那去看看！")
end

function muren()
	Talk(1,"no","太岁师：我听说瑶池的<color=red>福星<color>那里的新来了一种叫做<color=green>替身木人<color>的神秘物品，使用它可以让你能转动每天的第4-6次太岁之轮，并且你获得的奖励将会更丰厚，快去看看吧！")	
end

function yes_1()
	Talk(2,"no","太岁师：太岁之轮分为十二部分，分别代表天机、巨门、天相、武曲、天同、紫微、廉贞、贪狼、太阴、太阳、禄存、破军！","太岁师：太岁之轮是上古战争遗留下来的神器，通过它能知道一个人的未来。当你等级达到40级以后,你就可以试着转动它看看你的运势变化！")
end;

function yes_2()
	MsgBox(11082,"yes_3","no")
end;

function yes_3()
	if GetTask(Roulette_key) == 1 then
		Roulette(GetTask(72))
		return
	end
	if GetLevel() < 40 or GetMorphType() ~= -1 then
		Talk(1,"no","太岁师：等级达到40级、且没有附体效果时，才可以转动太岁之轮。")
		return
	end
	local LastBuyTime = GetTask(73) -- 郔缀珨棒划枪奀洁
	local NowBuyTime = SystemTime()    -- 珋婓奀洁
	local nRollCount = GetTask(764)
	if (GetTask(800)~=0) then
		Talk(1,"no","太岁师：你在结婚状态下不可以转动太岁之轮。")
		Msg2Player("在结婚状态下不可以转动太岁之轮。")	
	elseif(ceil(LastBuyTime / 86400) == ceil(NowBuyTime / 86400)) then-- 枪彻剩,揭烩枑尨
		if(nRollCount<=2) then
			if (GetTask(costIb_key) > 0) then
				rolling()
			elseif (HaveNormalItem(6,1,232,1)~=0) then	--cishu<=2&have(zhiren)
				SetTask(costIb_key, 1)
				DelNormalItem(6,1,232,1)
				Msg2Player("替身纸人代替你承受了太岁之轮的力量。")
				rolling()
			elseif(GetCopperCashCount() >= 1)then
				SetTask(costIb_key, 2)
				WasteCopperCash(1)
				Msg2Player("一个铜钱可以承受两次太岁之轮的力量。")
				rolling()
			else
				Talk(1,"no",11083)
			end
		elseif(nRollCount >= 3 and nRollCount <= 5)then
			if (HaveNormalItem(6,1,407,1)~=0) then	--木人
				SetTask(costIb_key, 1)
				DelNormalItem(6,1,407,1)
				Msg2Player("替身木人代替你承受了太岁之轮的力量。")
				rolling()
			elseif(GetCopperCashCount() >= 3)then
				SetTask(costIb_key, 1)
				WasteCopperCash(3)
				Msg2Player("三个铜钱可以承受1次太岁之轮的力量。")
				rolling()
			else
				Talk(1,"no","再次转动太岁之轮需要一个替身木人或者三个铜钱。")
			end
		elseif(nRollCount>5)then
			Talk(1,"no","太岁师：虽然你用木人代替你承受了大部分转动太岁之轮的力量，但是因为你转动太岁之轮的次数过多，你的体质还是不足以承受再一次的冲击，你下次再来吧！")
		else
			Talk(1,"no",11083)
		end
		SetTask(825,1)
	else
		SetTask(764,0)
		SetTask(costIb_key,0)
		rolling()
		SetTask(825,2)
	end
end

function rolling()
	local UTask_cp_2=GetTask(72)
	local buytime=GetTask(73)
	local LastBuyTime = GetTask(73) -- 郔缀珨棒划枪奀洁
	local NowBuyTime = SystemTime()    -- 珋婓奀洁
	local t = random(1,120)  -- 汜伧笢蒋疡
	local k = 0
	if(t<=10)then
		k=0
	elseif(t<=20)then
		k=1
	elseif(t<=30)then
		k=2
	elseif(t<=40)then
		k=3
	elseif(t<=50)then
		k=4
	elseif(t<=60)then
		k=5
	elseif(t<=70)then
		k=6	
	elseif(t<=80)then
		k=7
	elseif(t<=90)then
		k=8
	elseif(t<=100)then
		k=9
	elseif(t<=110)then
		k=10
	else
		k=11
	end
	CloseDialog()
	SetTask(73,NowBuyTime) -- 伧髡奀暮翘狟郔缀珨棒划枪奀洁
	SetTask(72,k)
	SetTask(764,GetTask(764)+1)
	Roulette(k) -- 谛誧伤湖羲赐酦	
	SetTask(Roulette_key, 1)
end;

function Finished() -- 谛誧伤绂钎俇救
	if (GetTask(Roulette_key) ~= 1) then
		CloseDialog()
		return 0
	end
	SetTask(Roulette_key, 0) 
	local nRollCount = GetTask(764)
	if (nRollCount > 1) then
		SetTask(costIb_key, max(0,GetTask(costIb_key)-1))
	end
	local UTask_cp_2=GetTask(72)	
	local NowTime=SystemTime()
	local UTask_cp_time=GetTask(75)
	local NowTime=SystemTime()
	local Name=GetName()
	local UTask_jingyan=GetTask(8)
	if (UTask_cp_2==0) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			AddRepute(10)
			Talk(1,"no","太岁师：你转动到的是<color=green>天机<color>，你将获得10点声望值，请你好好加以利用。")
			Msg2Player("你在转动太岁之轮的时候，意外地获得10的声望值。")
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			AddRepute(20)
			Talk(1,"no","太岁师：你转动到的是<color=green>天机<color>，你将获得20点声望值，请你好好加以利用。")
			Msg2Player("你在转动太岁之轮的时候，意外地获得20的声望值。")
		end
	elseif (UTask_cp_2==1) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			AddRepute(100)
			Talk(1,"no","太岁师：你转动到的是<color=green>巨门<color>，你将获得100点声望值，请你好好加以利用。")
			Msg2Player("你在转动太岁之轮的时候，意外地获得100的声望值。")
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			AddRepute(200)
			Talk(1,"no","太岁师：你转动到的是<color=green>巨门<color>，你将获得200点声望值，请你好好加以利用。")
			Msg2Player("你在转动太岁之轮的时候，意外地获得200的声望值。")
		end
	elseif (UTask_cp_2==2) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			Earn(10000)
			Talk(1,"no","太岁师：你转动到的是<color=green>天相<color>，你将一次性获得10000金钱。")
			Msg2Player("你在转动太岁之轮的时候，意外地得到10000金钱。")
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			Earn(20000)
			Talk(1,"no","太岁师：你转动到的是<color=green>天相<color>，你将一次性获得20000金钱。")
			Msg2Player("你在转动太岁之轮的时候，意外地得到20000金钱。")
		end
	elseif (UTask_cp_2==3) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			Earn(100000)
			Talk(1,"no","太岁师：你转动到的是<color=green>武曲<color>，你将一次性获得100000金钱。")
			Msg2Player("你在转动太岁之轮的时候，意外地得到100000金钱。")
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			Earn(200000)
			Talk(1,"no","太岁师：你转动到的是<color=green>武曲<color>，你将一次性获得200000金钱。")
			Msg2Player("你在转动太岁之轮的时候，意外地得到200000金钱。")
		end
	elseif (UTask_cp_2==4) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			Talk(1,"no","太岁师：你转动到的是<color=green>天同<color>，你将获得在未来1小时内经验值2倍的状态。请你好好加以利用。")
			AddBuff(5,3600,2,0,1)
			Msg2Player("你在转动太岁之轮的时候，意外地获得在未来1小时内经验值翻倍的状态。")
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			Talk(1,"no","太岁师：你转动到的是<color=green>天同<color>，你将获得在未来1小时内经验值和技能熟练度获取速度2倍的状态。请你好好加以利用。")
			AddBuff(5,3600,2,0,1)
			AddBuff(12,3600,100,0,1)
			Msg2Player("你在转动太岁之轮的时候，意外地获得在未来1小时内经验值和技能熟练度获取加倍的状态。")
		end
	elseif (UTask_cp_2==5) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			Talk(1,"no","太岁师：你转动到的是<color=green>紫微<color>，你将获得在未来2小时内经验值2倍的状态。请你好好加以利用。")
			AddBuff(5,7200,2,0,1)
			Msg2Player("你在转动太岁之轮的时候，意外地获得在未来2小时内经验值翻倍的状态。")
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			Talk(1,"no","太岁师：你转动到的是<color=green>天同<color>，你将获得在未来2小时内经验值和技能熟练度获取速度2倍的状态。请你好好加以利用。")
			AddBuff(5,7200,2,0,1)
			AddBuff(12,7200,100,0,1)
			Msg2Player("你在转动太岁之轮的时候，意外地获得在未来2小时内经验值和技能熟练度获取加倍的状态。")
		end
	elseif (UTask_cp_2==6) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			Talk(1,"no","太岁师：你转动到的是<color=green>廉贞<color>，你将一次性获得15000经验。")
			AddOwnExp(15000)
			Msg2Player("你在转动太岁之轮的时候，意外地得到15000经验。")
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			Talk(1,"no","太岁师：你转动到的是<color=green>廉贞<color>，你将一次性获得30000经验。")
			AddOwnExp(30000)
			Msg2Player("你在转动太岁之轮的时候，意外地得到30000经验。")
		end
	elseif (UTask_cp_2==7) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			Talk(1,"no","太岁师：你转动到的是<color=green>贪狼<color>，你将一次性获得150000经验。")
			AddOwnExp(150000)
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			Talk(1,"no","太岁师：你转动到的是<color=green>贪狼<color>，你将一次性获得300000经验。")
			AddOwnExp(300000)
		end
		Msg2Player(format("你在转动太岁之轮的时候，意外地得到%d经验。", nRollCount <= 3 and 150000 or 300000))
	elseif (UTask_cp_2==8) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			if (GetSex()==1) then
				PolyMorph(84,1,0,-1,3600)
				Talk(1,"futi_shuoming","太岁师：你转动到的是<color=green>太阴<color>，你受太阴神的福泽，将在1小时之内被凤凰附体。")
				SetTask(851,0)
				Msg2Player("你将在1小时之内被凤凰附体。")
			else
				PolyMorph(84,0,0,-1,3600)
				Talk(1,"futi_shuoming","太岁师：你转动到的是<color=green>太阴<color>，将在1小时之内被凤凰附体，很不幸，由于你是男性，没有受到太阴神的福泽，附体状态下将不可参与战斗。")
				SetTask(851,0)
				Msg2Player("你将在1小时之内被凤凰附体，但不可参与战斗。")
			end;
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			if (GetSex()==1) then
				PolyMorph(84,1,0,-1,3600)
				Talk(1,"futi_shuoming","太岁师：你转动到的是<color=green>太阴<color>，你受太阴神的福泽，将在1小时之内被凤凰附体。并且你在一小时内将会获得内力回复增加10点的状态，请好好加以利用！")
				SetTask(851,0)
				Msg2Player("你将在1小时之内被凤凰附体。")
				AddBuff(13,3600,10,0,1)
			else
				PolyMorph(84,0,0,-1,3600)
				Talk(1,"futi_shuoming","太岁师：你转动到的是<color=green>太阴<color>，将在1小时之内被凤凰附体，很不幸，由于你是男性，没有受到太阴神的福泽，附体状态下将不可参与战斗。并且你在一小时内将会获得内力回复增加10点的状态，请好好加以利用！")
				SetTask(851,0)
				Msg2Player("你将在1小时之内被凤凰附体，但不可参与战斗。")
				AddBuff(13,3600,10,0,1)
			end;
		end
	elseif (UTask_cp_2==9) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			if (GetSex()==0) then
				PolyMorph(99,1,0,-1,3600)
				Talk(1,"futi_shuoming","太岁师：你转动到的是<color=green>太阳<color>，你受太阳神的福泽，将在1小时之内被巨龙附体。")
				SetTask(851,0)
				Msg2Player("你将在1小时之内被巨龙附体。")
			else
				PolyMorph(99,0,0,-1,3600)
				Talk(1,"futi_shuoming","太岁师：你转动到的是<color=green>太阳<color>，将在1小时之内被巨龙附体，很不幸，由于你是女性，没有受到太阳神的福泽，附体状态下将不可参与战斗。")
				SetTask(851,0)
				Msg2Player("你将在1小时之内被巨龙附体，但不可参与战斗。")
			end;
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			if (GetSex()==0) then
				PolyMorph(99,1,0,-1,3600)
				Talk(1,"futi_shuoming","太岁师：你转动到的是<color=green>太阳<color>，你受太阳神的福泽，将在1小时之内被巨龙附体。并且你在一小时内将会获得生命回复增加10点的状态，请好好加以利用！")
				SetTask(851,0)
				Msg2Player("你将在1小时之内被巨龙附体。")
				AddBuff(14,3600,10,0,1)
			else
				PolyMorph(99,0,0,-1,3600)
				Talk(1,"futi_shuoming","太岁师：你转动到的是<color=green>太阳<color>，将在1小时之内被巨龙附体，很不幸，由于你是女性，没有受到太阳神的福泽，附体状态下将不可参与战斗。并且你在一小时内将会获得生命回复增加10点的状态，请好好加以利用！")
				SetTask(851,0)
				Msg2Player("你将在1小时之内被巨龙附体，但不可参与战斗。")
				AddBuff(14,3600,10,0,1)
			end;
		end
	elseif (UTask_cp_2==10) then
		if (nRollCount >= 1 and nRollCount <= 3) then
			Talk(1,"no","太岁师：你转动到的是<color=green>禄存<color>，你将获得在未来1小时内所有技能等级+1的状态，请你好好加以利用。")
			AddBuff(7,3600,1,0,1)
			Msg2Player("你在转动太岁之轮的时候，意外地获得在未来1小时内所有技能等级+1的状态。")
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			-- modified by yaoxin for 2010-11 begin
			AddBuff(7,3600,1,0,1)
			Msg2Player("你在转动太岁之轮的时候，意外地获得在未来1小时内所有技能等级+1的状态。")
			if (IsExistBuff(23)==0) then
				local SkillList = {}
				local LastSkillIdx = GetLastSkillIdx()
				local AddIdx = 1
				for i=1,LastSkillIdx do 
					if ( IsSkillActivedByIdx(i) == 1 and GetSkillId(i) > 2 ) then
						SkillList[AddIdx] = GetSkillId(i)
						AddIdx = AddIdx + 1
					end
				end
				
				local SkillNum = getn(SkillList)
				if (SkillNum > 0) then
					local n = random(1, SkillNum)
					AddBuff(8,3600,SkillList[n],1,0) --替换掉
				end
				Talk(1,"no","太岁师：你转动到的是<color=green>禄存<color>，你将获得在未来1小时内获得所有技能等级+1的状态，且你的某一项技能等级也在此时额外提升了，请你好好加以利用。")
			else
				Talk(1,"no","太岁师：你转动到的是<color=green>禄存<color>，你将获得在未来1小时内所有技能等级+1的状态，请你好好加以利用。")				
			end
			-- modified by yaoxin for 2010-11 end
		end
	else
		if (nRollCount >= 1 and nRollCount <= 3) then
			Talk(1,"no","太岁师：你转动到的是<color=green>破军<color>，你将获得在未来2小时内所有技能等级+1的状态，请你好好加以利用。")
			AddBuff(7,7200,1,0,1)
			Msg2Player("你在转动太岁之轮的时候，意外地获得在未来2小时内所有技能等级+1的状态。")
		elseif(nRollCount >= 4 and nRollCount <= 6) then
			-- modified by yaoxin for 2010-11 begin
			AddBuff(7,7200,1,0,1)
			Msg2Player("你在转动太岁之轮的时候，意外地获得在未来2小时内所有技能等级+1的状态。")
			if (IsExistBuff(23)==0) then
				local SkillList = {}
				local LastSkillIdx = GetLastSkillIdx()
				local AddIdx = 1
				for i=1,LastSkillIdx do 
					if ( IsSkillActivedByIdx(i) == 1 and GetSkillId(i) > 2 ) then
						SkillList[AddIdx] = GetSkillId(i)
						AddIdx = AddIdx + 1
					end
				end
				
				local SkillNum = getn(SkillList)
				if (SkillNum > 0) then
					local n = random(1, SkillNum)
					AddBuff(8,7200,SkillList[n],1,0) --替换掉
				end
				Talk(1,"no","太岁师：你转动到的是<color=green>破军<color>，你将在未来2小时内获得所有技能等级+1的状态，且你的某一项技能等级也在此时额外提升了，请你好好加以利用。")
			else
				Talk(1,"no","太岁师：你转动到的是<color=green>破军<color>，你将获得在未来2小时内所有技能等级+1的状态，请你好好加以利用。")
			end
			-- modified by yaoxin for 2010-11 end
		end
	end;
--	if (GetTask(825)==2) then    --暑期活动
--		AddNormalItem(6,1,232,1,0,0)
--		TopMessage("你获得了一个<color=green>替身纸人<color>。")
--		Msg2Player("你获得了一个替身纸人。")
--	end;
end;

function huodong()
	Talk(1,"no","太岁师：在活动期间，每人每天将在<color=green>第一次<color>转动太岁之后免费获得一个<color=green>替身纸人<color>，")
end;

function no()
		CloseDialog()
end;








-- 附体说明单独续页，保留原文，避免多段对白超过通用UI包容量。
function futi_shuoming()
	Talk(1,"no","太岁师：你遇到困难了吗？虽然我不能帮你驱除附体效果，但我听说<color=green>朝歌<color>的某位姑娘可以帮助你，不过听说这位姑娘跟花神一样也生性爱花，如果你拿一些<color=green>鲜花<color>去找到她，也许她能帮助你。")
end
