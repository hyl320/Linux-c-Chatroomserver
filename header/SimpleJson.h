#ifndef SIMPLE_JSON_H
#define SIMPLE_JSON_H

#include <string>
#include <vector>
#include <sstream>
#include <cstdint>
#include <cctype>
#include "Protocol.h"

namespace Chat {

class SimpleJson {
public:
    static std::string escape(const std::string& s) {
        std::ostringstream o;
        for (char c : s) {
            if (c == '"') o << "\\\"";
            else if (c == '\\') o << "\\\\";
            else if (c == '\b') o << "\\b";
            else if (c == '\f') o << "\\f";
            else if (c == '\n') o << "\\n";
            else if (c == '\r') o << "\\r";
            else if (c == '\t') o << "\\t";
            else if ('\x00' <= c && c <= '\x1f') {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                o << buf;
            } else {
                o << c;
            }
        }
        return o.str();
    }

    static std::string buildLoginResp(int code, const std::string& msg, const std::string& username, int avatar = 1) {
        std::ostringstream os;
        os << "{\"code\":" << code
           << ",\"msg\":\"" << escape(msg) << "\""
           << ",\"username\":\"" << escape(username) << "\""
           << ",\"avatar\":" << avatar << "}";
        return os.str();
    }

    static std::string buildChat(const std::string& sender, const std::string& receiver,
                                 const std::string& content, int avatar, uint64_t timestamp) {
        std::ostringstream os;
        os << "{\"sender\":\"" << escape(sender) << "\""
           << ",\"receiver\":\"" << escape(receiver) << "\""
           << ",\"content\":\"" << escape(content) << "\""
           << ",\"avatar\":" << avatar
           << ",\"timestamp\":" << timestamp << "}";
        return os.str();
    }

    static std::string buildUserList(const std::vector<UserInfo>& users) {
        std::ostringstream os;
        os << "{\"users\":[";
        for (size_t i = 0; i < users.size(); ++i) {
            if (i > 0) os << ",";
            os << "{\"name\":\"" << escape(users[i].username) << "\",\"avatar\":" << users[i].avatar_id << "}";
        }
        os << "]}";
        return os.str();
    }

    static std::string buildUserJoin(const std::string& username, int avatar) {
        std::ostringstream os;
        os << "{\"username\":\"" << escape(username) << "\",\"avatar\":" << avatar << "}";
        return os.str();
    }

    static std::string buildUserLeave(const std::string& username) {
        std::ostringstream os;
        os << "{\"username\":\"" << escape(username) << "\"}";
        return os.str();
    }

    static std::string buildHeartbeat(bool isPong) {
        return isPong ? "{\"type\":\"pong\"}" : "{\"type\":\"ping\"}";
    }

    static std::string getStr(const std::string& json, const std::string& key) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos + pattern.size());
        if (pos == std::string::npos) return "";

        // 跳过空白
        pos++;
        while (pos < json.size() && (std::isspace(json[pos]) || json[pos] == '"')) {
            if (json[pos] == '"') { pos++; break; }
            pos++;
        }

        std::string res;
        bool escapeNext = false;
        for (; pos < json.size(); ++pos) {
            char c = json[pos];
            if (escapeNext) {
                if (c == 'n') res += '\n';
                else if (c == 'r') res += '\r';
                else if (c == 't') res += '\t';
                else res += c;
                escapeNext = false;
            } else if (c == '\\') {
                escapeNext = true;
            } else if (c == '"') {
                break;
            } else {
                res += c;
            }
        }
        return res;
    }

    static int getInt(const std::string& json, const std::string& key, int defaultVal = 0) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return defaultVal;
        pos = json.find(':', pos + pattern.size());
        if (pos == std::string::npos) return defaultVal;

        pos++;
        while (pos < json.size() && (std::isspace(json[pos]) || json[pos] == '"')) pos++;

        size_t end = pos;
        while (end < json.size() && (std::isdigit(json[end]) || json[end] == '-')) end++;
        if (end == pos) return defaultVal;
        try {
            return std::stoi(json.substr(pos, end - pos));
        } catch (...) {
            return defaultVal;
        }
    }
};

} // namespace Chat

#endif // SIMPLE_JSON_H
