# 当前 Lua 可用函数说明（Lua函数表）

核对日期：2026-11-11。适用项目：`F:\mxd\Jx` 当前 C/C++ 服务端。本文是后续NPC脚本开发的接口基准；增加、修改、删除注册函数或改变参数、返回值、事务行为时，必须同步维护本文。

## 阅读方法与范围

- 编写或修改NPC Lua前，先查本文中的函数名、参数、返回值、状态和限制。本文收录服务端游戏脚本 **263个不同注册名**，世界脚本 **9个注册名**（与游戏脚本重叠7个、另有2个专用名），以及实际打开的Lua标准库 **90个名字**（含4个注册为报错的废弃名）；合计 **355个不同名字**，均有独立条目。
- “实现已核对”表示已对照当前注册和函数正文，**不表示全部接口已通过新版客户端实测**。带“旧接口”“待适配”“有缺陷”“不可用”的条目保留用于查错；不能把存在名字当成功能已完整适配。
- 以服务端 `_SERVER` 分支为准，排除注释中的注册项、客户端专用 `PlaySound` / `PlaySprMovie`、未注册的C++辅助函数。原脚本内 `main`、`OnCompound`、公共库中自定义函数不属于引擎全局API；只能在加载了对应脚本后使用其实际定义。
- 普通NPC／玩家脚本由 `GameScriptFuns` 注册；全局世界脚本由 `WorldScriptFuns` 注册。标准库在创建Lua状态时打开。脚本可覆盖同名全局，使用时还要核对原脚本及 `Include` 内容。
- 文中的方括号表示可选参数，不是实际Lua语法；`genre`、`taskId`、`mapId`等示例变量需从已确认的原资源／上下文赋值，示例不会自动生成有效编号。函数返回“无”时，不可将它当成功标志判断。

## 当前 Lua 版本和调用约定

本项目使用 **Lua 4.0**。基础、字符串、数学和IO库已打开，调试库 `Lua_OpenDBLib` 未打开。使用 `random`、`floor`、`getn`、`format`、`strfind` 等全局函数；不要直接套用 `math.random`、`table.insert`、`string.find`、`os.time`、`pairs`、`ipairs`、`require`、`pcall`、`#table` 或 Lua5的metatable API。

Lua4中仅 `nil` 为假，数字 **0为真**。数字结果必须显式比较，例如 `Pay(amount) == 1` 或 `HaveNormalItem(...) > 0`。不要写 `if Pay(amount) then`。逻辑假值用 `nil`，不能直接套用Lua5的 `true` / `false` 字面量约定。闭包／upvalue也须遵循原Lua4语法和现有脚本风格。

玩家API依赖C++提供的 `playerindex`；世界API可能还依赖 `subworldindex`，物件API依赖 `objindex`。它们是运行时上下文，不是让脚本任意写入的参数。无玩家的世界脚本不得调用需要玩家的旧接口。

模板身份、网络ID和索引不可混用：

| 名称 | 含义 |
| --- | --- |
| genre/detail/particular/level | 原资源中物品模板的四项身份；不是物品唯一ID |
| GetSelectedItems数组下标 | 当前窗口请求的1起始提交槽号 |
| npcIndex | 服务端Npc数组的运行时索引 |
| npcDwordId | 网络同步的NPC唯一DWORD ID，供旧AttackNpc/KillNpc查附近目标 |
| playerIndex | 服务端Player数组的运行时索引 |
| dataIndex | 某个Mission活动内部的玩家数据索引 |
| mapId | 地图资源ID |
| subWorldIndex | 本服务器已加载世界的运行时索引 |
| MPS坐标 | 原始点坐标；GetWorldPos/NewWorld/SetPos使用MPS÷32的脚本格坐标 |

## NPC业务分层和回滚边界

对白、菜单、任务条件、配方匹配、材料数量、费用、成功率、奖励和提示优先写对应NPC Lua；规则取原 `Resources` 脚本和配置。C++提供通用协议、真实物品校验与操作、同步、存档和事务能力。缺少能力时补通用API并同步本文，不能把NPC配方或专属分支重新搬回C++。

Lua不得直接连接、查询或修改数据库，不得编写、拼接或执行SQL，也不得通过外部程序绕过限制。数据库连接、查询、写入、事务及失败回滚统一由现有C++数据访问与存档流程处理；Lua仅调用经过校验的通用业务接口。不得向Lua暴露任意SQL执行接口或数据库连接信息。

当前窗口请求事务提供 `GetSelectedItems` → `DelSelectedItem` / `Pay` / `AddNormalItem` → `CompleteItemOperation` 的调用链。`CompleteItemOperation(0)`表示**概率失败但操作已完成，提交合法消耗**；未完成或Lua报错则回滚，`CompleteItemOperation(1)`之后报错也会回滚。

已有快照覆盖随身背包、快捷栏、鼠标物品、各房间现金、玩家任务、普通宠物状态和经典／IB技能Buff；`AddOwnExp`在成功后延迟发放；所有脚本UI、`PutMessage` / `Msg2Player`以及`WasteCopperCash`扣铜钱通知在事务成功后延迟发送。`PolyMorph`验证后延迟提交，失败不变身。**不保证任意Lua回调都有事务**：当前所有实际绑定NPC脚本的对白入口与物品窗口入口建立事务。穿戴、仓库、技能学习、阵营、帮会、婚姻、位置、系统文件副作用等未泛化纳入完整快照。`WriteLog`以及某些旧协议通知也不回滚。新业务涉及这些范围时，先补通用事务与存档支持，不能宣称已支持完整回滚。

物品模板中的脚本入口及其对白回调新增自动事务，但仅开放 `KSortScript.cpp::IsItemTransactionFunction` 中已覆盖快照的接口和只读／纯计算接口；地图类仅开放下述受限延迟接口 `TeleportWithItem`；`NewWorld`、`SetPos`、`SetRevPos`、`UseTownPortal` 等即时接口，以及技能、Buff、外部IO等仍不在该入口的支持范围。每个回调执行前重新检查调用关系，失败不提交本次道具消耗、现金及宠物变更。NPC脚本入口已扩展到所有地图；其物品、现金、任务、Buff和延迟变身在上述范围内回滚，不能据此认为穿戴、仓库等即时副作用已完整覆盖。

## 分类索引

