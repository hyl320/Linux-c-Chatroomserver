# High Performance Epoll Reactor Chat Server (高性能即时通讯服务端)

<div align="center">

![C++17](https://img.shields.io/badge/C++-17-blue.svg?style=flat-square&logo=c%2B%2B)
![Linux](https://img.shields.io/badge/Platform-Linux%20%7C%20WSL-lightgrey.svg?style=flat-square&logo=linux)
![Architecture](https://img.shields.io/badge/I/O-Epoll%20Reactor%20%2B%20ThreadPool-orange.svg?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)

**基于 Linux C++17 (Epoll Reactor + 线程池) 架构的高并发长连接即时通讯服务端**  
*定长包头防粘包协议、零外部依赖轻量 JSON 解析、30 秒超时僵死连接自动巡检清理机制*

</div>

---

## 🌟 服务端核心亮点

- 🚀 **Epoll Reactor 多路复用 + 线程池**：
  - 彻底摒弃阻塞调用 `recv()` 的设计，采用非阻塞 Socket 与 Epoll 事件多路复用监听。
  - I/O 事件驱动与业务逻辑彻底解耦，解包后的完整帧任务统一派发至工作线程池并发处理，单机支持海量长连接。
- 📦 **定长包头防粘包帧切分**：
  - 协议头统一为 16 字节（魔数 `0x5A5A` + 消息类型 + 长度 + 时间戳）。
  - 各连接独立维护 `ClientContext` 环形缓冲，自动处理流式网络传输中的粘包、半包问题，保证数据包完整准确派发。
- 🛡️ **僵死连接巡检与自愈清理**：
  - 独立后台心跳巡检线程每 5 秒自动扫描全量客户端。
  - 侦测到超过 30 秒无任何交互的超时僵死连接时，自动执行安全清理、释放 Socket 资源并全网广播 `USER_LEAVE` 下线通知。
- 💡 **轻量级零依赖 JSON 解析**：
  - 纯手工实现轻量级 `SimpleJson`，极致轻快，零第三方库依赖，编译开箱即用。

---

## 📁 源码结构

```text
server/Linux-c-Chatroomserver/
├── header/
│   ├── chatServer.h         # Epoll Reactor、线程池与心跳巡检核心类定义
│   ├── Protocol.h           # 定长包头 (0x5A5A) 帧格式与 MessageType 状态枚举
│   └── SimpleJson.h         # 零第三方依赖的轻量 JSON 解析与序列化器
├── src/
│   ├── chatServer.cpp       # Reactor 事件循环、连接管理、消息广播与线程池分发
│   └── main.cpp             # 命令行参数解析与服务器启动入口
├── tests/
│   └── test_protocol.cpp    # 协议帧切包与序列化单元测试
└── CMakeLists.txt           # CMake 构建脚本 (C++17, pthread)
```

---

## 🔨 编译与运行

### 1. 编译构建
```bash
cd server/Linux-c-Chatroomserver
mkdir -p build && cd build
cmake .. -DCMAKE_CXX_COMPILER=clang++
make -j$(nproc)
```

### 2. 启动服务
```bash
# 启动监听 0.0.0.0:8888
./ChatServer 0.0.0.0 8888
```
