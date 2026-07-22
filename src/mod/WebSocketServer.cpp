#include "mod/WebSocketServer.h"
#include "mod/MclistenerWsServerMod.h"

#include <exception>
#include <utility>
#include <vector>

namespace mclistener_ws_server {

WebSocketServer::WebSocketServer(const std::string& host, int port, MclistenerWsServerMod* mod)
    : mHost(host), mPort(port), mMod(mod) {
}

WebSocketServer::~WebSocketServer() {
    stop();
}

bool WebSocketServer::start() {
    mMod->getSelf().getLogger().debug("【-- WS init --】 Initializing Winsock...");

    // 初始化 Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        mMod->getSelf().getLogger().error("【-- WS init --】 WSAStartup failed: {}", result);
        return false;
    }
    mMod->getSelf().getLogger().trace("【-- WS init --】 Winsock initialized successfully");

    // 创建服务器 socket
    mMod->getSelf().getLogger().debug("【-- WS socket --】 Creating server socket...");
    mServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mServerSocket == INVALID_SOCKET) {
        mMod->getSelf().getLogger().error("【-- WS socket --】 Failed to create socket: {}", WSAGetLastError());
        WSACleanup();
        return false;
    }
    mMod->getSelf().getLogger().trace("【-- WS socket --】 Server socket created: {}", static_cast<int>(mServerSocket));

    // 设置 socket 选项
    int opt = 1;
    setsockopt(mServerSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    mMod->getSelf().getLogger().trace("【-- WS socket --】 Socket options set (SO_REUSEADDR)");

    // 绑定地址
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(mPort));

    if (mHost == "0.0.0.0") {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        mMod->getSelf().getLogger().debug("【-- WS socket --】 Binding to all interfaces (0.0.0.0)");
    } else {
        inet_pton(AF_INET, mHost.c_str(), &serverAddr.sin_addr);
        mMod->getSelf().getLogger().debug("【-- WS socket --】 Binding to specific host: {}", mHost);
    }

    if (bind(mServerSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        mMod->getSelf().getLogger().error("【-- WS socket --】 Bind failed on port {}: {}", mPort, WSAGetLastError());
        closesocket(mServerSocket);
        WSACleanup();
        return false;
    }
    mMod->getSelf().getLogger().debug("【-- WS socket --】 Successfully bound to port {}", mPort);

    // 开始监听
    if (listen(mServerSocket, SOMAXCONN) == SOCKET_ERROR) {
        mMod->getSelf().getLogger().error("【-- WS socket --】 Listen failed: {}", WSAGetLastError());
        closesocket(mServerSocket);
        WSACleanup();
        return false;
    }
    mMod->getSelf().getLogger().debug("【-- WS socket --】 Socket is now listening (backlog: SOMAXCONN)");

    mRunning = true;
    mAcceptThread = std::thread(&WebSocketServer::acceptLoop, this);
    mMod->getSelf().getLogger().debug("【-- WS accept --】 Accept thread started");

    mMod->getSelf().getLogger().info("【-- WS server --】 WebSocket server started on ws://{}:{}", mHost, mPort);
    return true;
}

void WebSocketServer::stop() {
    if (!mRunning.exchange(false)) {
        mMod->getSelf().getLogger().debug("【-- WS server --】 WebSocket server already stopped");
        return;
    }

    mMod->getSelf().getLogger().debug("【-- WS server --】 Stopping WebSocket server...");

    // 关闭服务器 socket，这会让 accept() 返回
    if (mServerSocket != INVALID_SOCKET) {
        mMod->getSelf().getLogger().trace("【-- WS server --】 Closing server socket...");
        closesocket(mServerSocket);
        mServerSocket = INVALID_SOCKET;
    }

    // 等待接受线程结束
    if (mAcceptThread.joinable()) {
        mMod->getSelf().getLogger().trace("【-- WS server --】 Waiting for accept thread to finish...");
        mAcceptThread.join();
    }

    // 唤醒所有可能阻塞在握手或接收操作中的客户端线程
    {
        std::lock_guard<std::mutex> lock(mClientsMutex);
        mMod->getSelf().getLogger().debug(
            "【-- WS server --】 Shutting down {} client sessions...",
            mSessionSockets.size()
        );
        for (SOCKET client : mSessionSockets) {
            shutdown(client, SD_BOTH);
        }
    }

    // DLL 卸载前必须等待所有客户端线程彻底退出
    for (auto& clientThread : mClientThreads) {
        if (clientThread.thread.joinable()) {
            clientThread.thread.join();
        }
    }
    mClientThreads.clear();

    {
        std::lock_guard<std::mutex> lock(mClientsMutex);
        mClients.clear();
        mSessionSockets.clear();
    }

    WSACleanup();
    mMod->getSelf().getLogger().info("【-- WS server --】 WebSocket server stopped");
}

void WebSocketServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(mClientsMutex);

    mMod->getSelf().getLogger().trace("【-- WS broadcast --】 Broadcasting to {} clients: {}", mClients.size(), message);

    std::vector<SOCKET> disconnected;

    for (SOCKET client : mClients) {
        if (!sendFrame(client, message)) {
            mMod->getSelf().getLogger().debug("【-- WS broadcast --】 Failed to send to client, marking for removal");
            disconnected.push_back(client);
        }
    }

    // 移除断开的连接
    for (SOCKET client : disconnected) {
        mClients.erase(client);
        closesocket(client);
        mMod->getSelf().getLogger().debug("【-- WS broadcast --】 Removed disconnected client");
    }

    if (!disconnected.empty()) {
        mMod->getSelf().getLogger().debug("【-- WS broadcast --】 Removed {} disconnected clients, {} remaining",
                                          disconnected.size(), mClients.size());
    }
}

void WebSocketServer::setMessageCallback(MessageCallback callback) {
    mMessageCallback = std::move(callback);
    mMod->getSelf().getLogger().debug("【-- WS callback --】 Message callback set");
}

void WebSocketServer::reapFinishedClientThreads() {
    for (auto it = mClientThreads.begin(); it != mClientThreads.end();) {
        if (!it->finished || !it->finished->load(std::memory_order_acquire)) {
            ++it;
            continue;
        }
        if (it->thread.joinable()) {
            it->thread.join();
        }
        it = mClientThreads.erase(it);
    }
}

void WebSocketServer::acceptLoop() {
    mMod->getSelf().getLogger().debug("【-- WS accept --】 Accept loop started");
    while (mRunning) {
        reapFinishedClientThreads();

        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(mServerSocket, (sockaddr*)&clientAddr, &clientAddrLen);

        if (clientSocket == INVALID_SOCKET) {
            if (mRunning) {
                // 真正的错误
                int error = WSAGetLastError();
                if (error != WSAEINTR && error != WSAENOTSOCK) {
                    mMod->getSelf().getLogger().warn("【-- WS accept --】 Accept failed with error: {}", error);
                }
            }
            continue;
        }

        bool sessionAccepted = false;
        {
            std::lock_guard<std::mutex> lock(mClientsMutex);
            if (mRunning) {
                mSessionSockets.insert(clientSocket);
                sessionAccepted = true;
            }
        }
        if (!sessionAccepted) {
            closesocket(clientSocket);
            break;
        }

        // 获取客户端 IP
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        mMod->getSelf().getLogger().info("【-- WS accept --】 New WebSocket connection from {}:{}", clientIP, ntohs(clientAddr.sin_port));
        mMod->getSelf().getLogger().debug("【-- WS accept --】 Client socket: {}", static_cast<int>(clientSocket));

        // 在新线程中处理客户端
        bool workerAdded = false;
        try {
            auto finished = std::make_shared<std::atomic<bool>>(false);
            mClientThreads.emplace_back();
            workerAdded = true;
            auto& worker = mClientThreads.back();
            worker.finished = finished;
            worker.thread = std::thread([this, clientSocket, clientIP = std::string(clientIP), finished]() {
                try {
                    mMod->getSelf().getLogger().trace("【-- WS accept --】 Starting client handler thread for {}", clientIP);
                    handleClient(clientSocket);
                } catch (const std::exception& e) {
                    mMod->getSelf().getLogger().error("【-- WS client --】 Unhandled client thread error: {}", e.what());
                    closeClient(clientSocket);
                } catch (...) {
                    mMod->getSelf().getLogger().error("【-- WS client --】 Unknown client thread error");
                    closeClient(clientSocket);
                }
                finished->store(true, std::memory_order_release);
            });
        } catch (const std::exception& e) {
            if (workerAdded) {
                mClientThreads.pop_back();
            }
            closeClient(clientSocket);
            mMod->getSelf().getLogger().error("【-- WS accept --】 Failed to start client thread: {}", e.what());
        }
    }
    reapFinishedClientThreads();
    mMod->getSelf().getLogger().debug("【-- WS accept --】 Accept loop ended");
}

} // namespace mclistener_ws_server
