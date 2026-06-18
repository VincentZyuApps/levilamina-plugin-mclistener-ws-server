#include "mod/WsMessageHandler.h"
#include "mod/MclistenerWsServerMod.h"
#include "mod/WebSocketServer.h"
#include "mod/ExecCommandNative.h"

#include "ll/api/service/Bedrock.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include <nlohmann/json.hpp>

namespace mclistener_ws_server {

void handleWsMessage(
    const std::string&     message,
    const Config&          config,
    WebSocketServer*       ws,
    MclistenerWsServerMod* mod
) {
    auto& logger = mod->getSelf().getLogger();
    logger.trace("【-- WS --】 Raw message received: {}", message);
    try {
        auto json = nlohmann::json::parse(message);
        std::string type = json.value("type", "");
        logger.debug("【-- WS --】 Parsed message type: {}", type);

        if (type == "chat_platform_to_server") {
            std::string groupId   = json.value("group_id", "");
            std::string groupName = json.value("group_name", "");
            std::string nickname  = json.value("nickname", "未知用户");
            std::string content   = json.value("message", "");

            std::string formattedMsg = config.groupMessageFormat;
            size_t pos;
            while ((pos = formattedMsg.find("{group_id}"))   != std::string::npos) formattedMsg.replace(pos, 10, groupId);
            while ((pos = formattedMsg.find("{group_name}")) != std::string::npos) formattedMsg.replace(pos, 12, groupName);
            while ((pos = formattedMsg.find("{nickname}"))   != std::string::npos) formattedMsg.replace(pos, 10, nickname);
            while ((pos = formattedMsg.find("{message}"))    != std::string::npos) formattedMsg.replace(pos, 9,  content);

            auto level = ll::service::getLevel();
            if (level) {
                level->forEachPlayer([&formattedMsg](Player& player) -> bool {
                    player.sendMessage(formattedMsg);
                    return true;
                });
            } else {
                logger.warn("【-- Group -> Server --】 Level not available, cannot broadcast message");
            }
            logger.info("【-- Group -> Server --】 [{}] {}: {}", groupName, nickname, content);

        } else if ((type == "external_command_to_server" || type == "command_result")
                   && config.execCommandMode == "js-relay") {
            if (ws) {
                ws->broadcast(message);
                logger.debug("【-- Relay --】 Relayed {} message", type);
            }

        } else if (type == "auth") {
            logger.debug("【-- WS auth --】 Received post-handshake auth message (ignored)");

        } else if (type == "external_command_to_server" && config.execCommandMode == "cpp-native") {
            std::string cmd   = json.value("command", "");
            std::string reqId = json.value("request_id", "");
            auto [ok, out] = execCommandCppNative(cmd);
            nlohmann::json res;
            res["type"]       = "command_result";
            res["request_id"] = reqId;
            res["command"]    = cmd;
            res["ok"]         = ok;
            res["result"]     = out;
            if (ws) ws->broadcast(res.dump());
            logger.info("【-- CppNative --】 exec '{}': {}", cmd, out);

        } else {
            logger.debug("【-- WS --】 Ignoring message with type: {}", type);
        }
    } catch (const nlohmann::json::parse_error& e) {
        mod->getSelf().getLogger().error("【-- WS --】 JSON parse error: {}", e.what());
        mod->getSelf().getLogger().debug("【-- WS --】 Invalid JSON: {}", message);
    } catch (const std::exception& e) {
        mod->getSelf().getLogger().error("【-- WS --】 Failed to process message: {}", e.what());
    }
}

} // namespace mclistener_ws_server