- **对白、窗口与提示**：[Say](#say)、[SayTask](#saytask)、[Talk](#talk)、[MsgBox](#msgbox)、[CloseDialog](#closedialog)、[PutMessage](#putmessage)、[Msg2Player](#msg2player)、[TopMessage](#topmessage)、[Sale](#sale)、[OpenBox](#openbox)、[OpenNewCompound](#opennewcompound)、[EnchaseItem](#enchaseitem)、[AddNote](#addnote)、[PlayMusic](#playmusic)、[FadeInMusic](#fadeinmusic)、[FadeOutMusic](#fadeoutmusic)。
- **普通宠物与铜钱商店**：[SaleEx](#saleex)、[GetPetType](#getpettype)、[GetPetHonor](#getpethonor)、[GetPetColor](#getpetcolor)、[GetPetName](#getpetname)、[IsPetReleased](#ispetreleased)、[GenPet](#genpet)、[DelPet](#delpet)、[SetPetType](#setpettype)、[SetPetHonor](#setpethonor)、[ReleasePet](#releasepet)、[GetPetFuncAuthority](#getpetfuncauthority)、[SetPetFuncAuthority](#setpetfuncauthority)、[GetPetFuncActive](#getpetfuncactive)、[GetPetSettings](#getpetsettings)、[GetItemInfo](#getiteminfo)、[RepairItem](#repairitem)。
- **资源、脚本与服务器日志**：[ReadTabFile](#readtabfile)、[GetGameVersion](#getgameversion)、[Include](#include)、[WriteLog](#writelog)、[Message](#message)、[ExeScript](#exescript)。
- **物品、现金、铜钱与窗口事务**：[HaveNormalItem](#havenormalitem)、[HaveItem2](#haveitem2)、[IsExistItem](#isexistitem)、[ConsumeUseItem](#consumeuseitem)、[DelNormalItem](#delnormalitem)、[DelItem2](#delitem2)、[AddNormalItem](#addnormalitem)、[AddNormalItem2](#addnormalitem2)、[AddNormalItemPile](#addnormalitempile)、[GetItemLevel2](#getitemlevel2)、[HaveNormalItemInQuick](#havenormaliteminquick)、[DelNormalItemInQuick](#delnormaliteminquick)、[GetSelectedItems](#getselecteditems)、[DelSelectedItem](#delselecteditem)、[CompleteItemOperation](#completeitemoperation)、[GetCash](#getcash)、[Pay](#pay)、[Earn](#earn)、[PrePay](#prepay)、[GetCopperCashCount](#getcoppercashcount)、[WasteCopperCash](#wastecoppercash)、[GetCredit](#getcredit)、[DecCredit](#deccredit)、[GetRepute](#getrepute)、[AddRepute](#addrepute)、[ImproveItemMagicAttr](#improveitemmagicattr)、[AddItem](#additem)、[AddEventItem](#addeventitem)、[GetItemCount](#getitemcount)、[HaveEventItem](#haveeventitem)、[HaveItem](#haveitem)、[DelItem](#delitem)、[DelEventItem](#deleventitem)、[DropItem](#dropitem)。
- **玩家任务与位、字节操作**：[GetTask](#gettask)、[SetTask](#settask)、[GetTaskTemp](#gettasktemp)、[SetTaskTemp](#settasktemp)、[TaskNote](#tasknote)、[GetTaskByte](#gettaskbyte)、[SetTaskByte](#settaskbyte)、[GetBit](#getbit)、[SetBit](#setbit)、[GetByte](#getbyte)、[SetByte](#setbyte)。
- **角色查询、属性与身份**：[GetName](#getname)、[GetUUID](#getuuid)、[GetLevel](#getlevel)、[GetSex](#getsex)、[GetSeries](#getseries)、[GetPlayerType](#getplayertype)、[GetDefend](#getdefend)、[GetExp](#getexp)、[GetLeadExp](#getleadexp)、[GetLeadLevel](#getleadlevel)、[GetRestAP](#getrestap)、[GetRestSP](#getrestsp)、[GetProp](#getprop)、[GetMagicPoint](#getmagicpoint)、[GetCurCamp](#getcurcamp)、[GetCamp](#getcamp)、[GetRank](#getrank)、[GetPK](#getpk)、[GetLucky](#getlucky)、[GetEng](#geteng)、[GetDex](#getdex)、[GetStrg](#getstrg)、[GetVit](#getvit)、[GetLife](#getlife)、[GetMana](#getmana)、[GetStamina](#getstamina)、[RestoreLife](#restorelife)、[RestoreMana](#restoremana)、[RestoreStamina](#restorestamina)、[GetColdR](#getcoldr)、[GetFireR](#getfirer)、[GetLightR](#getlightr)、[GetPoisonR](#getpoisonr)、[GetPhyR](#getphyr)、[SetFireR](#setfirer)、[SetLightR](#setlightr)、[SetPoisonR](#setpoisonr)、[SetPhyR](#setphyr)、[SetColdR](#setcoldr)、[SetCurCamp](#setcurcamp)、[SetCamp](#setcamp)、[RestoreCamp](#restorecamp)、[GetFaction](#getfaction)、[SetFaction](#setfaction)、[SetSeries](#setseries)、[SetRank](#setrank)、[AddOwnExp](#addownexp)、[AddExp](#addexp)、[AddLeadExp](#addleadexp)、[AddMagicPoint](#addmagicpoint)、[AddProp](#addprop)、[GetExtPoint](#getextpoint)、[PayExtPoint](#payextpoint)。
- **技能与限时效果**：[IsSkillActived](#isskillactived)、[IsExistBuff](#isexistbuff)、[HaveIBBuff](#haveibbuff)、[GetIBBuffLeftTimes](#getibbufflefttimes)、[AddBuff](#addbuff)、[AddIBBuff](#addibbuff)、[RemoveBuff](#removebuff)、[AddMagic](#addmagic)、[DelMagic](#delmagic)、[HaveMagic](#havemagic)、[GetMagicLevel](#getmagiclevel)、[GetSkillId](#getskillid)、[SetSkillLevel](#setskilllevel)、[RollbackSkill](#rollbackskill)、[UpdateSkill](#updateskill)。
- **地图、坐标、NPC与复活**：[GetWorldPos](#getworldpos)、[GetUseItemInfo](#getuseiteminfo)、[TeleportWithItem](#teleportwithitem)、[GetPos](#getpos)、[SetPos](#setpos)、[NewWorld](#newworld)、[SubWorldID2Idx](#subworldid2idx)、[GetNpcIdx](#getnpcidx)、[AddNpc](#addnpc)、[DelNpc](#delnpc)、[SetNpcScript](#setnpcscript)、[SetNpcCurCamp](#setnpccurcamp)、[SetRevPos](#setrevpos)、[SetTempRevPos](#settemprevpos)、[SetDeathScript](#setdeathscript)、[SetLogoutRV](#setlogoutrv)、[UseTownPortal](#usetownportal)、[ReturnFromPortal](#returnfromportal)、[SetFightState](#setfightstate)、[GetFightState](#getfightstate)、[AttackNpc](#attacknpc)、[KillNpc](#killnpc)、[Death](#death)、[KillPlayer](#killplayer)、[HideNpc](#hidenpc)、[RevivalAllNpc](#revivalallnpc)、[SetPropState](#setpropstate)。
- **队伍、聊天、公告及管理**：[IsCaptain](#iscaptain)、[GetTeam](#getteam)、[GetTeamSize](#getteamsize)、[LeaveTeam](#leaveteam)、[SetCreateTeam](#setcreateteam)、[Msg2Team](#msg2team)、[Msg2SubWorld](#msg2subworld)、[Msg2Region](#msg2region)、[Msg2GM](#msg2gm)、[Msg2IP](#msg2ip)、[AddGlobalNews](#addglobalnews)、[AddGlobalCountNews](#addglobalcountnews)、[AddGlobalTimeNews](#addglobaltimenews)、[AddLocalNews](#addlocalnews)、[AddLocalCountNews](#addlocalcountnews)、[AddLocalTimeNews](#addlocaltimenews)、[GetInfo](#getinfo)、[GetIP](#getip)、[GetServerName](#getservername)、[GetPlayerCount](#getplayercount)、[KickOutPlayer](#kickoutplayer)、[KickOutSelf](#kickoutself)、[KickOutAccount](#kickoutaccount)、[SetChatFlag](#setchatflag)、[SetPK](#setpk)、[SetPunish](#setpunish)、[ShowLadder](#showladder)、[OpenTong](#opentong)、[SwearBrother](#swearbrother)、[MakeEnemy](#makeenemy)、[AddLoaclTimeNews](#addloacltimenews)。
- **旧车站、码头与路标资源**：[AddStation](#addstation)、[AddTermini](#addtermini)、[GetStation](#getstation)、[GetStationCount](#getstationcount)、[GetCityCount](#getcitycount)、[GetWayPoint](#getwaypoint)、[GetStationName](#getstationname)、[GetWayPointName](#getwaypointname)、[GetPrice2Station](#getprice2station)、[GetPrice2WayPoint](#getprice2waypoint)、[GetStationPos](#getstationpos)、[GetWayPointPos](#getwaypointpos)、[GetWharfName](#getwharfname)、[GetWharfCount](#getwharfcount)、[GetWharfPrice](#getwharfprice)、[GetWharf](#getwharf)、[GetWharfPos](#getwharfpos)、[GetTerminiFState](#getterminifstate)、[GetCity](#getcity)。
- **计时与系统时间**：[SystemTime](#systemtime)、[GetHMS](#gethms)、[GetYMD](#getymd)、[Time2LocalYMD](#time2localymd)、[SetTimer](#settimer)、[StopTimer](#stoptimer)、[GetTimerId](#gettimerid)、[GetRestTime](#getresttime)。
- **旧任务副本与全局活动**：[GetMissionV](#getmissionv)、[SetMissionV](#setmissionv)、[GetGlbMissionV](#getglbmissionv)、[SetGlbMissionV](#setglbmissionv)、[OpenMission](#openmission)、[RunMission](#runmission)、[CloseMission](#closemission)、[StartMissionTimer](#startmissiontimer)、[StopMissionTimer](#stopmissiontimer)、[GetMSRestTime](#getmsresttime)、[AddMSPlayer](#addmsplayer)、[DelMSPlayer](#delmsplayer)、[GetNextPlayer](#getnextplayer)、[PIdx2MSDIdx](#pidx2msdidx)、[MSDIdx2PIdx](#msdidx2pidx)、[GetMSIdxGroup](#getmsidxgroup)、[GetMSPlayerCount](#getmsplayercount)、[SetPMParam](#setpmparam)、[GetPMParam](#getpmparam)、[Msg2MSGroup](#msg2msgroup)、[Msg2MSAll](#msg2msall)、[NpcIdx2PIdx](#npcidx2pidx)、[Msg2MSPlayer](#msg2msplayer)。
- **Lua 4基础库与表操作**：[assert](#assert)、[error](#error)、[type](#type)、[tonumber](#tonumber)、[tostring](#tostring)、[print](#print)、[getn](#getn)、[tinsert](#tinsert)、[tremove](#tremove)、[sort](#sort)、[next](#next)、[foreach](#foreach)、[foreachi](#foreachi)、[rawget](#rawget)、[rawset](#rawset)、[getglobal](#getglobal)、[setglobal](#setglobal)、[globals](#globals)、[call](#call)、[dofile](#dofile)、[dostring](#dostring)、[gcinfo](#gcinfo)、[collectgarbage](#collectgarbage)、[newtag](#newtag)、[tag](#tag)、[settag](#settag)、[gettagmethod](#gettagmethod)、[settagmethod](#settagmethod)、[copytagmethods](#copytagmethods)、[_ALERT](#_alert)、[_ERRORMESSAGE](#_errormessage)、[rawgettable](#rawgettable)、[rawsettable](#rawsettable)、[foreachvar](#foreachvar)、[nextvar](#nextvar)、[rawgetglobal](#rawgetglobal)、[rawsetglobal](#rawsetglobal)。
- **Lua 4字符串库**：[strlen](#strlen)、[strsub](#strsub)、[strlower](#strlower)、[strupper](#strupper)、[strchar](#strchar)、[strbyte](#strbyte)、[strrep](#strrep)、[format](#format)、[strfind](#strfind)、[gsub](#gsub)、[ascii](#ascii)。
- **Lua 4数学库**：[abs](#abs)、[sin](#sin)、[cos](#cos)、[tan](#tan)、[asin](#asin)、[acos](#acos)、[atan](#atan)、[atan2](#atan2)、[ceil](#ceil)、[floor](#floor)、[mod](#mod)、[frexp](#frexp)、[ldexp](#ldexp)、[sqrt](#sqrt)、[min](#min)、[max](#max)、[log](#log)、[log10](#log10)、[exp](#exp)、[deg](#deg)、[rad](#rad)、[random](#random)、[randomseed](#randomseed)。
- **Lua 4文件、系统与诊断库**：[openfile](#openfile)、[closefile](#closefile)、[read](#read)、[write](#write)、[flush](#flush)、[seek](#seek)、[readfrom](#readfrom)、[writeto](#writeto)、[appendto](#appendto)、[clock](#clock)、[date](#date)、[getenv](#getenv)、[remove](#remove)、[rename](#rename)、[tmpname](#tmpname)、[setlocale](#setlocale)、[execute](#execute)、[exit](#exit)、[debug](#debug)。

## 对白、窗口与提示

### `Say`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
Say(textOrId, count, option1, ...)
```

- 参数：textOrId：对白字符串或 stringresource 文本ID；count：选项数；option："显示文本/回调名"。第三参数也可传连续选项数组。
- 返回：无。
- 限制／说明：选项无斜线时回调默认为 main；最多 MAX_ANSWERNUM 个。显示和回调分开发送，回调必须存在于当前脚本，不在本次函数内直接执行。
- 实现：[`LuaSelectUI`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:631)。

用法示例：

```lua
Say("请选择", 2, "继续/OnContinue", "关闭/OnClose")
```

### `SayTask`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SayTask(textOrId, options)
```

- 参数：textOrId：对白或文本ID；options：连续数组，每项 {"按钮文本", "回调名", show=1}；show省略则显示，show=0则隐藏。
- 返回：无。
- 限制／说明：扫描最多256行；回调名不带“文本/”前缀。
- 实现：[`LuaSayTask`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:788)。

用法示例：

```lua
SayTask("请选择", {{"继续", "OnContinue"}, {"关闭", "OnClose"}})
```

### `Talk`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
Talk(count, callback, line1, ...)
```

- 参数：count：1～255的整数对白段数，超过实际提供的段数时按实际段数；callback：结束回调名，空字符串表示不回调；line：字符串或数字文本ID，同次调用类型一致。
- 返回：无。
- 限制／说明：当前实现不支持把对白数组作为第三参数，尽管旧注释提到这种形式。只使用逐项参数。
- 容量：整次正文缓冲区300字节，每段都包含一个尾部`|`，另留一个NUL；单段字面量最多298个GBK字节。安全保留能够完整放入的连续段落，其后超出容量的段落不发送并记录服务端警告；一段都放不下则不发送。数字文本ID也逐段计入分隔符。较长说明必须由Lua拆成`Talk(1, "next_page", text)`的回调续页，不能依赖截断或修改协议容量。
- 空／无效的正文类型、非整数或越界的文本ID不发送；count非法时不发送。
- 实现：[`LuaTalkUI`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2222)。

用法示例：

```lua
Talk(1, "", "这是一段对白。")
```

### `MsgBox`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
MsgBox(textOrId, onConfirm[, onCancel])
```

- 参数：textOrId：对白或文本ID；onConfirm：确认回调名；onCancel：可选取消回调名。
- 返回：无。
- 限制／说明：2或3个参数；回调只写函数名，不是 Lua 代码。
- 实现：[`LuaMsgBox`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:883)。

用法示例：

```lua
MsgBox("确定继续？", "OnConfirm", "OnCancel")
```

### `CloseDialog`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
CloseDialog()
```

- 参数：无。
- 返回：无。
- 限制／说明：清除等待回答、选项及对话脚本状态，并向当前玩家发送关闭。
- 实现：[`LuaCloseDialog`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1978)。

用法示例：

```lua
CloseDialog()
```

### `PutMessage`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
PutMessage(textOrId)
```

- 参数：textOrId：提示字符串或文本ID。
- 返回：无。
- 限制／说明：字符串按原64字节缓冲截取；物品事务内延迟到提交后发送，回滚丢弃。
- 实现：[`LuaSendMessageInfo`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1431)。

用法示例：

```lua
PutMessage("材料不足。")
```

### `Msg2Player`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
Msg2Player(text)
```

- 参数：text：系统公告／聊天提示字符串。
- 返回：无。
- 限制／说明：当前玩家；物品事务内延迟发送。该接口发送系统公告，当前客户端会同时显示在聊天栏和系统栏，不能作为右侧专用通知。不等同于 PutMessage 的界面提示。
- 实现：[`LuaMsgToPlayer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2339)。

用法示例：

```lua
Msg2Player("合成成功。")
```

### `TopMessage`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
TopMessage(text)
```

- 参数：text：系统公告／聊天提示字符串。
- 返回：无。
- 限制／说明：与 `Msg2Player` 同一实现、同一参数和返回值。当前玩家；物品事务内延迟发送。该接口发送系统公告，当前客户端会同时显示在聊天栏和系统栏，不能作为右侧专用通知。不等同于 PutMessage 的界面提示。
- 实现：[`LuaMsgToPlayer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2339)。

用法示例：

```lua
TopMessage("合成成功。")
```

### `Sale`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
Sale(shopId)
```

- 参数：shopId：原商店配置的1起始编号，C++内部减1。
- 返回：无。
- 限制／说明：打开商店，不直接购买或发放商品；有效商店ID从原资源查询。
- 实现：[`LuaSale`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2135)。

用法示例：

```lua
Sale(shopId)
```

### `OpenBox`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
OpenBox(boxId)
```

- 参数：boxId：当前仅支持4；必须来自有效储物箱物件交互上下文。
- 返回：无。
- 限制／说明：只打开仓库；许可、任务和重生地业务由原箱子／NPC脚本决定。底层还验证m_nObjectIdx和CanUseStoreBox，不能从无箱子上下文的任意NPC直接开仓库。
- 实现：[`LuaOpenBox`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2155)。

用法示例：

```lua
OpenBox(4)
```

### `OpenNewCompound`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
OpenNewCompound([callback])
```

- 参数：callback：当前脚本的物品请求回调名，省略为 OnCompound。
- 返回：无。
- 限制／说明：打开新合成强化窗口。回调名必须是合法脚本标识符；窗口材料请求进入通用校验和事务入口。不是直接调用合成业务。
- 实现：[`LuaOpenNewCompound`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6300)。

用法示例：

```lua
OpenNewCompound("OnCompound")
```

### `EnchaseItem`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
EnchaseItem([callback])
```

- 参数：callback：当前脚本的物品请求回调名，省略为 OnCompound。
- 返回：无。
- 限制／说明：打开旧合成窗口。回调名必须是合法脚本标识符；窗口材料请求进入通用校验和事务入口。不是直接调用合成业务。
- 实现：[`LuaEnchaseItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6290)。

用法示例：

```lua
EnchaseItem("OnCompound")
```

### `AddNote`

状态：旧接口；新版界面待核对。作用域：游戏／NPC脚本。

```lua
AddNote(textOrId[, parameter=0])
```

- 参数：textOrId：旧记事信息字符串或ID；parameter：附加整数。
- 返回：无。
- 限制／说明：旧 UI_NOTEINFO；与有存档及任务编号的 TaskNote 不同，新任务优先使用 TaskNote。
- 实现：[`LuaAddNote`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1798)。

用法示例：

```lua
AddNote("记事内容", 0)
```

### `PlayMusic`

状态：旧接口；新版表现待核对。作用域：游戏／NPC脚本。

```lua
PlayMusic(path)
```

- 参数：path：客户端可读取的音乐资源路径字符串。
- 返回：无。
- 限制／说明：只读取第1参数；旧注释中的 loop、vol 未实现。客户端是否接收该旧脚本动作需实测。
- 实现：[`LuaPlayMusic`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6233)。

用法示例：

```lua
PlayMusic("\\music\\example.mp3")
```

### `FadeInMusic`

状态：不可用：空实现。作用域：游戏／NPC脚本。

```lua
FadeInMusic(...)
```

- 参数：任意参数均未读取；没有有效参数约定。
- 返回：无。
- 限制／说明：当前函数体仅 return 0。
- 实现：[`LuaFadeInMusic`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6263)。

用法示例：

```lua
-- FadeInMusic() 当前不执行任何动作。
```

### `FadeOutMusic`

状态：不可用：空实现。作用域：游戏／NPC脚本。

```lua
FadeOutMusic(...)
```

- 参数：任意参数均未读取；没有有效参数约定。
- 返回：无。
- 限制／说明：当前函数体仅 return 0。
- 实现：[`LuaFadeOutMusic`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6268)。

用法示例：

```lua
-- FadeOutMusic() 当前不执行任何动作。
```

## 资源、脚本与服务器日志

### `ReadTabFile`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
ReadTabFile(path)
```

- 参数：path：以反斜杠开头的资源路径；禁止盘符与“..”，长度小于 MAX_PATH。
- 返回：成功为二维字符串数组；失败 nil。
- 限制／说明：行列从1开始，含表头；空单元格为 ""。最多20000行、256列、总计1000000格；单元格按4096字节缓冲检查，拒绝截断。业务解析、版本选择与缓存由 Lua 负责。
- 实现：[`LuaReadTabFile`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:408)。

用法示例：

```lua
local rows = ReadTabFile("\\settings\\item\\001\\装备合成规则表.txt")
```

### `GetGameVersion`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetGameVersion()
```

- 参数：无。
- 返回：当前资源版本整数。
- 实现：[`LuaGetGameVersion`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:449)。

用法示例：

```lua
local version = GetGameVersion()
```

### `Include`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
Include(path)
```

- 参数：path：脚本文件路径；以 / 或反斜杠开头则基于资源根，否则基于服务进程当前目录。
- 返回：无，不转交被载脚本返回值。
- 限制／说明：直接执行文件并共享该脚本的全局环境；按原资源实际路径调用。内部忽略 lua_dofile 返回码；不能把它当可返回成功标志的加载API。
- 实现：[`LuaIncludeFile`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1995)。

用法示例：

```lua
Include("\\script\\lib\\example.lua")
```

### `WriteLog`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
WriteLog(text)
```

- 参数：text：日志字符串。
- 返回：无。
- 限制／说明：写原 g_DebugLog，带 player/name 上下文，消息缓冲512字节；不发送给客户端。推荐用于业务日志。
- 实现：[`LuaWriteLog`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3928)。

用法示例：

```lua
WriteLog("赤松子：开始校验合成请求")
```

### `Message`

状态：可调用；旧日志接口有限制。作用域：游戏／NPC脚本。

```lua
Message(text)
```

- 参数：text：服务端日志文本。
- 返回：无。
- 限制／说明：直接作为 g_DebugLog 格式串使用；不要传含未转义百分号或玩家可控格式文本。推荐 WriteLog。不是玩家消息。
- 实现：[`LuaMessage`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4670)。

用法示例：

```lua
Message("Lua diagnostic")
```

### `ExeScript`

状态：不可用：空实现且返回栈不完整。作用域：游戏／NPC脚本。

```lua
ExeScript(...)
```

- 参数：旧注释称脚本路径和参数，当前实现全部未读取。
- 返回：没有可靠返回值：return 1 但未压入结果。
- 限制／说明：不会执行目标脚本。
- 实现：[`LuaPlayerExecuteScript`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4411)。

用法示例：

```lua
-- 不使用 ExeScript；载入现有公共脚本可用 Include。
```

## 物品、现金、铜钱与窗口事务

### `HaveNormalItem`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
HaveNormalItem(genre, detail, particular, level)
```

- 参数：四项依次为物品大类、子类、具体类别、模板等级；必须正好4个参数。
- 返回：背包匹配物品的总叠加数量；无匹配／参数不符为0。
- 限制／说明：只统计 pos_equiproom 随身背包；不含穿戴、仓库、快捷栏或鼠标。不是布尔值，判断必须 count > 0。
- 实现：[`LuaHaveNormalItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1158)。

用法示例：

```lua
local count = HaveNormalItem(genre, detail, particular, level)
```

### `HaveItem2`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
HaveItem2(genre, detail, particular, level)
```

- 参数：四项依次为物品大类、子类、具体类别、模板等级；必须正好4个参数。
- 返回：背包匹配物品的总叠加数量；无匹配／参数不符为0。
- 限制／说明：与 `HaveNormalItem` 同一实现、同一参数和返回值。只统计 pos_equiproom 随身背包；不含穿戴、仓库、快捷栏或鼠标。不是布尔值，判断必须 count > 0。
- 实现：[`LuaHaveNormalItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1158)。

用法示例：

```lua
local count = HaveItem2(genre, detail, particular, level)
```

### `IsExistItem`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
IsExistItem(genre, detail, particular, level)
```

- 参数：四项依次为物品大类、子类、具体类别、模板等级；必须正好4个参数。
- 返回：背包匹配物品的总叠加数量；无匹配／参数不符为0。
- 限制／说明：与 `HaveNormalItem` 同一实现、同一参数和返回值。只统计 pos_equiproom 随身背包；不含穿戴、仓库、快捷栏或鼠标。不是布尔值，判断必须 count > 0。
- 实现：[`LuaHaveNormalItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1158)。

用法示例：

```lua
local count = IsExistItem(genre, detail, particular, level)
```

### `DelNormalItem`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
DelNormalItem(genre, detail, particular, level[, count=1])
```

- 参数：前四项与 HaveNormalItem 相同；count：正整数，可跨背包内多叠扣取。
- 返回：成功1，失败0；已有物品事务内失败抛错使事务回滚。
- 限制／说明：默认仅背包。特定临仙露类消耗前检查互斥及时间上限；不是任意物品服务器索引删除接口。
- 实现：[`LuaDelNormalItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1347)。

用法示例：

```lua
local ok = DelNormalItem(genre, detail, particular, level, 5)
```

### `DelItem2`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
DelItem2(genre, detail, particular, level[, count=1])
```

- 参数：前四项与 HaveNormalItem 相同；count：正整数，可跨背包内多叠扣取。
- 返回：成功1，失败0；已有物品事务内失败抛错使事务回滚。
- 限制／说明：与 `DelNormalItem` 同一实现、同一参数和返回值。默认仅背包。特定临仙露类消耗前检查互斥及时间上限；不是任意物品服务器索引删除接口。
- 实现：[`LuaDelNormalItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1347)。

用法示例：

```lua
local ok = DelItem2(genre, detail, particular, level, 5)
```

### `AddNormalItem`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
AddNormalItem(genre, detail, particular, level, series, luck[, options])
```

- 参数：genre/detail/particular/level：原资源物品身份；series：模板生成系别；luck：生成幸运参数。前六项为非负整数。options：{Count=1, Bound=0, BindingMode=0}；或兼容旧第7参数0。
- 返回：通常为 成功标志(1/0), 属性说明字符串；部分早期失败仅返回0（第二值为nil）。不返回物品索引。
- 限制／说明：Count范围1～100000且受模板堆叠上限约束，装备只能1；Bound为0/1，BindingMode为0～255，未绑定时模式必须0。按当前资源版本生成，装备满耐久，自动找背包空位／叠加并同步。材料level=1的旧调用可回退level=0。满包或发奖失败在已有事务内抛错回滚。不要套用其他版本“第7参数=数量”的调用。
- 实现：[`LuaAddNormalItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3801)。

用法示例：

```lua
local ok, desc = AddNormalItem(genre, detail, particular, level, 0, 0,
    {Count=1, Bound=0, BindingMode=0})
```

### `AddNormalItem2`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
AddNormalItem2(genre, detail, particular, level, series, luck[, options])
```

- 参数：genre/detail/particular/level：原资源物品身份；series：模板生成系别；luck：生成幸运参数。前六项为非负整数。options：{Count=1, Bound=0, BindingMode=0}；或兼容旧第7参数0。
- 返回：通常为 成功标志(1/0), 属性说明字符串；部分早期失败仅返回0（第二值为nil）。不返回物品索引。
- 限制／说明：与 `AddNormalItem` 同一实现、同一参数和返回值。Count范围1～100000且受模板堆叠上限约束，装备只能1；Bound为0/1，BindingMode为0～255，未绑定时模式必须0。按当前资源版本生成，装备满耐久，自动找背包空位／叠加并同步。材料level=1的旧调用可回退level=0。满包或发奖失败在已有事务内抛错回滚。不要套用其他版本“第7参数=数量”的调用。
- 实现：[`LuaAddNormalItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3801)。

用法示例：

```lua
local ok, desc = AddNormalItem2(genre, detail, particular, level, 0, 0,
    {Count=1, Bound=0, BindingMode=0})
```

### `AddNormalItemPile`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
AddNormalItemPile(genre, detail, particular, level, series, luck[, options])
```

- 参数：genre/detail/particular/level：原资源物品身份；series：模板生成系别；luck：生成幸运参数。前六项为非负整数。options：{Count=1, Bound=0, BindingMode=0}；或兼容旧第7参数0。
- 返回：通常为 成功标志(1/0), 属性说明字符串；部分早期失败仅返回0（第二值为nil）。不返回物品索引。
- 限制／说明：与 `AddNormalItem` 同一实现、同一参数和返回值。Count范围1～100000且受模板堆叠上限约束，装备只能1；Bound为0/1，BindingMode为0～255，未绑定时模式必须0。按当前资源版本生成，装备满耐久，自动找背包空位／叠加并同步。材料level=1的旧调用可回退level=0。满包或发奖失败在已有事务内抛错回滚。不要套用其他版本“第7参数=数量”的调用。
- 实现：[`LuaAddNormalItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3801)。

用法示例：

```lua
local ok, desc = AddNormalItemPile(genre, detail, particular, level, 0, 0,
    {Count=1, Bound=0, BindingMode=0})
```

### `GetItemLevel2`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetItemLevel2(genre, detail, particular)
```

- 参数：三项原物品身份，不传等级。
- 返回：背包中匹配物品最高模板等级；没有返回0。
- 限制／说明：不含装备栏、仓库、鼠标和快捷栏。
- 实现：[`LuaGetItemLevel2`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1110)。

用法示例：

```lua
local level = GetItemLevel2(genre, detail, particular)
```

### `HaveNormalItemInQuick`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
HaveNormalItemInQuick(genre, detail, particular, level)
```

- 参数：四项精确物品模板身份。
- 返回：快捷栏中匹配物品的总数量；否则0。
- 实现：[`LuaHaveNormalItemInQuick`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1096)。

用法示例：

```lua
local count = HaveNormalItemInQuick(genre, detail, particular, level)
```

### `DelNormalItemInQuick`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
DelNormalItemInQuick(genre, detail, particular, level)
```

- 参数：四项精确物品模板身份；固定消耗1个，没有数量参数。
- 返回：成功1，失败0。
- 限制／说明：只扣快捷栏。特定临仙露类消耗前检查互斥与累计时间；此接口失败不主动调用事务拒绝函数，调用者须检查结果并中止业务。
- 实现：[`LuaDelNormalItemInQuick`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1101)。

用法示例：

```lua
local ok = DelNormalItemInQuick(genre, detail, particular, level)
```

### `GetSelectedItems`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetSelectedItems()
```

- 参数：无；必须位于当前有效合成／物品窗口请求回调中。
- 返回：1起始数组，元素含 Genre、Detail、Particular、Level、Count、Bound、BindingMode；无上下文、已完成或物品失效返回nil。
- 限制／说明：下标是本次请求提交槽号，非背包坐标、物品唯一ID或服务器索引。只在这次请求有效，不得跨回调保存作操作凭据。
- 实现：[`LuaGetSelectedItems`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:455)。

用法示例：

```lua
local selected = GetSelectedItems()
```

### `DelSelectedItem`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
DelSelectedItem(slot, count)
```

- 参数：slot：本次提交数组1起始槽号；count：1～100000正整数，且不得超过该槽剩余数。
- 返回：成功1；失败0或在事务内抛错回滚。
- 限制／说明：仅有当前窗口请求、尚未完成且存在物品事务时可调用；精确扣本次提交那一叠，不从其他同名物品补扣。
- 实现：[`LuaDelSelectedItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:491)。

用法示例：

```lua
local ok = DelSelectedItem(1, 5)
```

### `CompleteItemOperation`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
CompleteItemOperation(result)
```

- 参数：result：1为业务成功；0为已完成的概率失败，保留按规则产生的消耗。
- 返回：登记成功1；失败0或抛错回滚。
- 限制／说明：仅本次窗口事务中调用一次；未调用、脚本报错或事务失败均完整回滚当前支持范围；先调用再报错同样回滚。不是任意 Lua 的通用事务提交接口。
- 实现：[`LuaCompleteItemOperation`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:523)。

用法示例：

```lua
CompleteItemOperation(1)
```

### `GetCash`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetCash()
```

- 参数：无。
- 返回：随身现金整数，无角色返回0。
- 限制／说明：不含仓库现金，不是铜钱道具。
- 实现：[`LuaGetPlayerCashMoney`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4348)。

用法示例：

```lua
local cash = GetCash()
```

### `Pay`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
Pay(amount)
```

- 参数：amount：正整数，单位两，扣随身现金。
- 返回：成功1、余额不足0；amount<=0时无返回值。
- 限制／说明：通过原物品列表扣费；发送原金钱同步，由客户端显示原生金钱变化提示；不要再为同一笔扣费调用 `Msg2Player` 或 `PutMessage`，否则会额外出现系统公告或黄色提示。已有事务内现金可回滚；不能仅依赖 if Pay(amount)（数字0在Lua4为真）。
- 实现：[`LuaPlayerPayMoney`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4361)。

用法示例：

```lua
if Pay(200000) ~= 1 then
    return
end
```

### `Earn`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
Earn(amount)
```

- 参数：amount：正整数，单位两。
- 返回：无；包装层不返回底层 AddMoney 结果。
- 限制／说明：增加随身现金并走原同步；达到现金上限等情况不能通过 Lua 返回值识别，涉及奖励事务需先完善通用接口再使用。
- 实现：[`LuaPlayerEarnMoney`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4380)。

用法示例：

```lua
Earn(1000)
```

### `PrePay`

状态：旧接口；预付流程需核对。作用域：游戏／NPC脚本。

```lua
PrePay(amount)
```

- 参数：amount：正整数，单位两。
- 返回：成功1、扣费失败0；非正数无返回。
- 限制／说明：实际立即扣费并记录 m_nPrePayMoney；不是只做余额预检／锁定。旧业务接口，新脚本优先 Pay。
- 实现：[`LuaPlayerPrePayMoney`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4392)。

用法示例：

```lua
local ok = PrePay(1000)
```

### `GetCopperCashCount`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetCopperCashCount()
```

- 参数：无。
- 返回：随身可用铜钱总数，无角色返回0。
- 限制／说明：铜钱为原任务类47的道具，区别于现金、声望、扩展点。
- 实现：[`LuaGetCopperCashCount`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1185)。

用法示例：

```lua
local count = GetCopperCashCount()
```

### `WasteCopperCash`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
WasteCopperCash(amount)
```

- 参数：amount：正整数，且不大于随身铜钱数量。
- 返回：成功1；失败0，已有事务内失败抛错回滚。
- 限制／说明：跨背包铜钱堆扣取并发送实际数额扣费通知。物品可被事务恢复，但该通知直接发出，并未纳入 PutMessage/Msg2Player 延迟队列。
- 实现：[`LuaWasteCopperCash`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1193)。

用法示例：

```lua
local ok = WasteCopperCash(10)
```

### `GetCredit`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetCredit()
```

- 参数：无。
- 返回：TASKVALUE_REPUTE中保存的声望值。
- 限制／说明：当前等同 GetRepute，不是铜钱。
- 实现：[`LuaGetRepute`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:569)。

用法示例：

```lua
local credit = GetCredit()
```

### `DecCredit`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
DecCredit(amount)
```

- 参数：amount：大于0且不超过当前声望的整数。
- 返回：成功1，失败0。
- 限制／说明：修改任务保存值，未自动发送业务提示。
- 实现：[`LuaDecCredit`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1137)。

用法示例：

```lua
local ok = DecCredit(10)
```

### `GetRepute`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetRepute()
```

- 参数：无。
- 返回：声望任务值，取不到角色时0。
- 实现：[`LuaGetRepute`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:569)。

用法示例：

```lua
local repute = GetRepute()
```

### `AddRepute`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
AddRepute(delta)
```

- 参数：delta：声望增量整数，可负。
- 返回：无。
- 限制／说明：加到保存任务值；结果小于0或超过INT_MAX时拒绝，事务中报错回滚。原有接口无返回值，提示由脚本Msg2Player处理。
- 实现：[`LuaModifyRepute`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:542)。

用法示例：

```lua
AddRepute(10)
```

### `ImproveItemMagicAttr`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
ImproveItemMagicAttr(srcGenre, srcDetail, srcParticular, srcLevel,
    dstGenre, dstDetail, dstParticular, dstLevel, reserved, ruleIndex)
```

- 参数：前4项：原装备模板身份；接着4项：目标模板身份；reserved：保留数值，当前未用于业务；ruleIndex：Lua全局 SuitMaterial 的1起始行号。
- 返回：底层 ReforgeItem 结果：-1拒绝、0合法失败、1成功。
- 限制／说明：依赖当前脚本已加载 SuitMaterial、ItemCostIBIndex，校验id1～id4、CostItem和Coppernums；只支持背包中恰好1件匹配装备。重铸扣费／属性仍由既有 C++ ReforgeItem 完成，此项是已存在的业务接口，不能作为新增NPC业务继续堆入C++的范例。
- 实现：[`LuaImproveItemMagicAttr`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1243)。

用法示例：

```lua
local result = ImproveItemMagicAttr(g, d, p, l, g2, d2, p2, l2, 0, ruleIndex)
```

### `AddItem`

状态：旧接口；不推荐新业务。作用域：游戏／NPC脚本。

```lua
AddItem(genre, detail, particular, level, series, luck, magic1[, magic2, ..., magic6])
```

- 参数：前6项：物品生成模板参数；第7项：魔法属性等级；至少12参数时读全部6项，否则六项均使用magic1。
- 返回：生成失败0，生成后1；不是索引。
- 限制／说明：旧接口；背包不足会把物品放鼠标，甚至将原鼠标物品丢地上；未接入新的发奖失败回滚契约。新NPC使用 AddNormalItem。
- 实现：[`LuaAddItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2617)。

用法示例：

```lua
local ok = AddItem(g, d, p, l, 0, 0, magicLevel)
```

### `AddEventItem`

状态：旧接口；优先 AddNormalItem。作用域：游戏／NPC脚本。

```lua
AddEventItem(idOrName)
```

- 参数：idOrName：旧事件物品具体类别ID，或 g_EventItemTab 中名称。
- 返回：成功1、失败0；已有物品事务内失败抛错。
- 限制／说明：旧任务物品接口。事务内按当前版本创建并寻位；事务外仍是旧生成路径与鼠标兜底。不能把普通物品四项模板压成一个ID。
- 实现：[`LuaAddEventItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2517)。

用法示例：

```lua
local ok = AddEventItem(eventId)
```

### `GetItemCount`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetItemCount(idOrName)
```

- 参数：idOrName：旧任务物品具体类别ID或事件表名称。
- 返回：原 GetTaskItemNum 数量，无匹配0。
- 限制／说明：按item_task与GetDetailType匹配并累加所有已登记匹配物品的堆叠数，不过滤背包位置；不是按任意物品服务器索引查数量。
- 实现：[`LuaGetTaskItemCount`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2715)。

用法示例：

```lua
local count = GetItemCount(eventId)
```

### `HaveEventItem`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
HaveEventItem(idOrName)
```

- 参数：idOrName：旧任务物品具体类别ID或事件表名称。
- 返回：原 GetTaskItemNum 数量，无匹配0。
- 限制／说明：与 `GetItemCount` 同一实现、同一参数和返回值。按item_task与GetDetailType匹配并累加所有已登记匹配物品的堆叠数，不过滤背包位置；不是按任意物品服务器索引查数量。
- 实现：[`LuaGetTaskItemCount`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2715)。

用法示例：

```lua
local count = HaveEventItem(eventId)
```

### `HaveItem`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
HaveItem(idOrName)
```

- 参数：idOrName：旧任务物品具体类别ID或事件表名称。
- 返回：有1、无0。
- 限制／说明：只检查旧任务物品；四参数物品查询用 HaveNormalItem。
- 实现：[`LuaHaveItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2804)。

用法示例：

```lua
local exists = HaveItem(eventId)
```

### `DelItem`

状态：旧接口；不推荐新业务。作用域：游戏／NPC脚本。

```lua
DelItem(idOrName)
```

- 参数：idOrName：旧任务物品具体类别ID或事件表名称。
- 返回：无；不返回内部 RemoveTaskItem 结果。
- 限制／说明：按item_task及GetDetailType匹配，遍历已登记物品的首个匹配叠固定扣1，没有count参数；包装层不限制背包位置，也不检查返回值，不适合新的精确材料兑换。新业务用 DelNormalItem 或 DelSelectedItem。
- 实现：[`LuaDelItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2765)。

用法示例：

```lua
DelItem(eventId)
```

### `DelEventItem`

状态：旧接口；不推荐新业务。作用域：游戏／NPC脚本。

```lua
DelEventItem(idOrName)
```

- 参数：idOrName：旧任务物品具体类别ID或事件表名称。
- 返回：无；不返回内部 RemoveTaskItem 结果。
- 限制／说明：与 `DelItem` 同一实现、同一参数和返回值。按item_task及GetDetailType匹配，遍历已登记物品的首个匹配叠固定扣1，没有count参数；包装层不限制背包位置，也不检查返回值，不适合新的精确材料兑换。新业务用 DelNormalItem 或 DelSelectedItem。
- 实现：[`LuaDelItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2765)。

用法示例：

```lua
DelEventItem(eventId)
```

### `DropItem`

状态：不可用：空实现。作用域：游戏／NPC脚本。

```lua
DropItem(...)
```

- 参数：旧注释称 NPC ID和物品ID／名，实际不读取任何参数。
- 返回：无。
- 限制／说明：函数体仅 return 0。
- 实现：[`LuaDropItem`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2510)。

用法示例：

```lua
-- DropItem 当前不产生掉落。
```

## 玩家任务与位、字节操作

### `GetTask`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetTask(taskId)
```

- 参数：taskId：原资源约定的保存任务槽编号，范围按 MAX_TASK。
- 返回：任务槽整数；无玩家nil。
- 限制／说明：任务数据随角色保存；不得随意占用原资源已用的任务ID。
- 实现：[`LuaGetTaskValue`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2054)。

用法示例：

```lua
local step = GetTask(taskId)
```

### `SetTask`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetTask(taskId, value)
```

- 参数：taskId：保存任务槽；value：写入的整数。
- 返回：无。
- 限制／说明：在有物品事务的回调中该任务快照可回滚；不自动替代 TaskNote 的任务栏文本。
- 实现：[`LuaSetTaskValue`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2069)。

用法示例：

```lua
SetTask(taskId, 1)
```

### `GetTaskTemp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetTaskTemp(index)
```

- 参数：index：0起始临时任务槽，小于 MAX_TEMP_TASK。
- 返回：临时任务整数；无上下文或超上限nil。
- 限制／说明：不作持久任务进度，调用者必须保证非负编号。
- 实现：[`LuaGetTempTaskValue`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2086)。

用法示例：

```lua
local state = GetTaskTemp(index)
```

### `SetTaskTemp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetTaskTemp(index, value)
```

- 参数：index：有效临时槽；value：整数。
- 返回：无。
- 限制／说明：使用玩家临时任务值，不是全局变量或存档任务值；调用者先验证槽范围。
- 实现：[`LuaSetTempTaskValue`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2116)。

用法示例：

```lua
SetTaskTemp(index, 1)
```

### `TaskNote`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
TaskNote(taskId, step[, text=""])
```

- 参数：taskId：任务条目编号；step：原任务阶段，负数删除任务条目；text：任务显示文本，长度须小于MAX_TASK_NOTE_TEXT。
- 返回：无，失败写 Task 拒绝日志。
- 限制／说明：同时保存条目并同步任务栏；先校验settings/taskinfo.tbl的Task_ID及Step_n。当前任务3/9/15的step=1也按原仓库许可任务删除条目，其余不自动把最后一步当完成；普通任务需要显式负数阶段删除。示例taskId及阶段必须在原资源存在，不凭空分配ID。
- 实现：[`LuaTaskNote`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1411)。

用法示例：

```lua
TaskNote(taskId, 1, "使用仓库：收集所需材料。")
```

### `GetTaskByte`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetTaskByte(taskId, byteIndex)
```

- 参数：taskId：0～MAX_TASK-1；byteIndex：1～4，低字节为1。
- 返回：指定无符号字节0～255；参数无效0。
- 实现：[`LuaGetTaskByte`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3762)。

用法示例：

```lua
local value = GetTaskByte(taskId, 1)
```

### `SetTaskByte`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetTaskByte(taskId, byteIndex, value)
```

- 参数：taskId：保存任务槽；byteIndex：1～4；value：整数，仅低8位生效。
- 返回：无。
- 限制／说明：保留任务值中其他3个字节。
- 实现：[`LuaSetTaskByte`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3781)。

用法示例：

```lua
SetTaskByte(taskId, 1, 2)
```

### `GetBit`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetBit(value, bitIndex)
```

- 参数：value：整数；bitIndex：1～31，1为最低位。
- 返回：指定bit为1返回1，否则0。
- 限制／说明：当前实现把第32位判无效，返回0。
- 实现：[`LuaGetBit`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:93)。

用法示例：

```lua
local enabled = GetBit(flags, 1)
```

### `SetBit`

状态：可调用；历史逻辑限制。作用域：游戏／NPC脚本。

```lua
SetBit(value, bitIndex, bitValue)
```

- 参数：value：原整数；bitIndex：1～32；bitValue：名义目标位值。
- 返回：按位或置1后的整数。
- 限制／说明：实现计算bitValue但未用：无论传0还是1都只置1，不能清位。不要写 SetBit(v,n,0) 期待清零。
- 实现：[`LuaSetBit`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:108)。

用法示例：

```lua
local flags = SetBit(flags, 1, 1)
```

### `GetByte`

状态：可调用；有符号限制。作用域：游戏／NPC脚本。

```lua
GetByte(value, byteIndex)
```

- 参数：value：32位整数；byteIndex：1～4。
- 返回：指定字节值；无效编号0。
- 限制／说明：旧有符号移位：最高字节含符号位时可能得到负数。任务字节优先 GetTaskByte。
- 实现：[`LuaGetByte`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:125)。

用法示例：

```lua
local part = GetByte(flags, 1)
```

### `SetByte`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetByte(value, byteIndex, byteValue)
```

- 参数：value：32位整数；byteIndex：1～4；byteValue：仅低8位使用。
- 返回：替换对应字节后的整数；非法编号保持原值。
- 实现：[`LuaSetByte`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:141)。

用法示例：

```lua
local flags = SetByte(flags, 2, 3)
```

## 角色查询、属性与身份

### `GetName`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetName()
```

- 参数：无。
- 返回：角色名字符串；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_PlayerName`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerName`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4129)。

用法示例：

```lua
local value = GetName()
```

### `GetUUID`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetUUID()
```

- 参数：无。
- 返回：角色原32位ID数值（不是UUID字符串）；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_dwID`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerID`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4144)。

用法示例：

```lua
local value = GetUUID()
```

### `GetLevel`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetLevel()
```

- 参数：无。
- 返回：角色等级；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_Level`；仅用于有效玩家回调。
- 实现：[`LuaGetLevel`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4597)。

用法示例：

```lua
local value = GetLevel()
```

### `GetSex`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetSex()
```

- 参数：无。
- 返回：原性别枚举值；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_nSex`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerSex`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4095)。

用法示例：

```lua
local value = GetSex()
```

### `GetSeries`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetSeries()
```

- 参数：无。
- 返回：当前项目职业字段值；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_Series`；仅用于有效玩家回调。不要依据旧注释当成性别或随意套用旧剑侠五行编号；职业枚举跟随当前创建角色和原资源。
- 实现：[`LuaGetPlayerSeries`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4106)。

用法示例：

```lua
local value = GetSeries()
```

### `GetPlayerType`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetPlayerType()
```

- 参数：无。
- 返回：当前项目职业字段值；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_Series`；仅用于有效玩家回调。不要依据旧注释当成性别或随意套用旧剑侠五行编号；职业枚举跟随当前创建角色和原资源。
- 实现：[`LuaGetPlayerType`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3888)。

用法示例：

```lua
local value = GetPlayerType()
```

### `GetDefend`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetDefend()
```

- 参数：无。
- 返回：基础防御字段值；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_Defend`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerDefend`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4090)。

用法示例：

```lua
local value = GetDefend()
```

### `GetExp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetExp()
```

- 参数：无。
- 返回：当前经验值；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_nExp`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerExp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3694)。

用法示例：

```lua
local value = GetExp()
```

### `GetLeadExp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetLeadExp()
```

- 参数：无。
- 返回：统率经验；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_dwLeadExp`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerLeadExp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4157)。

用法示例：

```lua
local value = GetLeadExp()
```

### `GetLeadLevel`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetLeadLevel()
```

- 参数：无。
- 返回：统率等级；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_dwLeadLevel`；仅用于有效玩家回调。同名注册两次，后注册的 LuaGetPlayerLeadLevel 生效。
- 实现：[`LuaGetPlayerLeadLevel`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4171)。

用法示例：

```lua
local value = GetLeadLevel()
```

### `GetRestAP`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetRestAP()
```

- 参数：无。
- 返回：剩余属性点；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_nAttributePoint`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerRestAttributePoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4186)。

用法示例：

```lua
local value = GetRestAP()
```

### `GetRestSP`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetRestSP()
```

- 参数：无。
- 返回：剩余技能点；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_nSkillPoint`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerRestSkillPoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4199)。

用法示例：

```lua
local value = GetRestSP()
```

### `GetProp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetProp()
```

- 参数：无。
- 返回：剩余属性点；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_nAttributePoint`；仅用于有效玩家回调。
- 实现：[`LuaGetRestPropPoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6061)。

用法示例：

```lua
local value = GetProp()
```

### `GetMagicPoint`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetMagicPoint()
```

- 参数：无。
- 返回：剩余技能点；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_nSkillPoint`；仅用于有效玩家回调。
- 实现：[`LuaGetMagicPoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5996)。

用法示例：

```lua
local value = GetMagicPoint()
```

### `GetCurCamp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetCurCamp()
```

- 参数：无。
- 返回：当前阵营；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_CurrentCamp`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerCurrentCamp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3277)。

用法示例：

```lua
local value = GetCurCamp()
```

### `GetCamp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetCamp()
```

- 参数：无。
- 返回：基础阵营；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_Camp`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerCamp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3282)。

用法示例：

```lua
local value = GetCamp()
```

### `GetRank`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetRank()
```

- 参数：无。
- 返回：原称号／排名字段ID；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_btRankId`；仅用于有效玩家回调。
- 实现：[`LuaGetRank`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4932)。

用法示例：

```lua
local value = GetRank()
```

### `GetPK`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetPK()
```

- 参数：无。
- 返回：PK值；无上下文时部分旧接口返回nil或0，不能在无玩家的世界脚本中调用。
- 限制／说明：读取 `m_cPK.GetPKValue()`；仅用于有效玩家回调。
- 实现：[`LuaGetPlayerPKValue`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5852)。

用法示例：

```lua
local value = GetPK()
```

### `GetLucky`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetLucky(kind)
```

- 参数：kind：0读取当前 Lucky；1或2读取基础 Lucky，1与2当前相同。
- 返回：角色幸运整数，非法kind为nil；须有效玩家。
- 限制／说明：对应 m_nCurLucky / m_nLucky；旧注释不可靠，以属性同步字段和新资源解释为准。
- 实现：[`LuaGetPlayerLucky`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4232)。

用法示例：

```lua
local value = GetLucky(0)
```

### `GetEng`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetEng(kind)
```

- 参数：kind：0读取当前 Engergy；1或2读取基础 Engergy，1与2当前相同。
- 返回：内功／能量字段整数，非法kind为nil；须有效玩家。
- 限制／说明：对应 m_nCurEngergy / m_nEngergy；旧注释不可靠，以属性同步字段和新资源解释为准。
- 实现：[`LuaGetPlayerEngergy`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4255)。

用法示例：

```lua
local value = GetEng(0)
```

### `GetDex`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetDex(kind)
```

- 参数：kind：0读取当前 Dexterity；1或2读取基础 Dexterity，1与2当前相同。
- 返回：敏捷字段整数，非法kind为nil；须有效玩家。
- 限制／说明：对应 m_nCurDexterity / m_nDexterity；旧注释不可靠，以属性同步字段和新资源解释为准。
- 实现：[`LuaGetPlayerDexterity`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4278)。

用法示例：

```lua
local value = GetDex(0)
```

### `GetStrg`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetStrg(kind)
```

- 参数：kind：0读取当前 Strength；1或2读取基础 Strength，1与2当前相同。
- 返回：力量字段整数，非法kind为nil；须有效玩家。
- 限制／说明：对应 m_nCurStrength / m_nStrength；旧注释不可靠，以属性同步字段和新资源解释为准。
- 实现：[`LuaGetPlayerStrength`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4301)。

用法示例：

```lua
local value = GetStrg(0)
```

### `GetVit`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetVit(kind)
```

- 参数：kind：0读取当前 Vitality；1或2读取基础 Vitality，1与2当前相同。
- 返回：体质字段整数，非法kind为nil；须有效玩家。
- 限制／说明：对应 m_nCurVitality / m_nVitality；旧注释不可靠，以属性同步字段和新资源解释为准。
- 实现：[`LuaGetPlayerVitality`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4324)。

用法示例：

```lua
local value = GetVit(0)
```

### `GetLife`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetLife(kind)
```

- 参数：kind：0=生命当前值；1=当前生效生命上限；2=基础生命上限。
- 返回：对应整数；无玩家或非法kind返回nil。
- 限制／说明：显式传kind，不按旧注释省略参数。
- 实现：[`LuaGetPlayerLife`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3965)。

用法示例：

```lua
local current, maximum = GetLife(0), GetLife(1)
```

### `GetMana`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetMana(kind)
```

- 参数：kind：0=内力当前值；1=当前生效内力上限；2=基础内力上限。
- 返回：对应整数；无玩家或非法kind返回nil。
- 限制／说明：显式传kind，不按旧注释省略参数。
- 实现：[`LuaGetPlayerMana`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4030)。

用法示例：

```lua
local current, maximum = GetMana(0), GetMana(1)
```

### `GetStamina`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetStamina(kind)
```

- 参数：kind：0=体力当前值；1=体力上限；2=体力上限。
- 返回：对应整数；无玩家或非法kind返回nil。
- 限制／说明：显式传kind，不按旧注释省略参数。
- 实现：[`LuaGetPlayerStamina`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4057)。

用法示例：

```lua
local current, maximum = GetStamina(0), GetStamina(1)
```

### `RestoreLife`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
RestoreLife()
```

- 参数：无。
- 返回：无。
- 限制／说明：恢复生命至原 Restore 函数上限；生命／内力接口随后发送当前数值同步，体力包装层没有对应同步调用。
- 实现：[`LuaRestorePlayerLife`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3992)。

用法示例：

```lua
RestoreLife()
```

### `RestoreMana`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
RestoreMana()
```

- 参数：无。
- 返回：无。
- 限制／说明：恢复内力至原 Restore 函数上限；生命／内力接口随后发送当前数值同步，体力包装层没有对应同步调用。
- 实现：[`LuaRestorePlayerMana`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4003)。

用法示例：

```lua
RestoreMana()
```

### `RestoreStamina`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
RestoreStamina()
```

- 参数：无。
- 返回：无。
- 限制／说明：恢复体力至原 Restore 函数上限；生命／内力接口随后发送当前数值同步，体力包装层没有对应同步调用。
- 实现：[`LuaRestorePlayerStamina`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4014)。

用法示例：

```lua
RestoreStamina()
```

### `GetColdR`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetColdR(kind)
```

- 参数：kind：0当前抗性；1基础抗性；2抗性上限。
- 返回：对应抗性整数；无玩家或非法kind为nil。
- 限制／说明：分别是冰、火、电、毒、物理抗性中的对应一种；读取原抗性字段。
- 实现：[`LuaGetPlayerColdResist`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3406)。

用法示例：

```lua
local value = GetColdR(0)
```

### `GetFireR`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetFireR(kind)
```

- 参数：kind：0当前抗性；1基础抗性；2抗性上限。
- 返回：对应抗性整数；无玩家或非法kind为nil。
- 限制／说明：分别是冰、火、电、毒、物理抗性中的对应一种；读取原抗性字段。
- 实现：[`LuaGetPlayerFireResist`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3460)。

用法示例：

```lua
local value = GetFireR(0)
```

### `GetLightR`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetLightR(kind)
```

- 参数：kind：0当前抗性；1基础抗性；2抗性上限。
- 返回：对应抗性整数；无玩家或非法kind为nil。
- 限制／说明：分别是冰、火、电、毒、物理抗性中的对应一种；读取原抗性字段。
- 实现：[`LuaGetPlayerLightResist`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3518)。

用法示例：

```lua
local value = GetLightR(0)
```

### `GetPoisonR`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetPoisonR(kind)
```

- 参数：kind：0当前抗性；1基础抗性；2抗性上限。
- 返回：对应抗性整数；无玩家或非法kind为nil。
- 限制／说明：分别是冰、火、电、毒、物理抗性中的对应一种；读取原抗性字段。
- 实现：[`LuaGetPlayerPoisonResist`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3573)。

用法示例：

```lua
local value = GetPoisonR(0)
```

### `GetPhyR`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetPhyR(kind)
```

- 参数：kind：0当前抗性；1基础抗性；2抗性上限。
- 返回：对应抗性整数；无玩家或非法kind为nil。
- 限制／说明：分别是冰、火、电、毒、物理抗性中的对应一种；读取原抗性字段。
- 实现：[`LuaGetPlayerPhysicsResist`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3630)。

用法示例：

```lua
local value = GetPhyR(0)
```

### `SetFireR`

状态：旧接口；属性重算与同步需核对。作用域：游戏／NPC脚本。

```lua
SetFireR(kind, value)
```

- 参数：kind：0当前值、1基础值、2上限；value：整数，先截到0～调用前的抗性上限。
- 返回：无。
- 限制／说明：直接写旧抗性字段，包装层不触发完整重算、存档或客户端全量同步；修改上限也受旧上限约束。
- 实现：[`LuaSetPlayerFireResist`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3484)。

用法示例：

```lua
SetFireR(0, value)
```

### `SetLightR`

状态：旧接口；属性重算与同步需核对。作用域：游戏／NPC脚本。

```lua
SetLightR(kind, value)
```

- 参数：kind：0当前值、1基础值、2上限；value：整数，先截到0～调用前的抗性上限。
- 返回：无。
- 限制／说明：直接写旧抗性字段，包装层不触发完整重算、存档或客户端全量同步；修改上限也受旧上限约束。
- 实现：[`LuaSetPlayerLightResist`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3541)。

用法示例：

```lua
SetLightR(0, value)
```

### `SetPoisonR`

状态：旧接口；属性重算与同步需核对。作用域：游戏／NPC脚本。

```lua
SetPoisonR(kind, value)
```

- 参数：kind：0当前值、1基础值、2上限；value：整数，先截到0～调用前的抗性上限。
- 返回：无。
- 限制／说明：直接写旧抗性字段，包装层不触发完整重算、存档或客户端全量同步；修改上限也受旧上限约束。
- 实现：[`LuaSetPlayerPoisonResist`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3596)。

用法示例：

```lua
SetPoisonR(0, value)
```

### `SetPhyR`

状态：旧接口；属性重算与同步需核对。作用域：游戏／NPC脚本。

```lua
SetPhyR(kind, value)
```

- 参数：kind：0当前值、1基础值、2上限；value：整数，先截到0～调用前的抗性上限。
- 返回：无。
- 限制／说明：直接写旧抗性字段，包装层不触发完整重算、存档或客户端全量同步；修改上限也受旧上限约束。
- 实现：[`LuaSetPlayerPhysicsResist`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3659)。

用法示例：

```lua
SetPhyR(0, value)
```

### `SetColdR`

状态：不可用作设置：错绑定。作用域：游戏／NPC脚本。

```lua
SetColdR(kind[, ignoredValue])
```

- 参数：kind：0当前、1基础、2上限；第二参数被忽略。
- 返回：返回冰抗值，行为与 GetColdR 相同。
- 限制／说明：注册表错误绑定 LuaGetPlayerColdResist；本轮只记录，不修业务代码。
- 实现：[`LuaGetPlayerColdResist`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3406)。

用法示例：

```lua
-- SetColdR(0, 50) 不会设置冰抗。
```

### `SetCurCamp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetCurCamp(camp)
```

- 参数：camp：非负原阵营枚举。
- 返回：无。
- 限制／说明：设置当前阵营；走 KNpc 对应设置方法，不要套用其他项目的阵营枚举。
- 实现：[`LuaSetPlayerCurrentCamp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3300)。

用法示例：

```lua
SetCurCamp(camp)
```

### `SetCamp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetCamp(camp)
```

- 参数：camp：非负原阵营枚举。
- 返回：无。
- 限制／说明：设置基础阵营；走 KNpc 对应设置方法，不要套用其他项目的阵营枚举。
- 实现：[`LuaSetPlayerCamp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3287)。

用法示例：

```lua
SetCamp(camp)
```

### `RestoreCamp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
RestoreCamp()
```

- 参数：无。
- 返回：无。
- 限制／说明：恢复角色当前阵营到基础阵营。
- 实现：[`LuaRestorePlayerCamp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3323)。

用法示例：

```lua
RestoreCamp()
```

### `GetFaction`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetFaction()
```

- 参数：无。
- 返回：原门派名字符串；无玩家为空字符串。
- 限制／说明：旧门派系统字段，不能当当前角色职业名称使用。
- 实现：[`LuaGetPlayerFaction`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3367)。

用法示例：

```lua
local faction = GetFaction()
```

### `SetFaction`

状态：旧接口；非原子变更。作用域：游戏／NPC脚本。

```lua
SetFaction(name)
```

- 参数：name：门派名称字符串，空字符串表示离开当前门派。
- 返回：原加入门派结果，空字符串退出返回1。
- 限制／说明：先退出当前门派再尝试加入，失败不保证恢复原门派；不是可原子回滚的职业变更接口。
- 实现：[`LuaChangePlayerFaction`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3383)。

用法示例：

```lua
local ok = SetFaction(name)
```

### `SetSeries`

状态：旧接口；完整转职待适配。作用域：游戏／NPC脚本。

```lua
SetSeries(series)
```

- 参数：series：原整数枚举。
- 返回：无。
- 限制／说明：只走 KNpc::SetSeries；不要用作性别设置。缺少完整职业变更契约，不直接用于当前NPC转职业务。
- 实现：[`LuaSetPlayerSeries`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4111)。

用法示例：

```lua
-- 确认当前职业与外观、技能及存档联动后才能使用 SetSeries(series)。
```

### `SetRank`

状态：旧接口；新版称号流程待核对。作用域：游戏／NPC脚本。

```lua
SetRank(rankId)
```

- 参数：rankId：转换为BYTE的原称号／排名ID。
- 返回：无。
- 限制／说明：直接修改 m_btRankId；没有包装层同步和持久化保证。
- 实现：[`LuaSetRank`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4941)。

用法示例：

```lua
SetRank(rankId)
```

### `AddOwnExp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
AddOwnExp(amount)
```

- 参数：amount：非负整数，角色直接经验奖励。
- 返回：无。
- 限制／说明：已有物品事务内先累计，最外层提交后发放；溢出拒绝回滚，负数不处理。任务奖励优先此接口。
- 实现：[`LuaAddOwnExp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3726)。

用法示例：

```lua
AddOwnExp(100)
```

### `AddExp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
AddExp(amount, targetLevel[, shareTeam=0])
```

- 参数：amount：经验值；targetLevel：经验来源目标等级；shareTeam：非0调用队伍分配，否则仅自身。
- 返回：无。
- 限制／说明：走旧战斗经验分配及等级修正，不是固定任务经验；未采用 AddOwnExp 的事务延迟方式。
- 实现：[`LuaModifyPlayerExp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3707)。

用法示例：

```lua
AddExp(100, 5, 0)
```

### `AddLeadExp`

状态：旧接口；新版统率流程待核对。作用域：游戏／NPC脚本。

```lua
AddLeadExp(amount)
```

- 参数：amount：统率经验整数。
- 返回：无。
- 限制／说明：调用原统率经验逻辑；其新版客户端表现须单独验证。
- 实现：[`LuaAddLeadExp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5952)。

用法示例：

```lua
AddLeadExp(10)
```

### `AddMagicPoint`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
AddMagicPoint(delta)
```

- 参数：delta：剩余技能点增量，可负，结果低于0归0。
- 返回：无。
- 限制／说明：只修改 m_nSkillPoint；不是技能熟练度，包装层无即时完整同步。
- 实现：[`LuaAddMagicPoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5979)。

用法示例：

```lua
AddMagicPoint(1)
```

### `AddProp`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
AddProp(delta)
```

- 参数：delta：剩余属性点增量整数。
- 返回：无。
- 限制／说明：直接加 m_nAttributePoint，无防负数或溢出校验，包装层无属性同步。
- 实现：[`LuaAddPropPoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6008)。

用法示例：

```lua
AddProp(1)
```

### `GetExtPoint`

状态：旧接口；保留参数怪癖。作用域：游戏／NPC脚本。

```lua
GetExtPoint(dummy)
```

- 参数：dummy：当前实现要求至少传1个参数，但不读取该值，兼容传0。
- 返回：原扩展点余额；不传参数则直接0。
- 限制／说明：不是铜钱或现金；依赖旧扩展点业务。
- 实现：[`LuaGetExtPoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6043)。

用法示例：

```lua
local value = GetExtPoint(0)
```

### `PayExtPoint`

状态：旧接口；扩展点流程待核对。作用域：游戏／NPC脚本。

```lua
PayExtPoint(amount)
```

- 参数：amount：非负整数扩展点。
- 返回：底层扣点结果；无效上下文／负数0。
- 限制／说明：不用于铜钱支付。
- 实现：[`LuaPayExtPoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6021)。

用法示例：

```lua
local ok = PayExtPoint(amount)
```

## 技能与限时效果

### `IsSkillActived`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
IsSkillActived(skillId)
```

- 参数：skillId：原技能ID。
- 返回：已学习1，否则0。
- 限制／说明：检查 IsPlayerSkillLearned，不是当前是否选中技能。
- 实现：[`LuaIsSkillActived`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:964)。

用法示例：

```lua
local learned = IsSkillActived(skillId)
```

### `IsExistBuff`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
IsExistBuff(buffId)
```

- 参数：buffId：当前技能／技能熟练度效果ID。
- 返回：剩余时间大于0返回1，否则0。
- 限制／说明：只查询 m_SkillBuffs 中的技能相关限时效果，不涵盖任意战斗状态。
- 实现：[`LuaIsExistBuff`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:972)。

用法示例：

```lua
local active = IsExistBuff(107)
```

### `HaveIBBuff`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
HaveIBBuff(buffId)
```

- 参数：buffId：当前技能／技能熟练度效果ID。
- 返回：剩余时间大于0返回1，否则0。
- 限制／说明：与 `IsExistBuff` 同一实现、同一参数和返回值。只查询 m_SkillBuffs 中的技能相关限时效果，不涵盖任意战斗状态。
- 实现：[`LuaIsExistBuff`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:972)。

用法示例：

```lua
local active = HaveIBBuff(107)
```

### `GetIBBuffLeftTimes`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetIBBuffLeftTimes(buffId)
```

- 参数：buffId：技能相关限时效果ID。
- 返回：剩余秒数，未生效0。
- 限制／说明：按系统time(NULL)计算；不是剩余使用次数。
- 实现：[`LuaGetIBBuffLeftTimes`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:980)。

用法示例：

```lua
local seconds = GetIBBuffLeftTimes(107)
```

### `AddBuff`

状态：实现已核对，太岁新增状态待客户端实测。作用域：游戏／NPC脚本；道具事务白名单未开放。

```lua
AddBuff(buffId, seconds, firstValue, secondValue, reserved)
```

- 参数：必须5项；seconds为1～360000秒。reserved保留第5位置，当前不读取。时间按同种状态替换，不累计。
- buff5：firstValue为打怪经验倍率1～5；不会倍增`AddOwnExp`直接奖励。
- buff12：firstValue为技能熟练增幅百分比1～1000，原太岁传100，即翻倍；与临仙露目前乘算，组合规则尚无正式服样本。
- buff13／14：firstValue为自然恢复周期附加内力／生命点数1～32767；不修改基础属性，避免穿脱装备或刷新状态重复累加。
- buff7／23：firstValue为全技能加级值1～5；buff8：firstValue为已学技能ID、secondValue为加级值1～5。当前技能等级增益总和仍封顶+2。
- 返回：成功1、失败0；事务中失败会报错中止并回滚。事务中的状态先变更服务器记录，提交后刷新属性及E5/F7，失败恢复原记录。
- 保存：经典状态按绝对到期时间，退出期间仍计时；临仙露保持原配置的在线计时规则。
- 实现：`Sources/Core/Src/ScriptFuns.cpp::LuaAddBuff`、`KSkillList.cpp`。

```lua
assert(AddBuff(5, 3600, 2, 0, 1) == 1)
assert(AddBuff(8, 3600, skillId, 1, 0) == 1)
```

### `AddIBBuff`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
AddIBBuff(buffId)
```

- 参数：buffId：有原 PLAYER_SKILL_BUFF_RULE 的技能倍率效果ID；当前临仙露组107/271/272。
- 返回：成功1，失败0。
- 限制／说明：持续时间和倍率取原规则；同组不同ID互斥，同ID累计时间且受上限限制。不会扣消耗品；脚本先检查并按原消耗流程处理。
- 实现：[`LuaAddIBBuff`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1049)。

用法示例：

```lua
local ok = AddIBBuff(107)
```

### `RemoveBuff`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
RemoveBuff(buffId)
```

- 参数：buffId：技能相关限时效果ID。
- 返回：成功移除1，未找到／无角色0。
- 限制／说明：只删除该技能效果列表条目，触发技能等级刷新及Buff同步，不是任意战斗状态删除器。
- 实现：[`LuaRemoveBuff`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:988)。

用法示例：

```lua
local ok = RemoveBuff(107)
```

### `AddMagic`

状态：旧接口；完整学习流程需核对。作用域：游戏／NPC脚本。

```lua
AddMagic(skillIdOrName[, level=0])
```

- 参数：skillIdOrName：技能ID或 g_OrdinSkillsSetting 中名称；level：加入等级，默认0。
- 返回：无。
- 限制／说明：旧技能列表入口，成功发送旧单技能同步；不等同于当前技能书使用的全套资格、消耗和熟练度流程。新学习业务须复用／扩展通用学习接口。
- 实现：[`LuaAddMagic`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2853)。

用法示例：

```lua
AddMagic(skillId, 1)
```

### `DelMagic`

状态：旧接口；删除后联动需核对。作用域：游戏／NPC脚本。

```lua
DelMagic(skillIdOrName)
```

- 参数：技能ID或原技能表名称。
- 返回：无。
- 限制／说明：直接移除技能；包装层未完整同步客户端或修复新技能学习状态。
- 实现：[`LuaDelMagic`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2892)。

用法示例：

```lua
DelMagic(skillId)
```

### `HaveMagic`

状态：可调用；名称分支有缺陷。作用域：游戏／NPC脚本。

```lua
HaveMagic(skillId)
```

- 参数：skillId：推荐仅传数字技能ID。
- 返回：存在返回技能等级，不存在-1；不是0/1。
- 限制／说明：字符串分支有提前return，返回不可靠，不使用名称查询。等级0和未学习也须结合 IsSkillActived 区分。
- 实现：[`LuaHaveMagic`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2920)。

用法示例：

```lua
local level = HaveMagic(skillId)
```

### `GetMagicLevel`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetMagicLevel(skillIdOrName)
```

- 参数：技能ID或原技能表中名称。
- 返回：技能列表等级；名称无效或无角色时无返回值。
- 限制／说明：原 GetLevel 与临时 Buff 后的 CurrentLevel 不同；不代表熟练度。
- 实现：[`LuaGetMagicLevel`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3022)。

用法示例：

```lua
local level = GetMagicLevel(skillId)
```

### `GetSkillId`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetSkillId(listIndex)
```

- 参数：listIndex：技能列表中的有效1起始内部槽号。
- 返回：该槽技能ID；无效通常0。
- 限制／说明：不是按角色等级查技能或物品技能书编号。
- 实现：[`LuaGetSkillIdInSkillList`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2958)。

用法示例：

```lua
local skillId = GetSkillId(1)
```

### `SetSkillLevel`

状态：旧接口；新技能成长需核对。作用域：游戏／NPC脚本。

```lua
SetSkillLevel(skillIdOrName, level)
```

- 参数：技能ID或名称；level：非负技能等级。
- 返回：无。
- 限制／说明：直接设置技能列表等级，不执行技能书条件、熟练度扣除或完整学习状态维护。
- 实现：[`LuaSetSkillLevel`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2976)。

用法示例：

```lua
SetSkillLevel(skillId, level)
```

### `RollbackSkill`

状态：旧接口；新版洗点待适配。作用域：游戏／NPC脚本。

```lua
RollbackSkill()
```

- 参数：无。
- 返回：ClearSkillLevel(true)返回的总点数。
- 限制／说明：清空技能等级旧流程；不自动把返回点数返还给角色，不能未经适配用于当前洗点。
- 实现：[`LuaRollBackSkills`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3002)。

用法示例：

```lua
local points = RollbackSkill()
```

### `UpdateSkill`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
UpdateSkill()
```

- 参数：无。
- 返回：无。
- 限制／说明：发送当前角色技能全量同步，不改变技能、熟练度或技能点。
- 实现：[`LuaUpdateSkillList`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3013)。

用法示例：

```lua
UpdateSkill()
```

## 地图、坐标、NPC与复活

### `GetWorldPos`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetWorldPos()
```

- 参数：无。
- 返回：mapId, x, y；无玩家返回0,0,0。
- 限制／说明：返回地图资源ID，坐标为MPS/32的整数，可配合NewWorld／SetPos。
- 实现：[`LuaGetNewWorldPos`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2478)。

用法示例：

```lua
local mapId, x, y = GetWorldPos()
```

### `GetUseItemInfo`

状态：已实现，2026-11-10原生及协议回归接口。作用域：有效道具脚本入口及其对白回调；普通NPC／世界脚本无有效使用实例时返回nil。

```lua
local item = GetUseItemInfo()
if item ~= nil then
    Msg2Player(format("本叠剩余%d张。", item.Count))
end
```

- 参数：无。
- 返回：本次实际使用道具的表，字段 `Genre`、`Detail`、`Particular`、`Level` 为原模板身份，`Count` 为当前堆叠数；没有有效实例时返回nil。
- 限制：只读取本次使用的确切实例，不按同名模板改扣另一叠。每次调用检查玩家已进图、存活、不交易、不跨服，仍在打开菜单时的源世界，原实例仍在背包／快捷栏，且模板脚本仍与本次脚本一致。原实例移到背包／快捷栏之外、卖出、用完，或当前不在源世界时，上下文无效。
- 实现：`ScriptFuns.cpp::LuaGetUseItemInfo`、`KPlayer.cpp::FindUseScriptItem`。

### `ConsumeUseItem`

状态：已实现。作用域：C++建立的道具自动事务；只接受当前使用上下文的真实物品。

```lua
local item = GetUseItemInfo() -- 消耗前读取，最后一个消失后该查询返回nil
assert(ConsumeUseItem() == 1) -- 默认消耗本次右键实例1个
assert(AddNormalItemPile(6, 1, 506, 1, 0, 0, {Count=50}) == 1)
-- 也可 ConsumeUseItem(count)，从该实例消耗指定数量，不从同名其他物品补扣。
```

- 参数：可省略的 `count`，1～100000的正整数，不能超过当前实例数量。
- 返回：成功1；无事务返回0；已有事务中任何失败抛错、标记失败并回滚。
- 限制：本次使用来源必须仍在背包／快捷栏、模板脚本匹配、角色存活且不在交易或跨服中。模板“使用后消失”为1的道具已由C++自动扣除，禁止再显式扣。一次事务仅允许一次，不能与 `TeleportWithItem` 混用。普通NPC／世界脚本无有效道具上下文不能扣物品。
- 同步与回滚：精确消耗本次实例并立即同步，释放的原格可以用于发奖；Lua失败或入包失败由外层事务完整恢复本次支持范围并同步，成功走原存档链，Lua不操作数据库。先消耗再奖励时须检查两步结果，成功提示使用事务延迟的 `Msg2Player`。
- 实现：`ScriptFuns.cpp::LuaConsumeUseItem`、`KItemList::ConsumeItem`、`KNpcScriptTransaction`。

### `TeleportWithItem`

状态：已实现，受限延迟传送。作用域：C++建立的道具自动事务；只接受本次有效使用实例。其他NPC、世界或无事务调用返回0。

```lua
-- 立即返回设置重生点；没有本次有效道具时不会传送。
if TeleportWithItem() ~= 1 then
    Msg2Player("当前无法回城。")
end
-- 或在目的地菜单回调中指定地图和脚本格坐标：
-- TeleportWithItem(3, 1692, 3138, 0)
```

- 形式1：`TeleportWithItem()`，读取角色当前死亡重生点的地图及精确MPS坐标，成功后设为非战斗状态，不生成回程门。
- 形式2：`TeleportWithItem(mapId, x, y, fightMode[, revivalId])`。`mapId` 为本服已加载地图；`x/y` 为脚本格（底层乘32），`fightMode` 只能0或1。`revivalId`省略或0表示保留原重生点；正值要求原 `settings/RevivePos.ini` 中该地图下存在这一编号，传送成功后才更新重生点。
- 数字参数必须是0～67108863的整数，目的地图和区域必须有效；不支持转交其他GameServer。调用失败返回0且不扣符；成功返回1只表示**已接受计划**，此时尚未切图或扣符。同一次事务只允许一个传送计划，不能与 `ConsumeUseItem` 混用。
- 最外层Lua成功完成并提交后，C++再次校验实例和目的地，扣除本次实例1张，复用当前 `KNpc::ChangeWorld` 及49／F3同步链，再应用战斗状态及可选重生点。取消菜单、无效参数、重复回调、Lua报错或事务未提交均不执行计划；失败恢复本次原有物品、现金、任务和宠物快照。
- 不要再调用 `DelNormalItem` 预扣该张符，不要同时用 `NewWorld/SetPos/SetFightState/SetRevPos` 处理传送。模板“使用后是否消失”需为0，由成功提交统一扣本次实例。它不提供任意位置／Buff／技能／外部IO的通用回滚。
- 实现：`ScriptFuns.cpp::LuaTeleportWithItem`、`KNpcScriptTransaction`。

### `GetPos`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetPos()
```

- 参数：无。
- 返回：mpsX, mpsY, subWorldIndex；无玩家无返回。
- 限制／说明：坐标为MPS原值，第三值是运行时世界索引；与GetWorldPos顺序和单位均不同。不可直接 NewWorld(GetPos())。
- 实现：[`LuaGetPos`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2459)。

用法示例：

```lua
local mpsX, mpsY, worldIndex = GetPos()
```

### `SetPos`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetPos(x, y)
```

- 参数：x/y：当前地图脚本格坐标，内部各乘32。
- 返回：无。
- 限制／说明：只改当前地图位置；须原地图合法落点，避免落在障碍中。
- 实现：[`LuaSetPos`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2443)。

用法示例：

```lua
SetPos(x, y)
```

### `NewWorld`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
NewWorld(mapId, x, y)
```

- 参数：mapId：地图资源ID；x/y：脚本格坐标，内部各乘32。
- 返回：KNpc::ChangeWorld的结果值；无效角色可能无返回。
- 限制／说明：走已有切图和同步；不负责扣传送费、业务资格或目的地配置。
- 实现：[`LuaEnterNewWorld`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2426)。

用法示例：

```lua
local ok = NewWorld(mapId, x, y)
```

### `SubWorldID2Idx`

状态：实现已核对。作用域：游戏／NPC脚本、世界脚本。

```lua
SubWorldID2Idx(mapId)
```

- 参数：mapId：地图资源ID。
- 返回：本服务器已加载的运行时世界索引；未找到-1。
- 限制／说明：不能混用资源mapId与运行时worldIndex。
- 实现：[`LuaSubWorldIDToIndex`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:602)。

用法示例：

```lua
local worldIndex = SubWorldID2Idx(mapId)
```

### `GetNpcIdx`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetNpcIdx()
```

- 参数：无。
- 返回：当前玩家自身对应的NPC运行时索引。
- 限制／说明：不是当前对话NPC索引，也不是网络DWORD ID；只能在有效玩家上下文调用。
- 实现：[`LuaGetCurNpcIndex`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5869)。

用法示例：

```lua
local selfNpcIndex = GetNpcIdx()
```

### `AddNpc`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
AddNpc(templateIdOrName, level, subWorldIndex, mpsX, mpsY)
```

- 参数：模板ID或 g_NpcSetting 的名称；level：等级；subWorldIndex：已加载世界的运行时索引；mpsX/mpsY：原始点坐标。
- 返回：NpcSet.Add返回的运行时NPC索引；失败按底层结果判定，不保证nil。
- 限制／说明：5参数；不读取旧注释中省略level的形式。内部将等级和模板MAKELONG组合。不要用该接口替代已规定的原生地图点位资源。
- 实现：[`LuaAddNpc`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3116)。

用法示例：

```lua
local npcIndex = AddNpc(templateId, level, worldIndex, mpsX, mpsY)
```

### `DelNpc`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
DelNpc(npcIndex)
```

- 参数：npcIndex：本服务器有效NPC运行时索引。
- 返回：无，旧注释称1/0但实际未返回。
- 限制／说明：从区域和NpcSet移除；调用者保证非玩家NPC且索引在范围内，包装层只有>0检查。
- 实现：[`LuaDelNpc`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3151)。

用法示例：

```lua
DelNpc(npcIndex)
```

### `SetNpcScript`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetNpcScript(npcIndex, path)
```

- 参数：npcIndex：有效NPC运行时索引；path：已有动作脚本资源路径字符串。
- 返回：无。
- 限制／说明：写 ActionScript 及路径哈希；只用原有短路径，包装层strcpy没有显式长度保护。
- 实现：[`LuaSetNpcActionScript`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3214)。

用法示例：

```lua
SetNpcScript(npcIndex, scriptPath)
```

### `SetNpcCurCamp`

状态：可调用；索引校验有缺陷。作用域：游戏／NPC脚本。

```lua
SetNpcCurCamp(npcIndex, camp)
```

- 参数：npcIndex：有效NPC运行时索引；camp：0～camp_num-1的原阵营。
- 返回：无。
- 限制／说明：原边界条件把AND写成了不可能同时成立的形式；调用者必须验证索引及camp非负。
- 实现：[`LuaSetNpcCurCamp`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3313)。

用法示例：

```lua
SetNpcCurCamp(npcIndex, camp)
```

### `SetRevPos`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetRevPos([mapId,] revivalId)
```

- 参数：两个参数时为地图资源ID和该地图重生点ID；一个参数时地图ID=-1，由原角色方法解析。
- 返回：无。
- 限制／说明：不是x/y坐标。引用原地图重生点配置，持久重生地由原角色存档流程保存。
- 实现：[`LuaSetPlayerRevivalPos`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3228)。

用法示例：

```lua
SetRevPos(mapId, revivalId)
```

### `SetTempRevPos`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetTempRevPos(mapId, mpsX, mpsY) 或 SetTempRevPos(revivalId)
```

- 参数：3参数：地图ID和未乘32的MPS坐标；1参数：当前地图原重生点ID。
- 返回：无。
- 限制／说明：修改 GetDeathRevivalPos 临时死亡重生点；不同于永久 SetRevPos，2参数形式无效。
- 实现：[`LuaSetDeathRevivalPos`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5362)。

用法示例：

```lua
SetTempRevPos(mapId, mpsX, mpsY)
```

### `SetDeathScript`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetDeathScript(path)
```

- 参数：path：已有玩家死亡脚本资源路径。
- 返回：无。
- 限制／说明：记录文件哈希；不自动创建脚本或补全当前复活协议。
- 实现：[`LuaSetPlayerDeathScript`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5636)。

用法示例：

```lua
SetDeathScript(scriptPath)
```

### `SetLogoutRV`

状态：旧接口；新版策略需核对。作用域：游戏／NPC脚本。

```lua
SetLogoutRV(state)
```

- 参数：state：0设置LoginType=0，非0设置LoginType=1。
- 返回：无。
- 限制／说明：旧登录／重生策略标志，语义按KPlayer::SetLoginType及登录流程核实，不能当立即复活。
- 实现：[`LuaSetPlayerRevivalOptionWhenLogout`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5819)。

用法示例：

```lua
SetLogoutRV(0)
```

### `UseTownPortal`

状态：旧入口；使用条件由底层决定。作用域：游戏／NPC脚本。

```lua
UseTownPortal()
```

- 参数：无。
- 返回：无。
- 限制／说明：调用当前玩家原 UseTownPortal；不是支付／消耗品封装，不在Lua任意重复扣回城符。不在道具事务白名单内；当前回城类道具改用 `TeleportWithItem()`。
- 实现：[`LuaUseTownPortal`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4607)。

用法示例：

```lua
UseTownPortal()
```

### `ReturnFromPortal`

状态：旧入口；返回流程需核对。作用域：游戏／NPC脚本。

```lua
ReturnFromPortal()
```

- 参数：无。
- 返回：无。
- 限制／说明：调用旧 BackToTownPortal，依赖先前保存的回城门上下文。
- 实现：[`LuaReturnFromTownPortal`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4618)。

用法示例：

```lua
ReturnFromPortal()
```

### `SetFightState`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetFightState(state)
```

- 参数：state：0非战斗，非0战斗。
- 返回：无。
- 限制／说明：调用原KNpc::SetFightMode。
- 实现：[`LuaSetFightState`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4577)。

用法示例：

```lua
SetFightState(0)
```

### `GetFightState`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetFightState()
```

- 参数：无。
- 返回：当前m_FightMode数值，无有效角色时无返回。
- 实现：[`LuaGetFightState`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4587)。

用法示例：

```lua
local state = GetFightState()
```

### `AttackNpc`

状态：旧接口；伤害协议与属性待适配。作用域：游戏／NPC脚本。

```lua
AttackNpc(npcDwordId, physical[, cold, fire, light, poison])
```

- 参数：npcDwordId：玩家附近目标的网络DWORD ID；其余为旧伤害数值参数。
- 返回：无。
- 限制／说明：包装层未设置各追加伤害的nAttribType，且无充分参数个数上限；不是已适配的通用技能伤害API。
- 实现：[`LuaAttackNpc`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4418)。

用法示例：

```lua
-- 旧入口 AttackNpc(targetId, damage)，新战斗业务先核实伤害结构。
```

### `KillNpc`

状态：旧接口；死亡流程待适配。作用域：游戏／NPC脚本。

```lua
KillNpc(npcDwordId)
```

- 参数：npcDwordId：当前玩家附近目标网络ID，不是Npc数组索引。
- 返回：无。
- 限制／说明：旧伤害结构也未完整设置类型；不要用作当前怪物死亡的可靠通用入口。
- 实现：[`LuaKillNpc`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4524)。

用法示例：

```lua
-- KillNpc(targetId) 走旧50000伤害结构，不保证正确死亡。
```

### `Death`

状态：旧接口；死亡流程待适配。作用域：游戏／NPC脚本。

```lua
Death(npcDwordId)
```

- 参数：npcDwordId：当前玩家附近目标网络ID，不是Npc数组索引。
- 返回：无。
- 限制／说明：与 `KillNpc` 同一实现、同一参数和返回值。旧伤害结构也未完整设置类型；不要用作当前怪物死亡的可靠通用入口。
- 实现：[`LuaKillNpc`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4524)。

用法示例：

```lua
-- Death(targetId) 走旧50000伤害结构，不保证正确死亡。
```

### `KillPlayer`

状态：旧接口；伤害结构待适配。作用域：游戏／NPC脚本。

```lua
KillPlayer()
```

- 参数：无。
- 返回：无。
- 限制／说明：用同样未完整标注类型的50000伤害结构攻击自身；不能替代当前完整死亡／复活流程。
- 实现：[`LuaKillPlayer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4555)。

用法示例：

```lua
-- KillPlayer() 旧自伤入口，需先适配。
```

### `HideNpc`

状态：有缺陷：使用前先修通用校验。作用域：游戏／NPC脚本。

```lua
HideNpc(npcIndexOrName, frames)
```

- 参数：第1项：NPC运行时索引或当前世界名字；frames：隐藏／复活动作帧数，<=0改1。
- 返回：无。
- 限制／说明：实际执行 ExecuteRevive 并重设帧计时；索引判断错误使用OR，无法有效拒绝越界；非通用隐身开关。
- 实现：[`LuaHideNpc`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6196)。

用法示例：

```lua
-- 有效NPC与世界上下文确认后才可 HideNpc(npcIndex, frames)。
```

### `RevivalAllNpc`

状态：旧世界操作；批量效果需核对。作用域：游戏／NPC脚本。

```lua
RevivalAllNpc()
```

- 参数：无；要求有效subworldindex全局上下文。
- 返回：无。
- 限制／说明：作用于上下文世界的全部NPC，调用原RevivalAllNpc，普通NPC脚本慎用。
- 实现：[`LuaRevivalAllNpc`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5435)。

用法示例：

```lua
RevivalAllNpc()
```

### `SetPropState`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetPropState([state=1])
```

- 参数：state：0或非0被归一为0/1；作用对象来自objindex全局。
- 返回：无。
- 限制／说明：只用于有效地图物件脚本，调用 Object.SetState；不是设置玩家属性，也不是任意NPC状态。
- 实现：[`LuaSetObjPropState`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4951)。

用法示例：

```lua
SetPropState(1)
```

## 队伍、聊天、公告及管理

### `IsCaptain`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
IsCaptain()
```

- 参数：无。
- 返回：当前玩家已组队且为队长1，否则0。
- 实现：[`LuaIsLeader`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2248)。

用法示例：

```lua
local captain = IsCaptain()
```

### `GetTeam`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetTeam()
```

- 参数：无。
- 返回：已组队返回队伍运行时ID，未组队nil。
- 限制／说明：0可能是有效队伍ID，不能用>0排除。
- 实现：[`LuaGetTeamId`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2264)。

用法示例：

```lua
local teamId = GetTeam()
```

### `GetTeamSize`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetTeamSize([teamId])
```

- 参数：teamId：可选有效队伍运行时ID，省略查询自身。
- 返回：成员数（含队长）；无队伍0。
- 限制／说明：显式teamId必须在有效数组范围且队伍确实存在，包装层未做完整上界检查。
- 实现：[`LuaGetTeamSize`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2278)。

用法示例：

```lua
local members = GetTeamSize()
```

### `LeaveTeam`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
LeaveTeam()
```

- 参数：无。
- 返回：无。
- 限制／说明：通过原离队处理，不自行维护成员数组。
- 实现：[`LuaLeaveTeam`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2306)。

用法示例：

```lua
LeaveTeam()
```

### `SetCreateTeam`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetCreateTeam(state)
```

- 参数：state：0禁用、非0允许当前玩家组队。
- 返回：无。
- 限制／说明：设置 CanTeamFlag，不创建队伍。
- 实现：[`LuaSetCreateTeamOption`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2321)。

用法示例：

```lua
SetCreateTeam(1)
```

### `Msg2Team`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
Msg2Team(text)
```

- 参数：text：字符串。
- 返回：无。
- 限制／说明：发给原队伍队长及成员；要求有效队伍，未纳入NPC延迟通知事务。
- 实现：[`LuaMsgToTeam`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2367)。

用法示例：

```lua
Msg2Team("队伍提示")
```

### `Msg2SubWorld`

状态：实现已核对。作用域：游戏／NPC脚本、世界脚本。

```lua
Msg2SubWorld(text)
```

- 参数：text：字符串。
- 返回：无。
- 限制／说明：包装层直接调用 SendSystemInfo(0,0,...)，未读取地图ID或subworldindex；实际广播范围按原聊天分发实现，不把名字当“指定地图API”。
- 实现：[`LuaMsgToSubWorld`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2395)。

用法示例：

```lua
Msg2SubWorld("世界提示")
```

### `Msg2Region`

状态：不可用：空实现。作用域：游戏／NPC脚本。

```lua
Msg2Region(...)
```

- 参数：旧注释为text，实际参数全部未读。
- 返回：无。
- 限制／说明：函数体为空。
- 实现：[`LuaMsgToAroundRegion`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2405)。

用法示例：

```lua
-- Msg2Region 当前不会发消息。
```

### `Msg2GM`

状态：旧接口；路由需核对。作用域：游戏／NPC脚本。

```lua
Msg2GM(text[, parameterId=0])
```

- 参数：text：消息字符串；parameterId：GM路由附加整数。
- 返回：无。
- 限制／说明：走原 SendInfoToGM；依赖GM／聊天服务路由，不是发当前玩家。
- 实现：[`LuaMsgToGameMaster`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6075)。

用法示例：

```lua
Msg2GM("诊断消息", 0)
```

### `Msg2IP`

状态：旧接口；路由需核对。作用域：游戏／NPC脚本、世界脚本。

```lua
Msg2IP(ip, id, text[, parameterId=0])
```

- 参数：ip：IPv4字符串；id：原IP路由ID；text：消息；parameterId：GM附加路由参数。
- 返回：无。
- 限制／说明：调用 SendInfoToIP，不是直接TCP发送；不得暴露给玩家输入。
- 实现：[`LuaMsgToIP`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6107)。

用法示例：

```lua
-- 按原GM路由使用 Msg2IP(ip, routeId, text, 0)。
```

### `AddGlobalNews`

状态：旧公告UI；新版表现需核对。作用域：游戏／NPC脚本。

```lua
AddGlobalNews(textOrId)
```

- 参数：textOrId：文本ID或公告字符串，字符串原64字节缓冲。
- 返回：无。
- 限制／说明：跨服广播入口BroadcastGlobal；是否跨游戏服还依赖原转发路由，不是当前玩家提示。
- 实现：[`LuaAddGlobalNews`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1482)。

用法示例：

```lua
AddGlobalNews("公告内容")
```

### `AddGlobalCountNews`

状态：旧公告UI；计时单位待核对。作用域：游戏／NPC脚本。

```lua
AddGlobalCountNews(textOrId, duration)
```

- 参数：textOrId：文本ID或字符串；duration：原NEWSMESSAGE_COUNTING计时整数，<=0改1。
- 返回：无。
- 限制／说明：跨服广播入口BroadcastGlobal；包装层不换算时间，客户端计时单位须按旧UI协议核对，不能直接假定秒。
- 实现：[`LuaAddGlobalCountNews`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1568)。

用法示例：

```lua
AddGlobalCountNews("倒计时公告", duration)
```

### `AddGlobalTimeNews`

状态：旧公告UI；新版表现需核对。作用域：游戏／NPC脚本。

```lua
AddGlobalTimeNews(textOrId, year, month, day, hour, minute)
```

- 参数：第1项：文本ID或字符串；其余为本地时间的年、月、日、时、分。
- 返回：无。
- 限制／说明：跨服广播入口BroadcastGlobal；Windows路径转SYSTEMTIME到UTC，秒为0。不是回调定时器。
- 实现：[`LuaAddGlobalTimeNews`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1670)。

用法示例：

```lua
AddGlobalTimeNews("定时公告", 2026, 11, 7, 20, 0)
```

### `AddLocalNews`

状态：旧公告UI；新版表现需核对。作用域：游戏／NPC脚本、世界脚本。

```lua
AddLocalNews(textOrId)
```

- 参数：textOrId：文本ID或公告字符串，字符串原64字节缓冲。
- 返回：无。
- 限制／说明：当前服务器BroadcastLocalServer；是否跨游戏服还依赖原转发路由，不是当前玩家提示。
- 实现：[`LuaAddLocalNews`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1525)。

用法示例：

```lua
AddLocalNews("公告内容")
```

### `AddLocalCountNews`

状态：旧公告UI；计时单位待核对。作用域：游戏／NPC脚本、世界脚本。

```lua
AddLocalCountNews(textOrId, duration)
```

- 参数：textOrId：文本ID或字符串；duration：原NEWSMESSAGE_COUNTING计时整数，<=0改1。
- 返回：无。
- 限制／说明：当前服务器BroadcastLocalServer；包装层不换算时间，客户端计时单位须按旧UI协议核对，不能直接假定秒。
- 实现：[`LuaAddLocalCountNews`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1619)。

用法示例：

```lua
AddLocalCountNews("倒计时公告", duration)
```

### `AddLocalTimeNews`

状态：旧公告UI；新版表现需核对。作用域：游戏／NPC脚本。

```lua
AddLocalTimeNews(textOrId, year, month, day, hour, minute)
```

- 参数：第1项：文本ID或字符串；其余为本地时间的年、月、日、时、分。
- 返回：无。
- 限制／说明：当前服务器BroadcastLocalServer；Windows路径转SYSTEMTIME到UTC，秒为0。不是回调定时器。
- 实现：[`LuaAddLocalTimeNews`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1734)。

用法示例：

```lua
AddLocalTimeNews("定时公告", 2026, 11, 7, 20, 0)
```

### `GetInfo`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetInfo()
```

- 参数：无。
- 返回：当前玩家Name/Lvl/IP/生命/内力/现金/技能点/属性点的诊断字符串，无玩家空字符串。
- 限制／说明：只供诊断，非结构化稳定协议；IP可能包含端口，字符串截到250字节缓冲。
- 实现：[`LuaGetPlayerInfo`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6138)。

用法示例：

```lua
local text = GetInfo()
```

### `GetIP`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetIP()
```

- 参数：无。
- 返回：g_pServer::GetClientInfo返回的连接描述字符串，无玩家空字符串。
- 限制／说明：不是保证纯IPv4，调用者按原连接实现解释。
- 实现：[`LuaGetIP`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6161)。

用法示例：

```lua
local address = GetIP()
```

### `GetServerName`

状态：实现已核对。作用域：游戏／NPC脚本、世界脚本。

```lua
GetServerName()
```

- 参数：无。
- 返回：操作系统主机名，失败为空字符串。
- 限制／说明：不是serverlist中的区服显示名。
- 实现：[`LuaGetServerName`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4971)。

用法示例：

```lua
local host = GetServerName()
```

### `GetPlayerCount`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetPlayerCount()
```

- 参数：无。
- 返回：PlayerSet当前玩家数。
- 限制／说明：本进程统计，不是全区服总人数。
- 实现：[`LuaGetPlayerCount`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4123)。

用法示例：

```lua
local count = GetPlayerCount()
```

### `KickOutPlayer`

状态：实现已核对。作用域：游戏／NPC脚本、世界脚本。

```lua
KickOutPlayer(name)
```

- 参数：name：已在线角色名，区分大小写精确匹配。
- 返回：无。
- 限制／说明：关闭找到的连接；管理用途。
- 实现：[`LuaKickOutPlayer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4463)。

用法示例：

```lua
-- 管理脚本：KickOutPlayer(playerName)
```

### `KickOutSelf`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
KickOutSelf()
```

- 参数：无。
- 返回：无。
- 限制／说明：关闭当前玩家连接。
- 实现：[`LuaKickOutSelf`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4509)。

用法示例：

```lua
-- 管理脚本：KickOutSelf()
```

### `KickOutAccount`

状态：世界脚本专用。作用域：世界脚本。

```lua
KickOutAccount(account)
```

- 参数：account：账号名，按原strcmpi不区分大小写。
- 返回：无。
- 限制／说明：只注册于 WorldScriptFuns，普通NPC的GameScriptFuns没有该名字。只关闭查到的首个在线账号连接。
- 实现：[`LuaKickOutAccount`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4486)。

用法示例：

```lua
-- 世界管理脚本：KickOutAccount(accountName)
```

### `SetChatFlag`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetChatFlag(flag)
```

- 参数：flag：原禁言标志整数。
- 返回：无。
- 限制／说明：调用原 SetChatForbiddenFlag；时限和解除按底层规则，不接收秒数。
- 实现：[`LuaSetPlayerChatForbiddenFlag`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4450)。

用法示例：

```lua
SetChatFlag(flag)
```

### `SetPK`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetPK(value)
```

- 参数：value：原PK值整数。
- 返回：无。
- 限制／说明：走原m_cPK.SetPKValue，不是切换战斗状态。
- 实现：[`LuaSetPlayerPKValue`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5837)。

用法示例：

```lua
SetPK(value)
```

### `SetPunish`

状态：不可用：玩家判断条件错误。作用域：游戏／NPC脚本。

```lua
SetPunish(state)
```

- 参数：名义state：0不惩罚，非0 PK战斗惩罚。
- 返回：无。
- 限制／说明：实现只在nPlayerIndex<=0时进入写字段，对有效玩家无效，无玩家反而可能越界。
- 实现：[`LuaSetDeathPunish`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:6177)。

用法示例：

```lua
-- 不使用 SetPunish，需先修实现条件。
```

### `ShowLadder`

状态：旧接口；榜单协议待核对。作用域：游戏／NPC脚本。

```lua
ShowLadder(count, id1, ...)
```

- 参数：count：榜单数，须1～50；后续对应榜单ID。
- 返回：无。
- 限制／说明：旧LADDER_LIST协议，缓冲最多50项但包装层未强制上限；不能传超过50，需核对新版协议。
- 实现：[`LuaShowLadder`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5885)。

用法示例：

```lua
ShowLadder(1, ladderId)
```

### `OpenTong`

状态：不推荐：广播范围与新UI待修。作用域：游戏／NPC脚本。

```lua
OpenTong(dummy)
```

- 参数：dummy：当前实现至少要求1参数但不读取值。
- 返回：无。
- 限制／说明：服务端分支BroadcastGlobal而非只发当前玩家；无参无动作。不能认作可用的个人帮会窗口接口。
- 实现：[`LuaOpenTong`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3333)。

用法示例：

```lua
-- OpenTong(0) 会走旧全局广播，不用于当前NPC。
```

### `SwearBrother`

状态：旧接口；结拜流程待核对。作用域：游戏／NPC脚本。

```lua
SwearBrother(teamId)
```

- 参数：teamId：有有效队长的原队伍ID，范围0～MAX_TEAM-1。
- 返回：无，旧注释ret并未实现。
- 限制／说明：收集队长和成员名字发原聊天结拜请求；依赖完整结拜／聊天服务，未提供事务保证。
- 实现：[`LuaSwearBrother`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5914)。

用法示例：

```lua
-- 原结拜流程：SwearBrother(teamId)
```

### `MakeEnemy`

状态：不可用：空实现。作用域：游戏／NPC脚本。

```lua
MakeEnemy(...)
```

- 参数：旧说明是仇人名字，当前不读取参数。
- 返回：无。
- 限制／说明：函数体为空。
- 实现：[`LuaMakeEnemy`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5945)。

用法示例：

```lua
-- MakeEnemy 当前不会结仇。
```

### `AddLoaclTimeNews`

状态：世界脚本专用；旧公告UI。作用域：世界脚本。

```lua
AddLoaclTimeNews(textOrId, year, month, day, hour, minute)
```

- 参数：第1项：文本ID或字符串；其余为本地时间的年、月、日、时、分。
- 返回：无。
- 限制／说明：WorldScriptFuns专用的原始拼写（Loacl），不是Local。参数和行为与游戏脚本 AddLocalTimeNews 相同。与 `AddLocalTimeNews` 同一实现、同一参数和返回值。当前服务器BroadcastLocalServer；Windows路径转SYSTEMTIME到UTC，秒为0。不是回调定时器。
- 实现：[`LuaAddLocalTimeNews`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:1734)。

用法示例：

```lua
AddLoaclTimeNews("定时公告", 2026, 11, 7, 20, 0)
```

## 旧车站、码头与路标资源

### `AddStation`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
AddStation(stationId)
```

- 参数：stationId：原车站ID。
- 返回：无。
- 限制／说明：加入玩家原车站列表，重复不加。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaAddPlayerStation`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4706)。

用法示例：

```lua
AddStation(stationId)
```

### `AddTermini`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
AddTermini(wayPointId)
```

- 参数：wayPointId：原路标ID。
- 返回：无。
- 限制／说明：加入原路标列表，重复不加；列表保留数量按旧逻辑处理。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaAddPlayerWayPoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4679)。

用法示例：

```lua
AddTermini(wayPointId)
```

### `GetStation`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetStation(ordinal, currentStation)
```

- 参数：ordinal：符合可达条件的1起始序号；currentStation：起点原车站ID。
- 返回：该序号对应可达已记录车站ID；无匹配0。
- 限制／说明：仅筛选价格>0并排除当前站。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetPlayerStation`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4737)。

用法示例：

```lua
local id = GetStation(1, currentStation)
```

### `GetStationCount`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetStationCount()
```

- 参数：无。
- 返回：玩家已记录车站总数。
- 限制／说明：不是 GetStation 可达筛选后的数量。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetPlayerStationCount`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4727)。

用法示例：

```lua
local count = GetStationCount()
```

### `GetCityCount`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetCityCount()
```

- 参数：无。
- 返回：车站表行数减1，最小0。
- 限制／说明：并非当前所有城市地图的数量。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetAllStationCount`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4844)。

用法示例：

```lua
local count = GetCityCount()
```

### `GetWayPoint`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetWayPoint(ordinal)
```

- 参数：ordinal：原玩家路标列表1起始序号。
- 返回：路标ID；不存在0。
- 限制／说明：必须自行保证ordinal>=1。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetPlayerWayPoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4776)。

用法示例：

```lua
local id = GetWayPoint(1)
```

### `GetStationName`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetStationName(stationId)
```

- 参数：stationId：原车站ID。
- 返回：原表DESC名称，缺省“无名城”。
- 限制／说明：读取stationId+1行。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetStationName`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4809)。

用法示例：

```lua
local name = GetStationName(stationId)
```

### `GetWayPointName`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetWayPointName(wayPointId)
```

- 参数：wayPointId：原路标ID。
- 返回：原表DESC，缺省“未记录”。
- 限制／说明：读取wayPointId+1行。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetWayPointName`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4826)。

用法示例：

```lua
local name = GetWayPointName(wayPointId)
```

### `GetPrice2Station`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetPrice2Station(fromId, toId)
```

- 参数：fromId/toId：原车站ID。
- 返回：原g_GetPriceToStation结果。
- 限制／说明：仅查询价格，不扣费。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetPriceToStation`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4868)。

用法示例：

```lua
local price = GetPrice2Station(fromId, toId)
```

### `GetPrice2WayPoint`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetPrice2WayPoint(fromStation, toWayPoint)
```

- 参数：fromStation：起点车站ID；toWayPoint：目标路标ID。
- 返回：原g_GetPriceToWayPoint结果。
- 限制／说明：仅查询价格。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetPriceToWayPoint`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4859)。

用法示例：

```lua
local price = GetPrice2WayPoint(fromStation, toWayPoint)
```

### `GetStationPos`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetStationPos(stationId)
```

- 参数：stationId：原车站ID。
- 返回：mapId,x,y；无有效COUNT时无返回。
- 限制／说明：从COUNT与随机SECTn读落点，坐标按原表单位。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetStationPos`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4876)。

用法示例：

```lua
local mapId, x, y = GetStationPos(stationId)
```

### `GetWayPointPos`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetWayPointPos(wayPointId)
```

- 参数：wayPointId：原路标ID。
- 返回：mapId,x,y，缺省0,0,0。
- 限制／说明：读取SECT字段。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetWayPointPos`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4907)。

用法示例：

```lua
local mapId, x, y = GetWayPointPos(wayPointId)
```

### `GetWharfName`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetWharfName(dockId)
```

- 参数：dockId：原码头ID。
- 返回：原DESC名称，缺省“未知码头”。
- 限制／说明：不是地图名查询。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetDockName`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5053)。

用法示例：

```lua
local name = GetWharfName(dockId)
```

### `GetWharfCount`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetWharfCount(currentDock)
```

- 参数：currentDock：起点码头ID。
- 返回：价格>0的目标码头数量。
- 限制／说明：仅可达目标计数。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetDockCount`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4995)。

用法示例：

```lua
local count = GetWharfCount(currentDock)
```

### `GetWharfPrice`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetWharfPrice(fromDock, toDock)
```

- 参数：fromDock/toDock：原码头ID。
- 返回：原g_GetPriceToDock结果。
- 限制／说明：不自动扣费。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetDockPrice`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5020)。

用法示例：

```lua
local price = GetWharfPrice(fromDock, toDock)
```

### `GetWharf`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetWharf(currentDock, ordinal)
```

- 参数：currentDock：起点码头；ordinal：价格>0目标的1起始序号。
- 返回：目标码头ID；无匹配0。
- 限制／说明：不是原码头行号直接索引。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetDock`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5028)。

用法示例：

```lua
local id = GetWharf(currentDock, 1)
```

### `GetWharfPos`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetWharfPos(dockId)
```

- 参数：dockId：原码头ID。
- 返回：mapId,x,y；越界或无COUNT无返回。
- 限制／说明：从COUNT和随机SECTn取落点。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetDockPos`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5071)。

