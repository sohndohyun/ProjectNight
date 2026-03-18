// NightServer.cpp : 애플리케이션의 진입점을 정의합니다.
//

#include "NightServer.h"

using namespace std;

// 클라이언트로부터 데이터를 읽는 함수
// 성공: 읽은 문자열 반환 / EOF: 빈 expected (nullopt 대신 빈 문자열) / 실패: 에러 메시지
std::expected<std::string, std::string> read_from(tcp::socket& socket)
{
    char buf[1024];
    boost::system::error_code ec;

    std::size_t len = socket.read_some(boost::asio::buffer(buf), ec);

    if (ec == boost::asio::error::eof)
        return std::unexpected("[종료] 클라이언트 연결 종료");

    if (ec)
        return std::unexpected("[에러] 수신 실패: " + ec.message());

    return std::string(buf, len);
}

// 클라이언트에게 데이터를 보내는 함수
std::expected<void, std::string> write_to(tcp::socket& socket, const std::string& data)
{
    boost::system::error_code ec;
    boost::asio::write(socket, boost::asio::buffer(data), ec);

    if (ec)
        return std::unexpected("[에러] 송신 실패: " + ec.message());

    return {};
}

// 서버 소켓을 여는 함수
std::expected<tcp::acceptor, std::string> start_server(boost::asio::io_context& io, unsigned short port)
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

    return acceptor;
}

// 클라이언트 접속을 수락하는 함수
std::expected<tcp::socket, std::string> accept_client(tcp::acceptor& acceptor, boost::asio::io_context& io)
{
    tcp::socket socket(io);
    boost::system::error_code ec;

    acceptor.accept(socket, ec);
    if (ec)
        return std::unexpected("[에러] 접속 수락 실패: " + ec.message());

    return socket;
}

// 클라이언트 1명을 처리하는 함수
void handle_client(tcp::socket socket)
{
    std::cout << "[접속] " << socket.remote_endpoint() << std::endl;

    // 계속 읽고 → 그대로 돌려보내기 (Echo)
    for (;;)
    {
        // 1) 클라이언트가 보낸 데이터 읽기
        auto result = read_from(socket);
        if (!result)
        {
            std::cout << result.error() << std::endl;
            break;
        }

        std::cout << "[수신] " << result.value();

        // 2) 받은 데이터를 그대로 돌려보내기
        auto send_result = write_to(socket, result.value());
        if (!send_result)
        {
            std::cerr << send_result.error() << std::endl;
            break;
        }

        std::cout << "[송신] " << result.value();
    }
}

int main()
{
    const unsigned short port = 12345;

    boost::asio::io_context io;

    // TCP 서버 소켓 열기
    auto server = start_server(io, port);
    if (!server)
    {
        std::cerr << server.error() << std::endl;
        return 1;
    }

    std::cout << "=== Echo Server 시작 (포트: " << port << ") ===" << std::endl;

    // 무한 루프: 클라이언트 접속을 기다리고 처리
    for (;;)
    {
        auto client = accept_client(server.value(), io);
        if (!client)
        {
            std::cerr << client.error() << std::endl;
            continue;
        }

        handle_client(std::move(client.value()));
    }

    return 0;
}
