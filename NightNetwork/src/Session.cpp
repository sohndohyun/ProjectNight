#include "Session.h"

#include "TransportDetail.h"

namespace NightNetwork
{

Session::Session(uint32_t id, tcp::socket socket, BufferPool& pool,
                 ReceiveCallback on_receive, CloseCallback on_close)
    : id_(id)
    , socket_(std::move(socket))
    , strand_(boost::asio::make_strand(socket_.get_executor()))
    , pool_(pool)
    , on_receive_(std::move(on_receive))
    , on_close_(std::move(on_close))
    , heartbeat_timer_(strand_)
    , last_activity_(std::chrono::steady_clock::now())
{
    body_buf_.reserve(Protocol::MAX_PAYLOAD_SIZE);
}

void Session::start()
{
    do_read_header();
    start_heartbeat();
}

void Session::send(std::span<const uint8_t> payload)
{
    if (payload.size() > Protocol::MAX_PAYLOAD_SIZE)
        return;

    auto frame = Detail::build_frame(pool_, payload);

    boost::asio::post(strand_,
        [self = shared_from_this(), frame = std::move(frame)]() mutable
        {
            self->enqueue_write(std::move(frame));
        });
}

void Session::enqueue_raw_frame(std::vector<uint8_t> frame)
{
    boost::asio::post(strand_,
        [self = shared_from_this(), frame = std::move(frame)]() mutable
        {
            self->enqueue_write(std::move(frame));
        });
}

void Session::close()
{
    boost::asio::post(strand_,
        [self = shared_from_this()]()
        {
            self->handle_close();
        });
}

void Session::enqueue_write(std::vector<uint8_t> frame)
{
    if (closed_)
    {
        pool_.release(std::move(frame));
        return;
    }

    Detail::enqueue_frame(write_queue_, writing_, std::move(frame),
        [this]()
        {
            do_write();
        });
}

void Session::do_read_header()
{
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(header_buf_, Protocol::HEADER_SIZE),
        boost::asio::bind_executor(strand_,
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (ec)
                {
                    handle_close();
                    return;
                }

                last_activity_ = std::chrono::steady_clock::now();

                uint32_t body_size = Detail::read_payload_size(header_buf_);

                if (body_size == 0)
                {
                    do_read_header();
                    return;
                }

                if (body_size > Protocol::MAX_PAYLOAD_SIZE)
                {
                    handle_close();
                    return;
                }

                body_buf_.resize(body_size);
                do_read_body();
            }));
}

void Session::do_read_body()
{
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(body_buf_),
        boost::asio::bind_executor(strand_,
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (ec)
                {
                    handle_close();
                    return;
                }

                auto copy = pool_.acquire(body_buf_.size());
                std::memcpy(copy.data(), body_buf_.data(), body_buf_.size());
                on_receive_(id_, std::move(copy));
                do_read_header();
            }));
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
        boost::asio::bind_executor(strand_,
            [this, self](boost::system::error_code ec, std::size_t)
            {
                pool_.release(std::move(write_queue_.front()));
                write_queue_.pop();
                if (ec)
                {
                    handle_close();
                    return;
                }
                do_write();
            }));
}

void Session::start_heartbeat()
{
    heartbeat_timer_.expires_after(Protocol::HEARTBEAT_INTERVAL);
    heartbeat_timer_.async_wait(
        boost::asio::bind_executor(strand_,
            [self = shared_from_this()](boost::system::error_code ec)
            {
                if (ec)
                    return;

                auto elapsed = std::chrono::steady_clock::now() - self->last_activity_;
                if (elapsed >= Protocol::HEARTBEAT_TIMEOUT)
                {
                    self->handle_close();
                    return;
                }

                self->send_keepalive();
                self->start_heartbeat();
            }));
}

void Session::send_keepalive()
{
    enqueue_write(Detail::build_keepalive_frame(pool_));
}

void Session::handle_close()
{
    if (closed_)
        return;

    closed_ = true;
    heartbeat_timer_.cancel();
    boost::system::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_both, ec);
    socket_.close(ec);
    on_close_(id_);
}

} // namespace NightNetwork
