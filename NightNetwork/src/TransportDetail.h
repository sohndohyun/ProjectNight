#pragma once

#include "BufferPool.h"
#include "Protocol.h"

#include <cstring>
#include <queue>
#include <span>
#include <vector>

namespace NightNetwork::Detail
{

inline std::vector<uint8_t> build_frame(
    BufferPool& pool, std::span<const uint8_t> payload)
{
    uint32_t size = static_cast<uint32_t>(payload.size());
    auto frame = pool.acquire(Protocol::HEADER_SIZE + size);
    if (frame.empty())
        return frame;

    std::memcpy(frame.data(), &size, Protocol::HEADER_SIZE);
    if (!payload.empty())
    {
        std::memcpy(frame.data() + Protocol::HEADER_SIZE, payload.data(), size);
    }

    return frame;
}

inline std::vector<uint8_t> clone_frame(
    BufferPool& pool, std::span<const uint8_t> frame_template)
{
    auto frame = pool.acquire(frame_template.size());
    if (frame.empty())
        return frame;

    if (!frame_template.empty())
    {
        std::memcpy(frame.data(), frame_template.data(), frame_template.size());
    }

    return frame;
}

inline uint32_t read_payload_size(const uint8_t* header)
{
    uint32_t body_size = 0;
    std::memcpy(&body_size, header, Protocol::HEADER_SIZE);
    return body_size;
}

inline std::vector<uint8_t> build_keepalive_frame(BufferPool& pool)
{
    return build_frame(pool, {});
}

template <typename StartWriteFn>
void enqueue_frame(
    std::queue<std::vector<uint8_t>>& write_queue,
    bool& writing,
    std::vector<uint8_t> frame,
    StartWriteFn&& start_write)
{
    write_queue.push(std::move(frame));
    if (!writing)
        start_write();
}

} // namespace NightNetwork::Detail
