--description: 琵琶
--author: yichuan	
--date: 2005/3/10

function main(sel)
			tasks = 
			{
				 {"交换树苗","renwu";show=0},
				 --{"红玫瑰","renwu1";show=0}
				 {"龙凤变身","renwu2";show=0}
			}
			if(GetLevel()>=12)then
					tasks[1].show=1
			end;
			--if(GetTask(318)==1)and(HaveNormalItem(3,28,0,0)>=1)then
			--		tasks[2].show=1
			--end;

			if (GetMorphType()==84) or (GetMorphType()==99) then
					tasks[2].show=1
			end;
				SayTask(11119,tasks)
end;


--function   renwu1()
--		Talk(1,"no","琵琶：唉，索姆拉要是能够大胆一些就好了。这颗红水晶我收下了，这朵红玫瑰还请你转交给索姆拉。")
--		SetTask(318,2)
--		DelNormalItem(3,28,0,0)
--		AddNormalItem(6,1,22,1,0,0)
--end;


function  renwu()
		if GetLevel() < 12 then
			Talk(1,"no",11119)
			return
		end
		if(HaveNormalItem(4,48,0,1)>=1)and(GetCash()>=10000)then
						assert(DelNormalItem(4,48,0,1)==1)
						assert(Pay(10000)==1)
						assert(AddNormalItemPile(4,49,0,1,0,0)==1)
						Talk(1,"no",11120)				
		else
						Talk(1,"no",11121)
		end;
end;


function renwu2()
	if (GetMorphType()==84) then
		MsgBox("琵琶：太岁临头，尊神附体！凤乃天女之象，翼爪飞扬，百鸟之王。不过，俗言福祸同倚！太岁之轮虽为神器，一切还得看各人福缘，不可强求。凡体，如此福瑞恐难承受。吾自幼修习清律梵音，倒是可以为你化解此灾，成就福瑞，你可愿意？","yes","no")
	elseif (GetMorphType()==99) then
		MsgBox("琵琶：太岁临头，尊神附体！龙乃天子之象，登天潜渊，百兽震惶。不过，俗言福祸同倚！太岁之轮虽为神器，一切还得看各人福缘，不可强求。凡体，如此福瑞恐难承受。吾自幼修习清律梵音，倒是可以为你化解此灾，成就福瑞，你可愿意？","yes","no")		
	end;
end;

function yes()
	if ((GetMorphType()~=99 )and (GetMorphType()~=84))then 
		Talk(1,"no",11119)
	elseif ((GetMorphType()==99 )or (GetMorphType()==84)) and (GetTask(851)<73 or GetTask(851)>76)then 
				local a=random(73,76)
				if (a==73) then
					Talk(1,"no","琵琶：梅花有王者之香，琵琶有希声之律。吾此清律梵音之法，需以梅花之香，琵琶之律催动。阁下如果能送我一朵<color=green>梅花<color>，我便为你施法。")
				elseif (a==74) then
					Talk(1,"no","琵琶：兰花有王者之香，琵琶有希声之律。吾此清律梵音之法，需以兰花之香，琵琶之律催动。阁下如果能送我一朵<color=green>兰花<color>，我便为你施法。")
				elseif (a==75) then
					Talk(1,"no","琵琶：竹花有王者之香，琵琶有希声之律。吾此清律梵音之法，需以竹花之香，琵琶之律催动。阁下如果能送我一朵<color=green>竹花<color>，我便为你施法。")
				elseif (a==76) then
					Talk(1,"no","琵琶：菊花有王者之香，琵琶有希声之律。吾此清律梵音之法，需以菊花之香，琵琶之律催动。阁下如果能送我一朵<color=green>菊花<color>，我便为你施法。")
				end;
				SetTask(851,a)
	elseif (GetTask(851)>0) then
			if ((GetMorphType()==99 )or (GetMorphType()==84)) and (HaveNormalItem(3,GetTask(851),0,0)>0) then
				local a=GetTask(851)
				Msg2Player("你帮琵琶找到了鲜花,琵琶给了你100000经验奖励。")
				if (a==73) then
					Talk(2,"no","琵琶：一些皆已就绪，你只需静坐安神，轻嗅梅花香，听吾为你弹奏一曲便可。","琵琶：福缘已至，灾难已除。你也莫要谢我，清弹梅花调，只为觅知音。你若有心，见着西岐伯邑考，替我传话一声：琵琶念瑶琴，合奏风下松。")
				elseif (a==74) then
					Talk(2,"no","琵琶：一些皆已就绪，你只需静坐安神，轻嗅兰花香，听吾为你弹奏一曲便可。","琵琶：福缘已至，灾难已除。你也莫要谢我，清弹兰花调，只为觅知音。你若有心，见着西岐伯邑考，替我传话一声：琵琶念瑶琴，合奏风下松。")
				elseif (a==75) then
					Talk(2,"no","琵琶：一些皆已就绪，你只需静坐安神，轻嗅竹花香，听吾为你弹奏一曲便可。","琵琶：福缘已至，灾难已除。你也莫要谢我，清弹竹花调，只为觅知音。你若有心，见着西岐伯邑考，替我传话一声：琵琶念瑶琴，合奏风下松。")
				elseif (a==76) then
					Talk(2,"no","琵琶：一些皆已就绪，你只需静坐安神，轻嗅菊花香，听吾为你弹奏一曲便可。","琵琶：福缘已至，灾难已除。你也莫要谢我，清弹菊花调，只为觅知音。你若有心，见着西岐伯邑考，替我传话一声：琵琶念瑶琴，合奏风下松。")
				end;
				assert(PolyMorph(-1,1,0,-1,0)==1)
				assert(DelNormalItem(3,a,0,0)==1)
				AddOwnExp(100000)
				SetTask(851,0)
			elseif  ((GetMorphType()==99 )or (GetMorphType()==84)) and (HaveNormalItem(3,GetTask(851),0,0)==0) then
				local a=GetTask(851)
				if (a==73) then
					Talk(1,"no","琵琶：梅花有王者之香，琵琶有希声之律。吾此清律梵音之法，需以梅花之香，琵琶之律催动。阁下如果能送我一朵<color=green>梅花<color>，我便为你施法。")
				elseif (a==74) then
					Talk(1,"no","琵琶：兰花有王者之香，琵琶有希声之律。吾此清律梵音之法，需以兰花之香，琵琶之律催动。阁下如果能送我一朵<color=green>兰花<color>，我便为你施法。")
				elseif (a==75) then
					Talk(1,"no","琵琶：竹花有王者之香，琵琶有希声之律。吾此清律梵音之法，需以竹花之香，琵琶之律催动。阁下如果能送我一朵<color=green>竹花<color>，我便为你施法。")
				elseif (a==76) then
					Talk(1,"no","琵琶：菊花有王者之香，琵琶有希声之律。吾此清律梵音之法，需以菊花之香，琵琶之律催动。阁下如果能送我一朵<color=green>菊花<color>，我便为你施法。")
				end;
			end;
	end;
end;

function no()
		CloseDialog()
end;
