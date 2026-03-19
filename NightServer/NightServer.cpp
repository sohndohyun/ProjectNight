// NightServer.cpp : 애플리케이션의 진입점을 정의합니다.
//

#include "NightNetwork/Server.h"

#include <iostream>

int main()
{
    const unsigned short port = 12345;

    boost::asio::io_context io;

    auto server = NightNetwork::Server::create(io, port);
    if (!server)
    {
        std::cerr << server.error() << std::endl;
        return 1;
    }

    std::cout << "=== Echo Server 시작 (포트: " << port << ") ===" << std::endl;

    server->start();
    io.run();

    return 0;
}
