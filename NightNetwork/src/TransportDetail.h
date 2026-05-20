#pragma once

#include "BufferPool.h"
#include "Protocol.h"

#include <cstring>
#include <expected>
#include <queue>
#include <span>
#include <vector>

namespace NightNetwork::Detail
{

enum class HeaderDecodeError
{
    InvalidMagic,
    UnsupportedVersion,
    InvalidFlags,
    InvalidPayloadSize,
};

constexpr uint16_t read_u16_be(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0] << 8 | p[1]);
}

constexpr uint32_t read_u32_be(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) << 24
         | static_cast<uint32_t>(p[1]) << 16
         | static_cast<uint32_t>(p[2]) << 8
         | static_cast<uint32_t>(p[3]);
}

constexpr void write_header(uint8_t* output, uint8_t flags, uint32_t payload_size)
{
    output[Protocol::MAGIC_OFFSET]     = static_cast<uint8_t>(Protocol::MAGIC >> 8);
    output[Protocol::MAGIC_OFFSET + 1] = static_cast<uint8_t>(Protocol::MAGIC & 0xFF);
    output[Protocol::VERSION_OFFSET]   = Protocol::VERSION;
    output[Protocol::FLAGS_OFFSET]     = flags;
    output[Protocol::PAYLOAD_SIZE_OFFSET]     = static_cast<uint8_t>(payload_size >> 24);
    output[Protocol::PAYLOAD_SIZE_OFFSET + 1] = static_cast<uint8_t>(payload_size >> 16);
    output[Protocol::PAYLOAD_SIZE_OFFSET + 2] = static_cast<uint8_t>(payload_size >> 8);
    output[Protocol::PAYLOAD_SIZE_OFFSET + 3] = static_cast<uint8_t>(payload_size);
}

inline std::vector<uint8_t> build_frame(
    BufferPool& pool, uint8_t flags, std::span<const uint8_t> payload)
{
    uint32_t size = static_cast<uint32_t>(payload.size());
    auto frame = pool.acquire(Protocol::HEADER_SIZE + size);
    if (frame.empty())
        return frame;

    write_header(frame.data(), flags, size);
    if (!payload.empty())
    {
        std::memcpy(frame.data() + Protocol::HEADER_SIZE, payload.data(), size);
    }

    return frame;
}

inline std::vector<uint8_t> build_data_frame(
    BufferPool& pool, std::span<const uint8_t> payload)
{
    if (payload.empty())
        return {};

    return build_frame(pool, 0, payload);
}

inline std::vector<uint8_t> clone_frame(
    BufferPool& pool, std::span<const uint8_t> frame_template)
{
    if (frame_template.empty())
        return {};

    auto frame = pool.acquire(frame_template.size());
    if (frame.empty())
        return frame;

    if (!frame_template.empty())
    {
        std::memcpy(frame.data(), frame_template.data(), frame_template.size());
    }

    return frame;
}

constexpr std::expected<Protocol::Header, HeaderDecodeError> decode_header(
    const uint8_t* input)
{
    Protocol::Header header{
        .magic = read_u16_be(input + Protocol::MAGIC_OFFSET),
        .version = input[Protocol::VERSION_OFFSET],
        .flags = input[Protocol::FLAGS_OFFSET],
        .payload_size = read_u32_be(input + Protocol::PAYLOAD_SIZE_OFFSET),
    };

    if (header.magic != Protocol::MAGIC)
        return std::unexpected(HeaderDecodeError::InvalidMagic);

    if (header.version != Protocol::VERSION)
        return std::unexpected(HeaderDecodeError::UnsupportedVersion);

    if ((header.flags & ~Protocol::KNOWN_FLAGS_MASK) != 0)
        return std::unexpected(HeaderDecodeError::InvalidFlags);

    if (header.payload_size > Protocol::MAX_PAYLOAD_SIZE)
        return std::unexpected(HeaderDecodeError::InvalidPayloadSize);

    if (header.is_keepalive())
    {
        if (header.payload_size != 0)
            return std::unexpected(HeaderDecodeError::InvalidPayloadSize);

        return header;
    }

    if (header.payload_size == 0)
        return std::unexpected(HeaderDecodeError::InvalidPayloadSize);

    return header;
}

inline std::vector<uint8_t> build_keepalive_frame(BufferPool& pool)
{
    return build_frame(pool, Protocol::FLAG_KEEPALIVE, {});
}

template <typename StartWriteFn>
bool enqueue_frame(
    std::queue<std::vector<uint8_t>>& write_queue,
    bool& writing,
    std::vector<uint8_t>&& frame,
    StartWriteFn&& start_write,
    std::size_t max_pending_frames = 0)
{
    if (max_pending_frames != 0 && write_queue.size() >= max_pending_frames)
        return false;

    write_queue.push(std::move(frame));
    if (!writing)
        start_write();

    return true;
}

} // namespace NightNetwork::Detail
