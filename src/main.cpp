#include <iostream>
#include <csignal>
#include <cstdlib>
#include "chatServer.h"

static Chat::chatServer* g_server = nullptr;

void signalHandler(int sig) {
    std::cout << "\n[Server] Caught signal " << sig << ", shutting down gracefully..." << std::endl;
    if (g_server) {
        g_server->stopServer();
    }
}

int main(int argc, char const *argv[])
{
    std::string ip = "0.0.0.0";
    int port = 8888;

    if (argc >= 3) {
        ip = argv[1];
        port = std::atoi(argv[2]);
    } else if (argc == 2) {
        port = std::atoi(argv[1]);
    } else {
        std::cout << "Usage: " << argv[0] << " [ip] <port>" << std::endl;
        std::cout << "Defaulting to 0.0.0.0:8888" << std::endl;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    Chat::chatServer server(ip.c_str(), port, 4);
    g_server = &server;

    server.run();

    g_server = nullptr;
    std::cout << "[Server] Stopped cleanly." << std::endl;
    return 0;
}
