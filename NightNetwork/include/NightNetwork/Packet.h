#pragma once

#include <cstdint>
#include <vector>

namespace NightNetwork
{

enum class PacketType : uint8_t
{
    Connect,
    Disconnect,
    Data
};

struct Packet
{
    PacketType type;
    uint32_t session_id;
    std::vector<uint8_t> data;
};

} // namespace NightNetwork
