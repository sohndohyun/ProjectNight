#pragma once

#include "Protocol.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NightNetwork
{

/// <summary>
/// Thread-local LIFO 메모리 풀. 고정 capacity(BLOCK_SIZE) 버퍼를 재활용하여
/// send/receive 경로에서의 반복적인 힙 할당을 줄인다.
///
/// 각 스레드가 자체 풀을 소유하므로 동기화가 필요 없다.
/// strand가 다른 스레드에서 실행되면 버퍼가 스레드 간 자연 이동한다.
/// </summary>
class BufferPool
{
public:
    using Buffer = std::vector<uint8_t>;

    static constexpr std::size_t BLOCK_SIZE =
        Protocol::HEADER_SIZE + Protocol::MAX_PAYLOAD_SIZE;

    explicit BufferPool(std::size_t max_size_per_thread = 64)
        : max_size_per_thread_(max_size_per_thread)
    {
    }

    /// <summary>
    /// thread_local 풀에서 버퍼를 꺼내고 resize(size)하여 반환한다.
    /// 풀이 비었으면 reserve(BLOCK_SIZE)로 새로 생성한다.
    /// size > BLOCK_SIZE이면 빈 벡터를 반환한다(거부).
    /// </summary>
    Buffer acquire(std::size_t size)
    {
        if (size > BLOCK_SIZE)
            return {};

        auto& pool = thread_local_pool();
        if (!pool.empty())
        {
            auto buf = std::move(pool.back());
            pool.pop_back();
            buf.resize(size);
            return buf;
        }

        Buffer buf;
        buf.reserve(BLOCK_SIZE);
        buf.resize(size);
        return buf;
    }

    /// <summary>
    /// capacity가 BLOCK_SIZE인 버퍼만 thread_local 풀에 반환한다.
    /// 풀이 max_size_per_thread에 도달했으면 자연 해제(drop)된다.
    /// </summary>
    void release(Buffer buf)
    {
        if (buf.capacity() != BLOCK_SIZE)
            return;

        auto& pool = thread_local_pool();
        if (pool.size() < max_size_per_thread_)
        {
            buf.clear();
            pool.push_back(std::move(buf));
        }
    }

private:
    std::size_t max_size_per_thread_;

    static std::vector<Buffer>& thread_local_pool()
    {
        thread_local std::vector<Buffer> pool;
        return pool;
    }
};

} // namespace NightNetwork
