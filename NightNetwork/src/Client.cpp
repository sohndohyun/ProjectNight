#include <NightNetwork/Client.h>

#include "Protocol.h"

#include <cstring>
#include <queue>
#include <boost/asio.hpp>

namespace NightNetwork
{

using boost::asio::ip::tcp;

struct Client::Impl
{
    boost::asio::io_context io;
    tcp::socket socket;
    bool connected = false;

    std::queue<std::vector<uint8_t>> receive_queue;
    std::queue<std::vector<uint8_t>> write_queue;
    bool writing = false;

    uint8_t header_buf[Protocol::HEADER_SIZE];
    std::vector<uint8_t> body_buf;

    Impl()
        : socket(io)
    {
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

        connected = true;
        return {};
    }

    void start_read()
    {
        do_read_header();
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
                    connected = false;
                    return;
                }

                uint32_t body_size = 0;
                std::memcpy(&body_size, header_buf, Protocol::HEADER_SIZE);

                if (body_size == 0 || body_size > Protocol::MAX_PAYLOAD_SIZE)
                {
                    connected = false;
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
                    connected = false;
                    return;
                }

                receive_queue.push(std::move(body_buf));
                do_read_header();
            });
    }

    void enqueue_send(std::span<const uint8_t> data)
    {
        if (data.size() > Protocol::MAX_PAYLOAD_SIZE)
            return;

        uint32_t size = static_cast<uint32_t>(data.size());
        std::vector<uint8_t> frame(Protocol::HEADER_SIZE + size);
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
                write_queue.pop();
                if (ec)
                {
                    connected = false;
                    return;
                }
                do_write();
            });
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
    impl_->start_read();
}

Client::~Client() = default;
Client::Client(Client&&) noexcept = default;
Client& Client::operator=(Client&&) noexcept = default;

void Client::update()
{
    impl_->io.poll();
    impl_->io.restart();
}

std::vector<std::vector<uint8_t>> Client::poll_packets(std::size_t max_count)
{
    std::vector<std::vector<uint8_t>> result;
    auto& q = impl_->receive_queue;

    std::size_t count = q.size();
    if (max_count > 0 && count > max_count)
        count = max_count;

    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        result.push_back(std::move(q.front()));
        q.pop();
    }
    return result;
}

void Client::send(std::span<const uint8_t> data)
{
    impl_->enqueue_send(data);
}

bool Client::is_connected() const
{
    return impl_->connected;
}

} // namespace NightNetwork
