#include <NightNetwork/Client.h>

#include "BufferPool.h"
#include "Protocol.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>
#include <boost/asio.hpp>

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

    std::mutex receive_mutex;
    std::queue<std::vector<uint8_t>> receive_queue;

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
        heartbeat_timer.cancel();
        connected.store(false, std::memory_order_relaxed);
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

                last_activity = std::chrono::steady_clock::now();

                uint32_t body_size = 0;
                std::memcpy(&body_size, header_buf, Protocol::HEADER_SIZE);

                if (body_size == 0)
                {
                    do_read_header();
                    return;
                }

                if (body_size > Protocol::MAX_PAYLOAD_SIZE)
                {
                    handle_disconnect();
                    return;
                }

                body_buf.resize(body_size);
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
                    std::lock_guard lock(receive_mutex);
                    receive_queue.push(std::move(copy));
                }
                do_read_header();
            });
    }

    void enqueue_send(std::span<const uint8_t> data)
    {
        if (data.size() > Protocol::MAX_PAYLOAD_SIZE)
            return;

        uint32_t size = static_cast<uint32_t>(data.size());
        auto frame = pool.acquire(Protocol::HEADER_SIZE + size);
        std::memcpy(frame.data(), &size, Protocol::HEADER_SIZE);
        std::memcpy(frame.data() + Protocol::HEADER_SIZE, data.data(), size);

        write_queue.push(std::move(frame));
        if (!writing)
            do_write();
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
        uint32_t zero = 0;
        std::vector<uint8_t> frame(Protocol::HEADER_SIZE);
        std::memcpy(frame.data(), &zero, Protocol::HEADER_SIZE);
        write_queue.push(std::move(frame));
        if (!writing)
            do_write();
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
    std::lock_guard lock(impl_->receive_mutex);
    if (impl_->receive_queue.empty())
        return std::nullopt;

    auto data = std::move(impl_->receive_queue.front());
    impl_->receive_queue.pop();
    return data;
}

void Client::send(std::span<const uint8_t> data)
{
    if (data.size() > Protocol::MAX_PAYLOAD_SIZE)
        return;

    uint32_t size = static_cast<uint32_t>(data.size());
    auto frame = impl_->pool.acquire(Protocol::HEADER_SIZE + size);
    std::memcpy(frame.data(), &size, Protocol::HEADER_SIZE);
    std::memcpy(frame.data() + Protocol::HEADER_SIZE, data.data(), size);

    boost::asio::post(impl_->io,
        [this, frame = std::move(frame)]() mutable
        {
            impl_->write_queue.push(std::move(frame));
            if (!impl_->writing)
                impl_->do_write();
        });
}

bool Client::is_connected() const
{
    return impl_->connected.load(std::memory_order_relaxed);
}

} // namespace NightNetwork