用法示例：

```lua
local mapId, x, y = GetWharfPos(dockId)
```

### `GetTerminiFState`

状态：旧资源接口；调用前核对加载。作用域：游戏／NPC脚本。

```lua
GetTerminiFState(wayPointId)
```

- 参数：wayPointId：原路标ID。
- 返回：原FightState整数，缺省0。
- 限制／说明：不会设置战斗状态。依赖原 g_StationTabFile / g_WayPointTabFile / g_DockTabFile 及价格表，不能当当前全部maplist与城市阵API；表未加载时返回空／0或无值。使用前核实资源和路线。
- 实现：[`LuaGetWayPointFightState`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5107)。

用法示例：

```lua
local state = GetTerminiFState(wayPointId)
```

### `GetCity`

状态：不可用：空实现。作用域：游戏／NPC脚本。

```lua
GetCity(...)
```

- 参数：没有有效参数约定，当前未读取参数。
- 返回：无。
- 限制／说明：空函数，与GetCityCount不同。
- 实现：[`LuaGetCity`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:4853)。

用法示例：

```lua
-- GetCity 当前无查询结果。
```

## 计时与系统时间

### `SystemTime`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SystemTime()
```

- 参数：无。
- 返回：Unix时间戳，秒。
- 限制／说明：服务端系统时间，不是游戏帧。
- 实现：[`LuaSystemTime`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3898)。

用法示例：

```lua
local timestamp = SystemTime()
```

### `GetHMS`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetHMS()
```

