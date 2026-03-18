// NightClient.cpp : 애플리케이션의 진입점을 정의합니다.
//

#include "NightClient.h"

using namespace std;

// 서버에 연결하는 함수
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

// 서버에 데이터를 보내는 함수
std::expected<void, std::string> send_to(tcp::socket& socket, const std::string& data)
{
    boost::system::error_code ec;
    boost::asio::write(socket, boost::asio::buffer(data), ec);

    if (ec)
        return std::unexpected("[에러] 송신 실패: " + ec.message());

    return {};
}

// 서버로부터 데이터를 읽는 함수
std::expected<std::string, std::string> read_from(tcp::socket& socket)
{
    char buf[1024];
    boost::system::error_code ec;

    std::size_t len = socket.read_some(boost::asio::buffer(buf), ec);

    if (ec == boost::asio::error::eof)
        return std::unexpected("[종료] 서버 연결 종료");

    if (ec)
        return std::unexpected("[에러] 수신 실패: " + ec.message());

    return std::string(buf, len);
}

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

    std::string line;
    while (std::getline(std::cin, line))
    {
        line += "\n";

        auto send_result = send_to(connection.value(), line);
        if (!send_result)
        {
            std::cerr << send_result.error() << std::endl;
            break;
        }

        auto recv_result = read_from(connection.value());
        if (!recv_result)
        {
            std::cerr << recv_result.error() << std::endl;
            break;
        }

        std::cout << "[에코] " << recv_result.value();
    }

    std::cout << "=== Echo Client 종료 ===" << std::endl;
    return 0;
}
