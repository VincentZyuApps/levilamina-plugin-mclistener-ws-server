#include "mod/WebSocketServer.h"
#include "mod/MclistenerWsServerMod.h"

#include <exception>
#include <nlohmann/json.hpp>

namespace mclistener_ws_server {

size_t WebSocketServer::closeClient(SOCKET clientSocket) {
    bool owned = false;
    size_t remainingClients = 0;
    {
        std::lock_guard<std::mutex> lock(mClientsMutex);
        mClients.erase(clientSocket);
        owned = mSessionSockets.erase(clientSocket) > 0;
        remainingClients = mClients.size();
    }

    if (owned) {
        shutdown(clientSocket, SD_BOTH);
        closesocket(clientSocket);
    }
    return remainingClients;
}

void WebSocketServer::handleClient(SOCKET clientSocket) {
    mMod->getSelf().getLogger().debug("【-- WS handshake --】 Performing WebSocket handshake...");

    // 执行 WebSocket 握手
    bool urlAuthPassed = false;
    if (!performHandshake(clientSocket, urlAuthPassed)) {
        mMod->getSelf().getLogger().warn("【-- WS handshake --】 WebSocket handshake failed for socket {}", static_cast<int>(clientSocket));
        closeClient(clientSocket);
        return;
    }

    mMod->getSelf().getLogger().debug("【-- WS handshake --】 WebSocket handshake successful");

    // Post-connection message auth（wsTokenMode == "message" 或 "any" 且 URL 未通过时）
    {
        const auto& wsToken = mMod->getConfig().wsToken;
        const auto& mode    = mMod->getConfig().wsTokenMode;
        bool needsMsgAuth   = !wsToken.empty()
            && (mode == "message" || (mode == "any" && !urlAuthPassed));
        if (needsMsgAuth) {
            DWORD timeout = static_cast<DWORD>(mMod->getConfig().wsTokenAuthTimeoutMs);
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            WebSocketFrame firstFrame;
            bool received = receiveFrame(clientSocket, firstFrame);
            DWORD zero = 0;
            setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&zero, sizeof(zero));
            bool authOk = false;
            try {
                if (received && firstFrame.opcode == 0x1) {
                    auto j = nlohmann::json::parse(firstFrame.payload);
                    authOk = (j.value("type", "") == "auth" && j.value("token", "") == wsToken);
                }
            } catch (...) {}
            if (!authOk) {
                mMod->getSelf().getLogger().warn("【-- WS auth --】 Message auth failed or timed out, rejecting socket {}", static_cast<int>(clientSocket));
                closeClient(clientSocket);
                return;
            }
            mMod->getSelf().getLogger().debug("【-- WS auth --】 Message auth passed");
        }
    }

    // 添加到客户端列表
    size_t clientCount = 0;
    {
        std::lock_guard<std::mutex> lock(mClientsMutex);
        mClients.insert(clientSocket);
        clientCount = mClients.size();
    }

    mMod->getSelf().getLogger().info("【-- WS client --】 WebSocket client connected, total clients: {}", clientCount);

    // 接收消息循环
    mMod->getSelf().getLogger().trace("【-- WS client --】 Entering message receive loop for socket {}", static_cast<int>(clientSocket));
    while (mRunning) {
        WebSocketFrame frame;
        if (!receiveFrame(clientSocket, frame)) {
            mMod->getSelf().getLogger().debug("【-- WS client --】 Frame receive failed, connection may be closed");
            break;
        }

        if (frame.opcode == 0x8) {
            mMod->getSelf().getLogger().debug("【-- WS client --】 Close frame received");
            sendFrame(clientSocket, 0x8, frame.payload);
            break;
        }
        if (frame.opcode == 0x9) {
            mMod->getSelf().getLogger().trace("【-- WS heartbeat --】 Ping received, replying Pong");
            if (!sendFrame(clientSocket, 0xA, frame.payload)) break;
            continue;
        }
        if (frame.opcode == 0xA) {
            mMod->getSelf().getLogger().trace("【-- WS heartbeat --】 Pong received");
            continue;
        }
        if (frame.opcode != 0x1) {
            mMod->getSelf().getLogger().warn("【-- WS client --】 Unsupported WebSocket opcode: {}", static_cast<int>(frame.opcode));
            break;
        }

        mMod->getSelf().getLogger().debug("【-- WS message --】 Received WebSocket message ({} bytes): {}", frame.payload.length(), frame.payload);

        if (mMessageCallback) {
            try {
                mMod->getSelf().getLogger().trace("【-- WS callback --】 Invoking message callback...");
                mMessageCallback(frame.payload);
            } catch (const std::exception& e) {
                mMod->getSelf().getLogger().error("【-- WS message --】 Error in message callback: {}", e.what());
            }
        } else {
            mMod->getSelf().getLogger().warn("【-- WS callback --】 No message callback set, ignoring message");
        }
    }

    // 关闭连接并从客户端列表中移除
    const size_t remainingClients = closeClient(clientSocket);
    mMod->getSelf().getLogger().info("【-- WS client --】 WebSocket client disconnected, remaining clients: {}", remainingClients);
}

} // namespace mclistener_ws_server
