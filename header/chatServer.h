#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <vector>
#include <thread>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <string>
#include <atomic>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "Protocol.h"
#include "SimpleJson.h"

namespace Chat {

struct ClientContext {
    int fd = -1;
    struct sockaddr_in cin;
    std::string in_buffer;        // 接收缓冲区，解决粘包/半包
    std::string username;         // 登录绑定的用户名
    int avatar_id = 1;
    bool logged_in = false;
    uint64_t last_heartbeat = 0;  // 上次通信时间戳
};

class chatServer {
public:
    chatServer(const char* ip, int port, size_t threadPoolSize = 4);
    ~chatServer();

    void run();
    void stopServer();

    // 发送与广播
    void sendPacket(int client_fd, MsgType type, const std::string& payload);
    void broadcastPacket(MsgType type, const std::string& payload, int exclude_fd = -1);
    void sendToUser(const std::string& targetUser, MsgType type, const std::string& payload);

private:
    int sfd = -1;
    int epfd = -1;
    std::atomic<bool> stop{false};

    // 客户端会话映射 (fd -> ClientContext)
    std::unordered_map<int, ClientContext> clients;
    std::unordered_map<std::string, int> user_fd_map; // username -> fd
    std::mutex client_mutex;

    // 线程池
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex task_mutex;
    std::condition_variable task_cv;

    void startThreadPool(size_t numThreads);
    void addTask(std::function<void()> task);

    // Epoll 事件分发
    void setNonBlocking(int fd);
    void handleNewConnection();
    void handleClientRead(int client_fd);
    void onClientDisconnect(int client_fd);

    // 业务处理逻辑 (运行在工作线程中)
    void processMessage(int client_fd, const MsgHeader& header, const std::string& payload);
    void handleLogin(int client_fd, const std::string& payload);
    void handleChat(int client_fd, const std::string& payload);
    void handleQuit(int client_fd);

    uint64_t getCurrentTimeMs();
};

} // namespace Chat

#endif // CHATSERVER_H