#include <NightNetwork/Client.h>

#include "BufferPool.h"
#include "Protocol.h"
#include "TransportDetail.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <queue>
#include <thread>
#include <boost/asio.hpp>
#include <boost/lockfree/spsc_queue.hpp>

namespace NightNetwork
{

using boost::asio::ip::tcp;

struct Client::Impl
{
    boost::asio::io_context io;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard;
    tcp::socket socket;

    BufferPool pool{32};

    std::atomic<bool> connected = false;

    boost::lockfree::spsc_queue<std::vector<uint8_t>*,
        boost::lockfree::capacity<4096>> receive_queue;

    std::queue<std::vector<uint8_t>> write_queue;
    bool writing = false;

    uint8_t header_buf[Protocol::HEADER_SIZE];
    std::vector<uint8_t> body_buf;

    boost::asio::steady_timer heartbeat_timer;
    std::chrono::steady_clock::time_point last_activity;

    std::jthread io_thread;

    Impl()
        : work_guard(boost::asio::make_work_guard(io))
        , socket(io)
        , heartbeat_timer(io)
        , last_activity(std::chrono::steady_clock::now())
    {
        body_buf.reserve(Protocol::MAX_PAYLOAD_SIZE);
    }

    ~Impl()
    {
        std::vector<uint8_t>* ptr = nullptr;
        while (receive_queue.pop(ptr))
            delete ptr;
    }

    std::expected<void, std::string> connect(
        const std::string& host, unsigned short port)
    {
        tcp::resolver resolver(io);
        boost::system::error_code ec;

        auto endpoints = resolver.resolve(host, std::to_string(port), ec);
        if (ec)
            return std::unexpected("[에러] 주소 해석 실패: " + ec.message());

        boost::asio::connect(socket, endpoints, ec);
        if (ec)
            return std::unexpected("[에러] 서버 연결 실패: " + ec.message());

        connected.store(true, std::memory_order_relaxed);
        return {};
    }

    void start_io()
    {
        do_read_header();
        start_heartbeat();
        io_thread = std::jthread([this]()
        {
            io.run();
        });
    }

    void handle_disconnect()
    {
        if (!connected.exchange(false, std::memory_order_relaxed))
            return;

        heartbeat_timer.cancel();
        boost::system::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);
        socket.close(ec);
    }

    void do_read_header()
    {
        boost::asio::async_read(
            socket,
            boost::asio::buffer(header_buf, Protocol::HEADER_SIZE),
            [this](boost::system::error_code ec, std::size_t)
            {
                if (ec)
                {
                    handle_disconnect();
                    return;
                }

                auto header = Detail::decode_header(header_buf);
                if (!header)
                {
                    handle_disconnect();
                    return;
                }

                last_activity = std::chrono::steady_clock::now();

                if (header->is_keepalive())
                {
                    do_read_header();
                    return;
                }

                body_buf.resize(header->payload_size);
                do_read_body();
            });
    }

    void do_read_body()
    {
        boost::asio::async_read(
            socket,
            boost::asio::buffer(body_buf),
            [this](boost::system::error_code ec, std::size_t)
            {
                if (ec)
                {
                    handle_disconnect();
                    return;
                }

                {
                    auto copy = pool.acquire(body_buf.size());
                    std::memcpy(copy.data(), body_buf.data(), body_buf.size());
                    auto packet = new std::vector<uint8_t>(std::move(copy));
                    if (!receive_queue.push(packet))
                        delete packet;
                }
                do_read_header();
            });
    }

    void enqueue_frame(std::vector<uint8_t> frame)
    {
        if (!connected.load(std::memory_order_relaxed))
        {
            pool.release(std::move(frame));
            return;
        }

        Detail::enqueue_frame(write_queue, writing, std::move(frame),
            [this]()
            {
                do_write();
            });
    }

    void do_write()
    {
        if (write_queue.empty())
        {
            writing = false;
            return;
        }

        writing = true;
        auto& front = write_queue.front();
        boost::asio::async_write(
            socket,
            boost::asio::buffer(front),
            [this](boost::system::error_code ec, std::size_t)
            {
                pool.release(std::move(write_queue.front()));
                write_queue.pop();
                if (ec)
                {
                    handle_disconnect();
                    return;
                }
                do_write();
            });
    }

    void start_heartbeat()
    {
        heartbeat_timer.expires_after(Protocol::HEARTBEAT_INTERVAL);
        heartbeat_timer.async_wait(
            [this](boost::system::error_code ec)
            {
                if (ec)
                    return;

                auto elapsed = std::chrono::steady_clock::now() - last_activity;
                if (elapsed >= Protocol::HEARTBEAT_TIMEOUT)
                {
                    handle_disconnect();
                    return;
                }

                send_keepalive();
                start_heartbeat();
            });
    }

    void send_keepalive()
    {
        enqueue_frame(Detail::build_keepalive_frame(pool));
    }
};

std::expected<Client, std::string> Client::create(
    const std::string& host, unsigned short port)
{
    auto impl = std::make_unique<Impl>();

    auto result = impl->connect(host, port);
    if (!result)
        return std::unexpected(result.error());

    return Client(std::move(impl));
}

Client::Client(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
    impl_->start_io();
}

Client::~Client()
{
    if (impl_)
    {
        impl_->work_guard.reset();
        impl_->io.stop();
    }
}

Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;

void Client::update()
{
}

std::optional<std::vector<uint8_t>> Client::poll_packet()
{
    std::vector<uint8_t>* ptr = nullptr;
    if (!impl_->receive_queue.pop(ptr))
        return std::nullopt;

    auto data = std::move(*ptr);
    delete ptr;
    return data;
}

void Client::send(std::span<const uint8_t> data)
{
    if (data.empty() || data.size() > Protocol::MAX_PAYLOAD_SIZE)
        return;

    if (!impl_->connected.load(std::memory_order_relaxed))
        return;

    auto frame = Detail::build_data_frame(impl_->pool, data);
    if (frame.empty())
        return;

    boost::asio::post(impl_->io,
        [this, frame = std::move(frame)]() mutable
        {
            impl_->enqueue_frame(std::move(frame));
        });
}

bool Client::is_connected() const
{
    return impl_->connected.load(std::memory_order_relaxed);
}

} // namespace NightNetwork
