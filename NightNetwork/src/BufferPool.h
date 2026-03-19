#pragma once

#include "Protocol.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace NightNetwork
{

class BufferPool
{
public:
    using Buffer = std::vector<uint8_t>;

    static constexpr std::size_t BLOCK_SIZE =
        Protocol::HEADER_SIZE + Protocol::MAX_PAYLOAD_SIZE;

    explicit BufferPool(std::size_t max_size = 256)
        : max_size_(max_size)
    {
    }

    Buffer acquire(std::size_t size)
    {
        if (size > BLOCK_SIZE)
            return {};

        std::lock_guard lock(mutex_);
        if (!pool_.empty())
        {
            auto buf = std::move(pool_.back());
            pool_.pop_back();
            buf.resize(size);
            return buf;
        }

        Buffer buf;
        buf.reserve(BLOCK_SIZE);
        buf.resize(size);
        return buf;
    }

    void release(Buffer buf)
    {
        if (buf.capacity() != BLOCK_SIZE)
            return;

        std::lock_guard lock(mutex_);
        if (pool_.size() < max_size_)
        {
            buf.clear();
            pool_.push_back(std::move(buf));
        }
    }

private:
    std::mutex mutex_;
    std::vector<Buffer> pool_;
    std::size_t max_size_;
};

} // namespace NightNetwork
