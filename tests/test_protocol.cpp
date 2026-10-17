#include <iostream>
#include <cassert>
#include "header/Protocol.h"
#include "header/SimpleJson.h"

int main() {
    std::cout << "[TEST] Running Protocol and SimpleJson tests..." << std::endl;

    // Test 1: JSON builder
    std::string chatJson = Chat::SimpleJson::buildChat("Alice", "ALL", "Hello World!", 1, 123456789);
    assert(!chatJson.empty());
    assert(Chat::SimpleJson::getStr(chatJson, "sender") == "Alice");
    assert(Chat::SimpleJson::getStr(chatJson, "receiver") == "ALL");
    assert(Chat::SimpleJson::getStr(chatJson, "content") == "Hello World!");
    assert(Chat::SimpleJson::getInt(chatJson, "avatar") == 1);

    // Test 2: User list
    std::vector<Chat::UserInfo> users = {{"Alice", 1}, {"Bob", 2}};
    std::string listJson = Chat::SimpleJson::buildUserList(users);
    assert(listJson.find("Alice") != std::string::npos);
    assert(listJson.find("Bob") != std::string::npos);

    // Test 3: Header size
    assert(sizeof(Chat::MsgHeader) == 16);

    std::cout << "[TEST] All Protocol & SimpleJson tests passed successfully!" << std::endl;
    return 0;
}