- 参数：无。
- 返回：本地 hour, minute, second；localtime失败无返回。
- 实现：[`LuaGetHMS`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3904)。

用法示例：

```lua
local hour, minute, second = GetHMS()
```

### `GetYMD`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetYMD()
```

- 参数：无。
- 返回：本地 year, month(1～12), day；失败无返回。
- 实现：[`LuaGetYMD`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3916)。

用法示例：

```lua
local year, month, day = GetYMD()
```

### `Time2LocalYMD`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
Time2LocalYMD(timestamp)
```

- 参数：timestamp：唯一数字参数，Unix秒。
- 返回：本地year, month, day；参数不符或转换失败无返回。
- 实现：[`LuaTime2LocalYMD`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:3951)。

用法示例：

```lua
local year, month, day = Time2LocalYMD(SystemTime())
```

### `SetTimer`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
SetTimer(interval, timerTaskId)
```

- 参数：interval：游戏时间tick间隔，直接加到GetGameTime；0关闭；timerTaskId：原定时任务ID，底层为unsigned short。
- 返回：无。
- 限制／说明：玩家单计时器，任务ID到原计时脚本映射并回调OnTimer；不是任意函数名。不要把Unix秒和游戏tick混用。
- 实现：[`LuaSetTimer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2166)。

用法示例：

