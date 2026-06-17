#pragma once

#include <string>

namespace mclistener_ws_server {

struct Config {
    int version = 1;
    
    // ── 📋 日志配置 ──────────────────────────────────────
    // 日志级别: "silent", "fatal", "error", "warn", "info", "debug", "trace"
    std::string logLevel = "info";
    
    // ── 🌐 WebSocket 服务器配置 ─────────────────────────
    // 监听地址（默认 0.0.0.0 即所有网卡）
    std::string host = "0.0.0.0";
    
    // 🔌 监听端口
    int port = 60605;
    
    // ── 🔑 WebSocket Token 认证 ─────────────────────────
    // 客户端连接时需要提供的 Token（空字符串表示不校验）
    std::string wsToken = "test12345";

    // ── 🔑 Token 校验模式 ───────────────────────────────
    // "any"      — URL ?token= 或 连接后 auth 消息均可（默认，向后兼容）
    // "param"    — 仅 URL ?token= 查询参数
    // "message"  — 仅连接后发 {"type":"auth","token":"..."} 消息（适合 js-relay）
    // "disabled" — 关闭 token 校验
    std::string wsTokenMode = "any";

    // post-connection message 鉴权超时（ms），超时未收到有效 auth 消息则断开
    int wsTokenAuthTimeoutMs = 5555;
    
    // ── 🎮 功能开关 ─────────────────────────────────────
    // ✅ 玩家加入服务器事件 广播通知
    bool enablePlayerJoinBroadcast = true;
    
    // ✅ 玩家离开服务器事件 广播通知
    bool enablePlayerLeaveBroadcast = true;
    
    // ✅ 玩家聊天时间 广播通知
    bool enablePlayerChatBroadcast = true;
    
    // ✅ 是否 接收聊天平台消息 并广播到游戏内
    bool enableReceiveGroupMessage = true;
    
    // ── ⚙️ 聊天捕获方式 ─────────────────────────────────
    // "event"       — 使用 LeviLamina 的 PlayerChatEvent（默认，兼容性最好）
    // "hook_packet" — 直接 hook TextPacket，不受其他插件拦截
    // "both"        — 同时使用两种方式（调试用，可能重复）
    std::string chatCaptureMode = "event";
    
    // ── ✏️ 消息格式 ─────────────────────────────────────
    // 群消息在游戏内的显示格式，支持占位符：
    //   {group_name} — 群名称 / 平台标识
    //   {group_id}   — 群号 / 频道 ID
    //   {nickname}   — 发送者昵称
    //   {message}    — 消息内容
    std::string groupMessageFormat = "§6§l[{group_name}]§r §b({group_id})§r §a§o{nickname}§r§f: {message}";

    // ── 🔧 远程指令执行 ─────────────────────────────────────
    // "disabled"   — 关闭远程指令执行
    // "js-relay"   — LSE JS 插件中继（配合 js/exec-relay.js）
    // "cpp-native" — C++ 直接调用 BDS API（TODO）
    std::string execCommandMode = "disabled";
};

} // namespace mclistener_ws_server
