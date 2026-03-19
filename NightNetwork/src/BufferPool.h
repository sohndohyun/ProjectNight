#pragma once

#include "Protocol.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace NightNetwork
{

/// <summary>
/// Thread-safe LIFO 메모리 풀. 고정 capacity(BLOCK_SIZE) 버퍼를 재활용하여
/// send/receive 경로에서의 반복적인 힙 할당을 줄인다.
///
/// Server::Impl이 하나의 풀을 소유하고 모든 Session에 참조를 전달한다.
/// Client::Impl은 자체 풀(기본 32개)을 소유한다.
/// </summary>
class BufferPool
{
public:
    using Buffer = std::vector<uint8_t>;

    /// <summary>
    /// 풀에서 관리하는 버퍼의 고정 capacity (헤더 + 최대 페이로드)
    /// </summary>
    static constexpr std::size_t BLOCK_SIZE =
        Protocol::HEADER_SIZE + Protocol::MAX_PAYLOAD_SIZE;

    explicit BufferPool(std::size_t max_size = 256)
        : max_size_(max_size)
    {
    }

    /// <summary>
    /// 풀에서 버퍼를 꺼내고 resize(size)하여 반환한다.
    /// 풀이 비었으면 reserve(BLOCK_SIZE)로 새로 생성한다.
    /// size > BLOCK_SIZE이면 빈 벡터를 반환한다(거부).
    /// </summary>
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

    /// <summary>
    /// capacity가 BLOCK_SIZE인 버퍼만 풀에 반환한다.
    /// 풀이 max_size에 도달했으면 자연 해제(drop)된다.
    /// </summary>
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
