#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace NightNetwork
{

class Client
{
public:
    static std::expected<Client, std::string> create(
        const std::string& host, unsigned short port);

    ~Client();
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void update();
    std::vector<std::vector<uint8_t>> poll_packets(std::size_t max_count = 0);
    void send(std::span<const uint8_t> data);
    bool is_connected() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    explicit Client(std::unique_ptr<Impl> impl);
};

} // namespace NightNetwork
