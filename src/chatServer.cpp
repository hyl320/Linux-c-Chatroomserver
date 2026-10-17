#include "chatServer.h"
#include <chrono>

namespace Chat {

chatServer::chatServer(const char *ip, int port, size_t threadPoolSize)
    : stop(false)
{
    // 1. 创建服务器套接字
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) {
        perror("socket error");
        return;
    }

    // 2. 端口复用
    int reuse = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt SO_REUSEADDR error");
    }

    // 3. 绑定地址
    struct sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &sin.sin_addr) <= 0) {
        sin.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(sfd, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) < 0) {
        perror("bind error");
        close(sfd);
        sfd = -1;
        return;
    }

    // 4. 监听
    if (listen(sfd, 128) < 0) {
        perror("listen error");
        close(sfd);
        sfd = -1;
        return;
    }

    setNonBlocking(sfd);

    // 5. 创建 Epoll
    epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1 error");
        close(sfd);
        sfd = -1;
        return;
    }

    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = sfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev) < 0) {
        perror("epoll_ctl add sfd error");
        close(sfd);
        close(epfd);
        sfd = -1;
        epfd = -1;
        return;
    }

    // 6. 启动工作线程池
    startThreadPool(threadPoolSize);
    std::cout << "[Server] Started successfully on " << ip << ":" << port
              << " with " << threadPoolSize << " worker threads." << std::endl;
}

chatServer::~chatServer() {
    stopServer();
}

void chatServer::stopServer() {
    bool expected = false;
    if (!stop.compare_exchange_strong(expected, true)) {
        return;
    }

    task_cv.notify_all();
    for (auto &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    std::unique_lock<std::mutex> lock(client_mutex);
    for (auto &kv : clients) {
        if (kv.second.fd >= 0) {
            close(kv.second.fd);
        }
    }
    clients.clear();
    user_fd_map.clear();

    if (epfd >= 0) close(epfd);
    if (sfd >= 0) close(sfd);
}

void chatServer::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

void chatServer::startThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(task_mutex);
                    task_cv.wait(lock, [this] {
                        return stop.load() || !tasks.empty();
                    });
                    if (stop.load() && tasks.empty()) {
                        return;
                    }
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                if (task) {
                    task();
                }
            }
        });
    }
}

void chatServer::addTask(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(task_mutex);
        tasks.push(std::move(task));
    }
    task_cv.notify_one();
}

uint64_t chatServer::getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count();
}

void chatServer::run() {
    if (sfd < 0 || epfd < 0) {
        std::cerr << "[Server] Server initialization failed, cannot run." << std::endl;
        return;
    }

    constexpr int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    while (!stop.load()) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait error");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == sfd) {
                // 新连接接入
                handleNewConnection();
            } else if (ev & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 连接异常或对端关闭
                onClientDisconnect(fd);
            } else if (ev & EPOLLIN) {
                // 可读数据到达
                handleClientRead(fd);
            }
        }
    }
}

void chatServer::handleNewConnection() {
    while (true) {
        struct sockaddr_in cin{};
        socklen_t len = sizeof(cin);
        int cfd = accept(sfd, reinterpret_cast<struct sockaddr*>(&cin), &len);
        if (cfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // 已处理完所有就绪连接
            }
            perror("accept error");
            break;
        }

        setNonBlocking(cfd);

        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
        ev.data.fd = cfd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev) < 0) {
            perror("epoll_ctl add client error");
            close(cfd);
            continue;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cin.sin_addr, ip_str, sizeof(ip_str));
        std::cout << "[Server] Client connected: " << ip_str << ":" << ntohs(cin.sin_port)
                  << " (fd=" << cfd << ")" << std::endl;

        std::unique_lock<std::mutex> lock(client_mutex);
        ClientContext ctx;
        ctx.fd = cfd;
        ctx.cin = cin;
        ctx.last_heartbeat = getCurrentTimeMs();
        clients[cfd] = ctx;
    }
}

