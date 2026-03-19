#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

/// <summary>
/// 와이어 포맷 및 전송 계층 상수.
/// 모든 TCP 메시지는 [uint32_t payload_size][payload] 구조를 따른다.
/// payload_size == 0인 프레임은 keepalive 신호로 예약되어 있다.
/// </summary>
namespace NightNetwork::Protocol
{

constexpr std::size_t HEADER_SIZE = sizeof(uint32_t);   ///< length-prefix 헤더 크기 (4바이트)
constexpr std::size_t MAX_PAYLOAD_SIZE = 2048;           ///< 단일 메시지 최대 페이로드 (바이트)

constexpr auto HEARTBEAT_INTERVAL = std::chrono::seconds(5);   ///< keepalive 전송 주기
constexpr auto HEARTBEAT_TIMEOUT  = std::chrono::seconds(15);  ///< 무응답 시 연결 종료 기한

} // namespace NightNetwork::Protocol
