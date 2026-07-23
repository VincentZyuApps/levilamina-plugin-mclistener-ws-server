#pragma once

#include "ll/api/mod/NativeMod.h"
#include "ll/api/event/ListenerBase.h"
#include "mod/Config.h"

#include <atomic>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

class Level;

namespace mclistener_ws_server {

// 前向声明
class WebSocketServer;

class MclistenerWsServerMod {

public:
    static MclistenerWsServerMod& getInstance();

    MclistenerWsServerMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    [[nodiscard]] const Config& getConfig() const { return mConfig; }

    [[nodiscard]] WebSocketServer* getWebSocketServer() const { return mWsServer.get(); }

    bool enqueueGroupMessage(
        std::string groupName,
        std::string nickname,
        std::string content,
        std::string formattedMessage
    );

    /// @return True if the mod is loaded successfully.
    bool load();

    /// @return True if the mod is enabled successfully.
    bool enable();

    /// @return True if the mod is disabled successfully.
    bool disable();

    /// @return True if the mod is unloaded successfully.
    bool unload();

private:
    struct PendingGroupMessage {
        std::string groupName;
        std::string nickname;
        std::string content;
        std::string formattedMessage;
    };

    static constexpr std::size_t MaxQueuedGroupMessages = 256;
    static constexpr std::size_t MaxGroupMessagesPerTick = 32;

    void drainGroupMessages(Level& level);
    void clearGroupMessages();

    ll::mod::NativeMod& mSelf;
    Config mConfig;
    
    // WebSocket 服务器实例
    std::unique_ptr<WebSocketServer> mWsServer;
    
    // 事件监听器
    ll::event::ListenerPtr mPlayerJoinListener;
    ll::event::ListenerPtr mPlayerLeaveListener;
    ll::event::ListenerPtr mPlayerChatListener;
    ll::event::ListenerPtr mLevelTickListener;

    // WS 工作线程只入队，LevelTickEvent 在服务器线程中执行游戏 API。
    std::atomic_bool mAcceptingGroupMessages{false};
    std::mutex mGroupMessageQueueMutex;
    std::deque<PendingGroupMessage> mGroupMessageQueue;
};

} // namespace mclistener_ws_server
