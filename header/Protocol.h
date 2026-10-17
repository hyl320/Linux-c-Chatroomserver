#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>

namespace Chat {

constexpr uint16_t MAGIC_HEADER = 0x5A5A;
constexpr uint32_t MAX_BODY_LEN = 65536; // 最大包体 64KB

enum class MsgType : uint16_t {
    UNKNOWN     = 0,
    LOGIN_REQ   = 1, // 客户端登录请求
    LOGIN_RESP  = 2, // 服务端登录响应
    CHAT        = 3, // 聊天消息 (群聊 / 私聊)
    USER_LIST   = 4, // 在线用户列表下发
    USER_JOIN   = 5, // 用户上线广播
    USER_LEAVE  = 6, // 用户下线广播
    HEARTBEAT   = 7, // 心跳保活 (Ping/Pong)
    QUIT        = 8  // 主动退出
};

#pragma pack(push, 1)
struct MsgHeader {
    uint16_t magic;      // 0x5A5A
    uint16_t msg_type;   // MsgType 枚举值 (网络字节序)
    uint32_t body_len;   // 包体长度 (网络字节序)
    uint64_t timestamp;  // 时间戳毫秒 (网络字节序)
};
#pragma pack(pop)

constexpr size_t HEADER_SIZE = sizeof(MsgHeader);

struct UserInfo {
    std::string username;
    int avatar_id = 1;
};

struct ChatMessage {
    std::string sender;
    std::string receiver; // "ALL" 表示群聊广播，具体用户名表示单发私聊
    std::string content;
    int avatar_id = 1;
    uint64_t timestamp = 0;
};

} // namespace Chat

#endif // PROTOCOL_H
