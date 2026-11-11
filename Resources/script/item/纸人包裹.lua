--description: 纸人包裹；原模板使用后消失，由C++道具事务只消耗当前实例一次。
function main()
	assert(AddNormalItemPile(6,1,232,1,0,0,{Count=12}) > 0)
	Talk(1,"no","你获得了12个<color=green>替身纸人<color>")
	Msg2Player("你获得了12个替身纸人")
end
function no()
	CloseDialog()
end
