#include "Session.h"

#include <cstring>

namespace NightNetwork
{

Session::Session(uint32_t id, tcp::socket socket,
                 ReceiveCallback on_receive, CloseCallback on_close)
    : id_(id)
    , socket_(std::move(socket))
    , on_receive_(std::move(on_receive))
    , on_close_(std::move(on_close))
{
}

void Session::start()
{
    do_read_header();
}

void Session::send(std::vector<uint8_t> payload)
{
    if (payload.size() > Protocol::MAX_PAYLOAD_SIZE)
        return;

    uint32_t size = static_cast<uint32_t>(payload.size());
    std::vector<uint8_t> frame(Protocol::HEADER_SIZE + size);
    std::memcpy(frame.data(), &size, Protocol::HEADER_SIZE);
    std::memcpy(frame.data() + Protocol::HEADER_SIZE, payload.data(), size);

    write_queue_.push(std::move(frame));
    if (!writing_)
        do_write();
}

void Session::close()
{
    boost::system::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_both, ec);
    socket_.close(ec);
}

void Session::do_read_header()
{
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(header_buf_, Protocol::HEADER_SIZE),
        [this, self](boost::system::error_code ec, std::size_t)
        {
            if (ec)
            {
                on_close_(id_);
                return;
            }

            uint32_t body_size = 0;
            std::memcpy(&body_size, header_buf_, Protocol::HEADER_SIZE);

            if (body_size == 0 || body_size > Protocol::MAX_PAYLOAD_SIZE)
            {
                on_close_(id_);
                return;
            }

            body_buf_.resize(body_size);
            do_read_body();
        });
}

void Session::do_read_body()
{
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(body_buf_),
        [this, self](boost::system::error_code ec, std::size_t)
        {
            if (ec)
            {
                on_close_(id_);
                return;
            }

            on_receive_(id_, std::move(body_buf_));
            do_read_header();
        });
}

void Session::do_write()
{
    if (write_queue_.empty())
    {
        writing_ = false;
        return;
    }

    writing_ = true;
    auto self = shared_from_this();
    auto& front = write_queue_.front();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(front),
        [this, self](boost::system::error_code ec, std::size_t)
        {
            write_queue_.pop();
            if (ec)
            {
                on_close_(id_);
                return;
            }
            do_write();
        });
}

} // namespace NightNetwork
