#pragma once

#include "Protocol.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <vector>
#include <boost/asio.hpp>

namespace NightNetwork
{

using boost::asio::ip::tcp;

using ReceiveCallback = std::function<void(uint32_t, std::vector<uint8_t>)>;
using CloseCallback = std::function<void(uint32_t)>;

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(uint32_t id, tcp::socket socket,
            ReceiveCallback on_receive, CloseCallback on_close);

    void start();
    void send(std::vector<uint8_t> payload);
    void close();

    uint32_t id() const { return id_; }

private:
    void enqueue_write(std::vector<uint8_t> frame);
    void do_read_header();
    void do_read_body();
    void do_write();

    uint32_t id_;
    tcp::socket socket_;
    boost::asio::strand<boost::asio::any_io_executor> strand_;
    ReceiveCallback on_receive_;
    CloseCallback on_close_;

    uint8_t header_buf_[Protocol::HEADER_SIZE];
    std::vector<uint8_t> body_buf_;

    std::queue<std::vector<uint8_t>> write_queue_;
    bool writing_ = false;
};

} // namespace NightNetwork
