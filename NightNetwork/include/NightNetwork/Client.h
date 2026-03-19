#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace NightNetwork
{

/// <summary>
/// TCP 게임 클라이언트. 서버와 1:1 연결을 유지하며 바이트 송수신만 담당한다.
///
/// 단일 I/O 스레드가 소켓 읽기/쓰기를 처리하고,
/// public API는 게임 스레드 하나에서 호출하는 것을 전제로 설계되었다.
/// pimpl 패턴으로 Boost.Asio 의존성이 public 헤더에 노출되지 않는다.
/// </summary>
class Client
{
public:
    /// <summary>
    /// 서버에 동기적으로 연결한 뒤 I/O 스레드를 시작한다.
    /// </summary>
    /// <returns>실패 시(주소 해석·연결 거부 등) 에러 메시지를 담은 unexpected</returns>
    static std::expected<Client, std::string> create(
        const std::string& host, unsigned short port);

    ~Client();
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    /// <summary>
    /// 게임 루프에서 매 틱마다 호출. 현재는 예약용이며, 향후 확장을 위해 존재한다.
    /// </summary>
    void update();

    /// <summary>
    /// 수신 큐에서 페이로드를 하나 꺼낸다 (wire 헤더는 제거된 순수 데이터).
    /// 큐가 비었으면 nullopt을 반환한다. 루프에서 nullopt까지 반복 호출해야 한다.
    /// </summary>
    std::optional<std::vector<uint8_t>> poll_packet();

    /// <summary>
    /// 서버에 페이로드를 전송한다. I/O 스레드로 post 되므로 게임 스레드에서 안전하다.
    /// Protocol::MAX_PAYLOAD_SIZE(2048)를 초과하면 무시된다.
    /// </summary>
    void send(std::span<const uint8_t> data);

    /// <summary>
    /// 서버와의 연결 상태를 반환한다.
    /// atomic으로 관리되므로, I/O 스레드의 단절 감지와 약간의 시차가 있을 수 있다.
    /// </summary>
    bool is_connected() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    explicit Client(std::unique_ptr<Impl> impl);
};

} // namespace NightNetwork
