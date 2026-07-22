#include "mod/WebSocketServer.h"
#include "mod/MclistenerWsServerMod.h"

#include <vector>

namespace mclistener_ws_server {

bool WebSocketServer::recvExact(SOCKET socket, void* buffer, size_t length) {
    auto* cursor = static_cast<char*>(buffer);
    size_t totalReceived = 0;
    while (totalReceived < length) {
        int received = recv(socket, cursor + totalReceived, static_cast<int>(length - totalReceived), 0);
        if (received == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) continue;
        if (received <= 0) return false;
        totalReceived += static_cast<size_t>(received);
    }
    return true;
}

bool WebSocketServer::sendAll(SOCKET socket, const void* buffer, size_t length) {
    const auto* cursor = static_cast<const char*>(buffer);
    size_t totalSent = 0;
    while (totalSent < length) {
        int sent = send(socket, cursor + totalSent, static_cast<int>(length - totalSent), 0);
        if (sent == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) continue;
        if (sent <= 0) return false;
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}

bool WebSocketServer::sendFrame(SOCKET clientSocket, const std::string& message) {
    return sendFrame(clientSocket, 0x1, message);
}

bool WebSocketServer::sendFrame(SOCKET clientSocket, std::uint8_t opcode, const std::string& payload) {
    if (opcode >= 0x8 && payload.size() > 125) return false;

    std::vector<unsigned char> frame;
    frame.push_back(static_cast<unsigned char>(0x80 | (opcode & 0x0F)));

    size_t length = payload.length();

    if (length <= 125) {
        frame.push_back(static_cast<unsigned char>(length));
    } else if (length <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<unsigned char>((length >> 8) & 0xFF));
        frame.push_back(static_cast<unsigned char>(length & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<unsigned char>((length >> (i * 8)) & 0xFF));
        }
    }

    frame.insert(frame.end(), payload.begin(), payload.end());

    std::lock_guard<std::mutex> lock(mSendMutex);
    return sendAll(clientSocket, frame.data(), frame.size());
}

bool WebSocketServer::receiveFrame(SOCKET clientSocket, WebSocketFrame& frame) {
    unsigned char header[2];
    if (!recvExact(clientSocket, header, sizeof(header))) return false;

    if ((header[0] & 0x70) != 0) return false;
    frame.fin = (header[0] & 0x80) != 0;
    frame.opcode = header[0] & 0x0F;

    bool masked = (header[1] & 0x80) != 0;
    uint64_t payloadLength = header[1] & 0x7F;

    if (payloadLength == 126) {
        unsigned char extLength[2];
        if (!recvExact(clientSocket, extLength, sizeof(extLength))) return false;
        payloadLength = (static_cast<uint64_t>(extLength[0]) << 8) | extLength[1];
    } else if (payloadLength == 127) {
        unsigned char extLength[8];
        if (!recvExact(clientSocket, extLength, sizeof(extLength))) return false;
        if ((extLength[0] & 0x80) != 0) return false;
        payloadLength = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLength = (payloadLength << 8) | extLength[i];
        }
    }

    const bool controlFrame = frame.opcode >= 0x8;
    if (!frame.fin || (controlFrame && payloadLength > 125)) {
        mMod->getSelf().getLogger().warn("【-- WS frame --】 Fragmented or oversized control frame is not supported");
        return false;
    }
    if (frame.opcode == 0x8 && payloadLength == 1) return false;

    // 读取掩码（如果有）
    unsigned char mask[4] = {0};
    if (masked) {
        if (!recvExact(clientSocket, mask, sizeof(mask))) return false;
    }

    // 读取负载
    if (payloadLength > 1024 * 1024) { // 限制最大 1MB
        return false;
    }

    frame.payload.resize(static_cast<size_t>(payloadLength));
    if (payloadLength > 0 && !recvExact(clientSocket, frame.payload.data(), frame.payload.size())) return false;

    // 解码消息（如果有掩码）
    if (masked) {
        for (size_t i = 0; i < payloadLength; ++i) {
            frame.payload[i] = static_cast<char>(
                static_cast<unsigned char>(frame.payload[i]) ^ mask[i % 4]
            );
        }
    }

    return true;
}

} // namespace mclistener_ws_server
