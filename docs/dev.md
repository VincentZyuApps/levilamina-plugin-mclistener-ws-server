# 开发编译指南

## 关于 LeviLamina 插件开发

### 为什么必须用 C++？

LeviLamina 是一个 **native mod loader**，通过 DLL 注入的方式加载插件。插件必须编译成 **Windows DLL (.dll)** 格式：

1. Minecraft 基岩版专用服务器 (BDS) **只有 Windows 版本**
2. LeviLamina 使用 C++ 直接 Hook 游戏函数
3. 需要调用 BDS 的内部 API，这些 API 没有其他语言的绑定

### 编译产物

```
bin/mclistener-ws-server/
├── mclistener-ws-server.dll    # 插件主体
└── manifest.json               # 插件清单
```

### 部署到服务端

将整个 `mclistener-ws-server` 文件夹复制到服务端 `plugins/` 目录：

```
BDS服务端/
├── bedrock_server_mod.exe
├── plugins/
│   └── mclistener-ws-server/
│       ├── mclistener-ws-server.dll
│       └── manifest.json
└── ...
```

---

## ⚠️ 编译环境要求

### ❌ WSL 不能用于编译

WSL **不能直接编译** LeviLamina 插件：

1. LeviLamina 依赖 **MSVC (Microsoft Visual C++)** 编译器
2. 需要 Windows SDK 头文件和库
3. xmake 的 LeviLamina 包只支持 Windows 平台
4. MinGW 交叉编译无法正确链接 LeviLamina 库

### ✅ 必须在 Windows 上编译

#### 方法一：Visual Studio 2022（推荐）

1. 安装 [Visual Studio 2022 Community](https://visualstudio.microsoft.com/)
   - 勾选 "C++ 桌面开发" 工作负载
   - 确保安装最新的 MSVC 和 Windows SDK

2. 安装 [xmake](https://xmake.io/#/guide/installation)
   ```powershell
   winget install xmake
   # 或
   scoop install xmake
   ```

3. 编译
   ```powershell
   cd 项目目录
   xmake repo -u
   xmake f -y -p windows -a x64 -m release
   xmake
   ```

#### 方法二：Visual Studio Build Tools（轻量）

1. 下载 [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
2. 安装时选择 "C++ 生成工具"
3. 安装 xmake（同上）

#### 方法三：GitHub Actions 云编译（推荐懒人）

> 详细流程见 [`.github/workflows/build.md`](../.github/workflows/build.md)

本项目配置了自动化 CI/CD，**不需要本地环境**即可编译：

```bash
# 仅编译（不上传 Release）
git commit -m "build action. feat: xxx"

# 编译 + 创建 GitHub Release
git commit -m "build release. feat: xxx"
```

版本号从 `tooth.json` 的 `version` 字段读取。

---

## 快速开始

假设已配置好编译环境：

```powershell
# 1. 进入项目目录
cd 项目目录

# 2. 首次配置（下载 LeviLamina SDK，需联网）
xmake repo -u
xmake f -y -p windows -a x64 -m release

# 3. 编译
xmake

# 4. 查看产物
dir bin\mclistener-ws-server\
```

### 代理配置

如果下载依赖失败，可能需要配置代理：

```powershell
# 使用 GitHub 镜像
xmake g --proxy_pac=github_mirror.lua

# 或使用 HTTP 代理
xmake g --proxy="http://127.0.0.1:7890"
```

---

## 项目结构

```
.
├── .github/
│   └── workflows/
│       ├── build.yml       # CI/CD 流水线
│       └── build.md        # CI/CD 使用说明
├── src/
│   └── mod/
│       ├── MclistenerWsServerMod.h   # 主模组类
│       ├── MclistenerWsServerMod.cpp # 主模组实现
│       ├── WebSocketServer.h         # WebSocket 服务器
│       ├── WebSocketServer.cpp
│       ├── Config.h                  # 配置结构体
│       └── MemoryOperators.cpp       # 内存操作符
├── xmake.lua              # xmake 构建配置
├── tooth.json             # lip 包配置（版本号）
└── manifest.json          # 插件清单模板
```

---

## 常见问题

### Q: 编译报错 "xxx is not a member of std"
A: MSVC 或 Windows SDK 版本太旧，更新到最新。

### Q: xmake 下载依赖失败
A: 配置代理或使用镜像，参考上面的代理配置命令。

### Q: 插件加载失败
A: 检查：
1. LeviLamina 版本是否匹配（本插件需要 **1.8.x**）
2. DLL 和 `manifest.json` 是否都在 `plugins/mclistener-ws-server/` 目录下
3. 查看服务端日志的错误信息

### Q: 如何更新版本号？
A: 修改 `tooth.json` 的 `version` 字段即可。CI 会自动读取此版本号用于构建和 Release。

---

## 参考链接

- [LeviLamina 文档](https://lamina.levimc.org/)
- [创建你的第一个模组](https://lamina.levimc.org/zh/developer_guides/tutorials/create_your_first_mod/)
- [xmake 官网](https://xmake.io/)
- [LeviLamina 模板仓库](https://github.com/LiteLDev/levilamina-mod-template)
- [CI/CD 工作流说明](../.github/workflows/build.md)
