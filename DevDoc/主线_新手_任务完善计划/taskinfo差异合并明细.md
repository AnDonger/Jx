# taskinfo差异合并明细

日期：2026-11-22。状态：已写入项目有效Resources；未部署客户端或重启服务端。

## 文件与规则

- 目标：`F:\mxd\Jx\Resources\settings\taskinfo.tbl`。
- 来源：`F:\mxd\Jx\Resources\资源归档\未确认原名\serverlist.pak\02541-bb0aeddd.bin`。
- 只追加缺失任务节；保留所有原有节及字段值。已有节内没有缺少的字段，因此本轮没有在已有节内部插入字段。
- 同名字段冲突保留当前有效值，防止退回旧颜色标记、删除寻路链接或引入旧材料描述。
- 归档原文不修改，不将其加入运行时默认资源来源。

## 数量与校验

| 项目 | 合并前 | 合并后／结果 |
| --- | --- | --- |
| 任务节 | 84 | 132 |
| 文件字节数 | 142169 | 190493 |
| 新增任务节 | — | 48 |
| 新增字段 | — | 359 |
| 已有字段内容冲突 | — | 358处，保留当前值 |
| 纯空白差异 | — | 1处，保留当前值 |
| 编码／换行 | GBK／CRLF | 保持 |
| 原文件字节 | — | 完整保留为合并文件前缀 |

原有效表SHA256：`67edac910e316c802781421eba6d0562f879b35fc33d96f355515458794d0164`。

来源归档SHA256：`2aa49118e26cc25b1b0980d03b711d896b28504d8bbf1667cd22e335602d94d3`。

合并后SHA256：`2cae825b76841b54c17aabcf6d6f95ab5bd540966a8b2a15b99320099c3864ee`。

备份：`F:\mxd\Jx\Test\QuestResources20261122\TaskInfoMerge\taskinfo.before.tbl`。

候选与最终内容一致；无重复节／重复键。原84节字段逐项保持不变，新增48节字段逐项与归档一致，均有Title及Step。上述属于文件和语义校验，尚未进行新增任务文字的真实客户端显示验收。

## 补入任务节

- 甲士：Task_111～Task_126。
- 道士：Task_141～Task_156。
- 异人：Task_171～Task_186。

## 保留当前值的冲突字段

下面按任务节列出全部358处冲突的字段名。原值与归档值的逐条完整内容保存在 `F:/mxd/Jx/Test/QuestResources20261122/TaskInfoMerge/merge-result.json` 的 conflicts_preserved 中；不是省略未处理的差异。

