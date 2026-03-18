// NightServer.cpp : 애플리케이션의 진입점을 정의합니다.
//

#include "NightServer.h"

using namespace std;

// 클라이언트 1명의 비동기 읽기/쓰기를 담당하는 세션
// shared_ptr로 수명을 관리하여, 비동기 콜백 체인이 끝나면 자동 소멸
class Session : public std::enable_shared_from_this<Session>
{
public:
    explicit Session(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        std::cout << "[접속] " << socket_.remote_endpoint() << std::endl;
        do_read();
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

                std::cout << "[송신] " << *data;
                do_read();
            });
    }

    tcp::socket socket_;
    char buf_[1024];
};

// 비동기로 클라이언트 접속을 수락하는 서버
class Server
{
public:
    static std::expected<Server, std::string> create(boost::asio::io_context& io, unsigned short port)
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

    void start()
    {
        do_accept();
    }

private:
    explicit Server(tcp::acceptor acceptor)
        : acceptor_(std::move(acceptor))
    {
    }

    void do_accept()
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

    tcp::acceptor acceptor_;
};

int main()
{
    const unsigned short port = 12345;

    boost::asio::io_context io;

    auto server = Server::create(io, port);
    if (!server)
    {
        std::cerr << server.error() << std::endl;
        return 1;
    }

    std::cout << "=== Echo Server 시작 (포트: " << port << ") ===" << std::endl;

    server->start();
    io.run();

    return 0;
}
