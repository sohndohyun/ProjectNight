#include "Server.h"
#include "Session.h"

namespace NightNetwork
{

std::expected<Server, std::string> Server::create(boost::asio::io_context& io, unsigned short port)
{
    boost::system::error_code ec;
    tcp::acceptor acceptor(io);

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

    return Server(std::move(acceptor));
}

void Server::start()
{
    do_accept();
}

Server::Server(tcp::acceptor acceptor)
    : acceptor_(std::move(acceptor))
{
}

void Server::do_accept()
{
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
            if (ec)
                std::cerr << "[에러] 접속 수락 실패: " << ec.message() << std::endl;
            else
                std::make_shared<Session>(std::move(socket))->start();

            do_accept();
        });
}

} // namespace NightNetwork
