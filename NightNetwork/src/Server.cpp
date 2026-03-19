#include <NightNetwork/Server.h>

#include "BufferPool.h"
#include "Session.h"
#include "TransportDetail.h"

#include <algorithm>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <vector>
#include <boost/asio.hpp>
#include <boost/lockfree/queue.hpp>

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

    boost::lockfree::queue<Packet*, boost::lockfree::capacity<8192>> packet_queue;

    std::vector<std::jthread> io_threads;

    Impl()
        : work_guard(boost::asio::make_work_guard(io))
        , server_strand(boost::asio::make_strand(io))
        , acceptor(server_strand)
    {
    }

    ~Impl()
    {
        Packet* ptr = nullptr;
        while (packet_queue.pop(ptr))
            delete ptr;
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

    bool push_packet(Packet pkt)
    {
        auto packet = new Packet(std::move(pkt));
        if (packet_queue.push(packet))
            return true;

        delete packet;
        return false;
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

                        if (push_packet(Packet{PacketType::Connect, id, {}}))
                        {
                            sessions.emplace(id, session);
                            session->start();
                        }
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
    Packet* ptr = nullptr;
    if (!impl_->packet_queue.pop(ptr))
        return std::nullopt;

    auto pkt = std::move(*ptr);
    delete ptr;
    return pkt;
}

void Server::send(uint32_t session_id, std::span<const uint8_t> data)
{
    if (data.size() > Protocol::MAX_PAYLOAD_SIZE)
        return;

    auto frame = Detail::build_frame(impl_->pool, data);

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

    auto frame_template = Detail::build_frame(impl_->pool, data);

    boost::asio::post(impl_->server_strand,
        [this, frame_template = std::move(frame_template)]() mutable
        {
            for (auto& [id, session] : impl_->sessions)
            {
                auto frame = Detail::clone_frame(impl_->pool, frame_template);
                session->enqueue_raw_frame(std::move(frame));
            }
            impl_->pool.release(std::move(frame_template));
        });
}

void Server::disconnect(uint32_t session_id)
{
    boost::asio::post(impl_->server_strand,
        [this, session_id]()
        {
            auto it = impl_->sessions.find(session_id);
            if (it != impl_->sessions.end())
                it->second->close();
        });
}

} // namespace NightNetwork
