#pragma once

#include <memory>
#include <string>
#include <boost/asio.hpp>

namespace NightNetwork
{

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session>
{
public:
    explicit Session(tcp::socket socket);

    void start();

private:
    void do_read();
    void do_write(std::shared_ptr<std::string> data);

    tcp::socket socket_;
    char buf_[1024];
};

} // namespace NightNetwork
