#pragma once

#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>

// Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

namespace mclistener_ws_server {

// 前向声明
class MclistenerWsServerMod;

/**
 * 简单的 WebSocket 服务器实现
 * 用于与 koishi-plugin-mclistener-ws-client 通信
 */
class WebSocketServer {
public:
    using MessageCallback = std::function<void(const std::string& message)>;

    WebSocketServer(const std::string& host, int port, MclistenerWsServerMod* mod);
    ~WebSocketServer();

    // 禁止拷贝
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    // 启动服务器
    bool start();

    // 停止服务器
    void stop();

    // 广播消息给所有连接的客户端
    void broadcast(const std::string& message);

    // 设置消息回调
    void setMessageCallback(MessageCallback callback);

    // 检查服务器是否正在运行
    bool isRunning() const { return mRunning; }

private:
    struct WebSocketFrame {
        std::uint8_t opcode = 0;
        bool fin = false;
        std::string payload;
    };

    struct ClientThread {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> finished;
    };

    // 接受连接的线程函数
    void acceptLoop();

    // 处理单个客户端连接
    void handleClient(SOCKET clientSocket);

    // 关闭并移除客户端连接
    size_t closeClient(SOCKET clientSocket);

    // 回收已经结束的客户端线程
    void reapFinishedClientThreads();

    // WebSocket 握手
    bool performHandshake(SOCKET clientSocket, bool& urlAuthPassed);

    // 发送 WebSocket 帧
    bool sendFrame(SOCKET clientSocket, const std::string& message);
    bool sendFrame(SOCKET clientSocket, std::uint8_t opcode, const std::string& payload);

    // 接收 WebSocket 帧
    bool receiveFrame(SOCKET clientSocket, WebSocketFrame& frame);

    // TCP 是字节流，单次 send/recv 不保证处理完整缓冲区
    bool recvExact(SOCKET socket, void* buffer, size_t length);
    bool sendAll(SOCKET socket, const void* buffer, size_t length);

    // 计算 WebSocket Accept Key
    std::string computeAcceptKey(const std::string& clientKey);

    // Base64 编码
    std::string base64Encode(const unsigned char* data, size_t length);

    // SHA1 哈希
    void sha1(const std::string& input, unsigned char output[20]);

    std::string mHost;
    int mPort;
    MclistenerWsServerMod* mMod;
    
    SOCKET mServerSocket = INVALID_SOCKET;
    std::atomic<bool> mRunning{false};
    std::thread mAcceptThread;
    
    std::mutex mClientsMutex;
    std::mutex mSendMutex;
    std::set<SOCKET> mClients;
    std::set<SOCKET> mSessionSockets;
    std::vector<ClientThread> mClientThreads;
    
    MessageCallback mMessageCallback;
};

} // namespace mclistener_ws_server