```lua
SetTimer(intervalTicks, timerTaskId)
```

### `StopTimer`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
StopTimer()
```

- 参数：无。
- 返回：无。
- 限制／说明：关闭当前玩家原单计时器。
- 实现：[`LuaStopTimer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2176)。

用法示例：

```lua
StopTimer()
```

### `GetTimerId`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetTimerId()
```

- 参数：无。
- 返回：当前玩家计时任务ID；没有／无玩家0。
- 实现：[`LuaGetCurTimerId`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2184)。

用法示例：

```lua
local id = GetTimerId()
```

### `GetRestTime`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
GetRestTime()
```

- 参数：无。
- 返回：原计时器剩余游戏tick，过期为0；无玩家nil。
- 限制／说明：不是秒，单位与SetTimer一致。
- 实现：[`LuaGetRestTime`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2197)。

用法示例：

```lua
local ticks = GetRestTime()
```

## 旧任务副本与全局活动

### `GetMissionV`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
GetMissionV(valueId)
```

- 参数：valueId：当前世界活动变量编号，包装层只读取>0编号。
- 返回：变量整数，取不到0。
- 限制／说明：读取当前世界MissionArray变量，不接收missionId。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaGetMissionValue`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5142)。

用法示例：

```lua
local value = GetMissionV(valueId)
```

