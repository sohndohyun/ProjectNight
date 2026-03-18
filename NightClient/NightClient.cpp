// NightClient.cpp : 애플리케이션의 진입점을 정의합니다.
//

#include "NightClient.h"

using namespace std;

// 서버에 연결하는 함수 (1회성이므로 동기 처리, error_code 사용)
std::expected<tcp::socket, std::string> connect_to_server(
    boost::asio::io_context& io,
    const std::string& host,
    unsigned short port)
{
    tcp::resolver resolver(io);
    boost::system::error_code ec;

    auto endpoints = resolver.resolve(host, std::to_string(port), ec);
    if (ec)
        return std::unexpected("[에러] 주소 해석 실패: " + ec.message());

    tcp::socket socket(io);
    boost::asio::connect(socket, endpoints, ec);
    if (ec)
        return std::unexpected("[에러] 서버 연결 실패: " + ec.message());

    return socket;
}

// 서버와의 비동기 읽기/쓰기를 담당하는 세션
// stdin은 별도 스레드에서 읽고, 네트워크 I/O는 io_context에서 비동기 처리
class Session : public std::enable_shared_from_this<Session>
{
public:
    explicit Session(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        do_read();

        std::thread([self = shared_from_this()]()
        {
            std::string line;
            while (std::getline(std::cin, line))
            {
                line += "\n";
                auto data = std::make_shared<std::string>(std::move(line));
                boost::asio::post(self->socket_.get_executor(),
                    [self, data]() { self->do_write(data); });
            }
            boost::asio::post(self->socket_.get_executor(),
                [self]() { self->socket_.close(); });
        }).detach();
    }

private:
    void do_read()
    {
        auto self = shared_from_this();
        socket_.async_read_some(
            boost::asio::buffer(buf_),
            [this, self](boost::system::error_code ec, std::size_t len)
            {
                if (ec)
                {
                    if (ec == boost::asio::error::eof)
                        std::cout << "[종료] 서버 연결 종료" << std::endl;
                    else if (ec != boost::asio::error::operation_aborted)
                        std::cerr << "[에러] 수신 실패: " << ec.message() << std::endl;
                    return;
                }

                std::cout << "[에코] " << std::string(buf_, len);
                do_read();
            });
    }

    void do_write(std::shared_ptr<std::string> data)
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
            });
    }

    tcp::socket socket_;
    char buf_[1024];
};

int main()
{
    const std::string host = "localhost";
    const unsigned short port = 12345;

    boost::asio::io_context io;

    auto connection = connect_to_server(io, host, port);
    if (!connection)
    {
        std::cerr << connection.error() << std::endl;
        return 1;
    }

    std::cout << "=== Echo Client 시작 (서버: " << host << ":" << port << ") ===" << std::endl;
    std::cout << "메시지를 입력하세요 (종료: Ctrl+Z):" << std::endl;

    std::make_shared<Session>(std::move(connection.value()))->start();
    io.run();

    std::cout << "=== Echo Client 종료 ===" << std::endl;
    return 0;
}
