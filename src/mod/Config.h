#pragma once

#include <string>

namespace mclistener_ws_server {

struct Config {
    int version = 1;

    std::string _comment_logLevel = "📋 日志等级：silent | fatal | error | warn | info | debug | trace";
    std::string logLevel = "info";

    std::string _comment_host = "🌐 WebSocket 服务器监听地址，0.0.0.0 表示监听所有网卡";
    std::string host = "0.0.0.0";

    std::string _comment_port = "🔌 WebSocket 服务器监听端口";
    int port = 60605;

    std::string _comment_wsToken = "🔑 客户端连接时需要提供的 Token，空字符串表示不校验";
    std::string wsToken = "test12345";

    std::string _comment_wsTokenMode = "🔐 Token 校验模式：any(URL或消息均可) | param(仅URL参数) | message(仅auth消息) | disabled(关闭校验)";
    std::string wsTokenMode = "any";

    std::string _comment_wsTokenAuthTimeoutMs = "⏱️ post-connection auth 消息鉴权超时（毫秒），超时未收到有效 auth 消息则断开";
    int wsTokenAuthTimeoutMs = 5555;

    std::string _comment_enablePlayerJoinBroadcast = "🚪 玩家加入服务器时是否广播通知到聊天平台";
    bool enablePlayerJoinBroadcast = true;

    std::string _comment_enablePlayerLeaveBroadcast = "🚶 玩家离开服务器时是否广播通知到聊天平台";
    bool enablePlayerLeaveBroadcast = true;

    std::string _comment_enablePlayerChatBroadcast = "💬 玩家在游戏内聊天时是否广播到聊天平台";
    bool enablePlayerChatBroadcast = true;

    std::string _comment_enableReceiveGroupMessage = "📥 是否接收聊天平台消息并转发到游戏内";
    bool enableReceiveGroupMessage = true;

    std::string _comment_chatCaptureMode = "⚙️ 聊天捕获方式：event(事件系统) | hook_packet(数据包钩子) | both(两者同时)";
    std::string chatCaptureMode = "event";

    std::string _comment_groupMessageFormat = "✏️ 群消息在游戏内的显示格式，占位符：{group_name} {group_id} {nickname} {message}";
    std::string groupMessageFormat = "§6§l[{group_name}]§r §b({group_id})§r §a§o{nickname}§r§f: {message}";

    std::string _comment_execCommandMode = "🔧 远程指令执行模式：disabled(关闭) | js-relay(JS插件中继) | cpp-native(C++直接执行)";
    std::string execCommandMode = "disabled";
};

} // namespace mclistener_ws_server