### `SetMissionV`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
SetMissionV(valueId, value)
```

- 参数：valueId：非负活动变量编号；value：整数。
- 返回：无。
- 限制／说明：设置当前世界MissionArray变量；写入0号但GetMissionV无法读取0号。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaSetMissionValue`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5123)。

用法示例：

```lua
SetMissionV(valueId, 1)
```

### `GetGlbMissionV`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
GetGlbMissionV(valueId)
```

- 参数：valueId：非负全局活动变量编号。
- 返回：变量整数，取不到0。
- 限制／说明：作用于g_GlobalMissionArray，不要求玩家。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaGetGlobalMissionValue`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5181)。

用法示例：

```lua
local value = GetGlbMissionV(valueId)
```

### `SetGlbMissionV`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
SetGlbMissionV(valueId, value)
```

- 参数：valueId：非负全局活动变量编号；value：整数。
- 返回：无。
- 限制／说明：不是玩家存档任务；不要假定重启持久化。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaSetGlobalMissionValue`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5166)。

用法示例：

```lua
SetGlbMissionV(valueId, 1)
```

### `OpenMission`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
OpenMission(missionId)
```

- 参数：missionId：非负原任务表ID。
- 返回：无。
- 限制／说明：新增当前世界Mission实例并从原Mission表第2列读取脚本，执行InitMission；还要求有效玩家，实例存在则拒绝。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaInitMission`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5200)。

用法示例：

```lua
OpenMission(missionId)
```

### `RunMission`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
RunMission(missionId)
```

- 参数：missionId：已打开的原Mission ID。
- 返回：无。
- 限制／说明：查找实例并执行原脚本RunMission；要求有效玩家。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaRunMission`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5240)。

用法示例：

```lua
RunMission(missionId)
```

### `CloseMission`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
CloseMission(missionId)
```

- 参数：missionId：原Mission ID。
- 返回：无。
- 限制／说明：执行EndMission，停止并移除实例。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaCloseMission`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5270)。

用法示例：

```lua
CloseMission(missionId)
```

### `StartMissionTimer`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
StartMissionTimer(missionId, timerId, interval)
```

- 参数：missionId：原Mission ID；timerId：非负定时任务ID；interval：非负游戏tick间隔。
- 返回：无。
- 限制／说明：在Mission的计时集合增加定时器；0按底层关闭语义，不是Unix时间戳。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaStartMissionTimer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5332)。

用法示例：

```lua
StartMissionTimer(missionId, timerId, intervalTicks)
```

### `StopMissionTimer`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
StopMissionTimer(missionId, timerId)
```

- 参数：原Mission ID及其定时器ID。
- 返回：无。
- 限制／说明：关闭并移除匹配的计时任务。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaStopMissionTimer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5298)。

用法示例：

```lua
StopMissionTimer(missionId, timerId)
```

### `GetMSRestTime`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
GetMSRestTime(missionId, timerId)
```

- 参数：原Mission ID及定时器ID。
- 返回：剩余游戏tick；无任务／定时器0。
- 限制／说明：单位同StartMissionTimer，不是秒。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaGetMissionRestTime`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2215)。

用法示例：

```lua
local ticks = GetMSRestTime(missionId, timerId)
```

### `AddMSPlayer`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
AddMSPlayer(missionId, groupId) 或 AddMSPlayer(missionId, playerIndex, groupId)
```

- 参数：missionId：原Mission ID；playerIndex：可选玩家运行时索引，省略用当前玩家；groupId：原分组编号。
- 返回：无。
- 限制／说明：绑定玩家运行时索引、角色ID与组。不是NPC数组索引。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaAddMissionPlayer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5397)。

用法示例：

```lua
AddMSPlayer(missionId, groupId)
```

### `DelMSPlayer`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
DelMSPlayer(missionId, groupId) 或 DelMSPlayer(missionId, playerIndex, groupId)
```

- 参数：与AddMSPlayer同形式；groupId仍需非负，但移除只依据playerIndex。
- 返回：无。
- 限制／说明：调用原RemovePlayer。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaRemoveMissionPlayer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5446)。

用法示例：

```lua
DelMSPlayer(missionId, groupId)
```

### `GetNextPlayer`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
GetNextPlayer(missionId, previousDataIndex, groupId)
```

- 参数：missionId：原Mission ID；previousDataIndex：上次数据索引，第一次0；groupId：0遍历全部，其余筛组。
- 返回：nextDataIndex, playerIndex；结束0,0。
- 限制／说明：虽然源码只检查至少2个参数，实际读取3项，必须显式传groupId。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaGetNextPlayer`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5485)。

用法示例：

```lua
local dataIndex, playerIndex = GetNextPlayer(missionId, 0, 0)
```

### `PIdx2MSDIdx`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
PIdx2MSDIdx(missionId, playerIndex)
```

- 参数：原Mission ID、玩家运行时索引。
- 返回：活动内部数据索引，无匹配0。
- 限制／说明：与NPC索引、网络ID不同。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaGetMissionPlayer_DataIndex`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5695)。

用法示例：

```lua
local dataIndex = PIdx2MSDIdx(missionId, playerIndex)
```

### `MSDIdx2PIdx`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
MSDIdx2PIdx(missionId, dataIndex)
```

- 参数：原Mission ID、有效活动内部数据索引。
- 返回：玩家运行时索引，无结果通常0。
- 限制／说明：数据索引必须来自该实例，底层边界校验不充分。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaGetMissionPlayer_PlayerIndex`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5666)。

用法示例：

```lua
local playerIndex = MSDIdx2PIdx(missionId, dataIndex)
```

### `GetMSIdxGroup`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
GetMSIdxGroup(missionId, dataIndex)
```

- 参数：原Mission ID、活动内部数据索引。
- 返回：分组ID，取不到0。
- 限制／说明：包装层变量名误叫nNpcIndex，实际传给GetMissionPlayer_GroupId的是活动数据索引。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaGetPlayerMissionGroup`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5789)。

用法示例：

```lua
local groupId = GetMSIdxGroup(missionId, dataIndex)
```

### `GetMSPlayerCount`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
GetMSPlayerCount(missionId[, groupId=0])
```

- 参数：missionId：原Mission ID；groupId：0全部，其他为对应分组。
- 返回：人数，取不到0。
- 限制／说明：依据已加入活动的玩家集合统计。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaMissionPlayerCount`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5596)。

用法示例：

```lua
local count = GetMSPlayerCount(missionId, 0)
```

### `SetPMParam`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
SetPMParam(missionId, dataIndex, parameterId, value)
```

- 参数：missionId：原ID；dataIndex：有效活动数据索引；parameterId：只用1或2；value：整数。
- 返回：无。
- 限制／说明：parameterId=1写Param1，其余<=2都会写Param2；不要传0或负数利用旧分支。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaSetMissionPlayerParam`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5725)。

用法示例：

```lua
SetPMParam(missionId, dataIndex, 1, value)
```

### `GetPMParam`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
GetPMParam(missionId, dataIndex, parameterId)
```

- 参数：原Mission ID、活动数据索引、参数号1或2。
- 返回：对应参数整数，无结果0。
- 限制／说明：与SetPMParam对应。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaGetMissionPlayerParam`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5756)。

用法示例：

```lua
local value = GetPMParam(missionId, dataIndex, 1)
```

### `Msg2MSGroup`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
Msg2MSGroup(missionId, text, groupId)
```

- 参数：原Mission ID；text：字符串；groupId：非负组ID。
- 返回：无。
- 限制／说明：只发该活动中精确分组的玩家；此处组0不是广播所有组。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaMissionMsg2Group`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5523)。

用法示例：

```lua
Msg2MSGroup(missionId, "活动提示", groupId)
```

### `Msg2MSAll`

状态：旧活动接口；依赖上下文。作用域：游戏／NPC脚本。

```lua
Msg2MSAll(missionId, text)
```

- 参数：原Mission ID及消息字符串。
- 返回：无。
- 限制／说明：发该活动所有已加入玩家。要求服务端提供正确的subworldindex上下文及已加载的原任务表；属于旧Mission活动系统，不是角色任务栏TaskNote，也不代表当前活动已适配。
- 实现：[`LuaMissionMsg2All`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5548)。

用法示例：

```lua
Msg2MSAll(missionId, "活动提示")
```

### `NpcIdx2PIdx`

状态：实现已核对。作用域：游戏／NPC脚本。

```lua
NpcIdx2PIdx(npcIndex)
```

- 参数：npcIndex：有效NPC运行时索引。
- 返回：该NPC是玩家时返回其玩家运行时索引，否则0。
- 限制／说明：不要求该玩家已加入活动。
- 实现：[`LuaNpcIndexToPlayerIndex`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5647)。

用法示例：

```lua
local playerIndex = NpcIdx2PIdx(npcIndex)
```

### `Msg2MSPlayer`

状态：不可用作定向发送：错调用。作用域：游戏／NPC脚本。

```lua
Msg2MSPlayer(missionId, text, playerIndex)
```

- 参数：名义为原Mission ID、消息文本和玩家索引。
- 返回：无。
- 限制／说明：当前错误调用 Msg2Group(text, playerIndex)，将玩家索引当组号；不是定向玩家消息。
- 实现：[`LuaMissionMsg2Player`](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:5572)。

用法示例：

```lua
-- 不用此接口定向发玩家，先修通用实现。
```

## Lua 4基础库与表操作

### `assert`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
assert(condition[, message])
```

- 参数：condition：仅nil为假；message：失败文本，默认空。
- 返回：成功无返回，失败抛Lua错误。
- 限制／说明：本版不返回condition；assert(0)会通过，不可拿它校验数字成功码。
- 实现：[`luaB_assert`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:376)。

用法示例：

```lua
assert(rows ~= nil, "资源表读取失败")
```

### `error`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
error([message])
```

- 参数：message：错误字符串；省略按原错误传播。
- 返回：不正常返回，抛Lua错误。
- 限制／说明：事务入口能据此中止和回滚支持范围；普通Lua回调并不会因此自动获得事务。
- 实现：[`luaB_error`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:115)。

用法示例：

```lua
error("操作未完成")
```

### `type`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
type(value)
```

- 参数：value：任意存在的参数。
- 返回：nil/number/string/table/function/userdata等类型名称字符串。
- 限制／说明：本版无Lua5的boolean类型。
- 实现：[`luaB_type`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:217)。

用法示例：

```lua
local kind = type(value)
```

### `tonumber`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
tonumber(value[, base=10])
```

- 参数：value：数值或可转换字符串；base：2～36，非十进制时要求字符串。
- 返回：数字，转换失败nil。
- 限制／说明：业务数字还需验证整数、范围和非负。
- 实现：[`luaB_tonumber`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:87)。

用法示例：

```lua
local value = tonumber("200000")
```

### `tostring`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
tostring(value)
```

- 参数：value：任意值。
- 返回：字符串，table/function/userdata通常为类型及地址描述。
- 限制／说明：不用于稳定存档序列化。
- 实现：[`luaB_tostring`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:311)。

用法示例：

```lua
local text = tostring(value)
```

### `print`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
print(value1, ...)
```

- 参数：任意参数序列，经tostring转换。
- 返回：无。
- 限制／说明：写stdout，非客户端消息；业务日志优先WriteLog。
- 实现：[`luaB_print`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:66)。

用法示例：

```lua
print("debug", value)
```

### `getn`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
getn(table)
```

- 参数：table：数组表。
- 返回：旧lua_getn长度（优先原n字段，否则按数字键计算）。
- 限制／说明：不是Lua5的#运算符；包含nil或手动维护n字段时须核对。
- 实现：[`luaB_getn`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:384)。

用法示例：

```lua
local count = getn(rows)
```

### `tinsert`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
tinsert(table, [position,] value)
```

- 参数：table：数组；position：可选插入下标，默认尾部；value：插入值。
- 返回：无。
- 限制／说明：更新table.n并移动后续数组元素。
- 实现：[`luaB_tinsert`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:391)。

用法示例：

```lua
tinsert(options, {"关闭", "OnClose"})
```

### `tremove`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
tremove(table[, position])
```

- 参数：table：数组；position：移除下标，默认末尾。
- 返回：移除的值；空数组无返回。
- 限制／说明：更新table.n及后续下标。
- 实现：[`luaB_tremove`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:413)。

用法示例：

```lua
local value = tremove(items, 1)
```

### `sort`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
sort(table[, less])
```

- 参数：table：待原地排序数组；less：可选比较函数，参数a/b，小于应返回非nil，否则nil。
- 返回：无。
- 限制／说明：比较函数不能用0表示假：Lua4中0为真。
- 实现：[`luaB_sort`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:527)。

用法示例：

```lua
sort(values)
```

### `next`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
next(table[, key])
```

- 参数：table：遍历表；key：上次键，第一次nil。
- 返回：下一key,value；结束nil。
- 限制／说明：遍历顺序不保证；不是“第i行”函数。
- 实现：[`luaB_next`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:224)。

用法示例：

```lua
local key, value = next(rows, nil)
```

### `foreach`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
foreach(table, callback)
```

- 参数：table：任意表；callback(key,value)：逐键调用。
- 返回：回调首次非nil的值，全部完成无返回。
- 限制／说明：回调返回0也会中止。
- 实现：[`luaB_foreach`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:358)。

用法示例：

```lua
foreach(data, function(k, v) print(k, v) end)
```

### `foreachi`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
foreachi(table, callback)
```

- 参数：table：数组；callback(index,value)，索引1～getn(table)。
- 返回：首个非nil回调结果；完成无返回。
- 限制／说明：回调返回0也会中止。
- 实现：[`luaB_foreachi`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:340)。

用法示例：

```lua
foreachi(rows, function(i, row) print(i) end)
```

### `rawget`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
rawget(table, key)
```

- 参数：table及键。
- 返回：原始键值，不触发tag方法。
- 限制／说明：绕过tag读行为。
- 实现：[`luaB_rawget`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:165)。

用法示例：

```lua
local value = rawget(data, "Count")
```

### `rawset`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
rawset(table, key, value)
```

- 参数：table、键、值。
- 返回：原table。
- 限制／说明：绕过tag写行为。
- 实现：[`luaB_rawset`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:172)。

用法示例：

```lua
rawset(data, "Count", 1)
```

### `getglobal`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
getglobal(name)
```

- 参数：name：全局变量名字符串。
- 返回：全局值，未定义nil。
- 限制／说明：是当前脚本Lua状态的全局，不是所有脚本共享存档。
- 实现：[`luaB_getglobal`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:126)。

用法示例：

```lua
local callback = getglobal("OnCompound")
```

### `setglobal`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
setglobal(name, value)
```

- 参数：name：全局名；value：任意值。
- 返回：无。
- 限制／说明：不要覆盖引擎函数、playerindex/subworldindex等上下文。
- 实现：[`luaB_setglobal`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:120)。

用法示例：

```lua
setglobal("RuleCache", rows)
```

### `globals`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
globals([newTable])
```

- 参数：newTable：可选替换的全局环境表。
- 返回：调用前的全局环境表。
- 限制／说明：有参数会替换整个全局环境；普通NPC仅用于查询，避免破坏接口和上下文。
- 实现：[`luaB_globals`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:155)。

用法示例：

```lua
local environment = globals()
```

### `call`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
call(func, args[, options=""[, errorHandler]])
```

- 参数：func：函数；args：参数数组；options：可含x（出错返回nil）；errorHandler：可选临时错误处理器。
- 返回：目标函数全部返回值；x选项出错nil，否则重新抛错。
- 限制／说明：p打包选项已废弃并报错；新业务不要吞掉事务错误后继续扣费／发奖。
- 实现：[`luaB_call`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:273)。

用法示例：

```lua
local result = call(callback, {1, 2})
```

### `dofile`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
dofile([path])
```

- 参数：path：本地脚本路径，省略为标准输入。
- 返回：脚本返回值；无返回时一个非nil成功标记；失败nil,错误类别字符串。
- 限制／说明：不走ReadTabFile资源路径约束；省略path可能阻塞。
- 实现：[`luaB_dofile`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:266)。

用法示例：

```lua
-- 资源公共脚本优先 Include(resourcePath)。
```

### `dostring`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
dostring(code[, chunkName])
```

- 参数：code：Lua源代码字符串；chunkName：可选错误来源名称。
- 返回：代码返回值；无结果时非nil成功标记；失败nil,错误类别。
- 限制／说明：拒绝预编译二进制字符串；不拼接玩家输入执行。
- 实现：[`luaB_dostring`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:256)。

用法示例：

```lua
local value = dostring("return 1 + 2")
```

### `gcinfo`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
gcinfo()
```

- 参数：无。
- 返回：当前GC用量KB, 回收阈值KB。
- 限制／说明：运行时诊断，非角色存档。
- 实现：[`luaB_gcinfo`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:204)。

