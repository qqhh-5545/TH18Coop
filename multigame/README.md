# Touhou Co-op 1.0.5

当前双游戏启动器及辉针城原生模块源码。虹龙洞原生模块沿用官方 1.0.4，来源与固定哈希见 docs/TH18_V104_IMPORT.json。仓库根目录保留该版虹龙洞原生源码。

Windows 上使用 Zig 0.13.0、.NET Framework C# 编译器。在本目录执行构建，RuntimeDirectory 指向已完整解压的官方 1.0.4 便携包。

```powershell
./build.ps1 -ZigPath C:/Tools/zig/zig.exe -RuntimeDirectory C:/TH18Coop-1.0.4-portable
./test-dev.ps1 -SkipShared -ZigPath C:/Tools/zig/zig.exe
./test-th14-hud.ps1 -ZigPath C:/Tools/zig/zig.exe
./test-th14-progress.ps1 -ZigPath C:/Tools/zig/zig.exe
```

运行文件输出到 release。构建可选传入 -IconDirectory，读取本机自行提取的 th14.ico、th18.ico 并嵌入游戏选择器。游戏图标不进入源码仓库。测试会生成 tests/bin，使用合成输入和模拟引擎，实际游戏需用户已有的受支持安装。

使用说明见仓库 README 和 docs/TH14.md。代码按仓库 MIT 许可及保留的第三方许可分发。
