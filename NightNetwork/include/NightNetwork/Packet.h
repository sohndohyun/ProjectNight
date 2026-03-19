#pragma once

#include <cstdint>
#include <vector>

namespace NightNetwork
{

/// <summary>
/// 패킷의 종류. Connect/Disconnect는 와이어를 타지 않는 내부 이벤트이며,
/// Data만 실제 TCP로 송수신된다.
/// </summary>
enum class PacketType : uint8_t
{
    Connect,    ///< 새 클라이언트가 접속했음을 알리는 이벤트
    Disconnect, ///< 클라이언트 연결이 끊어졌음을 알리는 이벤트
    Data        ///< 실제 페이로드를 담은 데이터 패킷
};

/// <summary>
/// I/O 스레드에서 게임 스레드로 전달되는 패킷.
/// poll_packet()으로 하나씩 꺼내 소비한다.
/// </summary>
struct Packet
{
    PacketType type;
    uint32_t session_id;         ///< 패킷을 보낸(또는 관련된) 세션 ID
    std::vector<uint8_t> data;   ///< Data 타입일 때만 유효한 페이로드
};

} // namespace NightNetwork