用法示例：

```lua
local used, threshold = gcinfo()
```

### `collectgarbage`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
collectgarbage([threshold=0])
```

- 参数：threshold：回收阈值整数，0按原GC逻辑触发回收。
- 返回：无。
- 限制／说明：全Lua状态的GC控制，不要放NPC高频循环。
- 实现：[`luaB_collectgarbage`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:211)。

用法示例：

```lua
collectgarbage()
```

### `newtag`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
newtag()
```

- 参数：无。
- 返回：新tag整数。
- 限制／说明：Lua4旧tag体系，不是Lua5 metatable API。
- 实现：[`luaB_newtag`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:144)。

用法示例：

```lua
local tagId = newtag()
```

### `tag`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
tag(value)
```

- 参数：value：任意值。
- 返回：对应tag整数。
- 限制／说明：旧类型／行为标签。
- 实现：[`luaB_tag`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:131)。

用法示例：

```lua
local tagId = tag(value)
```

### `settag`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
settag(table, tagId)
```

- 参数：table：表；tagId：合法tag。
- 返回：原table。
- 限制／说明：不是setmetatable。
- 实现：[`luaB_settag`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:137)。

用法示例：

```lua
settag(data, tagId)
```

### `gettagmethod`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
gettagmethod(tagId, event)
```

- 参数：tagId：合法tag；event：事件名字符串。
- 返回：现有tag处理函数或nil。
- 限制／说明：禁止获取gc事件，会报错。
- 实现：[`luaB_gettagmethod`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:194)。

用法示例：

```lua
local handler = gettagmethod(tagId, "gettable")
```

### `settagmethod`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
settagmethod(tagId, event, handler)
```

- 参数：tagId：合法tag；event：事件名；handler：函数或nil。
- 返回：原事件处理函数或nil。
- 限制／说明：禁止设置gc事件，不要改变共享底层类型行为。
- 实现：[`luaB_settagmethod`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:180)。

用法示例：

```lua
settagmethod(tagId, "gettable", handler)
```

### `copytagmethods`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
copytagmethods(toTag, fromTag)
```

- 参数：目标及来源合法tag整数。
- 返回：底层lua_copytagmethods返回的tag整数。
- 限制／说明：复制旧tag事件处理。
- 实现：[`luaB_copytagmethods`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:149)。

用法示例：

```lua
local tagId = copytagmethods(toTag, fromTag)
```

### `_ALERT`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
_ALERT(message)
```

- 参数：message：错误提示字符串。
- 返回：无。
- 限制／说明：默认写stderr，非玩家提示。
- 实现：[`luaB__ALERT`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:26)。

用法示例：

```lua
-- 运行时错误输出钩子：_ALERT("diagnostic")
```

### `_ERRORMESSAGE`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
_ERRORMESSAGE(message)
```

- 参数：message：错误文本。
- 返回：无。
- 限制／说明：IO库覆盖基础实现，附加调用栈后交给_ALERT。
- 实现：[`errorfb`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:595)。

用法示例：

```lua
-- 由Lua运行时调用；业务使用error(message)。
```

### `rawgettable`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
rawgettable(table, key)
```

- 参数：table及键。
- 返回：原始键值，不触发tag方法。
- 限制／说明：与 `rawget` 同一实现、同一参数和返回值。绕过tag读行为。
- 实现：[`luaB_rawget`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:165)。

用法示例：

```lua
local value = rawgettable(data, "Count")
```

### `rawsettable`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
rawsettable(table, key, value)
```

- 参数：table、键、值。
- 返回：原table。
- 限制／说明：与 `rawset` 同一实现、同一参数和返回值。绕过tag写行为。
- 实现：[`luaB_rawset`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:172)。

用法示例：

```lua
rawsettable(data, "Count", 1)
```

### `foreachvar`

状态：不可用：标准库废弃函数。作用域：所有已初始化的Lua状态。

```lua
foreachvar(callback)
```

- 参数：仅保留旧名称；当前编译未定义LUA_DEPRECATEDFUNCS，不接受有效业务调用。
- 返回：调用报“function is deprecated”错误。
- 限制／说明：不是可用兼容别名。
- 实现：[`deprecated_func`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:564)。

用法示例：

```lua
-- 改用 globals()/foreach()/next()/rawget()/rawset()。
```

### `nextvar`

状态：不可用：标准库废弃函数。作用域：所有已初始化的Lua状态。

```lua
nextvar([name])
```

- 参数：仅保留旧名称；当前编译未定义LUA_DEPRECATEDFUNCS，不接受有效业务调用。
- 返回：调用报“function is deprecated”错误。
- 限制／说明：不是可用兼容别名。
- 实现：[`deprecated_func`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:564)。

用法示例：

```lua
-- 改用 globals()/foreach()/next()/rawget()/rawset()。
```

### `rawgetglobal`

状态：不可用：标准库废弃函数。作用域：所有已初始化的Lua状态。

```lua
rawgetglobal(name)
```

- 参数：仅保留旧名称；当前编译未定义LUA_DEPRECATEDFUNCS，不接受有效业务调用。
- 返回：调用报“function is deprecated”错误。
- 限制／说明：不是可用兼容别名。
- 实现：[`deprecated_func`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:564)。

用法示例：

```lua
-- 改用 globals()/foreach()/next()/rawget()/rawset()。
```

### `rawsetglobal`

状态：不可用：标准库废弃函数。作用域：所有已初始化的Lua状态。

```lua
rawsetglobal(name, value)
```

- 参数：仅保留旧名称；当前编译未定义LUA_DEPRECATEDFUNCS，不接受有效业务调用。
- 返回：调用报“function is deprecated”错误。
- 限制／说明：不是可用兼容别名。
- 实现：[`deprecated_func`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lbaselib.c:564)。

用法示例：

```lua
-- 改用 globals()/foreach()/next()/rawget()/rawset()。
```

## Lua 4字符串库

### `strlen`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
strlen(text)
```

- 参数：text：字符串。
- 返回：字节长度整数。
- 限制／说明：GBK中文通常2字节，不是Unicode字符数。
- 实现：[`str_len`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:21)。

用法示例：

```lua
local length = strlen("abc")
```

### `strsub`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
strsub(text, first[, last=-1])
```

- 参数：text：字符串；first/last：1起始、两端包含的字节位置，负数从末尾倒数。
- 返回：截取字符串。
- 限制／说明：按字节截取中文可能截断汉字。
- 实现：[`str_sub`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:35)。

用法示例：

```lua
local head = strsub("abc", 1, 2)
```

### `strlower`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
strlower(text)
```

- 参数：text：字符串。
- 返回：经当前C locale逐字节tolower的字符串。
- 限制／说明：不作Unicode大小写转换。
- 实现：[`str_lower`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:49)。

用法示例：

```lua
local name = strlower("ABC")
```

### `strupper`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
strupper(text)
```

- 参数：text：字符串。
- 返回：经当前C locale逐字节toupper的字符串。
- 限制／说明：同样按字节。
- 实现：[`str_upper`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:62)。

用法示例：

```lua
local name = strupper("abc")
```

### `strchar`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
strchar(code1, ...)
```

- 参数：每个code：0～255字节值。
- 返回：按各字节拼接的字符串。
- 限制／说明：不是Unicode码点转字符。
- 实现：[`str_char`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:97)。

用法示例：

```lua
local text = strchar(65, 66)
```

### `strbyte`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
strbyte(text[, index=1])
```

- 参数：text：字符串；index：1起始字节位置，可负数。
- 返回：该单字节整数0～255；越界报错。
- 限制／说明：单位置接口，不支持新版start/end多返回形式。
- 实现：[`str_byte`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:87)。

用法示例：

```lua
local code = strbyte("ABC", 1)
```

### `strrep`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
strrep(text, count)
```

- 参数：text：字符串；count：重复整数次数。
- 返回：拼接后的字符串。
- 限制／说明：控制输出长度，避免构造过大文本。
- 实现：[`str_rep`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:74)。

用法示例：

```lua
local line = strrep("-", 10)
```

### `format`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
format(pattern, value1, ...)
```

- 参数：pattern：printf式格式字符串；后续：对应参数，支持%d/%s/%f/%q/%%等。
- 返回：格式化字符串。
- 限制／说明：%q生成Lua可读引号字符串；格式宽度精度受原实现限制。
- 实现：[`str_format`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:533)。

用法示例：

```lua
local text = format("消耗%d两", amount)
```

### `strfind`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
strfind(text, pattern[, start=1[, plain]])
```

- 参数：text：原字符串；pattern：Lua4模式；start：字节起点；plain：非nil时纯文本匹配。
- 返回：起止字节位置及捕获值；未找到nil。
- 限制／说明：不是正则库；0也会开启plain模式。
- 实现：[`str_find`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:405)。

用法示例：

```lua
local first, last = strfind("a=b", "=", 1, 1)
```

### `gsub`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
gsub(text, pattern, replacement[, limit])
```

- 参数：text：字符串；pattern：Lua4模式；replacement：字符串（%1捕获、%%百分号）或回调函数；limit：最多替换次数。
- 返回：替换后的字符串, 实际替换次数。
- 限制／说明：按Lua4模式，不能套用Lua5新增模式或任意正则语法。
- 实现：[`str_gsub`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:472)。

用法示例：

```lua
local text, count = gsub("a,b", ",", "|")
```

### `ascii`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
ascii(text[, index=1])
```

- 参数：text：字符串；index：1起始字节位置，可负数。
- 返回：该单字节整数0～255；越界报错。
- 限制／说明：与 `strbyte` 同一实现、同一参数和返回值。单位置接口，不支持新版start/end多返回形式。
- 实现：[`str_byte`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lstrlib.c:87)。

用法示例：

```lua
local code = ascii("ABC", 1)
```

## Lua 4数学库

### `abs`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
abs(x)
```

- 参数：x：数值。
- 返回：绝对值。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_abs`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:36)。

用法示例：

```lua
local value = abs(-2)
```

### `sin`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
sin(angle)
```

- 参数：angle：角度，当前未定义RADIANS。
- 返回：正弦值。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_sin`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:41)。

用法示例：

```lua
local value = sin(90)
```

### `cos`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
cos(angle)
```

- 参数：angle：角度。
- 返回：余弦值。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_cos`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:46)。

用法示例：

```lua
local value = cos(0)
```

### `tan`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
tan(angle)
```

- 参数：angle：角度。
- 返回：正切值。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_tan`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:51)。

用法示例：

```lua
local value = tan(45)
```

### `asin`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
asin(x)
```

- 参数：x：[-1,1]数值。
- 返回：反正弦角度。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_asin`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:56)。

用法示例：

```lua
local angle = asin(1)
```

### `acos`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
acos(x)
```

- 参数：x：[-1,1]数值。
- 返回：反余弦角度。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_acos`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:61)。

用法示例：

```lua
local angle = acos(0)
```

### `atan`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
atan(x)
```

- 参数：x：数值。
- 返回：反正切角度。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_atan`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:66)。

用法示例：

```lua
local angle = atan(1)
```

### `atan2`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
atan2(y, x)
```

- 参数：y/x：平面方向分量。
- 返回：对应象限的角度。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_atan2`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:71)。

用法示例：

```lua
local angle = atan2(1, 0)
```

### `ceil`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
ceil(x)
```

- 参数：x：数值。
- 返回：向上取整。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_ceil`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:76)。

用法示例：

```lua
local value = ceil(1.2)
```

### `floor`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
floor(x)
```

- 参数：x：数值。
- 返回：向下取整。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_floor`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:81)。

用法示例：

```lua
local value = floor(1.8)
```

### `mod`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
mod(x, divisor)
```

- 参数：x：被除数；divisor：非0除数。
- 返回：C fmod余数，负数语义与新版%不一定相同。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_mod`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:86)。

用法示例：

```lua
local remainder = mod(10, 3)
```

### `frexp`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
frexp(x)
```

- 参数：x：数值。
- 返回：尾数m,指数e，x=m*2^e。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_frexp`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:126)。

用法示例：

```lua
local mantissa, exponent = frexp(8)
```

### `ldexp`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
ldexp(m, exponent)
```

- 参数：m：数值；exponent：整数指数。
- 返回：m*2^exponent。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_ldexp`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:133)。

用法示例：

```lua
local value = ldexp(0.5, 4)
```

### `sqrt`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
sqrt(x)
```

- 参数：x：非负数。
- 返回：平方根。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_sqrt`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:91)。

用法示例：

```lua
local value = sqrt(9)
```

### `min`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
min(x1, ...)
```

- 参数：至少1个数值。
- 返回：最小值。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_min`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:140)。

用法示例：

```lua
local value = min(1, 2, 3)
```

### `max`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
max(x1, ...)
```

- 参数：至少1个数值。
- 返回：最大值。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_max`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:155)。

用法示例：

```lua
local value = max(1, 2, 3)
```

### `log`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
log(x)
```

- 参数：x：正数。
- 返回：自然对数。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_log`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:101)。

用法示例：

```lua
local value = log(1)
```

### `log10`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
log10(x)
```

- 参数：x：正数。
- 返回：常用对数。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_log10`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:106)。

用法示例：

```lua
local value = log10(100)
```

### `exp`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
exp(x)
```

- 参数：x：数值。
- 返回：e的x次幂。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_exp`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:111)。

用法示例：

```lua
local value = exp(1)
```

### `deg`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
deg(radians)
```

- 参数：radians：弧度。
- 返回：角度。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_deg`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:116)。

用法示例：

```lua
local angle = deg(PI)
```

### `rad`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
rad(degrees)
```

- 参数：degrees：角度。
- 返回：弧度。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_rad`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:121)。

用法示例：

```lua
local angle = rad(180)
```

### `random`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
random() 或 random(upper) 或 random(lower, upper)
```

- 参数：无参数：[0,1)小数；1参数：正整数上限；2参数：整数上下限且lower<=upper。
- 返回：随机数；有上下限时包含两端。
- 限制／说明：全局函数，不使用 math.
- 实现：[`math_random`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:169)。

用法示例：

```lua
local roll = random(1, 100)
```

### `randomseed`

状态：Lua 4标准库。作用域：所有已初始化的Lua状态。

```lua
randomseed(seed)
```

- 参数：seed：整数随机种子。
- 返回：无。
- 限制／说明：全局函数，不使用 math.会调用进程CRT srand影响共享随机序列，不在NPC对话或每次合成中重新播种。
- 实现：[`math_randomseed`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/lmathlib.c:197)。

用法示例：

```lua
-- 仅可控测试：randomseed(12345)
```

## Lua 4文件、系统与诊断库

### `openfile`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
openfile(path, mode)
```

- 参数：path：真实文件路径；mode：C fopen模式，如rb。
- 返回：成功文件句柄；失败nil,错误字符串,errno。
- 限制／说明：不走PAK资源优先级；读取配置优先ReadTabFile，路径不得来自玩家输入。
- 实现：[`io_open`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:173)。

用法示例：

```lua
local file, message = openfile(path, "rb")
```

### `closefile`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
closefile(file)
```

- 参数：file：openfile返回的有效句柄。
- 返回：成功非nil userdata标记；失败nil,错误,errno。
- 限制／说明：标准输入输出句柄不会真正关闭。
- 实现：[`io_close`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:157)。

用法示例：

```lua
closefile(file)
```

### `read`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
read([file,] format1, ...)
```

- 参数：file：可选句柄，默认_INPUT；format：默认*l一行，*n数值、*a全部、*w单词或整数个字节。
- 返回：成功读取的各项值；无内容时无返回（赋值为nil）。
- 限制／说明：只读到失败前的结果；不要省略file读服务器stdin。旧read pattern未开启。
- 实现：[`io_read`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:385)。

用法示例：

```lua
local line = read(file, "*l")
```

### `write`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
write([file,] value1, ...)
```

- 参数：file：可选句柄，默认_OUTPUT；后续：数字或字符串。
- 返回：成功非nil userdata标记；失败路径当前只返回栈顶errno，不能当统一nil契约。
- 限制／说明：不自动换行；文件副作用不能由NPC物品事务回滚。
- 实现：[`io_write`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:441)。

用法示例：

```lua
write(file, "text")
```

### `flush`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
flush([file])
```

- 参数：file：可选有效句柄；省略对C所有输出流fflush(NULL)。
- 返回：成功非nil标记；失败nil,错误,errno。
- 限制／说明：不会替代角色存档。
- 实现：[`io_flush`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:487)。

用法示例：

```lua
flush(file)
```

### `seek`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
seek(file[, mode="cur"[, offset=0]])
```

- 参数：file：句柄；mode：set/cur/end；offset：字节偏移。
- 返回：成功为新文件偏移；失败nil,错误,errno。
- 限制／说明：不是资源压缩流寻址API。
- 实现：[`io_seek`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:465)。

用法示例：

```lua
local offset = seek(file, "set", 0)
```

### `readfrom`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
readfrom([pathOrHandle])
```

- 参数：pathOrHandle：文件路径或兼容的已打开句柄；省略关闭旧输入并恢复stdin。
- 返回：成功新当前输入句柄；失败nil,错误,errno。
- 限制／说明：改变当前Lua状态_INPUT；读资源优先ReadTabFile。
- 实现：[`io_readfrom`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:206)。

用法示例：

```lua
local file = readfrom(path)
```

### `writeto`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
writeto([pathOrHandle])
```

- 参数：pathOrHandle：路径或兼容句柄；路径以w模式打开并截断；省略恢复stdout。
- 返回：成功新输出句柄；失败nil,错误,errno。
- 限制／说明：改变_OUTPUT并可能清空文件；NPC业务不使用它写配置／存档。
- 实现：[`io_writeto`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:211)。

用法示例：

```lua
-- 受控工具脚本：local file = writeto(path)
```

### `appendto`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
appendto(path)
```

- 参数：path：文件路径，以a模式打开。
- 返回：成功输出句柄并设置_OUTPUT；失败nil,错误,errno。
- 限制／说明：业务日志用WriteLog，不自建另一套日志路径。
- 实现：[`io_appendto`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:216)。

用法示例：

```lua
-- 工具脚本：local file = appendto(path)
```

### `clock`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
clock()
```

- 参数：无。
- 返回：C clock()/CLOCKS_PER_SEC数值。
- 限制／说明：含义取决于平台CRT，不能当Unix时间、可靠墙钟或游戏tick。
- 实现：[`io_clock`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:535)。

用法示例：

```lua
local value = clock()
```

### `date`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
date([format="%c"])
```

- 参数：format：C strftime格式字符串，仅格式化当前本地时间。
- 返回：日期字符串；格式结果无效时Lua错误。
- 限制／说明：不是新版os.date；不支持时间戳参数或*t日期表。
- 实现：[`io_date`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:541)。

用法示例：

```lua
local text = date("%Y-%m-%d %H:%M:%S")
```

### `getenv`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
getenv(name)
```

- 参数：name：环境变量名。
- 返回：字符串，未定义nil。
- 限制／说明：普通NPC无需读取主机环境。
- 实现：[`io_getenv`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:529)。

用法示例：

```lua
local value = getenv("TEMP")
```

### `remove`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
remove(path)
```

- 参数：path：实际文件路径。
- 返回：成功非nil标记；失败nil,错误,errno。
- 限制／说明：会删除文件，不能回滚；NPC业务不应调用。
- 实现：[`io_remove`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:511)。

用法示例：

```lua
-- 工具用途：remove(path)
```

### `rename`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
rename(oldPath, newPath)
```

- 参数：两个实际文件路径。
- 返回：成功非nil标记；失败nil,错误,errno。
- 限制／说明：修改文件系统，不能用于角色存档迁移。
- 实现：[`io_rename`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:516)。

用法示例：

```lua
-- 工具用途：rename(oldPath, newPath)
```

### `tmpname`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
tmpname()
```

- 参数：无。
- 返回：C tmpnam生成的临时名称字符串，不创建文件。
- 限制／说明：项目测试文件仍须显式放Test，不能按临时名乱写其他目录。
- 实现：[`io_tmpname`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:522)。

用法示例：

```lua
local path = tmpname()
```

### `setlocale`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
setlocale(locale[, category="all"])
```

- 参数：locale：locale名称；category：all/collate/ctype/monetary/numeric/time。
- 返回：设置后的locale字符串，失败nil。
- 限制／说明：影响进程C运行库，NPC业务不改locale。
- 实现：[`setloc`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:555)。

用法示例：

```lua
-- 系统用途：setlocale("C", "numeric")
```

### `execute`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
execute(command)
```

- 参数：command：系统命令字符串。
- 返回：C system命令返回码。
- 限制／说明：当前实际注册且可调用外部命令；不用于NPC业务，不拼接玩家输入。
- 实现：[`io_execute`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:505)。

用法示例：

```lua
-- NPC脚本禁止使用；仅记录运行库暴露了此函数。
```

