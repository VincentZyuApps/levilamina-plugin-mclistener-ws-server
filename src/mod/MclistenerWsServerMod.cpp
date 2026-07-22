#include "mod/MclistenerWsServerMod.h"
#include "mod/WebSocketServer.h"
#include "mod/WsMessageHandler.h"

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

static MclistenerWsServerMod* g_modInstance = nullptr;
static bool hookEnabled = false;

// Hook TextPacket：High 优先级，在 LeviLamina PlayerChatEvent (Normal=200) 之前捕获
LL_TYPE_INSTANCE_HOOK(
    TextPacketHook,
    ll::memory::HookPriority::High,
    ServerNetworkHandler,
    &ServerNetworkHandler::$handle,
    void,
    NetworkIdentifier const& identifier,
    TextPacket const&        packet
) {
    if (hookEnabled && g_modInstance && g_modInstance->getConfig().enablePlayerChatBroadcast) {
        try {
            auto player = thisFor<NetEventCallback>()->_getServerPlayer(identifier, packet.mSenderSubId);
            if (player) {
                std::string msg        = packet.getMessage();
                std::string playerName = player->getRealName();
                if (msg.empty()) { origin(identifier, packet); return; }

                nlohmann::json jsonMsg;
                jsonMsg["type"]        = "player_chat";
                jsonMsg["player_name"] = playerName;
                jsonMsg["content"]     = msg;

                if (auto* ws = g_modInstance->getWebSocketServer()) {
                    ws->broadcast(jsonMsg.dump());
                    g_modInstance->getSelf().getLogger().info("【-- Chat --】 {}: {}", playerName, msg);
                }
            }
        } catch (const std::exception& e) {
            g_modInstance->getSelf().getLogger().error("【-- Hook --】 TextPacketHook error: {}", e.what());
        } catch (...) {
            g_modInstance->getSelf().getLogger().error("【-- Hook --】 TextPacketHook unknown error");
        }
    }
    origin(identifier, packet);
}

static ll::memory::HookRegistrar<TextPacketHook> textPacketHookRegistrar;

static ll::io::LogLevel parseLogLevel(const std::string& levelStr) {
    std::string lower = levelStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
    if (lower == "silent" || lower == "off") return ll::io::LogLevel::Off;
    if (lower == "fatal")                    return ll::io::LogLevel::Fatal;
    if (lower == "error")                    return ll::io::LogLevel::Error;
    if (lower == "warn" || lower == "warning") return ll::io::LogLevel::Warn;
    if (lower == "info")                     return ll::io::LogLevel::Info;
    if (lower == "debug")                    return ll::io::LogLevel::Debug;
    if (lower == "trace")                    return ll::io::LogLevel::Trace;
    return ll::io::LogLevel::Info;
}

MclistenerWsServerMod& MclistenerWsServerMod::getInstance() {
    static MclistenerWsServerMod instance;
    return instance;
}

bool MclistenerWsServerMod::load() {
    auto& logger = getSelf().getLogger();
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

    const auto& configFilePath = getSelf().getConfigDir() / "config.json";
    if (!ll::config::loadConfig(mConfig, configFilePath)) {
        logger.warn("【-- Config --】 Cannot load configurations from {}", configFilePath.string());
        logger.info("【-- Config --】 Saving default configurations...");
        if (!ll::config::saveConfig(mConfig, configFilePath))
            logger.error("【-- Config --】 Failed to save default configurations!");
    }

    ll::io::LogLevel logLevel = parseLogLevel(mConfig.logLevel);
    logger.setLevel(logLevel);
    logger.info("【-- Config --】 Log level set to: {}", std::string(mConfig.logLevel));
    logger.debug("【-- Config --】   - host: {}", std::string(mConfig.host));
    logger.debug("【-- Config --】   - port: {}", mConfig.port);
    logger.debug("【-- Config --】   - wsToken: {}", mConfig.wsToken.empty() ? "(disabled)" : "*** (enabled)");
    logger.debug("【-- Config --】   - execCommandMode: {}", std::string(mConfig.execCommandMode));

    logger.info("【-- Plugin --】 mclistener-ws-server loaded successfully!");
    return true;
}

