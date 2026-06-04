# 部署运维手册

## 安装

### 使用 lip 安装（推荐）

```bash
lip install github.com/VincentZyuApps/levilamina-plugin-mclistener-ws-server
```

### 手动安装

1. 从 [GitHub Releases](https://github.com/VincentZyuApps/levilamina-plugin-mclistener-ws-server/releases) 下载最新版本
2. 解压后将 `mclistener-ws-server` 文件夹放入服务端 `plugins/` 目录
3. 启动服务端，插件会自动生成配置文件

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

## 配置文件

首次启动后，配置文件自动生成在 `plugins/mclistener-ws-server/config/config.json`。

### 完整配置示例

```json
{
    "version": 1,
    "logLevel": "info",
    "host": "0.0.0.0",
    "port": 60201,
    "enablePlayerJoinBroadcast": true,
    "enablePlayerLeaveBroadcast": true,
    "enablePlayerChatBroadcast": true,
    "enableReceiveGroupMessage": true,
    "chatCaptureMode": "event",
    "groupMessageFormat": "§6§l[{group_name}]§r §b({group_id})§r §a§o{nickname}§r§f: {message}"
}
```

### 配置项说明

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `version` | int | `1` | 配置文件版本，请勿修改 |
| `logLevel` | string | `"info"` | 日志级别，见下表 |
| `host` | string | `"0.0.0.0"` | WebSocket 服务器监听地址 |
| `port` | int | `60201` | WebSocket 服务器监听端口 |
| `enablePlayerJoinBroadcast` | bool | `true` | 是否广播玩家加入事件 |
| `enablePlayerLeaveBroadcast` | bool | `true` | 是否广播玩家离开事件 |
| `enablePlayerChatBroadcast` | bool | `true` | 是否广播玩家聊天事件 |
| `enableReceiveGroupMessage` | bool | `true` | 是否接收群消息并转发到游戏内 |
| `chatCaptureMode` | string | `"event"` | 聊天捕获方式，见下表 |
| `groupMessageFormat` | string | 见下文 | 群消息在游戏内的显示格式 |

---

### 日志级别 (logLevel)

| 值 | 说明 | 输出内容 |
|----|------|----------|
| `"silent"` | 静默模式 | 不输出任何日志 |
| `"fatal"` | 致命错误 | 仅输出导致程序崩溃的错误 |
| `"error"` | 错误 | 输出错误信息 |
| `"warn"` | 警告 | 输出警告和错误 |
| `"info"` | 信息（推荐） | 输出一般信息、警告和错误 |
| `"debug"` | 调试 | 输出详细调试信息（开发用） |
| `"trace"` | 追踪 | 输出所有信息（开发用） |

**建议**：正常使用设置为 `"info"`，排查问题时设置为 `"debug"`。

---

### 聊天捕获方式 (chatCaptureMode)

| 值 | 说明 | 适用场景 |
|----|------|----------|
| `"event"` | 使用 LeviLamina 的 PlayerChatEvent | 默认方式，兼容性最好 |
| `"hook_packet"` | 直接 Hook TextPacket 处理函数 | 当有插件（如 GwChat）拦截事件时使用 |
| `"both"` | 同时使用两种方式 | 调试用，可能导致重复消息 |

**选择建议**：
- 聊天消息能正常转发 → 默认 `"event"`
- 聊天消息无法转发（被 GwChat 等拦截） → 改用 `"hook_packet"`
- 不确定哪种有效 → 先用 `"both"` 测试，再选其中一种

**技术说明**：
- `event` 模式使用 LeviLamina 事件系统，若其他插件取消了 `PlayerChatEvent` 则无法捕获
- `hook_packet` 使用高优先级（High=100）Hook `ServerNetworkHandler::$handle`，在事件系统（Normal=200）之前执行，不受其他插件影响

---

### 消息格式 (groupMessageFormat)

群消息转发到游戏内时的显示格式，支持以下占位符：

| 占位符 | 说明 | 示例 |
|--------|------|------|
| `{group_name}` | 群名称 / 平台标识 | `onebot` |
| `{group_id}` | 群号 / 频道 ID | `1085190201` |
| `{nickname}` | 发送者昵称 | `Alice` |
| `{message}` | 消息内容 | `大家好` |

> 注意：`group_name` 字段填入的是平台标识（如 `onebot`、`discord`），**并非**真实群名/频道名，这是由 Koishi 客户端决定的。

#### Minecraft 颜色代码

| 代码 | 颜色 | 代码 | 颜色 |
|------|------|------|------|
| `§0` | 黑色 | `§8` | 深灰 |
| `§1` | 深蓝 | `§9` | 蓝色 |
| `§2` | 深绿 | `§a` | 绿色 |
| `§3` | 深青 | `§b` | 青色 |
| `§4` | 深红 | `§c` | 红色 |
| `§5` | 紫色 | `§d` | 粉色 |
| `§6` | 金色 | `§e` | 黄色 |
| `§7` | 灰色 | `§f` | 白色 |

| 代码 | 样式 |
|------|------|
| `§l` | **粗体** |
| `§o` | *斜体* |
| `§n` | <u>下划线</u> |
| `§m` | ~~删除线~~ |
| `§r` | 重置样式 |

#### 格式示例

默认格式：
```
§6§l[{group_name}]§r §b({group_id})§r §a§o{nickname}§r§f: {message}
```

显示效果：**[onebot]** (1085190201) *Alice*: 大家好

简洁格式：
```json
"groupMessageFormat": "§b[QQ]§r {nickname}: {message}"
```

显示效果：[QQ] Alice: 大家好

---

## WebSocket 消息格式速查

> 完整协议说明见 [主 README](../README.md#-websocket-协议)

### 服务端 → 客户端

```json
{"type": "player_join", "player_name": "Steve"}
{"type": "player_leave", "player_name": "Steve"}
{"type": "player_chat", "player_name": "Steve", "content": "Hello"}
```

### 客户端 → 服务端

```json
{
    "type": "chat_platform_to_server",
    "group_id": "1085190201",
    "group_name": "onebot",
    "nickname": "Alice",
    "message": "大家好"
}
```

---

## 常见问题

### Q: 修改配置后需要重启吗？
A: 是的，修改 `config.json` 后需要重启服务端才能生效。

### Q: WebSocket 连接不上？
A: 检查：
1. 服务器防火墙是否开放了对应端口（默认 **60201**）
2. 云服务器安全组是否放行该端口
3. 服务端是否正确加载了插件（查看服务端日志）
4. Koishi 客户端 `wsServerUrl` 是否与服务端地址端口一致

### Q: 如何测试 WebSocket 连接？
A: 使用 `wscat` 工具：
```bash
npm install -g wscat
wscat -c ws://你的服务器IP:60201
```

### Q: 群消息能转发到游戏，但游戏消息不能转发到群？
A: 检查：
1. 确认 `enablePlayerChatBroadcast` 为 `true`
2. 将 `logLevel` 设为 `"debug"` 查看日志
3. 如果使用 `chatCaptureMode: event`，确认没有其他插件拦截 `PlayerChatEvent`
4. 如果被其他插件拦截，切换为 `hook_packet` 模式

### Q: Koishi 客户端默认端口是 60601，但本插件默认是 60201？
A: 两个默认端口不同，需要在 Koishi 客户端配置中将 `wsServerUrl` 设置为 `ws://你的IP:60201` 以匹配本插件。或者修改本插件的 `port` 配置为其他端口。

---

## 参考链接

- [主 README](../README.md)
- [koishi-plugin-mclistener-ws-client](https://github.com/VincentZyuApps/koishi-plugin-mclistener-ws-client) — Koishi 客户端插件
- [LeviLamina 文档](https://lamina.levimc.org/)
