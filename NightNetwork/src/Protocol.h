#pragma once

#include <cstddef>
#include <cstdint>

/// <summary>
/// 와이어 포맷 상수. 모든 TCP 메시지는 [uint32_t payload_size][payload] 구조를 따른다.
/// </summary>
namespace NightNetwork::Protocol
{

constexpr std::size_t HEADER_SIZE = sizeof(uint32_t);   ///< length-prefix 헤더 크기 (4바이트)
constexpr std::size_t MAX_PAYLOAD_SIZE = 2048;           ///< 단일 메시지 최대 페이로드 (바이트)

} // namespace NightNetwork::Protocol