void chatServer::handleClientRead(int client_fd) {
    char buf[4096];
    std::string newlyRead;

    // 循环非阻塞读取
    while (true) {
        ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
        if (n > 0) {
            newlyRead.append(buf, n);
        } else if (n == 0) {
            onClientDisconnect(client_fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // 读缓冲区已空
            }
            onClientDisconnect(client_fd);
            return;
        }
    }

    if (newlyRead.empty()) return;

    std::unique_lock<std::mutex> lock(client_mutex);
    auto it = clients.find(client_fd);
    if (it == clients.end()) return;

    it->second.in_buffer.append(newlyRead);
    std::string &inBuf = it->second.in_buffer;

    // 从缓冲区提取完整帧 (防粘包/半包)
    while (inBuf.size() >= HEADER_SIZE) {
        const MsgHeader *rawHdr = reinterpret_cast<const MsgHeader*>(inBuf.data());
        uint16_t magic = ntohs(rawHdr->magic);

        if (magic != MAGIC_HEADER) {
            // 魔数不符，滑动单字节纠错
            inBuf.erase(0, 1);
            continue;
        }

        uint32_t body_len = ntohl(rawHdr->body_len);
        if (body_len > MAX_BODY_LEN) {
            // 非法超长包，丢弃头部
            inBuf.erase(0, HEADER_SIZE);
            continue;
        }

        if (inBuf.size() < HEADER_SIZE + body_len) {
            // 半包，等待后续数据
            break;
        }

        MsgHeader hdr;
        hdr.magic = magic;
        hdr.msg_type = ntohs(rawHdr->msg_type);
        hdr.body_len = body_len;
        hdr.timestamp = (static_cast<uint64_t>(ntohl(rawHdr->timestamp >> 32)) << 32) |
                         ntohl(rawHdr->timestamp & 0xFFFFFFFF);

        std::string payload = inBuf.substr(HEADER_SIZE, body_len);
        inBuf.erase(0, HEADER_SIZE + body_len);

        // 投递到工作线程池执行业务逻辑，解耦 I/O 与业务！
        addTask([this, client_fd, hdr, payload] {
            this->processMessage(client_fd, hdr, payload);
        });
    }
}

void chatServer::onClientDisconnect(int client_fd) {
    std::string leftUser;
    {
        std::unique_lock<std::mutex> lock(client_mutex);
        auto it = clients.find(client_fd);
        if (it != clients.end()) {
            if (it->second.logged_in) {
                leftUser = it->second.username;
                user_fd_map.erase(leftUser);
            }
            clients.erase(it);
        }
    }

    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);

    if (!leftUser.empty()) {
        std::cout << "[Server] User disconnected: " << leftUser << " (fd=" << client_fd << ")" << std::endl;
        std::string leaveJson = SimpleJson::buildUserLeave(leftUser);
        broadcastPacket(MsgType::USER_LEAVE, leaveJson, -1);
    } else {
        std::cout << "[Server] Unauthenticated client closed: fd=" << client_fd << std::endl;
    }
}

void chatServer::processMessage(int client_fd, const MsgHeader& header, const std::string& payload) {
    auto type = static_cast<MsgType>(header.msg_type);

    switch (type) {
    case MsgType::LOGIN_REQ:
        handleLogin(client_fd, payload);
        break;
    case MsgType::CHAT:
        handleChat(client_fd, payload);
        break;
    case MsgType::HEARTBEAT: {
        std::unique_lock<std::mutex> lock(client_mutex);
        auto it = clients.find(client_fd);
        if (it != clients.end()) {
            it->second.last_heartbeat = getCurrentTimeMs();
        }
        lock.unlock();
        sendPacket(client_fd, MsgType::HEARTBEAT, SimpleJson::buildHeartbeat(true));
        break;
    }
    case MsgType::QUIT:
        handleQuit(client_fd);
        break;
    default:
        std::cerr << "[Server] Unknown message type: " << static_cast<int>(type) << std::endl;
        break;
    }
}

