#include "Session.h"

namespace NightNetwork
{

Session::Session(tcp::socket socket)
    : socket_(std::move(socket))
{
}

void Session::start()
{
    std::cout << "[접속] " << socket_.remote_endpoint() << std::endl;
    do_read();
}

void Session::do_read()
{
    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(buf_),
        [this, self](boost::system::error_code ec, std::size_t len)
        {
            if (ec)
            {
                if (ec == boost::asio::error::eof)
                    std::cout << "[종료] 클라이언트 연결 종료" << std::endl;
                else
                    std::cerr << "[에러] 수신 실패: " << ec.message() << std::endl;
                return;
            }

            auto data = std::make_shared<std::string>(buf_, len);
            std::cout << "[수신] " << *data;

            do_write(data);
        });
}

void Session::do_write(std::shared_ptr<std::string> data)
{
    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(*data),
        [this, self, data](boost::system::error_code ec, std::size_t)
        {
            if (ec)
            {
                std::cerr << "[에러] 송신 실패: " << ec.message() << std::endl;
                return;
            }

            std::cout << "[송신] " << *data;
            do_read();
        });
}

} // namespace NightNetwork
