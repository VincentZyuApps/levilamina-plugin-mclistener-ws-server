#include "mod/MclistenerWsServerMod.h"
#include "mod/WebSocketServer.h"

#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/Config.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerChatEvent.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/io/LogLevel.h"
#include "ll/api/memory/Hook.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/network/ServerNetworkHandler.h"
#include "mc/network/NetworkIdentifier.h"
#include "mc/network/packet/TextPacket.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>

namespace mclistener_ws_server {

// 全局变量用于 hook 回调
static MclistenerWsServerMod* g_modInstance = nullptr;
static bool hookEnabled = false;

// Hook TextPacket 处理函数
// 使用 High 优先级，在 LeviLamina 的 PlayerChatEvent hook (Normal=200) 之前执行
// 这样即使 GwChat 取消了 PlayerChatEvent，我们也能捕获到消息
LL_TYPE_INSTANCE_HOOK(
    TextPacketHook,
    ll::memory::HookPriority::High, // High=100, 在 Normal=200 之前执行
    ServerNetworkHandler,
    &ServerNetworkHandler::$handle,
    void,
    NetworkIdentifier const& identifier,
    TextPacket const&        packet
) {
    // 在调用 origin 之前先捕获消息（在其他插件处理之前）
    if (hookEnabled && g_modInstance && g_modInstance->getConfig().enablePlayerChatBroadcast) {
        try {
            // 获取玩家对象
            auto player = thisFor<NetEventCallback>()->_getServerPlayer(identifier, packet.mSenderSubId);
            if (player) {
                // 使用 TextPacketPayload 提供的安全 API 获取消息内容
                std::string msg = packet.getMessage();
                std::string playerName = player->getRealName();
                
                // 跳过空消息
                if (msg.empty()) {
                    origin(identifier, packet);
                    return;
                }
                
                g_modInstance->getSelf().getLogger().trace("【-- Hook --】 TextPacketHook triggered (High priority, before event system)");
                g_modInstance->getSelf().getLogger().debug("【-- Hook --】 {} said: {}", playerName, msg);
                
                nlohmann::json jsonMsg;
                jsonMsg["type"] = "player_chat";
                jsonMsg["player_name"] = playerName;
                jsonMsg["content"] = msg;
                
                std::string jsonStr = jsonMsg.dump();
                g_modInstance->getSelf().getLogger().trace("【-- Hook --】 Broadcasting JSON via hook: {}", jsonStr);
                
                if (auto* ws = g_modInstance->getWebSocketServer()) {
                    ws->broadcast(jsonStr);
                    g_modInstance->getSelf().getLogger().info("【-- Chat --】 {}: {}", playerName, msg);
                }
            }
        } catch (const std::exception& e) {
            g_modInstance->getSelf().getLogger().error("【-- Hook --】 TextPacketHook error: {}", e.what());
        } catch (...) {
            g_modInstance->getSelf().getLogger().error("【-- Hook --】 TextPacketHook unknown error");
        }
    }
    
    // 调用原始函数链（让 LeviLamina 和其他插件继续处理）
    origin(identifier, packet);
}

// Hook 注册器
static ll::memory::HookRegistrar<TextPacketHook> textPacketHookRegistrar;

// 将字符串转换为日志级别
static ll::io::LogLevel parseLogLevel(const std::string& levelStr) {
    std::string lower = levelStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), 
                   [](unsigned char c){ return std::tolower(c); });
    
    if (lower == "silent" || lower == "off") return ll::io::LogLevel::Off;
    if (lower == "fatal") return ll::io::LogLevel::Fatal;
    if (lower == "error") return ll::io::LogLevel::Error;
    if (lower == "warn" || lower == "warning") return ll::io::LogLevel::Warn;
    if (lower == "info") return ll::io::LogLevel::Info;
    if (lower == "debug") return ll::io::LogLevel::Debug;
    if (lower == "trace") return ll::io::LogLevel::Trace;
    
    return ll::io::LogLevel::Info; // 默认 info
}

MclistenerWsServerMod& MclistenerWsServerMod::getInstance() {
    static MclistenerWsServerMod instance;
    return instance;
}

