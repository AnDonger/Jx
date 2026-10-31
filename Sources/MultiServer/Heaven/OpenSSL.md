# 新客户端 TLS 构建说明

`ClientTls.cpp` 是现有 Heaven 网络模块的一部分。Bishop 的客户端监听和 GameServer 的客户端监听通过 `IClientTls` 显式启用 TLS 1.3；内部服务仍使用原来的连接方式。

使用 OpenSSL 3.5.9 的原生 Win32 静态库，运行库为 `/MT`，无需部署 OpenSSL DLL。源码下载和编译工作目录均位于项目 `Test/Dependencies`，不将第三方编译产物提交到 Git。

首次构建前，使用本机已有的原生 Windows Perl 和 Visual Studio C++ 工具执行：

```powershell
& .\Sources\MultiServer\Heaven\BuildOpenSsl.ps1 -PerlPath '已安装的原生Windows Perl完整路径'
```

脚本固定下载官方 OpenSSL 仓库的 `openssl-3.5.9` 标签归档并核对 SHA256。Git 自带的 MSYS Perl 不适合该构建目标。脚本不会安装 Perl 或修改系统设置。

Heaven 工程默认从 `Test/Dependencies/OpenSSL` 查找头文件和静态库；也可以通过 MSBuild 的 `OpenSslRoot` 属性指定同版本、同架构的依赖目录。完成后正常编译 Heaven、Bishop、GameServer。

本地监听读取用户批准的固定身份：`Resources/ServerConfig/ClientTls/server-cert.pem` 和 `server-key.pem`。路径从服务端 EXE 所在目录计算，因此改变启动工作目录不会更换证书。Bishop 与 GameServer 共用同一对文件；缺失、私钥不匹配或证书不在有效期内时，监听初始化明确失败，不会生成新身份覆盖已有文件。

这对身份由 `Sources/S3Tools/ClientTls/CreateLocalIdentity.cpp` 生成，使用随机 RSA-3072 密钥。SAN 包含 localhost、127.0.0.1 和客户端固定校验名称 fs.seayoo.com；后者仅供独立本地测试客户端兼容，不代表正式服身份。客户端补丁只追加本地公开证书，保留原正式服信任及证书校验，不安装 Windows 系统根证书。生成、构建、客户端适配和回退方式见 `Sources/S3Tools/ClientTls/README.md`。

私钥仅保存在服务端资源目录，权限限制为文件所有者与 SYSTEM。不得复制到客户端或日志，也不得提交 Git。身份轮换需要重新部署对应客户端的公开证书并重启服务，不能只替换其中一个 PEM。

网络日志区分原始 TLS 接收片段、TLS 解密后的应用片段、重组后的游戏帧以及业务解码结果。TLS 主密钥及流量密钥不写入日志。

官方源码、许可和 API 参考：

- https://github.com/openssl/openssl/tree/openssl-3.5.9
- https://docs.openssl.org/3.5/man3/SSL_set_bio/
- https://docs.openssl.org/3.5/man3/SSL_get_error/

OpenSSL 使用 Apache License 2.0；构建脚本将其 `LICENSE.txt` 一并保留在依赖目录。
