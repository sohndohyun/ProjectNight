#pragma once

#include "Packet.h"

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace NightNetwork
{

class Server
{
public:
    static std::expected<Server, std::string> create(unsigned short port);

    ~Server();
    Server(Server&&) noexcept;
    Server& operator=(Server&&) noexcept;

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void update();
    std::vector<Packet> poll_packets(std::size_t max_count = 0);
    void send(uint32_t session_id, std::span<const uint8_t> data);
    void broadcast(std::span<const uint8_t> data);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    explicit Server(std::unique_ptr<Impl> impl);
};

} // namespace NightNetwork
