#pragma once

#include <cstddef>
#include <cstdint>

namespace NightNetwork::Protocol
{

constexpr std::size_t HEADER_SIZE = sizeof(uint32_t);
constexpr std::size_t MAX_PAYLOAD_SIZE = 2048;

} // namespace NightNetwork::Protocol
