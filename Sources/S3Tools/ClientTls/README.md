# 本地测试客户端 TLS 身份工具

本工具配合现有 Heaven 网络模块使用。新增身份文件及指定独立客户端的信任适配已由用户批准，方案记录在 `Test/Client20261030/待批准方案.md`。服务端逻辑仍在原 Heaven、Bishop 和 GameServer 中。

## 构建

在项目根目录运行：

```powershell
& .\Sources\S3Tools\ClientTls\Build.ps1
```

需要已有 Visual Studio 2022 C++ 工具和 `Test/Dependencies/OpenSSL` 的 Win32 `/MT` 静态依赖。编译产物仅写入 `Test/Client20261030/Tools`。

## 身份生成

```powershell
& .\Test\Client20261030\Tools\CreateLocalIdentity.exe `
    .\Resources\ServerConfig\ClientTls\server-cert.pem `
    .\Resources\ServerConfig\ClientTls\server-key.pem
```

目标目录须预先存在。只在初次建立已批准身份或明确批准轮换时运行；任一目标文件已存在就拒绝执行，不覆盖已有证书或私钥。若创建中断留下单个文件，先核实原因与配对状态，再处理，不得直接重复生成并覆盖。

证书有效期一年，使用随机 RSA-3072 密钥。私钥文件关闭 ACL 继承，仅允许文件所有者与 SYSTEM 访问。两个 PEM 内部含来源、用途、加载和轮换说明。不要把私钥复制到客户端、提交 Git 或写入日志。

Heaven 在启动时从 EXE 目录的 `../../Resources/ServerConfig/ClientTls` 读取这对文件；缺失、格式错误、过期或不匹配会导致 TLS 监听初始化失败。Bishop 与 GameServer 使用同一身份，重启后指纹保持一致。

## 独立客户端适配

```powershell
& .\Test\Client20261030\Tools\PatchClientTrust.exe `
    .\Test\Client20261030\BeforeDeployment\20261101-002601\Client\Game.unpacked.20261030.exe `
    .\Resources\ServerConfig\ClientTls\server-cert.pem `
    .\Test\Client20261030\Game.unpacked.20261030.local-pin-call.exe
```

补丁只接受后台 GUI IDA 核对过的原始 EXE：

- 大小：32,083,968 字节。
- SHA256：`2F795666BC02B4948B6B7416855272FF703C543CE404523AD44E4727912975BF`。
- x86，固定映像基址 `0x400000`，未启用 ASLR。

工具在新的 `.jxtls` 只读可执行节中加入初始化代码和本地公开 DER。原 `SSL_CTX_new_ex` 入口的完整 5 字节指令转到该代码；它先调用原初始化，再使用客户端自带的 `d2i_X509`、`X509_STORE_add_cert` 追加公开证书，释放临时 X509 引用。证书解析或追加失败会释放新 context 并返回 NULL，不跳过校验。原证书验证函数、原正式服证书及其加载代码保持原字节。工具不读取私钥。

该版本在 TLS 握手成功后还核对证书公钥的 SPKI SHA-256。工具从同一公开证书计算本地指纹，在登录流程 `0x679AF8` 的既有 CALL 处追加参数适配：通过客户端已有的 `getpeername` 导入核对实际远端地址，仅当地址恰为 IPv4 `127.0.0.1`、原校验名称为 `fs.seayoo.com`、原预期值为正式服已确认指纹时，改用本地公钥指纹，随后进入完整保留的原连接函数 `0x69E730` 和原比较代码。其他地址、名称、预期值或地址查询失败均保留原参数；错误公钥仍被原逻辑拒绝。不接受所有回环地址，不取消证书链、名称或公钥核对。早期修改受保护函数入口的候选未通过实测，已经撤回，不应部署。

地址、调用约定和 `SSL_CTX->cert_store` 偏移只适用于上述哈希。不得直接套用到其他版本；输入或输出不满足前置条件时工具拒绝操作，输出也不覆盖。

候选必须先在 Test 验证。替换 `Release/Client/Game.unpacked.20261030.exe` 前，关闭占用客户端并将原 EXE 备份到 Test。不要替换 `D:/Program Files/Kingsoft/FSOnline` 中的正式客户端。

## 验证与回退

`Test/Client20261030/VerifyTrustHook.cmd` 执行候选中的真实 x86 初始化代码，以隔离的 OpenSSL 调用夹具核对成功、原初始化失败、DER 失败、追加失败四种返回及引用释放行为。后台 GUI IDA 的候选代码反汇编记录在 `trust-candidate-asm.result.json`。

`VerifyPinHook.cmd` 执行候选中实际的 x86 参数适配代码，核对本地正确公钥、正式服原指纹、错误公钥、查询失败、其他名称、其他预期值、非本地地址、IPv6 及其他回环地址九种场景，并检查调用约定及寄存器保存。隔离夹具只把两处外部调用目标绑定到测试桩，不执行正式客户端的原连接代码；其逻辑分支、参数、寄存器保存和数据均取自候选。后台 GUI IDA 的最终候选反汇编记录在 `pin-call-candidate-asm2.result.json`。

`Verify-ClientPatch.py` 核对原文件变更仅位于预期 PE 头、初始化入口 5 字节、登录 CALL 5 字节和新增节，并从资源公开证书重新计算 SPKI 指纹核对候选数据；单独确认受保护函数入口字节没有变化。`Verify-Identity.py` 核对固定指纹、TLS 1.3、证书和主机名；同时检查无信任、错误信任、错误主机名及 TLS 1.2 被拒绝。`Verify-Transport.py` 和 `Verify-World.py` 现在均启用证书验证。

模拟测试不能替代真实客户端。须继续用指定 EXE 实测服务器选择、账号登录、已有角色进图、移动和驻留，并核对服务端 `tls_ready` 与业务请求日志。

2026-11-01 已完成该轮真实客户端测试，用户反馈暂未发现异常，登录和游戏监听均完成 TLS 1.3，角色列表、进图、移动和心跳有日志依据。实际记录见 `Test/Client20261030/actual-client-pin-verification.json`，当前已部署 SHA256 为 `1480DD54638B7B7DBAB9BC0AFB33F4A1B5DFA0FE759D16E03276163DC2010B6C`。本轮约 35 秒的通信记录不能代替长期稳定性或其他功能测试。

回退时先退出测试客户端并确认账号离线，再从 `Test/Client20261030/BeforeDeployment` 对应备份恢复客户端和 Heaven.dll，重新启动原服务。固定证书文件可以保留；旧程序不会读取它们。不要为回退修改 Windows 根证书库。
