#include <NightNetwork/Server.h>

#include "Session.h"

#include <queue>
#include <unordered_map>
#include <boost/asio.hpp>

namespace NightNetwork
{

using boost::asio::ip::tcp;

struct Server::Impl
{
    boost::asio::io_context io;
    tcp::acceptor acceptor;
    uint32_t next_id = 1;
    std::unordered_map<uint32_t, std::shared_ptr<Session>> sessions;
    std::queue<Packet> packet_queue;

    Impl()
        : acceptor(io)
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

    void do_accept()
    {
        acceptor.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    uint32_t id = next_id++;
                    auto session = std::make_shared<Session>(
                        id, std::move(socket),
                        [this](uint32_t sid, std::vector<uint8_t> data)
                        {
                            packet_queue.push(Packet{
                                PacketType::Data, sid, std::move(data)});
                        },
                        [this](uint32_t sid)
                        {
                            packet_queue.push(Packet{
                                PacketType::Disconnect, sid, {}});
                            sessions.erase(sid);
                        });

                    sessions.emplace(id, session);
                    packet_queue.push(Packet{PacketType::Connect, id, {}});
                    session->start();
                }

                do_accept();
            });
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
}

Server::~Server() = default;
Server::Server(Server&&) noexcept = default;
Server& Server::operator=(Server&&) noexcept = default;

void Server::update()
{
    impl_->io.poll();
    impl_->io.restart();
}

std::vector<Packet> Server::poll_packets(std::size_t max_count)
{
    std::vector<Packet> result;
    auto& q = impl_->packet_queue;

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

void Server::send(uint32_t session_id, std::span<const uint8_t> data)
{
    auto it = impl_->sessions.find(session_id);
    if (it != impl_->sessions.end())
        it->second->send(std::vector<uint8_t>(data.begin(), data.end()));
}

void Server::broadcast(std::span<const uint8_t> data)
{
    auto payload = std::vector<uint8_t>(data.begin(), data.end());
    for (auto& [id, session] : impl_->sessions)
        session->send(payload);
}

} // namespace NightNetwork