bool MclistenerWsServerMod::load() {
    auto& logger = getSelf().getLogger();
    
    // ASCII Art Banner
    logger.info("");
    logger.info(R"(                         ___      __                        )");
    logger.info(R"(   ____ ___  _____      / (_)____/ /____  ____  ___  _____   )");
    logger.info(R"(  / __ `__ \/ ___/_____/ / / ___/ __/ _ \/ __ \/ _ \/ ___/   )");
    logger.info(R"( / / / / / / /__/_____/ / (__  ) /_/  __/ / / /  __/ /       )");
    logger.info(R"(/_/ /_/ /_/\___/     /_/_/____/\__/\___/_/ /_/\___/_/        )");
    logger.info(R"(                                                             )");
    logger.info(R"( _      _______      ________  ______   _____  _____         )");
    logger.info(R"(| | /| / / ___/_____/ ___/ _ \/ ___/ | / / _ \/ ___/         )");
    logger.info(R"(| |/ |/ (__  )_____(__  )  __/ /   | |/ /  __/ /             )");
    logger.info(R"(|__/|__/____/     /____/\___/_/    |___/\___/_/              )");
    logger.info("");
    logger.info("  Author: VincentZyu");
    logger.info("  GitHub: https://github.com/VincentZyu233");
    logger.info("");

    // 读取配置文件
    const auto& configFilePath = getSelf().getConfigDir() / "config.json";
    if (!ll::config::loadConfig(mConfig, configFilePath)) {
        logger.warn("【-- Config --】 Cannot load configurations from {}", configFilePath.string());
        logger.info("【-- Config --】 Saving default configurations...");
        if (!ll::config::saveConfig(mConfig, configFilePath)) {
            logger.error("【-- Config --】 Failed to save default configurations!");
        }
    }

    // 设置日志级别
    ll::io::LogLevel logLevel = parseLogLevel(mConfig.logLevel);
    logger.setLevel(logLevel);
    logger.info("【-- Config --】 Log level set to: {}", std::string(mConfig.logLevel));

    // 输出配置信息 (debug 级别)
    logger.debug("【-- Config --】 Configuration loaded:");
    logger.debug("【-- Config --】   - host: {}", std::string(mConfig.host));
    logger.debug("【-- Config --】   - port: {}", mConfig.port);
    logger.debug("【-- Config --】   - enablePlayerJoinBroadcast: {}", mConfig.enablePlayerJoinBroadcast);
    logger.debug("【-- Config --】   - enablePlayerLeaveBroadcast: {}", mConfig.enablePlayerLeaveBroadcast);
    logger.debug("【-- Config --】   - enablePlayerChatBroadcast: {}", mConfig.enablePlayerChatBroadcast);
    logger.debug("【-- Config --】   - enableReceiveGroupMessage: {}", mConfig.enableReceiveGroupMessage);
    logger.debug("【-- Config --】   - chatCaptureMode: {}", std::string(mConfig.chatCaptureMode));
    logger.debug("【-- Config --】   - wsToken: {}", mConfig.wsToken.empty() ? "(disabled)" : "*** (enabled)");

    logger.info("【-- Plugin --】 mclistener-ws-server loaded successfully!");
    return true;
}