void chatServer::handleLogin(int client_fd, const std::string& payload) {
    std::string username = SimpleJson::getStr(payload, "username");
    int avatar = SimpleJson::getInt(payload, "avatar", 1);

    if (username.empty()) {
        sendPacket(client_fd, MsgType::LOGIN_RESP,
                   SimpleJson::buildLoginResp(1, "用户名不能为空", ""));
        return;
    }

    std::vector<UserInfo> currentUsers;
    {
        std::unique_lock<std::mutex> lock(client_mutex);
        if (user_fd_map.find(username) != user_fd_map.end()) {
            lock.unlock();
            sendPacket(client_fd, MsgType::LOGIN_RESP,
                       SimpleJson::buildLoginResp(2, "该用户名已被使用，请更换", ""));
            return;
        }

        auto it = clients.find(client_fd);
        if (it == clients.end()) return;

        it->second.username = username;
        it->second.avatar_id = avatar;
        it->second.logged_in = true;
        it->second.last_heartbeat = getCurrentTimeMs();
        user_fd_map[username] = client_fd;

        // 收集所有在线用户
        for (const auto &pair : clients) {
            if (pair.second.logged_in) {
                currentUsers.push_back({pair.second.username, pair.second.avatar_id});
            }
        }
    }

    std::cout << "[Server] User login success: " << username << " (avatar=" << avatar << ")" << std::endl;

    // 1. 响应登录成功
    sendPacket(client_fd, MsgType::LOGIN_RESP,
               SimpleJson::buildLoginResp(0, "OK", username, avatar));

    // 2. 下发当前全量在线用户列表
    sendPacket(client_fd, MsgType::USER_LIST, SimpleJson::buildUserList(currentUsers));

    // 3. 向其他在线用户广播新成员上线通知
    broadcastPacket(MsgType::USER_JOIN, SimpleJson::buildUserJoin(username, avatar), client_fd);
}

void chatServer::handleChat(int client_fd, const std::string& payload) {
    std::string sender = SimpleJson::getStr(payload, "sender");
    std::string receiver = SimpleJson::getStr(payload, "receiver");
    std::string content = SimpleJson::getStr(payload, "content");
    int avatar = SimpleJson::getInt(payload, "avatar", 1);
    uint64_t ts = getCurrentTimeMs();

    std::string forwardPayload = SimpleJson::buildChat(sender, receiver, content, avatar, ts);

    if (receiver == "ALL" || receiver.empty()) {
        // 群聊：广播给所有人 (包括自己以确保时序一致，或除自己外)
        broadcastPacket(MsgType::CHAT, forwardPayload, -1);
    } else {
        // 私聊：定向转发给目标接收者，同时回显给发送者
        sendToUser(receiver, MsgType::CHAT, forwardPayload);
        sendPacket(client_fd, MsgType::CHAT, forwardPayload);
    }
}

void chatServer::handleQuit(int client_fd) {
    onClientDisconnect(client_fd);
}

void chatServer::sendPacket(int client_fd, MsgType type, const std::string& payload) {
    MsgHeader hdr{};
    hdr.magic = htons(MAGIC_HEADER);
    hdr.msg_type = htons(static_cast<uint16_t>(type));
    hdr.body_len = htonl(static_cast<uint32_t>(payload.size()));

    uint64_t nowMs = getCurrentTimeMs();
    hdr.timestamp = (static_cast<uint64_t>(htonl(nowMs >> 32)) << 32) | htonl(nowMs & 0xFFFFFFFF);

    std::string packet;
    packet.append(reinterpret_cast<const char*>(&hdr), HEADER_SIZE);
    packet.append(payload);

    ssize_t sent = 0;
    while (sent < static_cast<ssize_t>(packet.size())) {
        ssize_t n = send(client_fd, packet.data() + sent, packet.size() - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            break;
        }
        sent += n;
    }
}

void chatServer::broadcastPacket(MsgType type, const std::string& payload, int exclude_fd) {
    std::vector<int> targetFds;
    {
        std::unique_lock<std::mutex> lock(client_mutex);
        for (const auto &pair : clients) {
            if (pair.second.logged_in && pair.first != exclude_fd) {
                targetFds.push_back(pair.first);
            }
        }
    }

    for (int fd : targetFds) {
        sendPacket(fd, type, payload);
    }
}

void chatServer::sendToUser(const std::string& targetUser, MsgType type, const std::string& payload) {
    int targetFd = -1;
    {
        std::unique_lock<std::mutex> lock(client_mutex);
        auto it = user_fd_map.find(targetUser);
        if (it != user_fd_map.end()) {
            targetFd = it->second;
        }
    }

    if (targetFd >= 0) {
        sendPacket(targetFd, type, payload);
    } else {
        std::cout << "[Server] Private message target not online: " << targetUser << std::endl;
    }
}

} // namespace Chat