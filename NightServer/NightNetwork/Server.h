#pragma once

#include "../NightServer.h"

namespace NightNetwork
{

class Server
{
public:
    static std::expected<Server, std::string> create(boost::asio::io_context& io, unsigned short port);

    void start();

private:
    explicit Server(tcp::acceptor acceptor);

    void do_accept();

    tcp::acceptor acceptor_;
};

} // namespace NightNetwork
