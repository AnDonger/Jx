# 2026-10-29 serverlist.pak 重新打包

用户要求使用 `Resources` 重新生成 `Release/Client/Data/serverlist.pak`，原包已由用户移至 `Release/Client/Data/备份/serverlist.pak`。

已生成目标文件，大小 5,027,277 字节，SHA256：

`10ebfa1723593ccb3c53a768bbff4e010e60e1d1b2c14f184ed5f13e3773da3d`

原包实际包含 3,503 条配置、脚本和界面资源，不是仅含服务器列表的单文件包。因此保留原有全部条目及 ID，用 `Resources` 中对应原路径的文件核对并更新，避免移走原包后丢失其他有效资源。

- 1,793 条已还原原名的资源全部与 `Resources` 当前文件逐字节核对。
- 实际变更仅有 `\settings\serverlist.ini`，ID 为 `0x769629AC`，原 4,200 字节，现 4,178 字节。
- 尚未恢复原名的 1,710 条保留原包数据；不猜测名称或补造内容。
- 未变化条目的原始压缩块保持一致。
- 没有改变用户修改后的服务器列表内容、编码、区名、地址和区 ID。
- 没有修改 `package.ini`，也没有修改用户原包备份。

使用资源管理后端现有 `app/pak/codec.py` 的无损重写功能：保留原始块，追加变化内容和新索引。变化的 INI 使用原生引擎 `XPackFile.cpp` 支持的 TYPE_NONE 存储。原包 CRC 字段为 0，保持不变。离线脚本及构建、校验产物位于 `Test/RepackServerList`；没有新增服务进程或业务配置。

已重新读取生成包的全部 3,503 条，验证 Resources 来源内容一致、保留条目内容一致、未变更压缩块一致，且原包备份 SHA256 未变。部署后的文件与生成文件 SHA256 一致。

根据客户端 `package.ini`，`Update.pak`、`blaze.pak` 位于 `serverlist.pak` 之前；已检查这两个实际包的索引，均不存在 `\settings\serverlist.ini` 对应 ID，因此服务器列表不会被它们覆盖。

完整报告：`Test/RepackServerList/20261029-003252/report.json`。

本次完成包格式、全部条目内容及加载优先级校验，尚未启动客户端实测服务器选择界面。应完全退出并重新启动 `Release/Client` 中的客户端后观察。
