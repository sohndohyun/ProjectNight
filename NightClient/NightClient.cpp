#include <NightNetwork/Client.h>

#include <chrono>
#include <iostream>
#include <print>
#include <string>
#include <thread>

int main()
{
    const std::string host = "localhost";
    const unsigned short port = 12345;

    auto client = NightNetwork::Client::create(host, port);
    if (!client)
    {
        std::cerr << client.error() << std::endl;
        return 1;
    }

    std::println("=== Client 시작 (서버: {}:{}) ===", host, port);
    std::println("메시지를 입력하세요 (종료: Ctrl+Z):");

    std::thread input_thread([&client]()
    {
        std::string line;
        while (std::getline(std::cin, line))
        {
            line += "\n";
            auto bytes = std::vector<uint8_t>(line.begin(), line.end());
            client->send(bytes);
        }
    });
    input_thread.detach();

    const auto tick_rate = std::chrono::milliseconds(33);
    auto next_tick = std::chrono::steady_clock::now();

    while (client->is_connected())
    {
        client->update();

        while (auto data = client->poll_packet())
        {
            std::print("[에코] {}",
                       std::string(data->begin(), data->end()));
        }

        next_tick += tick_rate;
        std::this_thread::sleep_until(next_tick);
    }

    std::println("=== Client 종료 ===");
    return 0;
}