bool MclistenerWsServerMod::enable() {
    getSelf().getLogger().info("【-- Plugin --】 Enabling mclistener-ws-server...");
    getSelf().getLogger().debug("【-- WS --】 Creating WebSocket server instance...");

    // 创建并启动 WebSocket 服务器
    mWsServer = std::make_unique<WebSocketServer>(mConfig.host, mConfig.port, this);
    
    getSelf().getLogger().debug("【-- WS --】 Starting WebSocket server on {}:{}...", std::string(mConfig.host), mConfig.port);
    if (!mWsServer->start()) {
        getSelf().getLogger().error("【-- WS --】 Failed to start WebSocket server!");
        getSelf().getLogger().fatal("【-- Plugin --】 Plugin cannot function without WebSocket server!");
        return false;
    }

    // 设置消息回调 - 处理从聊天平台来的消息
    if (mConfig.enableReceiveGroupMessage) {
        getSelf().getLogger().debug("【-- Callback --】 Setting up message callback for group messages...");
        mWsServer->setMessageCallback([this](const std::string& message) {
            getSelf().getLogger().trace("【-- WS --】 Raw message received: {}", message);
            try {
                auto json = nlohmann::json::parse(message);
                std::string type = json.value("type", "");
                getSelf().getLogger().debug("【-- WS --】 Parsed message type: {}", type);
                
                if (type == "chat_platform_to_server") {
                    std::string groupId = json.value("group_id", "");
                    std::string groupName = json.value("group_name", "");
                    std::string nickname = json.value("nickname", "未知用户");
                    std::string content = json.value("message", "");

                    getSelf().getLogger().debug("【-- Group -> Server --】 Group message details - group: {} ({}), user: {}", 
                                                 groupName, groupId, nickname);

                    // 使用配置的消息格式
                    std::string formattedMsg = mConfig.groupMessageFormat;
                    
                    // 替换占位符
                    size_t pos;
                    while ((pos = formattedMsg.find("{group_id}")) != std::string::npos) {
                        formattedMsg.replace(pos, 10, groupId);
                    }
                    while ((pos = formattedMsg.find("{group_name}")) != std::string::npos) {
                        formattedMsg.replace(pos, 12, groupName);
                    }
                    while ((pos = formattedMsg.find("{nickname}")) != std::string::npos) {
                        formattedMsg.replace(pos, 10, nickname);
                    }
                    while ((pos = formattedMsg.find("{message}")) != std::string::npos) {
                        formattedMsg.replace(pos, 9, content);
                    }

                    getSelf().getLogger().trace("【-- Group -> Server --】 Formatted message: {}", formattedMsg);

                    // 在游戏中广播消息
                    auto level = ll::service::getLevel();
                    if (level) {
                        int playerCount = 0;
                        level->forEachPlayer([&formattedMsg, &playerCount](Player& player) -> bool {
                            player.sendMessage(formattedMsg);
                            playerCount++;
                            return true; // 继续遍历
                        });
                        getSelf().getLogger().debug("【-- Group -> Server --】 Broadcasted to {} players in-game", playerCount);
                    } else {
                        getSelf().getLogger().warn("【-- Group -> Server --】 Level not available, cannot broadcast message");
                    }

                    getSelf().getLogger().info("【-- Group -> Server --】 [{}] {}: {}", groupName, nickname, content);
                } else {
                    getSelf().getLogger().debug("【-- WS --】 Ignoring message with type: {}", type);
                }
            } catch (const nlohmann::json::parse_error& e) {
                getSelf().getLogger().error("【-- WS --】 JSON parse error: {}", e.what());
                getSelf().getLogger().debug("【-- WS --】 Invalid JSON: {}", message);
            } catch (const std::exception& e) {
                getSelf().getLogger().error("【-- WS --】 Failed to process message: {}", e.what());
            }
        });
        getSelf().getLogger().debug("【-- Callback --】 Message callback registered successfully");
    } else {
        getSelf().getLogger().debug("【-- Config --】 Group message receiving is disabled in config");
    }

    auto& eventBus = ll::event::EventBus::getInstance();
    getSelf().getLogger().debug("【-- Event --】 Registering event listeners...");

    // 订阅玩家加入事件
    if (mConfig.enablePlayerJoinBroadcast) {
        bool hasJoinEvent = eventBus.hasEvent(ll::event::getEventId<ll::event::PlayerJoinEvent>);
        getSelf().getLogger().info("【-- Event --】 PlayerJoinEvent registered in EventBus: {}", hasJoinEvent ? "YES" : "NO");
        
        mPlayerJoinListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
            [this](ll::event::PlayerJoinEvent& event) {
                getSelf().getLogger().trace("【-- Event --】 PlayerJoinEvent triggered");
                auto& player = event.self();
                std::string playerName = player.getRealName();

                nlohmann::json msg;
                msg["type"] = "player_join";
                msg["player_name"] = playerName;

                std::string jsonStr = msg.dump();
                getSelf().getLogger().trace("【-- Player --】 Broadcasting JSON: {}", jsonStr);
                mWsServer->broadcast(jsonStr);
                getSelf().getLogger().info("【-- Player --】 Player {} joined", playerName);
            }
        );
        
        if (mPlayerJoinListener) {
            getSelf().getLogger().info("【-- Event --】 PlayerJoinEvent listener registered successfully");
        } else {
            getSelf().getLogger().error("【-- Event --】 Failed to register PlayerJoinEvent listener!");
        }
    } else {
        getSelf().getLogger().debug("【-- Config --】 Player join broadcast is disabled in config");
    }

    // 订阅玩家离开事件
    if (mConfig.enablePlayerLeaveBroadcast) {
        bool hasLeaveEvent = eventBus.hasEvent(ll::event::getEventId<ll::event::PlayerDisconnectEvent>);
        getSelf().getLogger().info("【-- Event --】 PlayerDisconnectEvent registered in EventBus: {}", hasLeaveEvent ? "YES" : "NO");
        
        mPlayerLeaveListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>(
            [this](ll::event::PlayerDisconnectEvent& event) {
                getSelf().getLogger().trace("【-- Event --】 PlayerDisconnectEvent triggered");
                auto& player = event.self();
                std::string playerName = player.getRealName();

                nlohmann::json msg;
                msg["type"] = "player_leave";
                msg["player_name"] = playerName;

                std::string jsonStr = msg.dump();
                getSelf().getLogger().trace("【-- Player --】 Broadcasting JSON: {}", jsonStr);
                mWsServer->broadcast(jsonStr);
                getSelf().getLogger().info("【-- Player --】 Player {} left", playerName);
            }
        );
        
        if (mPlayerLeaveListener) {
            getSelf().getLogger().info("【-- Event --】 PlayerDisconnectEvent listener registered successfully");
        } else {
            getSelf().getLogger().error("【-- Event --】 Failed to register PlayerDisconnectEvent listener!");
        }
    } else {
        getSelf().getLogger().debug("【-- Config --】 Player leave broadcast is disabled in config");
    }

    // 订阅玩家聊天事件
    if (mConfig.enablePlayerChatBroadcast) {
        // 设置全局实例指针供 hook 使用
        g_modInstance = this;
        
        std::string mode = mConfig.chatCaptureMode;
        std::transform(mode.begin(), mode.end(), mode.begin(), 
                       [](unsigned char c){ return std::tolower(c); });
        
        getSelf().getLogger().info("【-- Config --】 Chat capture mode: {}", std::string(mConfig.chatCaptureMode));
        
        // 使用 event 方式
        if (mode == "event" || mode == "both") {
            bool hasEvent = eventBus.hasEvent(ll::event::getEventId<ll::event::PlayerChatEvent>);
            getSelf().getLogger().info("【-- Event --】 PlayerChatEvent registered in EventBus: {}", hasEvent ? "YES" : "NO");
            
            // 使用高优先级(High=100)注册监听器，确保在 LSE 插件(Normal=200)之前执行
            // 这样即使 GwChat 等插件取消事件，我们也能捕获到消息
            mPlayerChatListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
                [this](ll::event::PlayerChatEvent& event) {
                    getSelf().getLogger().trace("【-- Event --】 PlayerChatEvent triggered (High priority)");
                    auto& player = event.self();
                    std::string playerName = player.getRealName();
                    std::string message = event.message();

                    getSelf().getLogger().debug("【-- Chat --】 {} said: {}", playerName, message);

                    nlohmann::json msg;
                    msg["type"] = "player_chat";
                    msg["player_name"] = playerName;
                    msg["content"] = message;

                    std::string jsonStr = msg.dump();
                    getSelf().getLogger().trace("【-- Chat --】 Broadcasting JSON: {}", jsonStr);
                    mWsServer->broadcast(jsonStr);
                    getSelf().getLogger().info("【-- Chat --】 [Event] {}: {}", playerName, message);
                },
                ll::event::EventPriority::High  // 优先级: High(100) < Normal(200)，先执行
            );
            
            if (mPlayerChatListener) {
                getSelf().getLogger().info("【-- Event --】 PlayerChatEvent listener registered with HIGH priority (ID: {})", 
                                            mPlayerChatListener->getId());
            } else {
                getSelf().getLogger().warn("【-- Event --】 Failed to register PlayerChatEvent listener!");
                getSelf().getLogger().warn("【-- Event --】 Consider using 'hook_packet' mode if this persists.");
            }
        }
        
        // 使用 hook_packet 方式
        if (mode == "hook_packet" || mode == "both") {
            getSelf().getLogger().info("【-- Hook --】 Enabling TextPacket hook for chat capture...");
            hookEnabled = true;
            getSelf().getLogger().info("【-- Hook --】 TextPacket hook enabled successfully");
        }
        
        if (mode != "event" && mode != "hook_packet" && mode != "both") {
            getSelf().getLogger().warn("【-- Config --】 Unknown chatCaptureMode '{}', defaulting to 'event'", std::string(mConfig.chatCaptureMode));
            // 默认使用 event 方式
            bool hasEvent = eventBus.hasEvent(ll::event::getEventId<ll::event::PlayerChatEvent>);
            mPlayerChatListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
                [this](ll::event::PlayerChatEvent& event) {
                    getSelf().getLogger().trace("【-- Event --】 PlayerChatEvent triggered");
                    auto& player = event.self();
                    std::string playerName = player.getRealName();
                    std::string message = event.message();

                    nlohmann::json msg;
                    msg["type"] = "player_chat";
                    msg["player_name"] = playerName;
                    msg["content"] = message;

                    mWsServer->broadcast(msg.dump());
                    getSelf().getLogger().info("【-- Chat --】 {}: {}", playerName, message);
                }
            );
        }
    } else {
        getSelf().getLogger().debug("【-- Config --】 Player chat broadcast is disabled in config");
    }

    getSelf().getLogger().info("【-- Plugin --】 mclistener-ws-server enabled successfully!");
    getSelf().getLogger().info("【-- WS --】 WebSocket server listening on ws://{}:{}", std::string(mConfig.host), mConfig.port);
    return true;
}

