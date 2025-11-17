# Offline Runtime Packages

本目录存放离线运行时安装包，供系统检测/自动修复在无网络环境下使用。

| 文件 | 来源 | SHA256 |
| --- | --- | --- |
| VC_redist.x86.exe | https://aka.ms/vs/17/release/vc_redist.x86.exe | 0c09f2611660441084ce0df425c51c11e147e6447963c3690f97e0b25c55ed64 |
| VC_redist.x64.exe | https://aka.ms/vs/17/release/vc_redist.x64.exe | cc0ff0eb1dc3f5188ae6300faef32bf5beeba4bdd6e8e445a9184072096b713b |

> 说明：若需要更新此目录，请运行`scripts/download-offline-deps.sh`重新拉取并同步`manifest.json`中的SHA256。
