#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

/// <summary>
/// 와이어 포맷 및 전송 계층 상수.
/// 모든 TCP 메시지는
/// [magic(2)][version(1)][flags(1)][payload_length_be(4)][payload] 구조를 따른다.
/// </summary>
namespace NightNetwork::Protocol
{

constexpr uint16_t MAGIC = 0x4E4E;                ///< 'NN'
constexpr uint8_t VERSION = 1;
constexpr uint8_t FLAG_KEEPALIVE = 0x01;
constexpr uint8_t KNOWN_FLAGS_MASK = FLAG_KEEPALIVE;

constexpr std::size_t MAGIC_OFFSET = 0;
constexpr std::size_t VERSION_OFFSET = 2;
constexpr std::size_t FLAGS_OFFSET = 3;
constexpr std::size_t PAYLOAD_SIZE_OFFSET = 4;
constexpr std::size_t HEADER_SIZE = 8;
constexpr std::size_t MAX_PAYLOAD_SIZE = 2048;    ///< 단일 메시지 최대 페이로드 (바이트)

struct Header
{
    uint16_t magic;
    uint8_t version;
    uint8_t flags;
    uint32_t payload_size;

    [[nodiscard]] constexpr bool is_keepalive() const
    {
        return (flags & FLAG_KEEPALIVE) != 0;
    }
};

constexpr auto HEARTBEAT_INTERVAL = std::chrono::seconds(5);   ///< keepalive 전송 주기
constexpr auto HEARTBEAT_TIMEOUT  = std::chrono::seconds(15);  ///< 무응답 시 연결 종료 기한

} // namespace NightNetwork::Protocol
