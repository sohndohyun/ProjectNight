#include <NightNetwork/Server.h>

#include "BufferPool.h"
#include "Session.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>
#include <boost/asio.hpp>

namespace NightNetwork
{

using boost::asio::ip::tcp;

struct Server::Impl
{
    boost::asio::io_context io;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard;
    boost::asio::strand<boost::asio::io_context::executor_type> server_strand;
    tcp::acceptor acceptor;

    BufferPool pool;

    uint32_t next_id = 1;
    std::unordered_map<uint32_t, std::shared_ptr<Session>> sessions;

    std::mutex packet_mutex;
    std::queue<Packet> packet_queue;

    std::vector<std::jthread> io_threads;

    Impl()
        : work_guard(boost::asio::make_work_guard(io))
        , server_strand(boost::asio::make_strand(io))
        , acceptor(server_strand)
    {
    }

    std::expected<void, std::string> listen(unsigned short port)
    {
        boost::system::error_code ec;

        acceptor.open(tcp::v4(), ec);
        if (ec)
            return std::unexpected("[에러] 소켓 열기 실패: " + ec.message());

        acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
        if (ec)
            return std::unexpected("[에러] 옵션 설정 실패: " + ec.message());

        acceptor.bind(tcp::endpoint(tcp::v4(), port), ec);
        if (ec)
            return std::unexpected("[에러] 바인드 실패: " + ec.message());

        acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
            return std::unexpected("[에러] 리슨 실패: " + ec.message());

        return {};
    }

    void push_packet(Packet pkt)
    {
        std::lock_guard lock(packet_mutex);
        packet_queue.push(std::move(pkt));
    }

    void start_threads()
    {
        auto count = std::max(1u, std::thread::hardware_concurrency());
        io_threads.reserve(count);
        for (unsigned i = 0; i < count; ++i)
        {
            io_threads.emplace_back([this]()
            {
                io.run();
            });
        }
    }

    void do_accept()
    {
        acceptor.async_accept(
            boost::asio::bind_executor(server_strand,
                [this](boost::system::error_code ec, tcp::socket socket)
                {
                    if (!ec)
                    {
                        uint32_t id = next_id++;
                        auto session = std::make_shared<Session>(
                            id, std::move(socket), pool,
                            [this](uint32_t sid, std::vector<uint8_t> data)
                            {
                                push_packet(Packet{
                                    PacketType::Data, sid, std::move(data)});
                            },
                            [this](uint32_t sid)
                            {
                                push_packet(Packet{
                                    PacketType::Disconnect, sid, {}});
                                boost::asio::post(server_strand,
                                    [this, sid]()
                                    {
                                        sessions.erase(sid);
                                    });
                            });

                        sessions.emplace(id, session);
                        push_packet(Packet{PacketType::Connect, id, {}});
                        session->start();
                    }

                    do_accept();
                }));
    }
};

std::expected<Server, std::string> Server::create(unsigned short port)
{
    auto impl = std::make_unique<Impl>();

    auto result = impl->listen(port);
    if (!result)
        return std::unexpected(result.error());

    return Server(std::move(impl));
}

Server::Server(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
    impl_->do_accept();
    impl_->start_threads();
}

Server::~Server()
{
    if (impl_)
    {
        impl_->work_guard.reset();
        impl_->io.stop();
    }
}

Server::Server(Server&&) noexcept = default;
Server& Server::operator=(Server&&) noexcept = default;

void Server::update()
{
}

std::optional<Packet> Server::poll_packet()
{
    std::lock_guard lock(impl_->packet_mutex);
    if (impl_->packet_queue.empty())
        return std::nullopt;

    auto pkt = std::move(impl_->packet_queue.front());
    impl_->packet_queue.pop();
    return pkt;
}

void Server::send(uint32_t session_id, std::span<const uint8_t> data)
{
    if (data.size() > Protocol::MAX_PAYLOAD_SIZE)
        return;

    uint32_t size = static_cast<uint32_t>(data.size());
    auto frame = impl_->pool.acquire(Protocol::HEADER_SIZE + size);
    std::memcpy(frame.data(), &size, Protocol::HEADER_SIZE);
    std::memcpy(frame.data() + Protocol::HEADER_SIZE, data.data(), size);

    boost::asio::post(impl_->server_strand,
        [this, session_id, frame = std::move(frame)]() mutable
        {
            auto it = impl_->sessions.find(session_id);
            if (it != impl_->sessions.end())
                it->second->enqueue_raw_frame(std::move(frame));
            else
                impl_->pool.release(std::move(frame));
        });
}

void Server::broadcast(std::span<const uint8_t> data)
{
    if (data.size() > Protocol::MAX_PAYLOAD_SIZE)
        return;

    uint32_t size = static_cast<uint32_t>(data.size());
    auto frame_template = impl_->pool.acquire(Protocol::HEADER_SIZE + size);
    std::memcpy(frame_template.data(), &size, Protocol::HEADER_SIZE);
    std::memcpy(frame_template.data() + Protocol::HEADER_SIZE, data.data(), size);

    boost::asio::post(impl_->server_strand,
        [this, frame_template = std::move(frame_template)]() mutable
        {
            for (auto& [id, session] : impl_->sessions)
            {
                auto frame = impl_->pool.acquire(frame_template.size());
                std::memcpy(frame.data(), frame_template.data(), frame_template.size());
                session->enqueue_raw_frame(std::move(frame));
            }
            impl_->pool.release(std::move(frame_template));
        });
}

} // namespace NightNetwork
