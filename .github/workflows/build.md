# GitHub Actions 工作流指南

本仓库使用 GitHub Actions 进行持续集成和发布。当代码推送到 `main`/`master` 分支时，工作流会根据 commit 信息中的关键词自动执行。

[![xmake](https://img.shields.io/badge/xmake-v2.9.7-0094D9?style=for-the-badge&logo=data:image/svg%2bxml;base64,...)](https://xmake.io)
[![C++20](https://img.shields.io/badge/C++-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![MSVC](https://img.shields.io/badge/MSVC-2022-0078D4?style=for-the-badge&logo=visualstudio&logoColor=white)](...)
[![GitHub CI](https://img.shields.io/github/actions/workflow/status/VincentZyuApps/levilamina-plugin-mclistener-ws-server/build.yml?style=for-the-badge&logo=githubactions&logoColor=white)](...)

## 📋 概述

CI/CD 流水线由 **commit 信息中的关键词** 驱动。推送到主分支时，只需在 commit message 中包含对应关键词，GitHub Actions 会自动完成后续工作。

## 🔑 关键词

| Commit 信息中的关键词 | 构建 | GitHub Release |
|----------------------|:---:|:---:|
| `build action` | ✅ | ❌ |
| `build release` | ✅ | ✅ |

### 使用示例

```bash
# 仅构建，不发版（用于测试）
git commit -m "build action. fix: correct typo in config"

# 构建 + 发版
git commit -m "build release. feat: add new WebSocket event"
```

## 📦 流水线阶段

```
check ──→ build ──→ release
  │         │         │
  │         │         └─ 下载构建产物
  │         │            删除旧的 release/tag（幂等）
  │         │            生成 Release Notes
  │         │            创建 GitHub Release
  │         │
  │         └─ 编译 (xmake)
  │            上传构建产物 (artifact)
  │
  └─→ 读取 tooth.json 的版本号
      解析 commit 关键词
```

```mermaid
flowchart TB
    subgraph check["check"]
        C1[读取 tooth.json 版本号]
        C2[解析 commit 关键词]
    end

    subgraph build["build"]
        B1[编译 (xmake)]
        B2[上传 artifact]
    end

    subgraph release["release"]
        R1[下载 artifact]
        R2[准备 zip 包]
        R3[删除旧 release/tag]
        R4[生成 Release Notes]
        R5[创建 GitHub Release]
    end

    C1 --> C2
    C2 -->|build action / build release| B1
    C2 -->|build release only| R3
    B1 --> B2
    B2 --> R1
    R1 --> R2
    R3 --> R4 --> R5
```

## 🏷️ 版本号管理

版本号的**唯一数据源**是 `tooth.json`：

```json
{
    "version": "0.6.3-alpha.12"
}
```

**发版流程：**

1. 修改 `tooth.json` 中的版本号
2. 提交并 push：
   ```bash
   git add tooth.json
   git commit -m "build release. feat: ..."
   git push
   ```

Release 标签格式为 `v{version}`（如 `vx.y.z-alpha.w+yyyymmdd`），与 `tooth.json` 的下载 URL 模板一致。

## 📁 Release 产物结构

发布的 zip 包结构：

```
mclistener-ws-server/
├── mclistener-ws-server.dll
├── manifest.json
└── ...
```

解压到服务端 `plugins/` 目录即可。

## 🖱️ 手动触发

可以从 GitHub 仓库的 Actions 页面手动触发工作流：

1. 进入 **Actions → Build & Release → Run workflow**
2. 可选填写：
   - **Version**：覆盖 `tooth.json` 的版本号
   - **Create GitHub Release**：勾选以创建 Release

```
           ┌─ 有输入版本 ──→ 使用输入的版本
workflow ──┤
dispatch   └─ 无输入版本 ──→ 使用 tooth.json 的版本

```

## 🏗️ 构建环境
| 包 | 版本 | 说明 |
|:---|:---|:---|
| [![xmake](https://img.shields.io/badge/xmake-2.9.x-0094D9?style=flat-square&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZD0iTTE4LjkgNS4xTDEyIDEybDYuOSA2LjktMS40IDEuNEwxMiAxMy40bC02LjkgNi45LTEuNC0xLjRMMTAuNiAxMiAzLjcgNS4xbDEuNC0xLjRMMTIgMTAuNmw2LjktNi45IDEuNCAxLjR6IiBmaWxsPSIjZmZmIi8+PC9zdmc+)](https://xmake.io) | 2.9.x | 构建系统 |
| [![MSVC](https://img.shields.io/badge/MSVC-2022-0078D4?style=flat-square&logo=cplusplus&logoColor=white)](https://learn.microsoft.com/en-us/cpp/) | 2022 | C++ 编译器 |
| [![C++20](https://img.shields.io/badge/C++20-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20) | C++20 | 语言标准 |
| [![LeviLamina](https://img.shields.io/badge/LeviLamina-26.10.x-7FA973?style=flat-square&logo=cplusplus&logoColor=white&labelColor=2C5E3B)](https://github.com/LiteLDev/LeviLamina) | 26.10.x | 模组加载器 SDK |
| [![LeviBuildScript](https://img.shields.io/badge/LeviBuildScript-v0.x-585858?style=flat-square)](https://github.com/LiteLDev/LeviBuildScript) | — | 构建脚本插件 |
| [![nlohmann/json](https://img.shields.io/badge/nlohmann%2Fjson-v3.x-585858?style=flat-square&logo=json&logoColor=white)](https://github.com/nlohmann/json) | — | JSON 序列化 |
| [![WinSock2](https://img.shields.io/badge/WinSock2-Ws2_32-585858?style=flat-square)](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/) | — | 网络 API (Ws2_32) |