| 任务节 | 差异字段数 | 字段名 |
| --- | --- | --- |
| Task_1 | 8 | Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6, Step_7 |
| Task_2 | 4 | Step_0, Step_1, Step_2, Step_3 |
| Task_3 | 2 | Intro, Step_0 |
| Task_4 | 1 | Step_0 |
| Task_5 | 5 | Intro, Step_0, Step_1, Step_2, Step_3 |
| Task_6 | 2 | Intro, Step_0 |
| Task_7 | 11 | Intro, Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6, Step_7, Step_8, Step_9 |
| Task_8 | 8 | Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6, Step_7 |
| Task_9 | 2 | Intro, Step_0 |
| Task_10 | 3 | Step_0, Step_1, Step_2 |
| Task_11 | 3 | Step_0, Step_8, Step_9 |
| Task_12 | 2 | Intro, Step_0 |
| Task_13 | 8 | Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6, Step_7 |
| Task_14 | 5 | Step_0, Step_1, Step_2, Step_3, Step_4 |
| Task_15 | 2 | Intro, Step_0 |
| Task_16 | 6 | Step_0, Step_1, Step_2, Step_3, Step_4, Step_5 |
| Task_17 | 8 | Intro, Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6 |
| Task_18 | 2 | Intro, Step_0 |
| Task_20 | 13 | Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6, Step_7, Step_8, Step_9, Step_10, Step_11, Step_12 |
| Task_21 | 8 | Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6, Step_7 |
| Task_22 | 5 | Step_0, Step_1, Step_2, Step_3, Step_5 |
| Task_23 | 4 | Intro, Step_1, Step_2, Step_3 |
| Task_24 | 2 | Intro, Step_0 |
| Task_25 | 4 | Intro, Step_0, Step_1, Step_2 |
| Task_26 | 4 | Intro, Step_0, Step_1, Step_4 |
| Task_27 | 37 | Title, Intro, Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6, Step_7, Step_8, Step_9, Step_10, Step_11, Step_12, Step_13, Step_14, Step_15, Step_16, Step_17, Step_18, Step_19, Step_20, Step_21, Step_22, Step_23, Step_24, Step_25, Step_26, Step_27, Step_28, Step_29, Step_30, Step_31, Step_32, Step_33, Step_34 |
| Task_28 | 40 | Title, Intro, Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6, Step_7, Step_8, Step_9, Step_10, Step_11, Step_12, Step_13, Step_14, Step_15, Step_16, Step_17, Step_18, Step_19, Step_20, Step_21, Step_22, Step_23, Step_24, Step_25, Step_26, Step_27, Step_28, Step_29, Step_30, Step_31, Step_32, Step_33, Step_34, Step_35, Step_36, Step_37 |
| Task_29 | 35 | Title, Intro, Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6, Step_7, Step_8, Step_9, Step_10, Step_11, Step_12, Step_13, Step_14, Step_15, Step_16, Step_17, Step_18, Step_19, Step_20, Step_21, Step_22, Step_23, Step_24, Step_25, Step_26, Step_27, Step_28, Step_29, Step_30, Step_31, Step_32 |
| Task_30 | 14 | Step_7, Step_8, Step_9, Step_10, Step_11, Step_12, Step_17, Step_18, Step_19, Step_20, Step_25, Step_26, Step_27, Step_28 |
| Task_31 | 46 | Step_5, Step_6, Step_7, Step_8, Step_9, Step_10, Step_11, Step_12, Step_13, Step_14, Step_15, Step_16, Step_17, Step_18, Step_19, Step_21, Step_22, Step_23, Step_24, Step_25, Step_26, Step_27, Step_28, Step_29, Step_30, Step_31, Step_32, Step_33, Step_34, Step_35, Step_36, Step_37, Step_38, Step_39, Step_40, Step_41, Step_42, Step_43, Step_44, Step_45, Step_46, Step_47, Step_48, Step_49, Step_50, Step_51 |
| Task_33 | 46 | Step_5, Step_6, Step_7, Step_8, Step_9, Step_10, Step_11, Step_12, Step_13, Step_14, Step_15, Step_16, Step_17, Step_18, Step_19, Step_21, Step_22, Step_23, Step_24, Step_25, Step_26, Step_27, Step_28, Step_29, Step_30, Step_31, Step_32, Step_33, Step_34, Step_35, Step_36, Step_37, Step_38, Step_39, Step_40, Step_41, Step_42, Step_43, Step_44, Step_45, Step_46, Step_47, Step_48, Step_49, Step_50, Step_51 |
| Task_34 | 18 | Title, Intro, Step_0, Step_1, Step_2, Step_3, Step_4, Step_5, Step_6, Step_7, Step_8, Step_9, Step_10, Step_11, Step_12, Step_13, Step_14, Step_16 |

纯空白差异也保存在同一JSON的whitespace_preserved中。当前保留冲突不表示已逐一确认其业务正确；需要改动的实际规则在相应任务实施阶段结合有效Lua核对。

## 生效与回退

服务端任务表有首次加载缓存；后续发布时安排安全重启并校验。客户端有独立包加载顺序，须同步到其实际有效位置，本轮没有改客户端PAK。

若需回退，先确认当前文件仍是上述合并后哈希，核对备份哈希，再恢复该单个文件；如果此后已有新修改，应比较差异后只撤回本次追加，不能直接覆盖。
