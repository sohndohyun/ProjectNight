#include <NightNetwork/Client.h>

#include "BufferPool.h"
#include "Protocol.h"
#include "TransportDetail.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <queue>
#include <thread>
#include <asio.hpp>
#include <boost/lockfree/spsc_queue.hpp>

namespace NightNetwork
{

using asio::ip::tcp;

namespace
{
constexpr std::size_t MAX_PENDING_WRITE_FRAMES = 1024;
}

struct Client::Impl
{
    asio::io_context io;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
    tcp::socket socket;

    BufferPool pool{32};

    std::atomic<bool> connected = false;
    std::atomic<bool> shutting_down = false;

    boost::lockfree::spsc_queue<std::vector<uint8_t>*,
        boost::lockfree::capacity<4096>> receive_queue;
    std::atomic_uint64_t dropped_packets = 0;

    std::queue<std::vector<uint8_t>> write_queue;
    bool writing = false;

    uint8_t header_buf[Protocol::HEADER_SIZE];
    std::vector<uint8_t> body_buf;

    asio::steady_timer heartbeat_timer;
    std::chrono::steady_clock::time_point last_activity;

    std::jthread io_thread;

    Impl()
        : work_guard(asio::make_work_guard(io))
        , socket(io)
        , heartbeat_timer(io)
        , last_activity(std::chrono::steady_clock::now())
    {
        body_buf.reserve(Protocol::MAX_PAYLOAD_SIZE);
    }

    ~Impl()
    {
        shutdown();

        std::vector<uint8_t>* ptr = nullptr;
        while (receive_queue.pop(ptr))
            delete ptr;
    }

    std::expected<void, std::string> connect(
        const std::string& host, unsigned short port)
    {
        tcp::resolver resolver(io);
        std::error_code ec;

        auto endpoints = resolver.resolve(host, std::to_string(port), ec);
        if (ec)
            return std::unexpected("[에러] 주소 해석 실패: " + ec.message());

        asio::connect(socket, endpoints, ec);
        if (ec)
            return std::unexpected("[에러] 서버 연결 실패: " + ec.message());

        socket.set_option(tcp::no_delay(true), ec);
        if (ec)
            return std::unexpected("[에러] TCP_NODELAY 설정 실패: " + ec.message());

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

        close_socket();
    }

    void close_socket()
    {
        heartbeat_timer.cancel();
        std::error_code ec;
        socket.shutdown(tcp::socket::shutdown_both, ec);
        socket.close(ec);
    }

    void shutdown()
    {
        if (shutting_down.exchange(true, std::memory_order_acq_rel))
            return;

        if (!io.stopped() && io_thread.joinable())
        {
            auto done = std::make_shared<std::promise<void>>();
            auto future = done->get_future();
            asio::post(io,
                [this, done]()
                {
                    connected.store(false, std::memory_order_relaxed);
                    close_socket();
                    done->set_value();
                });
            future.wait();
        }
        else
        {
            connected.store(false, std::memory_order_relaxed);
            close_socket();
        }

        work_guard.reset();
        io.stop();

        if (io_thread.joinable())
            io_thread.join();
    }

    void do_read_header()
    {
        asio::async_read(
            socket,
            asio::buffer(header_buf, Protocol::HEADER_SIZE),
            [this](std::error_code ec, std::size_t)
            {
                if (ec || shutting_down.load(std::memory_order_acquire))
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
        asio::async_read(
            socket,
            asio::buffer(body_buf),
            [this](std::error_code ec, std::size_t)
            {
                if (ec || shutting_down.load(std::memory_order_acquire))
                {
                    handle_disconnect();
                    return;
                }

                {
                    auto copy = pool.acquire(body_buf.size());
                    std::memcpy(copy.data(), body_buf.data(), body_buf.size());
                    auto packet = new std::vector<uint8_t>(std::move(copy));
                    if (shutting_down.load(std::memory_order_acquire))
                    {
                        delete packet;
                        return;
                    }

                    if (!receive_queue.push(packet))
                    {
                        delete packet;
                        dropped_packets.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                do_read_header();
            });
    }

    void enqueue_frame(std::vector<uint8_t> frame)
    {
        if (shutting_down.load(std::memory_order_acquire)
            || !connected.load(std::memory_order_relaxed))
        {
            pool.release(std::move(frame));
            return;
        }

        if (!Detail::enqueue_frame(write_queue, writing, std::move(frame),
            [this]()
            {
                do_write();
            },
            MAX_PENDING_WRITE_FRAMES))
        {
            pool.release(std::move(frame));
            handle_disconnect();
        }
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
        asio::async_write(
            socket,
            asio::buffer(front),
            [this](std::error_code ec, std::size_t)
            {
                pool.release(std::move(write_queue.front()));
                write_queue.pop();
                if (ec || shutting_down.load(std::memory_order_acquire))
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
            [this](std::error_code ec)
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
        impl_->shutdown();
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

uint64_t Client::dropped_packet_count() const
{
    return impl_->dropped_packets.load(std::memory_order_relaxed);
}

void Client::send(std::span<const uint8_t> data)
{
    if (data.empty() || data.size() > Protocol::MAX_PAYLOAD_SIZE)
        return;

    if (!impl_->connected.load(std::memory_order_relaxed))
        return;

    if (impl_->shutting_down.load(std::memory_order_acquire))
        return;

    auto frame = Detail::build_data_frame(impl_->pool, data);
    if (frame.empty())
        return;

    asio::post(impl_->io,
        [this, frame = std::move(frame)]() mutable
        {
            if (impl_->shutting_down.load(std::memory_order_acquire))
            {
                impl_->pool.release(std::move(frame));
                return;
            }

            impl_->enqueue_frame(std::move(frame));
        });
}

bool Client::is_connected() const
{
    return impl_->connected.load(std::memory_order_relaxed);
}

} // namespace NightNetwork
