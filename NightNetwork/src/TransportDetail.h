#pragma once

#include "BufferPool.h"
#include "Protocol.h"

#include <bit>
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

inline uint16_t to_big_endian(uint16_t value)
{
    if constexpr (std::endian::native == std::endian::little)
        return std::byteswap(value);

    return value;
}

inline uint32_t to_big_endian(uint32_t value)
{
    if constexpr (std::endian::native == std::endian::little)
        return std::byteswap(value);

    return value;
}

inline uint16_t from_big_endian(uint16_t value)
{
    return to_big_endian(value);
}

inline uint32_t from_big_endian(uint32_t value)
{
    return to_big_endian(value);
}

inline void write_header(uint8_t* output, uint8_t flags, uint32_t payload_size)
{
    auto magic_be = to_big_endian(Protocol::MAGIC);
    auto payload_size_be = to_big_endian(payload_size);

    std::memcpy(output + Protocol::MAGIC_OFFSET, &magic_be, sizeof(magic_be));
    output[Protocol::VERSION_OFFSET] = Protocol::VERSION;
    output[Protocol::FLAGS_OFFSET] = flags;
    std::memcpy(
        output + Protocol::PAYLOAD_SIZE_OFFSET,
        &payload_size_be,
        sizeof(payload_size_be));
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

inline std::expected<Protocol::Header, HeaderDecodeError> decode_header(
    const uint8_t* input)
{
    uint16_t magic_be = 0;
    uint32_t payload_size_be = 0;

    std::memcpy(
        &magic_be,
        input + Protocol::MAGIC_OFFSET,
        sizeof(magic_be));
    std::memcpy(
        &payload_size_be,
        input + Protocol::PAYLOAD_SIZE_OFFSET,
        sizeof(payload_size_be));

    Protocol::Header header{
        .magic = from_big_endian(magic_be),
        .version = input[Protocol::VERSION_OFFSET],
        .flags = input[Protocol::FLAGS_OFFSET],
        .payload_size = from_big_endian(payload_size_be),
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