bool MclistenerWsServerMod::enable() {
    getSelf().getLogger().info("【-- Plugin --】 Enabling mclistener-ws-server...");

    mWsServer = std::make_unique<WebSocketServer>(mConfig.host, mConfig.port, this);
    if (!mWsServer->start()) {
        getSelf().getLogger().fatal("【-- Plugin --】 Plugin cannot function without WebSocket server!");
        return false;
    }

    if (mConfig.enableReceiveGroupMessage) {
        mWsServer->setMessageCallback([this](const std::string& message) {
            handleWsMessage(message, mConfig, mWsServer.get(), this);
        });
        getSelf().getLogger().debug("【-- Callback --】 Message callback registered successfully");
    }

    auto& eventBus = ll::event::EventBus::getInstance();

    if (mConfig.enablePlayerJoinBroadcast) {
        mPlayerJoinListener = eventBus.emplaceListener<ll::event::PlayerJoinEvent>(
            [this](ll::event::PlayerJoinEvent& event) {
                nlohmann::json msg;
                msg["type"]        = "player_join";
                msg["player_name"] = event.self().getRealName();
                mWsServer->broadcast(msg.dump());
                getSelf().getLogger().info("【-- Player --】 Player {} joined", event.self().getRealName());
            }
        );
        getSelf().getLogger().info("【-- Event --】 PlayerJoinEvent listener registered");
    }

    if (mConfig.enablePlayerLeaveBroadcast) {
        mPlayerLeaveListener = eventBus.emplaceListener<ll::event::PlayerDisconnectEvent>(
            [this](ll::event::PlayerDisconnectEvent& event) {
                nlohmann::json msg;
                msg["type"]        = "player_leave";
                msg["player_name"] = event.self().getRealName();
                mWsServer->broadcast(msg.dump());
                getSelf().getLogger().info("【-- Player --】 Player {} left", event.self().getRealName());
            }
        );
        getSelf().getLogger().info("【-- Event --】 PlayerDisconnectEvent listener registered");
    }

    if (mConfig.enablePlayerChatBroadcast) {
        g_modInstance = this;
        std::string mode = mConfig.chatCaptureMode;
        std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c){ return std::tolower(c); });
        getSelf().getLogger().info("【-- Config --】 Chat capture mode: {}", std::string(mConfig.chatCaptureMode));

        if (mode == "event" || mode == "both") {
            mPlayerChatListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
                [this](ll::event::PlayerChatEvent& event) {
                    nlohmann::json msg;
                    msg["type"]        = "player_chat";
                    msg["player_name"] = event.self().getRealName();
                    msg["content"]     = event.message();
                    mWsServer->broadcast(msg.dump());
                    getSelf().getLogger().info("【-- Chat --】 [Event] {}: {}", event.self().getRealName(), event.message());
                },
                ll::event::EventPriority::High
            );
            getSelf().getLogger().info("【-- Event --】 PlayerChatEvent listener registered (High priority)");
        }
        if (mode == "hook_packet" || mode == "both") {
            hookEnabled = true;
            getSelf().getLogger().info("【-- Hook --】 TextPacket hook enabled");
        }
    }

    getSelf().getLogger().info("【-- Plugin --】 mclistener-ws-server enabled successfully!");
    getSelf().getLogger().info("【-- WS --】 WebSocket server listening on ws://{}:{}", std::string(mConfig.host), mConfig.port);
    return true;
}

bool MclistenerWsServerMod::disable() {
    getSelf().getLogger().info("【-- Plugin --】 Disabling mclistener-ws-server...");
    hookEnabled = false;
    g_modInstance = nullptr;

    auto& eventBus = ll::event::EventBus::getInstance();
    if (mPlayerJoinListener)  { eventBus.removeListener(mPlayerJoinListener);  mPlayerJoinListener  = nullptr; }
    if (mPlayerLeaveListener) { eventBus.removeListener(mPlayerLeaveListener); mPlayerLeaveListener = nullptr; }
    if (mPlayerChatListener)  { eventBus.removeListener(mPlayerChatListener);  mPlayerChatListener  = nullptr; }

    if (mWsServer) { mWsServer->stop(); mWsServer.reset(); }

    getSelf().getLogger().info("【-- Plugin --】 mclistener-ws-server disabled successfully!");
    return true;
}

bool MclistenerWsServerMod::unload() {
    getSelf().getLogger().info("【-- Plugin --】 Unloading mclistener-ws-server...");
    getSelf().getLogger().info("【-- Plugin --】 mclistener-ws-server unloaded successfully!");
    return true;
}

} // namespace mclistener_ws_server

LL_REGISTER_MOD(mclistener_ws_server::MclistenerWsServerMod, mclistener_ws_server::MclistenerWsServerMod::getInstance());