bool MclistenerWsServerMod::disable() {
    getSelf().getLogger().info("【-- Plugin --】 Disabling mclistener-ws-server...");
    getSelf().getLogger().debug("【-- Event --】 Removing event listeners...");

    auto& eventBus = ll::event::EventBus::getInstance();

    // 禁用 hook
    if (hookEnabled) {
        hookEnabled = false;
        getSelf().getLogger().debug("【-- Hook --】 TextPacket hook disabled");
    }
    g_modInstance = nullptr;

    // 取消订阅事件
    if (mPlayerJoinListener) {
        eventBus.removeListener(mPlayerJoinListener);
        mPlayerJoinListener = nullptr;
        getSelf().getLogger().debug("【-- Event --】 PlayerJoinEvent listener removed");
    }

    if (mPlayerLeaveListener) {
        eventBus.removeListener(mPlayerLeaveListener);
        mPlayerLeaveListener = nullptr;
        getSelf().getLogger().debug("【-- Event --】 PlayerDisconnectEvent listener removed");
    }

    if (mPlayerChatListener) {
        eventBus.removeListener(mPlayerChatListener);
        mPlayerChatListener = nullptr;
        getSelf().getLogger().debug("【-- Event --】 PlayerChatEvent listener removed");
    }

    // 停止 WebSocket 服务器
    if (mWsServer) {
        getSelf().getLogger().debug("【-- WS --】 Stopping WebSocket server...");
        mWsServer->stop();
        mWsServer.reset();
        getSelf().getLogger().debug("【-- WS --】 WebSocket server stopped and cleaned up");
    }

    getSelf().getLogger().info("【-- Plugin --】 mclistener-ws-server disabled successfully!");
    return true;
}

} // namespace mclistener_ws_server

LL_REGISTER_MOD(mclistener_ws_server::MclistenerWsServerMod, mclistener_ws_server::MclistenerWsServerMod::getInstance());
