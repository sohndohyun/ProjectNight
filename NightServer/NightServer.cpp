#include <NightNetwork/Server.h>

#include <chrono>
#include <iostream>
#include <print>
#include <thread>

int main()
{
    constexpr unsigned short port = 12345;

    auto server = NightNetwork::Server::create(port);
    if (!server)
    {
        std::println(stderr, "{}", server.error());
        return 1;
    }

    std::println("=== Game Server 시작 (포트: {}) ===", port);

    constexpr auto tick_rate = std::chrono::milliseconds(33);
    auto next_tick = std::chrono::steady_clock::now();

    while (true)
    {
        server->update();

        while (auto pkt = server->poll_packet())
        {
            switch (pkt->type)
            {
            case NightNetwork::PacketType::Connect:
                std::println("[접속] 클라이언트 #{}", pkt->session_id);
                break;

            case NightNetwork::PacketType::Disconnect:
                std::println("[종료] 클라이언트 #{}", pkt->session_id);
                break;

            case NightNetwork::PacketType::Data:
                std::println("[수신] 클라이언트 #{}: {}바이트",
                             pkt->session_id, pkt->data.size());
                server->send(pkt->session_id, pkt->data);
                break;
            }
        }

        next_tick += tick_rate;
        std::this_thread::sleep_until(next_tick);
    }

    return 0;
}
