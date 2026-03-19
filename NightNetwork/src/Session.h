#pragma once

#include "BufferPool.h"
#include "Protocol.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <span>
#include <vector>
#include <boost/asio.hpp>

namespace NightNetwork
{

using boost::asio::ip::tcp;

using ReceiveCallback = std::function<void(uint32_t, std::vector<uint8_t>)>;  ///< (session_id, payload) 수신 콜백
using CloseCallback = std::function<void(uint32_t)>;                          ///< (session_id) 연결 종료 콜백

/// <summary>
/// 개별 TCP 연결을 관리하는 내부 클래스. public 헤더에 노출되지 않는다.
///
/// 자체 strand를 가지며, 소켓 I/O와 send/close 호출이 strand 내에서 직렬화된다.
/// shared_from_this()로 비동기 콜백 체인 동안 수명을 유지한다.
/// </summary>
class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(uint32_t id, tcp::socket socket, BufferPool& pool,
            ReceiveCallback on_receive, CloseCallback on_close);

    /// <summary>
    /// 비동기 읽기 체인(header -> body -> header -> ...)을 시작한다.
    /// </summary>
    void start();

    /// <summary>
    /// 페이로드를 length-prefix 프레임으로 감싸 strand에 post한다.
    /// MAX_PAYLOAD_SIZE 초과 시 무시된다.
    /// </summary>
    void send(std::span<const uint8_t> payload);

    /// <summary>
    /// 이미 빌드된 프레임을 strand에 post한다.
    /// Server::send()가 프레임을 직접 빌드할 때 사용하여 이중 할당을 방지한다.
    /// </summary>
    void enqueue_raw_frame(std::vector<uint8_t> frame);

    /// <summary>
    /// 소켓을 graceful shutdown한다. strand에 post되어 안전하다.
    /// </summary>
    void close();

    uint32_t id() const { return id_; }

private:
    void enqueue_write(std::vector<uint8_t> frame);
    void do_read_header();
    void do_read_body();
    void do_write();

    uint32_t id_;
    tcp::socket socket_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;
    BufferPool& pool_;
    ReceiveCallback on_receive_;
    CloseCallback on_close_;

    uint8_t header_buf_[Protocol::HEADER_SIZE];
    std::vector<uint8_t> body_buf_;

    std::queue<std::vector<uint8_t>> write_queue_;
    bool writing_ = false;
};

} // namespace NightNetwork
