#pragma once

#include "ll/api/mod/NativeMod.h"
#include "ll/api/event/ListenerBase.h"
#include "mod/Config.h"

#include <memory>

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

    /// @return True if the mod is loaded successfully.
    bool load();

    /// @return True if the mod is enabled successfully.
    bool enable();

    /// @return True if the mod is disabled successfully.
    bool disable();

    /// @return True if the mod is unloaded successfully.
    bool unload();

private:
    ll::mod::NativeMod& mSelf;
    Config mConfig;
    
    // WebSocket 服务器实例
    std::unique_ptr<WebSocketServer> mWsServer;
    
    // 事件监听器
    ll::event::ListenerPtr mPlayerJoinListener;
    ll::event::ListenerPtr mPlayerLeaveListener;
    ll::event::ListenerPtr mPlayerChatListener;
};

} // namespace mclistener_ws_server