### `exit`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
exit(...)
```

- 参数：当前不读取参数。
- 返回：无。
- 限制／说明：本项目把真正exit调用注释，不能终止进程，也不等同return／CloseDialog。
- 实现：[`io_exit`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:567)。

用法示例：

```lua
-- exit()只打印三行LUASCRIPT_ERROR提示。
```

### `debug`

状态：Lua 4标准库；按用途限制。作用域：所有已初始化的Lua状态。

```lua
debug()
```

- 参数：无。
- 返回：输入cont或EOF后无返回。
- 限制／说明：启动stdin交互循环，可能阻塞服务线程；并非已开启Lua调试库。
- 实现：[`io_debug`](F:/mxd/Jx/Sources/Library/LuaLib/src/baselib/liolib.c:579)。

用法示例：

```lua
-- 不在服务端NPC执行debug()。
```

## 世界脚本可用名字完整清单

`WorldScriptFuns` 仅注册下列9个游戏接口，此外仍有Lua标准库。普通NPC表中的其他角色、任务、物品、Mission接口不能假定在世界脚本存在。

| 名字 | 参数／返回及限制 |
| --- | --- |
| `AddLocalNews` | `AddLocalNews(textOrId)`；textOrId：文本ID或公告字符串，字符串原64字节缓冲。 返回：无。见上方同名条目。 |
| `AddLoaclTimeNews` | `AddLoaclTimeNews(textOrId, year, month, day, hour, minute)`；第1项：文本ID或字符串；其余为本地时间的年、月、日、时、分。 返回：无。见上方同名条目。 |
| `AddLocalCountNews` | `AddLocalCountNews(textOrId, duration)`；textOrId：文本ID或字符串；duration：原NEWSMESSAGE_COUNTING计时整数，<=0改1。 返回：无。见上方同名条目。 |
| `Msg2SubWorld` | `Msg2SubWorld(text)`；text：字符串。 返回：无。见上方同名条目。 |
| `Msg2IP` | `Msg2IP(ip, id, text[, parameterId=0])`；ip：IPv4字符串；id：原IP路由ID；text：消息；parameterId：GM附加路由参数。 返回：无。见上方同名条目。 |
| `SubWorldID2Idx` | `SubWorldID2Idx(mapId)`；mapId：地图资源ID。 返回：本服务器已加载的运行时世界索引；未找到-1。见上方同名条目。 |
| `GetServerName` | `GetServerName()`；无。 返回：操作系统主机名，失败为空字符串。见上方同名条目。 |
| `KickOutPlayer` | `KickOutPlayer(name)`；name：已在线角色名，区分大小写精确匹配。 返回：无。见上方同名条目。 |
| `KickOutAccount` | `KickOutAccount(account)`；account：账号名，按原strcmpi不区分大小写。 返回：无。见上方同名条目。 |

## 未提供的常见名字与开发步骤

`Pay()`、`AddNormalItem()`、`Talk()`、`Msg2Player()`已提供；但它们不能代替原子事务入口。没有统一的“Lua直接保存角色”或“任意状态回滚”API，保存、同步、拒绝请求和完整恢复要由C++原通用流程负责。

`GetMorphType`、`PolyMorph`、`GetTeamMember`、`GetNpcTask`、`SetNpcTask`、`SetNpcTimer`等资源脚本中的候选名，不在本次生效的Game/World注册表中。若当前脚本及其Include没有自行定义，就不能直接调用，也不能用固定返回值或空函数冒充实现。完整候选清单另见 `DevDoc/20261107-Lua接口缺口候选.md`。

开发时依次核对：原NPC挂接脚本及其Include → 本表参数和状态 → 原Resources业务规则 → 已有通用接口及事务边界。已有脚本就修改原脚本；确认Resources中没有时，按AGENTS资源审批规则申请新增。需要补C++接口时，接口面向通用能力，完成真实校验、同步、存档、失败回滚后再接Lua，并维护本文。

## 普通宠物接口（2026-11-08）

状态：已通过原生测试；具体TLS和客户端验收进展见同日宠物开发日记。写接口只允许在已有通用事务内执行，失败应终止当前回调，不能继续发奖或提示成功。

| functionId | 技能 | 权限位 |
| --- | --- | --- |
| 0 | 拾金捡宝 | 0x01 |
| 1 | 弃物清理 | 0x02 |
| 2 | 灵宠献技 | 0x04 |
| 3 | 灵力修复 | 0x08 |
| 4 | 灵眼观真 | 0x10 |

用户正式服记录中的前三项已学（拾金捡宝、弃物清理、灵力修复）对应0x0B；其余两项未学。

2026-11-08执行状态：拾金捡宝、弃物清理、灵力修复均已获用户客户端测试通过。随后按用户要求将运行规则迁移到 `Resources/script/宠物技能.lua`，通过原 `settings/pet.ini` 的 `[Normal] FunctionScript` 引用；寿星仍负责原有购买、学习和对话。C++保留权限、物品归属、真实操作、同步与存档。灵宠献技、灵眼观真本轮只迁移现有启用权限判断，实际献技和查看他人血量效果仍未实现。迁移后的编译、原生和TLS验证与客户端复测状态见[宠物技能Lua迁移](20261108-宠物技能Lua迁移.md)。

### `SaleEx`

```lua
SaleEx(shopId)
```

- 参数：shopId：原 settings/buysell.txt 的1起始商店号，整数1～65535，最终还须通过实际商店范围检查。
- 返回：无。参数不合法不打开商店。
- 作用域与限制：游戏／玩家脚本。以原物品模板“铜钱价格”结算，铜钱为背包中的任务物品(4,47)，不是现金。检查当前商店会话、距离、货币和背包容量；失败不扣费。当前购买落背包，铜钱会话不接受现金出售或回购。
- 实现：[LuaSaleEx](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2164)。

用法示例：

```lua
SaleEx(22)
```

### `GetPetType`

```lua
GetPetType()
```

- 参数：无。
- 返回：普通宠物类型编号；没有宠物或没有玩家上下文返回0。
- 作用域与限制：游戏／玩家脚本。类型取原 settings/pets.txt 的Type；不是NPC模板index。
- 实现：[LuaGetPetType](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2191)。

用法示例：

```lua
local petType = GetPetType()
```

### `GetPetHonor`

```lua
GetPetHonor()
```

- 参数：无。
- 返回：当前普通宠物亲密度，0～100；没有宠物返回0。
- 作用域与限制：游戏／玩家脚本。新宠物初值由对应原认主脚本传给GenPet；绿飞飞为50，不统一改为100。
- 实现：[LuaGetPetHonor](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2198)。

用法示例：

```lua
if GetPetHonor() == 0 then
	Talk(1, "no", "请到寿星处唤醒宠物。")
end
```

### `GetPetColor`

```lua
GetPetColor()
```

- 参数：无。
- 返回：宠物原始颜色值0～255；没有宠物返回0。
- 作用域与限制：游戏／玩家脚本。返回协议中的颜色字节，不是客户端换算后的角度。
- 实现：[LuaGetPetColor](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2205)。

用法示例：

```lua
local color = GetPetColor()
```

### `GetPetName`

```lua
GetPetName()
```

- 参数：无。
- 返回：当前宠物GBK名字；没有宠物或玩家上下文返回空字符串。
- 作用域与限制：游戏／玩家脚本。这是查询接口，不会修改名字。
- 实现：[LuaGetPetName](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2212)。

用法示例：

```lua
local name = GetPetName()
```

### `IsPetReleased`

```lua
IsPetReleased()
```

- 参数：无。
- 返回：已召出返回1；已收回、没有宠物或玩家上下文返回0。
- 作用域与限制：游戏／玩家脚本。须显式比较数字；Lua4中的0为真。
- 实现：[LuaIsPetReleased](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2219)。

用法示例：

```lua
if IsPetReleased() == 1 then
	ReleasePet(0)
end
```

### `GenPet`

```lua
GenPet(name, type, honor, color)
```

- 参数：name：非空GBK字符串，最多31字节，不得含内嵌NUL；type：原pets.txt中存在的1～127整数；honor：初始亲密度整数1～100；color：整数0～255。
- 返回：成功返回1；失败标记事务失败并触发Lua错误。
- 作用域与限制：游戏／玩家脚本。需要当前玩家的NPC／道具事务；已有宠物时拒绝覆盖。默认召出。亲密度、类型、名字、颜色和权限随角色扩展存档，不由Lua直接操作数据库。仅创建普通宠物，不包含仙兽培养。
- 实现：[LuaGenPet](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2226)。

用法示例：

```lua
GenPet("绿飞飞", 4, 50, 0)
```

### `DelPet`

```lua
DelPet()
```

- 参数：无，不接受额外参数。
- 返回：成功返回1；无宠物或无有效事务时触发Lua错误。
- 作用域与限制：游戏／玩家脚本。只清除普通宠物状态；是否消耗召役伏魔铃由原Lua业务决定。删除和同次材料扣除可一起回滚。
- 实现：[LuaDelPet](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2241)。

用法示例：

```lua
DelPet()
```

### `SetPetType`

```lua
SetPetType(type)
```

- 参数：type：原pets.txt中存在的1～127整数。
- 返回：成功返回1；失败触发Lua错误并使事务失败。
- 作用域与限制：游戏／玩家脚本。须已有宠物；更换外观类型，保留名字、亲密度、颜色及已学权限，不是创建第二只宠物。
- 实现：[LuaSetPetType](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2255)。

用法示例：

```lua
SetPetType(4)
```

### `SetPetHonor`

```lua
SetPetHonor(honor)
```

- 参数：honor：整数0～100。
- 返回：成功返回1；越界、小数、非有限值、无宠物等失败触发Lua错误。
- 作用域与限制：游戏／玩家脚本。须在事务内，设为0会同时收回宠物；不会自动把0改为100。在线陪伴、离线衰减和喂食计时尚未适配，不能用猜测的时间单位调用。
- 实现：[LuaSetPetHonor](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2267)。

用法示例：

```lua
SetPetHonor(20)
```

### `ReleasePet`

```lua
ReleasePet(released)
```

- 参数：released：整数1表示召出，整数0表示收回。
- 返回：成功返回1；无宠物、亲密度0时召出或参数错误会触发Lua错误。
- 作用域与限制：游戏／玩家脚本。须在事务内；只改召出状态，不改亲密度、已学技能或物品。事务成功后同步，召出和收回均存档。
- 实现：[LuaReleasePet](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2279)。

用法示例：

```lua
ReleasePet(1)
```

### `GetPetFuncAuthority`

```lua
GetPetFuncAuthority(functionId)
```

- 参数：functionId：整数0～4，含义见下表。
- 返回：已学返回1，未学、非法编号、无宠物／玩家返回0。
- 作用域与限制：游戏／玩家脚本。查询学习权限，不表示自动技能正在执行。
- 实现：[LuaGetPetFuncAuthority](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2291)。

用法示例：

```lua
if GetPetFuncAuthority(0) == 1 then
	Talk(1, "no", "宠物已学会拾金捡宝。")
end
```

### `SetPetFuncAuthority`

```lua
SetPetFuncAuthority(functionId, enabled)
```

- 参数：functionId：整数0～4；enabled：整数1为学会，整数0为取消。
- 返回：成功返回1；非法编号、非整数、无宠物／事务时触发Lua错误。
- 作用域与限制：游戏／玩家脚本。只设置并存档学习权限。费用、召出条件和弃物清理的前置条件由原寿星Lua检查；编号0的自动拾取和编号1的弃物清理执行链已接通，实际执行还须宠物召出、亲密度大于0且对应技能已启用；拾取分类与清理阈值由客户端A4/4、A4/5设置，C++校验、同步和保存。灵力修复已接通，运行条件与通知由独立宠物技能Lua决定；献技及查看他人血量的实际效果仍待接通。
- 实现：[LuaSetPetFuncAuthority](F:/mxd/Jx/Sources/Core/Src/ScriptFuns.cpp:2301)。

用法示例：

```lua
SetPetFuncAuthority(0, 1)
```

## 文档核验和维护

依据当前 `Sources/Core/Src/ScriptFuns.cpp` 注册表及正文、`KSortScript.cpp`、`KCore.cpp`、`KPlayer.cpp`、`KItemList.cpp`、`KSkillList.cpp`、`KTaskFuns.h`、`KMission.h`，以及 `Sources/Engine/Src/KLuaScript.cpp` 的 `RegisterStandardFunctions` 和原Lua库源码核对。

本轮仅整理文档和开发规则，没有修复上述历史接口、改变业务、运行有副作用的游戏API、重新编译或重启服务。注册覆盖检查按服务端条件分支、注释过滤、同名覆盖和标准库实际打开情况执行，排除客户端专用分支；不以静态覆盖代替真实客户端行为验收。

复核工具：`F:\mxd\Jx\Test\LuaFunctionTable20261107\Audit.py`；注册快照：同目录 `inventory.json`。运行覆盖检查：

```powershell
F:\mxd\FsbFastApi\.venv\Scripts\python.exe -X utf8 F:\mxd\Jx\Test\LuaFunctionTable20261107\Audit.py
```

新增／改名接口必须有独立条目；别名也要可检索并说明同一实现。每项至少维护签名、参数、返回值、示例、作用域、限制和源码位置。涉及新版协议的旧API，只有完成实际验证后才能更新相应状态。

### 普通宠物同步补充（2026-11-08）

`SetPetFuncAuthority(functionId, enabled)`仍只负责学习权限，撤销学习会一并清除该项Active。事务提交后通过DB/0、DB/13 kind4同步；不能把DB/0、DB/1的+11字段视为唯一的技能按钮状态。客户端A4/7请求负责启用、关闭已学技能，C++校验后更新Active并存档；Lua不需要通过再次学习来启用技能，不重复扣费。`ReleasePet`保留已学及启用位，重登、切图由C++恢复显示。自动拾取、弃物清理的实际执行链已在KPlayer中接通：自动拾取受分类筛选约束，手动拾取不受宠物拾取筛选约束；弃物清理只处理本次拾取的可出售普通白蓝装备，遵守不卖标志、需求等级与蓝色属性条数阈值，不扫描现有背包。2026-11-08正式服日志复核后，自动出售的原装备会保留到14格商店回购栏，可通过普通商店回购；满栏只淘汰最旧一件，拾取／出售使用原生91通知。灵力修复已按穿戴装备0耐久、NPC同价接通，原生及TLS回归通过，界面待用户验收；献技等其他技能效果仍待接通。本次没有增加或改变Lua函数签名。


## 玩家自动回调与通用物品接口（2026-11-08）

普通宠物运行脚本位于 `Resources/script/宠物技能.lua`。它不是寿星对话脚本，也不是新的服务器。`settings/pet.ini` 的 `[Normal] FunctionScript` 指向该路径；修改路径需重启GameServer，修改函数后可用原脚本重载机制重新加载或重启。脚本缺失、回调不存在或报错时拒绝该次操作，不回退C++硬编码规则。

C++自动回调使用当前玩家上下文，保存并恢复Lua栈、`playerindex/playerid/subworldindex`；不会改写NPC对白的 `ActionScriptID`、选项和等待状态。同一玩家回调不可重入。回调必须返回非负整数；不能返回nil或小数。返回0表示不执行。

自动回调的只读作用域仅允许现有白名单中的查询、位操作和纯计算函数；不允许Pay、发奖、对白、Include、外部IO、动态执行或数据库。物品动作作用域额外允许 `RepairItem` 与 `Msg2Player`。本轮只支持每次对当前指定物品安排一次修理，并非任意NPC业务的通用事务替代品。

当前独立脚本定义的回调（这些是Lua函数，不计入引擎注册函数数）：

| 回调 | 参数 | 返回及用途 |
| --- | --- | --- |
| PetCanEnableFunction | 技能编号0～4 | 1允许启用，0拒绝；检查宠物、召出、亲密度和学习权限 |
| PetFunctionEnabled | 技能编号0～4 | 1当前可执行，0不执行；增加当前启用位检查 |
| PetPickupInterval | 无需使用参数 | 返回扫描间隔，单位为游戏tick；现值20，0停止扫描 |
| PetPickupLogging | 无需使用参数 | 返回拾取设置第8位0/1，控制服务端拾取诊断日志 |
| PetCanPickItem | 当前候选物品网络ID | 1允许拾取，0拒绝；按拾取排除项分类 |
| PetCanSellItem | 当前候选物品网络ID | 1允许清理，0保留；绑定、等级、蓝属性条数等规则 |
| PetRepairItem | 当前穿戴物品网络ID | 1提交已安排的修理，0放弃；目前只修0耐久 |

### `GetPetFuncActive`

```lua
GetPetFuncActive(functionId)
```

- 参数：`functionId`为整数0～4，编号同前述宠物技能表。
- 返回：当前保存的启用位，启用1、关闭0；参数不符或缺少玩家返回0。
- 作用域：游戏／玩家脚本、自动回调。只查询启用位，不替代已学权限、亲密度和召出条件判断，不改变设置或存档。
- 示例：`if GetPetFuncAuthority(3) == 1 and GetPetFuncActive(3) == 1 then ... end`。
- 实现：`Sources/Core/Src/ScriptFuns.cpp::LuaGetPetFuncActive`。

### `GetPetSettings`

```lua
GetPetSettings()
```

- 参数：无。
- 返回：当前玩家宠物设置快照 `{PickMask=数值, SellMask=数值}`；参数不符或没有玩家上下文返回nil。修改该Lua表不会修改服务端设置。
- `PickMask`一基位号：1不拾白装，2不拾蓝装，3原额外属性排除位，4材料／矿物，5药品，6技能书，7原保留位，8拾取记录开关，9法宝（护符表）；第10位保留原请求值。第3、7、10位本轮不新增未经确认的过滤效果。
- `SellMask`：第1、2位分别为不卖白装／蓝装；第2字节为保护需求等级，第3字节为保护蓝色属性条数。需求等级大于等于阈值保留；蓝色属性条数大于等于阈值保留。
- 作用域：游戏／玩家脚本、自动回调。设置仍由原A4请求经C++校验、同步、存档；没有任意掩码写入接口。
- 示例：`local settings = GetPetSettings(); local level = GetByte(settings.SellMask, 2)`。
- 实现：`Sources/Core/Src/ScriptFuns.cpp::LuaGetPetSettings`。

### `GetItemInfo`

```lua
GetItemInfo(itemNetworkId)
```

- 参数：`itemNetworkId`为1～4294967295的整数，必须是物品网络DWORD ID；不是模板ID、服务端数组下标，也不是NPC ID。
- 返回：下表只读快照；参数不符、找不到或无权读取返回nil。修改Lua表不会修改真实物品。
- 可读范围：当前玩家拥有的物品；自动回调还可读取C++本次选中的候选掉落物。不能传其他玩家ID绕过归属。

| 字段 | 含义 |
| --- | --- |
| Name | 原物品GBK名称 |
| Genre / DetailType | 原大类／子类编号 |
| Price | 模板价格，非商店最终买价 |
| Bound | 绑定1、未绑定0 |
| GenerationFlags | 原生成标志整数，保留原含义 |
| Upgraded | 有原升级记录1、否则0 |
| Durability / MaxDurability | 原当前／上限耐久，无可磨损耐久的物品保留原值 |
| RepairPrice | 原NPC同价修至满的费用；正式执行时重新校验 |
| Place | 当前玩家列表中的原位置编号；2穿戴、3背包；不在玩家列表为-1 |
| MagicCount | 有效魔法属性条数 |
| RequireLevel | 装备需求等级 |
| TemplateValid | 找到对应装备原模板1、否则0 |
| TemplateSeries | 装备原模板系列字段；不是玩家五行，模板无效时不能单独使用该值 |

- 作用域：游戏／玩家脚本、自动回调。只读取快照，不构成拾取、出售或修理授权；执行仍由C++再次核对。
- 示例：`local item = GetItemInfo(itemNetworkId); if item ~= nil and item.Durability == 0 then ... end`。
- 实现：`Sources/Core/Src/ScriptFuns.cpp::LuaGetItemInfo`。

### `RepairItem`

```lua
RepairItem(itemNetworkId)
```

- 参数：本次C++物品动作回调选中的物品网络ID，整数范围同GetItemInfo。
- 返回：已通过预检并安排修理返回1；可修耐久、随身余额等条件不符返回0。此时尚未扣费或修改耐久。
- 作用域：仅显式物品动作回调；不能在普通NPC对白、道具事务或只读回调中直接调用。上下文不符、重复安排、非当前物品ID会抛出Lua错误，整次动作不提交。
- 提交规则：Lua成功返回整数1后，C++再次校验角色会话、状态、物品归属和随身现金，再调用与NPC相同的公共计费 `KItem::GetRepairPrice`：读取原 `abraderate.ini` 的价格比例、魔法条数系数，计入对应版本 `greenaddon.txt` 的套装附加条数；耐久为0时还应用改造次数与原模板需求等级系数。随后统一扣费、修满、必要时重算属性、同步0xA6和现金0x77、标记存档。不用银行钱，不删除或重新生成物品。Lua不计算另一套费用，也不要再调用 `Pay` 重复扣款。
- 失败规则：Lua报错、nil、小数、返回0、重复安排或外来ID均不扣款、不改耐久、不发送成功通知。该规则覆盖本次延迟修理和通知；不是任意Lua副作用的回滚承诺。
- `Msg2Player`在此回调内延迟发送，只有安排的修理实际提交成功才发出；不调用 `Pay`，避免重复扣费。
- 示例（仅在物品动作回调内）：

```lua
function OnRepairSelectedItem(itemNetworkId)
	local item = GetItemInfo(itemNetworkId)
	if item == nil or item.Durability ~= 0 then
		return 0
	end
	if RepairItem(itemNetworkId) ~= 1 then
		return 0
	end
	Msg2Player(format("已修复%s。", item.Name))
	return 1
end
```

- 实现：`Sources/Core/Src/ScriptFuns.cpp::LuaRepairItem`、`KPlayer.cpp::CheckRepairItem/RepairItem`。

## 2026-11-11新增通用转盘与形态接口

### `Roulette`

```lua
Roulette(result)
```

- 作用域：NPC脚本。result为服务端确定的0～11整数，对应原客户端12格；不能由客户端提交中奖值。
- 返回：成功排入UI事务队列1，参数或当前脚本的`Finished`回调不可用时0；事务内失败报错回滚。
- 发送当前客户端0x79／UI13窗口。第二WORD固定15，已通过正式服封包及GUI IDA确认供转盘Tips配置选择，不是等待15秒。
- 客户端91字节0x67完成请求必须带匹配的当前会话token及Action=4；服务端只调用原绑定脚本`Finished()`，会话一次消费。普通对话仍走B8/45。
- 不自动扣材料、记次数或发奖，这些必须由NPC原Lua决定；断线恢复由原Lua任务值记录待领奖结果并重新打开窗口。

```lua
SetTask(resultTask, result)
SetTask(pendingTask, 1)
assert(Roulette(result) == 1)
function Finished()
    if GetTask(pendingTask) ~= 1 then return end
    SetTask(pendingTask, 0)
    Earn(10000)
end
```

### `GetMorphType`

```lua
local shape = GetMorphType()
```

- 作用域：NPC／玩家脚本；无参数。返回有效变身模板ID，无有效形态时-1。该接口只读，不改变到期时间。
- 示例：`if GetMorphType() == -1 then ... end`。当前道具事务白名单未开放。

### `PolyMorph`

```lua
PolyMorph(type, canCast, safeguard, speed, seconds)
```

- 作用域：NPC脚本；必须5个整数。type为有效NPC资源模板，canCast为0／1；当前仅实现safeguard=0、speed=-1，其他组合明确失败。
- seconds为1～360000；`PolyMorph(-1,1,0,-1,0)`清除形态。不能以不存在的模板替代外观。
- 返回成功1、失败0；事务内验证后延迟提交，失败不扣资源也不改形态。canCast=0禁止主动施法。
- 状态通过当前客户端4B同步，过图／重登继续，按绝对到期时间还原；角色扩展SPG1 v4兼容读取旧1～3版本。
- 示例：`assert(PolyMorph(99, 0, 0, -1, 3600) == 1)`。

### `GetLastSkillIdx`

```lua
local last = GetLastSkillIdx()
```

- 作用域：NPC／玩家脚本；无参数。返回最后一个非空技能槽下标；无技能时0。是内部槽号，不是技能ID，也不等于已学技能总数。
- 示例：从1到last循环，配合`IsSkillActivedByIdx`及`GetSkillId`挑选已学技能；当前道具事务白名单未开放。

### `IsSkillActivedByIdx`

```lua
local learned = IsSkillActivedByIdx(index)
```

- 作用域：NPC／玩家脚本。index为内部1起始技能槽号；有效、有技能且学习状态为3时返回1，否则0。
- 只查询学习权限，不表示技能正在施放、光环打开或冷却结束；不要用于绕过技能冷却。
- 示例：`if IsSkillActivedByIdx(i) == 1 then local id = GetSkillId(i) end`。当前道具事务白名单未开放。
