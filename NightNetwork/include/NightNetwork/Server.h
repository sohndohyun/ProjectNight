#pragma once

#include "Packet.h"

#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace NightNetwork
{

/// <summary>
/// TCP 게임 서버. 바이트 운반만 담당하며 게임 로직을 모른다.
///
/// 내부적으로 멀티스레드 I/O(hardware_concurrency 개수)를 운용하지만,
/// public API는 모두 게임 스레드 하나에서 호출하는 것을 전제로 설계되었다.
/// pimpl 패턴으로 Boost.Asio 의존성이 public 헤더에 노출되지 않는다.
/// </summary>
class Server
{
public:
    /// <summary>
    /// 지정 포트에서 리슨하는 서버를 생성한다.
    /// 생성 즉시 accept 루프와 I/O 스레드가 시작된다.
    /// </summary>
    /// <returns>실패 시 에러 메시지를 담은 unexpected</returns>
    static std::expected<Server, std::string> create(unsigned short port);

    ~Server();
    Server(Server&&) noexcept;
    Server& operator=(Server&&) noexcept;

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    /// <summary>
    /// 게임 루프에서 매 틱마다 호출. 현재는 예약용이며, 향후 확장을 위해 존재한다.
    /// </summary>
    void update();

    /// <summary>
    /// 수신 큐에서 패킷을 하나 꺼낸다. 큐가 비었으면 nullopt을 반환한다.
    /// Connect/Disconnect 이벤트도 이 경로로 전달되므로, 루프에서
    /// nullopt이 나올 때까지 반복 호출해야 한다.
    /// </summary>
    std::optional<Packet> poll_packet();

    /// <summary>
    /// 특정 세션에 페이로드를 전송한다. 내부에서 length-prefix 프레임을 빌드한다.
    /// Protocol::MAX_PAYLOAD_SIZE(2048)를 초과하면 무시된다.
    /// 세션이 이미 종료된 경우에도 안전하다(조용히 버려진다).
    /// I/O 스레드로 post 되므로 게임 스레드에서 자유롭게 호출 가능하다.
    /// </summary>
    void send(uint32_t session_id, std::span<const uint8_t> data);

    /// <summary>
    /// 현재 연결된 모든 세션에 동일한 페이로드를 전송한다.
    /// 프레임 템플릿을 1회 빌드한 뒤 세션별로 풀에서 복사하여 전달한다.
    /// MAX_PAYLOAD_SIZE 초과 시 무시된다.
    /// </summary>
    void broadcast(std::span<const uint8_t> data);

    /// <summary>
    /// 특정 세션을 강제 종료한다. 소켓을 graceful shutdown한 뒤
    /// Disconnect 이벤트가 poll_packet()으로 전달된다.
    /// 이미 없는 session_id에 대해서는 안전하게 무시된다.
    /// I/O 스레드로 post 되므로 게임 스레드에서 자유롭게 호출 가능하다.
    /// </summary>
    void disconnect(uint32_t session_id);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    explicit Server(std::unique_ptr<Impl> impl);
};

} // namespace NightNetwork
