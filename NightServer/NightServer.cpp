#include "GameLogicSystem.h"
#include "NetworkSystem.h"
#include "RoomSystem.h"

#include <chrono>
#include <print>
#include <thread>

int main()
{
    constexpr unsigned short port = 12345;

    auto created_server = NightNetwork::Server::create(port);
    if (!created_server)
    {
        std::println(stderr, "{}", created_server.error());
        return 1;
    }

    NightServer::NetworkSystem network_system(std::move(created_server.value()));
    NightServer::RoomSystem room_system;
    NightServer::GameLogicSystem game_logic_system(room_system);

    std::println("=== Game Server 시작 (포트: {}) ===", port);

    constexpr auto tick_rate = std::chrono::milliseconds(33);
    auto next_tick = std::chrono::steady_clock::now();

    while (true)
    {
        auto events = network_system.Receive();
        game_logic_system.Update(events);
        network_system.Flush(
            game_logic_system.TakeOutgoingMessages(),
            game_logic_system.TakePendingDisconnects());

        next_tick += tick_rate;
        std::this_thread::sleep_until(next_tick);
    }

    return 0;
}
