#include "mod/WebSocketServer.h"
#include "mod/MclistenerWsServerMod.h"

#include <sstream>
#include <vector>

namespace mclistener_ws_server {

// WebSocket GUID (RFC 6455)
static const char* WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

bool WebSocketServer::performHandshake(SOCKET clientSocket, bool& urlAuthPassed) {
    urlAuthPassed = false;
    mMod->getSelf().getLogger().trace("【-- WS handshake --】 Reading handshake request...");

    std::string request;
    char buffer[2048];
    while (request.find("\r\n\r\n") == std::string::npos) {
        int bytesReceived = recv(clientSocket, buffer, static_cast<int>(sizeof(buffer)), 0);
        if (bytesReceived <= 0) {
            mMod->getSelf().getLogger().debug("【-- WS handshake --】 No data received during handshake");
            return false;
        }
        request.append(buffer, static_cast<size_t>(bytesReceived));
        if (request.size() > 16 * 1024) {
            mMod->getSelf().getLogger().warn("【-- WS handshake --】 HTTP upgrade request is too large");
            return false;
        }
    }

    mMod->getSelf().getLogger().trace("【-- WS handshake --】 Received {} bytes for handshake", request.size());

    // 检查是否是 HTTP GET 请求
    if (request.find("GET ") != 0) {
        return false;
    }

    // ── Token 认证 ──────────────────────────────────────
    {
        const std::string& wsToken = mMod->getConfig().wsToken;
        const std::string& mode    = mMod->getConfig().wsTokenMode;

        if (wsToken.empty() || mode == "disabled") {
            urlAuthPassed = true; // 不需要校验
        } else if (mode != "message") {
            // "param" 或 "any"：尝试从 URL 提取 token
            std::string token;
            size_t pathStart = request.find(' ') + 1;
            size_t pathEnd   = request.find(' ', pathStart);
            if (pathStart != std::string::npos && pathEnd != std::string::npos) {
                std::string path = request.substr(pathStart, pathEnd - pathStart);
                size_t queryPos = path.find('?');
                if (queryPos != std::string::npos) {
                    std::string query = path.substr(queryPos + 1);
                    size_t tokenPos = query.find("token=");
                    if (tokenPos != std::string::npos) {
                        size_t valStart = tokenPos + 6;
                        size_t valEnd   = query.find('&', valStart);
                        token = (valEnd == std::string::npos)
                                    ? query.substr(valStart)
                                    : query.substr(valStart, valEnd - valStart);
                    }
                }
            }

            if (!token.empty() && token == wsToken) {
                urlAuthPassed = true;
                mMod->getSelf().getLogger().debug("【-- WS auth --】 URL param token authentication passed");
            } else if (mode == "param" || (!token.empty() && token != wsToken)) {
                // "param" 模式必须有 URL token，或者提供了错误的 token（任何模式都拒绝）
                mMod->getSelf().getLogger().warn("【-- WS auth --】 Client token does not match configured token, rejecting");
                std::string reject =
                    "HTTP/1.1 401 Unauthorized\r\n"
                    "Content-Type: text/plain\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "401 Unauthorized: Invalid or missing token";
                std::lock_guard<std::mutex> lock(mSendMutex);
                sendAll(clientSocket, reject.data(), reject.size());
                return false;
            }
            // mode == "any" && token.empty()：urlAuthPassed=false，后续 message auth 处理
        }
        // mode == "message"：urlAuthPassed=false，后续 message auth 处理
    }

    // 查找 Sec-WebSocket-Key
    std::string keyHeader = "Sec-WebSocket-Key: ";
    size_t keyPos = request.find(keyHeader);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t keyStart = keyPos + keyHeader.length();
    size_t keyEnd = request.find("\r\n", keyStart);
    if (keyEnd == std::string::npos) {
        return false;
    }

    std::string clientKey = request.substr(keyStart, keyEnd - keyStart);
    std::string acceptKey = computeAcceptKey(clientKey);

    // 构建响应
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n";
    response << "Upgrade: websocket\r\n";
    response << "Connection: Upgrade\r\n";
    response << "Sec-WebSocket-Accept: " << acceptKey << "\r\n";
    response << "\r\n";

    std::string responseStr = response.str();
    std::lock_guard<std::mutex> lock(mSendMutex);
    return sendAll(clientSocket, responseStr.data(), responseStr.size());
}

std::string WebSocketServer::computeAcceptKey(const std::string& clientKey) {
    std::string combined = clientKey + WS_GUID;

    unsigned char hash[20];
    sha1(combined, hash);

    return base64Encode(hash, 20);
}

// 简单的 SHA1 实现
void WebSocketServer::sha1(const std::string& input, unsigned char output[20]) {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    // 填充消息
    std::vector<unsigned char> msg(input.begin(), input.end());
    uint64_t originalBitLen = msg.size() * 8;

    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }

    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<unsigned char>((originalBitLen >> (i * 8)) & 0xFF));
    }

    // 处理每个 512 位块
    for (size_t chunk = 0; chunk < msg.size(); chunk += 64) {
        uint32_t w[80];

        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(msg[chunk + i * 4]) << 24) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(msg[chunk + i * 4 + 3]));
        }

        for (int i = 16; i < 80; ++i) {
            uint32_t temp = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = (temp << 1) | (temp >> 31);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    // 输出哈希值
    output[0] = static_cast<unsigned char>((h0 >> 24) & 0xFF);
    output[1] = static_cast<unsigned char>((h0 >> 16) & 0xFF);
    output[2] = static_cast<unsigned char>((h0 >> 8) & 0xFF);
    output[3] = static_cast<unsigned char>(h0 & 0xFF);
    output[4] = static_cast<unsigned char>((h1 >> 24) & 0xFF);
    output[5] = static_cast<unsigned char>((h1 >> 16) & 0xFF);
    output[6] = static_cast<unsigned char>((h1 >> 8) & 0xFF);
    output[7] = static_cast<unsigned char>(h1 & 0xFF);
    output[8] = static_cast<unsigned char>((h2 >> 24) & 0xFF);
    output[9] = static_cast<unsigned char>((h2 >> 16) & 0xFF);
    output[10] = static_cast<unsigned char>((h2 >> 8) & 0xFF);
    output[11] = static_cast<unsigned char>(h2 & 0xFF);
    output[12] = static_cast<unsigned char>((h3 >> 24) & 0xFF);
    output[13] = static_cast<unsigned char>((h3 >> 16) & 0xFF);
    output[14] = static_cast<unsigned char>((h3 >> 8) & 0xFF);
    output[15] = static_cast<unsigned char>(h3 & 0xFF);
    output[16] = static_cast<unsigned char>((h4 >> 24) & 0xFF);
    output[17] = static_cast<unsigned char>((h4 >> 16) & 0xFF);
    output[18] = static_cast<unsigned char>((h4 >> 8) & 0xFF);
    output[19] = static_cast<unsigned char>(h4 & 0xFF);
}

std::string WebSocketServer::base64Encode(const unsigned char* data, size_t length) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    result.reserve(((length + 2) / 3) * 4);

    for (size_t i = 0; i < length; i += 3) {
        unsigned int n = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < length) n |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < length) n |= static_cast<unsigned int>(data[i + 2]);

        result += chars[(n >> 18) & 0x3F];
        result += chars[(n >> 12) & 0x3F];
        result += (i + 1 < length) ? chars[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < length) ? chars[n & 0x3F] : '=';
    }

    return result;
}

} // namespace mclistener_ws_server
